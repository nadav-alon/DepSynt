#include "inp_dep_utils.h"
#include <spot/tl/formula.hh>
#include <spot/tl/parse.hh>
#include "synt_instance.h"
#include "synthesis.h"
#include "synthesis_utils.h"
#include <unordered_set>

namespace Options = boost::program_options;
using namespace std;

/**
 * Options:
 * - Decompostion (Only possible if skip synt dependencies)
 * - Skip eject dependencies
 * - Skip synt dependencies
 */
bool parse_input_dependencies_synthesis_cli(int argc, const char* argv[],
                                            InputDependenciesCLIOptions& options) {


    Options::options_description desc("Input Dependencies Synthesis Options", 100);
    parse_cli_common(options, desc, false);
    desc.add_options()
        ("env-formula,e", Options::value<string>(&options.env_formula)->required(),
         "Environment formula")
        ("system-formula,s", Options::value<string>(&options.system_formula)->required(),
         "System formula")
        ("measure-bdd,m", Options::bool_switch(&options.measure_bdd)->default_value(false),
         "Measure BDD")
        ("dependency-timeout,t", Options::value<int>(&options.dependency_timeout)->default_value(60000),
         "Dependency timeout in milliseconds")
        ("apply-model-checking,a", Options::bool_switch(&options.apply_model_checking)->default_value(false),
         "Apply model checking")
        ("model-name,n", Options::value<string>(&options.model_name)->default_value("model"),
         "Model name")
        ("algorithm,l", Options::value<string>()->default_value("ultra_naive"),
         "Algorithm to use: ultra_naive, naive, naive_projected")
        ("help,h", "Produce help message");

    // Check if help is requested
    for(int i = 0; i < argc; i++) {
        if(strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            cout << desc << endl;
            return false;
        }
    }

    try {
        Options::command_line_parser parser{argc, argv};
        parser.options(desc).allow_unregistered().style(
            Options::command_line_style::default_style |
            Options::command_line_style::allow_slash_for_short);
        Options::parsed_options parsed_options = parser.run();

        Options::variables_map vm;
        Options::store(parsed_options, vm);
        Options::notify(vm);

        if (vm.count("algorithm")) {
            options.algorithm = string_to_inp_dep_algorithm(vm["algorithm"].as<string>());
        }

        return true;
    } catch (const Options::error &e) {
        cerr << "Error: " << e.what() << endl;
        cerr << desc << endl;
        return false;
    }
}

Inp_Dep_Algorithm string_to_inp_dep_algorithm(const std::string &str) {
    if (str == "ultra_naive") {
        return ULTRA_NAIVE;
    } else if (str == "naive") {
        return NAIVE;
    } else if (str == "naive_projected") {
        return NAIVE_PROJECTED;
    } else {
        return ULTRA_NAIVE;
    }
}

std::string inp_dep_algorithm_to_string(Inp_Dep_Algorithm algo) {
    switch (algo) {
        case ULTRA_NAIVE:
            return "ultra_naive";
        case NAIVE:
            return "naive";
        case NAIVE_PROJECTED:
            return "naive_projected";
        default:
            return "unknown";
    }
}

std::ostream &operator<<(std::ostream &out, const InputDependenciesCLIOptions &options) {
    out << "Environment Formula: " << options.env_formula << endl;
    out << "System Formula: " << options.system_formula << endl;
    out << "Verbose: " << options.verbose << endl;
    out << "Inputs: " << options.inputs << endl;
    out << "Outputs: " << options.outputs << endl;
    out << "Algorithm: " << inp_dep_algorithm_to_string(options.algorithm) << endl;
    out << "Measure BDD: " << options.measure_bdd << endl;
    return out;
}

bool get_inp_dep_transducer(
    const std::string& formula_str,
    const std::vector<std::string>& input_vars,
    const std::vector<std::string>& output_vars,
    const InputDependenciesCLIOptions& options,
    SynthesisMeasure& measure,
    std::ostream& verbose,
    spot::twa_graph_ptr& out_deps_nba,
    spot::twa_graph_ptr& out_deps_strategy_aut)
{
    out_deps_strategy_aut = nullptr;
    out_deps_nba = nullptr;
    // 1. Parse formula
    spot::parsed_formula pf = spot::parse_infix_psl(formula_str);
    if (pf.format_errors(std::cerr)) {
        return false;
    }

    // 2. Add X to original output variables (the new inputs in the swapped formula)
    std::unordered_set<std::string> output_set(output_vars.begin(), output_vars.end());
    spot::formula modified_f = pf.f.map([&output_set](spot::formula f) {
        if (f.is(spot::op::ap) && output_set.count(f.ap_name())) {
            return spot::formula::X(f);
        }
        return f;
    });

    // 3. Setup options for dependency synthesis (swapped)
    SynthesisCLIOptions dependant_synthesis_opts;
    dependant_synthesis_opts.inputs = options.outputs;
    dependant_synthesis_opts.outputs = options.inputs;
    dependant_synthesis_opts.verbose = options.verbose;
    dependant_synthesis_opts.measures_path = options.measures_path;
    dependant_synthesis_opts.merge_strategies = true;
    dependant_synthesis_opts.apply_model_checking = options.apply_model_checking;
    dependant_synthesis_opts.measure_bdd = options.measure_bdd;
    dependant_synthesis_opts.skip_unates = true;
    dependant_synthesis_opts.dependency_timeout = options.dependency_timeout;

    // 4. Create swapped SyntInstance
    // original inputs are now outputs, original outputs are now inputs
    std::vector<std::string> swapped_inputs = output_vars;
    std::vector<std::string> swapped_outputs = input_vars;
    SyntInstance dependent_instance(swapped_inputs,
                                    swapped_outputs,
                                    modified_f);

    // 5. Setup spot synthesis info
    spot::synthesis_info gi;
    gi.s = spot::synthesis_info::algo::SPLIT_DET;
    gi.minimize_lvl = 2;

    // 6. Get NBA
    out_deps_nba = get_nba_for_synthesis(
        dependent_instance.get_formula_parsed(),
        gi,
        measure,
        verbose
    );

    // 7. Find dependencies and get strategy automaton
    spot::twa_graph_ptr deps_strategy_aut = nullptr;
    vector<string> independent_variables;
    vector<string> dependent_variables;

    auto dep_realizable = decompose_synthesis_only_dependents_as_aut(
        dependant_synthesis_opts,
        measure,
        gi,
        dependent_instance,
        out_deps_nba,
        swapped_inputs,
        verbose,
        out_deps_strategy_aut,
        independent_variables,
        dependent_variables);

    // dep_realizable is false only when DependentsSynthesiser::define_output_gates()
    // finds that a dependent variable has zero outgoing transitions in the NBA
    // (i.e. a dead/sink state in the automaton — a degenerate structural condition).
    // This is NOT caused by the formula being unrealizable in the game-theoretic
    // sense: input dependencies are a property of the formula's language, and
    // the transducer should still be extractable whenever the NBA has the right
    // structure. We therefore warn and return true with a null strategy, so the
    // caller can distinguish "no dependency found" from "dependency found but
    // transducer not extracted" via out_deps_strategy_aut being null.
    if (!dep_realizable) {
        verbose << "[get_inp_dep_transducer] Warning: dependent variables were "
                   "found in the swapped problem but their transducer could not "
                   "be extracted. Returning with null strategy." << std::endl;
        out_deps_strategy_aut = nullptr;
    }

    return true;
}