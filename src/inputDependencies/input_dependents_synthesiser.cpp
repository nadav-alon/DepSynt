#include "input_dependents_synthesiser.h"
#include <spot/tl/parse.hh>
#include <spot/tl/print.hh>
#include <spot/twaalgos/aiger.hh>
#include <spot/twaalgos/translate.hh>
#include <string>
#include <vector>
#include <unordered_map>

using namespace std;
using namespace spot;

spot::aig_ptr InputDependentsSynthesiser::synthesis() {
    init_aiger();
    define_next_state_logic();
    define_dependency_logic();

    if (m_is_realizable == Realizability::UNREALIZABLE) {
        return nullptr;
    }
    return m_aiger;
}

spot::aig_ptr InputDependentsSynthesiser::compose_transition_and_dependency_aigers(
    const spot::aig_ptr& nba_aiger,
    const spot::aig_ptr& dep_aiger,
    const std::vector<std::string>& indep_vars,
    const std::vector<std::string>& dep_vars,
    const std::vector<std::string>& output_vars,
    const spot::bdd_dict_ptr& dict) {

    unsigned num_state_bits_nba = 0;
    while (num_state_bits_nba < nba_aiger->input_names().size() && 
           nba_aiger->input_names()[num_state_bits_nba].find("curr_s") == 0) num_state_bits_nba++;

    unsigned num_state_bits_dep = 0;
    while (num_state_bits_dep < dep_aiger->input_names().size() && 
           dep_aiger->input_names()[num_state_bits_dep].find("curr_s") == 0) num_state_bits_dep++;

    unsigned num_choice_bits = 0;
    while (num_choice_bits < nba_aiger->input_names().size() &&
           nba_aiger->input_names()[nba_aiger->input_names().size() - 1 - num_choice_bits].find("choice") == 0) num_choice_bits++;

    unsigned num_acc_bits = 0;
    while (num_acc_bits < nba_aiger->output_names().size() &&
           nba_aiger->output_names()[nba_aiger->output_names().size() - 1 - num_acc_bits].find("acc") == 0) num_acc_bits++;

    std::vector<std::string> comp_inputs;
    for (unsigned i = 0; i < num_state_bits_nba; ++i) comp_inputs.push_back("curr_s" + std::to_string(i));
    for (unsigned i = 0; i < num_state_bits_dep; ++i) comp_inputs.push_back("curr_s" + std::to_string(num_state_bits_nba + i));
    for (const auto& var : indep_vars) comp_inputs.push_back(var);
    for (const auto& var : output_vars) comp_inputs.push_back(var);
    for (unsigned i = 0; i < num_choice_bits; ++i) comp_inputs.push_back("choice" + std::to_string(i));

    std::vector<std::string> comp_outputs;
    for (unsigned i = 0; i < num_state_bits_nba; ++i) comp_outputs.push_back("next_s" + std::to_string(i));
    for (unsigned i = 0; i < num_state_bits_dep; ++i) comp_outputs.push_back("next_s" + std::to_string(num_state_bits_nba + i));
    for (unsigned i = 0; i < num_acc_bits; ++i) comp_outputs.push_back("acc" + std::to_string(i));
    for (const auto& var : dep_vars) comp_outputs.push_back(var);

    auto comp_aiger = std::make_shared<aig>(comp_inputs, comp_outputs, 0, dict);

    auto translate_gates = [&](const aig_ptr& src_aiger, const std::unordered_map<unsigned, Gate>& input_mapping) {
        auto gate_translations = std::make_shared<std::vector<Gate>>(src_aiger->num_gates());
        auto get_translated_gate = [comp_aiger, src_aiger, input_mapping, gate_translations](Gate g) -> Gate {
            if (g == src_aiger->aig_true()) return comp_aiger->aig_true();
            if (g == src_aiger->aig_false()) return comp_aiger->aig_false();
            bool is_not = (g % 2 != 0);
            Gate g_pos = is_not ? g - 1 : g;
            unsigned v_idx = (g_pos / 2) - 1;
            Gate res;
            if (v_idx < src_aiger->num_inputs() + src_aiger->num_latches()) res = input_mapping.at(v_idx);
            else res = gate_translations->at(v_idx - src_aiger->num_inputs() - src_aiger->num_latches());
            return is_not ? comp_aiger->aig_not(res) : res;
        };
        const auto& src_gates = src_aiger->gates();
        for (unsigned i = 0; i < src_gates.size(); ++i) {
            (*gate_translations)[i] = comp_aiger->aig_and(get_translated_gate(src_gates[i].first), get_translated_gate(src_gates[i].second));
        }
        return get_translated_gate;
    };

    std::unordered_map<unsigned, Gate> dep_input_mapping;
    for (unsigned i = 0; i < dep_aiger->input_names().size(); ++i) {
        std::string name = dep_aiger->input_names()[i];
        if (name.find("curr_s") == 0) {
            unsigned idx = std::stoi(name.substr(6));
            dep_input_mapping[i] = comp_aiger->input_var(num_state_bits_nba + idx);
        } else {
            bool found = false;
            for (unsigned j = 0; j < comp_inputs.size(); ++j) {
                if (comp_inputs[j] == name) { dep_input_mapping[i] = comp_aiger->input_var(j); found = true; break; }
            }
            if (!found) dep_input_mapping[i] = comp_aiger->aig_false();
        }
    }
    auto trans_dep = translate_gates(dep_aiger, dep_input_mapping);
    std::unordered_map<std::string, Gate> dep_var_gates;
    for (unsigned i = 0; i < dep_vars.size(); ++i) dep_var_gates[dep_vars[i]] = trans_dep(dep_aiger->output(i));

    std::unordered_map<unsigned, Gate> nba_input_mapping;
    for (unsigned i = 0; i < nba_aiger->input_names().size(); ++i) {
        std::string name = nba_aiger->input_names()[i];
        if (name.find("curr_s") == 0) {
            unsigned idx = std::stoi(name.substr(6));
            nba_input_mapping[i] = comp_aiger->input_var(idx);
        } else if (name.find("choice") == 0) {
            unsigned idx = std::stoi(name.substr(6));
            nba_input_mapping[i] = comp_aiger->input_var(num_state_bits_nba + num_state_bits_dep + indep_vars.size() + output_vars.size() + idx);
        } else if (dep_var_gates.count(name)) {
            nba_input_mapping[i] = dep_var_gates[name];
        } else {
            bool found = false;
            for (unsigned j = 0; j < comp_inputs.size(); ++j) {
                if (comp_inputs[j] == name) { nba_input_mapping[i] = comp_aiger->input_var(j); found = true; break; }
            }
            if (!found) nba_input_mapping[i] = comp_aiger->aig_false();
        }
    }
    auto trans_nba = translate_gates(nba_aiger, nba_input_mapping);

    unsigned out_idx = 0;
    for (unsigned i = 0; i < num_state_bits_nba; ++i) comp_aiger->set_output(out_idx++, trans_nba(nba_aiger->output(i)));
    for (unsigned i = 0; i < num_state_bits_dep; ++i) comp_aiger->set_output(out_idx++, trans_dep(dep_aiger->output(dep_vars.size() + i)));
    for (unsigned i = 0; i < num_acc_bits; ++i) comp_aiger->set_output(out_idx++, trans_nba(nba_aiger->output(num_state_bits_nba + i)));
    for (unsigned i = 0; i < dep_vars.size(); ++i) comp_aiger->set_output(out_idx++, dep_var_gates[dep_vars[i]]);

    return comp_aiger;
}

