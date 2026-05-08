#include "naive_algorithm.h"
#include <spot/twa/fwd.hh>
#include <spot/twaalgos/synthesis.hh>
#include "inp_dep_utils.h"
#include "synt_instance.h"
#include "synthesis.h"
#include "synthesis_utils.h"
#include <spot/twaalgos/product.hh>
#include <spot/tl/relabel.hh>
#include <spot/tl/parse.hh>
#include "automaton_aiger_builder.h"
#include "input_dependents_synthesiser.h"
#include <iostream>
#include <sstream>

using namespace std;



struct NullBuffer : std::streambuf {
    int overflow(int c) override { return c; }
};

int naive(InputDependenciesCLIOptions& options, spot::aig_ptr& final_strategy, SynthesisMeasure*& measure) {

    NullBuffer nullbuf;
    ostream nullout(&nullbuf);
    ostream& verbose = options.verbose ? std::cout : nullout;

    spot::synthesis_info gi;
    gi.s = spot::synthesis_info::algo::SPLIT_DET;
    gi.minimize_lvl = 2;  // i.e, simplication level

    SynthesisCLIOptions dependant_synthesis_opts;

    spot::parsed_formula pf_env = spot::parse_infix_psl(options.env_formula);
    if (pf_env.format_errors(std::cerr)) {
        return EXIT_FAILURE;
    }
    spot::parsed_formula pf_sys = spot::parse_infix_psl(options.system_formula);
    if (pf_sys.format_errors(std::cerr)) {
        return EXIT_FAILURE;
    }

    spot::formula full_formula = spot::formula::Implies(pf_env.f, pf_sys.f);

    std::vector<std::string> inputs_vec, outputs_vec;
    extract_variables(options.inputs, inputs_vec);
    extract_variables(options.outputs, outputs_vec);

    auto* synt_instance = new SyntInstance(inputs_vec, outputs_vec, full_formula);

    dependant_synthesis_opts.inputs = options.inputs;
    dependant_synthesis_opts.outputs = options.outputs;
    dependant_synthesis_opts.verbose = options.verbose;
    dependant_synthesis_opts.measures_path = options.measures_path;
    dependant_synthesis_opts.merge_strategies = true;
    dependant_synthesis_opts.apply_model_checking = options.apply_model_checking;
    dependant_synthesis_opts.measure_bdd = options.measure_bdd;
    dependant_synthesis_opts.skip_unates = true;
    dependant_synthesis_opts.dependency_timeout = options.dependency_timeout;

    measure = new InpDepSyntMeasure(*synt_instance, options.measure_bdd, false);
    InpDepSyntMeasure& dependent_measure = static_cast<InpDepSyntMeasure&>(*measure);

    dependent_measure.set_measure_bdd(options.measure_bdd);

    final_strategy = nullptr;


    twa_graph_ptr deps_nba = get_nba_for_synthesis(
        synt_instance->get_formula_parsed(), 
        gi, 
        dependent_measure, 
        verbose
    );

    spot::aig_ptr deps_strategy = nullptr; 

    vector<string> independent_variables;
    vector<string> dependent_variables;

    vector<string> ignored_vars;
    extract_variables(options.outputs, ignored_vars);

    auto output_vars = synt_instance->get_output_vars();
    auto input_vars = synt_instance->get_input_vars();

    bool dep_realizable = decompose_synthesis_only_input_dependents(dependant_synthesis_opts,
                                               dependent_measure,
                                               gi,
                                               *synt_instance,
                                               deps_nba,
                                               input_vars,
                                               output_vars,
                                               verbose,
                                               deps_strategy,
                                               independent_variables,
                                               dependent_variables,
                                               ignored_vars);

    if (!dep_realizable) {
        return EXIT_FAILURE;
    }

    double input_dep_duration = dependent_measure.get_find_deps_duration();
    int input_dep_count = dependent_measure.get_dependent_variables_count();
    double input_dep_synthesis_duration = dependent_measure.get_dependents_synthesis_duration();
    dependent_measure.set_input_dep_measures(input_dep_duration, input_dep_count, input_dep_synthesis_duration);
    dependent_measure.reset_deps_measurements();
    dependent_measure.reset_dependents_synthesis_duration();


    final_strategy = nullptr;

    SynthesisCLIOptions full_synt_options;
    full_synt_options.formula = "("+options.env_formula+")->("+options.system_formula+")";
    full_synt_options.inputs = options.inputs;
    full_synt_options.outputs = options.outputs;
    full_synt_options.verbose = options.verbose;
    full_synt_options.measures_path = options.measures_path;

    full_synt_options.merge_strategies = true;
    full_synt_options.apply_model_checking = options.apply_model_checking;
    full_synt_options.measure_bdd = options.measure_bdd;
    full_synt_options.skip_unates = true;
    full_synt_options.dependency_timeout = options.dependency_timeout;

    auto* synt = new SyntInstance(options.inputs, options.outputs, full_synt_options.formula);

    twa_graph_ptr full_nba = get_nba_for_synthesis(
        synt->get_formula_parsed(), 
        gi, 
        dependent_measure, 
        verbose
    );

    twa_graph_ptr final_synthesis_aut = full_nba;
    if (deps_strategy != nullptr) {
        auto full_nba_aiger = AutomatonAigerBuilder::construct_transition_aiger(full_nba);
        auto composed_aiger = InputDependentsSynthesiser::compose_transition_and_dependency_aigers(
            full_nba_aiger, deps_strategy, independent_variables, dependent_variables, output_vars, full_nba->get_dict());
            
        vector<string> combined_ap_names = independent_variables;
        combined_ap_names.insert(combined_ap_names.end(), output_vars.begin(), output_vars.end());

        unsigned num_state_bits_dep = 0;
        while (num_state_bits_dep < deps_strategy->input_names().size() && 
               deps_strategy->input_names()[num_state_bits_dep].find("curr_s") == 0) num_state_bits_dep++;
        
        unsigned num_states_combined = full_nba->num_states() * (1 << num_state_bits_dep);

        final_synthesis_aut = AutomatonAigerBuilder::aiger_to_automaton(composed_aiger, num_states_combined, combined_ap_names, full_nba->get_dict(), full_nba->get_acceptance());
        final_synthesis_aut = spot::scc_filter_states(final_synthesis_aut);
    }

    synthesis(full_synt_options, *synt, dependent_measure, final_strategy, final_synthesis_aut);
    

    return final_strategy == nullptr ? EXIT_FAILURE : EXIT_SUCCESS; 
}