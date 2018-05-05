
#ifndef UC_REFINEMENT_ON_RN_H
#define UC_REFINEMENT_ON_RN_H

#include "uc_refinement.h"
#include "rn_map.h"

class UCRefinementOnRN : public UCRefinement {
protected:
    const bool c_use_root_state_conflicts;

    RNMap *m_rn_map;

    /* Input or computed from input */
    const std::vector<State> *_component;
    size_t state_component_size;
    std::vector<std::vector<unsigned> > current_states;
    std::vector<std::vector<int> > root_conflicts;
    size_t recognized_neighborhood_size;
    std::vector<std::vector<int> > successor_conflicts;
    std::set<unsigned>m_zero_achievers; //aid should be seem as the order

    /* Output data */
    std::vector<Conflict> _conflicts;
    std::vector<std::vector<std::vector<unsigned> > > facts_to_conflicts;
    void compute_conflict_set(Fluent &subgoal);
    //reload
    void compute_conflict_set(Fluent &subgoal, 
            const std::vector<State> &states, 
            const std::unordered_set<StateID> &rn, 
            std::vector<std::pair<StateID,int>> g_value, 
            int recursive_act,
            bool subgoal_check);

    bool conflict_exists(const Fluent &x) const;
    void break_subset(const Fluent &subgoal, Conflict &conflict) const;
    virtual void select_conflict(const Fluent &subgoal, Conflict &conflict) = 0;

    bool prepare_refinement(const std::vector<State> &states, const std::unordered_set<StateID> &rn);
    //reload
    bool prepare_refinement(const std::vector<State> &states, const std::unordered_set<StateID> &rn, int g_value);
    //reload
    bool prepare_refinement(const Fluent &subgoal,
                            const std::vector<State> &states, 
                            const std::unordered_set<StateID> &rn, 
                            std::vector<std::pair<StateID,int>> g_value,
                            int recursive_act,
                            bool subgoal_check);
    virtual RefinementResult refine(
            const std::vector<State> &root_component,
            const std::unordered_set<StateID> &recognized_neighbors);
    //reload
    virtual RefinementResult refine(
            const std::vector<State> &root_component,
            const std::unordered_set<StateID> &recognized_neighbors,
            int g_value);
    //reload
    virtual RefinementResult refine(
            const std::vector<State> &root_component,
            const std::unordered_set<StateID> &recognized_neighbors,
            std::vector<std::pair<StateID,int>> g_value);
// struct state_to_conjunction{
//         StateID state_id;
//         unsigned conjunction_id;
//         int cost;
//         state_to_conjunction(StateID state_id, unsigned id, int cost):
//         state_id(state_id),conjunction_id(id),cost(cost){}

//         inline void increase_cost(int action_cost){
//                  if(cost < 0) return;
//                  cost += action_cost;
//         }
//         inline void decrease_cost(int action_cost){
//                 if(cost < 0) return;
//                 cost-=action_cost;
//         }
// };
// std::vector<state_to_conjunction> recursive_cost;

public:
    UCRefinementOnRN(const Options &opts);
    virtual bool dead_end_learning_requires_full_component() { return true; }
    virtual bool dead_end_learning_requires_recognized_neighbors() { return true; }
    virtual void statistics() {
        UCRefinement::statistics();
    }
    static void add_options_to_parser(OptionParser &parser);
};

#endif
