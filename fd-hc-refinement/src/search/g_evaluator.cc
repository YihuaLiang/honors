#include "g_evaluator.h"
#include "option_parser.h"
#include "plugin.h"

GEvaluator::GEvaluator() {
}

GEvaluator::~GEvaluator() {
}

void GEvaluator::evaluate(int g, bool) {
    value = g;
}

bool GEvaluator::is_dead_end() const {
    return false;
}

bool GEvaluator::dead_end_is_reliable() const {
    return true;
}

int GEvaluator::get_value() const {
    return value;
}


NEGGEvaluator::NEGGEvaluator() {
}

NEGGEvaluator::~NEGGEvaluator() {
}

void NEGGEvaluator::evaluate(int g, bool) {
    value = -g;
}



static ScalarEvaluator *_parse(OptionParser &parser) {
    parser.document_synopsis("g-value evaluator",
                             "Returns the current g-value of the search.");
    parser.parse();
    if (parser.dry_run())
        return 0;
    else
        return new GEvaluator;
}

static ScalarEvaluator *_parse_neg(OptionParser &parser) {
    parser.document_synopsis("g-value evaluator",
                             "Returns the current g-value of the search.");
    parser.parse();
    if (parser.dry_run())
        return 0;
    else
        return new NEGGEvaluator;
}



static Plugin<ScalarEvaluator> _plugin("g", _parse);
static Plugin<ScalarEvaluator> _plugin_negg("negg", _parse_neg);
