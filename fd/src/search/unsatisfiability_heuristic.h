
#ifndef UNSATISFIABILITY_HEURISTIC_H
#define UNSATISFIABILITY_HEURISTIC_H

#include "heuristic.h"

class UnsatisfiabilityHeuristic : public Heuristic {
 protected:
  Heuristic *h;
  virtual int compute_heuristic(const State &state);
  virtual void initialize();
 public:
  UnsatisfiabilityHeuristic(const Options &opts);
};

#endif
