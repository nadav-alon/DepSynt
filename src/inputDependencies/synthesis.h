#ifndef INP_DEP_SYNTHESIS_H
#define INP_DEP_SYNTHESIS_H

#include "synt_instance.h"
#include "synt_measure.h"
#include "utils.h"
#include <iostream>
#include <spot/twaalgos/aiger.hh>
#include <spot/twaalgos/synthesis.hh>
#include <vector>

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
    vector<string>& dependent_variables);


/**
 * @brief Prepares the NBA by cloning it and removing dependent atomic propositions.
 */
void prepare_nba_for_decomposition(
    SynthesisMeasure& synt_measure,
    spot::twa_graph_ptr& nba,
    const vector<string>& dependent_variables,
    spot::twa_graph_ptr& nba_with_deps,
    spot::twa_graph_ptr& nba_without_deps,
    unordered_map<int, bdd>& bdd_to_bdd_without_deps,
    const vector<string>& ignored_vars);


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
    spot::aig_ptr& deps_strategy);


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
    vector<string>& dependent_variables);


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
    vector<string>& dependent_variables);


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
    vector<string>& dependent_variables);

void find_input_dependencies_in_decomposition(
    SynthesisCLIOptions& options,
    SynthesisMeasure& synt_measure,
    SyntInstance& synt_instance,
    spot::twa_graph_ptr& nba,
    ostream& verbose,
    vector<string>& independent_variables,
    vector<string>& dependent_variables,
    const vector<string>& ignored_vars);

bool synthesise_input_dependents(
    SynthesisCLIOptions& options,
    SynthesisMeasure& synt_measure,
    spot::twa_graph_ptr nba_without_deps,
    spot::twa_graph_ptr nba_with_deps,
    vector<string>& input_vars,
    vector<string>& independent_variables,
    vector<string>& dependent_variables,
    unordered_map<int, bdd>& bdd_to_bdd_without_deps,
    spot::aig_ptr& deps_strategy);

bool decompose_synthesis_only_input_dependents_as_aut(
    SynthesisCLIOptions& options,
    SynthesisMeasure& synt_measure,
    spot::synthesis_info& gi,
    SyntInstance& synt_instance,
    spot::twa_graph_ptr& nba,
    vector<string>& input_vars,
    vector<string>& output_vars,
    ostream& verbose,
    spot::twa_graph_ptr& deps_strategy_aut,
    vector<string>& independent_variables,
    vector<string>& dependent_variables);


int synthesis(SynthesisCLIOptions options, SyntInstance& synt_instance, SynthesisMeasure& synt_measure, spot::aig_ptr& final_strategy, spot::twa_graph_ptr precalc_nba = nullptr);

#endif // INP_DEP_SYNTHESIS_H