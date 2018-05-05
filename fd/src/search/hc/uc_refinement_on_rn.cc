
#include "uc_refinement_on_rn.h"

#include "conjunction_operations.h"

#include "../globals.h"
#include "../option_parser.h"
#include "../state.h"
#include "../state_registry.h"
#include "../plugin.h"

#include <algorithm>
#include <cassert>

#include <map>
#include <cstdio>

UCRefinementOnRN::UCRefinementOnRN(const Options &opts)
    : UCRefinement(opts), c_use_root_state_conflicts(true), m_rn_map(opts.get<RNMap*>("rn"))
{}

HeuristicRefiner::RefinementResult UCRefinementOnRN::refine(const std::vector<State> &states,
                            const std::unordered_set<StateID> &rn)
{
    std::unordered_set<StateID> mapped_rn;
    (*m_rn_map)(uc, rn, mapped_rn);//contruct the
    if (prepare_refinement(states, mapped_rn)) {
        return UNCHANGED;//some how detected the dead end
    }

    Fluent goal;
    for (uint i = 0; i < g_goal.size(); i++) {
        goal.insert(g_goal[i]);
    }

    compute_conflict_set(goal);
    
    //return the update effect
    bool res = uc->update_c(_conflicts);

    _component = NULL;
    current_states.clear();
    facts_to_conflicts.clear();
    _conflicts.clear();
    root_conflicts.clear();
    successor_conflicts.clear();

    return res ? SUCCESSFUL : FAILED;
}
//reload
HeuristicRefiner::RefinementResult UCRefinementOnRN::refine(const std::vector<State> &states,
                            const std::unordered_set<StateID> &rn,
                            int g_value)
{
    
    Fluent goal;
    for (uint i = 0; i < g_goal.size(); i++) {
        goal.insert(g_goal[i]);
    }

    std::unordered_set<StateID> mapped_rn;
    (*m_rn_map)(uc, rn, mapped_rn);//contruct the map
    //debug
    //cout<<"mapped rn "<<mapped_rn.size()<<endl;
    //cout<<"states size "<<states.size()<<endl;
    
    if (prepare_refinement(states, mapped_rn, g_value)) {
        return UNCHANGED;//some how detected the dead end
    }

    compute_conflict_set(goal);
    
    //return the update effect
    bool res = uc->update_c(_conflicts);
    //debug
    // cout<<"Fluent here"<<endl;
    // for(std::vector<Conflict>::iterator i = _conflicts.begin(); i != _conflicts.end() ; i++){
    //     Fluent new_conflict = i->get_fluent();
    //     for(Fluent::iterator it = new_conflict.begin(); it != new_conflict.end(); it ++){
    //         cout<<"conj added "<<g_fact_names[it->first][it->second]<<endl;
    //     }
    // }
    _component = NULL;
    current_states.clear();
    facts_to_conflicts.clear();
    _conflicts.clear();
    root_conflicts.clear();
    successor_conflicts.clear();

    return res ? SUCCESSFUL : FAILED;
}
//reload
HeuristicRefiner::RefinementResult UCRefinementOnRN::refine(const std::vector<State> &states,
                            const std::unordered_set<StateID> &rn,
                            std::vector<std::pair<StateID,int>> g_value)
{
    
    Fluent goal;
    for (uint i = 0; i < g_goal.size(); i++) {
        goal.insert(g_goal[i]);
    }
    std::unordered_set<StateID> mapped_rn;
    (*m_rn_map)(uc, rn, mapped_rn);//contruct the map
    //debug
    // cout<<"mapped rn "<<mapped_rn.size()<<endl;
    // cout<<"new states size "<<states.size()<<endl;
    
    if(prepare_refinement(goal,states,rn,g_value,0, false)){
        return UNCHANGED;
    }//RUN AN EXTRA ONE TO FOR DETECTING CONFLICTS EXIST
    compute_conflict_set(goal, states, rn, g_value, 0, false);
    
    //return the update effect
    bool res = uc->update_c(_conflicts);
    //debug
    // cout<<"Fluent here"<<endl;
    // for(std::vector<Conflict>::iterator i = _conflicts.begin(); i != _conflicts.end() ; i++){
    //     Fluent new_conflict = i->get_fluent();
    //     cout<<endl;
    //     for(Fluent::iterator it = new_conflict.begin(); it != new_conflict.end(); it ++){
    //         cout<<"conj added "<<g_fact_names[it->first][it->second]<<endl;
    //     }
    // }
    _component = NULL;
    current_states.clear();
    facts_to_conflicts.clear();
    _conflicts.clear();
    root_conflicts.clear();
    successor_conflicts.clear();

    return res ? SUCCESSFUL : FAILED;
}
bool UCRefinementOnRN::prepare_refinement(
                            const std::vector<State> &states,
                            const std::unordered_set<StateID> &rn)
{
    // check if the states are already recognized
    bool _early = uc->set_early_termination(false);
    //only quote uc here???
    uc->evaluate(states[0]);//take the evaluation
    uc->set_early_termination(_early);
    if (uc->is_dead_end()) {
        return true; //don't need refine
    }

    // setup data structures used for the refinement
    // component which should be recognized afterwards
    _component = &states;

    //not finished??
    // used to check conflict existence (i.e., termination condition)
    facts_to_conflicts.resize(g_variable_domain.size());
    for (unsigned var = 0; var < facts_to_conflicts.size(); var++) {
        facts_to_conflicts[var].resize(g_variable_domain[var]);
    }//resize the number of conflict and the size of conflicts
    //first level --- conflict; second -- number of fact ; third --- value of fact

    // used to check whether the conflict intermediate result is a subset
    // of a state
    state_component_size = states.size();
    current_states.resize(uc->num_facts());//which fact are involved in this state
    for (unsigned var = 0; var < g_variable_domain.size(); var++) {
        for (int val = 0; val < g_variable_domain[var]; val++) {
            unsigned f = uc->get_fact_id(var, val);
            for (unsigned state = 0; state < states.size(); state++) {
                if (states[state][var] != val) {
                    current_states[f].push_back(state);
                } //
            }
        }
    }
    //heuristic will restore conjunction information

    // store conjunctions not reachable from the states in the given component
    if (c_use_root_state_conflicts) {
        root_conflicts.resize(uc->num_facts());
        for (unsigned i = 0; i < uc->num_conjunctions(); i++) {
            if (!uc->get_conjunction(i).is_achieved()) { //conjunction is not achieved ----> leave the extract to the hc
                const Fluent &fluent = uc->get_fluent(i);
                for (Fluent::const_iterator it = fluent.begin(); it != fluent.end(); it++) {
                    root_conflicts[uc->get_fact_id(it->first, it->second)].push_back(i);
                }
            }
        }
    }

    // for each successor state in the recognized neighborhood, store the conjunctions
    // that are not reachable from this state
    successor_conflicts.resize(uc->num_conjunctions());
    recognized_neighborhood_size = 0;
    for (std::unordered_set<StateID>::const_iterator succ_id = rn.begin();
         succ_id != rn.end(); succ_id++) {
        // update the uc data structure to mark all conjunctions not reachable from
        // this state
        (*m_rn_map)(uc, *succ_id);
        // collect and store all of these conjunctions
        for (unsigned i = 0; i < uc->num_conjunctions(); i++) {
            const Conjunction &conj = uc->get_conjunction(i);
            if (!conj.is_achieved()) {//push in the not achieved for successor
                successor_conflicts[i].push_back(recognized_neighborhood_size);
            }
        }
        recognized_neighborhood_size++; // add number
    }

    return false;//need refine
}
//reload
bool UCRefinementOnRN::prepare_refinement(
                            const std::vector<State> &states,
                            const std::unordered_set<StateID> &rn,
                            int g_value)//only passin the g_value of first one?
{
    // check if the states are already recognized
    bool _early = uc->set_early_termination(false);
    //only quote uc here???
    uc->evaluate(states[0],g_value);//take the evaluation
    uc->set_early_termination(_early);
    if (uc->is_dead_end()) {
        return true; //don't need refine
    }

    _component = &states;

    //not finished??
    // used to check conflict existence (i.e., termination condition)
    facts_to_conflicts.resize(g_variable_domain.size());
    for (unsigned var = 0; var < facts_to_conflicts.size(); var++) {
        facts_to_conflicts[var].resize(g_variable_domain[var]);
    }//resize the number of conflict and the size of conflicts
    //first level --- conflict; second -- number of fact ; third --- value of fact

    // used to check whether the conflict intermediate result is a subset
    // of a state
    state_component_size = states.size();
    current_states.resize(uc->num_facts());//which fact are involved in this state
    for (unsigned var = 0; var < g_variable_domain.size(); var++) {
        for (int val = 0; val < g_variable_domain[var]; val++) {
            unsigned f = uc->get_fact_id(var, val);
            for (unsigned state = 0; state < states.size(); state++) {
                if (states[state][var] != val) {
                    current_states[f].push_back(state);
                } //
                //debug
                // else if(states[state][var] == val){
                //     cout<<"in state "<<g_fact_names[var][val]<<endl;
                // }
            }
        }
    }
    //heuristic will restore conjunction information
    // store conjunctions not reachable from the states in the given component
    if (c_use_root_state_conflicts) {
        root_conflicts.resize(uc->num_facts());
        for (unsigned i = 0; i < uc->num_conjunctions(); i++) {
            if (!uc->get_conjunction(i).is_achieved()) { //conjunction is not achieved ----> leave the extract to the hc
                const Fluent &fluent = uc->get_fluent(i);
                for (Fluent::const_iterator it = fluent.begin(); it != fluent.end(); it++) {
                    root_conflicts[uc->get_fact_id(it->first, it->second)].push_back(i);
                }
            }
        }
    }
    // for each successor state in the recognized neighborhood, store the conjunctions
    // that are not reachable from this state
    successor_conflicts.resize(uc->num_conjunctions());
    recognized_neighborhood_size = 0;
    for (std::unordered_set<StateID>::const_iterator succ_id = rn.begin();
         succ_id != rn.end(); succ_id++) {
        // update the uc data structure to mark all conjunctions not reachable from
        // this state
        //reload
        //(*m_rn_map)(uc, *succ_id);
        (*m_rn_map)(uc, *succ_id, g_value);
        // collect and store all of these conjunctions
        for (unsigned i = 0; i < uc->num_conjunctions(); i++) {
            const Conjunction &conj = uc->get_conjunction(i);
            if (!conj.is_achieved()) {//push in the not achieved for successor
                successor_conflicts[i].push_back(recognized_neighborhood_size);
            }
        }
        recognized_neighborhood_size++; // add number
    }
    return false;//need refine
}
//reload
bool UCRefinementOnRN::prepare_refinement(
                            const Fluent &subgoal,
                            const std::vector<State> &states,
                            const std::unordered_set<StateID> &rn,
                            std::vector<std::pair<StateID,int>> g_value,
                            int recursive_act,
                            bool subgoal_check)
{
    // check if the states are already recognized
    bool _early = uc->set_early_termination(false);
    //debug
    //cout<<"prepare called"<<endl;
    uc->evaluate(states[0],g_value.front().second + recursive_act);//take the evaluation
    uc->set_early_termination(_early);
    if (uc->is_dead_end()) {
        return true; //don't need refine
    }
    //debug
    //cout<<"finish checking dead end in prepare"<<endl;
    _component = &states;
    
    //make it only applie to subgoal.
    if(subgoal_check){
        bool cost_flaged = false;
        for(int i = 0; i < states.size(); i++){//ignore the 0
            for(int j = 0; j < g_value.size(); j++){
                if(g_value[j].first == states[i].get_id()){
                    if(g_value[j].second + recursive_act > uc->bound) break;
                    else{
                        uc->evaluate(states[i], g_value[j].second + recursive_act);
                        for(int k = 0 ; k < uc->num_conjunctions(); k ++){
                            Fluent check_fluent = uc->get_fluent(k);
                            if((check_fluent == subgoal||fluent_op::is_subset(subgoal, check_fluent))&&uc->get_conjunction(k).is_achieved()){
                                cost_flaged = true;
                                //debug
                                //cout<<"break out in subgoal equal or contained "<<endl;
                                break;
                            }
                        }
                        if(cost_flaged) break;
                        std::vector<int> subgoal_cost;
                        subgoal_cost.resize(0);
                        for(Fluent::iterator it = subgoal.begin() ; it != subgoal.end() ; it++){
                            unsigned check_fid = uc->get_fact_id(*it);
                            const vector<unsigned> &check_conjs = uc->get_fact_conj_relation(check_fid);
                            for(int k = 0 ; k < check_conjs.size() ; k ++){
                                if(uc->get_conjunction(k).is_achieved()){
                                    subgoal_cost.push_back(uc->get_conjunction(k).cost);
                                    break;
                                }
                            }
                        }
                        if(subgoal_cost.size() == subgoal.size()){
                            cost_flaged = true;
                            //debug
                            //cout<<"subgoal break out in fact achieved separately"<<endl;
                            break;
                        }
                    }
                    break;
                }
            }
            if(cost_flaged){
                break;
            }
        }
        if(!cost_flaged){
            return true;
        }
    }
    facts_to_conflicts.resize(g_variable_domain.size());
    for (unsigned var = 0; var < facts_to_conflicts.size(); var++) {
        facts_to_conflicts[var].resize(g_variable_domain[var]);
    }
    //debug
    //cout<<"resize fact to conflict succeed"<<endl;
    // used to check whether the conflict intermediate result is a subset of a state
    state_component_size = states.size();
    //recompute the facts
    current_states.clear();
    current_states.resize(uc->num_facts());//which fact are involved in this state
    for (unsigned var = 0; var < g_variable_domain.size(); var++) {
        for (int val = 0; val < g_variable_domain[var]; val++) {
            unsigned f = uc->get_fact_id(var, val);
            for (unsigned state = 0; state < states.size(); state++) {
                if (states[state][var] != val) {
                    current_states[f].push_back(state);
                } 
                //debug
                // else if(states[state][var] == val){
                //     cout<<"in state "<<g_fact_names[var][val]<<endl;
                // }
            }
        }
    }
    //store conjunctions not reachable from the states in the given component
    if (c_use_root_state_conflicts) {
        root_conflicts.resize(uc->num_facts());
        for (unsigned i = 0; i < uc->num_conjunctions(); i++) {
            if (!uc->get_conjunction(i).is_achieved()) { //conjunction is not achieved ----> leave the extract to the hc
                const Fluent &fluent = uc->get_fluent(i);
                for (Fluent::const_iterator it = fluent.begin(); it != fluent.end(); it++) {
                    root_conflicts[uc->get_fact_id(it->first, it->second)].push_back(i);
                }
            }
        }
    }
    //clear facts
    successor_conflicts.clear();
    successor_conflicts.resize(uc->num_conjunctions());
    recognized_neighborhood_size = 0;
    for (std::unordered_set<StateID>::const_iterator succ_id = rn.begin();
         succ_id != rn.end(); succ_id++) {
         for(int j = 0 ; j < g_value.size() ; j++) {
            if(g_value[j].first == *succ_id){
                (*m_rn_map)(uc, *succ_id, g_value[j].second + recursive_act);
                // collect and store all of these conjunctions
                for (unsigned i = 0; i < uc->num_conjunctions(); i++) {
                    const Conjunction &conj = uc->get_conjunction(i);
                    if (!conj.is_achieved()) {//push in the not achieved for successor
                        successor_conflicts[i].push_back(recognized_neighborhood_size);
                        //debug
                        // Fluent conflict_fluent = uc->get_fluent(conj.id);
                        // cout<<"conj id "<<i<<endl;
                        // for(Fluent::iterator itt = conflict_fluent.begin(); itt != conflict_fluent.end();itt++ ){
                        //     cout<<"conflict add conj "<<g_fact_names[itt->first][itt->second]<<endl;
                        // }
                    }
                }
                recognized_neighborhood_size++;
                break;
            }
        }
    }
    //debug
    //cout<<"neighbors hc done recognized neigbhbor "<<recognized_neighborhood_size<<endl;
    return false;//need refine
}
/* returns true if (a) there is already a conjunction subset of subgoal that is
   unreachable from the current states, or (b) there is already a conflict that is
   a subset of subgoal. */
