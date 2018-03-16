#ifndef AUGMENTED_DELETE_RELAXATION_H
#define AUGMENTED_DELETE_RELAXATION_H

#include "../globals.h"
#include "../heuristic.h"
#include "../operator.h"
#include "../priority_queue.h"
#include "../rng.h"
#include "../timer.h"

#include "fluent_set_utilities.h"
#include "../graphviz_graph.h"
#include "list_multiset.h"
#include "operator_utilities.h"

#include <bitset>
#include <fstream>
#include <limits.h>
#include <sstream>
#include <string>
#include <vector>
#include <limits>

struct ADROperator;
struct GraphNode;

class ADRRefinement;

typedef std::list<std::pair<ADROperator *, int> > MultisetPlan;

bool adr_effect_edeletes_gn(const ADROperator *adr_op, const GraphNode *gn);

enum class LMCUTPropStatus
  {
    UNREACHED = 0,
      REACHED = 1,
      GOAL_ZONE = 2,
      BEFORE_GOAL_ZONE = 3
      };

// a node in the augmented delete relaxation hypergraph
// could be either a fact of original problem or pi-fluent

struct GraphNode {

  unsigned id;

  int cost;
  ADROperator *supporter;
  std::vector<ADROperator *> supporters;
  MultisetPlan plan;
  bool plan_computed;

  LMCUTPropStatus status;

  std::vector<ADROperator *> prec_of;
  std::vector<ADROperator *> effect_of;

  FluentSet values;

  std::set<GraphNode *> landmarks;
  std::set<GraphNode *> natural_predecessors;
  std::set<GraphNode *> greedy_necessary_predecessors;
  std::set<GraphNode *> necessary_predecessors;

  std::string toString() const {
    std::ostringstream oss;
    oss << values;
    return oss.str();
  }

GraphNode(unsigned idx) : id(idx) {
  cost = -1;
  supporter = NULL;
  plan_computed = false;
}

};

std::ostream &operator<<(std::ostream &os, const GraphNode &gn);

struct ADROperator {

  static bool m_remove_dominated_preconditions;
  bool remove_dominated_preconditions() {
    return m_remove_dominated_preconditions;
  }

  FluentSet pc_set;
  FluentSet context;
  std::vector<GraphNode *> prec;
  std::vector<GraphNode *> add;

  GraphNode *hmax_justification;
  int hmax_justification_cost;
  int cost;
  int base_cost;
  unsigned unsatisfied_preconditions;

  bool plan_computed;

  const Operator *op;
  // needed for landmark achievers
  int op_index;

  ADROperator *parent_op;

ADROperator(const Operator *op) : op(op), parent_op(NULL) { }

  std::vector<ADROperator *> condeffs;

  ADROperator *get_effect_with_context(const FluentSet &context) {
    for (int i = 0; i < condeffs.size(); i++) {
      ADROperator *cef = condeffs[i];
      if (cef->context == context) {
        return cef;
      }
    }
    return NULL;
  }

  std::string name() const {
    if (op) {
      return op->get_name();
    } else {
      return "end";
    }
  }

  void add_prec(GraphNode *gn);

};

std::ostream &operator<<(std::ostream &os, const ADROperator &adr_op);

std::string get_sup_string(const ADROperator *adr_op);

struct setSizeSort {
  bool operator()(const FluentSet &fs1, const FluentSet &fs2) const {
    return (fs1.size() < fs2.size());
  }
};

struct setSizeSort2 {
  bool operator()(const FluentSet &fs1, const FluentSet &fs2) const {
    return (fs1.size() > fs2.size());
  }
};

struct decreasingCostSort {
  bool operator()(const GraphNode *gn1, const GraphNode *gn2) const {
    if (gn1->cost > gn2->cost) {
      return true;
    }
    if (gn1->cost < gn2->cost) {
      return false;
    }
    return (gn1->values.size() > gn2->values.size());
  }
};

struct decreasingCostSort_ {
  bool operator()(const GraphNode *gn1, const GraphNode *gn2) const {
    if (gn1->cost < gn2->cost) {
      return true;
    }
    if (gn1->cost > gn2->cost) {
      return false;
    }
    return (gn1->values.size() < gn2->values.size());
  }
};

#define MAX_PLAN_SIZE 4096

struct GRPNode {

  std::vector<ADROperator *> effects;
  std::vector<GRPNode *> supporters;

