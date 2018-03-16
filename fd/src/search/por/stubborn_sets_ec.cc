#include "stubborn_sets_ec.h"

#include "../globals.h"
#include "../operator.h"
#include "../option_parser.h"
#include "../plugin.h"

#include "../utils/markup.h"

#include <cassert>

using namespace std;

namespace stubborn_sets {
  // DTGs are stored as one adjacency list per value.
  using StubbornDTG = vector<vector<int>>;

  static inline bool is_v_applicable(int var,
                                     int op_no,
                                     const State &state,
                                     vector<vector<int>> &preconditions) {
    int precondition_on_var = preconditions[op_no][var];
    return precondition_on_var == -1 || precondition_on_var == state[var];
  }

  // Copied from SimpleStubbornSets
  static inline Fact find_unsatisfied_goal(const State &state) {
    for (size_t i = 0; i < g_goal.size(); ++i) {
      int goal_var = g_goal[i].first;
      int goal_value = g_goal[i].second;
      if (state[goal_var] != goal_value)
        return Fact(goal_var, goal_value);
    }
    return Fact(-1, -1);
  }

  vector<StubbornDTG> build_dtgs() {
    /*
      NOTE: Code lifted and adapted from M&S atomic abstraction code.
      We need a more general mechanism for creating data structures of
      this kind.
    */

    /*
      NOTE: for stubborn sets ec, the DTG for v *does* include
      self-loops from d to d if there is an operator that sets the
      value of v to d and has no precondition on v. This is different
      from the usual DTG definition.
    */

    // Create the empty DTG nodes.
    vector<StubbornDTG> dtgs;
    int num_variables = g_variable_domain.size();
    for (int var_no = 0; var_no < num_variables; ++var_no) {
      dtgs.emplace_back(g_variable_domain[var_no]);
    }

    for (unsigned opn = 0; opn < g_operators.size(); opn++) {
      for (const auto & eff : g_operators[opn].get_pre_post()) {
        int eff_var = eff.var;
        int eff_val = eff.post;
        int pre_val = eff.pre;

        StubbornDTG &dtg = dtgs[eff_var];
        if (pre_val == -1) {
          for (int value = 0; value < g_variable_domain[eff_var]; ++value) {
            dtg[value].push_back(eff_val);
          }
        } else {
          dtg[pre_val].push_back(eff_val);
        }
      }
    }

    return dtgs;
  }

  void recurse_forwards(const StubbornDTG &dtg,
                        int start_value,
                        int current_value,
                        vector<bool> &reachable) {
    if (!reachable[current_value]) {
      reachable[current_value] = true;
      for (int successor_value : dtg[current_value])
        recurse_forwards(dtg, start_value, successor_value, reachable);
    }
  }

  // Relies on both fact sets being sorted by variable.
  void get_conflicting_vars(const vector<Fact> &facts1,
                            const vector<Fact> &facts2,
                            vector<int> &conflicting_vars) {
    conflicting_vars.clear();
    auto facts1_it = facts1.begin();
    auto facts2_it = facts2.begin();
    while (facts1_it != facts1.end() &&
           facts2_it != facts2.end()) {
      if (facts1_it->first < facts2_it->first) {
        ++facts1_it;
      } else if (facts1_it->first > facts2_it->first) {
        ++facts2_it;
      } else {
        if (facts2_it->second != facts1_it->second) {
          conflicting_vars.push_back(facts2_it->first);
        }
        ++facts1_it;
        ++facts2_it;
      }
    }
  }

  StubbornSetsEC::StubbornSetsEC(const Options &opts)
    : StubbornSets(opts),
      conflicting_and_disabling(g_operators.size()),
      conflicting_and_disabling_computed(g_operators.size(), false),
      disabled(g_operators.size()),
      disabled_computed(g_operators.size(), false) {
    cout << "pruning method: stubborn sets ec" << endl;

    compute_operator_preconditions();
    build_reachability_map();

    int num_variables = g_variable_domain.size();
    for (int var = 0; var < num_variables; var++) {
      nes_computed.push_back(vector<bool>(g_variable_domain[var], false));
    }
  }

  void StubbornSetsEC::compute_operator_preconditions() {
    int num_operators = g_operators.size();
    int num_variables = g_variable_domain.size();
    op_preconditions_on_var.resize(num_operators);
    for (int op_no = 0; op_no < num_operators; op_no++) {
      op_preconditions_on_var[op_no].resize(num_variables, -1);
      const Operator &op = g_operators[op_no];
      for (const auto &pre : op.get_prevail()) {
        op_preconditions_on_var[op_no][pre.var] = pre.prev;
      }
      for (const auto & eff : op.get_pre_post()) {
        if (eff.pre != -1) {
          op_preconditions_on_var[op_no][eff.var] = eff.pre;
        }
      }
    }
  }

