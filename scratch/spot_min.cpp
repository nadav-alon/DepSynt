#include <iostream>
#include <spot/tl/parse.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/aiger.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/contains.hh>
#include "automaton_aiger_builder.h"

using namespace std;
using namespace spot;

int main() {
    auto dict = make_bdd_dict();
    auto f = parse_formula("GF a & GF b");
    auto aut = translator(dict).run(f);
    auto aiger = AutomatonAigerBuilder::construct_transition_aiger(aut);
    
    vector<string> ap_names;
    for (const auto& ap : aut->ap()) ap_names.push_back(ap.ap_name());
    
    cout << "Reconstructing..." << endl;
    auto recon = AutomatonAigerBuilder::aiger_to_automaton(aiger, aut->num_states(), ap_names, dict, aut->acc());
    recon->set_init_state(aut->get_init_state_number());
    
    cout << "Checking equivalence..." << endl;
    if (are_equivalent(aut, recon)) {
        cout << "SUCCESS!" << endl;
    } else {
        cout << "FAILURE!" << endl;
    }
    return 0;
}
