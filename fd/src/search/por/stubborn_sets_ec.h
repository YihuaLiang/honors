#ifndef PRUNING_STUBBORN_SETS_EC_H
#define PRUNING_STUBBORN_SETS_EC_H

#include "stubborn_sets.h"

namespace stubborn_sets {
class StubbornSetsEC : public stubborn_sets::StubbornSets {
private:
    std::vector<std::vector<std::vector<bool>>> reachability_map;
    std::vector<std::vector<int>> op_preconditions_on_var;
    std::vector<bool> active_ops;
    std::vector<std::vector<int>> conflicting_and_disabling;
    std::vector<bool> conflicting_and_disabling_computed;
    std::vector<std::vector<int>> disabled;
    std::vector<bool> disabled_computed;
    std::vector<bool> written_vars;
    std::vector<std::vector<bool>> nes_computed;

    void get_disabled_vars(int op1_no, int op2_no, std::vector<int> &disabled_vars);
    void build_reachability_map();
    void compute_operator_preconditions();
    const std::vector<int> &get_conflicting_and_disabling(int op_no);
    const std::vector<int> &get_disabled(int op_no);
    void add_conflicting_and_disabling(int op_no, const State &state);
    void compute_active_operators(const State &state);
    void mark_as_stubborn_and_remember_written_vars(int op_no, const State &state);
    void add_nes_for_fact(Fact fact, const State &state);
    void apply_s5(const Operator &op, const State &state);
protected:
    virtual void initialize_stubborn_set(const State &state) override;
    virtual void handle_stubborn_operator(const State &state, int op_no) override;
public:
    explicit StubbornSetsEC(const Options &opts);
    virtual ~StubbornSetsEC() = default;
};
}
#endif
