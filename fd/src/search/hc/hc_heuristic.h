#ifndef HC_HEURISTIC_H
#define HC_HEURISTIC_H

#include "../heuristic.h"
#include "../priority_queue.h"
#include "../segmented_vector.h"

#include <vector>
#include <set>
#include <unordered_set>
#include <string.h>
typedef std::pair<int, int> Fact;
typedef std::set<Fact> Fluent;

class Operator;
class State;

/* relaxation_heurstic extended by conjunctions */
/* implementation based on existing relaxation_heuristic.h */

/* Currently this heuristic is only used for dead end detection,
 * i.e., it will always start with C = \emptyset. So, if no one is
 * going to insert any new conjunction while searching, this heuristic
 * is an (possibly ineffecient) computation of hmax */

struct StripsAction {
  /*
   * static data -- computed in initialization of heuristic,
   *      does not change afterwards
   *  (STRIPS encoding of fast downward operator)
   *
   */
  /* NOTE: We cannot store pointers to the facts of the relaxation heuristic
   *  directly, as the set of conjunctions (where all facts are stored as
   *  well) might change over time, i.e., invalidating the stored pointers.
   *  This happens for example if the set of conjunctions is represented
   *  as vector.
   */
  /* operator_no -- index into fast downward operators */
  int operator_no; // -1 for axioms; index into g_operators otherwise
  /* precondition -- contains indices into the conjunction set of the
   *                  relaxation heuristic (only facts) */
  Fluent precondition;
  /* add_effect -- add effect of this actions, indices into the conjunction
   *              set of the relaxation heuristic */
  Fluent add_effect;
  /* del_effect -- del effect of this actions, indices into the conjunction
   *              set of the relaxation heuristic */
  Fluent del_effect;
  /* base_cost -- plain cost of this action, i.e., not including the cost of
   *              its precondition */
  int base_cost;

  unsigned aid;

  StripsAction() {}
StripsAction(int opnum, const Fluent &pre, const Fluent &add, const Fluent &del,
             int cost, unsigned id)
: operator_no(opnum), precondition(pre), add_effect(add), del_effect(del),
    base_cost(cost), aid(id) {}
  StripsAction(const StripsAction &act)
  : StripsAction(act.operator_no, act.precondition, act.add_effect, act.del_effect, act.base_cost, act.aid) {}
};

struct Conjunction;

struct ActionEffectCounter {
  /*
   * static data -- set during initialization
   */
  unsigned action_id; // to the action -- operator no to the cost??
  unsigned id;
  //int action; // id of corresponding strips action
  Conjunction *effect; // id of conjunction
  int base_cost; // base action cost
  int preconditions;

  /*
   * dynamic data -- these variables will be updated while computing hmax
   */
  /* unsatisfied_precondition -- represents the number of facts in the
   *                              precondition that have not been achieved
   *                              so far */
  int unsatisfied_preconditions;
  /* cost -- represents the maximal cost of any fact in precondition
   *          that is not infinity */
  int cost;

  /* for LM-cut computation */
  //int max_precondition;
  // ff
  //int difficulty;

  ActionEffectCounter &operator=(const ActionEffectCounter &cop)
  {
    action_id = cop.action_id;
    id = cop.id;
    effect = cop.effect;
    base_cost = cop.base_cost;
    preconditions = cop.preconditions;
    unsatisfied_preconditions = cop.unsatisfied_preconditions;
    cost = cop.cost;
    return *this;
  }

ActionEffectCounter() : action_id(-1), id(-1), effect(NULL) {}
ActionEffectCounter(unsigned action_id, unsigned id, Conjunction *fact, int base_cost)
: action_id(action_id), id(id), base_cost(base_cost), preconditions(0) {
  effect = fact;
}
};

struct Conjunction {
  /*
   * static data -- STRIPS encoding of a conjunction in fast downwards
   *                  planning task
   */
  /* NOTE (again): We cannot store direct pointers to strips actions
   *  stored inside the relaxation heuristic, as the set of conjunctions
   *  might change over time, meaning that new actions may be introduced
   *  invalidating existing pointers.
   */
  /* id -- reference to itself */
  unsigned id;
  /* is_goal -- set to true if this conjunction is part of the goal */
  //bool is_goal;
  /* triggered_counters -- all action-effect counters whose precondition
   *                      contains this conjunctions (might change if
   *                      conjunction set is updated, i.e., new actions
   *                      are introduced)*/
  std::vector<ActionEffectCounter *> triggered_counters;
  /* fluent -- represents a set of facts (note that a fact in STRIPS
   *              is just a pair <var, val> of the planning task of
   *              fast downward) */
  //Fluent fluent;
  int fluent_size;