  int index;

  void clear() {
    effects.clear();
    supporters.clear();
  }
};

std::ostream &operator<<(std::ostream &os, const GRPNode &node);

struct GraphRelaxedPlan {

  std::vector<std::bitset<MAX_PLAN_SIZE> > transitive_graph;
  std::bitset<MAX_PLAN_SIZE> processed;
  std::bitset<MAX_PLAN_SIZE> removed;

  int end_node_index;

  //  std::vector<std::vector<ADROperator*> > nodes;
  std::vector<GRPNode> nodes;
  int num_nodes;

  GraphRelaxedPlan() {
    transitive_graph.resize(MAX_PLAN_SIZE);
    nodes.resize(MAX_PLAN_SIZE);
    for (int i = 0; i < nodes.size(); i++) {
      nodes[i].index = i;
    }
    processed.reset();
    removed.reset();
    num_nodes = 0;
  }

  int get_node(ADROperator *op) {
    for (int i = 0; i < num_nodes; i++) {
      for (int j = 0; j < nodes[i].effects.size(); j++) {
        if (nodes[i].effects[j] == op) {
          return i;
        }
      }
    }

    nodes[num_nodes].clear();
    nodes[num_nodes].effects.push_back(op);
    transitive_graph[num_nodes].reset();
    return num_nodes++;
  }

  void add_end_node(ADROperator *op) {
    end_node_index = get_node(op);
  }

  GRPNode &end_node() {
    return nodes[end_node_index];
  }

  void add_node(ADROperator *op) {
    get_node(op);
  }

  void clear() {
    num_nodes = 0;
    processed.reset();
    removed.reset();
  }

  void add_edge(ADROperator *op1, ADROperator *op2) {
    int src = get_node(op1);
    int target = get_node(op2);

    nodes[target].supporters.push_back(&(nodes[src]));
    transitive_graph[target].set(src);

  }

  void dump(std::string filename) {
    GraphvizGraph gvg;
    GRPNode &end_node = nodes[end_node_index];
    recursive_dump(end_node, gvg);

    std::vector<const Operator *> preferred;
    get_preferred(preferred);

    std::ostringstream helpful;

    for (int i = 0; i < preferred.size(); i++) {
      helpful << "Helpful: " <<  preferred[i]->get_name()
              << ((i == preferred.size() - 1) ? "" : "\\n");
    }

    gvg.add_node(helpful.str());

    gvg.dump(filename);
  }

  void recursive_dump(GRPNode &node, GraphvizGraph &gvg) {

    std::ostringstream thisnode;
    thisnode << node;

    for (int i = 0; i < node.supporters.size(); i++) {
      recursive_dump(*(node.supporters[i]), gvg);
      std::ostringstream source;
      source << *(node.supporters[i]);
      gvg.add_edge(source.str(), thisnode.str());
    }
  }

  void sequentialize_plan(std::vector<const Operator *> &result);
  void sequentialize_plan(std::vector<const Operator *> &result, GRPNode &node, std::vector<GRPNode *> &dumped);
  void dump_sequential(std::string filename);
  void dump_sequential(std::ofstream &planfile, GRPNode &node,
                       std::vector<GRPNode *> &dumped);

  void do_transitive_closure();
  void recursive_transitive_closure(int node);
  bool compatible_nodes(GRPNode &node1,
                        GRPNode &node2);
  void do_possible_merges();

  void get_preferred(std::vector<const Operator *> &preferred) {
    for (int i = 0; i < num_nodes; i++) {

      if (!removed.test(i) &&
          nodes[i].supporters.size() == 0 &&
          // transitive_graph[i].none() &&
          (i != end_node_index)) {
        preferred.push_back(nodes[i].effects[0]->op);
      }
    }
  }

  int cost() {
    int h = 0;
    for (int i = 0; i < num_nodes; i++) {
      if (!removed.test(i)) {
        h += nodes[i].effects[0]->base_cost;
      }
    }
    return h;
  }

};

class AugmentedDeleteRelaxation : public Heuristic
{
  friend class ADRRefinement;

 public:
  //unsigned _passes_x;
  //unsigned _passes_c;
  //unsigned _passes_x_add;
  //unsigned _computations;

  void dump_statistics(std::ostream &)
  {
    print_augmentation_data();
  }

  AugmentedDeleteRelaxation(const Options &opts);

