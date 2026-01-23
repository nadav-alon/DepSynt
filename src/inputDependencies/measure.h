#ifndef INP_DEP_MEASURE_H
#define INP_DEP_MEASURE_H

#include "synt_measure.h"

class InpDepSyntMeasure: public SynthesisMeasure {
public:
    InpDepSyntMeasure(SyntInstance& synt_instance, bool measure_bdd, bool measure_bdd_without_deps) : SynthesisMeasure(synt_instance, measure_bdd, measure_bdd_without_deps) {}
};

#endif // INP_DEP_MEASURE_H