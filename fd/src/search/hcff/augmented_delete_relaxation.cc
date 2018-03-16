#include "augmented_delete_relaxation.h"

#include "../plugin.h"

/*****************************************************************
 *
 * Non-member helper functions
 *
 ****************************************************************/

bool adr_effect_edeletes_gn(const ADROperator *adr_op, const GraphNode *gn)
{
  // contexts are true before action with cond eff is applied and not
  // changed by action, so if mutex with those edeletes

  if (are_mutex(gn->values, adr_op->context)) {
    return true;
  }

  for (int i = 0; i < gn->values.size(); i++) {
    if (e_deletes(*(adr_op->op), gn->values[i])) {
      return true;
    }
  }
  return false;
}


bool grp_node_edeletes_gn(const GRPNode &node, const GraphNode *gn)
{
  // contexts are true before action with cond eff is applied and not
  // changed by action, so if mutex with those edeletes

  ADROperator *adr_op = NULL;

  for (int i = 0; i < node.effects.size(); i++) {
    adr_op = node.effects[i];
    if (are_mutex(gn->values, adr_op->context)) {
      return true;
    }
  }

  // all effects come from same op, so no problem using last one for
  // edeletes call below

  for (int i = 0; i < gn->values.size(); i++) {
    if (e_deletes(*(adr_op->op), gn->values[i])) {
      return true;
    }
  }
  return false;
}

bool op_deletes_gn(const Operator &op, const GraphNode *gn)
{
  for (int i = 0; i < gn->values.size(); i++) {
    if (deletes(op, gn->values[i])) {
      return true;
    }
  }
  return false;
}

void
indent(int x)
{
  for (int i = 0; i < x; i++) {
    std::cout << "  ";
  }
}

std::ostream &operator<<(std::ostream &os, const GraphNode &gn)
{
  return os << gn.values;
}

std::ostream &operator<<(std::ostream &os, const ADROperator &adr_op)
{
  os << adr_op.name();
  if (adr_op.parent_op) {
    for (int ce_index = 0; ce_index < adr_op.parent_op->condeffs.size();
         ce_index++) {
      if (adr_op.parent_op->condeffs[ce_index] == &adr_op) {
        os << "-ce" << ce_index;
        break;
      }
    }
    os << " (context: " << adr_op.context << ")";
  }
  return os;
}

std::string get_sup_string(const ADROperator *adr_op)
{

  std::ostringstream oss;
  if (adr_op) {
    oss << *adr_op;
  } else {
    oss << "Current state";
  }
  return oss.str();
}

std::ostream &operator<<(std::ostream &os, const GRPNode &node)
{
  for (int i = 0; i < node.effects.size(); i++) {
    os << *(node.effects[i]);
    if (i != node.effects.size() - 1) {
      os << ",";
    }
  }
  return os;
}

/*****************************************************************
 *
 * static initializers
 *
 ****************************************************************/

unsigned AugmentedDeleteRelaxation::graph_node_index = 0;
bool ADROperator::m_remove_dominated_preconditions = true;

/*****************************************************************
 *
 * ADROperator class
 *
 ****************************************************************/

void ADROperator::add_prec(GraphNode *gn)
{
  bool dominated = false;
  for (std::vector<GraphNode *>::iterator it = prec.begin();
       it != prec.end();) {
    if (has_subset((*it)->values, gn->values)) {
      dominated = true;
      break;
    } else if (remove_dominated_preconditions()
               && has_subset(gn->values, (*it)->values)) {
      (*it)->prec_of.erase(std::find((*it)->prec_of.begin(), (*it)->prec_of.end(),
                                     this));
      it = prec.erase(it);
    } else {
      it++;
    }
  }
  if (!dominated || (!remove_dominated_preconditions() &&
                     (std::find(prec.begin(), prec.end(), gn) == prec.end()))) {
    prec.push_back(gn);
    gn->prec_of.push_back(this);
  }
}

/*****************************************************************
 *
 * AugmentedDeleteRelaxation class
 *
 ****************************************************************/

AugmentedDeleteRelaxation::AugmentedDeleteRelaxation(const Options &opts)
  : Heuristic(opts)
{
  learn_on_i = opts.get<bool>("learn_on_i");

  m_max_m = opts.get<int>("m");
  m_max_num_pi_fluents = opts.get<int>("n");
  m_problem_size_bound = opts.get<float>("x");
  m_admissible = opts.get<bool>("admissible");
  m__fast = opts.get<bool>("_fast");
  m_conditional_effect_merging = opts.get<bool>("conditional_effect_merging");
  m_learning_time_bound = opts.get<int>("learning_time");
  m_no_search = opts.get<bool>("no_search");
  m_use_conditional_effects = opts.get<bool>("use_conditional_effects");
  m_read_from_file = opts.get<bool>("read_from_file");
  m_write_to_file = false;
  if (opts.contains("write_to_file")) {
    m_write_to_file = !m_read_from_file && opts.get<bool>("write_to_file");
  }
  m_use_hmax_best_supporter = opts.get<bool>("use_hmax_best_supporter");
  m_tie_breaking = TieBreaking(opts.get_enum("tie_breaking"));

  // workaround for citycar: if the domain is detected, disable no_search option
  //if (g_operators.front().get_name() ==
  //    "build_diagonal_oneway junction0-0 junction1-1 road0") {
  //    cout << "Citycar domain detected, setting no_search option to false!" << endl;
  //    m_no_search = false;
  //}

  int seed = opts.get<int>("seed");
  if (seed == -1) {
    m_rng = RandomNumberGenerator();
  } else {
    m_rng = RandomNumberGenerator(seed);
  }

  finished_initialization = false;

  std::cout << "x = " << problem_size_bound()
            << ", merging = " << conditional_effect_merging()
            << ", time limit = " << learning_time_bound()
            << ", no search = " << no_search()
            << ", tie breaking = ";
  switch (tie_breaking()) {
  case TieBreaking::ARBITRARY:
    std::cout << "ARBITRARY";
    break;
  case TieBreaking::DIFFICULTY:
    assert(cost_type == OperatorCost::ONE
           && "tie breaking does not work with action costs");
    std::cout << "DIFFICULTY";
    break;
  case TieBreaking::RANDOM:
    assert(cost_type == OperatorCost::ONE
           && "tie breaking does not work with action costs");
    std::cout << "RANDOM";
    break;
  }
  std::cout << std::endl;


  //_passes_c = 0;
  //_passes_x = 0;
  //_passes_x_add = 0;
  //_computations = 0;
}

void
AugmentedDeleteRelaxation::initialize(bool call_do_learning)
{

  cout << "Initializing ADR heuristic..." << endl;

  m_perfect = false;

  // m-fluents are stored in last entry
  nodes().resize(g_variable_domain.size() + 1);

  // m-fluents column stays at size 0 for now
  for (int i = 0; i < g_variable_domain.size(); i++) {
    nodes()[i].resize(g_variable_domain[i]);
    for (int j = 0; j < g_variable_domain[i]; j++) {
      nodes()[i][j] = new GraphNode(graph_node_index++);
      nodes()[i][j]->values.push_back(std::make_pair(i, j));
    }
  }

  // add a dummy goal
  m_dummy_goal = new GraphNode(graph_node_index++);

  // add a dummy precondition
  m_dummy_pc = new GraphNode(graph_node_index++);

  // build operators
  for (int i = 0; i < g_operators.size(); i++) {

    ADROperator *adr_op = new ADROperator(&(g_operators[i]));
    adr_op->op_index = i;
    adr_ops().push_back(adr_op);

    const vector<Prevail> &prevail = g_operators[i].get_prevail();
    const vector<PrePost> &pre_post = g_operators[i].get_pre_post();

    for (int j = 0; j < prevail.size(); j++) {
      GraphNode *fluent = nodes()[prevail[j].var][prevail[j].prev];
      adr_op->prec.push_back(fluent);
      adr_op->pc_set.push_back(std::make_pair(prevail[j].var, prevail[j].prev));
      fluent->prec_of.push_back(adr_ops()[i]);
    }

    for (int j = 0; j < pre_post.size(); j++) {

      if (pre_post[j].pre != -1) {
        GraphNode *fluent = nodes()[pre_post[j].var][pre_post[j].pre];
        adr_op->prec.push_back(fluent);
        adr_op->pc_set.push_back(std::make_pair(pre_post[j].var, pre_post[j].pre));
        fluent->prec_of.push_back(adr_ops()[i]);
      }

      adr_op->add.push_back(nodes()[pre_post[j].var][pre_post[j].post]);
      nodes()[pre_post[j].var][pre_post[j].post]->effect_of.push_back(adr_op);

    }

    if (adr_op->pc_set.size() == 0) {
      adr_op->prec.push_back(dummy_pc());
      dummy_pc()->prec_of.push_back(adr_op);
    }
    std::sort(adr_op->pc_set.begin(), adr_op->pc_set.end());
    adr_op->base_cost = get_adjusted_cost(g_operators[i]);
  }

  // build the end operator
  m_end_operator = new ADROperator(NULL);
  end_operator()->op_index = -1;
  end_operator()->base_cost = 0;
  end_operator()->add.push_back(dummy_goal());
  dummy_goal()->effect_of.push_back(end_operator());
  for (int i = 0; i < g_goal.size(); i++) {
    GraphNode *goal = nodes()[g_goal[i].first][g_goal[i].second];
    end_operator()->prec.push_back(goal);
    goal->prec_of.push_back(end_operator());
  }
  adr_ops().push_back(end_operator());

  eval_count = 0;

  // do learning in the initial state
  if (read_from_file()) {
    add_pi_fluents_from_file("pi_fluents.txt");
  } else if (call_do_learning) {
    do_learning(g_initial_state());
  }

  finished_initialization = true;

  if (write_to_file()) {
    add_pi_fluents_to_file("pi_fluents.txt");
  }

  print_augmentation_data();
}

/*****************************************************************
 *
 * Utility methods used in different places
 *
 ****************************************************************/