  /*
   * dynamic data -- these variables will be updated while computing hmax
   */
  /* cost -- stores the (intermediate) result in hmax computation */
  int cost;

  // for ff
  //std::vector<int> supporters;

  Conjunction &operator=(Conjunction &cop)
  {
    id = cop.id;
    fluent_size = cop.fluent_size;
    cost = cop.cost;
    triggered_counters.swap(cop.triggered_counters);
    return *this;
  }

Conjunction() : id(-1), fluent_size(-1) {}
Conjunction(unsigned id, int fluent_size)
: id(id), fluent_size(fluent_size) {
  //is_goal = false; // default: is not contained in goal
  cost = -1; // set to infinity
}

  inline bool is_achieved() const {
    return cost >= 0;
  }

  inline bool check_and_update(int x, ActionEffectCounter * /*counter*/) {
    if (cost == -1 || cost > x) {
      //supporters.clear();
      //if (counter >= 0) {
      //    supporters.push_back(counter);
      //}
      cost = x;
      return true;
    }
    //if (cost == x && counter >= 0) {
    //    supporters.push_back(counter);
    //}
    return false;
  }

  inline void clear() {
    cost = -1;
    //supporters.clear();
  }

};

struct Conflict {
private:
  Fluent fluent;
public:
  Conflict() {
  }
  Conflict(const Fluent &pi) {
    fluent.insert(pi.begin(), pi.end());
  }
  Conflict(const Conflict &c) {
    merge(c);
  }
  //void merge(const Conjunction &conj);
  void merge(const Fact &fact);
  void merge(const Conflict &conj);
  void merge(const Fluent &fluent);
  const Fluent &get_fluent() const {
    return fluent;
  }
  Fluent &get_fluent() {
    return fluent;
  }
  bool operator==(const Conflict &conj) const {
    return fluent == conj.fluent;
  }
  bool operator<(const Conflict &conj) const {
    return fluent < conj.fluent;
  }
};

// a little helper that should IMHO be standardized
template<typename T>
std::size_t make_hash(const T &v)
{
  return std::hash<T>()(v);
}

// adapted from boost::hash_combine
void My_hash_combine(std::size_t &h, const std::size_t &v);

// hash any container
template<typename T>
struct hash_container {
  size_t operator()(const T &v) const {
    size_t h = 0;
    for (const auto & e : v) {
      My_hash_combine(h, make_hash(e));
    }
    return h;
  }
};

namespace std
{
  // support for pair<T,U> if T and U can be hashed
  template<typename T, typename U>
    struct hash<pair<T, U>> {
    size_t operator()(const pair<T, U> &v) const {
      size_t h = make_hash(v.first);
      My_hash_combine(h, make_hash(v.second));
      return h;
    }
  };

  // support for vector<T> if T is hashable
  // (the T... is a required trick if the vector has a non-standard allocator)
  template<typename... T>
    struct hash<set<T...>> : hash_container<set<T...>> {};
}

class HCHeuristic : public Heuristic
{
 protected:
  const bool c_dump_conjunction_set;
  const bool c_minimize_counter_set;
  const bool c_dual_task;
  //struct ConjunctionHasher {
  //private:
  //    ConjunctionMaxHeuristic *obj;
  //    std::hash<Fluent> _hasher;
  //public:
  //    ConjunctionHasher(ConjunctionMaxHeuristic *obj)
  //        : obj(obj) {}
  //    size_t operator()(const int &i) const {
  //        return _hasher(obj->_fluents[i]);
  //    }
  //};
  class ConjunctionComparator
  {
  private:
    HCHeuristic *obj;
  public:
  ConjunctionComparator(HCHeuristic *obj) : obj(obj) {}
    bool operator()(const unsigned &i, const unsigned &j) const {
      return obj->_fluents[i] < obj->_fluents[j];
    }
  };

