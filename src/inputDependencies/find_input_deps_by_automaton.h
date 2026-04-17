#ifndef FIND_INPUT_DEPS_BY_AUTOMATON_H
#define FIND_INPUT_DEPS_BY_AUTOMATON_H

#include <map>
#include <spot/tl/parse.hh>
#include <spot/twaalgos/sccfilter.hh>
#include <spot/twaalgos/translate.hh>
#include <string>
#include <vector>
#include <atomic>

#include "bdd_utils.h"
#include "synt_instance.h"
#include "synt_measure.h"
#include "find_deps_by_automaton.h"

class FindInputDepsByAutomaton {
   private:
    SyntInstance& m_synt_instance;
    AutomatonFindDepsMeasure& m_measures;
    spot::twa_graph_ptr m_automaton;
    BDDVarsCacher* m_bdd_cacher;
    std::atomic<bool> m_stop_flag;
    std::atomic<bool> m_is_done;
    std::vector<std::string> m_ignored_vars;
    
    // Contextual Dependencies
    std::map<std::string, std::vector<std::pair<unsigned, unsigned>>> m_conflict_pairs;
    std::map<std::string, std::map<unsigned, bdd>> m_state_dep_functions;
    std::map<std::string, double> m_dependency_density;

    bool is_variable_dependent(std::string dependent_var,
                               std::vector<std::string>& dependency_vars,
                               std::vector<PairState>& pairStates,
                               bool use_single_bdd);

    bool is_dependent_by_pair_edges(int dependent_var,
                                    std::vector<int>& dependency_vars,
                                    std::vector<VarIndexer>& reset_vars,
                                    const PairEdges& edges);

    void find_dependencies_candidates(std::vector<std::string>& candidates_dst);

    void extract_dependency_set(std::vector<std::string>& dependency_set_dst,
                                std::vector<std::string>& current_candidates,
                                std::vector<std::string>& current_independents);

    bool get_all_compatible_states(std::vector<PairState>& pairStates,
                                   const spot::twa_graph_ptr& aut);

    bool is_global_constant(const std::string& var, bool& value_dst);

    /** Analyzes and stores contextual dependency info for a variable. */
    void check_contextual_deps(const std::string& var, const std::vector<std::string>& dependency_vars);

   public:
    explicit FindInputDepsByAutomaton(SyntInstance& synt_instance,
                                      AutomatonFindDepsMeasure& measure,
                                      spot::twa_graph_ptr aut, 
                                      bool should_prune,
                                      const std::vector<std::string>& ignored_vars)
        : m_synt_instance(synt_instance),
          m_measures(measure),
          m_stop_flag(false),
          m_is_done(false),
          m_ignored_vars(ignored_vars) {
        m_automaton = aut;

        if (should_prune) {
            m_measures.start_prune_automaton();
            m_automaton = spot::scc_filter_states(m_automaton);
            m_measures.end_prune_automaton(m_automaton);
        }

        m_bdd_cacher = new BDDVarsCacher(m_automaton);
    }

    ~FindInputDepsByAutomaton() {
        if(m_bdd_cacher != nullptr) {
            delete m_bdd_cacher;
        }
    }

    void find_dependencies(std::vector<std::string>& dependent_variables,
                           std::vector<std::string>& independent_variables,
                           bool use_single_bdd);

    void find_contextual_dependencies(bool use_single_bdd);

    const std::map<std::string, std::vector<std::pair<unsigned, unsigned>>>& get_conflict_pairs() const {
        return m_conflict_pairs;
    }

    const std::map<std::string, std::map<unsigned, bdd>>& get_state_dep_functions() const {
        return m_state_dep_functions;
    }

    double get_dependency_density(const std::string& var) const {
        auto it = m_dependency_density.find(var);
        return it != m_dependency_density.end() ? it->second : 0.0;
    }

    void stop() {
        m_stop_flag.store(true);
    }

    bool is_done() {
        return m_is_done.load();
    }
};

#endif
