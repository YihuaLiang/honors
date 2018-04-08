
#ifndef OPEN_SETS_H
#define OPEN_SETS_H

#include "../state_id.h"

class SearchNode;
class Options;

class OpenSet {
public:
    OpenSet() {}
    virtual void push(const SearchNode &node, bool is_preferred) = 0;
    virtual StateID pop() = 0;
    virtual StateID top() = 0;
    virtual size_t size() const = 0;
    virtual bool empty() const = 0;
    virtual void clear() = 0;
    virtual bool reopen(const SearchNode &, const SearchNode &, int ) const {
        return false;
    }
};

#endif
