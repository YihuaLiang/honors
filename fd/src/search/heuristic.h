#ifndef HEURISTIC_H
#define HEURISTIC_H

#include "scalar_evaluator.h"
#include "operator_cost.h"
#include "search_space.h"
#include "search_progress.h"
#include "state_id.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class Operator;
class State;
class OptionParser;
class Options;

class Heuristic : public ScalarEvaluator {
protected:
    enum {NOT_INITIALIZED = -2};
    int heuristic;
    int evaluator_value; // usually equal to heuristic but can be different
    // if set with set_evaluator_value which is done if we use precalculated
    // estimates, eg. when re-opening a search node

    std::vector<const Operator *> preferred_operators;
    bool is_unit_cost;
    OperatorCost cost_type;
    virtual void initialize() {}
    virtual int compute_heuristic(const State &state) = 0;
    //reload
    virtual int compute_heuristic(const State &, int ){return 0;}
    //provide basic function line 1&2
    // Usage note: It's OK to set the same operator as preferred
    // multiple times -- it will still only appear in the list of
    // preferred operators for this heuristic once.
    void set_preferred(const Operator *op);
    int get_adjusted_cost(const Operator &op) const;
    bool is_unit_cost_problem() const {
        return is_unit_cost;
    }
public:
    enum {DEAD_END = -1};
    Heuristic(const Options &options);
    Heuristic(const Heuristic &heuristic);
    virtual ~Heuristic();
    int bound;
    std::string h_name = "Heuristic";
    void evaluate(const State &state);
    //reload
    void evaluate(const State &state, int g_value);
    
    virtual void reevaluate(const State &) { heuristic = 0; evaluator_value = 0; }
    //reload
    virtual void reevaluate(const State &, int& ){heuristic = 0; evaluator_value = 0; }
    bool is_dead_end() const;
    void clear_cache() { heuristic = 0; evaluator_value = 0; }
    int get_heuristic();
    // changed to virtual, so HeuristicProxy can delegate this:
    virtual void get_preferred_operators(std::vector<const Operator *> &result);
    virtual bool dead_ends_are_reliable() const {return true; }
    virtual bool reach_state(const State &parent_state, const Operator &op,
                             const State &state);

    //virtual bool improve_h_dead_end(State main, std::set<State> &states);
    //virtual bool can_improve_h_dead_end(State main, std::set<State> &states,
    //        bool non_indicated_dead_end);

    // for abstract parent ScalarEvaluator
    int get_value() const;
    void evaluate(int g, bool preferred);
    bool dead_end_is_reliable() const;
    void set_evaluator_value(int val);
    void get_involved_heuristics(std::set<Heuristic *> &hset) {hset.insert(this); }
    OperatorCost get_cost_type() const {return cost_type; }

    static void add_options_to_parser(OptionParser &parser);
    static Options default_options();
};

#endif
