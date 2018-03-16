#include "stubborn_sets.h"

#include "../globals.h"
#include "../operator.h"
#include "../option_parser.h"

#include <algorithm>
#include <cassert>

using namespace std;

namespace stubborn_sets {
  struct SortFactsByVariable {
    bool operator()(const Fact &lhs, const Fact &rhs) {
      return lhs.first < rhs.first;
    }
  };

  /* TODO: get_op_index belongs to a central place.
     We currently have copies of it in different parts of the code. */
  static inline int get_op_index(const Operator *op) {
    return op->get_id();
  }

  // Relies on both fact sets being sorted by variable.
  bool contain_conflicting_fact(const vector<Fact> &facts1,
                                const vector<Fact> &facts2) {
    auto facts1_it = facts1.begin();
    auto facts2_it = facts2.begin();
    while (facts1_it != facts1.end() && facts2_it != facts2.end()) {
      if (facts1_it->first < facts2_it->first) {
        ++facts1_it;
      } else if (facts1_it->first > facts2_it->first) {
        ++facts2_it;
      } else {
        if (facts1_it->second != facts2_it->second)
          return true;
        ++facts1_it;
        ++facts2_it;
      }
    }
    return false;
  }

  StubbornSets::StubbornSets(const Options &opts)
    : num_successors_before_pruning(0),
      num_successors_after_pruning(0),
      min_pruning_ratio(opts.get<double>("min_pruning_ratio")),
      stubborn_calls(0),
      do_pruning(true) {
    cout << "minimal pruning ratio to keep pruning: "
         << min_pruning_ratio << endl;

    ::verify_no_axioms_no_conditional_effects();
    compute_sorted_operators();
    compute_achievers();
  }

  // Relies on op_preconds and op_effects being sorted by variable.
  bool StubbornSets::can_disable(int op1_no, int op2_no) {
    return contain_conflicting_fact(sorted_op_effects[op1_no],
                                    sorted_op_preconditions[op2_no]);
  }

  // Relies on op_effect being sorted by variable.
  bool StubbornSets::can_conflict(int op1_no, int op2_no) {
    return contain_conflicting_fact(sorted_op_effects[op1_no],
                                    sorted_op_effects[op2_no]);
  }

  void StubbornSets::compute_sorted_operators() {
    assert(sorted_op_preconditions.empty());
    assert(sorted_op_effects.empty());

    for (unsigned i = 0; i < g_operators.size(); i++) {
      sorted_op_preconditions.push_back(std::vector<Fact>());
      sorted_op_effects.push_back(std::vector<Fact>());
      std::vector<Fact> &precondition = sorted_op_preconditions.back();
      std::vector<Fact> &effect = sorted_op_effects.back();
      for (const auto & prev : g_operators[i].get_prevail()) {
        precondition.emplace_back(prev.var, prev.prev);
      }
      for (const auto & eff : g_operators[i].get_pre_post()) {
        if (eff.pre != -1) {
          precondition.emplace_back(eff.var, eff.pre);
        }
        effect.emplace_back(eff.var, eff.post);
      }
      std::sort(precondition.begin(), precondition.end(), SortFactsByVariable());
      std::sort(effect.begin(), effect.end(), SortFactsByVariable());
    }
  }

  void StubbornSets::compute_achievers() {
    achievers.resize(g_variable_domain.size());
    for (unsigned var = 0; var < g_variable_domain.size(); var++) {
      achievers[var].resize(g_variable_domain[var]);
    }

    for (size_t op_no = 0; op_no < g_operators.size(); ++op_no) {
      const Operator &op = g_operators[op_no];
      for (const auto & eff : op.get_pre_post()) {
        achievers[eff.var][eff.post].push_back(op_no);
      }
    }
  }

  bool StubbornSets::mark_as_stubborn(int op_no) {
    if (!stubborn[op_no]) {
      stubborn[op_no] = true;
      stubborn_queue.push_back(op_no);
      return true;
    }
    return false;
  }

  void StubbornSets::prune_operators(
                                     const State &state, vector<const Operator *> &ops) {
    if (!do_pruning) {
      return;
    }
    if (stubborn_calls == SAFETY_BELT_SIZE) {
      const double pruning_ratio = 1 - (
                                        static_cast<double>(num_successors_after_pruning) /
                                        static_cast<double>(num_successors_before_pruning));
      cout << "pruning ratio after " << SAFETY_BELT_SIZE
           << " calls: " << pruning_ratio << endl;
      if (pruning_ratio < min_pruning_ratio) {
        cout << "-- pruning ratio is lower than min pruning ratio ("
             << min_pruning_ratio << "); switching off pruning" << endl;
        do_pruning = false;
      }
    }

    num_successors_before_pruning += ops.size();
    stubborn_calls++;

    // Clear stubborn set from previous call.
    stubborn.clear();
    stubborn.assign(g_operators.size(), false);
    assert(stubborn_queue.empty());

    initialize_stubborn_set(state);
    /* Iteratively insert operators to stubborn according to the
       definition of strong stubborn sets until a fixpoint is reached. */
    while (!stubborn_queue.empty()) {
      int op_no = stubborn_queue.back();
      stubborn_queue.pop_back();
      handle_stubborn_operator(state, op_no);
    }

    // Now check which applicable operators are in the stubborn set.
    vector<const Operator *> remaining_ops;
    remaining_ops.reserve(ops.size());
    for (const Operator *op : ops) {
      int op_no = get_op_index(op);
      if (stubborn[op_no])
        remaining_ops.push_back(op);
    }
    if (remaining_ops.size() != ops.size()) {
      ops.swap(remaining_ops);
      sort(ops.begin(), ops.end());
    }

    num_successors_after_pruning += ops.size();
  }

  void StubbornSets::print_statistics() const {
    cout << "total successors before partial-order reduction: "
         << num_successors_before_pruning << endl
         << "total successors after partial-order reduction: "
         << num_successors_after_pruning << endl;
  }
}
