#ifndef PRUNING_NULL_PRUNING_METHOD_H
#define PRUNING_NULL_PRUNING_METHOD_H

#include "pruning_method.h"

class Operator;
class State;

namespace null_pruning_method {
class NullPruningMethod : public PruningMethod {
public:
    NullPruningMethod();
    virtual ~NullPruningMethod() = default;
    virtual void prune_operators(const State & /*state*/,
                                 std::vector<const Operator *> & /*ops*/) override {}
    virtual void print_statistics() const override {}
};
}

#endif
