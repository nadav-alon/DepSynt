#include "synt_instance.h"

#include <iostream>
#include <spot/twaalgos/aiger.hh>
#include <vector>
#include <stdexcept>
#include <future>
#include <chrono>
#include <thread>


#include "dependents_synthesiser.h"
#include "find_deps_by_automaton.h"
#include "merge_strategies.h"
#include "handle_unates_by_heuristic.h"
#include "nba_utils.h"
#include "synthesis_utils.h"
#include "find_input_deps_by_automaton.h"
#include "input_dependents_synthesiser.h"

using namespace std;
using namespace spot;

/**
 * @brief Performs synthesis by decomposing the outputs into independent and dependent variables.
 *
 * @return true if both independent and dependent variables are realizable.
 * @return false if either is unrealizable (calls synt_measure.completed() and dump_measures() before returning).
 */
/**
 * @brief Finds dependencies and updates the independent and dependent variable vectors.
 */
void find_dependencies_in_decomposition(
    SynthesisCLIOptions& options,
    SynthesisMeasure& synt_measure,
    SyntInstance& synt_instance,
    spot::twa_graph_ptr& nba,
    ostream& verbose,
    vector<string>& independent_variables,
    vector<string>& dependent_variables)
{
    bool skip_dependencies = options.dependency_timeout <= 0;
    if (skip_dependencies) {
        verbose << "=> Skipping finding and ejecting dependencies" << endl;
        independent_variables = synt_instance.get_output_vars();
    } else {
        FindDepsByAutomaton automaton_dependencies(synt_instance, synt_measure,
                                                   nba, false);

        std::future<void> fut = std::async(std::launch::async, [&] {
            automaton_dependencies.find_dependencies(dependent_variables,
                                                     independent_variables, false);
        });
        if (fut.wait_for(std::chrono::milliseconds (options.dependency_timeout)) == std::future_status::timeout) {
            automaton_dependencies.stop();
        }
        while (!automaton_dependencies.is_done()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }

        verbose << "Found " << dependent_variables.size()
                << " dependent variables" << endl;
    }
}

/**
 * @brief Prepares the NBA by cloning it and removing dependent atomic propositions.
 */
void prepare_nba_for_decomposition(
    SynthesisMeasure& synt_measure,
    spot::twa_graph_ptr& nba,
    const vector<string>& dependent_variables,
    spot::twa_graph_ptr& nba_with_deps,
    spot::twa_graph_ptr& nba_without_deps,
    unordered_map<int, bdd>& bdd_to_bdd_without_deps)
{
    bool found_dependencies = !dependent_variables.empty();
    if (found_dependencies) {
        synt_measure.start_clone_nba_with_deps();
        nba_with_deps = clone_nba(nba);
        synt_measure.end_clone_nba_with_deps();

        synt_measure.start_remove_dependent_ap();
        remove_ap_from_automaton(nba, const_cast<vector<string>&>(dependent_variables), bdd_to_bdd_without_deps);
        synt_measure.end_remove_dependent_ap(nba);
    }
    nba_without_deps = nba;
}

/**
 * @brief Synthesizes the dependent variables.
 *
 * @return true if successful, false if unrealizable.
 */
bool synthesise_dependents(
    SynthesisCLIOptions& options,
    SynthesisMeasure& synt_measure,
    spot::twa_graph_ptr nba_without_deps,
    spot::twa_graph_ptr nba_with_deps,
    vector<string>& input_vars,
    vector<string>& independent_variables,
    vector<string>& dependent_variables,
    unordered_map<int, bdd>& bdd_to_bdd_without_deps,
    spot::aig_ptr& deps_strategy)
{
    deps_strategy = nullptr;
    if(!dependent_variables.empty()) {
        synt_measure.start_dependents_synthesis();
        DependentsSynthesiser dependents_synt(nba_without_deps, nba_with_deps,
                                              input_vars, independent_variables,
                                              dependent_variables, bdd_to_bdd_without_deps);
        deps_strategy = dependents_synt.synthesis();
        synt_measure.end_dependents_synthesis(deps_strategy);
    }

    if(!dependent_variables.empty() && deps_strategy == nullptr) {
        cout << "UNREALIZABLE" << endl;
        synt_measure.completed();
        dump_measures(synt_measure, options);
        return false;
    }
    return true;
}

/**
 * @brief Performs synthesis by decomposing the outputs into independent and dependent variables.
 *
 * @return true if both independent and dependent variables are realizable.
 * @return false if either is unrealizable (calls synt_measure.completed() and dump_measures() before returning).
 */
