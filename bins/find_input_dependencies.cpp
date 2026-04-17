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

    std::ostream nullout(nullptr);
    ostream& verbose_out = options.verbose ? std::cout : nullout;

    // Negate the formula for input dependency discovery (environment's POV)
    string negated_formula = "!(" + options.formula + ")";
    
    verbose_out << "Initialize Synthesis Instance (Input Dependencies)..." << endl;
    verbose_out << "Original Formula: " << options.formula << endl;
    verbose_out << "Negated Formula:  " << negated_formula << endl;
    
    SyntInstance synt_instance(options.inputs, options.outputs, negated_formula);
    verbose_out << "Synthesis Problem: " << endl;
    verbose_out << synt_instance << endl;
    verbose_out << "================================" << endl;

    signal(SIGINT, on_sighup);
    signal(SIGTERM, on_sighup);
    signal(SIGHUP, on_sighup);

    try {
        // Input dependencies only support the Automaton algorithm for now
        auto* automaton_measures = new AutomatonFindDepsMeasure(synt_instance, false);
        synt_measures = automaton_measures;

        verbose_out << "Searching Input Dependencies By Causal Automaton Definition..." << endl;

        // Building Instance Automaton
        auto* gi_ptr = new spot::synthesis_info();
        spot::synthesis_info& gi = *gi_ptr;
        gi.dict = spot::make_bdd_dict();
        gi.s = spot::synthesis_info::algo::SPLIT_DET;
        gi.minimize_lvl = 2;

        spot::twa_graph_ptr automaton = get_nba_for_synthesis(
                synt_instance.get_formula_parsed(), gi, *automaton_measures, verbose_out);

        // Search for dependent input variables
        vector<string> dependent_variables, independent_variables;
        
        // Causal discovery ignores system outputs (m_ignored_vars)
        vector<string> outputs;
        extract_variables(options.outputs, outputs);

        vector<string> input_vars;
        extract_variables(options.inputs, input_vars);

        FindInputDepsByAutomaton automaton_dependencies(
            synt_instance, *automaton_measures, automaton, false, outputs);
            
        if (options.contextual) {
            verbose_out << "Performing contextual dependency discovery..." << endl;
            automaton_dependencies.find_contextual_dependencies(false);
            
            // For contextual discovery, we assume ALL inputs are candidates for dependency
            // but the independent set is defined as variables where density is 0?
            // Actually, for contextual, we just need the conflict pairs.
            // But let's fill dependent_variables for the transducer synthesis logic.
            for (const auto& var : input_vars) {
                if (automaton_dependencies.get_dependency_density(var) > 0) {
                    dependent_variables.push_back(var);
                } else {
                    independent_variables.push_back(var);
                }
            }
        } else {
            automaton_dependencies.find_dependencies(dependent_variables, independent_variables, false);
        }

        if (options.report_density) {
            cout << "Contextual Dependency Density Report:" << endl;
            for (const auto& var : input_vars) {
                cout << " - " << var << ": " << automaton_dependencies.get_dependency_density(var) << endl;
            }
        }

        cout << "Input Dependent Variables: " << dependent_variables << endl;
        cout << "Input Independent Variables: " << independent_variables << endl;

        if (!options.dependency_transducer_path.empty() && !dependent_variables.empty()) {
            verbose_out << "Synthesizing input dependency transducer..." << endl;
            
            spot::twa_graph_ptr nba_with_deps = clone_nba(automaton);
            spot::twa_graph_ptr nba_without_deps = automaton;
            unordered_map<int, bdd> bdd_to_bdd_without_deps;
            
            remove_ap_from_automaton(nba_without_deps, dependent_variables, bdd_to_bdd_without_deps);
            
            InputDependentsSynthesiser synthesis(
                nba_without_deps,
                nba_with_deps,
                input_vars,
                outputs, // these are system outputs in the negated formula's POV
                independent_variables,
                dependent_variables,
                bdd_to_bdd_without_deps,
                automaton_dependencies.get_conflict_pairs(),
                automaton_dependencies.get_state_dep_functions()
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
    } catch (const std::runtime_error& re) {
        std::cerr << "Runtime error: " << re.what() << std::endl;
        if (synt_measures) dump_measures(*synt_measures, options);
    } catch (const std::exception& ex) {
        std::cerr << "Error occurred: " << ex.what() << std::endl;
        if (synt_measures) dump_measures(*synt_measures, options);
    } catch (...) {
        std::cerr << "Unknown failure occurred." << std::endl;
        if (synt_measures) dump_measures(*synt_measures, options);
    }

    return EXIT_SUCCESS;
}