bool UCRefinementOnRN::conflict_exists(const Fluent &subgoal) const
{
    std::vector<int> subset;
    if (c_use_root_state_conflicts) {
        subset.resize(uc->num_conjunctions(), 0);
        for (Fluent::const_iterator it = subgoal.begin(); it != subgoal.end(); it++) {
            const std::vector<int> &conjs =
                root_conflicts[uc->get_fact_id(it->first, it->second)];
            for (const int & conj : conjs) {
                if (++subset[conj] == uc->get_conjunction(conj).fluent_size) {
                    return true;
                }
            }
        }
    }
    subset.clear();
    subset.resize(_conflicts.size(), 0);
    for (Fluent::const_iterator it = subgoal.begin(); it != subgoal.end(); it++) {
        const std::vector<unsigned> &confls = facts_to_conflicts[it->first][it->second];
        for (const unsigned & confl : confls) {
            if ((unsigned) ++subset[confl] == _conflicts[confl].get_fluent().size()) {
                return true;
            }
        }
    }
    subset.clear();

    return false;
}

void UCRefinementOnRN::compute_conflict_set(Fluent &subgoal)
{
    if (conflict_exists(subgoal)) { // the if() line
        return;
    }

    unsigned conflict_id = _conflicts.size(); //enlarge the size
    _conflicts.resize(_conflicts.size() + 1); //
    Conflict &conflict = _conflicts.back();
    //Extract
    select_conflict(subgoal, conflict);
    break_subset(subgoal, conflict);

    for (Fluent::const_iterator it = conflict.get_fluent().begin();
         it != conflict.get_fluent().end(); it++) {
        facts_to_conflicts[it->first][it->second].push_back(conflict_id);
    }

    std::vector<int> can_add;
    uc->compute_can_add(can_add, conflict.get_fluent(), true);
    for (unsigned act = 0; act < can_add.size(); act++) {
        // NOTE that the conflict reference might become invalid due to vector
        // reallocation when going into recursion!
        if (can_add[act] != 1) {
            continue;
        }
        //find the action 
        const StripsAction &action = uc->get_action(act);
        //recursive call
        Fluent pre;
        pre.insert(action.precondition.begin(), action.precondition.end());
        fluent_op::set_minus(_conflicts[conflict_id].get_fluent(), action.add_effect,
                             pre);

        //if (!uc->contains_mutex(pre)) {
        compute_conflict_set(pre);
        //here need recompute??
            //} 
    }
    can_add.clear();
}
//reload
void UCRefinementOnRN::compute_conflict_set(Fluent &subgoal, 
                                    const std::vector<State>& states,
                                    const std::unordered_set<StateID> & rn,
                                    std::vector<std::pair<StateID,int>> g_value,
                                    int recursive_act,
                                    bool subgoal_check)
{
    if (conflict_exists(subgoal)) { // the if() line
        return;
    }
    //debug
    // cout<<"right compute called"<<endl;
    // for(Fluent::iterator it = subgoal.begin(); it != subgoal.end(); it++){
    //     cout<<"In subgoal "<<g_fact_names[it->first][it->second]<<endl;
    // }
    if(prepare_refinement(subgoal, states, rn, g_value, recursive_act, subgoal_check)){//already dead end return
        return;
    }

    unsigned conflict_id = _conflicts.size(); //enlarge the size
    _conflicts.resize(_conflicts.size() + 1); //
    Conflict &conflict = _conflicts.back();
    //Extract
    select_conflict(subgoal, conflict);
    //debug
    // cout<<"conflict size "<<conflict.get_fluent().size()<<endl;
    // Fluent new_conflict = conflict.get_fluent();
    // for(Fluent::iterator it = new_conflict.begin(); it != new_conflict.end(); it ++){
    //     cout<<"out select conj added "<<g_fact_names[it->first][it->second]<<endl;
    // }
    
    break_subset(subgoal, conflict);

    for (Fluent::const_iterator it = conflict.get_fluent().begin();
         it != conflict.get_fluent().end(); it++) {
        facts_to_conflicts[it->first][it->second].push_back(conflict_id);
    }

    std::vector<int> can_add;
    uc->compute_can_add(can_add, conflict.get_fluent(), true);
    for (unsigned act = 0; act < can_add.size(); act++) {
        // NOTE that the conflict reference might become invalid due to vector
        // reallocation when going into recursion!
        if (can_add[act] != 1) {
            continue;
        }
        //find the action 
        const StripsAction &action = uc->get_action(act);
        //recursive call
        if(m_zero_achievers.count(action.aid)){
                //the zero cost action has been used once -- do not take it again
                continue;
        }else if(action.base_cost == 0){
           m_zero_achievers.insert(action.aid);
        }
        Fluent pre;
        pre.insert(action.precondition.begin(), action.precondition.end());
        fluent_op::set_minus(_conflicts[conflict_id].get_fluent(), action.add_effect,
                             pre);

        //if (!uc->contains_mutex(pre)) {
        compute_conflict_set(pre, states, rn, g_value, recursive_act + action.base_cost, true);
        //here need recompute??
            //} 
    }
    can_add.clear();
}

