
#include "tarjan_search_space.h"

#include "../globals.h"
#include "../state_registry.h"

namespace tarjan_dfs {
  SearchNode::SearchNode(unsigned state_id, StateInfo &info)
    : state_id(state_id), info(info) {}

  void SearchNode::set_idx_and_lowlink(unsigned idx)
  {
    info.idx = idx;
    info.lowlink = idx;
  }

  void SearchNode::update_lowlink(unsigned idx)
  {
    info.lowlink = idx < info.lowlink ? idx : info.lowlink;
  }

  void SearchNode::set_onstack(bool onstack)
  {
    info.onstack = onstack;
  }

  unsigned SearchNode::get_idx() const {
    return info.idx;
  }

  unsigned SearchNode::get_lowlink() const
  {
    return info.lowlink;
  }

  bool SearchNode::is_onstack() const {
    return info.onstack;
  }

  StateID SearchNode::get_state_id() const {
    return state_id;
  }

  State SearchNode::get_state() const {
    return g_state_registry->lookup_state(state_id);
  }

  bool SearchNode::is_set_index() const {
    return info.idx != StateInfo::UNDEFINED;
  }

  void SearchNode::set_h(int h) {
    info.h = h;
  }

  bool SearchNode::is_h_defined() const {
    return info.h != -2;
  }

  int SearchNode::get_h() const {
    return info.h;
  }

  void SearchNode::set_flag()
  {
    info.flag = 1;
  }

  bool SearchNode::is_flagged() const
  {
    return info.flag;
  }


  SearchSpace::SearchSpace() {}

  SearchNode SearchSpace::operator[](const unsigned &id)
  {
    if (id >= m_state_information.size()) {
      m_state_information.resize(id + 1);
    }
    assert(id < m_state_information.size());
    return SearchNode(id, m_state_information[id]);
  }

  SearchNode SearchSpace::operator[](const StateID &id)
  {
    return (*this)[id.hash()];
  }

  SearchNode SearchSpace::operator[](const State &id)
  {
    return (*this)[id.get_id()];
  }

  void SearchSpace::shrink(size_t size) {
    if (size < m_state_information.size()) {
      m_state_information.resize(size);
    }
  }
}
