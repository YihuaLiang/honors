#include "relaxed_plan_operators.h"

#include "../additive_heuristic.h"

#include "../operator.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../state.h"

#include <cassert>
#include <vector>
using namespace std;


// construction and destruction
RelaxedPlanOperators::RelaxedPlanOperators(const Options &opts)
    : FFHeuristic(opts) {
}

RelaxedPlanOperators::~RelaxedPlanOperators() {
}


void RelaxedPlanOperators::get_relaxed_plan_operators(vector<bool>& labels) {
	initialize();

	compute_relaxed_plan(g_initial_state(), labels);
}
