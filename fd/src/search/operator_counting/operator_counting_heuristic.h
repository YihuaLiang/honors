#ifndef OPERATOR_COUNTING_OPERATOR_COUNTING_HEURISTIC_H
#define OPERATOR_COUNTING_OPERATOR_COUNTING_HEURISTIC_H

#include "../heuristic.h"

#include "../lp/lp_solver.h"

#include <vector>

namespace operator_counting {
class ConstraintGenerator;

class OperatorCountingHeuristic : public Heuristic {
    std::vector<ConstraintGenerator *> constraint_generators;
    lp::LPSolver lp_solver;
protected:
    virtual void initialize() override;
    virtual int compute_heuristic(const State &state);
public:
    explicit OperatorCountingHeuristic(const Options &opts);
    ~OperatorCountingHeuristic();
};
}

#endif
