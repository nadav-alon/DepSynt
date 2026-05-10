#include "ultra_naive_algorithm.h"
#include "synt_instance.h"
#include <iostream>

using namespace std;


struct NullBuffer : std::streambuf {
    int overflow(int c) override { return c; }
};

int ultra_naive(InputDependenciesCLIOptions& options, spot::aig_ptr& final_strategy, SynthesisMeasure*& measure) {

    NullBuffer nullbuf;
    ostream nullout(&nullbuf);
    ostream& verbose = options.verbose ? std::cout : nullout;

    SynthesisCLIOptions synt_options;

    synt_options.formula = "(" + options.env_formula + ") -> (" + options.system_formula + ")";
    synt_options.inputs = options.inputs;
    synt_options.outputs = options.outputs;
    synt_options.verbose = options.verbose;
    synt_options.measures_path = options.measures_path;

    synt_options.merge_strategies = true;
    synt_options.apply_model_checking = options.apply_model_checking;
    synt_options.measure_bdd = options.measure_bdd;
    synt_options.skip_unates = options.skip_unates;
    synt_options.dependency_timeout = options.dependency_timeout;

    spot::parsed_formula pf_env = spot::parse_infix_psl(options.env_formula);
    if (pf_env.format_errors(std::cerr)) {
        return EXIT_FAILURE;
    }
    spot::parsed_formula pf_sys = spot::parse_infix_psl(options.system_formula);
    if (pf_sys.format_errors(std::cerr)) {
        return EXIT_FAILURE;
    }

    spot::formula full_formula = spot::formula::Implies(pf_env.f, pf_sys.f);

    std::vector<std::string> inputs_vec, outputs_vec;
    extract_variables(options.inputs, inputs_vec);
    extract_variables(options.outputs, outputs_vec);

    SyntInstance* synt_instance = new SyntInstance(inputs_vec, outputs_vec, full_formula);

    measure = new InpDepSyntMeasure(*synt_instance, options.measure_bdd, false);
    InpDepSyntMeasure& synt_measure = static_cast<InpDepSyntMeasure&>(*measure);

    final_strategy = nullptr;
    synthesis(synt_options, *synt_instance, synt_measure, final_strategy);

    return final_strategy == nullptr ? EXIT_FAILURE : EXIT_SUCCESS; 
}