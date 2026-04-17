#include "input_dependents_synthesiser.h"
#include <algorithm>
#include <iostream>
#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/twaalgos/aiger.hh>
#include <spot/twaalgos/translate.hh>
#include <string>
#include <vector>

using namespace std;
using namespace spot;

spot::aig_ptr InputDependentsSynthesiser::synthesis() {
    init_aiger();
    define_next_latches();

    if (m_is_realizable == Realizability::UNREALIZABLE) {
        return nullptr;
    }
    
    define_unified_output_gates();
    
    return m_aiger;
}

void InputDependentsSynthesiser::init_aiger() {
    // AIGER inputs: indep inputs (those not currently being "monitored"?) 
    // Wait, the monitor should take ALL inputs and outputs to update its belief state.
    // But it SHOULDN'T use the current dependent input values to predict themselves.
    // However, AIGER typically has inputs and outputs.
    // We'll treat all environment inputs and system outputs as monitor inputs.
    std::vector<std::string> aiger_inputs;
    std::copy(m_input_vars.begin(), m_input_vars.end(), std::back_inserter(aiger_inputs));
    std::copy(m_output_vars.begin(), m_output_vars.end(), std::back_inserter(aiger_inputs));

    // Monitor outputs: for each input v, we have out_v_val and out_v_is_dep
    std::vector<std::string> monitor_outputs;
    for (const auto& var : m_input_vars) {
        monitor_outputs.push_back("out_" + var + "_val");
        monitor_outputs.push_back("out_" + var + "_is_dep");
    }

    unsigned num_latches = m_nba_with_deps->num_states() + 1;
    m_aiger = std::make_shared<aig>(aiger_inputs, monitor_outputs, num_latches,
                                    m_nba_with_deps->get_dict());

    for (auto& var : m_input_vars) {
        deps_bdd_vars.insert(this->ap_to_bdd_varnum(var));
    }
}


/**
 * @brief Defines the transition logic for the AIGER circuit's latches.
 *
 * Each state in the automaton corresponds to a latch in the AIGER model.
 * This function iterates through all transitions in the pruned automaton
 * (nba_without_deps) and builds the logic (OR of ANDs) that determines
 * the next value of each latch based on current latches and inputs.
 */
void InputDependentsSynthesiser::define_next_latches() {
    unordered_map<State, std::vector<StateGatePair>> dst_transitions;

    for (State state = 0; state < m_nba_without_deps->num_states(); state++) {
        for (auto& transition : m_nba_without_deps->out(state)) {
            State src = transition.src;
            State dst = transition.dst;
            
            dst_transitions[dst].emplace_back(src, m_aiger->bdd2INFvar(transition.cond));
        }
    }

    for (auto& trans_to_dst : dst_transitions) {
        State dst = trans_to_dst.first;
        auto& trans = trans_to_dst.second;

        Gate started_gate = m_aiger->latch_var(m_nba_without_deps->num_states());
        State init_state = m_nba_without_deps->get_init_state_number();

        vector<Gate> next_latch_conds;
        for (auto& src_and_cond : trans) {
            Gate src_latch = m_aiger->latch_var(src_and_cond.first);
            Gate is_in_src = (src_and_cond.first == init_state) 
                ? m_aiger->aig_or(src_latch, m_aiger->aig_not(started_gate))
                : src_latch;
            Gate cond_gate = src_and_cond.second;
            next_latch_conds.emplace_back(m_aiger->aig_and(is_in_src, cond_gate));
        }

        Gate next_latch_gate;
        if (next_latch_conds.size() == 1) {
            next_latch_gate = next_latch_conds[0];
        } else {
            assert(!next_latch_conds.empty());
            next_latch_gate = m_aiger->aig_or(next_latch_conds);
        }
        m_aiger->set_next_latch(dst, next_latch_gate);
    }

    // Set next for the 'started' latch
    m_aiger->set_next_latch(m_nba_without_deps->num_states(), m_aiger->aig_true());

    for (State state = 0; state < m_nba_without_deps->num_states(); state++) {
        if (dst_transitions.find(state) == dst_transitions.end()) {
            m_aiger->set_next_latch(state, m_aiger->aig_false());
        }
    }
}

void InputDependentsSynthesiser::define_output_gates() {
    m_is_realizable = Realizability::REALIZABLE;

    for (unsigned dep_idx = 0; dep_idx < m_dep_vars.size(); dep_idx++) {
        string& dep_var = m_dep_vars[dep_idx];
        vector<Gate> dependent_conds;

        for (State state = 0; state < m_nba_with_deps->num_states(); state++) {
            for (auto& transition : m_nba_with_deps->out(state)) {
                State src = transition.src;
                
                // Causal Fix: Exists-quantify system output vars from the condition.
                // This ensures the choice of dependent variable value doesn't "peek" at simultaneous system outputs.
                bdd causal_cond = bdd_exist(transition.cond, m_output_vars_bdd);
                Gate partial_impl = get_partial_impl(causal_cond, dep_var);
                
                // Also quantify from the version without deps used for the gate activation
                bdd cond_without_deps = m_bdd_to_bdd_without_deps[transition.cond.id()];
                bdd causal_cond_without_deps = bdd_exist(cond_without_deps, m_output_vars_bdd);

                Gate started_gate = m_aiger->latch_var(m_nba_with_deps->num_states());
                State init_state = m_nba_with_deps->get_init_state_number();
                Gate src_latch = m_aiger->latch_var(src);
                Gate is_in_src = (src == init_state)
                    ? m_aiger->aig_or(src_latch, m_aiger->aig_not(started_gate))
                    : src_latch;

                std::vector<unsigned> dependent_edge_cond = {
                    is_in_src,
                    m_aiger->bdd2INFvar(causal_cond_without_deps),
                    partial_impl
                };
                dependent_conds.emplace_back(m_aiger->aig_and(dependent_edge_cond));
            }
        }

        if(dependent_conds.empty()) {
            m_is_realizable = Realizability::UNREALIZABLE;
            return;
        }
        m_aiger->set_output(dep_idx, m_aiger->aig_or(dependent_conds));
    }
}

