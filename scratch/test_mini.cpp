#include "automaton_aiger_builder.h"
#include <spot/tl/parse.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/contains.hh>
#include <spot/twa/twagraph.hh>
#include <iostream>

void run_test(const std::string& formula_str, const spot::bdd_dict_ptr& dict) {
    auto f = spot::parse_formula(formula_str);
    auto aut = spot::translator(dict).run(f);
    std::cout << "Testing: " << formula_str << " (States: " << aut->num_states() << ")" << std::endl;
    auto aiger = AutomatonAigerBuilder::construct_transition_aiger(aut);
    
    std::vector<std::string> ap_names;
    for (auto& ap : aut->ap()) ap_names.push_back(ap.ap_name());
    
    auto recon = AutomatonAigerBuilder::aiger_to_automaton(aiger, aut->num_states(), ap_names, dict, aut->acc());
    recon->set_init_state(aut->get_init_state_number());
    std::cout << "  Reconstruction finished" << std::endl;

    if (spot::are_equivalent(aut, recon)) {
        std::cout << "  Equivalence check: SUCCESS" << std::endl;
    } else {
        std::cout << "  Equivalence check: FAILURE" << std::endl;
    }
}

int main() {
    auto dict = spot::make_bdd_dict();
    run_test("a U b", dict);
    run_test("G(a -> F b)", dict);
    return 0;
}
