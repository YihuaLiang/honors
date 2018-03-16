
#ifndef RN_MAP_H
#define RN_MAP_H

#include "uc_heuristic.h"
#include "../state.h"
#include "../state_id.h"

#include <unordered_set>

struct RNMap {
    virtual void operator()(UCHeuristic *h, StateID state) = 0;
    virtual void operator()(UCHeuristic *h, const std::unordered_set<StateID> &in, std::unordered_set<StateID> &out) = 0;
};

struct StateRNMap : public RNMap {
    virtual void operator()(UCHeuristic *h, StateID state_id);
    virtual void operator()(UCHeuristic *h, const std::unordered_set<StateID> &in, std::unordered_set<StateID> &out);
};

struct ClauseRNMap : public RNMap {
    virtual void operator()(UCHeuristic *h, StateID clause_id);
    virtual void operator()(UCHeuristic *h, const std::unordered_set<StateID> &in, std::unordered_set<StateID> &out);
};


#endif