void InputDependentsSynthesiser::define_unified_output_gates() {
    for (unsigned i = 0; i < m_input_vars.size(); ++i) {
        const string& var = m_input_vars[i];
        
        // 1. Synthesize is_dep gate
        // is_dep = NOT(OR_{(s1, s2) in conflicts} (Latch_s1 AND Latch_s2))
        vector<Gate> conflict_gates;
        auto conflict_it = m_conflict_pairs.find(var);
        if (conflict_it != m_conflict_pairs.end()) {
            for (const auto& pair : conflict_it->second) {
                Gate l1 = m_aiger->latch_var(pair.first);
                Gate l2 = m_aiger->latch_var(pair.second);
                conflict_gates.push_back(m_aiger->aig_and(l1, l2));
            }
        }
        
        Gate is_dep_gate;
        if (conflict_gates.empty()) {
            is_dep_gate = m_aiger->aig_true();
        } else {
            is_dep_gate = m_aiger->aig_not(m_aiger->aig_or(conflict_gates));
        }
        
        // 2. Synthesize val gate
        // val = OR_{s in States} (Latch_s AND get_partial_impl(f_s, var))
        vector<Gate> val_conds;
        auto func_it = m_state_dep_functions.find(var);
        if (func_it != m_state_dep_functions.end()) {
            for (const auto& state_func : func_it->second) {
                unsigned state = state_func.first;
                bdd func = state_func.second;
                
                Gate latch = m_aiger->latch_var(state);
                Gate implementation = get_partial_impl(func, const_cast<string&>(var));
                val_conds.push_back(m_aiger->aig_and(latch, implementation));
            }
        }
        
        Gate val_gate = val_conds.empty() ? m_aiger->aig_false() : m_aiger->aig_or(val_conds);
        
        // Set AIGER outputs: val at offset 2*i, is_dep at offset 2*i + 1
        m_aiger->set_output(2 * i, val_gate);
        m_aiger->set_output(2 * i + 1, is_dep_gate);
    }
}

Gate InputDependentsSynthesiser::get_partial_impl(const bdd& cond, string& dep_var) {
    string partial_impl_key = std::to_string(cond.id()) + "#" + dep_var;

    // If exists in cache
    if (partial_impl_cache.find(partial_impl_key) != partial_impl_cache.end()) {
        return partial_impl_cache[partial_impl_key];
    }

    // Create partial implementation
    unordered_map<int, Gate> bdds_partial_impl;
    Gate partial_impl = generate_partial_impl(cond, dep_var, bdds_partial_impl);

    // Store to cache and return it
    partial_impl_cache[partial_impl_key] = partial_impl;
    return partial_impl_cache[partial_impl_key];
}

Gate InputDependentsSynthesiser::generate_partial_impl(
    const bdd& cond, string& dep_var, unordered_map<int, Gate>& bdd_partial_impl) {
    if (cond == bddtrue) {
        return m_aiger->aig_true();
    }
    if (cond == bddfalse) {
        return m_aiger->aig_false();
    }
    if (bdd_partial_impl.find(cond.id()) != bdd_partial_impl.end()) {
        return bdd_partial_impl[cond.id()];
    }

    // Post-order traversal
    Gate high_gate =
        generate_partial_impl(bdd_high(cond), dep_var, bdd_partial_impl);
    Gate low_gate = generate_partial_impl(bdd_low(cond), dep_var, bdd_partial_impl);

    bool is_dep_var = deps_bdd_vars.find(bdd_var(cond)) != deps_bdd_vars.end();
    Gate var_true_gate, var_false_gate;

    // Determine the AIGER gates for the current BDD variable's branches
    if (!is_dep_var) {
        // The variable is an independent input/output; map it to its AIGER gate.
        var_true_gate = m_aiger->bdd2aigvar(bdd_ithvar(bdd_var(cond)));
        var_false_gate = m_aiger->aig_not(var_true_gate);
    } else {
        // The variable is a dependent input.
        bool is_bdd_var_cur_dep = bdd_var(cond) == ap_to_bdd_varnum(dep_var);

        // If it's the current target variable, disable the false branch.
        var_false_gate = is_bdd_var_cur_dep ? m_aiger->aig_false() : m_aiger->aig_true();
        var_true_gate = m_aiger->aig_true();
    }


    bdd_partial_impl[cond.id()] = m_aiger->aig_or(
        m_aiger->aig_and(var_false_gate, low_gate), m_aiger->aig_and(var_true_gate, high_gate));
    return bdd_partial_impl[cond.id()];
}
