#include "naive_algorithm.h"
#include <spot/twa/fwd.hh>
#include <spot/twaalgos/synthesis.hh>
#include "inp_dep_utils.h"
#include "synt_instance.h"
#include "synthesis.h"
#include "synthesis_utils.h"
#include <spot/twaalgos/product.hh>
#include <iostream>

using namespace std;



int naive(InputDependenciesCLIOptions& options, spot::aig_ptr& final_strategy, SynthesisMeasure*& measure, bool project) {

    ostream nullout(nullptr);
    ostream& verbose = options.verbose ? std::cout : nullout;

    spot::synthesis_info gi;
    gi.s = spot::synthesis_info::algo::SPLIT_DET;
    gi.minimize_lvl = 2;  // i.e, simplication level

    SynthesisCLIOptions dependant_synthesis_opts;

    dependant_synthesis_opts.formula = options.env_formula;
    dependant_synthesis_opts.inputs = options.outputs;
    dependant_synthesis_opts.outputs = options.inputs;
    dependant_synthesis_opts.verbose = options.verbose;
    dependant_synthesis_opts.measures_path = options.measures_path;

    dependant_synthesis_opts.merge_strategies = true;
    dependant_synthesis_opts.apply_model_checking = options.apply_model_checking;
    dependant_synthesis_opts.measure_bdd = options.measure_bdd;
    dependant_synthesis_opts.skip_unates = true;
    dependant_synthesis_opts.dependency_timeout = options.dependency_timeout;

    auto* dependent_instance = new SyntInstance(options.outputs, options.inputs, dependant_synthesis_opts.formula);

    measure = new InpDepSyntMeasure(*dependent_instance, false, false);
    InpDepSyntMeasure& dependent_measure = static_cast<InpDepSyntMeasure&>(*measure);

    dependent_measure.set_measure_bdd(options.measure_bdd);

    final_strategy = nullptr;


    twa_graph_ptr deps_nba = get_nba_for_synthesis(
        dependent_instance->get_formula_parsed(), 
        gi, 
        dependent_measure, 
        verbose
    );

    twa_graph_ptr deps_strategy_aut = nullptr; 


    vector<string> independent_variables;
    vector<string> dependent_variables;

    auto output_vars = dependent_instance->get_output_vars();

    auto dep_realizable = decompose_synthesis_only_dependents_as_aut(dependant_synthesis_opts,
                                               dependent_measure,
                                               gi,
                                               *dependent_instance,
                                               deps_nba,
                                               output_vars,
                                               verbose,
                                               deps_strategy_aut,
                                               independent_variables,
                                               dependent_variables);

    
    auto product_aut = spot::product(deps_nba, deps_strategy_aut);

    product_aut = spot::scc_filter_states(product_aut);

    if (project) {
        // 1. Retrieve the mapping from product states to original state pairs
        auto ps = product_aut->get_named_prop<spot::product_states>("product-states");
        
        // 2. Create a new graph with the same dictionary as the NBA
        auto projected_aut = spot::make_twa_graph(deps_nba->get_dict());
        
        // 3. Ensure it has the same number of states as the original NBA
        for (unsigned i = 0; i < deps_nba->num_states(); ++i)
            projected_aut->new_state();
            
        // 4. Iterate over all transitions in the product and "forget" the strategy component
        for (unsigned s = 0; s < product_aut->num_states(); ++s) {
            unsigned s_nba = (*ps)[s].first; // The state ID in the original NBA
            for (auto& edge : product_aut->out(s)) {
                unsigned dst_nba = (*ps)[edge.dst].first;
                // Add the transition to the new graph using NBA state IDs
                projected_aut->new_edge(s_nba, dst_nba, edge.cond, edge.acc);
            }
        }
        
        // 5. Restore initial state and acceptance info
        projected_aut->set_init_state(deps_nba->get_init_state_number());
        projected_aut->copy_acceptance_of(deps_nba);
        
        // 6. Clean up: merge parallel transitions that might have been created
        projected_aut->merge_edges();
        
        product_aut = projected_aut;
    }


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

    SyntInstance synt(options.inputs, options.outputs, full_synt_options.formula);

    synthesis(full_synt_options, synt, dependent_measure, final_strategy, product_aut);
    

    return final_strategy == nullptr ? EXIT_FAILURE : EXIT_SUCCESS; 
}