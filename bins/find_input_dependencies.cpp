#include <signal.h>

#include <iostream>
#include <spot/twaalgos/sccfilter.hh>
#include <vector>

#include "find_input_deps_by_automaton.h"
#include "synt_instance.h"
#include "synthesis_utils.h"
#include "synt_measure.h"
#include "utils.h"

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
        spot::synthesis_info gi;
        gi.s = spot::synthesis_info::algo::SPLIT_DET;
        gi.minimize_lvl = 2;

        spot::twa_graph_ptr automaton = get_nba_for_synthesis(
                synt_instance.get_formula_parsed(), gi, *automaton_measures, verbose_out);

        // Search for dependent input variables
        vector<string> dependent_variables, independent_variables;
        
        // Causal discovery ignores system outputs (m_ignored_vars)
        vector<string> outputs;
        extract_variables(options.outputs, outputs);

        FindInputDepsByAutomaton automaton_dependencies(
            synt_instance, *automaton_measures, automaton, false, outputs);
            
        automaton_dependencies.find_dependencies(dependent_variables, independent_variables, false);

        cout << "Input Dependent Variables: " << dependent_variables << endl;
        cout << "Input Independent Variables: " << independent_variables << endl;

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
