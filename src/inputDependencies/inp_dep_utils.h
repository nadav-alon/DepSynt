#ifndef INP_DEP_UTILS_H
#define INP_DEP_UTILS_H

#include "utils.h"
#include "synt_measure.h"
#include <spot/twa/fwd.hh>
#include <vector>
#include <string>

enum Inp_Dep_Algorithm { ULTRA_NAIVE = 0, NAIVE = 1, NAIVE_PROJECTED = 2};

Inp_Dep_Algorithm string_to_inp_dep_algorithm(const std::string &str);
std::string inp_dep_algorithm_to_string(Inp_Dep_Algorithm algo);

struct InputDependenciesCLIOptions: public BaseCLIOptions {
    std::string env_formula;
    std::string system_formula;
    bool measure_bdd;
    Inp_Dep_Algorithm algorithm;
    int dependency_timeout;
    bool apply_model_checking;
    std::string model_name;
};

std::ostream &operator<<(std::ostream &out, const InputDependenciesCLIOptions &options);

bool parse_input_dependencies_synthesis_cli(int argc, const char* argv[], InputDependenciesCLIOptions& options);

bool get_inp_dep_transducer(
    const std::string& formula_str,
    const std::vector<std::string>& input_vars,
    const std::vector<std::string>& output_vars,
    const InputDependenciesCLIOptions& options,
    SynthesisMeasure& measure,
    std::ostream& verbose,
    spot::twa_graph_ptr& out_deps_nba,
    spot::twa_graph_ptr& out_deps_strategy_aut);

#endif