  //ConjunctionHasher _hasher;
  ConjunctionComparator _comp;
  std::set<unsigned, ConjunctionComparator> _unique_conjunctions;
  //std::unordered_set<int, ConjunctionHasher, ConjunctionComparator>
  //    _unique_conjunctions;

  // required for mapping of <var, val> to conjunction_id
  std::vector<unsigned> variable_offset;
  // first index of a conjunction
  unsigned conjunction_offset;

  std::vector<Fluent> _fluents;
  std::vector<StripsAction> actions;

  //std::vector<Conjunction*> conjunctions;
  //std::vector<ActionEffectCounter*> counters;
  SegmentedVector<Conjunction> conjunctions;//_fluents is C 
  SegmentedVector<ActionEffectCounter> counters;//counters number should be small or equal to action
  //std::vector<Conjunction*> goal_conjunctions;
  unsigned m_true_id;
  unsigned m_goal_id;
  unsigned m_goal_counter;

  std::vector<std::vector<unsigned> > facts_to_add;
  std::vector<std::vector<unsigned> > facts_to_del;

  int _num_counters;

  bool early_termination;
  AdaptiveQueue<Conjunction *> queue;

  std::vector<std::vector<unsigned> > facts_conjunctions_relation;

  unsigned current_fluents;
  unsigned max_number_fluents;
  unsigned max_num_counters;
  float max_ratio_repr_counters;

  virtual void add_counter_precondition(ActionEffectCounter *counter,
                                        Conjunction *conj);
  bool prune_counter_precondition;
  std::vector<std::vector<unsigned> > counter_preconditions;
  std::vector<std::vector<ActionEffectCounter *> > facts_to_counters;

  std::vector<std::vector<unsigned> > conjunction_achievers;

  std::vector<std::vector<unsigned> > m_results_in_mutex;

  bool hard_size_limit;

  int _m;

  std::string __load_from_file;

  const bool c_update_mutex;
  //std::vector<std::vector<unsigned> > m_fact_to_mutex;
  //std::vector<int> m_mutex_size;
  //std::vector<Fluent> m_mutex;
  //std::vector<int> m_mutex_subset;

  /* functions for debugging perposes */

  /* function for computation of h^max */
  //virtual void update_counter_cost(ActionEffectCounter &counter,
  //                                 const Conjunction &pre) const;
  void enqueue_if_necessary(Conjunction *conj, ActionEffectCounter *counter,
                            int cost);
  void clear_exploration_queue();
  void setup_exploration_queue_state(const State &state);
  void relaxed_exploration();

  void create_unit_conjunction(int var, int val,
                               Fluent &fluent) const;
  void compile_strips();
  //void create_inverse_actions_handle_no_precondition(
  //        unsigned op_num,
  //        unsigned &strips_action_id,
  //        const Fluent &pre,
  //        Fluent add,
  //        Fluent del,
  //        int cost,
  //        const std::vector<std::pair<int, int> > &no_prec,
  //        unsigned i
  //        );

  //virtual int choose_conflicting_fact(const Fluent &goal,
  //                                    const Fluent &state) const;
  void extract_all_conjunctions(const Fluent &fluent,
                                std::vector<unsigned> &conjs_unr,
                                std::vector<unsigned> &conjs, int cost)
    const;
  void extract_all_conjunctions(const Fluent &fluent,
                                std::vector<unsigned> &conjs);
  bool add_subsets_m(const Fluent &base, bool is_goal, unsigned to_go);

  void update_triggered_counters(unsigned conj_id);
  void create_counter(unsigned action, unsigned conj_id, int cost, const Fluent &precondition);
  template<typename T>
    void create_counter(unsigned counter_id, const T &precondition);

  bool fluent_in_state(const Fluent &pi, const State &state) const;

  virtual void initialize();
  virtual int compute_heuristic(const State &state);
  //reload
  virtual int compute_heuristic(const State &state, int g_value);
  
