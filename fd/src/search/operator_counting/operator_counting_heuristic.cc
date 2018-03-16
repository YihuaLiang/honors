#include "operator_counting_heuristic.h"

#include "constraint_generator.h"

#include "../globals.h"
#include "../operator.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../utils/markup.h"
#include "../utils/timer.h"

using namespace std;

namespace operator_counting {
OperatorCountingHeuristic::OperatorCountingHeuristic(const Options &opts)
    : Heuristic(opts),
      constraint_generators(
          opts.get_list<ConstraintGenerator*>("constraint_generators")),
      lp_solver(lp::LPSolverType(opts.get_enum("lpsolver"))) {
  std::cout << "a"  << std::endl;
}

OperatorCountingHeuristic::~OperatorCountingHeuristic() {
}

void OperatorCountingHeuristic::initialize() {
    utils::Timer timer;
    vector<lp::LPVariable> variables;
    double infinity = lp_solver.get_infinity();
    for (unsigned op = 0; op < g_operators.size(); op++) {
      variables.push_back(lp::LPVariable(0, infinity, get_adjusted_cost(g_operators[op])));
    }
    vector<lp::LPConstraint> constraints;
    for (auto generator : constraint_generators) {
        generator->initialize_constraints(constraints, infinity);
    }
    lp_solver.load_problem(lp::LPObjectiveSense::MINIMIZE, variables, constraints);
    std::cout << "Time for constructing and loading LP: " << timer << endl;
    std::cout << "LP variables: " << variables.size() << endl;
    std::cout << "LP constraints: " << constraints.size() << endl;
}

int OperatorCountingHeuristic::compute_heuristic(const State &state) {
    assert(!lp_solver.has_temporary_constraints());
    for (auto generator : constraint_generators) {
        bool dead_end = generator->update_constraints(state, lp_solver);
        if (dead_end) {
            lp_solver.clear_temporary_constraints();
            return DEAD_END;
        }
    }
    int result;
    lp_solver.solve();
    if (lp_solver.has_optimal_solution()) {
        double epsilon = 0.01;
        double objective_value = lp_solver.get_objective_value();
        result = ceil(objective_value - epsilon);
    } else {
        result = DEAD_END;
    }
    lp_solver.clear_temporary_constraints();
    return result;
}

}

static Heuristic *_parse(OptionParser &parser) {
  parser.add_list_option<operator_counting::ConstraintGenerator *>(
        "constraint_generators",
        "methods that generate constraints over operator counting variables");
    lp::add_lp_solver_option_to_parser(parser);
    Heuristic::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (parser.dry_run())
        return nullptr;
    return new operator_counting::OperatorCountingHeuristic(opts);
}

static Plugin<Heuristic> _plugin("operatorcounting", _parse);
