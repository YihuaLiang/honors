
#include "adr_heuristic_refiner.h"

#include "../option_parser.h"
#include "../state.h"
#include "../plugin.h"
#include "../operator_cost.h"
#include "../utilities.h"
#include "../globals.h"

#include "../rng.h"
#include "../successor_generator.h"

ADRRefinement::ADRRefinement(const Options &opts)
    : HeuristicRefiner(opts), Heuristic(opts),
      c_eval_hc(opts.get<bool>("eval")),
      c_reeval_hc(opts.get<bool>("reeval")),
      c_offline_refinement_threshold(opts.get<float>("offline_x")),
      nogoods(NULL)
{
    cost_type = OperatorCost::ONE;
    if (opts.get<bool>("nogoods")) {
        nogoods = new NoGoodsSingleVal(true);
    }
    pic = dynamic_cast<AugmentedDeleteRelaxation*>(opts.get<Heuristic*>("pic"));
    if (!pic) {
        std::cerr << "Provided heuristic is not of type AugmentedDeleteRelaxation!" << std::endl;
        exit_with(EXIT_CRITICAL_ERROR);
    }
}

bool ADRRefinement::check_nogoods(const State &state)
{
    _stats.start_check_nogood();
    bool res = nogoods && nogoods->match(state) != NoGoods::NONOGOOD;
    _stats.end_check_nogood();
    return res;
}

bool ADRRefinement::project_var(const State &state, const std::set<int> &vars, int var, int orig_val)
{
    //cout << "Projecting away " << g_fact_names[var][orig_val] << endl;

    vector<GraphNode*> exploration;

    // Add all values of the given variable to the current state
    for (int val = 0; val < g_variable_domain[var]; val++) {
        if (val == orig_val) {
            continue;
        }

        GraphNode *node = pic->nodes()[var][val];
        if (node->cost == -1) {
            node->cost = 0;
            exploration.push_back(node);
            pic->queue.push(0, node);
        }
    }

    std::vector<GraphNode *> &pi_fluents = pic->nodes().back();
    for (uint i = 0; i < pi_fluents.size(); i++) {
        if (pi_fluents[i]->cost != -1) continue;
        FluentSet &fs = pi_fluents[i]->values;
        uint j;
        for (j = 0; j < fs.size(); j++) {
            if (state[fs[j].first] != fs[j].second && fs[j].first != var && vars.find(fs[j].first) == vars.end()) {
                break;
            }
        }
        if (j == fs.size()) {
            exploration.push_back(pi_fluents[i]);
            pi_fluents[i]->cost = 0;
            pic->queue.push(0, pi_fluents[i]);
        }
    }

    // check whether the state is still a dead end
    pic->hmax_relaxed_exploration_get_reachable(exploration);
    bool is_dead = pic->end_operator()->unsatisfied_preconditions > 0;
    if (!is_dead) {
        pic->dummy_goal()->cost = -1;
        pic->dummy_goal()->status = LMCUTPropStatus::UNREACHED;
        pic->dummy_goal()->supporter = (ADROperator *)(-1);
        pic->dummy_goal()->plan.clear();
        pic->dummy_goal()->plan_computed = false;

        pic->dummy_pc()->cost = -1;
        pic->dummy_pc()->status = LMCUTPropStatus::UNREACHED;
        pic->dummy_pc()->supporter = (ADROperator *)(-1);
        pic->dummy_pc()->plan.clear();
        pic->dummy_pc()->plan_computed = false;

        for (GraphNode *node : exploration) {
            node->cost = -1;
            node->status = LMCUTPropStatus::UNREACHED;
            node->supporter = (ADROperator *)(-1);
            node->plan.clear();
            node->plan_computed = false;

            for (ADROperator *op : node->prec_of) {
                op->unsatisfied_preconditions++;
                op->hmax_justification = NULL;
                op->hmax_justification_cost = -1;
                op->plan_computed = false;
            }
        }
    }
    exploration.clear();
    return is_dead;
}

bool ADRRefinement::learn_nogoods(const State &state)
{
    bool added = false;
    vector<pair<int, int> > nogood;
    std::set<int> projected;
    for (int var = g_variable_domain.size() - 1; var >= 0; var--) {
        if (!project_var(state, projected, var, state[var])) {
            nogood.push_back(make_pair(var, state[var]));
        } else {
            projected.insert(var);
        }
    }
    added = nogoods->store(nogood);
    return added;
}

