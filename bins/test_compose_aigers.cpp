#include <iostream>
#include <vector>
#include <string>
#include <spot/tl/parse.hh>
#include <spot/twaalgos/translate.hh>
#include <spot/twaalgos/aiger.hh>
#include <spot/twa/twagraph.hh>
#include <spot/twaalgos/dot.hh>
#include <fstream>
#include <filesystem>
#include "input_dependents_synthesiser.h"
#include "automaton_aiger_builder.h"

using namespace std;
using namespace spot;

struct Step {
    vector<pair<string, bool>> inputs;
    vector<pair<string, bool>> expected_outputs;
};

typedef vector<Step> Trace;

void run_composition_test(const string& name, 
                          spot::twa_graph_ptr aut, 
                          spot::aig_ptr dep_aiger,
                          const vector<string>& indep_vars,
                          const vector<string>& dep_vars,
                          const vector<string>& output_vars,
                          unsigned expected_states,
                          const vector<string>& comp_ap_names,
                          const vector<Trace>& traces = {}) {
    cout << "--- Testing: " << name << " ---" << endl;
    auto dict = aut->get_dict();
    
    cout << "  Constructing NBA AIGER... (num_states = " << aut->num_states() << ")" << endl;
    auto nba_aiger = AutomatonAigerBuilder::construct_transition_aiger(aut);
    
    cout << "  Composing AIGERs..." << endl;
    auto comp_aiger = InputDependentsSynthesiser::compose_transition_and_dependency_aigers(
        nba_aiger, dep_aiger, indep_vars, dep_vars, output_vars, dict
    );
    
    auto comp_aut = AutomatonAigerBuilder::aiger_to_automaton(comp_aiger, expected_states, comp_ap_names, dict, aut->acc());
    cout << "  Reconstructed States: " << comp_aut->num_states() << endl;

    string safe_name = name;
    for (char& c : safe_name) {
        if (c == ' ' || c == '(' || c == ')' || c == '=' || c == '>') c = '_';
    }
    std::filesystem::path vis_dir = std::filesystem::path("visualizations") / safe_name;
    std::filesystem::create_directories(vis_dir);

    std::ofstream orig_aut_out(vis_dir / "original_automaton.dot");
    spot::print_dot(orig_aut_out, aut);

    std::ofstream transducer_aag_out(vis_dir / "transducer.aag");
    spot::print_aiger(transducer_aag_out, dep_aiger);
    
    std::ofstream transducer_dot_out(vis_dir / "transducer.dot");
    spot::print_dot(transducer_dot_out, dep_aiger);

    std::ofstream comb_aut_out(vis_dir / "combined_automaton.dot");
    spot::print_dot(comb_aut_out, comp_aut);

    if (!traces.empty()) {
        cout << "  Simulating traces..." << endl;
        unsigned num_state_bits = 0;
        while (num_state_bits < comp_aiger->input_names().size() && 
               comp_aiger->input_names()[num_state_bits].find("curr_s") == 0) num_state_bits++;

        for (const auto& trace : traces) {
            comp_aiger->circ_init();
            unsigned curr_state_val = 0; 
            
            for (size_t t = 0; t < trace.size(); ++t) {
                const auto& step = trace[t];
                vector<bool> inputs(comp_aiger->num_inputs(), false);
                for (unsigned b = 0; b < num_state_bits; ++b) {
                    inputs[b] = (curr_state_val >> b) & 1;
                }
                for (const auto& pr : step.inputs) {
                    for (unsigned k = 0; k < comp_aiger->input_names().size(); ++k) {
                        if (comp_aiger->input_names()[k] == pr.first) {
                            inputs[k] = pr.second;
                            break;
                        }
                    }
                }
                
                comp_aiger->circ_step(inputs);
                
                // Verify expected outputs
                for (const auto& pr : step.expected_outputs) {
                    bool found = false;
                    for (unsigned k = 0; k < comp_aiger->output_names().size(); ++k) {
                        if (comp_aiger->output_names()[k] == pr.first) {
                            bool actual = comp_aiger->circ_state_of(comp_aiger->output(k));
                            if (actual != pr.second) {
                                cout << "  FAILED: Step " << t << " output " << pr.first 
                                     << " expected " << pr.second << " got " << actual << endl;
                                return;
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        cout << "  FAILED: Step " << t << " expected output '" << pr.first 
                             << "' was not found among the composed AIGER outputs." << endl;
                        return;
                    }
                }

                curr_state_val = 0;
                for (unsigned b = 0; b < num_state_bits; ++b) {
                    if (comp_aiger->circ_state_of(comp_aiger->output(b))) {
                        curr_state_val |= (1 << b);
                    }
                }
            }
        }
        cout << "  Simulation finished." << endl;
    }
    
    cout << "  SUCCESS" << endl << endl;
}

void test_static_dependency() {
    auto dict = spot::make_bdd_dict();
    auto f = spot::parse_formula("G(o -> i)");
    auto aut = spot::translator(dict).run(f);
    
    vector<string> dep_vars = {"i"};
    auto dep_aiger = make_shared<aig>(vector<string>{"o"}, dep_vars, 0, dict);
    dep_aiger->set_output(0, dep_aiger->input_var(0)); // i = o
    
    run_composition_test("Static Dependency (i=o)", aut, dep_aiger, {}, {"i"}, {"o"}, aut->num_states(), {"o"});
}

void test_sequential_manual() {
    auto dict = spot::make_bdd_dict();
    auto f = spot::parse_formula("G(o -> i)");
    auto aut = spot::translator(dict).run(f);
    
    vector<string> dep_vars = {"i"};
    auto dep_aiger = make_shared<aig>(vector<string>{"curr_s0", "o"}, vector<string>{"i", "next_s0"}, 0, dict);
    dep_aiger->set_output(0, dep_aiger->input_var(0)); // i = curr_s0
    dep_aiger->set_output(1, dep_aiger->input_var(1)); // next_s0 = o
    
    Trace trace = {
        {{{"o", false}}, {{"i", false}}},
        {{{"o", true}},  {{"i", false}}},
        {{{"o", false}}, {{"i", true}}},
        {{{"o", true}},  {{"i", false}}}
    };
    
    run_composition_test("Sequential Manual (i=prev(o))", aut, dep_aiger, {}, {"i"}, {"o"}, aut->num_states() * 2, {"o"}, {trace});
}

void test_sequential_input_dependency() {
    auto dict = spot::make_bdd_dict();
    auto f = spot::parse_formula("i & G(X i <-> o)");
    auto aut_with_deps = spot::translator(dict).run(f);
    auto aut_without_deps = spot::translator(dict).run(spot::parse_formula("G(1)")); 
    
    vector<string> input_vars = {"i"};
    vector<string> output_vars = {"o"};
    vector<string> indep_vars = {};
    vector<string> dep_vars = {"i"};
    
    unordered_map<int, bdd> bdd_map;
    bdd dep_cube = bddtrue;
    for (auto v : dep_vars) dep_cube &= bdd_ithvar(dict->varnum(spot::formula::ap(v)));
    for (auto& edge : aut_with_deps->edges()) {
        bdd_map[edge.cond.id()] = bdd_exist(edge.cond, dep_cube); 
    }
    
    InputDependentsSynthesiser synthesiser(aut_without_deps, aut_with_deps, input_vars, output_vars, indep_vars, dep_vars, bdd_map);
    auto dep_aiger = synthesiser.synthesis();
    
    Trace trace = {
        {{{"o", false}}, {{"i", true}}},
        {{{"o", true}},  {{"i", false}}},
        {{{"o", false}}, {{"i", true}}}
    };
    
    run_composition_test("Sequential Input Dependency Result", aut_with_deps, dep_aiger, indep_vars, dep_vars, output_vars, 4, {"o"}, {trace});
}

void test_mutex_dependency() {
    auto dict = spot::make_bdd_dict();
    auto f = spot::parse_formula("G(i1 ^ i2)");
    auto aut_with_deps = spot::translator(dict).run(f);
    auto aut_without_deps = spot::translator(dict).run(spot::parse_formula("G(1)")); 
    
    vector<string> input_vars = {"i1", "i2"};
    vector<string> output_vars = {"o"};
    vector<string> indep_vars = {"i2"};
    vector<string> dep_vars = {"i1"};
    
    unordered_map<int, bdd> bdd_map;
    bdd dep_cube = bddtrue;
    for (auto v : dep_vars) dep_cube &= bdd_ithvar(dict->varnum(spot::formula::ap(v)));
    for (auto& edge : aut_with_deps->edges()) {
        bdd_map[edge.cond.id()] = bdd_exist(edge.cond, dep_cube); 
    }
    
    InputDependentsSynthesiser synthesiser(aut_without_deps, aut_with_deps, input_vars, output_vars, indep_vars, dep_vars, bdd_map);
    auto dep_aiger = synthesiser.synthesis();
    
    Trace trace = {
        {{{"i2", false}, {"o", false}}, {{"i1", true}}},
        {{{"i2", true},  {"o", true}},  {{"i1", false}}},
        {{{"i2", false}, {"o", false}}, {{"i1", true}}}
    };
    
    run_composition_test("Mutex Dependency Result", aut_with_deps, dep_aiger, indep_vars, dep_vars, output_vars, 4, {"o"}, {trace});
}

void test_multiple_variables() {
    auto dict = spot::make_bdd_dict();
    auto f = spot::parse_formula("G(o -> i) & G(p -> q)");
    auto aut_with_deps = spot::translator(dict).run(f);
    auto aut_without_deps = spot::translator(dict).run(spot::parse_formula("G(1)")); 
    
    vector<string> input_vars = {"i", "q"};
    vector<string> output_vars = {"o", "p"};
    vector<string> indep_vars = {};
    vector<string> dep_vars = {"i", "q"};
    
    unordered_map<int, bdd> bdd_map;
    bdd dep_cube = bddtrue;
    for (auto v : dep_vars) dep_cube &= bdd_ithvar(dict->varnum(spot::formula::ap(v)));
    for (auto& edge : aut_with_deps->edges()) {
        bdd_map[edge.cond.id()] = bdd_exist(edge.cond, dep_cube); 
    }
    
    InputDependentsSynthesiser synthesiser(aut_without_deps, aut_with_deps, input_vars, output_vars, indep_vars, dep_vars, bdd_map);
    auto dep_aiger = synthesiser.synthesis();
    
    run_composition_test("Multiple Variables Result", aut_with_deps, dep_aiger, indep_vars, dep_vars, output_vars, 2, {"o", "p"});
}

int main() {
    try {
        test_static_dependency();
        test_sequential_manual();
        test_sequential_input_dependency();
        test_mutex_dependency();
        test_multiple_variables();
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        return 1;
    }
    return 0;
}
