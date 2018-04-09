#ifndef SEARCH_SPACE_H
#define SEARCH_SPACE_H

#include "state.h"
#include "operator_cost.h"
#include "per_state_information.h"
#include "search_node_info.h"

#include <vector>
#include <unordered_set>

#include "segmented_vector.h"

class Operator;
class State;


class SearchNode {
public:
    StateID state_id;
    SearchNodeInfo &info;
    OperatorCost cost_type;

    SearchNode(StateID state_id_, SearchNodeInfo &info_,
               OperatorCost cost_type_);

    StateID get_state_id() const {
        return state_id;
    }
    State get_state() const;

    bool is_new() const;
    bool is_open() const;
    bool is_closed(unsigned restart = 1) const;
    bool is_dead_end() const;

    int get_g() const;
    int get_h() const;

    void open_initial(int h);
    void open(int h, const SearchNode &parent_node,
              const Operator *parent_op);
    void reopen(const SearchNode &parent_node,
                const Operator *parent_op);
    void update_parent(const SearchNode &parent_node,
                       const Operator *parent_op);
    void increase_h(int h);
    void close(unsigned restart = 1);
    void mark_as_dead_end();

    void dump() const;

    friend bool operator<(const SearchNode node1, const SearchNode node2);
    SearchNode& operator = (const SearchNode &node1);

    bool is_flagged() const;
    void set_flag(bool flag = true);
    bool is_u_flagged() const; 
    void set_u_flag(bool flag = true); //set flag true -- means u recognized??
    unsigned get_revision() const;
    void set_revision(unsigned x);
    bool add_parent(const State &state);
    const std::unordered_set<StateID> &get_all_parents() const;
    const std::vector<StateID> &get_all_successors() const;
    std::vector<StateID> &get_all_successors();
};

bool operator > (const SearchNode node1, const SearchNode node2);

class SearchSpace {
    SegmentedVector<SearchNodeInfo> m_infos;

    SearchNodeInfo &lookup(unsigned id);
    SearchNodeInfo &lookup(const State &state);

    OperatorCost cost_type;
public:
    SearchSpace(OperatorCost cost_type_);
    SearchNode get_node(const State &state);
    void trace_path(const State &goal_state,
                    std::vector<const Operator *> &path);

    void dump() const;
    void statistics() const;
    void clear();
};

#endif
