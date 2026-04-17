#ifndef INPUT_DEPENDENTS_SYNTHESISER_H
#define INPUT_DEPENDENTS_SYNTHESISER_H

#include <algorithm>
#include <iostream>
#include <spot/tl/parse.hh>
#include <spot/twa/fwd.hh>
#include <spot/twa/twa.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/aiger.hh>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>
#include <unordered_map>

#include "dependents_synthesiser.h"

class InputDependentsSynthesiser {
    enum Realizability { REALIZABLE, UNREALIZABLE, UNKNOWN };
   private:
    spot::twa_graph_ptr m_nba_without_deps;
    spot::twa_graph_ptr m_nba_with_deps;
    std::vector<std::string> m_input_vars; 
    std::vector<std::string> m_output_vars; 
    std::vector<std::string> m_indep_vars; 
    std::vector<std::string> m_dep_vars;   

    spot::aig_ptr m_aiger;

    std::unordered_set<BDDVar> deps_bdd_vars;
    std::unordered_map<std::string, Gate> partial_impl_cache;
    std::unordered_map<int, bdd>& m_bdd_to_bdd_without_deps;

    const std::map<std::string, std::vector<std::pair<unsigned, unsigned>>>& m_conflict_pairs;
    const std::map<std::string, std::map<unsigned, bdd>>& m_state_dep_functions;

    Realizability m_is_realizable;
    
    bdd m_output_vars_bdd;
    
    void init_aiger();
    void define_next_latches();
    void define_output_gates();
    void define_unified_output_gates();

    Gate get_partial_impl(const bdd& cond, std::string& dep_var);
    Gate generate_partial_impl(const bdd& cond, std::string& dep_var,
                               std::unordered_map<int, Gate>& bdd_partial_impl);

    BDDVar ap_to_bdd_varnum(std::string& ap) {
        return m_nba_with_deps->register_ap(ap);
    }

   public:
    InputDependentsSynthesiser(spot::twa_graph_ptr& nba_without_deps,
                               spot::twa_graph_ptr& nba_with_deps,
                               std::vector<std::string>& input_vars,
                               std::vector<std::string>& output_vars,
                               std::vector<std::string>& indep_vars,
                               std::vector<std::string>& dep_vars,
                               std::unordered_map<int, bdd>& bdd_to_bdd_without_deps,
                               const std::map<std::string, std::vector<std::pair<unsigned, unsigned>>>& conflict_pairs,
                               const std::map<std::string, std::map<unsigned, bdd>>& state_dep_functions)
        : m_nba_without_deps(nba_without_deps),
          m_nba_with_deps(nba_with_deps),
          m_input_vars(input_vars),
          m_output_vars(output_vars),
          m_indep_vars(indep_vars),
          m_dep_vars(dep_vars),
          m_bdd_to_bdd_without_deps(bdd_to_bdd_without_deps),
          m_conflict_pairs(conflict_pairs),
          m_state_dep_functions(state_dep_functions),
          m_is_realizable(Realizability::UNKNOWN) {
          
          m_output_vars_bdd = bddtrue;
          for (const auto& var : m_output_vars) {
              m_output_vars_bdd &= bdd_ithvar(m_nba_with_deps->register_ap(var));
          }
    };

    spot::aig_ptr synthesis();
};

#endif
