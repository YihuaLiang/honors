#ifndef PRUNING_STUBBORN_SETS_SIMPLE_H
#define PRUNING_STUBBORN_SETS_SIMPLE_H

#include "stubborn_sets.h"

namespace stubborn_sets_simple {
class StubbornSetsSimple : public stubborn_sets::StubbornSets {
    /* interference_relation[op1_no] contains all operator indices
       of operators that interfere with op1. */
    std::vector<std::vector<int>> interference_relation;
    std::vector<bool> interference_relation_computed;

    void add_necessary_enabling_set(Fact fact);
    void add_interfering(int op_no);

    inline bool interfere(int op1_no, int op2_no) {
        return can_disable(op1_no, op2_no) ||
               can_conflict(op1_no, op2_no) ||
               can_disable(op2_no, op1_no);
    }
    const std::vector<int> &get_interfering_operators(int op_no);
protected:
    virtual void initialize_stubborn_set(const GlobalState &state) override;
    virtual void handle_stubborn_operator(const GlobalState &state, int op_no) override;
public:
    explicit StubbornSetsSimple(const options::Options &opts);
    virtual ~StubbornSetsSimple() = default;
};
}

#endif
