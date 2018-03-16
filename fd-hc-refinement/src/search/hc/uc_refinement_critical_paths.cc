
#include "uc_refinement_critical_paths.h"

#include "hc_heuristic.h"
#include "conjunction_operations.h"// see the definition of conjunction

#include "../globals.h"
#include "../state.h"
#include "../option_parser.h"
#include "../plugin.h"

#include "../operator.h"
#include "../utilities.h"

#include <string>

//int START = 0;
// select the nogood
// uc may be a variable inherited from the ucrefinement
struct GreedyConflictSelector : public ConflictSelector {
    virtual unsigned operator()(UCRefinementCritPaths *r, const Fluent &subgoal,
                                int threshold) { // this function will call the refiner as a pointer--
        UCHeuristic *pic = dynamic_cast<UCHeuristic*>(r->get_heuristic()); // get the heuristic value
        // the get_heuristic is inherited from predecessors ---- pic is the heuristic
        int best_size = 0;
        int res = -1; // the result size
        std::vector<int> in_fluent;
        in_fluent.resize(pic->num_conjunctions(), 0);
        for (Fluent::iterator f = subgoal.begin(); f != subgoal.end(); f++) { // go through the subgoal set
            const vector<unsigned> &f_conjs = // go through the subgoalset 
                pic->get_fact_conj_relation(pic->get_fact_id(*f));
            for (uint i = 0; i < f_conjs.size(); i++) {
                unsigned cid = f_conjs[i]; 
                const Conjunction &conj = pic->get_conjunction(cid);
                if (++in_fluent[cid] == conj.fluent_size && // 
                    (!conj.is_achieved() ||
                     conj.cost >= threshold)) {
                    if (res == -1 || conj.fluent_size < best_size) { // greedy principle
                        res = cid; // the conjunction id
                        best_size = conj.fluent_size; // record the size of conjunction
                    }
                }
            }
        }
        in_fluent.clear(); //release the memory
        return res;
    }
};
// constructing function
UCRefinementCritPaths::UCRefinementCritPaths(const Options &opts) :
    UCRefinement(opts),// call another construct function
    c_use_caching(opts.get<bool>("caching")),
    m_selector(opts.get<ConflictSelector*>("conflicts"))
{}


void UCRefinementCritPaths::statistics()
{
    UCRefinement::statistics();// see what this function is used for
}

void UCRefinementCritPaths::release_memory() {
    m_conflicts.clear();
    m_achievers.clear();
    m_plan.clear();

    //m_duplicate_checking.clear();
    m_conflict_data.clear();
    m_pruned.clear();
    m_requires.clear();
    m_required_by.clear();
}

