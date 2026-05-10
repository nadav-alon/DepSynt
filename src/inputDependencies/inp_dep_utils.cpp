#include "inp_dep_utils.h"

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
        ("env-formula,e", Options::value<string>(&options.env_formula),
         "Environment formula")
        ("system-formula,s", Options::value<string>(&options.system_formula),
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
         "Algorithm to use: ultra_naive, naive")
        ("skip-unates,u", Options::bool_switch(&options.skip_unates)->default_value(false),
         "Skip unates")
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

        if (vm.count("formula")) {
            if (vm.count("env-formula") || vm.count("system-formula")) {
                cerr << "Error: Cannot provide both --formula and --env-formula/--system-formula" << endl;
                cerr << desc << endl;
                return false;
            }
        } else {
            if (!vm.count("env-formula") || !vm.count("system-formula")) {
                cerr << "Error: Must provide either --formula OR both --env-formula and --system-formula" << endl;
                cerr << desc << endl;
                return false;
            }
        }

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
        default:
            return "unknown";
    }
}

std::ostream &operator<<(std::ostream &out, const InputDependenciesCLIOptions &options) {
    if (!options.formula.empty()) {
        out << "Formula: " << options.formula << endl;
    } else {
        out << "Environment Formula: " << options.env_formula << endl;
        out << "System Formula: " << options.system_formula << endl;
    }
    out << "Verbose: " << options.verbose << endl;
    out << "Inputs: " << options.inputs << endl;
    out << "Outputs: " << options.outputs << endl;
    out << "Algorithm: " << inp_dep_algorithm_to_string(options.algorithm) << endl;
    out << "Measure BDD: " << options.measure_bdd << endl;
    out << "Skip Unates: " << options.skip_unates << endl;
    return out;
}