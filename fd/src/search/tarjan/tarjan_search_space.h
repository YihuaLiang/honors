
#ifndef TARJAN_SEARCH_SPACE_H
#define TARJAN_SEARCH_SPACE_H

#include "../state_id.h"
#include "../state.h"
#include "../segmented_vector.h"

#include "tarjan_state_info.h"

namespace tarjan_dfs {
  class SearchNode {
    StateID state_id;
    StateInfo &info;
  public:
    SearchNode(unsigned state_id, StateInfo &info);
    void set_idx_and_lowlink(unsigned idx);
    void update_lowlink(unsigned idx);
    void set_onstack(bool onstack = true);
  
    unsigned get_idx() const;
    unsigned get_lowlink() const;
    bool is_onstack() const;

    StateID get_state_id() const;
    State get_state() const;

    bool is_set_index() const;

    void set_h(int h);
    bool is_h_defined() const;
    int get_h() const;

    void set_flag();
    bool is_flagged() const;
  };

  class SearchSpace {
    SegmentedVector<StateInfo> m_state_information;
  public:
    SearchSpace();
    SearchNode operator[](const unsigned &id);
    SearchNode operator[](const StateID &id);
    SearchNode operator[](const State &id);
    void shrink(size_t size);
  };
}

#endif
