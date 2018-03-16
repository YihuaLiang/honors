#ifndef SEARCH_NODE_INFO_H
#define SEARCH_NODE_INFO_H

#include "operator.h"
#include "state_id.h"

#include <unordered_set>
#include <vector>

// For documentation on classes relevant to storing and working with registered
// states see the file state_registry.h.

struct SearchNodeInfo {
  enum NodeStatus {NEW = 0, OPEN = 1, CLOSED = 2, DEAD_END = 3};

  unsigned int status : 2;
  int g : 30;
  int h;
  unsigned revision : 30;
  unsigned flag : 1;
  unsigned u_flag : 1;
  StateID parent_state_id;
  const Operator *creating_operator;

  std::unordered_set<StateID> parents;
  std::vector<StateID> successors;

SearchNodeInfo()
: status(NEW), g(-1), h(-1), revision(0), flag(0), u_flag(0),
    parent_state_id(StateID::no_state), creating_operator(0) {
}
};

/*
  TODO: The C++ standard does not guarantee that bitfields with mixed
  types (unsigned int, int, bool) are stored in the compact way
  we desire. However, g++ does do what we want. To be safe for
  the future, we should add a static assertion that verifies
  that SearchNodeInfo has the desired size.
*/

#endif
