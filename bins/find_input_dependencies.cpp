#include <signal.h>

#include <iostream>
#include <spot/twaalgos/sccfilter.hh>
#include <vector>

#include "find_input_deps_by_automaton.h"
#include "synt_instance.h"
#include "synthesis_utils.h"
#include "synt_measure.h"
#include "utils.h"
#include "input_dependents_synthesiser.h"
#include "nba_utils.h"
#include <fstream>
#include <spot/twaalgos/aiger.hh>
#include <future>
#include <thread>

namespace Options = boost::program_options;
using namespace std;

static AutomatonFindDepsMeasure* synt_measures = nullptr;
static FindDependenciesCLIOptions options;

void on_sighup(int args) {
    if (synt_measures) {
        try {
            dump_measures(*synt_measures, options);
        } catch (...) {
            std::cerr << "Error dumping measures on signal" << std::endl;
        }
    }
    exit(EXIT_SUCCESS);
}

int main(int argc, const char* argv[]) {
    // We use the same CLI parser as find_dependencies for compatibility
    int parsed_cli_status = parse_find_dependencies_cli(argc, argv, options);
    if (!parsed_cli_status) {
        return EXIT_FAILURE;
    }

    if (options.formula.empty()) {
        cerr << "Error: Formula is empty or missing." << endl;
        return EXIT_FAILURE;
    }

    if (!options.model_name.empty()) {
        cout << "Processing Model: " << options.model_name << endl;
    }

    std::ostream nullout(nullptr);
    ostream& verbose_out = options.verbose ? std::cout : nullout;

    // Negate the formula for input dependency discovery (environment's POV)
    string negated_formula = "!(" + options.formula + ")";
    
    verbose_out << "Initialize Synthesis Instance (Input Dependencies)..." << endl;
    verbose_out << "Original Formula: " << options.formula << endl;
    verbose_out << "Negated Formula:  " << negated_formula << endl;
    
    auto* synt_instance = new SyntInstance(options.inputs, options.outputs, negated_formula);
    verbose_out << "Synthesis Problem: " << endl;
    verbose_out << *synt_instance << endl;
    verbose_out << "================================" << endl;

    signal(SIGINT, on_sighup);
    signal(SIGTERM, on_sighup);
    signal(SIGHUP, on_sighup);

    try {
        // Input dependencies only support the Automaton algorithm for now
        auto* automaton_measures = new AutomatonFindDepsMeasure(*synt_instance, false);
        synt_measures = automaton_measures;

        verbose_out << "Searching Input Dependencies By Causal Automaton Definition..." << endl;

        const bdd_dict_ptr& dict = make_bdd_dict();
        synthesis_info gi;
        gi.dict = dict;
        gi.s = synthesis_info::algo::SPLIT_DET;
        gi.minimize_lvl = 2;

        spot::twa_graph_ptr automaton = nullptr;
        if (options.dependency_timeout > 0) {
            std::future<spot::twa_graph_ptr> fut = std::async(std::launch::async, [&] {
                return get_nba_for_synthesis(
                    synt_instance->get_formula_parsed(), gi, *automaton_measures, verbose_out);
            });
            if (fut.wait_for(std::chrono::milliseconds(options.dependency_timeout)) == std::future_status::timeout) {
                cerr << "Timeout during automaton construction." << endl;
                if (synt_measures) dump_measures(*synt_measures, options);
                _exit(EXIT_FAILURE);
            }
            automaton = fut.get();
        } else {
            automaton = get_nba_for_synthesis(
                synt_instance->get_formula_parsed(), gi, *automaton_measures, verbose_out);
        }

        // Search for dependent input variables
        vector<string> dependent_variables, independent_variables;
        
        // Causal discovery ignores system outputs (m_ignored_vars)
        vector<string> outputs;
        extract_variables(options.outputs, outputs);

        FindInputDepsByAutomaton automaton_dependencies(
            *synt_instance, *automaton_measures, automaton, false, outputs);
            
        if (options.dependency_timeout > 0) {
            std::future<void> fut = std::async(std::launch::async, [&] {
                automaton_dependencies.find_dependencies(dependent_variables, independent_variables, false);
            });
            if (fut.wait_for(std::chrono::milliseconds(options.dependency_timeout)) == std::future_status::timeout) {
                cerr << "Timeout during dependency search." << endl;
                automaton_dependencies.stop();
                while (!automaton_dependencies.is_done()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(3));
                }
                if (synt_measures) dump_measures(*synt_measures, options);
                _exit(EXIT_FAILURE);
            }
        } else {
            automaton_dependencies.find_dependencies(dependent_variables, independent_variables, false);
        }

        cout << "Input Dependent Variables: " << dependent_variables << endl;
        cout << "Input Independent Variables: " << independent_variables << endl;

        if (!options.dependency_transducer_path.empty() && !dependent_variables.empty()) {
            verbose_out << "Synthesizing input dependency transducer..." << endl;
            
            spot::twa_graph_ptr nba_with_deps = clone_nba(automaton);
            spot::twa_graph_ptr nba_without_deps = automaton;
            unordered_map<int, bdd> bdd_to_bdd_without_deps;
            
            remove_ap_from_automaton(nba_without_deps, dependent_variables, bdd_to_bdd_without_deps);
            
            vector<string> input_vars;
            extract_variables(options.inputs, input_vars);
            
            InputDependentsSynthesiser synthesis(
                nba_without_deps,
                nba_with_deps,
                input_vars,
                outputs, // these are system outputs in the negated formula's POV
                independent_variables,
                dependent_variables,
                bdd_to_bdd_without_deps
            );
            
            spot::aig_ptr strategy = synthesis.synthesis();
            if (strategy) {
                verbose_out << "Saving transducer to " << options.dependency_transducer_path << endl;
                std::ofstream aiger_file(options.dependency_transducer_path);
                spot::print_aiger(aiger_file, strategy) << endl;
            } else {
                cerr << "Error: Failed to synthesize input dependency transducer." << endl;
            }
        }

        synt_measures->completed();
        dump_measures(*synt_measures, options);
        delete synt_measures;
        synt_measures = nullptr;
        delete synt_instance;
    } catch (const std::runtime_error& re) {
        std::cerr << "Runtime error: " << re.what() << std::endl;
        if (synt_measures) dump_measures(*synt_measures, options);
        delete synt_instance;
    } catch (const std::exception& ex) {
        std::cerr << "Error occurred: " << ex.what() << std::endl;
        if (synt_measures) dump_measures(*synt_measures, options);
        delete synt_instance;
    } catch (...) {
        std::cerr << "Unknown failure occurred." << std::endl;
        if (synt_measures) dump_measures(*synt_measures, options);
        delete synt_instance;
    }

    return EXIT_SUCCESS;
}
