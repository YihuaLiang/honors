
#ifndef UC_REFINEMENT_CRITICAL_PATHS
#define UC_REFINEMENT_CRITICAL_PATHS

#include "uc_refinement.h"

#include "../segmented_vector.h"
#include <set>

class UCRefinementCritPaths;

struct ConflictSelector {
    static constexpr const unsigned INVALID = - 1;
    virtual unsigned operator()(UCRefinementCritPaths *h, const Fluent &subgoal, int threshold) = 0;
};

class UCRefinementCritPaths : public UCRefinement {
protected:
    struct ConflictData {
        unsigned conflict;
        int threshold;
        ConflictData() {threshold = -1;}
        ConflictData(unsigned a, int b) : conflict(a), threshold(b) {}
    };

    const bool c_use_caching;

    ConflictSelector *m_selector;
    //std::vector<Conflict> m_conflicts;
    std::vector<Conflict> m_conflicts;
    std::vector<std::vector<ConflictData> > m_conflict_data;
    std::vector<bool> m_pruned;
    std::vector<std::set<unsigned> > m_requires;
    std::vector<std::set<unsigned> > m_required_by;
    std::vector<std::vector<unsigned> > m_achievers;
    std::vector<const Operator *> m_plan;

    //std::map<Fluent, ConflictData> m_duplicate_checking;

    void release_memory();
    std::pair<bool, unsigned> compute_conflict(const Fluent &subgoal, int threshold, const State &state);

    void prepare_refinement();
    //reload
    //void prepare_refinement(int g_value);

    virtual RefinementResult refine(const State &state);
    virtual RefinementResult refine(
            const std::vector<State> &root_component,
            const std::unordered_set<StateID> &) {
        return refine(root_component.front());
    }
    //reload
    virtual RefinementResult refine(const State &state, int g_value);
    virtual RefinementResult refine(
            const std::vector<State> &root_component,
            const std::unordered_set<StateID> &,
            int g_value) {
        return refine(root_component.front(),g_value);
    }
public:
    UCRefinementCritPaths(const Options &opts);
    virtual bool dead_end_learning_requires_full_component() { return false; }
    virtual bool dead_end_learning_requires_recognized_neighbors() { return false; }
    virtual void statistics();
    virtual void get_partial_plan(std::vector<const Operator *> &plan) const {
        plan.insert(plan.end(), m_plan.begin(), m_plan.end());
    }
    static void add_options_to_parser(OptionParser &parser);
};

#endif
