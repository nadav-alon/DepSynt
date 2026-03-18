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



struct NullBuffer : std::streambuf {
    int overflow(int c) override { return c; }
};

int naive(InputDependenciesCLIOptions& options, spot::aig_ptr& final_strategy, SynthesisMeasure*& measure, bool project) {

    NullBuffer nullbuf;
    ostream nullout(&nullbuf);
    ostream& verbose = options.verbose ? std::cout : nullout;

    spot::synthesis_info gi;
    gi.s = spot::synthesis_info::algo::SPLIT_DET;
    gi.minimize_lvl = 2;  // i.e, simplication level

    vector<string> inputs_vec, outputs_vec;
    extract_variables(options.inputs, inputs_vec);
    extract_variables(options.outputs, outputs_vec);

    auto* dummy_instance = new SyntInstance(inputs_vec, outputs_vec, options.system_formula);
    measure = new InpDepSyntMeasure(*dummy_instance, options.measure_bdd, false);
    InpDepSyntMeasure& dependent_measure = static_cast<InpDepSyntMeasure&>(*measure);
    dependent_measure.set_measure_bdd(options.measure_bdd);

    final_strategy = nullptr;

    twa_graph_ptr deps_nba = nullptr;
    twa_graph_ptr deps_strategy_aut = nullptr;
    auto dep_realizable = get_inp_dep_transducer(
        options.env_formula, 
        inputs_vec, 
        outputs_vec, 
        options, 
        dependent_measure, 
        verbose, 
        deps_nba,
        deps_strategy_aut
    );

    if (!dep_realizable) {
        return EXIT_FAILURE;
    }
    
    auto product_aut = deps_strategy_aut != nullptr ? spot::product(deps_nba, deps_strategy_aut) : deps_nba;

    if (project && deps_strategy_aut != nullptr) {
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
    
    product_aut = spot::scc_filter_states(product_aut);


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
    if (deps_strategy_aut != nullptr) {
        final_synthesis_aut = spot::product(full_nba, deps_strategy_aut);
        final_synthesis_aut = spot::scc_filter_states(final_synthesis_aut);
    }

    synthesis(full_synt_options, *synt, dependent_measure, final_strategy, final_synthesis_aut);
    

    return final_strategy == nullptr ? EXIT_FAILURE : EXIT_SUCCESS; 
}