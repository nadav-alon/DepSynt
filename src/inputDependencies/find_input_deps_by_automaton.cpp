#include "find_input_deps_by_automaton.h"
#include <boost/range/join.hpp>
#include <cmath>
#include <spot/twaalgos/sccfilter.hh>
#include <string>
#include <algorithm>
#include <unordered_set>

#include "utils.h"

using namespace std;

void FindInputDepsByAutomaton::find_dependencies(vector<string>& dependent_variables,
                                                 vector<string>& independent_variables,
                                                 bool use_single_bdd) {
    m_measures.start_find_deps();

    // Find Candidates
    std::vector<std::string> candidates;
    this->find_dependencies_candidates(candidates);

    // Find PairStates
    m_measures.start_search_pair_states();
    vector<PairState> compatibleStates;
    get_all_compatible_states(compatibleStates, m_automaton);
    m_measures.end_search_pair_states(static_cast<int>(compatibleStates.size()));

    // stopped by timeout
    if(m_stop_flag.load()) {
        independent_variables.insert(independent_variables.end(), candidates.begin(),
                                     candidates.end());
        m_measures.end_find_deps(false);
        m_is_done.store(true);
        return;
    }

    while (!candidates.empty() && !m_stop_flag.load()) {
        std::string dependent_var = candidates.back();
        candidates.pop_back();
        m_measures.start_testing_variable(dependent_var);

        vector<string> dependency_set;
        this->extract_dependency_set(dependency_set, candidates,
                                     independent_variables);

        // Check if candidates variable is dependent
        if (this->is_variable_dependent(dependent_var, dependency_set,
                                        compatibleStates, use_single_bdd)) {
            dependent_variables.push_back(dependent_var);
            m_measures.end_testing_variable(true, dependency_set);
        } else {
            independent_variables.push_back(dependent_var);
            m_measures.end_testing_variable(false, dependency_set);
        }
    }

    independent_variables.insert(independent_variables.end(), candidates.begin(),
                                 candidates.end());

    m_measures.end_find_deps(!m_stop_flag.load());
    m_is_done.store(true);
}

void FindInputDepsByAutomaton::find_dependencies_candidates(
    std::vector<std::string>& candidates_dst) {
    candidates_dst.clear();
    const vector<string>& candidates = m_synt_instance.get_input_vars();
    std::copy(candidates.begin(), candidates.end(),
              std::back_inserter(candidates_dst));
}

void FindInputDepsByAutomaton::extract_dependency_set(
    std::vector<std::string>& dependency_set_dst,
    std::vector<std::string>& current_candidates,
    std::vector<std::string>& current_independents) {
    dependency_set_dst.clear();
    
    // Causal Input Dependency: Dependency Set must NOT include ignored vars (outputs)
    // Dependency Set = current_candidates + current_independents
    auto target_deps_set = boost::join(current_candidates, current_independents);
    copy(target_deps_set.begin(), target_deps_set.end(),
         back_inserter(dependency_set_dst));
}

bool FindInputDepsByAutomaton::is_variable_dependent(std::string var,
                                                     vector<std::string>& dependency_vars,
                                                     vector<PairState>& compatibleStates,
                                                     bool use_single_bdd) {
    vector<int> dependency_vars_nums;
    int dependent_var_num = m_bdd_cacher->get_variable_index(var);

    for (auto& var : dependency_vars) {
        dependency_vars_nums.push_back(m_bdd_cacher->get_variable_index(var));
    }
    
    vector<string> remaining_vars;
    vector<VarIndexer> remaining_vars_nums;
    vector<string> excluded_vars(dependency_vars);
    excluded_vars.emplace_back(var);
    
    // Include ignored vars in reset_vars (they will be primed in the double-path check)
    m_synt_instance.all_vars_excluded(remaining_vars, excluded_vars);
    
    for (auto& var : remaining_vars) {
        remaining_vars_nums.push_back({m_bdd_cacher->get_variable_index(var),
                                   m_bdd_cacher->get_prime_variable_index(var)});
    }

    if(use_single_bdd) {
        for (auto pairState : compatibleStates) {
            if(m_stop_flag.load()) return false;
            
            if(are_states_collides_by_edges(m_automaton, pairState.first, pairState.second, dependent_var_num)) {
                return false;
            }
            if(are_states_collides_by_edges(m_automaton, pairState.second, pairState.first, dependent_var_num)) {
                return false;
            }
        }
        return true;
    } else {
        for (auto pairState : compatibleStates) {
            if(m_stop_flag.load()) return false;
            for (auto& t1 : m_automaton->out(pairState.first)) {
                for (auto& t2 : m_automaton->out(pairState.second)) {
                    PairEdges pair_edges = PairEdges(t1, t2);

                    if (!this->is_dependent_by_pair_edges(
                            dependent_var_num, dependency_vars_nums, remaining_vars_nums,
                            pair_edges)) {
                        return false;
                    }
                }
            }
        }
        return true;
    }
}

