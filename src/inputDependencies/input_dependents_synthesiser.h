#ifndef INPUT_DEPENDENTS_SYNTHESISER_H
#define INPUT_DEPENDENTS_SYNTHESISER_H

#include <spot/tl/parse.hh>
#include <spot/twa/fwd.hh>
#include <spot/twa/twa.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/aiger.hh>
#include <string>
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

    Realizability m_is_realizable;
    
    bdd m_output_vars_bdd;
    std::unordered_map<int, Gate> m_bdd_to_gate_map;

    void init_aiger();
    void define_next_state_logic();
    void define_dependency_logic();

    static unsigned count_bits(unsigned n) {
        if (n <= 1) return 0u;
        unsigned bits = 0;
        unsigned m = n - 1;
        while (m > 0) {
            m >>= 1;
            bits++;
        }
        return bits;
    }

    Gate get_partial_impl(const bdd& cond, std::string& dep_var);
    Gate generate_partial_impl(const bdd& cond, std::string& dep_var,
                               std::unordered_map<int, Gate>& bdd_partial_impl);

    Gate bdd_to_gate(const bdd& cond, std::unordered_map<int, Gate>& cache);
    Gate safe_aig_or(std::vector<Gate>& vs);

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
                               std::unordered_map<int, bdd>& bdd_to_bdd_without_deps)
        : m_nba_without_deps(nba_without_deps),
          m_nba_with_deps(nba_with_deps),
          m_input_vars(input_vars),
          m_output_vars(output_vars),
          m_indep_vars(indep_vars),
          m_dep_vars(dep_vars),
          m_bdd_to_bdd_without_deps(bdd_to_bdd_without_deps),
          m_is_realizable(Realizability::UNKNOWN) {
          
          m_output_vars_bdd = bddtrue;
          for (const auto& var : m_output_vars) {
              m_output_vars_bdd &= bdd_ithvar(m_nba_with_deps->register_ap(var));
          }
    };

    spot::aig_ptr synthesis();

    static spot::aig_ptr compose_transition_and_dependency_aigers(
        const spot::aig_ptr& nba_aiger,
        const spot::aig_ptr& dep_aiger,
        const std::vector<std::string>& indep_vars,
        const std::vector<std::string>& dep_vars,
        const std::vector<std::string>& output_vars,
        const spot::bdd_dict_ptr& dict);
};

#endif