void InputDependentsSynthesiser::init_aiger() {
    unsigned num_states = m_nba_with_deps->num_states();
    unsigned num_state_bits = count_bits(num_states + 1);

    std::vector<std::string> aiger_inputs;
    for (unsigned i = 0; i < num_state_bits; ++i) {
        aiger_inputs.push_back("curr_s" + std::to_string(i));
    }
    for (const auto& var : m_indep_vars) aiger_inputs.push_back(var);
    for (const auto& var : m_output_vars) aiger_inputs.push_back(var);

    std::vector<std::string> aiger_outputs;
    for (const auto& var : m_dep_vars) aiger_outputs.push_back(var);
    for (unsigned i = 0; i < num_state_bits; ++i) {
        aiger_outputs.push_back("next_s" + std::to_string(i));
    }

    m_aiger = std::make_shared<aig>(aiger_inputs, aiger_outputs, 0, m_nba_with_deps->get_dict());

    m_bdd_to_gate_map.clear();
    unsigned input_offset = num_state_bits;
    for (const auto& var : m_indep_vars) {
        m_bdd_to_gate_map[this->ap_to_bdd_varnum(var)] = m_aiger->input_var(input_offset++);
    }
    for (const auto& var : m_output_vars) {
        m_bdd_to_gate_map[this->ap_to_bdd_varnum(var)] = m_aiger->input_var(input_offset++);
    }

    for (const auto& var : m_dep_vars) {
        deps_bdd_vars.insert(this->ap_to_bdd_varnum(var));
    }
}

