
#include <spot/twaalgos/synthesis.hh>
#include "inp_dep_utils.h"
#include "synt_instance.h"
#include "measure.h"
#include "synthesis.cpp"

using namespace std;

static InpDepSyntMeasure* g_synt_measure = nullptr;

int ultra_naive(InputDependenciesCLIOptions& options, spot::aig_ptr& final_strategy) {

    ostream nullout(nullptr);
    ostream& verbose = options.verbose ? std::cout : nullout;

    spot::synthesis_info gi;
    gi.s = spot::synthesis_info::algo::SPLIT_DET;
    gi.minimize_lvl = 2;  // i.e, simplication level

    SynthesisCLIOptions synt_options;

    synt_options.formula = "(" + options.env_formula + ") -> (" + options.system_formula + ")";
    synt_options.inputs = options.inputs;
    synt_options.outputs = options.outputs;
    synt_options.verbose = options.verbose;
    synt_options.measures_path = options.measures_path;

    synt_options.merge_strategies = true;
    synt_options.apply_model_checking = options.apply_model_checking;
    synt_options.measure_bdd = options.measure_bdd;
    synt_options.skip_unates = true;
    synt_options.dependency_timeout = options.dependency_timeout;

    SyntInstance synt_instance(options.inputs, options.outputs, synt_options.formula);

    g_synt_measure = new InpDepSyntMeasure(synt_instance, false, false);
    InpDepSyntMeasure& synt_measure = *g_synt_measure;

    synt_measure.set_measure_bdd(options.measure_bdd);

    final_strategy = nullptr;
    synthesis(synt_options, synt_instance, synt_measure, final_strategy);

    return final_strategy == nullptr ? EXIT_FAILURE : EXIT_SUCCESS; 
}