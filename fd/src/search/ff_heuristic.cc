#include "ff_heuristic.h"

#include "globals.h"
#include "operator.h"
#include "option_parser.h"
#include "plugin.h"
#include "state.h"

#include <cassert>
#include <vector>
using namespace std;

// construction and destruction
FFHeuristic::FFHeuristic(const Options &opts)
  : AdditiveHeuristic(opts),
    c_tiebreaking(TieBreaking(opts.get_enum("ties"))),
    m_rng(opts.get<int>("seed"))
{
}

FFHeuristic::~FFHeuristic() {
}

// initialization
void FFHeuristic::initialize() {
    cout << "Initializing FF heuristic..." << endl;
    h_name = "FF_heuristic";
    AdditiveHeuristic::initialize();
    relaxed_plan.resize(g_operators.size(), false);
}

void FFHeuristic::mark_preferred_operators_and_relaxed_plan(
    const State &state, Proposition *goal) {
    if (!goal->marked) { // Only consider each subgoal once.
        goal->marked = true;
        UnaryOperator *unary_op = goal->reached_by;
        if (unary_op) { // We have not yet chained back to a start node.
            for (int i = 0; i < unary_op->precondition.size(); i++)
                mark_preferred_operators_and_relaxed_plan(
                    state, unary_op->precondition[i]);
            int operator_no = unary_op->operator_no;
            if (operator_no != -1) {
                // This is not an axiom.
                relaxed_plan[operator_no] = true;

                if (unary_op->cost == unary_op->base_cost) {
                    // This test is implied by the next but cheaper,
                    // so we perform it to save work.
                    // If we had no 0-cost operators and axioms to worry
                    // about, it would also imply applicability.
                    const Operator *op = &g_operators[operator_no];
                    if (op->is_applicable(state))
                        set_preferred(op);
                }
            }
        }
    }
}

int FFHeuristic::compute_heuristic(const State &state) {
    int h_add = compute_add_and_ff(state);
    if (h_add == DEAD_END)
        return h_add;
    std::vector<std::vector<UnaryOperator *> > m_achievers;
    m_achievers.resize(m_propositions);
    for (unsigned i = 0; i < unary_operators.size(); i++) {
      UnaryOperator &op = unary_operators[i];
      if (op.unsatisfied_preconditions == 0
          && op.effect->cost == op.cost) {
        m_achievers[op.effect->id].push_back(&op);
      }
    }
    unsigned prop_id = 0;
    for (unsigned var = 0; var < g_variable_domain.size(); var++) {
      for (int val = 0; val < g_variable_domain[var]; val++) {
        if (!m_achievers[prop_id].empty()) {
          switch(c_tiebreaking) {
          case(ARBITRARY):
            propositions[var][val].reached_by = m_achievers[prop_id][0];
            break;
          case(RANDOM):
            propositions[var][val].reached_by = m_achievers[prop_id][m_rng.next(m_achievers[prop_id].size())];
            break;
          }
        }
        prop_id++;
      }
    }
    m_achievers.clear();

    // Collecting the relaxed plan also sets the preferred operators.
    for (int i = 0; i < goal_propositions.size(); i++)
        mark_preferred_operators_and_relaxed_plan(state, goal_propositions[i]);

    int h_ff = 0;
    for (int op_no = 0; op_no < relaxed_plan.size(); op_no++) {
        if (relaxed_plan[op_no]) {
            relaxed_plan[op_no] = false; // Clean up for next computation.
            h_ff += get_adjusted_cost(g_operators[op_no]);
        }
    }
    return h_ff;
}

int FFHeuristic::compute_heuristic(const State &state, int g_value) {
    int h_add = compute_add_and_ff(state);
    if (h_add == DEAD_END)
        return h_add;
    std::vector<std::vector<UnaryOperator *> > m_achievers;
    m_achievers.resize(m_propositions);
    for (unsigned i = 0; i < unary_operators.size(); i++) {
      UnaryOperator &op = unary_operators[i];
      if (op.unsatisfied_preconditions == 0
          && op.effect->cost == op.cost) {
        m_achievers[op.effect->id].push_back(&op);
      }
    }
    unsigned prop_id = 0;
    for (unsigned var = 0; var < g_variable_domain.size(); var++) {
      for (int val = 0; val < g_variable_domain[var]; val++) {
        if (!m_achievers[prop_id].empty()) {
          switch(c_tiebreaking) {
          case(ARBITRARY):
            propositions[var][val].reached_by = m_achievers[prop_id][0];
            break;
          case(RANDOM):
            propositions[var][val].reached_by = m_achievers[prop_id][m_rng.next(m_achievers[prop_id].size())];
            break;
          }
        }
        prop_id++;
      }
    }
    m_achievers.clear();

    // Collecting the relaxed plan also sets the preferred operators.
    for (int i = 0; i < goal_propositions.size(); i++)
        mark_preferred_operators_and_relaxed_plan(state, goal_propositions[i]);

    int h_ff = 0;
    for (int op_no = 0; op_no < relaxed_plan.size(); op_no++) {
        if (relaxed_plan[op_no]) {
            relaxed_plan[op_no] = false; // Clean up for next computation.
            h_ff += get_adjusted_cost(g_operators[op_no]);
        }
    }
    if (h_ff+g_value > bound) {
        return DEAD_END;
    }else {
        return h_ff;
    }
}

void FFHeuristic::mark_relaxed_plan(const State &state, Proposition *goal) {
    if (!goal->marked) { // Only consider each subgoal once.
        goal->marked = true;
        UnaryOperator *unary_op = goal->reached_by;
        if (unary_op) { // We have not yet chained back to a start node.
            for (int i = 0; i < unary_op->precondition.size(); i++)
                mark_relaxed_plan(state, unary_op->precondition[i]);
            int operator_no = unary_op->operator_no;
            if (operator_no != -1) {
                // This is not an axiom.
                relaxed_plan[operator_no] = true;
            }
        }
    }
}

void FFHeuristic::compute_relaxed_plan(const State &state, RelaxedPlan &returned_relaxed_plan) {
	returned_relaxed_plan.clear();
	returned_relaxed_plan.resize(g_operators.size(), false);
	int h_add = compute_add_and_ff(state);
	if (h_add == DEAD_END)
		return;

	// mark only relaxed plan, no preferred operators
	for (int i = 0; i < goal_propositions.size(); i++)
		mark_relaxed_plan(state, goal_propositions[i]);

	returned_relaxed_plan = relaxed_plan;
	relaxed_plan.assign(g_operators.size(), false);
}


static Heuristic *_parse(OptionParser &parser) {
    parser.document_synopsis("FF heuristic", "See also Synergy.");
    parser.document_language_support("action costs", "supported");
    parser.document_language_support("conditional effects", "supported");
    parser.document_language_support(
        "axioms",
        "supported (in the sense that the planner won't complain -- "
        "handling of axioms might be very stupid "
        "and even render the heuristic unsafe)");
    parser.document_property("admissible", "no");
    parser.document_property("consistent", "no");
    parser.document_property("safe", "yes for tasks without axioms");
    parser.document_property("preferred operators", "yes");

    Heuristic::add_options_to_parser(parser);
    std::vector<std::string> ties;
    ties.push_back("arbitrary");
    ties.push_back("random");
    parser.add_enum_option("ties", ties, "", "arbitrary");
    parser.add_option<int>("seed", "", "999");
    Options opts = parser.parse();
    if (parser.dry_run())
        return 0;
    else
        return new FFHeuristic(opts);
}

static Plugin<Heuristic> _plugin("ff", _parse);