  ~AugmentedDeleteRelaxation() {

    print_augmentation_data();

    for (int i = 0; i < adr_ops().size(); i++) {
      delete adr_ops()[i];
    }
    for (int i = 0; i < nodes().size(); i++) {
      for (int j = 0; j < nodes()[i].size(); j++) {
        delete nodes()[i][j];
      }
    }
    delete dummy_goal();
    delete dummy_pc();
  }

  bool add_pi_fluent(FluentSet pi_fluent);

  // return number of new condeffs/actions if it is less than min, possibly min otherwise
  int new_effects_introduced(FluentSet pi_fluent,
                             int min = std::numeric_limits<int>::max());
  ADROperator *new_action_representative(ADROperator &base_op,
                                         const FluentSet &context);

  void add_all_pi_fluents_of_size_2();

  void add_pi_fluent_and_all_subsets(const FluentSet &pi_fluent);

  int num_pi_fluents() {
    return nodes().back().size();
  }

  int num_fluents() {
    unsigned total = 2; // include dummy goal and pc
    for (int i = 0; i < nodes().size(); i++) {
      total += nodes()[i].size();
    }
    return total;
  }

  void set_remove_dominated_preconditions(bool val) {
    ADROperator::m_remove_dominated_preconditions = val;
  }

  static void add_options_to_parser(OptionParser &parser);

  void dump(std::string filename);

  bool compute_dead_end(const State &state);
  int do_learning(const State &state);
 protected:
  bool learn_on_i;
  enum class TieBreaking
  {
    ARBITRARY,
      DIFFICULTY,
      RANDOM
      };

  friend class ConjunctionsHeuristic;
  friend class GenerateWithADR;
  std::vector<FluentSet> pi_fluents;

  friend class ADRLandmarks;

  static unsigned int graph_node_index;

  int eval_count;

  bool finished_initialization;

  int m_max_num_pi_fluents;
  int max_num_pi_fluents() {
    return m_max_num_pi_fluents;
  }

  int m_max_m;
  int max_m() {
    return m_max_m;
  }

  float m_problem_size_bound;
  float problem_size_bound() {
    return m_problem_size_bound;
  }
  void set_problem_size_bound(float x) {
    m_problem_size_bound = x;
  }

  float current_x() {
    return ((float)adr_ops().size() / (float)(g_operators.size() + 1));
  }

  bool m__fast;
  bool m_admissible;
  bool admissible() {
    return m_admissible;
  }

  bool m_conditional_effect_merging;
  bool conditional_effect_merging() {
    return m_conditional_effect_merging;
  }

  int m_learning_time_bound;
  int learning_time_bound() {
    return m_learning_time_bound;
  }

  bool m_no_search;
  bool no_search() {
    return m_no_search;
  }

  bool m_perfect;
  bool perfect() {
    return m_perfect;
  }

  bool m_use_conditional_effects;
  bool use_conditional_effects() {
    return m_use_conditional_effects;
  }

  bool m_read_from_file;
  bool read_from_file() {
    return m_read_from_file;
  }

  bool m_write_to_file;
  bool write_to_file() {
    return m_write_to_file;
  }

  bool m_use_hmax_best_supporter;
  bool use_hmax_best_supporter() {
    return m_use_hmax_best_supporter;
  }

  RandomNumberGenerator m_rng;
  RandomNumberGenerator &rng() {
    return m_rng;
  }

  TieBreaking m_tie_breaking;
  TieBreaking tie_breaking() {
    return m_tie_breaking;
  }

  // return "added anything?"
  bool add_pi_fluents_from_rps_edeletes(GraphRelaxedPlan &grp, int num_to_add,
                                        bool add_after_perfect);


  bool add_pi_fluents_from_file(std::string filename);
  bool add_pi_fluents_to_file(std::string filename);

  GraphNode *dummy_goal() {
    return m_dummy_goal;
  }
  GraphNode *m_dummy_goal;

  GraphNode *dummy_pc() {
    return m_dummy_pc;
  }
  GraphNode *m_dummy_pc;

  ADROperator *end_operator() {
    return m_end_operator;
  }
  ADROperator *m_end_operator;

  GraphRelaxedPlan m_grp;

  bool size_bound_reached() {
    return ((num_pi_fluents() >= max_num_pi_fluents()) ||
            (current_x() >= problem_size_bound()));
  }