Gate InputDependentsSynthesiser::bdd_to_gate(const bdd& cond, std::unordered_map<int, Gate>& cache) {
    if (cond == bddtrue) return m_aiger->aig_true();
    if (cond == bddfalse) return m_aiger->aig_false();
    
    auto it = cache.find(cond.id());
    if (it != cache.end()) return it->second;

    int var_num = bdd_var(cond);
    Gate v;
    if (m_bdd_to_gate_map.count(var_num)) {
        v = m_bdd_to_gate_map[var_num];
    } else {
        v = m_aiger->aig_false(); 
    }

    Gate high = bdd_to_gate(bdd_high(cond), cache);
    Gate low = bdd_to_gate(bdd_low(cond), cache);

    Gate res = m_aiger->aig_or(m_aiger->aig_and(v, high), m_aiger->aig_and(m_aiger->aig_not(v), low));
    return cache[cond.id()] = res;
}

Gate InputDependentsSynthesiser::safe_aig_or(std::vector<Gate>& vs) {
    if (vs.empty()) return m_aiger->aig_false();
    return m_aiger->aig_or(vs);
}

void InputDependentsSynthesiser::define_next_state_logic() {
    unsigned num_states = m_nba_with_deps->num_states();
    unsigned num_state_bits = count_bits(num_states + 1);
    
    auto get_state_cond = [&](unsigned s) {
        std::vector<Gate> bits;
        for (unsigned i = 0; i < num_state_bits; ++i) {
            Gate bit = m_aiger->input_var(i);
            if ((s >> i) & 1) bits.push_back(bit);
            else bits.push_back(m_aiger->aig_not(bit));
        }
        return m_aiger->aig_and(bits);
    };

    std::unordered_map<int, Gate> bdd_cache;
    std::vector<std::vector<Gate>> next_bits(num_state_bits);
    for (State s = 0; s < num_states; ++s) {
        Gate is_s = get_state_cond(s);
        for (auto& edge : m_nba_with_deps->out(s)) {
            bdd causal_cond_without_deps = m_bdd_to_bdd_without_deps[edge.cond.id()];
            std::vector<Gate> args = {is_s, bdd_to_gate(causal_cond_without_deps, bdd_cache)};
            Gate active = m_aiger->aig_and(args);
            for (unsigned i = 0; i < num_state_bits; ++i) {
                if ((edge.dst >> i) & 1) next_bits[i].push_back(active);
            }
        }
    }

    for (unsigned i = 0; i < num_state_bits; ++i) {
        m_aiger->set_output(m_dep_vars.size() + i, safe_aig_or(next_bits[i]));
    }
}

