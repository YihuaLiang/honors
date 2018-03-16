
#include "pic_landmarks.h"
#include "../utilities.h"
#include "../globals.h"
#include "../conjunction_operations.h"
#include "../plugin.h"
#include "../operator_cost.h"

#include <iostream>

using namespace std;

void union_with(set<size_t> &x, const set<size_t> &y)
{
    x.insert(y.begin(), y.end());
}

void intersect_with(set<size_t> &x, const set<size_t> &y)
{
    set<size_t>::iterator it1 = x.begin(),
                          it2 = y.begin(),
                          it2End = y.end();
    while (it1 != x.end() && it2 != it2End) {
        if (*it1 == *it2) {
            it1++;
            it2++;
        } else if (*it1 < *it2) {
            x.erase(it1++);
        } else {
            it2++;
        }
    }
    while (it1 != x.end()) {
        x.erase(it1++);
    }
}

void set_minus(set<size_t> &x, const set<size_t> &y)
{
    set<size_t>::iterator it1 = x.begin(),
                          it2 = y.begin(),
                          it2End = y.end();
    while (it1 != x.end() && it2 != it2End) {
        if (*it1 == *it2) {
            x.erase(it1++);
            it2++;
        } else if (*it1 < *it2) {
            it1++;
        } else {
            it2++;
        }
    }
}

PiCLandmark::PiCLandmark(const Options &opts, ConjunctionMaxHeuristic &h)
    : LandmarkFactory(opts), h(h)
{
    if (!h.store_counter_precondition) {
        cout << "Counter preconditions must be stored in order derive landmarks!"
             << endl;
        exit_with(EXIT_CRITICAL_ERROR);
    }
}

void PiCLandmark::enqueue_relevant_counters(size_t conj_id, bool decr_pre,
        vector<size_t> &queue)
{
    const vector<int> &ops = h.conjunctions[conj_id].triggered_counters;
    for (size_t i = 0; i < ops.size(); i++) {
        if (decr_pre) {
            counter_preconditions[ops[i]].preconditions--;
        }
        if (counter_preconditions[ops[i]].preconditions == 0) {
            queue.push_back(ops[i]);
        }
    }
}