void UCRefinementOnRN::break_subset(const Fluent &subgoal,
                                  Conflict &conflict) const
{
    std::vector<bool> counter;
    counter.resize(uc->num_facts(), false);
    std::vector<int> conflicts;
    conflicts.resize(state_component_size, -1);
    int togo = state_component_size;
    for (Fluent::const_iterator it = conflict.get_fluent().begin(); togo > 0
         && it != conflict.get_fluent().end();
         it++) {
        unsigned cid = uc->get_fact_id(it->first, it->second);
        counter[cid] = true;
        for (const unsigned & s : current_states[cid]) {
            if (conflicts[s] == -1) {
                togo--;
            }
            conflicts[s] = cid;
        }
    }
    if (togo == 0) {
        return;
    }
    for (Fluent::const_iterator it = subgoal.begin(); togo > 0
         && it != subgoal.end();
         it++) {
        unsigned cid = uc->get_fact_id(it->first, it->second);
        bool found_new = false;
        for (const unsigned & s : current_states[cid]) {
            if (conflicts[s] == -1) {
                found_new = true;
                break;
            }
        }
        if (found_new) {
            for (const unsigned & s : current_states[cid]) {
                if (conflicts[s] == -1) {
                    togo--;
                }
                conflicts[s] = cid;
            }
        }
    }
#ifndef NDEDBUG
    // if (togo > 0) {
    //     std::cout << "COULD NOT ENSURE SUBSET PROPERTY!" << std::endl;
    //     uc->dump_fluent_pddl(subgoal);
    //     std::cout << std::endl;
    //     for (uint i = 0; i < conflicts.size(); i++) {
    //         if (conflicts[i] == -1) {
    //             std::cout << "STATE " << i << std::endl;
    //             (*_component)[i].dump_pddl();
    //         }
    //     }
    // }
#endif
    //assert(togo == 0);
    for (unsigned i = 0; i < conflicts.size(); i++) {
        if(conflicts[i]==-1) continue;
        assert(conflicts[i] >= 0);
        if (counter[conflicts[i]]) {
            continue;
        }
        counter[conflicts[i]] = true;
        conflict.merge(uc->get_fluent(conflicts[i]));
    }
    counter.clear();
}