void InputDependentsSynthesiser::define_dependency_logic() {
    m_is_realizable = Realizability::REALIZABLE;
    unsigned num_states = m_nba_with_deps->num_states();
    unsigned num_state_bits = count_bits(num_states + 1);

    auto get_state_cond = [&](unsigned s) {
        std::vector<Gate> bits;
        for (unsigned i = 0; i < num_state_bits; ++i) {
            Gate bit = m_aiger->input_var(i);
            if ((s >> i) & 1) bits.push_back(bit);
            else bits.push_back(m_aiger->aig_not(bit));
        }
        return m_aiger->aig_and(bits);
    };

    std::unordered_map<int, Gate> bdd_cache;
    for (unsigned dep_idx = 0; dep_idx < m_dep_vars.size(); dep_idx++) {
        string& dep_var = m_dep_vars[dep_idx];
        vector<Gate> dependent_conds;

        for (State s = 0; s < num_states; s++) {
            Gate is_s = get_state_cond(s);
            for (auto& transition : m_nba_with_deps->out(s)) {
                bdd causal_cond = transition.cond;
                Gate partial_impl = get_partial_impl(causal_cond, dep_var);
                bdd causal_cond_without_deps = m_bdd_to_bdd_without_deps[transition.cond.id()];

                std::vector<Gate> args = {
                    is_s,
                    bdd_to_gate(causal_cond_without_deps, bdd_cache),
                    partial_impl
                };
                dependent_conds.emplace_back(m_aiger->aig_and(args));
            }
        }
        if(dependent_conds.empty()) {
            m_is_realizable = Realizability::UNREALIZABLE;
            return;
        }
        m_aiger->set_output(dep_idx, safe_aig_or(dependent_conds));
    }
}

Gate InputDependentsSynthesiser::get_partial_impl(const bdd& cond, const string& dep_var) {
    string partial_impl_key = std::to_string(cond.id()) + "#" + dep_var;

    if (partial_impl_cache.find(partial_impl_key) != partial_impl_cache.end()) {
        return partial_impl_cache[partial_impl_key];
    }

    std::unordered_map<int, Gate> bdd_partial_impl;
    Gate res = generate_partial_impl(cond, dep_var, bdd_partial_impl);
    partial_impl_cache[partial_impl_key] = res;
    return res;
}

Gate InputDependentsSynthesiser::generate_partial_impl(
    const bdd& cond, const string& dep_var,
    std::unordered_map<int, Gate>& bdd_partial_impl) {
    if (cond == bddtrue) return m_aiger->aig_true();
    if (cond == bddfalse) return m_aiger->aig_false();
    
    if (bdd_partial_impl.find(cond.id()) != bdd_partial_impl.end()) {
        return bdd_partial_impl[cond.id()];
    }

    Gate high_gate = generate_partial_impl(bdd_high(cond), dep_var, bdd_partial_impl);
    Gate low_gate = generate_partial_impl(bdd_low(cond), dep_var, bdd_partial_impl);

    int var_num = bdd_var(cond);
    bool is_dep_var = deps_bdd_vars.find(var_num) != deps_bdd_vars.end();
    Gate var_true_gate, var_false_gate;

    if (!is_dep_var) {
        if (m_bdd_to_gate_map.count(var_num)) {
            var_true_gate = m_bdd_to_gate_map[var_num];
        } else {
            var_true_gate = m_aiger->aig_false();
        }
        var_false_gate = m_aiger->aig_not(var_true_gate);
    } else {
        bool is_bdd_var_cur_dep = var_num == ap_to_bdd_varnum(dep_var);
        var_false_gate = is_bdd_var_cur_dep ? m_aiger->aig_false() : m_aiger->aig_true();
        var_true_gate = m_aiger->aig_true();
    }

    bdd_partial_impl[cond.id()] = m_aiger->aig_or(
        m_aiger->aig_and(var_false_gate, low_gate), m_aiger->aig_and(var_true_gate, high_gate));
    return bdd_partial_impl[cond.id()];
}