GraphNode *
AugmentedDeleteRelaxation::get_graph_node(const FluentSet &fluents)
{
  if (fluents.size() == 1) {
    return nodes()[fluents[0].first][fluents[0].second];
  } else {
    for (int i = 0; i < nodes().back().size(); i++) {
      if (nodes().back()[i]->values == fluents) {
        return nodes().back()[i];
      }
    }
  }
  return NULL;
}

bool
AugmentedDeleteRelaxation::any_cycles()
{

  bool found_cycle = false;

  for (int i = 0; i < nodes().size(); i++) {
    for (int j = 0; j < nodes()[i].size(); j++) {
      if (check_cycle(nodes()[i][j])) {
        std::cout << "Found cycle for: " << nodes()[i][j]->values << std::endl;
        found_cycle = true;
      }
    }
  }
  return found_cycle;
}

bool
AugmentedDeleteRelaxation::check_cycle(GraphNode *gn)
{
  std::set<GraphNode *> deps_set;
  getDependencies(gn, deps_set);
  return (deps_set.find(gn) != deps_set.end());
}

void
AugmentedDeleteRelaxation::getDependencies(GraphNode *gn,
                                           std::set<GraphNode *> &deps)
{

  if ((gn->cost == -1) || (gn->supporter == 0)) {
    return;
  }

  for (int i = 0; i < gn->supporter->prec.size(); i++) {
    if (deps.find(gn->supporter->prec[i]) == deps.end()) {
      deps.insert(gn->supporter->prec[i]);
      getDependencies(gn->supporter->prec[i], deps);
    }
  }
}

void
AugmentedDeleteRelaxation::getDependencies(ADROperator *adr_op,
                                           std::vector<GraphNode *> &deps)
{
  for (int i = 0; i < adr_op->prec.size(); i++) {
    GraphNode *pc = adr_op->prec[i];
    if (std::find(deps.begin(), deps.end(), pc) == deps.end()) {
      deps.push_back(pc);
      if (pc->supporter) {
        getDependencies(pc->supporter, deps);
      }
    }
  }
}

/*****************************************************************
 *
 * Methods for adding pi fluents to the relaxation
 *
 ****************************************************************/

bool
AugmentedDeleteRelaxation::add_pi_fluents_to_file(string filename)
{

  ofstream out;
  out.open(filename.c_str());
  for (vector<FluentSet>::const_iterator it = pi_fluents.begin();
       it != pi_fluents.end();
       it++) {
    bool add_sep = false;
    for (FluentSet::const_iterator f = it->begin(); f != it->end(); f++) {
      if (add_sep) {
        out << ",";
      }
      add_sep = true;
      out << g_fact_names[f->first][f->second];
    }
    out << endl;
  }
  out.close();

  return true;
}

bool
AugmentedDeleteRelaxation::add_pi_fluents_from_file(string filename)
{

  ifstream infile;
  infile.open(filename.c_str());
  while (!infile.eof()) {
    string line;
    getline(infile, line);
    FluentSet fromthisline;
    for (int i = 0; i < g_fact_names.size(); i++) {
      for (int j = 0; j < g_fact_names[i].size(); j++) {
        if (line.find(g_fact_names[i][j]) != string::npos) {
          fromthisline.push_back(std::make_pair(i, j));
        }
      }
    }
    if (fromthisline.size() > 1) {
      add_pi_fluent(fromthisline);
    }
  }
  infile.close();

  return true;
}

void
AugmentedDeleteRelaxation::add_pi_fluent_and_all_subsets(
                                                         const FluentSet &pi_fluent)
{
  std::vector<FluentSet> subsets;
  get_all_subsets(subsets, pi_fluent);

  std::cout << "Adding all subsets of goal, total: " << subsets.size() <<
    std::endl;

  for (int i = 0; i < subsets.size(); i++) {
    add_pi_fluent(subsets[i]);
    //    print_operators();
  }
}

ADROperator *
AugmentedDeleteRelaxation::new_action_representative(ADROperator &adr_op,
                                                     const FluentSet &context)
{

  //  std::cout << "New action rep for " << adr_op.name() << " with context " << context << std::endl;

  ADROperator *cef = new ADROperator(adr_op.op);
  cef->op_index = adr_op.op_index;
  cef->base_cost = adr_op.base_cost;
  cef->pc_set = context;
  cef->pc_set.insert(cef->pc_set.end(),
                     adr_op.pc_set.begin(), adr_op.pc_set.end());
  std::sort(cef->pc_set.begin(), cef->pc_set.end());
  cef->context = context;
  cef->parent_op = &adr_op;

  // set pcs
  for (int i = 0; i < adr_op.prec.size(); i++) {
    cef->add_prec(adr_op.prec[i]);
  }
  for (int i = 0; i < context.size(); i++) {
    cef->add_prec(nodes()[context[i].first][context[i].second]);
  }

  // check whether some previously existing pi-fluent is a condition
  for (int i = 0; i < nodes().back().size(); i++) {
    GraphNode *existing_pi_fluent = nodes().back()[i];
    FluentSet &existing_pf_values = existing_pi_fluent->values;
    if (have_intersection(context, existing_pf_values) &&
        has_subset(cef->pc_set, existing_pf_values)) {
      // add as precondition, remove existing
      cef->add_prec(existing_pi_fluent);
    }
  }

  // add the add effects of the main operator
  for (int i = 0; i < adr_op.add.size(); i++) {
    cef->add.push_back(adr_op.add[i]);
    adr_op.add[i]->effect_of.push_back(cef);
  }

  // other cond effs that require a subset of this context are
  // also add effects
  for (int i = 0; i < adr_op.condeffs.size(); i++) {
    ADROperator *other_cef = adr_op.condeffs[i];
    if (has_subset(context, other_cef->context)) {
      for (int j = 0; j < other_cef->add.size(); j++) {
        if (std::find(cef->add.begin(), cef->add.end(), other_cef->add[j]) ==
            cef->add.end()) {
          cef->add.push_back(other_cef->add[j]);
          other_cef->add[j]->effect_of.push_back(cef);
        }
      }
    }
  }

  return cef;
}

int
AugmentedDeleteRelaxation::new_effects_introduced(FluentSet pi_fluent, int min)
{

  if (min == 0 || inconsistent(pi_fluent)) {
    return 0;
  }

  // ensure comparability
  std::sort(pi_fluent.begin(), pi_fluent.end());

  GraphNode *new_gn = get_graph_node(pi_fluent);

  if (new_gn) {
    // previously introduced pi-fluent
    return 0;
  }

  int total = 0;

  for (int op_index = 0; op_index < g_operators.size(); op_index++) {

    Operator &op = g_operators[op_index];
    ADROperator &adr_op = *(adr_ops()[op_index]);

    int num_prevail = 0, num_add = 0, num_pre = 0;
    FluentSet context;

    int i;
    for (i = 0; i < pi_fluent.size(); i++) {

      Fluent &f = pi_fluent[i];

      if (prevailed_by(op, f)) {
        num_prevail++;
      } else if (adds(op, f)) {
        num_add++;
      } else if (deletes_precondition(op, f)) {
        num_pre++;
      } else if (impossible_post(op, f) || impossible_pre(op, f)) {
        break;
      } else {
        context.push_back(f);
      }
    }

    if (i < pi_fluent.size()) {
      continue;
    }

    std::sort(context.begin(), context.end());

    if (context.size() && num_add &&
        (num_add + num_prevail + context.size() == pi_fluent.size())) {

      if (! use_conditional_effects()) {
        for (int j = 0; j < adr_op.condeffs.size(); j++) {
          if (! are_mutex(context, adr_op.condeffs[j]->context)) {
            FluentSet combined_context = context;
            get_combined_fluent_set(adr_op.condeffs[j]->context, combined_context);
            if (! adr_op.get_effect_with_context(combined_context)) {
              if (++total >= min) {
                break;
              }
            }
          }
        }
        // one action rep for only that pi fluent
        if (++total >= min) {
          break;
        };
      }
      // using conditional effects
      else {
        if (! adr_op.get_effect_with_context(context)) {
          if (++total >= min) {
            break;
          }
        }
      }
    }
  }

  return total;
}

