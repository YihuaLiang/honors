#include "inf_heuristic.h"

#include "globals.h"
#include "operator.h"
#include "option_parser.h"
#include "plugin.h"
#include "state.h"

#include <limits>
#include <utility>
using namespace std;

InfinityHeuristic::InfinityHeuristic(const Options &opts)
    : Heuristic(opts) {
}

InfinityHeuristic::~InfinityHeuristic() {
}

void InfinityHeuristic::initialize() {
    cout << "Initializing infinity heuristic..." << endl;
}

int InfinityHeuristic::compute_heuristic(const State &state) {
    if (test_goal(state))
        return 0;
    else
        return DEAD_END;
}

static Heuristic *_parse(OptionParser &parser) {
    Heuristic::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return 0;
    else
        return new InfinityHeuristic(opts);
}

static Plugin<Heuristic> _plugin("inf", _parse);
