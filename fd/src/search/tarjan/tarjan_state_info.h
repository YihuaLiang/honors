
#ifndef TARJAN_STATE_INFO_H
#define TARJAN_STATE_INFO_H

#include <limits>

namespace tarjan_dfs {
  struct StateInfo {
    constexpr static const unsigned UNDEFINED = std::numeric_limits<int>::max();
    unsigned idx : 31;
    unsigned flag : 1;
    unsigned lowlink : 31;
    unsigned onstack : 1;
    int h;
  
  StateInfo() : idx(UNDEFINED), flag(0), lowlink(UNDEFINED), onstack(false), h(-2) {}
  };
}

#endif