  template<typename V1, typename V2>
    bool update_c_v(const V1 &new_conjunctions, const V2 &add)
  {
    if (new_conjunctions.size() == 0) {
      return true;
    }

    if (exceeded_size_limit()) {
      return false;
    }

    /* Instead of recompiling the whole task, we simply extend the existing
     * compiliation by introducing the new conjunctions, updating the
     * precondition of the existing actions, and adding new actions
     * accordingly */

    bool added_all_conjunctions = true;
    //conjunctions.reserve(conjunctions.size() + new_conjunctions.size());
    for (unsigned i = 0; i < new_conjunctions.size(); i++) {
      if (add[i]) {
        if (hard_size_limit && exceeded_size_limit()) {
          added_all_conjunctions = false;
          break;
        }
        add_conflict(new_conjunctions[i]);
      }
    }

    //if (added_all_conjunctions && c_update_mutex) {
    //  update_mutex();
    //}

    return added_all_conjunctions;
  }
  template<typename V>
    bool update_c_v(const V &new_conjunctions)
    {
      if (new_conjunctions.size() == 0) {
        return true;
      }

      if (exceeded_size_limit()) {
        return false;
      }

      /* Instead of recompiling the whole task, we simply extend the existing
       * compiliation by introducing the new conjunctions, updating the
       * precondition of the existing actions, and adding new actions
       * accordingly */

      bool added_all_conjunctions = true;
      //conjunctions.reserve(conjunctions.size() + new_conjunctions.size());
      for (unsigned i = 0; i < new_conjunctions.size(); i++) {
        if (hard_size_limit && exceeded_size_limit()) {
          added_all_conjunctions = false;
          break;
        }
        add_conflict(new_conjunctions[i]);
      }

      //if (added_all_conjunctions && c_update_mutex) {
      //  update_mutex();
      //}

      return added_all_conjunctions;
    }
 public:
  HCHeuristic(const Options &options);
  HCHeuristic(const HCHeuristic &h);
  
  void add_conflict(const Conflict &conflict);
  int simple_traversal_setup(const State &state,
                             std::vector<unsigned> &exploration);
  //virtual int simple_traversal_setup(const std::vector<State> &state,
  //                                    std::vector<int> &exploration);
  int simple_traversal_setup(const std::vector<std::pair<int, int> > &state,
                             std::vector<unsigned> &exploration);
  int simple_traversal(const State &state);
  //reload
  int simple_traversal(const State &state, int g_value);
  
  int simple_traversal_wrapper(std::vector<unsigned> &exploration, int lvl0);
  //reload
  int simple_traversal_wrapper(std::vector<unsigned> &exploration, int lvl0,int g_value);
  //test whether this is called
  int priority_traversal(const State &state);

  bool update_c(std::set<Conflict> &conjs);
  bool update_c(std::vector<Conflict> &conjs)
  {
    return update_c_v<std::vector<Conflict> >(conjs);
  }
  bool update_c(SegmentedVector<Conflict> &conjs, const std::vector<bool> &add)
  {
    return update_c_v<SegmentedVector<Conflict>, std::vector<bool> >(conjs, add);
  }
  bool update_c(SegmentedVector<Conflict> &conjs)
  {
    return update_c_v<SegmentedVector<Conflict> >(conjs);
  }

  void compute_can_add(std::vector<int> &can_add_fluent,
                       const Fluent &fluent,
                       bool mutex_check = false);

  const StripsAction &get_action(size_t i) const {
    return actions[i];
  }

  bool contains_mutex(const Fluent &f1, const Fluent &f2);
  bool contains_mutex(const Fluent &fluent);
  bool extract_mutex(const Fluent &f1, const Fluent &f2, Conflict &mutex);
  bool extract_mutex(const Fluent &goal, Conflict &mutex);
  bool extract_mutex(const Fluent &goal, Fluent &mutex);
  //void update_mutex();

  void load_conjunctions_from_file(std::string path = "pi_fluents.txt");
  void dump_conjunctions_to_file(std::string path = "pi_fluents.txt") const;
  void dump_conjunctions_to_ostream(std::ostream &out) const;

  void generate_sas_operator(
                             const std::vector<unsigned> &var_ids,
                             const StripsAction &action,
                             const std::vector<bool> &ineff_fixed,
                             const std::vector<bool> &ineff,
                             std::vector<std::string> &operators);
  void dump_compiled_task_to_file(std::string path = "pic.sas");

