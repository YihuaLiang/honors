
#ifndef TARJAN_DFS_H
#define TARJAN_DFS_H

#include "../search_engine.h"
#include "../search_progress.h"
#include "tarjan_search_space.h"
#include "tarjan_open_list.h"

#include "../state_id.h"
#include "../operator.h"
#include "../state.h"
#include "../heuristic.h"

#include <vector>
#include <set>

class Heuristic;
class PruningMethod;
class HeuristicRefiner;

namespace tarjan_dfs {
  class TarjanDFS : public SearchEngine {
  public:
    std::vector<HeuristicRefiner *> m_heuristic_refiner;

    const bool c_unsath_new;
    const bool c_unsath_new_full;
    const bool c_unsath_open;
    const bool c_unsath_open_full;
    const bool c_unsath_bwprop;
    const bool c_unsath_bwprop_full;
    const bool c_unsath_search;
    const bool c_refine_initial;
    const bool c_unsath_use_plan;

    const bool c_keep_u_consistent;

    const bool c_delete_states;

    bool c_unsath_compute_recognized_neighbors;
  
    SearchSpace m_search_space;
    SearchProgress m_search_progress;
    std::vector<StateID> m_stack;
    std::vector<unsigned> m_recognized_neighbors_offset;
    std::vector<StateID> m_recognized_neighbors;
    unsigned m_current_id;

    PruningMethod *m_pruning_method;

    std::vector<Heuristic *> m_heuristics;
    std::vector<Heuristic *> m_preferred;
    std::set<Heuristic *> m_underlying_heuristics;
    Heuristic *m_cached_h;   

    OpenListFactory *m_factory;
    size_t m_open_states;
    bool m_unsath_refine;

    std::vector<const Operator *> m_plan;

    bool call_u_refinement() const;

    Heuristic *check_dead_end(const State &state, bool full);
    bool trigger_refiner(const State &state, bool &success);
    bool evaluate(const State &state, bool &u);
    void get_preferred_operators(const State &state, std::set<const Operator *> &result);

    enum RecursionResult {
      R_SOLVED = 0,
      R_FAILED = 1,
      R_UPDATED = 2,
    };
    RecursionResult recursive_depth_first_search(const State &state, size_t min_delete);

    virtual void initialize();
    virtual SearchStatus step();

    size_t m_next_states_print;
    const size_t m_next_state_print_multiplyer;
    int m_smallest_h;
    int m_largest_depth;
    int m_current_depth;
    void print_progress_information() const;
    void progress_information();
  public:
    TarjanDFS(const Options &opts);
    virtual void statistics() const;
    static void add_options_to_parser(OptionParser &parser);
  };
}

#endif