bool
AugmentedDeleteRelaxation::add_pi_fluent(FluentSet pi_fluent)
{
  if (inconsistent(pi_fluent)) {
    // TODO: remove debug prints
    std::cout << "inconsistent pi fluent: " << pi_fluent << std::endl;
    return false;
  }

  // ensure comparability
  std::sort(pi_fluent.begin(), pi_fluent.end());

  GraphNode *new_gn = get_graph_node(pi_fluent);

  if (new_gn) {
    // previously introduced pi-fluent
    std::cout << "duplicate pi fluent: " << pi_fluent << std::endl;
    return false;
  }

  // Now we know that it's a valid addition, add fluent to problem
  new_gn = new GraphNode(graph_node_index++);
  nodes().back().push_back(new_gn);
  new_gn->values = pi_fluent;

  // check goal participation
  if (has_subset(g_goal, pi_fluent)) {
    end_operator()->add_prec(new_gn);
  }

  // check action participation
  for (int op_index = 0; op_index < g_operators.size(); op_index++) {

    Operator &op = g_operators[op_index];
    ADROperator &adr_op = *(adr_ops()[op_index]);

    int num_prevail = 0, num_pre = 0, num_add = 0;
    FluentSet context;

    int i;
    for (i = 0; i < pi_fluent.size(); i++) {

      Fluent &f = pi_fluent[i];

      if (prevailed_by(op, f)) {
        num_prevail++;
      } else if (adds(op, f)) {
        num_add++;
      } else if (deletes_precondition(op, f)) {
        num_pre++;
      } else if (impossible_pre(op, f) || impossible_post(op, f)) {
        break;
      } else {
        context.push_back(f);
      }
    }

    if (i < pi_fluent.size()) {
      continue;
    }

    std::sort(context.begin(), context.end());

    if (num_prevail + num_pre == pi_fluent.size()) {
      adr_op.add_prec(new_gn);
      for (int i = 0; i < adr_op.condeffs.size(); i++) {
        adr_op.condeffs[i]->add_prec(new_gn);
      }
    }

    else if (num_pre + num_prevail + context.size() == pi_fluent.size()) {
      for (int i = 0; i < adr_op.condeffs.size(); i++) {
        if (has_subset(adr_op.condeffs[i]->context, context)) {
          adr_op.condeffs[i]->add_prec(new_gn);
        }
      }
    }

    else if (num_prevail + num_add == pi_fluent.size()) {
      adr_op.add.push_back(new_gn);
      new_gn->effect_of.push_back(&adr_op);
      for (int i = 0; i < adr_op.condeffs.size(); i++) {
        adr_op.condeffs[i]->add.push_back(new_gn);
        new_gn->effect_of.push_back(adr_op.condeffs[i]);
      }
    }

    else if (num_add &&
             (num_add + num_prevail + context.size() == pi_fluent.size())) {

      if (! use_conditional_effects()) {

        // make a new action representative, combining this pi fluent
        // with the pi fluents added by the previous reps

        int prev_condeffs_size = adr_op.condeffs.size();
        for (int i = 0; i < prev_condeffs_size; i++) {

          if (are_mutex(context, adr_op.condeffs[i]->context)) {
            continue;
          }

          FluentSet combined_context = context;
          get_combined_fluent_set(adr_op.condeffs[i]->context, combined_context);

          ADROperator *cef = adr_op.get_effect_with_context(combined_context);
          if (! cef) {
            cef = new_action_representative(adr_op, combined_context);

            // add the new cond eff to the action
            adr_op.condeffs.push_back(cef);

            // add the new condeff to the set of all effects
            adr_ops().push_back(cef);
          }
        }
      }

      // make a new action representative just for this pi fluent
      // required for both conditional and non-conditional versions of P^C

      ADROperator *cef = adr_op.get_effect_with_context(context);

      if (! cef) {
        // we need a new conditional effect/representative
        ADROperator *cef = new_action_representative(adr_op, context);

        // add the new cond eff to the action
        adr_op.condeffs.push_back(cef);

        // add the new condeff to the set of all effects
        adr_ops().push_back(cef);
      }

      // if some other operator rep has a larger context, add this gn as its effect
      // cef will be covered here as well
      for (int i = 0; i < adr_op.condeffs.size(); i++) {
        ADROperator *some_cef = adr_op.condeffs[i];
        if (has_subset(some_cef->context, context)) {
          some_cef->add.push_back(new_gn);
          new_gn->effect_of.push_back(some_cef);
        }
      }
    }
  }

  std::cout << "Added: " << pi_fluent
            << ", x = " << current_x() << std::endl;

  pi_fluents.push_back(pi_fluent);

  return true;
}

/*****************************************************************
 *
 * Conflict detection/pi-fluent choice methods
 *
 ****************************************************************/

void
AugmentedDeleteRelaxation::simple_method(std::vector<GraphNode *> &pcs,
                                         std::vector<std::set<FluentSet> > &candidates)
{

  FluentSet together;
  if (pcs.size() > 1) {
    for (int i = 0; i < pcs.size(); i++) {
      get_combined_fluent_set(pcs[i]->values, together);
    }
    candidates[0].insert(together);
  } else if (pcs[0]->supporter) {
    simple_method(pcs[0]->supporter->prec, candidates);
  }
}


void
AugmentedDeleteRelaxation::collect_overlaps(std::vector<GraphNode *> &pcs,
                                            std::vector<std::set<FluentSet> > &candidates)
{

  for (int i = 0; i < pcs.size(); i++) {
    for (int j = i + 1; j < pcs.size(); j++) {
      if (have_intersection(pcs[i]->values, pcs[j]->values)) {
        FluentSet cand = pcs[i]->values;
        get_combined_fluent_set(pcs[j]->values, cand);
        candidates[0].insert(cand);
      }
    }
    if (pcs[i]->supporter) {
      collect_overlaps(pcs[i]->supporter->prec, candidates);
    }
  }
}

void
AugmentedDeleteRelaxation::collect_parallel_conflicts(const
                                                      std::vector<GraphNode *> &pcs,
                                                      GraphRelaxedPlan &grp,
                                                      std::set<std::pair<ADROperator *, Fluent> > &deleted,
                                                      std::set<std::pair<ADROperator *, Fluent> > &all_pcs,
                                                      std::vector<std::set<FluentSet> > &candidates)
{
  // for the conflicts collected by sets2, we use the heuristic
  // priority = length of path between deleter and failing action in
  // relaxed plan graph. here we have no such notion so we set a fixed
  // value.
  int priority = 1;
  // if true, return as soon as a conflict is found rather than doing
  // the full recursion.
  bool early_returns = false;

  std::vector<std::set<std::pair<ADROperator *, Fluent> > > local_deleted(
                                                                          pcs.size());
  std::vector<std::set<std::pair<ADROperator *, Fluent> > > local_pcs(pcs.size());

  for (int i = 0; i < pcs.size(); i++) {
    ADROperator *supporter = pcs[i]->supporter;

    if (supporter && !supporter->plan_computed) {

      collect_parallel_conflicts(supporter->prec, grp, local_deleted[i], local_pcs[i],
                                 candidates);

      if (early_returns && !candidates[priority].empty()) {
        return;
      }

      supporter->plan_computed = true;
      const Operator &op = *(supporter->op);

      for (int j = 0; j < supporter->pc_set.size(); j++) {
        Fluent f = supporter->pc_set[j];

        // mark that f is a precondition for supporter in this branch
        local_pcs[i].insert(std::make_pair(supporter, f));

        // mark that supporter deletes f
        if (deletes(op, f)) {
          local_deleted[i].insert(std::make_pair(supporter, f));
        }
      }

      // mark all those fluents deleted by supporter without being preconditions
      const std::vector<PrePost> &pp_vec = op.get_pre_post();
      for (int j = 0; j < pp_vec.size(); j++) {
        const PrePost &pp = pp_vec[j];
        // deletes any value of the variable, not necessarily its own precondition
        if (pp.pre == -1) {
          for (int k = 0; k < g_variable_domain[pp.var]; k++) {
            if (pp.post != k) {
              local_deleted[i].insert(std::make_pair(supporter, std::make_pair(pp.var, k)));
            }
          }
        }
      }
    }
  }

  for (int i = 0; i < pcs.size(); i++) {
    std::set<std::pair<ADROperator *, Fluent> > &i_deleted = local_deleted[i];
    for (std::set<std::pair<ADROperator *, Fluent> >::iterator i_it =
           i_deleted.begin();
         i_it != i_deleted.end(); i_it++) {
      for (int j = 0; j < pcs.size(); j++) {
        if (i != j) {

          std::set<std::pair<ADROperator *, Fluent> > &j_pcs = local_pcs[j];
          for (std::set<std::pair<ADROperator *, Fluent> >::iterator j_it = j_pcs.begin();
               j_it != j_pcs.end(); j_it++) {
            // delete the same fluent AND
            if ((i_it->second == j_it->second) &&
                // don't correspond to the same node in the graph RP
                (grp.get_node(i_it->first) != grp.get_node(j_it->first))) {

              // std::cout << i_it->second << " edeleted by " << *(i_it->first) << std::endl;
              // std::cout << j_it->second << "  precondition of " << *(j_it->first) << std::endl;

              FluentSet cand = pcs[i]->values;
              get_combined_fluent_set(pcs[j]->values, cand);
              candidates[priority].insert(cand);

              if (early_returns) {
                return;
              }

              break;
            }
          }
        }
      }
    }
  }

  for (int i = 0; i < pcs.size(); i++) {
    deleted.insert(local_deleted[i].begin(), local_deleted[i].end());
    all_pcs.insert(local_pcs[i].begin(), local_pcs[i].end());
  }
}

void
AugmentedDeleteRelaxation::get_non_dominated_pcs(GRPNode &grp_node,
                                                 std::vector<GraphNode *> &pcs)
{
  pcs.clear();
  pcs.insert(pcs.begin(), grp_node.effects[0]->prec.begin(),
             grp_node.effects[0]->prec.end());

  for (int i = 1; i < grp_node.effects.size(); i++) {
    for (int j = 0; j < grp_node.effects[i]->prec.size(); j++) {
      bool dominated = false;
      for (std::vector<GraphNode *>::iterator it = pcs.begin(); it != pcs.end()
             && !dominated;) {
        if (has_subset((*it)->values, grp_node.effects[i]->prec[j]->values)) {
          // already added a pc that dominates this one.
          dominated = true;
          break;
        } else if (has_subset(grp_node.effects[i]->prec[j]->values, (*it)->values)) {
          // dominates a pc. delete the existing one, but there may be
          // others it dominates as well so have to continue checking pcs.
          it = pcs.erase(it);
        } else {
          it++;
        }
      }
      if (!dominated) {
        pcs.push_back(grp_node.effects[i]->prec[j]);
      }
    }
  }
}

// faster method for getting zero priority conflicts.  i.e. conflicts
// where a is supporter of p but deletes q, which is in a precondition
// with p.
void AugmentedDeleteRelaxation::collect_zero_priority_sequential_conflicts(
                                                                           GraphRelaxedPlan &grp,
                                                                           std::vector<std::set<FluentSet> > &candidates)
{
  bool early_returns = false;

  for (int i = 0; i < grp.num_nodes; i++) {
    GRPNode &grp_node = grp.nodes[i];
    std::vector<GraphNode *> pcs;
    get_non_dominated_pcs(grp_node, pcs);
    for (std::vector<GraphNode *>::iterator it = pcs.begin(); it != pcs.end();
         it++) {
      if (!(*it)->supporter) {
        continue;
      }
      for (std::vector<GraphNode *>::iterator it2 = pcs.begin(); it2 != pcs.end();
           it2++) {
        if (it != it2) {
          if (adr_effect_edeletes_gn((*it)->supporter, *it2)) {
            FluentSet cand = (*it)->values;
            get_combined_fluent_set((*it2)->values, cand);
            candidates[0].insert(cand);

            if (early_returns) {
              return;
            }
          }
        }
      }
    }
  }
}

