
#ifndef ADR_HEURISTIC_REFINER_H
#define ADR_HEURISTIC_REFINER_H

#include "augmented_delete_relaxation.h"

#include "../heuristic_refiner.h"
#include "nogoods.h"

#include <ctime>
#include <unordered_set>
#include <vector>
#include <set>

class StateID;

struct DeadEndIdentStatistics {
private:
  inline double get_elapsed(const clock_t &t_start) const
  {
    clock_t t_end = clock();
    return (double)(t_end - t_start) / CLOCKS_PER_SEC;
  }

  clock_t _start;
  clock_t _start_i;

  size_t num_checks;
  size_t num_hc_evals;
  size_t num_hc_des;
  size_t num_nogoods;
  size_t memory_nogoods;

  size_t num_refinements_c;
  size_t num_updates_c;

  size_t num_presarch_nogoods;
  size_t num_presearch_refinements_c;

  double time_learn_nogood;
  double time_learn_c;

  double time_check_nogood;
  double time_check_hc;

  double time_initialize;

public:
DeadEndIdentStatistics()
: _start(0),
    num_checks(0),
    num_hc_evals(0),
    num_hc_des(0),
    num_nogoods(0),
    num_refinements_c(0),
    num_updates_c(0),
    num_presarch_nogoods(0),
    num_presearch_refinements_c(0),
    time_learn_nogood(0),
    time_learn_c(0),
    time_check_nogood(0),
    time_check_hc(0),
    time_initialize(0)
  {}

  void start_learning_nogood()
  {
    _start = clock();
  }

  void end_learning_nogood(const NoGoods *ng)
  {
    time_learn_nogood += get_elapsed(_start);
    if (ng) {
      num_nogoods = ng->size();
      memory_nogoods = ng->memory();
    }
  }

  void start_learning_c()
  {
    _start = clock();
    num_refinements_c++;
  }

  void end_learning_c(bool res = true)
  {
    if (res) {
      num_updates_c++;
    }
    time_learn_c += get_elapsed(_start);
  }

  void start_check_nogood()
  {
    num_checks++;
    _start = clock();
  }

  void end_check_nogood()
  {
    time_check_nogood += get_elapsed(_start);
  }

  void start_check_hc()
  {
    num_hc_evals++;
    _start = clock();
  }

  void end_check_hc(bool hit)
  {
    if (_start > 0) {
      time_check_hc += get_elapsed(_start);
    }
    if (hit) {
      num_hc_des++;
    }
    _start = 0;
  }

  void start_initialization()
  {
    _start_i = clock();
  }

  void end_initialization()
  {
    time_initialize = get_elapsed(_start_i);
    num_presearch_refinements_c = num_refinements_c;
    num_presarch_nogoods = num_nogoods;
  }

  void dump(std::ostream& out) const
  {
    out << "Number of dead end ident checks: " << num_checks
        << std::endl;
    out << "Number of hc checks: " << num_hc_evals
        << std::endl;
    out << "Number of hc des: " << num_hc_des << std::endl;
    out << "Number of hc refinements: " << num_refinements_c
        << std::endl;
    out << "Number of hc updates: " << num_updates_c
        << std::endl;
    out << "Number of nogoods: " << num_nogoods
        << std::endl;
    out << "Approx. memory usage of nogoods: " << memory_nogoods
        << std::endl;
    out << "Number of pre-search refinements: "
        << num_presearch_refinements_c << std::endl;
    out << "Number of pre-search nogoods: "
        << num_presarch_nogoods << std::endl;
    out << "Time spent on checking nogoods: " << time_check_nogood << "s"
        << std::endl;
    out << "Time spent on computing hc: " << time_check_hc << "s"
        << std::endl;
    out << "Time spent on learning nogoods: " << time_learn_nogood << "s"
        << std::endl;
    out << "Time spent on refining hc: " << time_learn_c << "s"
        << std::endl;
    out << "Time spent on pre-search learning: "
        << time_initialize << "s" << std::endl;
  }

  void reset()
  {
    _start = 0;
    num_checks = 0;
    num_hc_evals = 0;
    num_hc_des = 0;
    num_nogoods = 0;
    num_refinements_c = 0;
    num_updates_c = 0;
    num_presarch_nogoods = 0;
    num_presearch_refinements_c = 0;
    time_learn_nogood = 0;
    time_learn_c = 0;
    time_check_nogood = 0;
    time_check_hc = 0;
    time_initialize = 0;
  }
};

class ADRUHeuristic;

class ADRRefinement : public HeuristicRefiner, public Heuristic
{
  friend class ADRUHeuristic;
 protected:
  const bool c_eval_hc;
  const bool c_reeval_hc;
  const float c_offline_refinement_threshold;

  DeadEndIdentStatistics _stats;

  AugmentedDeleteRelaxation *pic;
  NoGoods *nogoods;

  std::vector<const Operator *> m_plan;

  bool project_var(const State &state, const std::set<int> &vars, int var, int orig_val);
  bool check_nogoods(const State &state);
  bool learn_nogoods(const State &state);
  bool learn_nogoods_wrapper(const State &state);

  virtual void initialize();
  virtual int compute_heuristic(const State &state);
 public:
  ADRRefinement(const Options &opts);

  virtual RefinementResult learn_unrecognized_dead_end(const State &state);
  virtual RefinementResult learn_unrecognized_dead_ends(
                                                        const std::vector<State> &root_component,
                                                        const std::unordered_set<StateID> &recognized_neighbors);
  virtual void learn_recognized_dead_ends(const std::vector<State> &dead_ends);
  virtual void learn_recognized_dead_end(const State &dead_end);

  virtual Heuristic *get_heuristic() { return this; }

  virtual void statistics() { _stats.dump(std::cout); pic->dump_statistics(std::cout); }

  virtual void reevaluate(const State &state);

  virtual void get_partial_plan(std::vector<const Operator *> &plan) const {
    plan.insert(plan.end(), m_plan.begin(), m_plan.end());
  }

  virtual void refine_offline();

  static void add_options_to_parser(OptionParser &parser);
};

//// HACK!
//class ADRUHeuristic : public Heuristic {
//protected:
//    ADRRefinement *h;
//    virtual void initialize() { h->initialize(); }
//    virtual int compute_heuristic(const State &state) { return h->compute_heuristic(state); }
//public:
//    ADRUHeuristic(const Options &opts);
//    virtual void reevaluate(const State &state);
//};

#endif
