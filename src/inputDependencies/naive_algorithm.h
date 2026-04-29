#ifndef NAIVE_ALGORITHM_H
#define NAIVE_ALGORITHM_H

#include <spot/twaalgos/synthesis.hh>
#include "inp_dep_utils.h"
#include "measure.h"

int naive(InputDependenciesCLIOptions& options, 
          spot::aig_ptr& final_strategy, 
          SynthesisMeasure*& measure);

#endif // NAIVE_ALGORITHM_H