void
AugmentedDeleteRelaxation::collect_sequential_conflicts(
                                                        const std::vector<GraphNode *> &pcs,
                                                        std::vector<std::vector<GraphNode *> > accumulated_pcs,
                                                        std::vector<GraphNode *> &accumulated_targets,
                                                        std::vector<std::set<FluentSet> > &candidates,
                                                        int max_priority = -1)
{
  static bool early_returns = true;
  accumulated_pcs.push_back(pcs);
  // reserve an empty space here for the pc set we recurse on
  accumulated_targets.push_back(NULL);

  for (int i = 0; i < pcs.size(); i++) {
    ADROperator *supporter = pcs[i]->supporter;
    // !supporter means pcs[i] true in state
    if (supporter) {
      std::vector<std::vector<GraphNode *> > local_accumulated_pcs = accumulated_pcs;

      // fill space reserved above with pcs[i]
      accumulated_targets.back() = pcs[i];

      int startval = (max_priority == -1) ? 0 :
        std::max(0, (int)(accumulated_pcs.size() - (max_priority + 1)));
      for (int j = startval; j < accumulated_pcs.size(); j++) {
        std::vector<GraphNode *> &pc_vec = accumulated_pcs[j];
        for (int k = 0; k < pc_vec.size(); k++) {
          GraphNode *pc = pc_vec[k];
          if (adr_effect_edeletes_gn(supporter, pc)) {
            FluentSet cand = pc->values;
            get_combined_fluent_set(accumulated_targets[j]->values, cand);
            int priority = accumulated_pcs.size() - (j + 1);
            candidates[priority].insert(cand);
            if (early_returns && (priority == 0)) {
              accumulated_targets.pop_back();
              return;
            }
          }
        }
      }

      // remove the values added by this action from the set of pcs we
      // pass back to the recursive call since we don't care if
      // earlier actions delete them.
      for (int i = 0; i < local_accumulated_pcs.size(); i++) {
        for (std::vector<GraphNode *>::iterator it = local_accumulated_pcs[i].begin();
             it != local_accumulated_pcs[i].end();) {
          if (std::find(supporter->add.begin(), supporter->add.end(),
                        *it) != supporter->add.end()) {
            it = local_accumulated_pcs[i].erase(it);
          } else {
            it++;
          }
        }
      }
      collect_sequential_conflicts(supporter->prec, local_accumulated_pcs,
                                   accumulated_targets, candidates, max_priority);
      if (early_returns && (!candidates[0].empty())) {
        accumulated_targets.pop_back();
        return;
      }

    }
  }
  accumulated_targets.pop_back();
}

void
AugmentedDeleteRelaxation::add_all_pi_fluents_of_size_2()
{
  for (int var1 = 0; var1 < g_variable_domain.size(); var1++) {
    for (int val1 = 0; val1 < g_variable_domain[var1]; val1++) {
      for (int var2 = var1 + 1; var2 < g_variable_domain.size(); var2++) {
        for (int val2 = 0; val2 < g_variable_domain[var2]; val2++) {
          FluentSet pi_fluent;
          pi_fluent.push_back(std::make_pair(var1, val1));
          pi_fluent.push_back(std::make_pair(var2, val2));
          std::sort(pi_fluent.begin(), pi_fluent.end());
          add_pi_fluent(pi_fluent);
        }
      }
    }
  }
}


// void
// AugmentedDeleteRelaxation::iterative_conflict_finder(GraphRelaxedPlan &grp) {

//   grp.do_transitive_closure();

//   for (int i = 0; i < grp.nodes.size(); i++) {
//     for (int j = 0; j < grp.nodes.size(); j++) {
//       if (i == j) {
// 	continue;
//       }
//       if (!grp.transitive_graph[i].test(j)) {
// 	// node j is *not* ordered before node i (not j --> ... --> i)
// 	// so i potentially deletes some pc of j
// 	bool node_i_deletes_js_pc = false;
// 	// for each action in node j
// 	//	for (ADROperator *adr_op : grp.nodes[j].effects) {
// 	for (auto
// 	  std::cout << "yes";
// 	}
// 	if (node_i_deletes_js_pc) {
// 	  // if i deletes some precondition of j
// 	  std::cout << "no";
// 	}
//       }
//     }
//   }
// }


bool
AugmentedDeleteRelaxation::add_pi_fluents_from_rps_edeletes(
                                                            GraphRelaxedPlan &grp, int num_to_add, bool add_after_perfect)
{

  Timer pif_timer;

  bool display_candidates = false;
  int added_this_call = 0;

  std::vector<std::set<FluentSet> > candidates(MAX_PLAN_SIZE);

  bool found_conflict = false;
  bool found_zero_priority_conflict = false;

  // if( !found_conflict ) {
  //   simple_method(end_operator()->prec, candidates);
  // }

  std::vector<std::vector<GraphNode *> > accumulated_pcs;
  accumulated_pcs.reserve(MAX_PLAN_SIZE);
  std::vector<GraphNode *> accumulated_targets;
  accumulated_targets.reserve(MAX_PLAN_SIZE);
  //  collect_overlaps(end_operator()->prec, candidates);

  if (candidates[0].empty()) {

    collect_zero_priority_sequential_conflicts(grp, candidates);

  }

  if (candidates[0].empty()) {

    collect_sequential_conflicts(end_operator()->prec, accumulated_pcs,
                                 accumulated_targets, candidates);

  }

  for (int i = 0; i < candidates.size(); i++) {
    if (! candidates[i].empty()) {
      if (i == 0) {
        found_zero_priority_conflict = true;
      }
      found_conflict = true;
      break;
    }
  }

  if (!found_zero_priority_conflict) {
    clear_plan_computed();
    std::set<std::pair<ADROperator *, Fluent> > deletions, all_pcs;
    collect_parallel_conflicts(end_operator()->prec, grp, deletions, all_pcs,
                               candidates);
    clear_plan_computed();
    for (int i = 0; i < candidates.size(); i++) {
      if (! candidates[i].empty()) {
        found_conflict = true;
        break;
      }
    }
  }

  if (!found_conflict && add_after_perfect) {
    simple_method(end_operator()->prec, candidates);
    collect_overlaps(end_operator()->prec, candidates);
    for (int i = 0; i < candidates.size(); i++) {
      if (! candidates[i].empty()) {
        found_conflict = true;
        break;
      }
    }
  }

  //  std::cout << "Finished extra methods: " << pif_timer() << std::endl;

  for (int i = 0; i < candidates.size(); i++) {
    if (! candidates[i].empty()) {
      if (display_candidates) {
        std::cout << "Priority " << i << " candidates: " << std::endl;
      }
      for (std::set<FluentSet>::iterator it = candidates[i].begin();
           it != candidates[i].end(); it++) {
        if (display_candidates) {
          std::cout << "\t" << *it << std::endl;
        }
        GraphNode *gn = get_graph_node(*it);
        if (gn) {
          std::cout << *gn << " was previously added!" << std::endl;
          exit(1);
        }
      }
    }
  }

  //  std::cout << "Checked previously introduced: " << pif_timer() << std::endl;

  if (found_conflict) {
    for (int i = 0; i < candidates.size(); i++) {

      std::set<FluentSet> &cands = candidates[i];

      if (cands.empty()) {
        continue;
      }

      // int index = rng().next(cands.size());
      // std::advance(it, index);

      //      std::set<FluentSet> min_fluent_sets;
      FluentSet fs_to_add;// = *(cands.begin());

      int min_effects = std::numeric_limits<int>::max();
      //      std::set<FluentSet>::iterator min_pif = cands.begin();
      //      fs_to_add = *min_pif;

      //      find the pi fluent additions that result in minimum representative additions
      for (std::set<FluentSet>::iterator it = cands.begin();
           (it != cands.end()) && !size_bound_reached(); it++) {

        int introduced_this_pif = new_effects_introduced(*it, min_effects);

        if (introduced_this_pif < min_effects) {
          min_effects = introduced_this_pif;
          fs_to_add = *it;
          // min_fluent_sets.clear();
          // min_fluent_sets.insert(*it);
        }
        //         else if (introduced_this_pif == min_effects) {
        //           min_fluent_sets.insert(*it);
        //         }
      }

      //      std::cout << "Chose min growth pif: " << pif_timer() << std::endl;

      // randomly choose one of them
      // int index = rng().next(min_fluent_sets.size());
      // std::set<FluentSet>::iterator it = min_fluent_sets.begin();
      // std::advance(it, index);
      // bool added = add_pi_fluent(*it);

      if (add_pi_fluent(fs_to_add)) {
        if (++added_this_call == num_to_add) {
          return true;
        }
      }
    }
  }

  //  std::cout << "Finished rest of function: " << pif_timer() << std::endl;

  return (added_this_call != 0);
}

void
AugmentedDeleteRelaxation::clear_plan_computed()
{
  for (int i = 0; i < adr_ops().size(); i++) {
    adr_ops()[i]->plan_computed = false;
  }
  end_operator()->plan_computed = false;
}

