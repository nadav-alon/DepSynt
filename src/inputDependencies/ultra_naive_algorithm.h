#ifndef ULTRA_NAIVE_ALGORITHM_H
#define ULTRA_NAIVE_ALGORITHM_H

#include <spot/twaalgos/synthesis.hh>
#include "inp_dep_utils.h"
#include "measure.h"
#include "synthesis.h"

int ultra_naive(InputDependenciesCLIOptions& options, 
                spot::aig_ptr& final_strategy, 
                SynthesisMeasure*& measure);

#endif // ULTRA_NAIVE_ALGORITHM_H