bool decompose_synthesis(
    SynthesisCLIOptions& options,
    SynthesisMeasure& synt_measure,
    spot::synthesis_info& gi,
    SyntInstance& synt_instance,
    spot::twa_graph_ptr& nba,
    vector<string>& input_vars,
    ostream& verbose,
    spot::aig_ptr& indeps_strategy,
    spot::aig_ptr& deps_strategy,
    vector<string>& independent_variables,
    vector<string>& dependent_variables)
{
    find_dependencies_in_decomposition(options, synt_measure, synt_instance, nba, verbose,
                                        independent_variables, dependent_variables);

    twa_graph_ptr nba_without_deps = nullptr, nba_with_deps = nullptr;
    unordered_map<int, bdd> bdd_to_bdd_without_deps;
    prepare_nba_for_decomposition(synt_measure, nba, dependent_variables, nba_with_deps,
                                   nba_without_deps, bdd_to_bdd_without_deps);

    // Synthesis the independent variables
    indeps_strategy = nullptr;
    if(!independent_variables.empty()) {
        vector<string> indep_outs = dependent_variables.empty()
                        ? synt_instance.get_output_vars()
                        : independent_variables;
        synt_measure.start_independents_synthesis();
        indeps_strategy = synthesis_nba_to_aiger(gi, nba_without_deps, indep_outs, input_vars, verbose);
        synt_measure.end_independents_synthesis(indeps_strategy);
    }

    // Check Realizability of independent variables
    if(!independent_variables.empty() && indeps_strategy == nullptr) {
        cout << "UNREALIZABLE" << endl;
        synt_measure.completed();
        dump_measures(synt_measure, options);
        return false;
    }

    return synthesise_dependents(options, synt_measure, nba_without_deps, nba_with_deps, input_vars,
                                 independent_variables, dependent_variables, bdd_to_bdd_without_deps, deps_strategy);
}

/**
 * @brief Performs synthesis by decomposing the outputs but only extracts the strategy of the dependencies.
 *
 * @return true if dependent variables are realizable (note: independently variables are not synthesized).
 * @return false if dependent variables are unrealizable.
 */
bool decompose_synthesis_only_dependents(
    SynthesisCLIOptions& options,
    SynthesisMeasure& synt_measure,
    spot::synthesis_info& gi,
    SyntInstance& synt_instance,
    spot::twa_graph_ptr& nba,
    vector<string>& input_vars,
    ostream& verbose,
    spot::aig_ptr& deps_strategy,
    vector<string>& independent_variables,
    vector<string>& dependent_variables)
{
    find_dependencies_in_decomposition(options, synt_measure, synt_instance, nba, verbose,
                                        independent_variables, dependent_variables);

    twa_graph_ptr nba_without_deps = nullptr, nba_with_deps = nullptr;
    unordered_map<int, bdd> bdd_to_bdd_without_deps;
    prepare_nba_for_decomposition(synt_measure, nba, dependent_variables, nba_with_deps,
                                   nba_without_deps, bdd_to_bdd_without_deps);

    return synthesise_dependents(options, synt_measure, nba_without_deps, nba_with_deps, input_vars,
                                 independent_variables, dependent_variables, bdd_to_bdd_without_deps, deps_strategy);
}

/**
 * @brief Performs synthesis by decomposing the outputs but only extracts the strategy of the dependencies as an automaton.
 *
 * @return true if dependent variables are realizable.
 * @return false if dependent variables are unrealizable.
 */
bool decompose_synthesis_only_dependents_as_aut(
    SynthesisCLIOptions& options,
    SynthesisMeasure& synt_measure,
    spot::synthesis_info& gi,
    SyntInstance& synt_instance,
    spot::twa_graph_ptr& nba,
    vector<string>& input_vars,
    ostream& verbose,
    spot::twa_graph_ptr& deps_strategy_aut,
    vector<string>& independent_variables,
    vector<string>& dependent_variables)
{
    spot::aig_ptr deps_strategy = nullptr;
    if (!decompose_synthesis_only_dependents(options, synt_measure, gi, synt_instance, nba, input_vars, verbose,
                                             deps_strategy, independent_variables, dependent_variables)) {
        return false;
    }

    if (deps_strategy != nullptr) {
        // EXPONENTIAL IN INPUTS
        deps_strategy_aut = deps_strategy->as_automaton(false);
    } else {
        deps_strategy_aut = nullptr;
    }
    return true;
}
struct NullBuffer : std::streambuf {
    int overflow(int c) override { return c; }
};