class GreedyUCRefinementOnRN : public UCRefinementOnRN {
protected: 
    virtual void select_conflict(const Fluent &subgoal, Conflict &conflict);
public:
    GreedyUCRefinementOnRN(const Options &opts);
};


GreedyUCRefinementOnRN::GreedyUCRefinementOnRN(const Options &opts)
    : UCRefinementOnRN(opts) {}

void GreedyUCRefinementOnRN::select_conflict(const Fluent &subgoal,
        Conflict &conflict)
{
    std::vector<int> counter;
    counter.resize(uc->num_conjunctions(), 0);
    std::vector<int> conflicts;
    conflicts.resize(recognized_neighborhood_size, -1);
    int togo = recognized_neighborhood_size;
    for (Fluent::const_iterator it = subgoal.begin(); togo > 0
         && it != subgoal.end();
         it++) {//for every fact in subgoal,, found the conjunction related to it
        const std::vector<unsigned> &conjs = uc->get_fact_conj_relation(uc->get_fact_id(
                *it));
        for (const unsigned & cid : conjs) {
            if (++counter[cid] == uc->get_conjunction(cid).fluent_size) {//why add one?
                bool found_new = false;
                for (const unsigned & succ : successor_conflicts[cid]) {
                    if (conflicts[succ] == -1) {//all be -1 at the beginning
                        found_new = true;
                        break;
                    }
                }
                if (found_new) {
                    for (int succ : successor_conflicts[cid]) {
                        if (conflicts[succ] == -1) {
                            togo--;
                        }
                        conflicts[succ] = cid;
                    }
                }
            }
        }
    }
    assert(togo == 0);
    counter.clear();
    counter.resize(uc->num_conjunctions(), 0);
    for (unsigned i = 0; i < recognized_neighborhood_size; i++) {
        if (counter[conflicts[i]]) {
            continue;
        }
        counter[conflicts[i]] = 1;
        conflict.merge(uc->get_fluent(conflicts[i]));
    }
    counter.clear();
}

