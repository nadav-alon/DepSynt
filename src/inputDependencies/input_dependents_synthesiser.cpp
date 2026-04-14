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
    define_output_gates();

    if (m_is_realizable == Realizability::UNREALIZABLE) {
        return nullptr;
    }
    return m_aiger;
}

void InputDependentsSynthesiser::init_aiger() {
    // AIGER inputs: indep inputs + outputs. dep inputs are calculated from indep inputs
    std::vector<std::string> aiger_inputs;
    
    std::copy(m_indep_vars.begin(), m_indep_vars.end(),
              std::back_inserter(aiger_inputs));

    std::copy(m_output_vars.begin(), m_output_vars.end(),
              std::back_inserter(aiger_inputs));

    unsigned num_latches = m_nba_with_deps->num_states();
    m_aiger = std::make_shared<aig>(aiger_inputs, m_dep_vars, num_latches,
                                    m_nba_with_deps->get_dict());

    for (auto& var : m_dep_vars) {
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

        vector<Gate> next_latch_conds;
        for (auto& src_and_cond : trans) {
            State src_gate = m_aiger->latch_var(src_and_cond.first);
            Gate cond_gate = src_and_cond.second;
            next_latch_conds.emplace_back(m_aiger->aig_and(src_gate, cond_gate));
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

                std::vector<unsigned> dependent_edge_cond = {
                    m_aiger->latch_var(src),
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
