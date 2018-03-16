#ifndef OPERATOR_COUNTING_FEATURE_CONSTRAINTS_H
#define OPERATOR_COUNTING_FEATURE_CONSTRAINTS_H

#include "constraint_generator.h"

#include "../utils/hash.h"
#include "../utils/collections.h"

#include <unordered_set>

namespace lp {
class LPConstraint;
}

class Options;

typedef std::pair<int, int> Fact;
namespace std {
  ostream &operator<<(ostream &o, const Fact &f);
}

namespace operator_counting {
class ExplicitOperator;

template<typename T>
class Combinations {
    std::vector<T> current_combination;
    std::vector<std::vector<T>> combinations;

    void add_combinations(const std::vector<T> &sequence, int offset, int k) {
        if (k == 0) {
            combinations.push_back(current_combination);
            return;
        }
        for (size_t i = offset; i <= sequence.size() - k; ++i) {
            assert(utils::in_bounds(i, sequence));
            current_combination.push_back(sequence[i]);
            add_combinations(sequence, i + 1, k - 1);
            current_combination.pop_back();
        }
    }

public:
    std::vector<std::vector<T>> && get_combinations(
        const std::vector<T> &sequence, int k) {
        assert(k >= 0);
        combinations.clear();
        current_combination.clear();
        int n = sequence.size();
        if (k > n) {
            return std::move(combinations);
        }
        add_combinations(sequence, 0, k);
        return std::move(combinations);
    }
};


class Feature {
    std::vector<Fact> values;

public:
    Feature(const std::vector<Fact> &&values);
    ~Feature() = default;

    bool operator==(const Feature &other) const {
        return values == other.values;
    }

    const std::vector<Fact> &get_values() const {
        return values;
    }

    bool can_be_consumed_by(const ExplicitOperator &op) const;
    bool always_produced_by(const ExplicitOperator &op) const;
    bool is_consistent_with(const std::vector<int> &partial_state) const;
    bool is_consistent_with(const State &state) const;

    friend std::ostream &operator<<(std::ostream &os, const Feature &feature);
};

std::ostream &operator<<(std::ostream &os, const Feature &feature);
}

namespace std {
template<>
struct hash<operator_counting::Feature> {
    size_t operator()(const operator_counting::Feature &feature) const {
        std::hash<std::vector<Fact>> hasher;
        return hasher(feature.get_values());
    }
};
}

namespace operator_counting {
class FeatureConstraints : public ConstraintGenerator {
    const int max_size;
    int constraint_offset;

    std::vector<ExplicitOperator> explicit_ops;
    std::vector<int> explicit_goals;
    std::vector<Feature> features;

    mutable int num_ignored_features;

    lp::LPConstraint get_constraint(const Feature &feature, double infinity) const;
    void add_feature_if_inconsistent_with_goal(
        std::unordered_set<Feature> &features, Feature &&feature) const;
    std::unordered_set<Feature> construct_features(int size);
    void find_features();

public:
    explicit FeatureConstraints(const Options &opts);
    ~FeatureConstraints() = default;

    virtual void initialize_constraints(std::vector<lp::LPConstraint> &constraints,
                                        double infinity);
    virtual bool update_constraints(const State &state, lp::LPSolver &lp_solver);
};
}

#endif