  void StubbornSetsEC::build_reachability_map() {
    vector<StubbornDTG> dtgs = build_dtgs();
    int num_variables = g_variable_domain.size();
    reachability_map.resize(num_variables);
    for (int var = 0; var < num_variables; ++var) {
      StubbornDTG &dtg = dtgs[var];
      int num_values = dtg.size();
      reachability_map[var].resize(num_values);
      for (int val = 0; val < num_values; ++val) {
        reachability_map[var][val].assign(num_values, false);
      }
      for (int start_value = 0; start_value < g_variable_domain[var]; start_value++) {
        vector<bool> &reachable = reachability_map[var][start_value];
        recurse_forwards(dtg, start_value, start_value, reachable);
      }
    }
  }

  void StubbornSetsEC::compute_active_operators(const State &state) {
    int num_operators = g_operators.size();
    for (int op_no = 0; op_no < num_operators; ++op_no) {
      const Operator &op = g_operators[op_no];
      bool all_preconditions_are_active = true;

      for (const auto & pre : op.get_prevail()) {
        const vector<bool> &reachable_values = reachability_map[pre.var][state[pre.var]];
        if (!reachable_values[pre.prev]) {
          all_preconditions_are_active = false;
          break;
        }
      }

      if (all_preconditions_are_active) {
        for (const auto & eff : op.get_pre_post()) {
          if (eff.pre != -1) {
            const vector<bool> &reachable_values = reachability_map[eff.var][state[eff.var]];
            if (!reachable_values[eff.pre]) {
              all_preconditions_are_active = false;
              break;
            }
          }
        }
      }

      if (all_preconditions_are_active) {
        active_ops[op_no] = true;
      }
    }
  }

  const vector<int> &StubbornSetsEC::get_conflicting_and_disabling(int op_no) {
    vector<int> &result = conflicting_and_disabling[op_no];
    if (!conflicting_and_disabling_computed[op_no]) {
      int num_operators = g_operators.size();
      for (int op2_no = 0; op2_no < num_operators; ++op2_no) {
        if (op_no != op2_no) {
          bool conflict = can_conflict(op_no, op2_no);
          bool disable = can_disable(op2_no, op_no);
          if (conflict || disable) {
            result.push_back(op2_no);
          }
        }
      }
      conflicting_and_disabling_computed[op_no] = true;
    }
    return result;
  }

  const std::vector<int> &StubbornSetsEC::get_disabled(int op_no) {
    int num_operators = g_operators.size();
    vector<int> &result = disabled[op_no];
    if (!disabled_computed[op_no]) {
      for (int op2_no = 0; op2_no < num_operators; ++op2_no) {
        if (op2_no != op_no) {
          if (can_disable(op_no, op2_no)) {
            result.push_back(op2_no);
          }
        }
      }
      disabled_computed[op_no] = true;
    }
    return result;
  }

  // TODO: find a better name.
  void StubbornSetsEC::mark_as_stubborn_and_remember_written_vars(
                                                                  int op_no, const State &state) {
    if (mark_as_stubborn(op_no)) {
      const Operator &op = g_operators[op_no];
      if (op.is_applicable(state)) {
        for (const auto & eff : op.get_pre_post()) {
          written_vars[eff.var] = true;
        }
      }
    }
  }

  /* TODO: think about a better name, which distinguishes this method
     better from the corresponding method for simple stubborn sets */
  void StubbornSetsEC::add_nes_for_fact(Fact fact, const State &state) {
    for (int achiever : achievers[fact.first][fact.second]) {
      if (active_ops[achiever]) {
        mark_as_stubborn_and_remember_written_vars(achiever, state);
      }
    }

    nes_computed[fact.first][fact.second] = true;
  }

  void StubbornSetsEC::add_conflicting_and_disabling(int op_no,
                                                     const State &state) {
    for (int conflict : get_conflicting_and_disabling(op_no)) {
      if (active_ops[conflict]) {
        mark_as_stubborn_and_remember_written_vars(conflict, state);
      }
    }
  }

  // Relies on op_effects and op_preconditions being sorted by variable.
  void StubbornSetsEC::get_disabled_vars(
                                         int op1_no, int op2_no, vector<int> &disabled_vars) {
    get_conflicting_vars(sorted_op_effects[op1_no],
                         sorted_op_preconditions[op2_no],
                         disabled_vars);
  }

