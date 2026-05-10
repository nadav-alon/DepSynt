#ifndef INP_DEP_UTILS_H
#define INP_DEP_UTILS_H

#include "utils.h"

enum Inp_Dep_Algorithm { ULTRA_NAIVE = 0, NAIVE = 1 };

Inp_Dep_Algorithm string_to_inp_dep_algorithm(const std::string &str);
std::string inp_dep_algorithm_to_string(Inp_Dep_Algorithm algo);

struct InputDependenciesCLIOptions: public BaseCLIOptions {
    std::string env_formula;
    std::string system_formula;
    std::string formula;
    bool measure_bdd;
    Inp_Dep_Algorithm algorithm;
    int dependency_timeout;
    bool apply_model_checking;
    std::string model_name;
    bool skip_unates;
};

std::ostream &operator<<(std::ostream &out, const InputDependenciesCLIOptions &options);

bool parse_input_dependencies_synthesis_cli(int argc, const char* argv[], InputDependenciesCLIOptions& options);

#endif