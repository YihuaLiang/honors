
#include "uc_refinement_critical_paths.h"

#include "hc_heuristic.h"
#include "conjunction_operations.h"

#include "../globals.h"
#include "../state.h"
#include "../option_parser.h"
#include "../plugin.h"

#include "../operator.h"
#include "../utilities.h"

#include <string>

//int START = 0;

struct GreedyConflictSelector : public ConflictSelector {
    virtual unsigned operator()(UCRefinementCritPaths *r, const Fluent &subgoal,
                                int threshold) {
        UCHeuristic *pic = dynamic_cast<UCHeuristic*>(r->get_heuristic());
        //get a heuristic
        int best_size = 0;
        int res = -1;
        std::vector<int> in_fluent;
        in_fluent.resize(pic->num_conjunctions(), 0);
        for (Fluent::iterator f = subgoal.begin(); f != subgoal.end(); f++) {
            const vector<unsigned> &f_conjs =
                pic->get_fact_conj_relation(pic->get_fact_id(*f));
            for (uint i = 0; i < f_conjs.size(); i++) {
                unsigned cid = f_conjs[i];
                const Conjunction &conj = pic->get_conjunction(cid);
                //go through the conjunction -- the first one equal to size
                if (++in_fluent[cid] == conj.fluent_size &&
                    (!conj.is_achieved() ||
                     conj.cost >= threshold)) {
                    if (res == -1 || conj.fluent_size < best_size) {
                        res = cid;
                        best_size = conj.fluent_size;
                        //control the fluent size
                    }
                }
            }
        }
        in_fluent.clear();
        return res;
    }
};

UCRefinementCritPaths::UCRefinementCritPaths(const Options &opts) :
    UCRefinement(opts),
    c_use_caching(opts.get<bool>("caching")),
    m_selector(opts.get<ConflictSelector*>("conflicts"))
{}

