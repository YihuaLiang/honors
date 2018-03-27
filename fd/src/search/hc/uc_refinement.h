
#ifndef UC_REFINEMENT_H
#define UC_REFINEMENT_H

#include "uc_heuristic.h"
#include "../heuristic_refiner.h"

#include "../option_parser.h"
#include "../utilities.h"

struct UCRefinementStatistics {
  clock_t _t;

  size_t num_all_refinements;
  size_t num_real_refinements;
  size_t size_all_component;
  size_t size_real_component;
  double t_all_refinements;
  double t_real_refinements;

UCRefinementStatistics() :
  num_all_refinements(0),
    num_real_refinements(0),
    size_all_component(0),
    size_real_component(0),
    t_all_refinements(0),
    t_real_refinements(0)
  {}

  void start() {
    _t = clock();
  }

  void end(double &res) {
    res += ((double) (clock() - _t)) / ((double) CLOCKS_PER_SEC);
  }

  void dump() const;
};


class UCRefinement : public HeuristicRefiner {
 protected:
  UCRefinementStatistics m_statistics;
  UCHeuristic *uc;// it has uc, so it could attach the cost from uc??

  virtual RefinementResult refine(const State &/*state*/) { return FAILED; }
  virtual RefinementResult refine(
                                  const std::vector<State> &/*root_component*/,
                                  const std::unordered_set<StateID> &/*recognized_neighbors*/) { return FAILED; }
  //reload
  virtual RefinementResult refine(const State &/*state*/, int ) { return FAILED; }
  virtual RefinementResult refine(
                                  const std::vector<State> &/*root_component*/,
                                  const std::unordered_set<StateID> &/*recognized_neighbors*/,
                                  int ) { return FAILED; }
 public:
 UCRefinement(const Options &opts) : HeuristicRefiner(opts)
  {
    uc = dynamic_cast<UCHeuristic*>(opts.get<Heuristic*>("uc"));
    if (!uc) {
      exit_with(EXIT_CRITICAL_ERROR);
    }
  }

  virtual RefinementResult learn_unrecognized_dead_end(const State &state) {
    m_statistics.num_all_refinements++;
    m_statistics.size_all_component += 1;
    m_statistics.start();
    RefinementResult res = refine(state);
    if (res != FAILED) {
      if (res != UNCHANGED) {
        m_statistics.num_real_refinements++;
        m_statistics.size_real_component += 1;
        m_statistics.end(m_statistics.t_real_refinements);
      }
      if (res != SOLVED) {
        uc->refine_clauses(state);
        uc->set_dead_end();
#ifndef NDEBUG
        uc->dump_compilation_information();
        uc->evaluate(state);
        assert(uc->is_dead_end());
#endif
      }
    }
    return res;
  }
//reload
  virtual RefinementResult learn_unrecognized_dead_end(const State &state, int g_value) {
    m_statistics.num_all_refinements++;
    m_statistics.size_all_component += 1;
    m_statistics.start();
    RefinementResult res = refine(state, g_value);
    if (res != FAILED) {
      if (res != UNCHANGED) {
        m_statistics.num_real_refinements++;
        m_statistics.size_real_component += 1;
        m_statistics.end(m_statistics.t_real_refinements);
      }
      if (res != SOLVED) {
        uc->refine_clauses(state);
        uc->set_dead_end();
#ifndef NDEBUG
        uc->dump_compilation_information();
        uc->evaluate(state, g_value);
        assert(uc->is_dead_end());
#endif
      }
    }
    return res;
  }
  virtual RefinementResult learn_unrecognized_dead_ends(
                                                        const std::vector<State> &root_component,
                                                        const std::unordered_set<StateID> &recognized_neighbors) {
    m_statistics.num_all_refinements++;
    m_statistics.size_all_component += root_component.size();
    m_statistics.start();
    RefinementResult res = refine(root_component, recognized_neighbors);
    m_statistics.end(m_statistics.t_all_refinements);
    if (res != FAILED) {
      if (res != UNCHANGED) {
        m_statistics.num_real_refinements++;
        m_statistics.size_real_component += root_component.size();
        m_statistics.end(m_statistics.t_real_refinements);
      }
      if (res != SOLVED) {
        uc->refine_clauses(root_component);
        uc->set_dead_end();
#ifndef NDEBUG
        uc->dump_compilation_information();
        for (uint i = 0; i < root_component.size(); i++) {
          uc->evaluate(root_component[i]);
          assert(uc->is_dead_end());
        }
#endif
      }
    }
    return res;
  }
//reload
 virtual RefinementResult learn_unrecognized_dead_ends(
                                                        const std::vector<State> &root_component,
                                                        const std::unordered_set<StateID> &recognized_neighbors,
                                                        int g_value) {
    m_statistics.num_all_refinements++;
    m_statistics.size_all_component += root_component.size();
    m_statistics.start();
    RefinementResult res = refine(root_component, recognized_neighbors, g_value);
    m_statistics.end(m_statistics.t_all_refinements);
    if (res != FAILED) {
      if (res != UNCHANGED) {
        m_statistics.num_real_refinements++;
        m_statistics.size_real_component += root_component.size();
        m_statistics.end(m_statistics.t_real_refinements);
      }
      if (res != SOLVED) {
        uc->refine_clauses(root_component);
        //uc->refine_clauses(root_component,g_value);
        uc->set_dead_end();
#ifndef NDEBUG
        uc->dump_compilation_information();
        for (uint i = 0; i < root_component.size(); i++) {
          uc->evaluate(root_component[i],g_value);
          assert(uc->is_dead_end());
        }
#endif
      }
    }
    return res;
  }
  virtual void learn_recognized_dead_ends(const std::vector<State> &dead_ends) {
    uc->refine_clauses(dead_ends);
  }

  virtual void learn_recognized_dead_end(const State &dead_end) {
    uc->refine_clauses(dead_end);
  }
//reload
  virtual void learn_recognized_dead_ends(const std::vector<State> &dead_ends, int g_value) {
    uc->refine_clauses(dead_ends, g_value);
  }

  virtual void learn_recognized_dead_end(const State &dead_end, int g_value) {
    uc->refine_clauses(dead_end, g_value);
  }

  virtual bool dead_end_learning_requires_full_component() { return false; }

  virtual bool dead_end_learning_requires_recognized_neighbors() { return false; }

  virtual Heuristic *get_heuristic() { return uc; }

  virtual void statistics() {
    uc->statistics();
    m_statistics.dump();
  }

  static void add_options_to_parser(OptionParser &parser);
  int get_bound(){return uc->bound;}
};

#endif