void UCRefinementCritPaths::prepare_refinement() {
    m_achievers.resize(uc->num_conjunctions());// it is defined in UCrefinement
    for (unsigned i = 0; i < uc->num_counters(); i++) {
        const ActionEffectCounter &counter = uc->get_counter(i);
        if (counter.unsatisfied_preconditions == 0 // it is not be a dead end (not recognized as dead end)
                && counter.cost + counter.base_cost == counter.effect->cost) {
                // The first condition is ok, the second condition not good    
            m_achievers[counter.effect->id].push_back(counter.action_id);
        }// found action for certain effects
    } // this prepare function work to find achiever id for 
    if (c_use_caching) {
        m_conflict_data.resize(uc->num_facts());
    }
#ifndef NDEBUG
    for (unsigned i = 0; i < uc->num_conjunctions(); i++) {
        const Conjunction &conj = uc->get_conjunction(i);
        assert(!conj.is_achieved() || conj.cost == 0 || m_achievers[conj.id].size() > 0);
    }
#endif
}
// this could be refine algorithms
//return the refinement results
HeuristicRefiner::RefinementResult UCRefinementCritPaths::refine(
    const State &state)// 
{
    //std::cout << ">>>>>>>>>>>REFINEMENT_START" << std::endl;
#ifndef NDEBUG // what is checked here?
    for (unsigned i = 0; i < uc->num_counters(); i++) {
        assert(uc->get_counter(i).base_cost >= 1);
    }
#endif
    bool updated_c = false;
    RefinementResult successful = SUCCESSFUL;// judge the results -- an enum type
    int old_val = -2;
    Fluent goal; // find and add the goal
    for (uint i = 0; i < g_goal.size(); i++) {
        goal.insert(g_goal[i]);
    } // constructe the goal
    while (successful != FAILED) { // what does FAILED mean
        bool x = uc->set_early_termination(false);
        uc->evaluate(state); // will get the heuristic value
        uc->set_early_termination(x);
#ifndef NDEBUG
        std::cout << "{val = " << uc->get_value() << "}" << std::endl;
#endif
        //std::cout << "val = " << uc->get_value() << std::endl;
        if (uc->is_dead_end()) {//if the state is judged as dead end, then don't need to refine
            //std::cout << ">>>>>>>>>>>REFINEMENT_END" << std::endl;
            return updated_c ? SUCCESSFUL : UNCHANGED;// if c does not change then no change is made 
            // leave this function, if return at first time -- unchanged. the following time--successful
        }
        if (uc->get_value() == old_val) { // uc value is not changed but c is updated, so some thing is wrong
            assert(false);
            break;
        }
        // now actually require the refinement operation
        old_val = uc->get_value(); // get the uc value
        updated_c = true; // c is really updated
        prepare_refinement(); // prepare the achiever vector
        //std::cout << "computing conflicts..." << std::flush;
        //START = uc->get_value();
        // begin refinement
        std::pair<bool, unsigned> res = compute_conflict(goal, uc->get_value(), state);
        //std::cout << "done => " << m_conflicts.size() << std::endl;
        if (res.second == (unsigned) -1) {//check state_id -- using as a special value
            //std::cout << "Found plan in uC refinement!" << std::endl;
            successful = SOLVED;//It means the result plan is found
            break;
        } else { // what is the meaning in this part
            if (c_use_caching) { // what is c_use_caching
                m_required_by.clear();
                unsigned i = 0;
                std::vector<unsigned> open;
                open.push_back(res.second);// put this state into the open vector
                m_pruned[res.second] = true;//cut down the state
                while (i < open.size()) {
                    for (std::set<unsigned>::iterator it = m_requires[open[i]].begin(); it != m_requires[open[i]].end(); it++) {
                        if (!m_pruned[*it]) {
                            m_pruned[*it] = true;
                            open.push_back(*it);
                        }
                    }
                    i++;
                }
                for (uint i = 0; i < m_conflicts.size(); i++) {
                    if (m_pruned[i]) {
                        if (uc->exceeded_size_limit()) {
                            successful = FAILED;
                            break;
                        } else {
                            uc->add_conflict(m_conflicts[i]);
                        }
                    }
                }
            }// this means c isn's updated...no more operation can be used --- end
            else if (!uc->update_c(m_conflicts)) {
                successful = FAILED;
            }
        }
#ifndef NDEBUG
        uc->dump_compilation_information(std::cout);
#endif
        //std::cout << "successful = " << successful << std::endl;
        //exit(1);
        release_memory();// finish refinement
    }
    //assert(false);
    //std::cout << ">>>>>>>>>>>REFINEMENT_END" << std::endl;
    return successful;
}
//PCR main algorithms 计算nogood
std::pair<bool, unsigned> UCRefinementCritPaths::compute_conflict(
    const Fluent &subgoal, int threshold, const State &state)
{
    //ConflictData *confl_data = NULL;
    // this part a little confusing
    if (c_use_caching) {
#if 0 // work as comment
        confl_data = &m_duplicate_checking[subgoal];
        if (confl_data->threshold  >= threshold) {
            return make_pair(false, confl_data->conflict);
        }
#else
        std::vector<unsigned> subset(m_conflicts.size(), 0);
        for (Fluent::const_iterator it = subgoal.begin(); it != subgoal.end(); it++) {
            unsigned fid = uc->get_fact_id(*it);
            const std::vector<ConflictData> &confls = m_conflict_data[fid];
            for (const ConflictData &data : confls) {
                if (data.threshold >= threshold &&
                        ++subset[data.conflict] == m_conflicts[data.conflict].get_fluent().size()) {
                    return std::make_pair(false, data.conflict);
                }
            }
        }
#endif
}
// enlarge conflict sets
    unsigned conflict_id = m_conflicts.size();// guess conflict is the set of C
    // every time just need to add a conjunction to the end of this set
    m_requires.resize(m_requires.size() + 1);
    m_required_by.resize(m_required_by.size() + 1);
    m_conflicts.resize(m_conflicts.size() + 1);
    m_pruned.push_back(false);
    if (!uc->extract_mutex(subgoal, m_conflicts[conflict_id])) {// 
        // there is no mutual exclusion in subgoal i.e. nothing is added into conflict
        unsigned cid = (*m_selector)(this, subgoal, threshold); // cid is a selector
        if (cid == ConflictSelector::INVALID) { // check whether it is effective
            return make_pair(false, -1); // there is a mutex and the selected conflict is invalid
        }
        // this conjunction cannot be achieved 
        if (!uc->get_conjunction(cid).is_achieved() ||
            uc->get_conjunction(cid).cost > threshold) {
            return make_pair(true, cid); //then this pair(conjunction,true) is returned
        }
        //
        m_conflicts[conflict_id].merge(uc->get_fluent(cid));//insert this fluent into conflict
        const vector<unsigned> &candidates = m_achievers[cid];// select a set
        // try all possible actions
        for (size_t i = 0; i < candidates.size(); i++) {
            const StripsAction &action = uc->get_action(candidates[i]);// use id to select an action
            Fact f_del;
            if (fluent_op::intersection_not_empty(subgoal, action.del_effect, f_del)) {
                m_conflicts[conflict_id].merge(f_del); 
                // some action in the subgoal will be deleted -- add it
                // then continue try rest candidates
                continue;
            }

            //std::cout <<
            //    std::string(START - threshold + 1, ' ') << g_operators[action.operator_no].get_name() << std::endl;
            Fluent regr; // used for regression -- at this position, 
            // add in the precondition -- 
            regr.insert(action.precondition.begin(), action.precondition.end()); 
            fluent_op::set_minus(subgoal, action.add_effect, regr);//minus the add_effect
            assert(action.base_cost > 0); // abnormal stop
            pair<bool, unsigned> child_confl =
                compute_conflict(regr, threshold - action.base_cost, state); // recursion calculate
            regr.clear(); // regression
            if (child_confl.second == (unsigned) -1) {//check the returned status
                //_plan.push_back(action.operator_no);
                // the state id is a special value
                // the operator_no is defined in the stripsacion
                m_plan.push_back(&g_operators[action.operator_no]);
                return child_confl;
            }
            if (child_confl.first) {// if true the remove variables in precondition
                fluent_op::set_minus(uc->get_fluent(child_confl.second),
                                     action.precondition, regr);
            } else {
                assert(child_confl.second < m_conflicts.size());// stop when it is not true(0)
                m_requires[conflict_id].insert(child_confl.second);// insert the found conflict
                m_required_by[child_confl.second].insert(conflict_id);// see what is required by child_confl
                fluent_op::set_minus(m_conflicts[child_confl.second].get_fluent(),
                                     action.precondition, regr);
            }
            m_conflicts[conflict_id].merge(regr);// insert this set to the conflict
            regr.clear();
        }

        bool is_subset = true;
        // check whether 
        //it -> a pair of fact, go through the selected conflict.
        //The conflict_id is the conflict that we will select
        for (Fluent::iterator it = m_conflicts[conflict_id].get_fluent().begin();
                is_subset && it != m_conflicts[conflict_id].get_fluent().end(); it++) {
            if (state[it->first] != it->second) {// what this mean? it is a fluent
                is_subset = false;// end this for loop
            }
        }// if it is broken then this if..else will not be used
        //mark down
        if (is_subset) {// go through subgoal 
            for (Fluent::iterator it = subgoal.begin(); it != subgoal.end(); it++) {
                if (state[it->first] != it->second) {
                    is_subset = false;
                    m_conflicts[conflict_id].merge(*it);//insert this fact to the conflict
                }
            }
            if (is_subset) {
                return std::make_pair(false, -1);
            }
        }
    }
    if (c_use_caching) {
#if 0
        confl_data->threshold = threshold;
        confl_data->conflict = m_conflicts.size();
#else
        assert(m_requires.size() == m_conflicts.size());
        assert(m_required_by.size() == m_conflicts.size());
        assert(m_pruned.size() == m_conflicts.size());
        std::vector<unsigned> subset(m_conflicts.size(), 0);
        for (Fluent::iterator it = m_conflicts[conflict_id].get_fluent().begin();
                it != m_conflicts[conflict_id].get_fluent().end(); it++) {
            unsigned fid = uc->get_fact_id(*it);
            const std::vector<ConflictData> &confls = m_conflict_data[fid];
            for (const ConflictData &data : confls) {
                if (data.threshold <= threshold &&
                        ++subset[data.conflict] == m_conflicts[conflict_id].get_fluent().size()) {
                    assert(data.conflict < m_conflicts.size());
                    //m_pruned[data.conflict] = true;
                    std::set<unsigned> &reqby = m_required_by[data.conflict];
                    for (std::set<unsigned>::iterator it2 = reqby.begin(); it2 != reqby.end(); it2++) {
                        assert(*it2 < m_conflicts.size());
                        assert(m_requires[*it2].find(data.conflict) != m_requires[*it2].end());
                        m_required_by[conflict_id].insert(*it2);
                        m_requires[*it2].erase(m_requires[*it2].find(data.conflict));
                        m_requires[*it2].insert(conflict_id);
                    }
                    reqby.clear();
                }
            }
            m_conflict_data[fid].push_back(ConflictData(conflict_id, threshold));
        }
#endif
}
//functions in STL
    return make_pair(false, conflict_id);
}
// used for initialize
void UCRefinementCritPaths::add_options_to_parser(OptionParser &parser) {
    UCRefinement::add_options_to_parser(parser);
    parser.add_option<bool>("caching", "", "true");
}
// used for initialize
static HeuristicRefiner *_parse(OptionParser &parser) {
    UCRefinementCritPaths::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (!parser.dry_run()) {
        opts.set<ConflictSelector*>("conflicts", new GreedyConflictSelector());
        return new UCRefinementCritPaths(opts);
    }
    return NULL;
}

static Plugin<HeuristicRefiner> _plugin("uccp", _parse);