void UCRefinementCritPaths::statistics()
{
    UCRefinement::statistics();
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
    m_achievers.resize(uc->num_conjunctions());
    for (unsigned i = 0; i < uc->num_counters(); i++) {
        const ActionEffectCounter &counter = uc->get_counter(i);
        if (counter.unsatisfied_preconditions == 0
                && counter.cost + counter.base_cost == counter.effect->cost) {
            m_achievers[counter.effect->id].push_back(counter.action_id);//find the true achiever
        }
    }
    if (c_use_caching) {
        m_conflict_data.resize(uc->num_facts());
    }
    //cout<<"prepare succeed"<<endl;
#ifndef NDEBUG
    for (unsigned i = 0; i < uc->num_conjunctions(); i++) {
        const Conjunction &conj = uc->get_conjunction(i);
        assert(!conj.is_achieved() || conj.cost == 0 || m_achievers[conj.id].size() > 0);
    }
#endif
}
//called by learn-ur-dd -> refine
//evaluate before actions are prepared
HeuristicRefiner::RefinementResult UCRefinementCritPaths::refine(
    const State &state)
{
    //std::cout << ">>>>>>>>>>>REFINEMENT_START" << std::endl;
#ifndef NDEBUG
    for (unsigned i = 0; i < uc->num_counters(); i++) {
        assert(uc->get_counter(i).base_cost >= 1);
    }//here we do not require base cost  > 1
#endif
    //cout<<endl<<"PCR called"<<endl;
    bool updated_c = false;
    RefinementResult successful = SUCCESSFUL;
    int old_val = -2;
    Fluent goal;
    for (uint i = 0; i < g_goal.size(); i++) {
        goal.insert(g_goal[i]);
    }
    while (successful != FAILED) {//loop == the recursive call
        bool x = uc->set_early_termination(false);
        uc->evaluate(state);//
        uc->set_early_termination(x);
#ifndef NDEBUG
        std::cout << "{val = " << uc->get_value() << "}" << std::endl;
#endif
        //std::cout << "val = " << uc->get_value() << std::endl;
        if (uc->is_dead_end()) {
            //std::cout << ">>>>>>>>>>>REFINEMENT_END" << std::endl;
            return updated_c ? SUCCESSFUL : UNCHANGED;
        }
        if (uc->get_value() == old_val) {//value is not updated --> there is a problem
            assert(false);
            break;
        }
        old_val = uc->get_value();
        updated_c = true;
        prepare_refinement();
        //std::cout << "computing conflicts..." << std::flush;
        //START = uc->get_value();
        std::pair<bool, unsigned> res = compute_conflict(goal, uc->get_value(), state);
        //std::cout << "done => " << m_conflicts.size() << std::endl;
        if (res.second == (unsigned) -1) {
            //std::cout << "Found plan in uC refinement!" << std::endl;
            successful = SOLVED;
            break;
        } else {
            if (c_use_caching) {
                m_required_by.clear();
                unsigned i = 0;
                std::vector<unsigned> open;
                open.push_back(res.second);
                m_pruned[res.second] = true;
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
            }
            else if (!uc->update_c(m_conflicts)) {
                successful = FAILED;//update the conflicts
            }
        }
#ifndef NDEBUG
        uc->dump_compilation_information(std::cout);
#endif
        //here the conflict is released 
        release_memory();
    }
    //assert(false);
    //std::cout << ">>>>>>>>>>>REFINEMENT_END" << std::endl;
    return successful;
}
//reload
HeuristicRefiner::RefinementResult UCRefinementCritPaths::refine(
    const State &state, int g_value)
{
    //std::cout << ">>>>>>>>>>>REFINEMENT_START" << std::endl;
#ifndef NDEBUG
    for (unsigned i = 0; i < uc->num_counters(); i++) {
        assert(uc->get_counter(i).base_cost >= 1);
    }//here we do not require base cost  > 1
#endif
    cout<<"come into pcr "<<endl;
    // for (Fluent::iterator f = _fluents.begin(); f != _fluents.end(); f++) {
    //     if (state[f->first] == f->second) {
    //       cout<<"Instate "<<g_fact_names[f->first][f->second]<<endl;
    //     }
    // }
    bool updated_c = false;
    RefinementResult successful = SUCCESSFUL;
    int old_val = -2;
    Fluent goal;
    for (uint i = 0; i < g_goal.size(); i++) {
        goal.insert(g_goal[i]);
    }
    //fail to quit?
    while (successful != FAILED) {//loop == the recursive call
        bool x = uc->set_early_termination(false);
        uc->evaluate(state, g_value);
        uc->set_early_termination(x);
#ifndef NDEBUG
        std::cout << "{val = " << uc->get_value() << "}" << std::endl;
#endif
        std::cout << "val = " << uc->get_value() <<std::endl;
        if (uc->is_dead_end()) {
            //std::cout << ">>>>>>>>>>>REFINEMENT_END" << std::endl;
            return updated_c ? SUCCESSFUL : UNCHANGED;
        }
        if (uc->get_value() == old_val) {//value is not updated
            cout<<"old val is "<<old_val<<endl;
            assert(false);
            break;
        }
        old_val = uc->get_value();
        updated_c = true;
        prepare_refinement();;
        //START = uc->get_value();
        std::pair<bool, unsigned> res = compute_conflict(goal, uc->get_value(), state);
        if (res.second == (unsigned) -1) {
            successful = SOLVED;
            break;
        } else {
            if (c_use_caching) {
                m_required_by.clear();
                unsigned i = 0;
                std::vector<unsigned> open;
                open.push_back(res.second);
                //for the conflicts, go through those conjunctions require it 
                m_pruned[res.second] = true;
                //cout<<"open size "<<open.size()<<endl;
                while (i < open.size()) {
                    //here is a bug //it is because the open[i] is larger than m_requires size --> the size problem is not guaranteed
                    //requires contains the child confl, required_by contains parents confl. a vector of unsigned set
                    for (std::set<unsigned>::iterator it = m_requires[open[i]].begin(); it != m_requires[open[i]].end(); it++) {
                        //*it is a conj requires the open[i]
                        if (!m_pruned[*it]) {//this line, *it value is not what expected(too large)
                            m_pruned[*it] = true;//set the child_conflict to true and push it into open
                            open.push_back(*it);//do this recursively             
                        }
                    }
                    i++;
                }               
                //conflict size equal to m_pruned
                for (uint i = 0; i < m_conflicts.size(); i++) {
                    if (m_pruned[i]) {
                        if (uc->exceeded_size_limit()) {
                            successful = FAILED;
                            break;
                        } else {
                            uc->add_conflict(m_conflicts[i]);
                            Fluent new_conflict = m_conflicts[i].get_fluent();
                            cout<<"Fluent here, id is"<<i<<endl;
                            for(std::set<Fact>::iterator it = new_conflict.begin(); it!=new_conflict.end(); ++it){
                                cout<<g_fact_names[(*it).first][(*it).second]<<endl;
                            }
                        }
                    }
                }
            }
            else if (!uc->update_c(m_conflicts)) {
                //cout<<"bug in update c"<<endl;
                successful = FAILED;//update the conflicts
            }
        }
#ifndef NDEBUG
        uc->dump_compilation_information(std::cout);
#endif
        //std::cout << "successful = " << successful << std::endl;
        //exit(1);
        release_memory();//every time it get a new X it will clean the m
        //cout<<"pcr next round"<<endl;
    }
    //if(successful == SOLVED){ cout<<"SOLVE"<<endl;}
    return successful;
}
//it will return true and id when success
std::pair<bool, unsigned> UCRefinementCritPaths::compute_conflict(
    const Fluent &subgoal, int threshold, const State &state)
{
    //ConflictData *confl_data = NULL;
    if (c_use_caching) {
#if 0
        confl_data = &m_duplicate_checking[subgoal];
        if (confl_data->threshold  >= threshold) {
            return make_pair(false, confl_data->conflict);
        }
#else
        std::vector<unsigned> subset(m_conflicts.size(), 0);
        for (Fluent::const_iterator it = subgoal.begin(); it != subgoal.end(); it++) {
            unsigned fid = uc->get_fact_id(*it);
            const std::vector<ConflictData> &confls = m_conflict_data[fid];
            for (const ConflictData &data : confls) {//go through the conflict facts
                if (data.threshold >= threshold &&
                        ++subset[data.conflict] == m_conflicts[data.conflict].get_fluent().size()) {
                    return std::make_pair(false, data.conflict);
                }
            }
        }
#endif
    }
    unsigned conflict_id = m_conflicts.size();
    m_requires.resize(m_requires.size() + 1);
    m_required_by.resize(m_required_by.size() + 1);
    m_conflicts.resize(m_conflicts.size() + 1);
    m_pruned.push_back(false);
    
    if (!uc->extract_mutex(subgoal, m_conflicts[conflict_id])) {
        //cid is id of the conj selected by greedy strategy
        unsigned cid = (*m_selector)(this, subgoal, threshold);
        if (cid == ConflictSelector::INVALID) {
            //cout<<"invalid conflict"<<endl;
            return make_pair(false, -1);
        }
        if (!uc->get_conjunction(cid).is_achieved() ||
            uc->get_conjunction(cid).cost > threshold) {//it has been calculated 
            return make_pair(true, cid);//mark the return true to refine
        }
        m_conflicts[conflict_id].merge(uc->get_fluent(cid));

        const vector<unsigned> &candidates = m_achievers[cid];
        for (size_t i = 0; i < candidates.size(); i++) {
            const StripsAction &action = uc->get_action(candidates[i]);//go through all action
            Fact f_del;
            if (fluent_op::intersection_not_empty(subgoal, action.del_effect, f_del)) {
                m_conflicts[conflict_id].merge(f_del);
                continue;//check every action --> intersect set not empty then ad the f_del to the conflict
            }

            //add this for pruning duplicated executing of same action
            else if(m_zero_achievers.count(action.aid)){
                //the zero cost action has been used once -- do not take it again
                continue;
            }
            else if(action.base_cost == 0){
                m_zero_achievers.insert(action.aid);
            }
            
            //    std::string(START - threshold + 1, ' ') << g_operators[action.operator_no].get_name() << std::endl;
            Fluent regr;
            regr.insert(action.precondition.begin(), action.precondition.end());
            fluent_op::set_minus(subgoal, action.add_effect, regr);//get the regr
            //
            //assert(action.base_cost > 0);//if base cost = 0 then don't do it?????
            //reload
            //assert(action.base_cost > 0);
            pair<bool, unsigned> child_confl =//recurssive call
                //it should be base_cost, no modification needed
                compute_conflict(regr, threshold - action.base_cost, state);
            regr.clear();//regr means the sub goal

            if (child_confl.second == (unsigned) -1) {//-1 means the result is invalid
                //_plan.push_back(action.operator_no);
                m_plan.push_back(&g_operators[action.operator_no]);
                return child_confl;//subset or invalid
            }
            if (child_confl.first) {//true --- larger than n or not is_achieved
                fluent_op::set_minus(uc->get_fluent(child_confl.second),
                                     action.precondition, regr);//delete similar function
            } else {//the last return case --false, cid
                assert(child_confl.second < m_conflicts.size());//learned cid should be smaller than conflict size?
                m_requires[conflict_id].insert(child_confl.second);//random in
                m_required_by[child_confl.second].insert(conflict_id);
                fluent_op::set_minus(m_conflicts[child_confl.second].get_fluent(),
                                     action.precondition, regr);
            }
            //child confl return true in can't achieved -> merge into regr
            m_conflicts[conflict_id].merge(regr);
            regr.clear();
        }

        bool is_subset = true;
        for (Fluent::iterator it = m_conflicts[conflict_id].get_fluent().begin();
                is_subset && it != m_conflicts[conflict_id].get_fluent().end(); it++) {
            if (state[it->first] != it->second) { //state do not contains the fact in conflict
                is_subset = false;
            }
        }
        if (is_subset) {//conflict contained in state---for the case threshold = 0
            for (Fluent::iterator it = subgoal.begin(); it != subgoal.end(); it++) {
                if (state[it->first] != it->second) { //state do not contains facts in subgoal
                    is_subset = false;
                    m_conflicts[conflict_id].merge(*it);
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
    return make_pair(false, conflict_id);
}

void UCRefinementCritPaths::add_options_to_parser(OptionParser &parser) {
    UCRefinement::add_options_to_parser(parser);
    parser.add_option<bool>("caching", "", "true");
}

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

