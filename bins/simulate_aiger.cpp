#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <spot/twaalgos/aiger.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/synthesis.hh>
#include <spot/twaalgos/mealy_machine.hh>

using namespace std;

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <aiger_file> [input_sequence...]" << endl;
        cerr << "Example: " << argv[0] << " transducer.aag 01 11 10" << endl;
        return 1;
    }

    string filename = argv[1];
    auto dict = spot::make_bdd_dict();
    try {
        auto aig = spot::aig::parse_aag(filename, dict);
        if (!aig) {
            cerr << "Error: Failed to parse AIGER file " << filename << endl;
            return 1;
        }

        auto aut = aig->as_automaton();
        if (!aut) {
            cerr << "Error: Failed to convert AIGER to automaton" << endl;
            return 1;
        }

        // Get inputs and outputs for printing information
        auto& inputs = aig->input_names();
        auto& outputs = aig->output_names();

        cout << "Transducer loaded from " << filename << endl;
        cout << "Inputs (" << inputs.size() << "): ";
        for (auto& in : inputs) cout << in << " ";
        cout << endl;
        cout << "Outputs (" << outputs.size() << "): ";
        for (auto& out : outputs) cout << out << " ";
        cout << endl << endl;

        unsigned curr_state = aut->get_init_state_number();

        for (int i = 2; i < argc; ++i) {
            string in_vec = argv[i];
            if (in_vec.length() != inputs.size()) {
                cerr << "Error: Input vector '" << in_vec << "' has length " << in_vec.length() 
                     << ", expected " << inputs.size() << endl;
                continue;
            }

            // Construct BDD for the input
            bdd input_bdd = bddtrue;
            for (size_t j = 0; j < inputs.size(); ++j) {
                if (in_vec[j] == '1') {
                    input_bdd &= aig->input_bdd(j);
                } else {
                    input_bdd &= aig->input_bdd(j, true);
                }
            }

            // Find transition
            bool transition_found = false;
            for (auto& edge : aut->out(curr_state)) {
                if ((edge.cond & input_bdd) != bddfalse) {
                    // This edge matches
                    cout << "Step " << i-1 << ": Input " << in_vec << " -> Output ";
                    
                    bdd step_cond = edge.cond & input_bdd;
                    // Evaluate outputs
                    for (size_t j = 0; j < outputs.size(); ++j) {
                        int var = aut->register_ap(outputs[j]);
                        if ((step_cond & bdd_ithvar(var)) != bddfalse) {
                            cout << "1";
                        } else if ((step_cond & bdd_nithvar(var)) != bddfalse) {
                            cout << "0";
                        } else {
                            cout << "?";
                        }
                    }
                    cout << " (Next State: " << edge.dst << ")" << endl;
                    curr_state = edge.dst;
                    transition_found = true;
                    break;
                }
            }

            if (!transition_found) {
                cerr << "Error: No transition found for input " << in_vec << " in state " << curr_state << endl;
                break;
            }
        }

    } catch (const runtime_error& e) {
        cerr << "Runtime error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
