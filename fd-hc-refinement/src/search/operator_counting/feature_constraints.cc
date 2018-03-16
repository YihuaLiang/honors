#include "feature_constraints.h"

#include "../globals.h"
#include "../operator.h"
#include "../option_parser.h"
#include "../plugin.h"

#include "../lp/lp_solver.h"
#include "../utils/markup.h"

#include "../timer.h"

using namespace std;

namespace std {
  ostream &operator<<(ostream &o, const Fact &f)
  {
    return o << "(" << f.first << ", " << f.second << ")";
  }
}

namespace operator_counting {
  static const int NONE = -1;

  struct ExplicitOperator {
    vector<int> preconditions;
    vector<int> postconditions;
    vector<Fact> real_effects;
    vector<Fact> prevail_conditions;
    ExplicitOperator(const Operator &op, int num_vars)
      : preconditions(num_vars, NONE),
        postconditions(num_vars, NONE) {
      for (const auto &pre : op.get_prevail()) {
        preconditions[pre.var] = pre.prev;
        postconditions[pre.var] = pre.prev;
      }
      for (const auto &post : op.get_pre_post()) {
        if (post.pre != -1) {
          preconditions[post.var] = post.pre;
        }
        postconditions[post.var] = post.post;
      }
      for (int var = 0; var < num_vars; ++var) {
        int pre = preconditions[var];
        int post = postconditions[var];
        if (post != NONE) {
          if (pre != NONE && pre != post) {
            // Real effect.
            real_effects.emplace_back(var, post);
          } else {
            // Prevail condition.
            prevail_conditions.emplace_back(var, post);
          }
        }
      }
    }

    void dump() const {
      cout << "Pre:  " << preconditions << endl;
      cout << "Post: " << postconditions << endl;
      cout << "Effects: " << real_effects << endl;
      cout << "Prevails: " << prevail_conditions << endl;
    }
  };

  Feature::Feature(const std::vector<Fact> &&values)
    : values(move(values)) {
    assert(is_sorted(values.begin(), values.end()));
  }

  bool Feature::can_be_consumed_by(const ExplicitOperator &op) const {
    /* o can consume f iff f could be true before (pre(o) does not contradict f)
       and o changes at least one value of f to something else */
    return is_consistent_with(op.preconditions) && !is_consistent_with(op.postconditions);
  }

  bool Feature::always_produced_by(const ExplicitOperator &op) const {
    /* o always produces f iff f could not be true before (pre(o) contradicts f)
       and prevail(o) and eff(o) together imply f */
    if (is_consistent_with(op.preconditions))
      return false;
    for (const Fact &fact : values) {
      if (op.postconditions[fact.first] == NONE ||
          op.postconditions[fact.first] != fact.second)
        return false;
    }
    return true;
  }

  bool Feature::is_consistent_with(const vector<int> &partial_state) const {
    for (const Fact &fact : values) {
      if (partial_state[fact.first] != fact.second && partial_state[fact.first] != NONE)
        return false;
    }
    return true;
  }

  bool Feature::is_consistent_with(const State &state) const {
    for (const Fact &fact : values) {
      if (state[fact.first] != fact.second)
        return false;
    }
    return true;
  }

  ostream &operator<<(ostream &os, const Feature &feature) {
    os << "{";
    string sep = "";
    for (const Fact &fact : feature.values) {
      os << sep << fact.first << "=" << fact.second;
      sep = ", ";
    }
    os << "}";
    return os;
  }


  FeatureConstraints::FeatureConstraints(const Options &opts)
    : max_size(opts.get<int>("max_size")),
      num_ignored_features(0) {
  }

  void FeatureConstraints::add_feature_if_inconsistent_with_goal(
                                                                 unordered_set<Feature> &features, Feature &&feature) const {
    if (feature.is_consistent_with(explicit_goals)) {
      ++num_ignored_features;
    } else {
      features.insert(move(feature));
    }
  }