  void StubbornSetsEC::apply_s5(const Operator &op, const State &state) {
    // Find a violated state variable and check if stubborn contains a writer for this variable.
    Fact violated_precondition(-1, -1);
    for (const auto & pre : op.get_prevail()) {
      if (state[pre.var] != pre.prev) {
        if (written_vars[pre.var]) {
          if (!nes_computed[pre.var][pre.prev]) {
            add_nes_for_fact(Fact(pre.var, pre.prev), state);
          }
          return;
        }
        if (violated_precondition.first == -1) {
          violated_precondition = Fact(pre.var, pre.prev);
        }
      }
    }
    for (const auto & eff : op.get_pre_post()) {
      if (eff.pre == -1) {
        continue;
      }
      int var = eff.var;
      int value = eff.pre;
      if (state[var] != value) {
        if (written_vars[var]) {
          if (!nes_computed[var][value]) {
            add_nes_for_fact(Fact(var, value), state);
          }
          return;
        }
        if (violated_precondition.first == -1) {
          violated_precondition = Fact(var, value);
        }
      }
    }

    assert(violated_precondition.first != -1);
    if (!nes_computed[violated_precondition.first][violated_precondition.second]) {
      add_nes_for_fact(violated_precondition, state);
    }
  }

  void StubbornSetsEC::initialize_stubborn_set(const State &state) {
    active_ops.clear();
    active_ops.assign(g_operators.size(), false);
    for (size_t i = 0; i < nes_computed.size(); i++) {
      nes_computed[i].clear();
      nes_computed[i].assign(g_variable_domain[i], false);
    }
    written_vars.assign(g_variable_domain.size(), false);

    compute_active_operators(state);

    //rule S1
    Fact unsatisfied_goal = find_unsatisfied_goal(state);
    assert(unsatisfied_goal.first != -1);
    add_nes_for_fact(unsatisfied_goal, state);     // active operators used
  }

  void StubbornSetsEC::handle_stubborn_operator(const State &state, int op_no) {
    const Operator &op = g_operators[op_no];
    if (op.is_applicable(state)) {
      //Rule S2 & S3
      add_conflicting_and_disabling(op_no, state);     // active operators used
      //Rule S4'
      vector<int> disabled_vars;
      for (int disabled_op_no : get_disabled(op_no)) {
        if (active_ops[disabled_op_no]) {
          get_disabled_vars(op_no, disabled_op_no, disabled_vars);
          if (!disabled_vars.empty()) {     // == can_disable(op1_no, op2_no)
            bool v_applicable_op_found = false;
            for (int disabled_var : disabled_vars) {
              //First case: add o'
              if (is_v_applicable(disabled_var,
                                  disabled_op_no,
                                  state,
                                  op_preconditions_on_var)) {
                mark_as_stubborn_and_remember_written_vars(disabled_op_no, state);
                v_applicable_op_found = true;
                break;
              }
            }

            //Second case: add a necessary enabling set for o' following S5
            if (!v_applicable_op_found) {
              apply_s5(g_operators[disabled_op_no], state);
            }
          }
        }
      }
    } else {     // op is inapplicable
      //S5
      apply_s5(op, state);
    }
  }
}


static PruningMethod* _parse(OptionParser &parser) {
  parser.document_synopsis(
                           "StubbornSetsEC",
                           "Stubborn sets represent a state pruning method which computes a subset "
                           "of applicable operators in each state such that completeness and "
                           "optimality of the overall search is preserved. As stubborn sets rely "
                           "on several design choices, there are different variants thereof. "
                           "The variant 'StubbornSetsEC' resolves the design choices such that "
                           "the resulting pruning method is guaranteed to strictly dominate the "
                           "Expansion Core pruning method. For details, see" + utils::format_paper_reference(
                                                                                                             {"Martin Wehrle", "Malte Helmert", "Yusra Alkhazraji", "Robert Mattmüller"},
                                                                                                             "The Relative Pruning Power of Strong Stubborn Sets and Expansion Core",
                                                                                                             "http://www.aaai.org/ocs/index.php/ICAPS/ICAPS13/paper/view/6053/6185",
                                                                                                             "Proceedings of the 23rd International Conference on Automated Planning "
                                                                                                             "and Scheduling (ICAPS 2013)",
                                                                                                             "251-259",
                                                                                                             "AAAI Press 2013"));

  parser.add_option<double>(
                            "min_pruning_ratio",
                            "minimal pruning ratio such that pruning is not switched off",
                            "0.0");

  Options opts = parser.parse();

  if (parser.dry_run()) {
    return nullptr;
  }
  return new stubborn_sets::StubbornSetsEC(opts);
}

static Plugin<PruningMethod> _plugin("stubborn_sets_ec", _parse);