int synthesis(SynthesisCLIOptions options, SyntInstance& synt_instance, SynthesisMeasure& synt_measure, spot::aig_ptr& final_strategy, spot::twa_graph_ptr precalc_nba = nullptr) {
    NullBuffer nullbuf;
    ostream nullout(&nullbuf);
    ostream& verbose = options.verbose ? std::cout : nullout;

    verbose << "==== Starting Synthesis ====" << endl;
    verbose << "=> Loaded Options: " << endl;
    verbose << options << endl;

    // Prepare synthesis info
    spot::synthesis_info gi;
    gi.s = spot::synthesis_info::algo::SPLIT_DET;
    gi.minimize_lvl = 2;  // i.e, simplication level
    vector<string> input_vars(synt_instance.get_input_vars());

    if(synt_instance.get_output_vars().empty()) {
        cout << "UNKNOWN (No Output)" << endl;
        synt_measure.completed();

        dump_measures(synt_measure, options);

        return EXIT_SUCCESS;
    }

    try {
        // Get NBA for synthesis
        spot::twa_graph_ptr nba = nullptr;
        if (precalc_nba != nullptr) {
            nba = precalc_nba;
        } else {
             nba = get_nba_for_synthesis(synt_instance.get_formula_parsed(), gi, synt_measure, verbose);
        }

        // Handle Unate
        if(options.skip_unates) {
            verbose << "=> Skipping finding and handling Unates" << endl;
        } else {
            verbose << "=> Finding and handling Unates" << endl;
            unsigned init_state = nba->get_init_state_number();

            // Init find unate code
#if USE_UNATE_COMPLEMENT
            HandleUnatesByComplement find_unates(nba, synt_instance, synt_measure);
#else
            HandleUnatesByHeuristic find_unates(nba, synt_instance, synt_measure);
#endif
            find_unates.run();

            assert(init_state == nba->get_init_state_number() && "Find Unate changed the automaton original state");
        }

        // Handle Dependent variables
        vector<string> dependent_variables, independent_variables;
        spot::aig_ptr indeps_strategy = nullptr;
        spot::aig_ptr deps_strategy = nullptr;

        if (!decompose_synthesis(options, synt_measure, gi, synt_instance, nba, input_vars, verbose,
                                 indeps_strategy, deps_strategy, independent_variables, dependent_variables)) {
            return EXIT_SUCCESS;
        }

        // Merge strategies
        if(options.merge_strategies) {
            synt_measure.start_merge_strategies();
            final_strategy = merge_strategies(
                    indeps_strategy, deps_strategy, input_vars,
                    independent_variables, dependent_variables, gi.dict, options.model_name);
            synt_measure.end_merge_strategies(final_strategy);
        }

        // Print the strategy
        if(final_strategy != nullptr) {
            spot::print_aiger(std::cout, final_strategy) << '\n';
        } else if(indeps_strategy != nullptr || deps_strategy != nullptr) {
            if (indeps_strategy != nullptr) {
                spot::print_aiger(std::cout, indeps_strategy) << '\n';
            }
            if (deps_strategy != nullptr) {
                spot::print_aiger(std::cout, deps_strategy) << '\n';
            }
        } else {
            throw std::runtime_error("No strategy was found");
        }

        if (options.apply_model_checking) {
            assert(final_strategy != nullptr && "Model checking is only supported when merging strategies");
            synt_measure.start_model_checking();

            spot::translator trans(gi.dict, &gi.opt);
            auto neg_spec =
                trans.run(spot::formula::Not(synt_instance.get_formula_parsed()));

            auto strategy_aut = final_strategy->as_automaton(false);
            bool model_checking_ok = !neg_spec->intersects(strategy_aut);

            if (model_checking_ok) {
                verbose << "=> Model checking: OK" << endl;
            } else {
                cerr << "=> Model checking: Error - Strategy intersects with "
                        "negation of specificaiton"
                     << endl;
            }

            synt_measure.end_model_checking(model_checking_ok ? "OK" : "Error");
        }
        // Print Measures
        synt_measure.completed();
        dump_measures(synt_measure, options);

        return EXIT_SUCCESS;
    } catch (const std::runtime_error& re) {
        std::cerr << "Runtime error: " << re.what() << std::endl;
        dump_measures(synt_measure, options);
        return EXIT_FAILURE;
    } catch (const std::exception& ex) {
        std::cerr << "Error occurred: " << ex.what() << std::endl;
        dump_measures(synt_measure, options);
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Unknown failure occurred. Possible memory corruption"
                  << std::endl;
        dump_measures(synt_measure, options);
        return EXIT_FAILURE;
    }

    verbose << "==== Synthesis Completed ====" << endl;
    return EXIT_SUCCESS;
}

