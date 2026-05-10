#include "automaton_aiger_builder.h"
#include <spot/twaalgos/aiger.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/sccfilter.hh>
#include <unordered_map>
#include <algorithm>
#include <iostream>

using namespace spot;
using Gate = AutomatonAigerBuilder::Gate;

// Recursive BDD to AIG conversion helper
static Gate bdd_to_aig(const bdd& cond, const std::shared_ptr<aig>& aiger, std::unordered_map<int, Gate>& cache) {
    if (cond == bddtrue) return aiger->aig_true();
    if (cond == bddfalse) return aiger->aig_false();
    
    auto it = cache.find(cond.id());
    if (it != cache.end()) return it->second;

    Gate v = aiger->bdd2aigvar(bdd_ithvar(bdd_var(cond)));
    Gate high = bdd_to_aig(bdd_high(cond), aiger, cache);
    Gate low = bdd_to_aig(bdd_low(cond), aiger, cache);

    // ITE: (v AND high) OR (NOT v AND low)
    Gate res = aiger->aig_or(aiger->aig_and(v, high), aiger->aig_and(aiger->aig_not(v), low));
    return cache[cond.id()] = res;
}

static Gate my_aig_and(const std::vector<Gate>& vs, const std::shared_ptr<aig>& aiger) {
    if (vs.empty()) return aiger->aig_true();
    Gate res = vs[0];
    for (size_t i = 1; i < vs.size(); ++i) res = aiger->aig_and(res, vs[i]);
    return res;
}

static Gate my_aig_or(const std::vector<Gate>& vs, const std::shared_ptr<aig>& aiger) {
    if (vs.empty()) return aiger->aig_false();
    Gate res = vs[0];
    for (size_t i = 1; i < vs.size(); ++i) res = aiger->aig_or(res, vs[i]);
    return res;
}

// NOTE: We avoid using local spot::acc_cond::mark_t objects on the stack to prevent
// false stack-protector triggers that may occur due to ABI mismatches with 
// pre-compiled Spot libraries in certain environments.
spot::aig_ptr AutomatonAigerBuilder::construct_transition_aiger(const spot::twa_graph_ptr& aut) {
    unsigned num_states = aut->num_states();
    unsigned num_state_bits = count_bits(num_states + 1);
    unsigned invalid_state_id = num_states;
    
    unsigned max_out_degree = 0;
    for (unsigned s = 0; s < num_states; ++s) {
        unsigned deg = 0;
        for (auto& edge : aut->out(s)) {
            (void)edge;
            deg++;
        }
        max_out_degree = std::max(max_out_degree, deg);
    }
    unsigned num_choice_bits = count_bits(max_out_degree);

    auto dict = aut->get_dict();
    std::vector<std::string> ap_names;
    for (auto& ap : aut->ap()) {
        ap_names.push_back(ap.ap_name());
    }

    std::vector<std::string> aiger_inputs;
    for (unsigned i = 0; i < num_state_bits; ++i) {
        aiger_inputs.push_back("curr_s" + std::to_string(i));
    }
    for (const auto& ap : ap_names) {
        aiger_inputs.push_back(ap);
    }
    for (unsigned i = 0; i < num_choice_bits; ++i) {
        aiger_inputs.push_back("choice" + std::to_string(i));
    }

    unsigned num_acc_sets = aut->acc().num_sets();
    bool add_dummy_acc = (num_acc_sets == 0);
    if (add_dummy_acc) num_acc_sets = 1;

    std::vector<std::string> aiger_outputs;
    for (unsigned i = 0; i < num_state_bits; ++i) {
        aiger_outputs.push_back("next_s" + std::to_string(i));
    }
    for (unsigned i = 0; i < num_acc_sets; ++i) {
        aiger_outputs.push_back("acc" + std::to_string(i));
    }

    auto aiger = std::make_shared<aig>(aiger_inputs, aiger_outputs, 0, dict);

    std::vector<std::vector<Gate>> next_state_bits_logic(num_state_bits);
    std::vector<std::vector<Gate>> acc_bits_logic(num_acc_sets);
    std::vector<Gate> all_transitions_active_list;
    all_transitions_active_list.reserve(num_states * max_out_degree);
    std::unordered_map<int, Gate> bdd_cache;

    for (unsigned s = 0; s < num_states; ++s) {
        std::vector<Gate> s_bits_cond;
        for (unsigned i = 0; i < num_state_bits; ++i) {
            Gate bit_input = aiger->input_var(i);
            if ((s >> i) & 1) s_bits_cond.push_back(bit_input);
            else s_bits_cond.push_back(aiger->aig_not(bit_input));
        }
        Gate is_state_s = my_aig_and(s_bits_cond, aiger);

        unsigned edge_idx = 0;
        for (auto& edge : aut->out(s)) {
            std::vector<Gate> choice_bits_cond;
            for (unsigned i = 0; i < num_choice_bits; ++i) {
                Gate bit_input = aiger->input_var(num_state_bits + ap_names.size() + i);
                if ((edge_idx >> i) & 1) choice_bits_cond.push_back(bit_input);
                else choice_bits_cond.push_back(aiger->aig_not(bit_input));
            }
            Gate is_choice_j = my_aig_and(choice_bits_cond, aiger);
            Gate valuation_matches = bdd_to_aig(edge.cond, aiger, bdd_cache);

            std::vector<Gate> trans_gates = {is_state_s, is_choice_j, valuation_matches};
            Gate transition_active = my_aig_and(trans_gates, aiger);
            all_transitions_active_list.push_back(transition_active);

            unsigned dst = edge.dst;
            for (unsigned i = 0; i < num_state_bits; ++i) {
                if ((dst >> i) & 1) next_state_bits_logic.at(i).push_back(transition_active);
            }

            for (unsigned i = 0; i < num_acc_sets; ++i) {
                if (add_dummy_acc || edge.acc.has(i)) acc_bits_logic.at(i).push_back(transition_active);
            }
            edge_idx++;
        }
    }

    Gate any_active = aiger->aig_false();
    if (!all_transitions_active_list.empty()) {
        any_active = my_aig_or(all_transitions_active_list, aiger);
    }
    Gate none_active = aiger->aig_not(any_active);

    for (unsigned i = 0; i < num_state_bits; ++i) {
        if ((invalid_state_id >> i) & 1) {
            next_state_bits_logic.at(i).push_back(none_active);
        }
    }

    for (unsigned i = 0; i < num_state_bits; ++i) {
        Gate bit_output = my_aig_or(next_state_bits_logic.at(i), aiger);
        aiger->set_output(i, bit_output);
    }

    for (unsigned i = 0; i < num_acc_sets; ++i) {
        Gate bit_output = my_aig_or(acc_bits_logic.at(i), aiger);
        aiger->set_output(num_state_bits + i, bit_output);
    }

    return aiger;
}