  unordered_set<Feature> FeatureConstraints::construct_features(int size) {
    unordered_set<Feature> features;
    for (const ExplicitOperator &explicit_op : explicit_ops) {
      const vector<Fact> &real_effects = explicit_op.real_effects;
      const vector<Fact> &prevail_conditions = explicit_op.prevail_conditions;

      int num_effects = real_effects.size();
      int num_prevails = prevail_conditions.size();

      const int min_effects = max(1, size - num_prevails);
      const int max_effects = min(size, num_effects);

      /* Create the features that this operator always produces:
         Select one real effect, then add combinations of the
         remaining post conditions. */
      Combinations<Fact> combos;
      for (int num_chosen_effects = min_effects;
           num_chosen_effects <= max_effects;
           ++num_chosen_effects) {
        const vector<vector<Fact>> effect_combos =
          combos.get_combinations(real_effects, num_chosen_effects);
        for (const vector<Fact> &effect_combo : effect_combos) {
          // Note that we could directly use effect_combo if num_prevails = 0.
          const int num_prevails = size - num_chosen_effects;
          for (vector<Fact> &prevail_combo : combos.get_combinations(
                                                                     prevail_conditions, num_prevails)) {
            vector<Fact> combo = effect_combo;
            combo.reserve(size);
            move(prevail_combo.begin(), prevail_combo.end(), back_inserter(combo));
            sort(combo.begin(), combo.end());
            Feature feature(move(combo));
            add_feature_if_inconsistent_with_goal(features, move(feature));
          }
        }
      }
    }
    return features;
  }

  lp::LPConstraint FeatureConstraints::get_constraint(const Feature &feature, double infinity) const {
    assert(!feature.is_consistent_with(explicit_goals));
    // sum_o Y_o * ([o can consume f] - [o always produces f]) >= [f \in s]
    // Adapt lower bound to each evaluated state later.
    lp::LPConstraint constraint(0, infinity);
    for (size_t op_id = 0; op_id < explicit_ops.size(); ++op_id) {
      const ExplicitOperator &explicit_op = explicit_ops[op_id];
      if (feature.can_be_consumed_by(explicit_op)) {
        constraint.insert(op_id, 1);
      } else if (feature.always_produced_by(explicit_op)) {
        constraint.insert(op_id, -1);
      }
    }
    return constraint;
  }

  void FeatureConstraints::find_features() {
    for (int size = 2; size <= max_size; ++size) {
      Timer construct_features_timer;
      unordered_set<Feature> new_features = construct_features(size);
      std::cout << "Found " << new_features.size() << " features of size "
                << size << " in " << construct_features_timer << "." << endl;
      std::cout << "Total number of ignored features: " << num_ignored_features << endl;
      if (new_features.empty()) {
        break;
      }
      move(new_features.begin(), new_features.end(), back_inserter(features));
    }
  }

  void FeatureConstraints::initialize_constraints(
                                                  
                                                  vector<lp::LPConstraint> &constraints,
                                                  double infinity) {
    cout << "Initializing feature constraints." << endl;
    ::verify_no_axioms();
    ::verify_no_conditional_effects();

    int num_vars = g_variable_domain.size();
    for (unsigned op = 0; op < g_operators.size(); op++) {
      explicit_ops.emplace_back(g_operators[op], num_vars);
    }
    explicit_goals.resize(num_vars, NONE);
    for (const auto &goal : g_goal) {
      explicit_goals[goal.first] = goal.second;
    }

    find_features();
    constraint_offset = constraints.size();
    for (const Feature &feature : features) {
      constraints.push_back(get_constraint(feature, infinity));
    }

    // Save memory.
    utils::release_vector_memory(explicit_ops);
    utils::release_vector_memory(explicit_goals);
  }

  bool FeatureConstraints::update_constraints(
                                              const State &state, lp::LPSolver &lp_solver) {
    for (size_t feature_id = 0; feature_id < features.size(); ++feature_id) {
      const Feature &feature = features[feature_id];
      double lower_bound = feature.is_consistent_with(state) ? 1 : 0;
      int constraint_id = constraint_offset + feature_id;
      lp_solver.set_constraint_lower_bound(constraint_id, lower_bound);
    }
    return false;
  }
}


static operator_counting::ConstraintGenerator* _parse(OptionParser &parser) {
  parser.document_synopsis(
                           "Feature constraints",
                           "");
  parser.add_option<int>(
                         "max_size",
                         "maximum number of atoms per feature",
                         "2"
                         );

  Options opts = parser.parse();
  if (parser.dry_run())
    return nullptr;
  return new operator_counting::FeatureConstraints(opts);
}

static Plugin<operator_counting::ConstraintGenerator> _plugin("feature_constraints", _parse);