void PiCLandmark::build_landmarks()
{
    conjunction_levels.clear();
    counter_preconditions.clear();
    conjunction_levels.resize(h.conjunctions.size());
    counter_preconditions.resize(h.counters.size());

    vector<size_t> queue, queue_next;

    Fluent initial_state;
    const State &state = g_initial_state();
    for (size_t var = 0; var < g_variable_domain.size(); var++) {
        initial_state.insert(make_pair(var, state[var]));
    }
    for (size_t i = 0; i < h.counters.size(); i++) {
        counter_preconditions[i].preconditions = h.counters[i].preconditions;
        counter_preconditions[i].effect = h.counters[i].effect;
        counter_preconditions[i].op_id =
            h.actions[h.counters[i].action].operator_no;
        if (counter_preconditions[i].preconditions == 0) {
            queue.push_back(i);
        }
    }
    for (size_t i = 0; i < h._fluents.size(); i++) {
        if (fluent_op::is_subset(h._fluents[i], initial_state)) {
            conjunction_levels[i].level = 0;
            enqueue_relevant_counters(i, true, queue);
        }
    }

    int level = 1;
    set<size_t> local_landmarks, local_necessary;
    while (!queue.empty()) {
        for (size_t _i = 0; _i < queue.size(); _i++) {
            int counter_id = queue[_i];
            LMOp &op = counter_preconditions[counter_id];
            if (op.handled == level) {
                continue;
            }
            op.handled = level;

            for (size_t pre = 0; pre < h.counter_preconditions[counter_id].size();
                 pre++) {
                int pre_id = h.counter_preconditions[counter_id][pre];
                union_with(local_landmarks,
                           conjunction_levels[pre_id].landmark);
                local_landmarks.insert(pre_id);
                if (lm_graph->use_orders()) {
                    local_necessary.insert(pre_id);
                }
            }

            LMEntry &effect = conjunction_levels[op.effect];
            if (effect.level == -1) {
                effect.level = level;
                effect.landmark = local_landmarks;
                if (lm_graph->use_orders()) {
                    effect.necessary = local_necessary;
                }
                //effect.first_achievers.insert(counter_id);
                effect.first_achievers.insert(op.op_id);
                enqueue_relevant_counters(op.effect, true, queue_next);
            } else {
                size_t num_landmarks = effect.landmark.size();
                intersect_with(effect.landmark, local_landmarks);
                if (local_landmarks.find(op.effect) == local_landmarks.end()) {
                    //effect.first_achievers.insert(counter_id);
                    effect.first_achievers.insert(op.op_id);
                    if (lm_graph->use_orders()) {
                        intersect_with(effect.necessary, local_necessary);
                    }
                }
                if (effect.landmark.size() != num_landmarks) {
                    enqueue_relevant_counters(op.effect, false, queue_next);
                }
            }
            if (op.effect == 31) {
                cout << level << ": " << counter_id << endl;
                cout << "Counter landmarks: ";
                vector<int> xxxxx;
                xxxxx.insert(xxxxx.end(), local_landmarks.begin(),
                       local_landmarks.end());
                h.dump_fluent_set_pddl(xxxxx);
                cout << endl; 
                xxxxx.clear();
                cout << "Counter necessary: ";
                xxxxx.insert(xxxxx.end(), local_necessary.begin(),
                       local_necessary.end());
                h.dump_fluent_set_pddl(xxxxx);
                cout << endl; 
                xxxxx.clear();
                cout << "Updated landmarks(31): ";
                xxxxx.insert(xxxxx.end(), effect.necessary.begin(),
                       effect.necessary.end());
                h.dump_fluent_set_pddl(xxxxx);
                cout << endl; 
            }

            local_landmarks.clear();
            local_necessary.clear();
        }
        queue.swap(queue_next);
        queue_next.clear();

        cout << "Level " << level << " completed." << endl;
        level++;
    }

    cout << "_______DEBUGGING" << endl;
    for (size_t i = 0; i < counter_preconditions.size(); i++) {
        if (counter_preconditions[i].preconditions > 0 && 
                counter_preconditions[i].effect == 31) {
            h.dump_counter(i);
        }
    }

    //cout << "Total levels until convergence: " << level << endl;
}

void PiCLandmark::add_lm_node(size_t conj_id)
{
    if (lm_nodes[conj_id] == NULL) {
        LandmarkNode *node = NULL;
        if (conj_id < h.conjunction_offset) {
            node = &lm_graph->landmark_add_simple(*h._fluents[conj_id].begin());
        } else {
            node = &lm_graph->landmark_add_conjunctive(h._fluents[conj_id]);
        }
        node->in_goal = h.conjunctions[conj_id].is_goal;
        node->first_achievers.insert(
            conjunction_levels[conj_id].first_achievers.begin(),
            conjunction_levels[conj_id].first_achievers.end());
        lm_nodes[conj_id] = node;
    }
}

