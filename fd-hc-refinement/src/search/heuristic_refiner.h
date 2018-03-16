
#ifndef HEURISTIC_REFINER_H
#define HEURISTIC_REFINER_H

#include "state_id.h"
#include "state.h"

#include <vector>
#include <unordered_set>

class Operator;
class Options;
class Heuristic;

class HeuristicRefiner {
public:
    enum RefinementResult {
        FAILED = 0,
        UNCHANGED = 1,
        SUCCESSFUL = 2,
        SOLVED = 3,
    };
    HeuristicRefiner(const Options &/*opts*/) {}
    virtual RefinementResult learn_unrecognized_dead_end(const State &/*state*/, int cost_bound) { return FAILED; }
    virtual RefinementResult learn_unrecognized_dead_ends(
            const std::vector<State> &/*root_component*/,
            const std::unordered_set<StateID> &/*recognized_neighbors*/, 
            int cost_bound) { return FAILED; }
    virtual void learn_recognized_dead_ends(const std::vector<State> &/*dead_ends*/) {};
    virtual void learn_recognized_dead_end(const State &/*dead_end*/) {};
    //just used for check in refiner
    virtual bool dead_end_learning_requires_full_component() { return false; }
    virtual bool dead_end_learning_requires_recognized_neighbors() { return false; }

    virtual void get_partial_plan(std::vector<const Operator *> & ) const {}

    virtual Heuristic *get_heuristic() { return NULL; }

    virtual void refine_offline() {}
    
    virtual void statistics() {}
};

#endif
