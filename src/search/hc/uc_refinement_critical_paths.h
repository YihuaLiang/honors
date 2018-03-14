
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
        int threshold; //?
        ConflictData() {threshold = -1;}
        ConflictData(unsigned a, int b) : conflict(a), threshold(b) {}
    };
    // use int and unsigned value to indicate the conflict and threshold 
    // threshold is only one value -- so the lower bound should be 0
    // conflict should be a conjunction
    const bool c_use_caching;

    ConflictSelector *m_selector;
    //std::vector<Conflict> m_conflicts;
    std::vector<Conflict> m_conflicts;
    std::vector<std::vector<ConflictData> > m_conflict_data;
    std::vector<bool> m_pruned;
    std::vector<std::set<unsigned> > m_requires;//vector of a set of unsigned
    std::vector<std::set<unsigned> > m_required_by;//vector of a set of unsigned
    std::vector<std::vector<unsigned> > m_achievers;// a vector of unsigned vector 
    std::vector<const Operator *> m_plan; // the final plan

    //std::map<Fluent, ConflictData> m_duplicate_checking;

    void release_memory();
    //计算 PCR
    std::pair<bool, unsigned> compute_conflict(const Fluent &subgoal, int threshold, const State &state);

    void prepare_refinement();

    virtual RefinementResult refine(const State &state); // Calculate is protected
    virtual RefinementResult refine(
            const std::vector<State> &root_component,
            const std::unordered_set<StateID> &) {
        return refine(root_component.front());
    }
public:
    UCRefinementCritPaths(const Options &opts); //construct functions -- 
    //use option that selected by user to initialize the function
    virtual bool dead_end_learning_requires_full_component() { return false; }
    virtual bool dead_end_learning_requires_recognized_neighbors() { return false; }
    virtual void statistics();//show out the search results
    virtual void get_partial_plan(std::vector<const Operator *> &plan) const {
        plan.insert(plan.end(), m_plan.begin(), m_plan.end());
    }
    static void add_options_to_parser(OptionParser &parser);
};

#endif