int
AugmentedDeleteRelaxation::do_learning(const State &state)
{
  int res = 0;

  int learn_per_iteration = 1;
  bool dump_improvement_plans = false;

  Timer improvement_timer;

  while (!size_bound_reached() && !time_bound_reached()) {
    setup_exploration_queue_state(state);
    if (admissible()) {
      hmax_relaxed_exploration();
    } else {
      hadd_relaxed_exploration();
    }

    //  std::cout << "Finished exploration, t = " << improvement_timer() << std::endl;

    if (dummy_goal()->cost == -1) {
      // proved state to be a dead end
      res = 1;
      break;
    }

    bool prev_do_merging_flag = conditional_effect_merging();
    m_conditional_effect_merging = false;
    m_grp.clear();
    compute_graph_plan(m_grp);
    m_conditional_effect_merging = prev_do_merging_flag;

    //  std::cout << "Computed graph plan, t = " << improvement_timer() << std::endl;

    if (dump_improvement_plans) {
      static int num_iterations = 0;
      std::ostringstream oss;
      oss << "improvement_plan_" << num_iterations++ << ".gv";
      m_grp.dump(oss.str());
      std::cout << "Dumped " << oss.str() << std::endl;
      m_grp.clear();
    }

    // continue adding pi-fluents even if a valid plan is found unless
    // no_search is set.
    bool add_after_valid = !no_search();

    bool changed = add_pi_fluents_from_rps_edeletes(m_grp, learn_per_iteration,
                                                    add_after_valid);

    //  std::cout << "Added new pi fluents, t = " << improvement_timer() << std::endl;

    if (!changed) {
      res = 2;
      std::cout << "Found plan with no search in ADR, x = "
                << current_x() << std::endl;

      if (finished_initialization) {
        break;
      }
      if (no_search()) {
        // Found a valid plan, dump and exit.
        std::string plan_file_name = "plan.soln";
        std::cout << "Dumping to " << g_plan_filename << "." << std::endl;
        compute_graph_plan(m_grp);
        m_grp.dump_sequential(g_plan_filename.c_str());
        exit(0);
      } else {
        // continuing to add fluents could improve heuristic
        // estimates, even though we already have a valid plan.
        m_perfect = true;
        break;
      }
    }
  }
  std::cout << "Finished adding pi-fluents, t = " << improvement_timer() <<
    std::endl;
  return res;
}

/*****************************************************************
 *
 * Methods for computing hmax/hadd
 *
 ****************************************************************/

void
AugmentedDeleteRelaxation::get_initially_true(const State &state,
                                              std::vector<GraphNode *> &initially_true)
{

  // non pi-fluents
  for (int i = 0; i < g_variable_domain.size(); i++) {
    initially_true.push_back(nodes()[i][state[i]]);
  }

  // pi-fluents and dummy fluents
  std::vector<GraphNode *> &pi_fluents = nodes().back();
  for (int i = 0; i < pi_fluents.size(); i++) {
    FluentSet &fs = pi_fluents[i]->values;
    int j;
    for (j = 0; j < fs.size(); j++) {
      if (state[fs[j].first] != fs[j].second) {
        break;
      }
    }
    if (j == fs.size()) {
      initially_true.push_back(nodes().back()[i]);
    }
  }

  initially_true.push_back(dummy_pc());
}

void
AugmentedDeleteRelaxation::setup_exploration_queue_state(const State &state)
{

  for (int var = 0; var < nodes().size(); var++) {
    for (int val = 0; val < nodes()[var].size(); val++) {
      nodes()[var][val]->cost = -1;
      nodes()[var][val]->status = LMCUTPropStatus::UNREACHED;
      nodes()[var][val]->supporter = (ADROperator *)(-1);
      nodes()[var][val]->plan.clear();
      nodes()[var][val]->plan_computed = false;
    }
  }

  dummy_goal()->cost = -1;
  dummy_goal()->status = LMCUTPropStatus::UNREACHED;
  dummy_goal()->supporter = (ADROperator *)(-1);
  dummy_goal()->plan.clear();
  dummy_goal()->plan_computed = false;

  dummy_pc()->cost = -1;
  dummy_pc()->status = LMCUTPropStatus::UNREACHED;
  dummy_pc()->supporter = (ADROperator *)(-1);
  dummy_pc()->plan.clear();
  dummy_pc()->plan_computed = false;

  for (int i = 0; i < adr_ops().size(); i++) {

    ADROperator *op = adr_ops()[i];

    op->unsatisfied_preconditions = op->prec.size();
    op->hmax_justification = NULL;
    op->hmax_justification_cost = -1;

    op->plan_computed = false;

    // will be increased by precondition costs, as in FD code
    op->cost = op->base_cost;
  }

  // initial state fluents are propagated here
  // with supporter = 0 --> current state
  std::vector<GraphNode *> initially_true;
  get_initially_true(state, initially_true);
  for (int i = 0; i < initially_true.size(); i++) {
    enqueue_if_necessary(initially_true[i], 0, NULL);
  }
}

// only debugging purposes
void
AugmentedDeleteRelaxation::show_queue_contents()
{

  std::vector<std::pair<int, GraphNode *> > contents;

  while (!queue.empty()) {
    contents.push_back(queue.pop());
  }

  std::cout << "\t\t-----" << std::endl;
  for (int i = 0; i < contents.size(); i++) {
    std::cout << "\t\t" << contents[i].second->values << " with cost " <<
      contents[i].first << std::endl;
  }
  std::cout << "\t\t-----" << std::endl;

  for (int i = 0; i < contents.size(); i++) {
    queue.push(contents[i].first, contents[i].second);
  }
}

void
AugmentedDeleteRelaxation::hadd_relaxed_exploration()
{

  while (!queue.empty()) {
    std::pair<int, GraphNode *> top_pair = queue.pop();
    int cost = top_pair.first;
    GraphNode *fluent = top_pair.second;

    if (fluent->cost < cost) {
      continue;
    }

    if (fluent == dummy_goal()) {
      break;
    }

    const std::vector<ADROperator *> &triggered = fluent->prec_of;
    for (int i = 0; i < triggered.size(); i++) {
      //    for (int i = triggered.size() - 1; i >= 0; i--) {
      ADROperator *op = triggered[i];
      op->unsatisfied_preconditions--;
      op->cost += cost;

      if (op->unsatisfied_preconditions == 0) {
        for (int j = 0; j < op->add.size(); j++) {
          enqueue_if_necessary(op->add[j], op->cost, op);
        }
      }
    }
  }
  queue.clear();
}

void
AugmentedDeleteRelaxation::hmax_relaxed_exploration_get_reachable(std::vector<GraphNode *> &reached)
{
  //_computations++;

  while (!queue.empty()) {
    //_passes_c++;

    std::pair<int, GraphNode *> top_pair = queue.pop();
    int cost = top_pair.first;
    GraphNode *fluent = top_pair.second;

    if (fluent->cost < cost) {
      continue;
    }

    const std::vector<ADROperator *> &triggered = fluent->prec_of;
    for (int i = 0; i < triggered.size(); i++) {
      //_passes_x++;
      ADROperator *op = triggered[i];
      op->unsatisfied_preconditions--;
      if (op->unsatisfied_preconditions == 0) {
        for (int j = 0; j < op->add.size(); j++) {
          if (op->add[j]->cost == -1) {
            enqueue_if_necessary(op->add[j], 0, op);
            reached.push_back(op->add[j]);
          }
        }
      }
    }
  }

  //std::cout << _computations << " : " << ((double)_passes_c / _computations)
  //    << " : " << ((double)_passes_x / _computations) << " " << _passes_x << " : "
  //    << ((double)_passes_x_add / _computations) << " " << _passes_x_add << std::endl;
}
void
AugmentedDeleteRelaxation::hmax_relaxed_exploration()
{
  //_computations++;

  while (!queue.empty()) {
    //_passes_c++;

    std::pair<int, GraphNode *> top_pair = queue.pop();
    int cost = top_pair.first;
    GraphNode *fluent = top_pair.second;

    if (fluent->cost < cost) {
      continue;
    }

    const std::vector<ADROperator *> &triggered = fluent->prec_of;
    for (int i = 0; i < triggered.size(); i++) {
      //_passes_x++;
      ADROperator *op = triggered[i];
      op->unsatisfied_preconditions--;
      if (op->unsatisfied_preconditions == 0) {
        op->hmax_justification = fluent;
        op->hmax_justification_cost = fluent->cost;
        int target_cost = op->cost + fluent->cost;
        for (int j = 0; j < op->add.size(); j++) {
          //_passes_x_add++;
          enqueue_if_necessary(op->add[j], target_cost, op);
        }
      }
    }
  }

  //std::cout << _computations << " : " << ((double)_passes_c / _computations)
  //    << " : " << ((double)_passes_x / _computations) << " " << _passes_x << " : "
  //    << ((double)_passes_x_add / _computations) << " " << _passes_x_add << std::endl;
}

void
AugmentedDeleteRelaxation::enqueue_if_necessary(GraphNode *fluent, int cost,
                                                ADROperator *sup)
{

  if (fluent->cost == -1 || fluent->cost > cost) {

    // std::cout << "\tEnqueued " << fluent->values
    //        << " with cost " << cost << " (previous: " << fluent->cost
    //        << "), achieved by ";
    // if(sup)
    //   std::cout << *sup << std::endl;
    // else
    //   std::cout << "current state" << std::endl;

    fluent->cost = cost;
    fluent->supporter = sup;
    fluent->supporters.clear();
    fluent->status = LMCUTPropStatus::REACHED;
    queue.push(cost, fluent);
  }

  if (finished_initialization && fluent->cost == cost && sup
      && tie_breaking() != TieBreaking::ARBITRARY) {
    fluent->supporters.push_back(sup);
  }
}

/*****************************************************************
 *
 * Methods for computing landmark cut
 *
 ****************************************************************/

void
AugmentedDeleteRelaxation::mark_goal_plateau(GraphNode *fluent, int x)
{

  //  indent(x);
  //  std::cout << "Fluent is " << *fluent << " with cost " << fluent->cost << std::endl;

  // copied from original lmcut implementation
  if (fluent && fluent->status != LMCUTPropStatus::GOAL_ZONE) {
    fluent->status = LMCUTPropStatus::GOAL_ZONE;
    //    indent(x);
    //    std::cout << "Marked " << *fluent << " as GOAL_ZONE." << std::endl;
    const std::vector<ADROperator *> &eff_of = fluent->effect_of;
    for (int i = 0; i < eff_of.size(); i++) {
      //      indent(x);
      //      std::cout << "Adding effect: " << *(eff_of[i]) << " with cost " << eff_of[i]->cost<< std::endl;
      // TODO - can eliminate the second test if not using debug printing
      if ((eff_of[i]->cost == 0) && (eff_of[i]->hmax_justification_cost != -1)) {
        //	indent(x);
        //	std::cout << "hmax just is: " << *(eff_of[i]->hmax_justification) << std::endl;
        mark_goal_plateau(eff_of[i]->hmax_justification, x + 1);
      }
    }
  }
}