void UCRefinementOnRN::add_options_to_parser(OptionParser &parser) {
    UCRefinement::add_options_to_parser(parser);
    parser.add_option<RNMap*>("rn", "", "state");
}


template<typename T>
class Greedy2UCRefinementOnRN : public UCRefinementOnRN {
protected:
    const bool update_successors;
    const bool update_size;
    std::vector<int> open_successors;
    std::vector<int> open_facts;
    T update_best;
    //int c1_max_successors;
    //int c2_min_size;
    int best;
    unsigned best_pos;
    //bool update_best(int cid);
    virtual void select_conflict(const Fluent &subgoal, Conflict &conflict);
public:
    Greedy2UCRefinementOnRN(const Options &opts);
    static void add_options_to_parser(OptionParser &parser) {
        UCRefinementOnRN::add_options_to_parser(parser);
        parser.add_option<bool>("succ", "", "true");
        parser.add_option<bool>("size", "", "false");
    }
};

template<typename T>
Greedy2UCRefinementOnRN<T>::Greedy2UCRefinementOnRN(const Options &opts)
    : UCRefinementOnRN(opts),
      update_successors(opts.get<bool>("succ")),
      update_size(opts.get<bool>("size")),
      update_best(open_successors, open_facts)
{}

template<typename T>
void Greedy2UCRefinementOnRN<T>::select_conflict(const Fluent &subgoal,
        Conflict &conflict)
{
    std::deque<unsigned> candidates;
    std::vector<int> counter;
    counter.resize(uc->num_conjunctions(), 0);

    std::vector<std::vector<unsigned> > selected_conflicts;
    selected_conflicts.resize(recognized_neighborhood_size);

    open_successors.resize(uc->num_conjunctions(), 0);
    open_facts.resize(uc->num_conjunctions(), 0);

    best = -1;

    update_best.reset();
    for (Fluent::const_iterator it = subgoal.begin(); it != subgoal.end();
         it++) {
        //debug
        //cout<<"it is "<<g_fact_names[it->first][it->second]<<endl;
        const std::vector<unsigned> &conjs = uc->get_fact_conj_relation(uc->get_fact_id(
                *it));
        //debug
        // cout<<"conjs size "<<conjs.size()<<endl;
        // for(int k = 0 ; k < conjs.size(); k++){
        //     cout<<"conjs id "<<conjs[k]<<endl;
        // }
        for (const unsigned & cid : conjs) {
            if (++counter[cid] == uc->get_conjunction(cid).fluent_size
                    && successor_conflicts[cid].size() > 0) {
                //debug
                //cout<<"cid added "<<cid<<endl;
                candidates.push_back(cid);
                for (const int &x : successor_conflicts[cid]) {
                    selected_conflicts[x].push_back(cid);
                }
                open_successors[cid] = successor_conflicts[cid].size();
                open_facts[cid] = uc->get_fluent(cid).size();
                if (update_best(cid)) {
                    //debug
                    //cout<<"updated cid "<<cid<<endl;
                    best = cid;
                    best_pos = candidates.size() - 1;
                }
            }
        }
    }

    std::vector<bool> successor_handled;
    successor_handled.resize(recognized_neighborhood_size, false);
    std::vector<bool> conjunction;
    conjunction.resize(uc->num_facts(), false);

    int togo = recognized_neighborhood_size;
    while (togo > 0 && !candidates.empty()) {
        bool merge = false;

        for (const unsigned & i : successor_conflicts[best]) {
            if (!successor_handled[i]) {
                merge = true;
                successor_handled[i] = true;
                togo--;
                if (update_successors) {
                    for (const unsigned &cid : selected_conflicts[i]) {
                        --open_successors[cid];
                    }
                }
            }
        }

        if (merge) {
            conflict.merge(uc->get_fluent(best));
        }

        // TODO only if merged?
        if (update_size && open_facts[best] > 0) {
            const Fluent &fluent = uc->get_fluent(best);
            for (const Fact &fact : fluent) {
                unsigned fid = uc->get_fact_id(fact.first, fact.second);
                if (!conjunction[fid]) {
                    conjunction[fid] = true;
                    const std::vector<unsigned> &conjs = uc->get_fact_conj_relation(fid);
                    for (const unsigned &cid : conjs) {
                        --open_facts[cid];
                    }
                }
            }
        }

        //candidates.erase(candidates.begin() + best_pos);
        best = -1;
        best_pos = -1;
        update_best.reset();
#if 1
        uint i = 0;
        std::deque<unsigned>::iterator it = candidates.begin();
        while (it != candidates.end()) {
            if (open_successors[*it] == 0) {
                it = candidates.erase(it);
                continue;
            }
            if (open_facts[*it] == 0) {
                best = *it;
                best_pos = i;
                break;
            }
            if (update_best(*it)) {
                best = *it;
                best_pos = i;
            }
            it++;
            i++;
        }
#else
        std::vector<unsigned> newcandidates;
        newcandidates.reserve(candidates.size());
        for (uint i = 0; i < candidates.size(); i++) {
            unsigned cid = candidates[i];
            if (open_successors[cid] > 0) {
                newcandidates.push_back(cid);
                if (update_best(cid)) {
                    best_pos = newcandidates.size() - 1;
                }
            }
        }
        candidates.swap(newcandidates);
#endif
    }

    open_successors.clear();
    open_facts.clear();

    assert(togo == 0);
}

