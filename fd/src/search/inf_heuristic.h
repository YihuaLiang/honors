#ifndef INF_HEURISTIC_H
#define INF_HEURISTIC_H

#include "heuristic.h"

class InfinityHeuristic : public Heuristic {
protected:
    virtual void initialize();
    virtual int compute_heuristic(const State &state);
public:
    InfinityHeuristic(const Options &options);
    ~InfinityHeuristic();
};

#endif
