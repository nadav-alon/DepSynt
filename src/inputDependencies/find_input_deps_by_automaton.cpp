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
            bool is_const = false;
            if (dependency_set.empty()) {
                bool val;
                is_const = is_global_constant(dependent_var, val);
            }
            m_measures.end_testing_variable(true, is_const, dependency_set);
        } else {
            independent_variables.push_back(dependent_var);
            m_measures.end_testing_variable(false, false, dependency_set);
        }
    }

    independent_variables.insert(independent_variables.end(), candidates.begin(),
                                 candidates.end());

    m_measures.end_find_deps(!m_stop_flag.load());
    m_is_done.store(true);
}

void FindInputDepsByAutomaton::find_contextual_dependencies(bool use_single_bdd) {
    m_measures.start_find_deps();

    std::vector<std::string> candidates = m_synt_instance.get_input_vars();
    std::vector<std::string> available_as_dependency_vars = candidates;

    for (const auto& var : candidates) {
        if (m_stop_flag.load()) break;
        
        string var_copy = var;
        m_measures.start_testing_variable(var_copy);
        
        std::vector<std::string> dependency_set;
        for (const auto& v : available_as_dependency_vars) {
            if (v != var) dependency_set.push_back(v);
        }

        // Include outputs as potential dependencies (non-causal)
        for (const auto& out_var : m_synt_instance.get_output_vars()) {
            dependency_set.push_back(out_var);
        }

        this->check_contextual_deps(var, dependency_set);
        
        // If it was found to be even partially dependent, remove it from available set for others to avoid cycles
        if (m_dependency_density.count(var) && m_dependency_density[var] > 0) {
            auto it = std::find(available_as_dependency_vars.begin(), available_as_dependency_vars.end(), var);
            if (it != available_as_dependency_vars.end()) {
                available_as_dependency_vars.erase(it);
            }
            m_measures.end_testing_variable(true, false, dependency_set);
        } else {
            m_measures.end_testing_variable(false, false, dependency_set);
        }
    }

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
                    if ((t1.cond & t2.cond) != bddfalse) {
                        queue.emplace_back(newState);
                    }
                }
            }
        }
    }

    return queue.empty() && !m_stop_flag.load();
}

bool FindInputDepsByAutomaton::is_global_constant(const std::string& var, bool& value_dst) {
    int var_num = m_bdd_cacher->get_variable_index(var);
    bdd var_bdd = bdd_ithvar(var_num);
    
    bool first = true;
    bool found_value = false;

    for (unsigned s = 0; s < m_automaton->num_states(); ++s) {
        for (auto& edge : m_automaton->out(s)) {
            // Check if this transition allows var=true
            if ((edge.cond & var_bdd) != bddfalse) {
                if (first) {
                    found_value = true;
                    first = false;
                } else if (!found_value) {
                    return false; // Found both true and false
                }
            }
            // Check if this transition allows var=false
            if ((edge.cond & !var_bdd) != bddfalse) {
                if (first) {
                    found_value = false;
                    first = false;
                } else if (found_value) {
                    return false; // Found both true and false
                }
            }
        }
    }
    
    value_dst = found_value;
    return !first;
}

/**
 * @brief Analyzes contextual (state-based) dependencies for a given input variable.
 * 
 * This function identifies states and pairs of states where an input variable 'var' is 
 * deterministically constrained by a set of dependency variables. It quantifies out 
 * all other variables to determine if 'var''s value is uniquely forced to satisfy 
 * at least one non-loop outgoing transition.
 * 
 * The analysis populates:
 * - m_state_dep_functions: Maps states to the BDD condition (over dependency_vars) 
 *   under which 'var' must be true.
 * - m_conflict_pairs: Lists pairs of states (i, j) where 'var' could take 
 *   different values for the same dependency assignment, potentially violating dependency.
 * - m_dependency_density: The ratio of states where 'var' is uniquely determined.
 * 
 * @param var The input variable to analyze.
 * @param dependency_vars The set of potential variables (inputs and outputs) 'var' depends on.
 */
void FindInputDepsByAutomaton::check_contextual_deps(const string& var, const vector<string>& dependency_vars) {
    int v_num = m_bdd_cacher->get_variable_index(var);
    bdd v_bdd = bdd_ithvar(v_num);
    bdd not_v_bdd = bdd_nithvar(v_num);
    
    std::unordered_set<string> deps(dependency_vars.begin(), dependency_vars.end());
    
    // Variables to quantify out: Everything NOT in dependency_vars and NOT the dependent var itself
    bdd vars_to_quantify = bddtrue;
    for (const auto& in_var : m_synt_instance.get_input_vars()) {
        if (in_var != var && deps.find(in_var) == deps.end()) {
            vars_to_quantify &= bdd_ithvar(m_bdd_cacher->get_variable_index(in_var));
        }
    }
    for (const auto& out_var : m_synt_instance.get_output_vars()) {
        if (deps.find(out_var) == deps.end()) {
            vars_to_quantify &= bdd_ithvar(m_bdd_cacher->get_variable_index(out_var));
        }
    }

    // We also quantify out the dependent variable itself to see if it's "possible" for a given X
    bdd vars_to_quantify_with_v = vars_to_quantify & v_bdd;

    std::vector<bdd> state_conds_v1(m_automaton->num_states(), bddfalse);
    std::vector<bdd> state_conds_v0(m_automaton->num_states(), bddfalse);
    std::vector<bdd> state_conds_any(m_automaton->num_states(), bddfalse);

    for (unsigned s = 0; s < m_automaton->num_states(); ++s) {
        bdd combined_cond = bddfalse;
        bool has_non_loop = false;
        for (auto& edge : m_automaton->out(s)) {
            if (edge.dst != s) {
                combined_cond |= edge.cond;
                has_non_loop = true;
            }
        }
        
        // If all edges are loops, then we have no "winning" move to follow at this step
        if (!has_non_loop) {
            state_conds_v1[s] = bddtrue;
            state_conds_v0[s] = bddtrue;
        } else {
            state_conds_v1[s] = bdd_exist(combined_cond & v_bdd, vars_to_quantify_with_v);
            state_conds_v0[s] = bdd_exist(combined_cond & not_v_bdd, vars_to_quantify_with_v);
        }
        
        state_conds_any[s] = bdd_exist(combined_cond, vars_to_quantify_with_v);
        m_state_dep_functions[var][s] = state_conds_v1[s];
    }

    std::vector<std::pair<unsigned, unsigned>> conflicts;
    unsigned deterministic_count = 0;

    for (unsigned i = 0; i < m_automaton->num_states(); ++i) {
        if ((state_conds_v1[i] & state_conds_v0[i]) != bddfalse) {
            conflicts.push_back({i, i});
        } else {
            deterministic_count++;
        }

        for (unsigned j = i + 1; j < m_automaton->num_states(); ++j) {
            if ((state_conds_v1[i] & state_conds_v0[j]) != bddfalse ||
                (state_conds_v1[j] & state_conds_v0[i]) != bddfalse) {
                conflicts.push_back({i, j});
            }
        }
    }

    m_conflict_pairs[var] = conflicts;
    m_dependency_density[var] = (double)deterministic_count / m_automaton->num_states();
}