void
AugmentedDeleteRelaxation::mark_zones(const State &state,
                                      std::vector<ADROperator *> &cut)
{
  std::vector<GraphNode *> local_queue;
  get_initially_true(state, local_queue);

  for (int i = 0; i < local_queue.size(); i++) {
    local_queue[i]->status = LMCUTPropStatus::BEFORE_GOAL_ZONE;
    //    std::cout << "Initially marked " << *(local_queue[i]) << " as before goal zone." << std::endl;
  }

  while (! local_queue.empty()) {
    GraphNode *gn = local_queue.back();
    local_queue.pop_back();

    const std::vector<ADROperator *> &triggered = gn->prec_of;
    for (int i = 0; i < triggered.size(); i++) {
      ADROperator *adr_op = triggered[i];

      //      std::cout << "Triggered: " << *adr_op << " with hmax_just " << *(adr_op->hmax_justification) << std::endl;


      if (adr_op->hmax_justification == gn) {
        bool reached_goal_zone = false;
        for (int j = 0; j < adr_op->add.size(); j++) {
          GraphNode *ef = adr_op->add[j];
          if (ef->status == LMCUTPropStatus::GOAL_ZONE) {
            reached_goal_zone = true;
            //      std::cout << "hmax justification for " << *adr_op << " is " << *gn
            //                << " and effect " << *ef << " is in goal zone." << std::endl;

            //      std::cout << *gn << " -------------- (" << *adr_op << ") --------------> " << *ef << std::endl;

            cut.push_back(adr_op);
            //      std::cout << "Added to cut: " << *adr_op << std::endl;

            // ADROperator *main_op = adr_op->parent_op ? adr_op->parent_op : adr_op;
            // if(std::find(cut.begin(), cut.end(), main_op) == cut.end()) {
            //   cut.push_back(main_op);
            //   std::cout << "Added to cut: " << *main_op << std::endl;
            // }

            break;
          }
        }
        if (!reached_goal_zone) {
          for (int j = 0; j < adr_op->add.size(); j++) {
            GraphNode *ef = adr_op->add[j];
            if (ef->status != LMCUTPropStatus::BEFORE_GOAL_ZONE) {
              ef->status = LMCUTPropStatus::BEFORE_GOAL_ZONE;
              //              std::cout << "Marked " << *ef << " as before goal zone in propagation." << std::endl;
              local_queue.push_back(ef);
            }
          }
        }
      }
    }
  }
}

void
AugmentedDeleteRelaxation::hmax_relaxed_exploration_incremental(
                                                                std::vector<ADROperator *> &cost_reduced_effs)
{

  for (int i = 0; i < cost_reduced_effs.size(); i++) {
    ADROperator *op = cost_reduced_effs[i];
    int new_cost = op->hmax_justification_cost;
    if (new_cost != -1) {
      new_cost += op->cost;
      for (int j = 0; j < op->add.size(); j++) {
        enqueue_if_necessary(op->add[j], new_cost, op);
        //	std::cout << "Enqueued " << *(op->add[j]) << " with cost " << new_cost << " using op " << *op << std::endl;
      }
    }

    // ADROperator *main_op = cut[i]; // cut[i]->parent_op ? cut[i]->parent_op : cut[i];
    // int op_cost = main_op->cost;

    // std::cout << "Operator: " << *main_op << " (cost " << op_cost << " + "
    //        << main_op->hmax_justification_cost << ")" << std::endl;

    // int target_cost = op_cost + main_op->hmax_justification_cost;

    // for(int j = 0; j < main_op->add.size(); j++) {
    //   std::cout << "Enqueued " << *(main_op->add[j]) << " with cost " << target_cost << std::endl;
    //   enqueue_if_necessary(main_op->add[j], target_cost, main_op);
    // }

    // std::cout << "Condeff size: " << main_op->condeffs.size() << std::endl;

    // for(int j = 0; j < main_op->condeffs.size(); j++) {
    //   ADROperator *ceff = main_op->condeffs[j];
    //   // make sure the cond eff condition is reachable
    //   if(ceff->hmax_justification_cost != -1) {
    //  int target_cost = op_cost + ceff->hmax_justification_cost;
    //  for(int k = 0; k < ceff->add.size(); k++) {
    //    enqueue_if_necessary(ceff->add[k], target_cost, ceff);
    //  }
    //   }
    // }
  }

  //  std::cout << "Finished initial enqueues." << std::endl;

  while (! queue.empty()) {
    std::pair<int, GraphNode *> top_pair = queue.pop();
    int cost = top_pair.first;
    GraphNode *fluent = top_pair.second;

    //    std::cout << "Dequeued fluent " << *fluent << " with cost " << cost << std::endl;

    if (fluent->cost < cost) {
      //      std::cout << "fluent->cost = " << fluent->cost << std::endl;
      continue;
    }
    const std::vector<ADROperator *> &triggered = fluent->prec_of;
    for (int i = 0; i < triggered.size(); i++) {
      ADROperator *op = triggered[i];
      if (op->hmax_justification == fluent) {
        int old_cost = op->hmax_justification_cost;
        if (old_cost > fluent->cost) {
          // update hmax_supporter
          for (int j = 0; j < op->prec.size(); j++) {
            if (op->prec[j]->cost > op->hmax_justification->cost) {
              op->hmax_justification = op->prec[j];
            }
          }
          op->hmax_justification_cost = op->hmax_justification->cost;
          if (old_cost != op->hmax_justification_cost) {
            int target_cost =  op->hmax_justification_cost + op->cost;
            for (int j = 0; j < op->add.size(); j++) {
              //              std::cout << "Enqueueing add " << *(op->add[j]) << " of " << *op << std::endl;
              enqueue_if_necessary(op->add[j], target_cost, op);
            }
          }
        }
      }
    }
  }
}

void
AugmentedDeleteRelaxation::collect_hmax_paths(ADROperator *adr_op,
                                              std::set<ADROperator *> &paths)
{
  //  std::cout << "Collecting for " << *adr_op << std::endl;
  for (int i = 0; i < adr_op->prec.size(); i++) {
    if (adr_op->prec[i]->cost == adr_op->hmax_justification->cost) {
      std::cout << "Recursing through prec " << *(adr_op->prec[i]) << std::endl;
      collect_hmax_paths(adr_op->prec[i], paths);
    }
  }
}

void
AugmentedDeleteRelaxation::collect_hmax_paths(GraphNode *gn,
                                              std::set<ADROperator *> &paths)
{
  if (gn->supporter) {
    collect_hmax_paths(gn->supporter, paths);
    paths.insert(gn->supporter);
  }
}

int
AugmentedDeleteRelaxation::compute_lmcut(const State &state)
{

  int h = 0;
  std::vector<ADROperator *> cut;

  //  recursive_print_supporters(dummy_goal(), 0);

  //    return dummy_goal()->cost;

  if (dummy_goal()->cost == DEAD_END) {
    return DEAD_END;
  }

  while (dummy_goal()->cost != 0) {
    mark_goal_plateau(dummy_goal(), 0);
    mark_zones(state, cut);

    int cut_cost = std::numeric_limits<int>::max();
    for (int i = 0; i < cut.size(); i++) {
      //      std::cout << "In cut: " << *(cut[i]) << " with cost " << cut[i]->cost << std::endl;
      cut_cost = std::min(cut_cost, cut[i]->cost);
    }
    //    std::cout << "-- end of cut --" << std::endl;
    //    std::cout << "cut_cost is " << cut_cost << std::endl;

    std::vector<ADROperator *> reduced_actions;
    std::vector<ADROperator *> reduced_effects;

    if (use_conditional_effects()) {
      for (int i = 0; i < cut.size(); i++) {
        ADROperator *main_op = cut[i]->parent_op ? cut[i]->parent_op : cut[i];
        if (std::find(reduced_actions.begin(), reduced_actions.end(), main_op) ==
            reduced_actions.end()) {
          reduced_actions.push_back(main_op);

          main_op->cost -= cut_cost;
          reduced_effects.push_back(main_op);

          for (int j = 0; j < main_op->condeffs.size(); j++) {
            main_op->condeffs[j]->cost -= cut_cost;
            reduced_effects.push_back(main_op->condeffs[j]);
          }
        }
      }
    } else {
      for (int i = 0; i < cut.size(); i++) {
        cut[i]->cost -= cut_cost;
        reduced_effects.push_back(cut[i]);
      }
    }

    h += cut_cost;

    //    std::cout << "Set h to " << h << std::endl;

    hmax_relaxed_exploration_incremental(reduced_effects);

    for (int i = 0; i < nodes().size(); i++) {
      for (int j = 0; j < nodes()[i].size(); j++) {
        GraphNode *gn = nodes()[i][j];
        if (gn->status == LMCUTPropStatus::BEFORE_GOAL_ZONE
            || gn->status == LMCUTPropStatus::GOAL_ZONE) {
          gn->status = LMCUTPropStatus::REACHED;
        }
        //	std::cout << "Cost of " << *gn << " " << gn->cost << " after incremental recomputation." << std::endl;
      }
    }

    dummy_goal()->status = LMCUTPropStatus::REACHED;
    dummy_pc()->status = LMCUTPropStatus::REACHED;

    cut.clear();
  }
  return h;
}

/*****************************************************************
 *
 * Methods for doing stuff with relaxed plans
 *
 ****************************************************************/

