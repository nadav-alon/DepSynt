#include <iostream>
#include <vector>
#include <string>
#include <spot/tl/parse.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/aiger.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/contains.hh>
#include "automaton_aiger_builder.h"

using namespace std;

void run_test(const string& formula_str, const spot::bdd_dict_ptr& dict) {
    cout << "Testing: " << formula_str << endl << flush;
    
    spot::formula f = spot::parse_formula(formula_str);
    if (!f) {
        cerr << "Parse error" << endl;
        return;
    }

    auto aut = spot::translator(dict).run(f);
    unsigned num_states = aut->num_states();
    
    auto aiger = AutomatonAigerBuilder::construct_transition_aiger(aut);
    
    vector<string> ap_names;
    for (const auto& ap : aut->ap()) ap_names.push_back(ap.ap_name());
    
    auto recon = AutomatonAigerBuilder::aiger_to_automaton(aiger, num_states, ap_names, dict, aut->acc());
    recon->set_init_state(aut->get_init_state_number());
    
    if (spot::are_equivalent(aut, recon)) {
        cout << "  SUCCESS" << endl << flush;
    } else {
        cout << "  FAILURE" << endl << flush;
        exit(1);
    }
}

int main() {
    auto dict = spot::make_bdd_dict();
    run_test("a U b", dict);
    run_test("G(a -> F b)", dict);
    run_test("GF(a & b)", dict);
    run_test("GF a & GF b", dict);
    run_test("(a & X a) | (a & X b)", dict);
    
    // Additional complex cases
    run_test("a R b", dict);
    run_test("F G a", dict);
    run_test("G F a", dict);
    run_test("G(a -> X F b)", dict);
    run_test("(a U b) | (c R d)", dict);
    run_test("GF a -> GF b", dict);
    run_test("G(p -> (q U r))", dict);
    run_test("X a & X X b & X X X c", dict);
    run_test("!(a U b) <-> (!a R !b)", dict);
    run_test("G(a -> (b U (c & d)))", dict);
    
    cout << "ALL TESTS PASSED!" << endl;
    return 0;
}
