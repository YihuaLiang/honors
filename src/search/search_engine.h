#ifndef SEARCH_ENGINE_H
#define SEARCH_ENGINE_H

#include <vector>

class Heuristic;
class OptionParser;
class Options;

#include "operator.h"
#include "operator_cost.h"

#include <vector>
class State;

class SearchEngine {
public:
    typedef std::vector<const Operator *> Plan;
    enum SearchStatus {FAILED = 0, SOLVED = 1, IN_PROGRESS = 2};
protected:
    bool solved;//equal to solution found
    Plan plan;
    OperatorCost cost_type;
    // add a bound -- now only add the bound
    int bound;
    virtual void initialize() {}
    virtual SearchStatus step() = 0;

    void set_plan(const Plan &plan);
    int get_adjusted_cost(const Operator &op) const;
    OperatorCost get_operator_cost() const { return cost_type; }
public:
    //related to the cost bound
    void set_bound(int b) {bound = b; }
    int get_bound() {return bound; }

    SearchEngine(const Options &opts);
    virtual ~SearchEngine();
    void save_plan_if_necessary() const;
    virtual void statistics() const;
    bool found_solution() const;
    const Plan &get_plan() const;
    void search();
    static void add_options_to_parser(OptionParser &parser);
};

#endif