void
GraphRelaxedPlan::recursive_transitive_closure(int node_index)
{

  std::vector<GRPNode *> &direct_supporters = nodes[node_index].supporters;

  for (int i = 0; i < direct_supporters.size(); i++) {
    int sup_index = direct_supporters[i]->index;
    if (!processed.test(sup_index)) {
      recursive_transitive_closure(sup_index);
      processed.set(sup_index);
    }
    transitive_graph[node_index] |= transitive_graph[sup_index];
  }
}

void
GraphRelaxedPlan::do_transitive_closure()
{
  recursive_transitive_closure(end_node_index);
}

void
GraphRelaxedPlan::dump_sequential(ofstream &planfile, GRPNode &node,
                                  std::vector<GRPNode *> &dumped)
{

  std::vector<GRPNode *> &sups = node.supporters;

  for (int i = 0; i < sups.size(); i++) {
    if (std::find(dumped.begin(), dumped.end(), sups[i]) == dumped.end()) {
      dump_sequential(planfile, *(sups[i]), dumped);
    }
  }
  if (node.index != end_node_index) {
    ADROperator *main_op = node.effects[0]->parent_op ?
      node.effects[0]->parent_op :
      node.effects[0];

    planfile << "(" << *main_op << ")" << std::endl;
    dumped.push_back(&node);
  }
}

void
GraphRelaxedPlan::dump_sequential(std::string filename)
{

  ofstream plan;
  plan.open(filename.c_str());
  std::vector<GRPNode *> dumped;
  dump_sequential(plan, nodes[end_node_index], dumped);
  plan.close();
}

void GraphRelaxedPlan::sequentialize_plan(std::vector<const Operator *> &result)
{
  std::vector<GRPNode *> dumped;
  sequentialize_plan(result, nodes[end_node_index], dumped);
}

void GraphRelaxedPlan::sequentialize_plan(std::vector<const Operator *> &result, GRPNode &node, std::vector<GRPNode *> &dumped)
{
  std::vector<GRPNode *> &sups = node.supporters;

  for (int i = 0; i < sups.size(); i++) {
    if (std::find(dumped.begin(), dumped.end(), sups[i]) == dumped.end()) {
      sequentialize_plan(result, *(sups[i]), dumped);
    }
  }
  if (node.index != end_node_index) {
    ADROperator *main_op = node.effects[0]->parent_op ?
      node.effects[0]->parent_op :
      node.effects[0];
    result.push_back(main_op->op);
    dumped.push_back(&node);
  }
}

bool
GraphRelaxedPlan::compatible_nodes(GRPNode &node1,
                                   GRPNode &node2)
{

  if (node1.effects[0]->op != node2.effects[0]->op) {
    return false;
  }

  for (int i = 0; i < node1.effects.size(); i++) {
    for (int j = 0; j < node2.effects.size(); j++) {
      if (are_mutex(node1.effects[i]->context, node2.effects[j]->context)) {
        return false;
      }
    }
  }

  //  std::cout << "Returning true in compatible_nodes" << std::endl;
  return true;
}

