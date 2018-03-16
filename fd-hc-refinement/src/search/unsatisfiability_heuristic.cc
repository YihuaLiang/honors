
#include "unsatisfiability_heuristic.h"

#include "state.h"

#include "option_parser.h"
#include "plugin.h"

UnsatisfiabilityHeuristic::UnsatisfiabilityHeuristic(const Options &opts)
  : Heuristic(opts), h(opts.get<Heuristic*>("h"))
{
}

int UnsatisfiabilityHeuristic::compute_heuristic(const State &state)
{
  h->evaluate(state);
  if (h->is_dead_end()) {
    return DEAD_END;
  }
  return 0;
}

void UnsatisfiabilityHeuristic::initialize()
{
}

static Heuristic *_parse(OptionParser &parser)
{
  parser.add_option<Heuristic*>("h");
  Heuristic::add_options_to_parser(parser);
  Options opts = parser.parse();
  if (!parser.dry_run()) {
    return new UnsatisfiabilityHeuristic(opts);
  }
  return NULL;
}

static Plugin<Heuristic> _plugin("unsath", _parse);