  bool time_bound_reached() {
    static Timer timer;
    return (timer() > learning_time_bound());
  }

  std::vector<std::vector<GraphNode *> > &nodes() {
    return m_nodes;
  }
  const std::vector<std::vector<GraphNode *> > &nodes() const {
    return m_nodes;
  }
  std::vector<std::vector<GraphNode *> > m_nodes;

  std::vector<ADROperator *> m_adr_operators;
  const std::vector<ADROperator *> &adr_ops() const {
    return m_adr_operators;
  }
  std::vector<ADROperator *> &adr_ops() {
    return m_adr_operators;
  }

  AdaptiveQueue<GraphNode *> queue;

  GraphNode *get_graph_node(const FluentSet &fluents);

  virtual void initialize() {
    initialize(learn_on_i);
  }
  void initialize(bool do_learning);
  virtual int compute_heuristic(const State &state);

  void setup_exploration_queue_state(const State &state);
  void enqueue_if_necessary(GraphNode *fluent, int cost, ADROperator *sup);
  void hadd_relaxed_exploration();
  void hmax_relaxed_exploration();
  void hmax_relaxed_exploration_get_reachable(std::vector<GraphNode *> &reached);
  void get_initially_true(const State &state,
                          std::vector<GraphNode *> &initially_true);
  void show_queue_contents();

  // LM cut stuff
  void mark_goal_plateau(GraphNode *, int);
  void mark_zones(const State &state, std::vector<ADROperator *> &cut);
  void hmax_relaxed_exploration_incremental(std::vector<ADROperator *>
                                            &cost_reduced_effs);
  void collect_hmax_paths(GraphNode *gn,
                          std::set<ADROperator *> &path);
  void collect_hmax_paths(ADROperator *op,
                          std::set<ADROperator *> &path);
  int compute_lmcut(const State &state);

  int mark_preferred_and_compute_multiset_plan(MultisetPlan &p);

  int compute_graph_plan(GraphRelaxedPlan &p);

  int get_plan_cost(std::list<std::pair<ADROperator *, int> > &plan) {
    int h = 0;
    for (std::list<std::pair<ADROperator *, int> >::iterator it = plan.begin();
         it != plan.end(); it++) {
      h += (it->second * it->first->base_cost);
    }
    return h;
  }

  bool recursive_multiset_plan(GraphNode *fluent);

  void recursive_graph_plan(ADROperator *act, GraphRelaxedPlan &global_plan,
                            int x);

  bool any_cycles();
  bool check_cycle(GraphNode *gn);
  void getDependencies(GraphNode *gn, std::set<GraphNode *> &deps);
  void getDependencies(ADROperator *adr_op, std::vector<GraphNode *> &deps);

  void get_non_dominated_pcs(GRPNode &grp_node, std::vector<GraphNode *> &pcs);

  void collect_sequential_conflicts(const std::vector<GraphNode *> &pcs,
                                    std::vector<std::vector<GraphNode *> > accumulated_pcs,
                                    std::vector<GraphNode *> &accumulated_targets,
                                    std::vector<std::set<FluentSet> > &candidates,
                                    int max_priority);

  void collect_parallel_conflicts(const std::vector<GraphNode *> &pcs,
                                  GraphRelaxedPlan &grp,
                                  std::set<std::pair<ADROperator *, Fluent> > &deleted,
                                  std::set<std::pair<ADROperator *, Fluent> > &all_pcs,
                                  std::vector<std::set<FluentSet> > &candidates);

  void collect_zero_priority_sequential_conflicts(GraphRelaxedPlan &grp,
                                                  std::vector<std::set<FluentSet> > &candidates);

  //  void iterative_conflict_finder(GraphRelaxedPlan &grp);

  void simple_method(std::vector<GraphNode *> &pcs,
                     std::vector<std::set<FluentSet> > &candidates);
  void collect_overlaps(std::vector<GraphNode *> &pcs,
                        std::vector<std::set<FluentSet> > &candidates);

  void clear_plan_computed();

  // output stuff

  void print_operators();
  void print_operator(const ADROperator *op);
  void print_augmentation_data();
  void print_supporter(GraphNode *gn);
  void print_supporters();
  void recursive_print_supporters(GraphNode *, int);
  void recursive_print_supporters();

  GraphRelaxedPlan &get_relaxed_plan()
    {
      return m_grp;
    }
};

#endif