/**
 * @brief Checks if a variable is dependent on a set of variables given a specific pair of transition edges.
 *
 * Mathematically, a variable X is dependent on set Y if for all pairs of compatible states (s1, s2),
 * there does NOT exist an assignment that allows X to take different values while Y remains the same.
 * This is checked by verifying that the conjunction of (s1 with X=True) and (s2 with X=False, Z primed)
 * is unsatisfiable (and vice-versa), where Z are the remaining variables.
 *
 * @param dependent_var The BDD index of the variable being tested for dependency.
 * @param dependency_vars A vector of BDD indices representing the potential dependency set.
 * @param reset_vars A vector of VarIndexer objects for variables that are neither dependent nor in the dependency set.
 * @param edges The pair of edges (from compatible states) to compare.
 * @return true if the variable remains dependent across these edges.
 * @return false if a counter-assignment is found (meaning the variable is NOT dependent).
 */
bool FindInputDepsByAutomaton::is_dependent_by_pair_edges(int dependent_var,
                                                          vector<int>& dependency_vars,
                                                          vector<VarIndexer>& reset_vars,
                                                          const PairEdges& edges) {
    bdd z1(edges.first.cond);
    bdd z2(edges.second.cond);

    bddPair* pairs = bdd_newpair();
    for (auto& var : reset_vars) {
        bdd_setpair(pairs, var.var_index, var.prime_var_index);
    }
    z2 = bdd_replace(z2, pairs);

    bdd cond1 = bdd_restrict(z1, bdd_ithvar(dependent_var)) &
                bdd_restrict(z2, bdd_nithvar(dependent_var));
    bool cond1_sat = cond1 != bddfalse;
    if (cond1_sat) {
        bdd_freepair(pairs);
        return false;
    }

    bdd cond2 = bdd_restrict(z1, bdd_nithvar(dependent_var)) &
                bdd_restrict(z2, bdd_ithvar(dependent_var));
    bool cond2_sat = cond2 != bddfalse;
    if (cond2_sat) {
        bdd_freepair(pairs);
        return false;
    }

    bdd_freepair(pairs);
    return true;
}

bool FindInputDepsByAutomaton::get_all_compatible_states(std::vector<PairState>& pairStates,
                                                        const spot::twa_graph_ptr& aut) {
    unsigned init_state = aut->get_init_state_number();
    std::vector<PairState> queue = {PairState(init_state, init_state)};
    std::unordered_set<std::pair<int, int>, pair_hash> tested_pairs;

    while (!queue.empty() && !m_stop_flag.load()) {
        PairState pairState = queue.back();
        queue.pop_back();

        if (pairState.first > pairState.second) {
            std::swap(pairState.first, pairState.second);
        }

        if (tested_pairs.find(pairState) != tested_pairs.end()) {
            continue;
        }

        tested_pairs.insert(pairState);
        pairStates.emplace_back(pairState.first, pairState.second);

        for (auto& t1 : aut->out(pairState.first)) {
            for (auto& t2 : aut->out(pairState.second)) {
                if(m_stop_flag.load()) {
                    return false;
                }

                PairState newState(t1.dst, t2.dst);

                if (newState.first > newState.second) {
                    std::swap(newState.first, newState.second);
                }
                if (tested_pairs.find(newState) != tested_pairs.end()) {
                    continue;
                }

                if(t1.dst == t2.dst) {
                    queue.emplace_back(newState);
                } else {
                    bdd causal_cond1 = bdd_exist(t1.cond, m_ignored_vars_bdd);
                    bdd causal_cond2 = bdd_exist(t2.cond, m_ignored_vars_bdd);
                    if ((causal_cond1 & causal_cond2) != bddfalse) {
                        queue.emplace_back(newState);
                    }
                }
            }
        }
    }

    return queue.empty() && !m_stop_flag.load();
}
