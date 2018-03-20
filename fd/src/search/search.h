
#ifndef SEARCH_H
#define SEARCH_H

#include "search_engine.h"

#include "search_space.h"
#include "search_progress.h"
#include "state_id.h"

#include <ctime>
#include <vector>
#include <set>
#include <unordered_set>

class Heuristic;
class HeuristicRefiner;
class OpenSet;
class Options;
class OptionParser;
class State;
class PruningMethod;

class Search : public SearchEngine
{
protected:
    SearchSpace search_space;
    SearchProgress search_progress;

    const bool c_reopen_nodes;

    const bool c_unsath_new_full;
    const bool c_unsath_open_full;
    const bool c_unsath_closed_full;
    const bool c_unsath_bprop_full;

    const bool c_unsath_new;
    const bool c_unsath_open;
    const bool c_unsath_closed;
    const bool c_unsath_bprop;

    const bool c_unsath_search;
    const bool c_unsath_use_plan;

    const bool c_unsath_refine_to_initial_state;

    const bool c_ensure_u_consistency;

    bool c_unsath_compute_recognized_neighbors;

    bool m_unsath_refine;
    bool m_solved;

    PruningMethod *m_pruning_method;
    
    OpenSet *m_open_set;
    std::vector<Heuristic *> m_heuristics;
    std::vector<Heuristic *> m_preferred;
    std::vector<HeuristicRefiner *> m_heuristic_refiner;
    std::set<Heuristic *> m_underlying_heuristics;
    Heuristic *m_cached_h;

    unsigned m_revision;
    size_t m_open_states;

    StateID fetch_next_state();

    virtual Heuristic *check_dead_end(const State &state, bool full);
    bool trigger_refiner(const State &state, bool &success);
    bool evaluate(const State &state, bool &u);
    bool evaluate(SearchNode &node);
    //see which one is better to reload
    //bool evaluate(const State &state, bool &u,int g);

    void get_preferred_operators(const State &state, std::set<const Operator *> &result);

    virtual bool check_goal_and_set_plan(const State &state);

    bool check_and_learn_dead_end(const SearchNode &node);
    bool check_and_learn_dead_end(const SearchNode &node, std::vector<State> &tbh);
    //bool check_and_learn_dead_end(const SearchNode &node, std::vector<State> &tbh, int g);
    bool search_open_state(SearchNode node,
                           std::vector<State> &closed_states,
                           std::unordered_set<StateID> &rn,
                           std::vector<State> &open_states);
    void backward_propagation(const State &state);
    void backward_propagation(std::vector<State> &states);
    void mark_dead_ends(SearchNode node, std::vector<State> &open);
    void mark_dead_ends(std::vector<State> &dead_ends, std::vector<State> &open);

    virtual void initialize();
    virtual SearchStatus step();

    size_t m_next_states_print;
    const size_t m_next_state_print_multiplyer;
    int m_smallest_h;
    int m_largest_g;
    void progress_information();
public:
    Search(const Options &opts);
    SearchProgress get_search_progress() const {return search_progress; }
    virtual void statistics() const;
    static void add_options_to_parser(OptionParser &parser);
};

#endif
