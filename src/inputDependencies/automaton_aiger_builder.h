#ifndef AUTOMATON_AIGER_BUILDER_H
#define AUTOMATON_AIGER_BUILDER_H

#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/aiger.hh>
#include <string>
#include <vector>
#include <cmath>

class AutomatonAigerBuilder {
public:
    using Gate = unsigned;

    /**
     * @brief Constructs a combinational AIGER circuit representing the transition function of an automaton.
     */
    static spot::aig_ptr construct_transition_aiger(const spot::twa_graph_ptr& aut);

    /**
     * @brief Reconstructs a Spot automaton from the transition AIGER.
     */
    static spot::twa_graph_ptr aiger_to_automaton(const spot::aig_ptr& aiger, unsigned num_states, const std::vector<std::string>& ap_names, const spot::bdd_dict_ptr& dict, const spot::acc_cond& acc);

private:
    // Helper to calculate required bits
    static unsigned count_bits(unsigned n) {
        if (n == 0) return 0;
        if (n == 1) return 0;
        unsigned bits = 0;
        unsigned m = n - 1;
        while (m > 0) {
            m >>= 1;
            bits++;
        }
        return bits;
    }
};

#endif // AUTOMATON_AIGER_BUILDER_H
