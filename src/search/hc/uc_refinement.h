
#ifndef UC_REFINEMENT_H
#define UC_REFINEMENT_H

#include "uc_heuristic.h"
#include "../heuristic_refiner.h"

#include "../option_parser.h"
#include "../utilities.h"
// some variables and statistics function used in both refinement methods
struct UCRefinementStatistics {
  clock_t _t;
  // this function is used for final experiment data 
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
  UCRefinementStatistics m_statistics;// for the result
  UCHeuristic *uc;
  // refine will be inherited by the pcr and nr algorithms
  virtual RefinementResult refine(const State &/*state*/, int cost_bound) { return FAILED; }
  virtual RefinementResult refine(
                                  const std::vector<State> &/*root_component*/,
                                  const std::unordered_set<StateID> &/*recognized_neighbors*/,
                                  int cost_bound) { return FAILED; }
 public:
 UCRefinement(const Options &opts) : HeuristicRefiner(opts)//Initialized
  {
    uc = dynamic_cast<UCHeuristic*>(opts.get<Heuristic*>("uc"));// looks like calculate the heuristic
    if (!uc) {
      exit_with(EXIT_CRITICAL_ERROR);
    }
  }
  
  //a high level function, run and call the refine
  //This one is for PCR to inherit
  //this is the first place to call the 
  virtual RefinementResult learn_unrecognized_dead_end(const State &state, int cost_bound) {
    m_statistics.num_all_refinements++;
    m_statistics.size_all_component += 1;
    m_statistics.start();// control working result
    RefinementResult res = refine(state,cost_bound);
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
        uc->evaluate(state,int cost_bound);
        assert(uc->is_dead_end());
#endif
      }
    }
    return res;
  }
  //This one is for NR to inherit
  virtual RefinementResult learn_unrecognized_dead_ends(
                                                        const std::vector<State> &root_component,
                                                        const std::unordered_set<StateID> &recognized_neighbors,
                                                        int cost_bound) {
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
 // This is used for constructing clause
  virtual void learn_recognized_dead_ends(const std::vector<State> &dead_ends) {
    uc->refine_clauses(dead_ends);
  }

  virtual void learn_recognized_dead_end(const State &dead_end) {
    uc->refine_clauses(dead_end);
  }

  virtual bool dead_end_learning_requires_full_component() { return false; }

  virtual bool dead_end_learning_requires_recognized_neighbors() { return false; }

  virtual Heuristic *get_heuristic() { return uc; } // take the heuristic out

  virtual void statistics() {
    uc->statistics();
    m_statistics.dump();
  }

  static void add_options_to_parser(OptionParser &parser);
};

#endif