struct Sel1 {
    const std::vector<int> &open_successors;
    const std::vector<int> &open_facts;
    int c1_max_successors;
    int c2_min_size;
    bool resetted;
    Sel1(const std::vector<int> &open_successors, const std::vector<int> &open_facts)
        : open_successors(open_successors), open_facts(open_facts), resetted(true)
    {}
    bool operator()(int cid) {
        if (resetted ||
            open_facts[cid] == 0 ||
            open_successors[cid] > c1_max_successors ||
            (open_successors[cid] == c1_max_successors
             && open_facts[cid] < c2_min_size)) {
            c1_max_successors = open_successors[cid];
            c2_min_size = open_facts[cid];
            resetted = false;
            return true;
        }
        return false;
    }
    void reset() {
        resetted = true;
    }
};

struct Sel2 {
    const std::vector<int> &open_successors;
    const std::vector<int> &open_facts;
    int x, y;
    bool resetted;
    Sel2(const std::vector<int> &open_successors, const std::vector<int> &open_facts)
        : open_successors(open_successors), open_facts(open_facts), resetted(true)
    {}
    bool operator()(int cid) {
        if (resetted ||
            x < open_facts[cid] ||
            (x == open_facts[cid] && y > open_successors[cid])) {
            x = open_facts[cid];
            y = open_successors[cid];
            resetted = false;
            return true;
        }
        return false;
    }
    void reset() {
        resetted = true;
    }
};


static HeuristicRefiner *_parse(OptionParser &parser)
{
    UCRefinementOnRN::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (!parser.dry_run()) {
        return new GreedyUCRefinementOnRN(opts);
    }
    return NULL;
}

template<typename T>
static HeuristicRefiner *_parse2(OptionParser &parser)
{
    Greedy2UCRefinementOnRN<T>::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (!parser.dry_run()) {
        return new Greedy2UCRefinementOnRN<T>(opts);
    }
    return NULL;
}

static Plugin<HeuristicRefiner> _plugin_rn1("ucrn1", _parse);
static Plugin<HeuristicRefiner> _plugin_rn2_1("ucrn2_1", _parse2<Sel1>);
static Plugin<HeuristicRefiner> _plugin_rn2_2("ucrn2_2", _parse2<Sel2>);