bool ADRRefinement::learn_nogoods_wrapper(const State &state)
{
    if (!nogoods) {
        return false;
    }
    // must first initialize the hc data structures
    if (!pic->compute_dead_end(state)) {
        cout << "Trying to learn a nogood on a non dead end state!" << endl;
        return false;
    }
    // try to extract a nogood
    return learn_nogoods(state);
}

void ADRRefinement::initialize()
{
    pic->initialize();
}

int ADRRefinement::compute_heuristic(const State &state)
{
    // if nogood learning is turned on, check whether there is a matching
    // nogood. if so, we do not have to evaluate hc
    if (check_nogoods(state)) {
        return DEAD_END;
    }

    // if lazy evaluation is turned on, we skip the evaluation of hc
    if (!c_eval_hc) {
        _stats.start_check_hc();
        if (pic->compute_dead_end(state)) {
            _stats.end_check_hc(true);
            if (nogoods) {
                _stats.start_learning_nogood();
                learn_nogoods(state);
                _stats.end_learning_nogood(nogoods);
            }
            return DEAD_END;
        }
        _stats.end_check_hc(false);
    }

    return 0;
}

void ADRRefinement::reevaluate(const State &state)
{
    if (heuristic == NOT_INITIALIZED) {
        initialize();
    }
    heuristic = 0;
    if (c_reeval_hc) {
        evaluate(state);
    } else if (check_nogoods(state)) {
        heuristic = DEAD_END;
        evaluator_value = DEAD_END;
    }
}

HeuristicRefiner::RefinementResult ADRRefinement::learn_unrecognized_dead_end(const State &root)
{
    if (check_nogoods(root)) {
        return UNCHANGED;
    }
    _stats.start_learning_c();
    std::streambuf* old = cout.rdbuf(NULL);
    int status = pic->do_learning(root);
    cout.rdbuf(old);
    _stats.end_learning_c(status == 1);
    if (status == 1) {
        learn_nogoods_wrapper(root);
        return SUCCESSFUL;
    } else if (status == 2) {
      pic->get_relaxed_plan().sequentialize_plan(m_plan);
      return SOLVED;
    }
    return FAILED;
}

HeuristicRefiner::RefinementResult ADRRefinement::learn_unrecognized_dead_ends(
        const std::vector<State> &root_component,
        const std::unordered_set<StateID> &)
{
    RefinementResult res = learn_unrecognized_dead_end(root_component.front());
    if (res != SOLVED && res != FAILED) {
        learn_recognized_dead_ends(root_component);
    }
    return res;
}

void ADRRefinement::learn_recognized_dead_end(const State &dead_end)
{
    if (!nogoods || check_nogoods(dead_end)) {
        return;
    }
    learn_nogoods_wrapper(dead_end);
}

void ADRRefinement::learn_recognized_dead_ends(const std::vector<State> &states)
{
    if (!nogoods) {
        return;
    }
    for (size_t i = 0; i < states.size(); i++) {
        if (!check_nogoods(states[i])) {
            learn_nogoods_wrapper(states[i]);
        }
    }
}

void ADRRefinement::refine_offline()
{
  if (c_offline_refinement_threshold <= 1) {
    return;
  }
  float oldx = pic->problem_size_bound();
  if (c_offline_refinement_threshold < oldx) {
    pic->set_problem_size_bound(c_offline_refinement_threshold);
  }
  pic->do_learning(g_initial_state());
  pic->set_problem_size_bound(oldx);
}

void ADRRefinement::add_options_to_parser(OptionParser &parser)
{
    //AugmentedDeleteRelaxation::add_options_to_parser(parser);
    Heuristic::add_options_to_parser(parser);
    parser.add_option<bool>("nogoods", "", "true");
    parser.add_option<bool>("eval", "", "true");
    parser.add_option<bool>("reeval", "", "false");
    parser.add_option<float>("offline_x", "", "0");
    parser.add_option<Heuristic *>("pic", "", "adr(learn_on_i=false, x=999999)");
}

static HeuristicRefiner *_parse(OptionParser &parser)
{
    ADRRefinement::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (!parser.dry_run()) {
        return new ADRRefinement(opts);
    }
    return NULL;
}

static Plugin<HeuristicRefiner> _plugin("uadr", _parse);