void PiCLandmark::build_landmark_graph()
{
    cout << "Collecting all landmarks ..." << endl;
    build_landmarks();

    cout << "Selecting goal relevant landmarks ..." << endl;
    set<size_t> lm_merged;
    for (size_t i = 0; i < h.goal_conjunctions.size(); i++) {
        size_t conj_id = h.goal_conjunctions[i];
        union_with(lm_merged, conjunction_levels[conj_id].landmark);
        lm_merged.insert(conj_id);
    }

    lm_nodes.resize(h.conjunctions.size(), NULL);
    // generate nodes
    cout << "Generating nodes of landmark graph ..." << endl;
    for (set<size_t>::iterator it = lm_merged.begin(); it != lm_merged.end();
         it++) {
        add_lm_node(*it);
    }

    if (lm_graph->use_orders()) {
        //cout << "Removing transitive relations ..." << endl;
        //set<size_t> remove_lm;
        //for (set<size_t>::iterator it = lm_merged.begin(); it != lm_merged.end();
        //     it++) {
        //    set<size_t> &landmarks = conjunction_levels[*it].landmark;
        //    for (set<size_t>::iterator it2 = landmarks.begin();
        //         it2 != landmarks.end(); it2++) {
        //        const set<size_t> &landmarks2 = conjunction_levels[*it2].landmark;
        //        remove_lm.insert(landmarks2.begin(), landmarks2.end());
        //    }
        //    set_minus(landmarks, remove_lm);
        //    set_minus(landmarks, conjunction_levels[*it].necessary);
        //    remove_lm.clear();
        //}

        // generate edges
        cout << "Generating edges in landmark graph ..." << endl;
        for (set<size_t>::iterator it = lm_merged.begin(); it != lm_merged.end();
             it++) {

            const set<size_t> &landmarks = conjunction_levels[*it].landmark;
            for (set<size_t>::iterator it2 = landmarks.begin();
                 it2 != landmarks.end(); it2++) {
                edge_add(*lm_nodes[*it2], *lm_nodes[*it], natural);
            }
            const set<size_t> &necessary = conjunction_levels[*it].necessary;
            for (set<size_t>::iterator it2 = necessary.begin();
                 it2 != necessary.end(); it2++) {
                edge_add(*lm_nodes[*it2], *lm_nodes[*it], greedy_necessary);
            }

        }
    }

    cout << "Cleaning up ..." << endl;
    lm_merged.clear();
    call_janitor();
}

void PiCLandmark::call_janitor()
{
    //h.cleanup_everything();
    conjunction_levels.clear();
    counter_preconditions.clear();
    lm_nodes.clear();
}

void PiCLandmark::generate_landmarks()
{
    build_landmark_graph();
}

void PiCLandmark::calc_achievers()
{
    for (set<LandmarkNode *>::iterator it_node = lm_graph->get_nodes().begin();
         it_node != lm_graph->get_nodes().end(); it_node++) {
        LandmarkNode &node = **it_node;
        set<size_t> candidates;
        Fluent lm;
        for (size_t i = 0; i < node.vars.size(); i++) {
            Fact f = make_pair(node.vars[i], node.vals[i]);
            const vector<int> &ops = lm_graph->get_operators_including_eff(f);
            lm.insert(f);
            candidates.insert(ops.begin(), ops.end());
        }
        for (set<size_t>::iterator it_op = candidates.begin();
             it_op != candidates.end(); it_op++) {
            const Fluent &add = h.actions[*it_op].add_effect;
            const Fluent &pre = h.actions[*it_op].add_effect;
            const Fluent &del = h.actions[*it_op].del_effect;
            if (fluent_op::are_disjoint(lm, del)) {
                Fluent pre_ac;
                pre_ac.insert(pre.begin(), pre.end());
                fluent_op::set_minus(lm, add, pre_ac);
                if (!h.contains_mutex(pre_ac)) {
                    node.possible_achievers.insert(*it_op);
                }
            }
        }
    }
}

void PiCLandmark::add_options_to_parser(OptionParser &parser)
{
    LandmarkGraph::add_options_to_parser(parser);
    parser.add_option<Heuristic *>("pic", "Conjunction container");
}

static LandmarkGraph *_parse(OptionParser &parser)
{
    PiCLandmark::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (parser.help_mode()) {
        return 0;
    }

    opts.set("explor", new Exploration(opts));

    parser.document_language_support("conditional_effects",
                                     "ignored, i.e. not supported");
    opts.set<bool>("supports_conditional_effects", false);

    if (parser.dry_run()) {
        return 0;
    } else {
        ConjunctionMaxHeuristic *pic_h =
            dynamic_cast<ConjunctionMaxHeuristic *>(opts.get<Heuristic *>("pic"));
        if (pic_h == NULL) {
            exit_with(EXIT_CRITICAL_ERROR);
        }
        pic_h->set_store_preconditions(true);
        pic_h->evaluate(g_initial_state());
        PiCLandmark lm_graph_factory(opts, *pic_h);
        LandmarkGraph *graph = lm_graph_factory.compute_lm_graph();
        return graph;
    }
}

static Plugin<LandmarkGraph> _plugin("lm_pic", _parse);