void
GraphRelaxedPlan::do_possible_merges()
{

  for (int i = 0; i < num_nodes; i++) {
    if (! removed.test(i)) {
      for (int j = i + 1; j < num_nodes; j++) {
        if (! removed.test(j)) {

          GRPNode &node1 = nodes[i];
          GRPNode &node2 = nodes[j];

          if (compatible_nodes(node1, node2) &&
              !transitive_graph[i].test(j) && !transitive_graph[j].test(i)) {

            // add all ceffs in node2 to node1
            node1.effects.insert(node1.effects.end(), node2.effects.begin(),
                                 node2.effects.end());

            // add all direct supporters of node2 to node1, if not already present
            for (int k = 0; k < node2.supporters.size(); k++) {
              if (std::find(node1.supporters.begin(), node1.supporters.end(),
                            node2.supporters[k]) == node1.supporters.end()) {
                node1.supporters.push_back(node2.supporters[k]);
              }
            }

            // if there was a path from x to j, there should now be a path from x to i
            transitive_graph[i] |= transitive_graph[j];

            removed.set(j);

            // update nodes that had incoming path from i or j
            for (int k = 0; k < num_nodes; k++) {
              if (! removed.test(k)) {
                if (transitive_graph[k].test(i) || transitive_graph[k].test(j)) {
                  transitive_graph[k].set(i);
                  transitive_graph[k] |= transitive_graph[i];

                  std::vector<GRPNode *>::iterator it =
                    std::find(nodes[k].supporters.begin(), nodes[k].supporters.end(), &node2);

                  if (it != nodes[k].supporters.end()) {
                    nodes[k].supporters.erase(it);
                    it = std::find(nodes[k].supporters.begin(), nodes[k].supporters.end(), &node1);
                    if (it == nodes[k].supporters.end()) {
                      nodes[k].supporters.push_back(&node1);
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

/*****************************************************************
 *
 * Methods for extracting a suboptimal plan
 *
 ****************************************************************/

void
AugmentedDeleteRelaxation::recursive_graph_plan(ADROperator *act,
                                                GraphRelaxedPlan &global_plan, int x)
{

  std::vector<GraphNode *> pcs = act->prec;
  for (int i = 0; i < pcs.size(); i++) {
    //    std::cout << "Fluent " << *(pcs[i]) << " (cost: " << pcs[i]->cost << ") ";
    ADROperator *supporter = pcs[i]->supporter;

    // tie breaking
    if (finished_initialization && supporter && pcs[i]->supporters.size() > 1) {

      assert(std::find(pcs[i]->supporters.begin(), pcs[i]->supporters.end(),
                       pcs[i]->supporter) != pcs[i]->supporters.end());

      switch (tie_breaking()) {
      case TieBreaking::ARBITRARY:
        break;

      case TieBreaking::DIFFICULTY: {
        int lowest_difficulty = numeric_limits<int>::max();
        for (auto sup : pcs[i]->supporters) {
          int difficulty = 0;
          for (auto precondition : sup->prec) {
            difficulty += precondition->cost;
          }
          if (difficulty < lowest_difficulty) {
            lowest_difficulty = difficulty;
            supporter = sup;
          }
        }
        break;
      }

      case TieBreaking::RANDOM: {
        int random_index = rng().next(pcs[i]->supporters.size());
        supporter = pcs[i]->supporters[random_index];
        break;
      }
      }
    } else {
      supporter = pcs[i]->supporter;
    }

    if (supporter) {
      //      std::cout << " with supporter " << *supporter << std::endl;
      global_plan.add_edge(supporter, act);
      if (!supporter->plan_computed) {
        recursive_graph_plan(supporter, global_plan, x + 1);
      }

    }
    // else {
    //   std::cout << " with no supporter." << std::endl;
    // }
  }
  act->plan_computed = true;
}

int
AugmentedDeleteRelaxation::compute_graph_plan(GraphRelaxedPlan &global_plan)
{
  clear_plan_computed();
  global_plan.clear();

  global_plan.add_end_node(end_operator());
  recursive_graph_plan(end_operator(), global_plan, 0);

  if (conditional_effect_merging()) {
    global_plan.do_transitive_closure();
    global_plan.do_possible_merges();
  }

  return global_plan.cost();
}

int
AugmentedDeleteRelaxation::mark_preferred_and_compute_multiset_plan(
                                                                    MultisetPlan &global_plan)
{

  for (int i = 0; i < end_operator()->prec.size(); i++) {

    if (end_operator()->prec[i]->cost == -1) {
      return DEAD_END;
    }

    recursive_multiset_plan(end_operator()->prec[i]);
    multiset_union(global_plan, end_operator()->prec[i]->plan);
  }

  return get_plan_cost(global_plan);

}

bool
AugmentedDeleteRelaxation::recursive_multiset_plan(GraphNode *fluent)
{

  if (fluent->supporter == 0) {
    return true;
  }

  if (! fluent->plan_computed) {
    //    gvsg.add_edge(get_sup_string(fluent->supporter, fluent->condeff_supporter), fluent->toString());
    bool all_pcs_true = true;

    // take max of counts with multiset_union
    for (int i = 0; i < fluent->supporter->prec.size(); i++) {
      all_pcs_true = (recursive_multiset_plan(fluent->supporter->prec[i])
                      && all_pcs_true);
      multiset_union(fluent->plan, fluent->supporter->prec[i]->plan);
      // gvsg.add_edge(fluent->supporter->prec[i]->toString(),
      //              get_sup_string(fluent->supporter, fluent->condeff_supporter));
    }

    if (all_pcs_true) {
      if (fluent->supporter->op) {
        set_preferred(fluent->supporter->op);
      }
    }

    multiset_insert(fluent->plan, fluent->supporter);
    fluent->plan_computed = true;
  }

  return false;
}

/*****************************************************************
 *
 * Main method called from the outside that switches between
 * admissible/inadmissible etc.
 *
 ****************************************************************/

bool
AugmentedDeleteRelaxation::compute_dead_end(const State &state)
{

  setup_exploration_queue_state(state);
  hmax_relaxed_exploration();
  return m_end_operator->unsatisfied_preconditions > 0;
}

int
AugmentedDeleteRelaxation::compute_heuristic(const State &state)
{

  bool dump_graph_plans = false;
  static int eval_count = 0;

  int h = 0;
  setup_exploration_queue_state(state);

  // h^C version
  //hmax_relaxed_exploration();
  //return dummy_goal()->cost;



  if (admissible()) {
    hmax_relaxed_exploration();
    if (m__fast) {
      if (m_end_operator->unsatisfied_preconditions > 0) {
        h = DEAD_END;
      } else {
        h = m_end_operator->hmax_justification_cost;
      }
    } else {
      h = compute_lmcut(state);
    }
  } else {
    if (use_hmax_best_supporter()) {
      hmax_relaxed_exploration();
    } else {
      hadd_relaxed_exploration();
    }

    if (dummy_goal()->cost == -1) {
      return DEAD_END;
    }

    h = compute_graph_plan(m_grp);

    std::vector<const Operator *> preferred;
    m_grp.get_preferred(preferred);
    for (int i = 0; i < preferred.size(); i++) {
      assert(preferred[i]->is_applicable(state));
      set_preferred(preferred[i]);
      // std::cout << "Set preferred: " << preferred[i]->get_name() << std::endl;
    }

    if (dump_graph_plans) {
      std::ostringstream oss;
      oss << "final_plan_" << eval_count++ << ".gv";
      m_grp.dump(oss.str());
    }

    m_grp.clear();
  }

  //  dump("problem_representation.gv");

  //  std::cout << "Return value: " << h << std::endl;
  //  exit(1);

  return h;
}

/*****************************************************************
 *
 * output functions
 *
 ****************************************************************/

struct alphabeticalADROperatorSort {
  bool operator()(const ADROperator *adr1, const ADROperator *adr2) {
    return adr1->name() < adr2->name();
  }
};

void
AugmentedDeleteRelaxation::print_operators()
{

  // std::set<ADROperator*, alphabeticalADROperatorSort> sorted;

  // sorted.insert(adr_ops().begin(), adr_ops().begin() + g_operators.size());

  // for(std::set<ADROperator*, alphabeticalADROperatorSort>::iterator it = sorted.begin();
  //     it != sorted.end(); it++) {
  //   print_operator(*it);
  //   std::cout << "---------------------------------------------" << std::endl;
  // }


  for (int op_index = 0; op_index < g_operators.size(); op_index++) {
    const ADROperator *op = adr_ops()[op_index];
    print_operator(op);
    std::cout << "---------------------------------------------" << std::endl;
  }
}

struct alphabeticalGraphNodeSort {
  bool operator()(const GraphNode *gn1, const GraphNode *gn2) {
    return gn1->toString() < gn2->toString();
  }
};

void
AugmentedDeleteRelaxation::print_operator(const ADROperator *op)
{

  std::cout << "Operator: " << op->name() << std::endl;

  //  std::set<GraphNode*, alphabeticalGraphNodeSort> sorted;

  std::cout << "Precondition: " << std::endl;

  // sorted.insert(op->prec.begin(), op->prec.end());
  // for(std::set<GraphNode*, alphabeticalGraphNodeSort>::iterator it = sorted.begin();
  //     it != sorted.end(); it++) {
  //   std::cout << "\t" << (*it)->values << std::endl;
  // }

  for (int i = 0; i < op->prec.size(); i++) {
    std::cout << "\t" << op->prec[i]->values << std::endl;
  }

  //  sorted.clear();
  //  sorted.insert(op->add.begin(), op->add.end());

  std::cout << "Effect: " << std::endl;

  // for(std::set<GraphNode*, alphabeticalGraphNodeSort>::iterator it = sorted.begin();
  //     it != sorted.end(); it++) {
  //   std::cout << "\t" << (*it)->values << std::endl;
  // }

  for (int i = 0; i < op->add.size(); i++) {
    std::cout << "\t" << op->add[i]->values << std::endl;
  }

  for (int i = 0; i < op->condeffs.size(); i++) {

    ADROperator *ceff = op->condeffs[i];

    std::cout << "Context #" << i << ": " << ceff->context << std::endl;

    std::cout << "\tConditions: " << std::endl;
    for (int j = 0; j < ceff->prec.size(); j++) {
      std::cout << "\t\t" << ceff->prec[j]->values << std::endl;
    }
    std::cout << "\tEffect:" << std::endl;
    for (int j = 0; j < ceff->add.size(); j++) {
      std::cout << "\t\t" << ceff->add[j]->values << std::endl;
    }
  }
}

void
AugmentedDeleteRelaxation::print_augmentation_data()
{
  std::cout << "Total added pi-fluents: " << num_pi_fluents()
            << " (max allowed was " << max_num_pi_fluents()
            << " with max m = " << max_m() << ")" << std::endl;
  std::cout << "Total effects: " << adr_ops().size()
            << " (max allowed was " << problem_size_bound() * g_operators.size()
            << ")" << std::endl;

  float growth_in_A = current_x();
  int total_facts = 0;
  for (auto domain : g_variable_domain) {
    total_facts += domain;
  }
  float growth_in_F = (pi_fluents.size() + total_facts) / (float)(total_facts);
  std::cout << "Growth in |A|: " << growth_in_A << std::endl;
  std::cout << "Growth in |F|: " << growth_in_F << std::endl;
  std::cout << "Growth ratio: " << (growth_in_A - 1) / (growth_in_F - 1) <<
    std::endl;

  unsigned total_strips_ops = 0;
  bool overflow = false;
  for (int op_index = 0; op_index < g_operators.size(); op_index++) {
    unsigned strips_op_size = (1 << adr_ops()[op_index]->condeffs.size());

    if ((UINT_MAX - strips_op_size) < total_strips_ops) {
      overflow = true;
    }

    total_strips_ops += strips_op_size;
  }

  std::cout << "STRIPS representation would contain " << total_strips_ops
            << " actions (overflow: " << overflow << ")." << std::endl;

}

void
AugmentedDeleteRelaxation::print_supporter(GraphNode *gn)
{
  std::cout << "Supporter of " << *gn << " is ";
  if (gn->supporter) {
    std::cout << *(gn->supporter);
  } else {
    std::cout << "current state";
  }

  std::cout << ", cost = " << gn->cost;
  if (gn->supporter && gn->supporter->hmax_justification) {
    std::cout << " (hmax justification " << *(gn->supporter->hmax_justification) <<
      ")";
  }
}

void
AugmentedDeleteRelaxation::print_supporters()
{
  for (int i = 0; i < nodes().size(); i++) {
    for (int j = 0; j < nodes()[i].size(); j++) {
      if (nodes()[i][j]->cost != -1) {
        print_supporter(nodes()[i][j]);
        std::cout << std::endl;
      }
    }
  }
}

void
AugmentedDeleteRelaxation::recursive_print_supporters(GraphNode *gn, int x)
{
  indent(x);
  print_supporter(gn);
  std::cout << std::endl;
  if (gn->supporter) {
    for (int i = 0; i < gn->supporter->prec.size(); i++) {
      recursive_print_supporters(gn->supporter->prec[i], x + 1);
    }
  }
}

void
AugmentedDeleteRelaxation::recursive_print_supporters()
{
  recursive_print_supporters(dummy_goal(), 0);
}

void
AugmentedDeleteRelaxation::dump(std::string filename)
{

  GraphvizGraph gvg;

  for (int i = 0; i < nodes().size(); i++) {
    for (int j = 0; j < nodes()[i].size(); j++) {
      std::ostringstream oss;
      oss << *(nodes()[i][j]);
      gvg.add_node(oss.str());
    }
  }

  for (int i = 0; i < adr_ops().size(); i++) {
    ADROperator &op = *(adr_ops()[i]);
    for (int pc = 0; pc < op.prec.size(); pc++) {
      string pc_name = op.prec[pc]->toString();
      for (int add = 0; add < op.add.size(); add++) {
        string add_name = op.add[add]->toString();
        if (add_name.compare("[<none of those>]")) {
          gvg.add_edge(pc_name, add_name, op.name());
          std::cout << "Adding edge: " << pc_name << " ------ (" << op.name() <<
            ") ------> " << add_name << std::endl;
        }
      }
    }
  }

  gvg.dump(filename);
}


/*****************************************************************
 *
 * interaction with fast downward
 *
 ****************************************************************/


void
AugmentedDeleteRelaxation::add_options_to_parser(OptionParser &parser)
{
  parser.add_option<int>("m",
                         "maximum pi-fluent size to use",
                         std::to_string(std::numeric_limits<int>::max()));
  parser.add_option<int>("n",
                         "maximum number of pi-fluents to use",
                         std::to_string(std::numeric_limits<int>::max()));
  parser.add_option<float>("x",
                           "bound on problem size as factor of original", "2.0");
  parser.add_option<bool>("conditional_effect_merging",
                          "merge conditional effects in relaxed plans", "true");
  parser.add_option<bool>("admissible",
                          "use lmcut on augmented delete relaxation to compute admissible heuristic",
                          "false");
  parser.add_option<int>("learning_time",
                         "Bound on learning time in seconds.", "10000");
  parser.add_option<bool>("no_search", "try to find plans with no search.",
                          "true");
  parser.add_option<bool>("use_conditional_effects",
                          "use the conditional effects compilation P^C_{ce} rather than standard P^C.",
                          "true");
  parser.add_option<bool>("read_from_file",
                          "read the pi-fluents to be used from pi-fluents.txt.", "false");
  parser.add_option<bool>("use_hmax_best_supporter",
                          "use h^max as best supporter function rather than h^add", "false");

  parser.add_option<bool>("write_to_file",
                          "read the pi-fluents to be used from pi-fluents.txt.", "false");

  parser.add_option<bool>("_fast", "", "false");

  parser.add_option<bool>("learn_on_i", "", "true");

  // tie breaking options
  std::vector<std::string> tie_breaking_options;
  tie_breaking_options.push_back("ARBITRARY");
  tie_breaking_options.push_back("DIFFICULTY");
  tie_breaking_options.push_back("RANDOM");
  parser.add_enum_option("tie_breaking", tie_breaking_options,
                         "Tie breaking options: ARBITRARY, DIFFICULTY (like FF), RANDOM.", "ARBITRARY");

  // random seed (only relevant if tie breaking is set to RANDOM)
  parser.add_option<int>("seed",
                         "Random seed (only used for random tie breaking). If this is set to -1, an arbitrary seed is used.",
                         "-1");
}

static Heuristic *_parse(OptionParser &parser)
{

  Heuristic::add_options_to_parser(parser);
  AugmentedDeleteRelaxation::add_options_to_parser(parser);

  Options opts = parser.parse();

  if (parser.dry_run()) {
    return 0;
  } else {
    return new AugmentedDeleteRelaxation(opts);
  }
}

static Plugin<Heuristic> _plugin("adr", _parse);