spot::twa_graph_ptr AutomatonAigerBuilder::aiger_to_automaton(const spot::aig_ptr& aiger, unsigned /*num_states*/, const std::vector<std::string>& ap_names, const spot::bdd_dict_ptr& dict, const spot::acc_cond& acc) {
    auto aut = make_twa_graph(dict);
    
    unsigned num_state_bits = 0;
    while (num_state_bits < aiger->input_names().size() && 
           aiger->input_names()[num_state_bits].find("curr_s") == 0) {
        num_state_bits++;
    }
    
    unsigned num_states = 1 << num_state_bits;
    unsigned num_acc_bits = 0;
    while (num_state_bits + num_acc_bits < aiger->num_outputs() &&
           aiger->output_names()[num_state_bits + num_acc_bits].find("acc") == 0) {
        num_acc_bits++;
    }

    if (num_acc_bits > 0 && acc.num_sets() == 0) {
        aut->set_acceptance(1, "Inf(0)");
    } else {
        aut->set_acceptance(acc.num_sets(), acc.get_acceptance());
    }
    
    unsigned num_acc_sets = num_acc_bits;

    unsigned num_choice_bits = aiger->num_inputs() - num_state_bits - ap_names.size();
    unsigned max_choices = 1 << num_choice_bits;
    aut->new_states(num_states);

    for (unsigned s = 0; s < num_states; ++s) {
        bdd state_restriction = bddtrue;
        for (unsigned k = 0; k < num_state_bits; ++k) {
            bdd var_bdd = aiger->input_bdd(k);
            if ((s >> k) & 1) state_restriction &= var_bdd;
            else state_restriction &= bdd_not(var_bdd);
        }

        for (unsigned i = 0; i < max_choices; ++i) {
            bdd choice_restriction = bddtrue;
            for (unsigned k = 0; k < num_choice_bits; ++k) {
                bdd var_bdd = aiger->input_bdd(num_state_bits + ap_names.size() + k);
                if ((i >> k) & 1) choice_restriction &= var_bdd;
                else choice_restriction &= bdd_not(var_bdd);
            }

            bdd combined_restriction = state_restriction & choice_restriction;
            std::vector<bdd> restricted_bits;
            for (unsigned k = 0; k < num_state_bits; ++k) {
                Gate out_var = aiger->output(k);
                if (out_var == (Gate)-1) {
                    std::cerr << "Error: Output " << k << " is -1" << std::endl;
                    return nullptr;
                }
                bdd bit_bdd = aiger->aigvar2bdd(out_var);
                restricted_bits.push_back(bdd_restrict(bit_bdd, combined_restriction));
            }

            std::vector<bdd> restricted_acc_bits;
            for (unsigned k = 0; k < num_acc_sets; ++k) {
                Gate out_var = aiger->output(num_state_bits + k);
                if (out_var == (Gate)-1) {
                    std::cerr << "Error: Acc Output " << k << " is -1" << std::endl;
                    return nullptr;
                }
                bdd bit_bdd = aiger->aigvar2bdd(out_var);
                restricted_acc_bits.push_back(bdd_restrict(bit_bdd, combined_restriction));
            }

            if (s == 2) {
                std::cout << "For s=2, choice=" << i << ", restricted_bits: " 
                          << (restricted_bits[0] == bddtrue) << ", " 
                          << (restricted_bits[1] == bddtrue) << std::endl;
            }

            for (unsigned s_prime = 0; s_prime < num_states; ++s_prime) {
                bdd trans_cond = bddtrue;
                for (unsigned k = 0; k < num_state_bits; ++k) {
                    if ((s_prime >> k) & 1) trans_cond &= restricted_bits.at(k);
                    else trans_cond &= bdd_not(restricted_bits.at(k));
                }
                if (trans_cond != bddfalse) {
                    std::vector<unsigned> marks;
                    for (unsigned k = 0; k < num_acc_sets; ++k) {
                        if ((trans_cond & restricted_acc_bits.at(k)) != bddfalse) {
                            marks.push_back(k);
                        }
                    }
                    aut->new_edge(s, s_prime, trans_cond, spot::acc_cond::mark_t(marks.begin(), marks.end()));
                }
            }
        }
    }
    aut->merge_edges(); 
    aut = spot::scc_filter_states(aut);
    return aut;
}
