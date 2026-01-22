#include <signal.h>
#include <iostream>
#include <spot/twaalgos/aiger.hh>
#include <stdexcept>


#include "../src/inputDependencies/inp_dep_utils.h"
#include "../src/inputDependencies/ultra_naive_algorithm.h"

using namespace std;
using namespace spot;

static InputDependenciesCLIOptions options;
static SynthesisMeasure* g_synt_measure;

void on_sighup(int args);

int main(int argc, const char* argv[]) {
    int parsed_cli_status = parse_input_dependencies_synthesis_cli(argc, argv, options);
    if (!parsed_cli_status) {
        return EXIT_FAILURE;
    }
    ostream nullout(nullptr);
    ostream& verbose = options.verbose ? std::cout : nullout;

    verbose << "=> Loaded Options: " << endl;
    verbose << options << endl;

    signal(SIGINT, on_sighup);
    signal(SIGTERM, on_sighup);
    signal(SIGHUP, on_sighup);
    
    spot::aig_ptr final_strategy = nullptr;

    if (options.algorithm == Inp_Dep_Algorithm::ULTRA_NAIVE) {
        ultra_naive(options, final_strategy);
    } else if (options.algorithm == Inp_Dep_Algorithm::NAIVE) {
        // return naive_inp_dep_synthesis(options, final_strategy);
    } else if (options.algorithm == Inp_Dep_Algorithm::NAIVE_PROJECTED) {
        // return naive_projected_inp_dep_synthesis(options, final_strategy);
    }

    if (final_strategy != nullptr) {
        verbose << "=> Strategy found" << endl;
        dump_measures(*g_synt_measure, options);
        return EXIT_SUCCESS;
    }

    verbose << "=> Strategy not found" << endl;
    dump_measures(*g_synt_measure, options);
    return EXIT_FAILURE;

}

void on_sighup(int args) {
    try {
        dump_measures(*g_synt_measure, options);
    } catch (const std::runtime_error& re) {
        std::cout << "Runtime error: " << re.what() << std::endl;
        dump_measures(*g_synt_measure, options);
    } catch (const std::exception& ex) {
        std::cout << "Error occurred: " << ex.what() << std::endl;
        dump_measures(*g_synt_measure, options);
    } catch (...) {
        std::cout << "Unknown failure occurred. Possible memory corruption"
                  << std::endl;
        dump_measures(*g_synt_measure, options);
    }

    exit(EXIT_SUCCESS);
}