void find_input_dependencies_in_decomposition(
    SynthesisCLIOptions& options,
    SynthesisMeasure& synt_measure,
    SyntInstance& synt_instance,
    spot::twa_graph_ptr& negated_nba,
    ostream& verbose,
    vector<string>& independent_variables,
    vector<string>& dependent_variables,
    const vector<string>& ignored_vars)
{
    bool skip_dependencies = options.dependency_timeout <= 0;
    if (skip_dependencies) {
        verbose << "=> Skipping finding and ejecting dependencies" << endl;
        independent_variables = synt_instance.get_input_vars();
    } else {
        FindInputDepsByAutomaton automaton_dependencies(synt_instance, synt_measure,
                                                         negated_nba, false, ignored_vars);

        std::future<void> fut = std::async(std::launch::async, [&] {
            automaton_dependencies.find_dependencies(dependent_variables,
                                                     independent_variables, false);
        });
        if (fut.wait_for(std::chrono::milliseconds (options.dependency_timeout)) == std::future_status::timeout) {
            automaton_dependencies.stop();
        }
        // TODO: This polling loop with sleep is sub-optimal. 
        // Consider using fut.wait() for standard C++ synchronization.
        while (!automaton_dependencies.is_done()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
        }

        verbose << "Found " << dependent_variables.size()
                << " dependent variables" << endl;
    }
}

bool synthesise_input_dependents(
    SynthesisCLIOptions& options,
    SynthesisMeasure& synt_measure,
    spot::twa_graph_ptr nba_without_deps,
    spot::twa_graph_ptr nba_with_deps,
    vector<string>& input_vars,
    vector<string>& output_vars,
    vector<string>& independent_variables,
    vector<string>& dependent_variables,
    unordered_map<int, bdd>& bdd_to_bdd_without_deps,
    spot::aig_ptr& deps_strategy,
    const vector<string>& ignored_vars)
{
    deps_strategy = nullptr;
    if(!dependent_variables.empty()) {
        synt_measure.start_dependents_synthesis();
        InputDependentsSynthesiser dependents_synt(nba_without_deps,
                                                   nba_with_deps,
                                                   input_vars,
                                                   output_vars,
                                                   independent_variables,
                                                   dependent_variables,
                                                   bdd_to_bdd_without_deps);
        deps_strategy = dependents_synt.synthesis();
        synt_measure.end_dependents_synthesis(deps_strategy);
    }

    if(!dependent_variables.empty() && deps_strategy == nullptr) {
        cout << "UNREALIZABLE" << endl;
        synt_measure.completed();
        dump_measures(synt_measure, options);
        return false;
    }
    return true;
}

bool decompose_synthesis_only_input_dependents(
    SynthesisCLIOptions& options, SynthesisMeasure& synt_measure,
    spot::synthesis_info& gi, SyntInstance& synt_instance, spot::twa_graph_ptr& nba,
    vector<string>& input_vars, vector<string>& output_vars, ostream& verbose,
    spot::aig_ptr& deps_strategy, vector<string>& independent_variables,
    vector<string>& dependent_variables, const vector<string>& ignored_vars)
{
    twa_graph_ptr negated_nba = construct_automaton_negation(synt_instance, gi.dict);

    find_input_dependencies_in_decomposition(options,
                                             synt_measure,
                                             synt_instance,
                                             negated_nba,
                                             verbose,
                                             independent_variables,
                                             dependent_variables,
                                             ignored_vars);

    twa_graph_ptr negated_nba_without_deps = nullptr, negated_nba_with_deps = nullptr;
    unordered_map<int, bdd> bdd_to_bdd_without_deps;
    prepare_nba_for_decomposition(synt_measure,
                                   negated_nba,
                                   dependent_variables,
                                   negated_nba_with_deps,
                                   negated_nba_without_deps,
                                   bdd_to_bdd_without_deps);

    return synthesise_input_dependents(options,
                                     synt_measure,
                                     negated_nba_without_deps,
                                     negated_nba_with_deps,
                                     input_vars,
                                     output_vars,
                                     independent_variables,
                                     dependent_variables,
                                     bdd_to_bdd_without_deps,
                                     deps_strategy,
                                     ignored_vars);
}

bool decompose_synthesis_only_input_dependents_as_aut(
    SynthesisCLIOptions& options, SynthesisMeasure& synt_measure,
    spot::synthesis_info& gi, SyntInstance& synt_instance, spot::twa_graph_ptr& nba,
    vector<string>& input_vars, vector<string>& output_vars, ostream& verbose,
    spot::twa_graph_ptr& deps_strategy_aut, vector<string>& independent_variables,
    vector<string>& dependent_variables, const vector<string>& ignored_vars)
{
    spot::aig_ptr deps_strategy = nullptr;
    if (!decompose_synthesis_only_input_dependents(options, synt_measure, gi, synt_instance, nba, input_vars, output_vars, verbose,
                                                   deps_strategy, independent_variables, dependent_variables, ignored_vars)) {
        return false;
    }

    if (deps_strategy != nullptr) {
        deps_strategy_aut = deps_strategy->as_automaton(false);
    } else {
        deps_strategy_aut = nullptr;
    }
    return true;
}