  Fact get_fact(int var, int val) const;
  unsigned get_fact_id(int var, int val) const;
  unsigned get_fact_id(const Fact &fact) const;
  const Conjunction &get_conjunction(unsigned cid) const {
    assert(cid < conjunctions.size());
    return conjunctions[cid];
  }
  Conjunction &get_conjunction(unsigned cid) {
    assert(cid < conjunctions.size());
    return conjunctions[cid];
  }
  const Fluent &get_fluent(unsigned cid) const {
    return _fluents[cid];
  }
  const std::vector<unsigned> &get_fact_conj_relation(unsigned fid) const {
    return facts_conjunctions_relation[fid];
  }
  size_t num_conjunctions() const {
    return conjunctions.size();
  }

  size_t num_facts() const {
    return conjunction_offset;
  }

  size_t num_counters() const {
    return counters.size();
  }

  const std::vector<unsigned> &get_conjunctions(int var, int val) const {
    return facts_conjunctions_relation[get_fact_id(var, val)];
  }
  const std::vector<unsigned> &get_conjunctions(unsigned fid) const {
    return facts_conjunctions_relation[fid];
  }

  std::set<unsigned>::const_iterator begin() const {
    return _unique_conjunctions.begin();
  }

  std::set<unsigned>::const_iterator end() const {
    return _unique_conjunctions.end();
  }

  const std::vector<unsigned> &adds(unsigned fid) const {
    return facts_to_add[fid] ;
  }

  const std::vector<unsigned> &dels(unsigned fid) const {
    return facts_to_del[fid];
  }

  const std::vector<unsigned> &mutual_exclusive(unsigned fid) const {
    return m_results_in_mutex[fid];
  }

  size_t num_actions() const {
    return actions.size();
  }

  ActionEffectCounter &get_counter(unsigned i) {
    assert(i < counters.size());
    return counters[i];
  }

  bool set_early_termination(bool x) {
    bool old = early_termination;
    early_termination = x;
    return old;
  }

  const std::vector<unsigned> &get_counter_precondition(unsigned counter) const {
    assert(counter < counter_preconditions.size());
    return counter_preconditions[counter];
  }

  const std::vector<unsigned> &get_conjunction_achievers(unsigned conj) const {
    assert(conj < conjunction_achievers.size());
    return conjunction_achievers[conj];
  }

  unsigned get_goal_counter_id() const {
    return m_goal_counter;
  }

  void set_size_limit_ratio(float ratio);
  void reset_size_limit_ratio();
  int get_number_of_conjunctions() const;
  bool reached_max_conjunctions() const;
  bool exceeded_size_limit() const;
  int get_number_of_counters() const;
  double get_counter_ratio() const;
  void dump_facts_pddl(bool breaks = false,
                       bool with_cost = false,
                       std::ostream &out = std::cout) const;
  void dump_c_pddl(bool breaks = false,
                   bool with_id = false,
                   bool with_cost = false,
                   std::ostream &out = std::cout) const;
  std::string get_description() const;

  //virtual void clear_all_conjunctions();

  void dump_compilation_information(std::ostream &out = std::cout)
    const;
  virtual void dump_options(std::ostream &out = std::cout) const;
  virtual void dump_statistics(std::ostream &out = std::cout) const;
  virtual void dump_statistics_json(std::ostream &out = std::cout) const;
  virtual void dump_heuristic_values(std::ostream &out = std::cout) const;

  //void set_store_preconditions(bool _store)
  //{
  //    store_counter_precondition = _store;
  //}

  static void add_options_to_parser(OptionParser &parser);


  bool dump_fact_pddl(int var, int val,
                      std::ostream &out = std::cout,
                      bool sep = false, bool negated = true) const;
  void dump_state_pddl(const State &state,
                       std::ostream &out = std::cout) const;
  void dump_fluent_pddl(const Fluent &pi,
                        std::ostream &out = std::cout,
                        bool lala = true) const;
  void dump_conjunctions_pddl(int from, int to,
                              bool breakes = false,
                              bool with_cost = false,
                              bool with_id = true,
                              std::ostream &out = std::cout) const;
  void dump_fluent_set_pddl(const std::vector<unsigned> &fluent_ids,
                            std::ostream &out = std::cout, int from = 0, int to = -1) const;

  void dump_counter(size_t counter_id, std::ostream &out = std::cout) const;

  void remove_conjunctions(const std::vector<unsigned> &ids);

  float get_counter_ratio_limit() const { return max_ratio_repr_counters; }
  
  static void add_default_options(Options &opts);
};

#endif

