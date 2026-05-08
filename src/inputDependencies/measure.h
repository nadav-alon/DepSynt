#ifndef INP_DEP_MEASURE_H
#define INP_DEP_MEASURE_H

#include "synt_measure.h"

class InpDepSyntMeasure: public SynthesisMeasure {
private:
    double m_input_dep_duration = 0;
    int m_input_dep_count = 0;
    double m_input_dep_synthesis_duration = 0;
    bool m_input_dep_measured = false;

public:
    InpDepSyntMeasure(SyntInstance& synt_instance, bool measure_bdd, bool measure_bdd_without_deps) : SynthesisMeasure(synt_instance, measure_bdd, measure_bdd_without_deps) {}

    void set_input_dep_measures(double duration, int count, double synthesis_duration) {
        m_input_dep_duration = duration;
        m_input_dep_count = count;
        m_input_dep_synthesis_duration = synthesis_duration;
        m_input_dep_measured = true;
    }

    void get_json_object(json& obj) const override {
        SynthesisMeasure::get_json_object(obj);
        if (m_input_dep_measured) {
            json input_dep_obj;
            input_dep_obj["duration"] = m_input_dep_duration;
            input_dep_obj["count"] = m_input_dep_count;
            input_dep_obj["synthesis_duration"] = m_input_dep_synthesis_duration;
            obj["input_dependencies"] = input_dep_obj;
        }
    }
};

#endif // INP_DEP_MEASURE_H