#ifndef MERGE_AND_SHRINK_SHRINK_EMPTY_LABELS_H
#define MERGE_AND_SHRINK_SHRINK_EMPTY_LABELS_H

#include "shrink_strategy.h"
#include <list>
#include <vector>

using namespace std;

class Options;

class ShrinkEmptyLabels : public ShrinkStrategy {
    void find_sccs(
    		vector<int>& stack,
    		vector<bool>& in_stack,
    		EquivalenceRelation& final_sccs,
    		const vector<vector<int> >& adjacency_matrix,
    		vector<int>& indices,
    		vector<int>& lowlinks,
    		int& index,
    		const int state,
    		const bool all_goal_vars_in,
    		vector<bool>& is_goal,
    		vector<int>& state_to_scc);

public:
    ShrinkEmptyLabels(const Options &opts);
    virtual ~ShrinkEmptyLabels();

    virtual std::string name() const;
    virtual void dump_strategy_specific_options() const;

    virtual bool reduce_labels_before_shrinking() const;

    virtual void shrink(Abstraction &abs, int target, bool force = false);
    virtual void shrink_atomic(Abstraction &abs);
    virtual void shrink_before_merge(Abstraction &abs1, Abstraction &abs2);

    static ShrinkEmptyLabels *create_default();

private:
    void determine_empty_label_operators(Abstraction& abs, vector<bool>& operators);

};

#endif
