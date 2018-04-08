
#ifdef V_OPEN_SETS_H

#include "v_open_set.h"

#include "../search_space.h"
#include "../option_parser.h"
#include "../plugin.h"

#include <cassert>

int GPlusHValue::operator()(const SearchNode &node) const
{
    return node.get_g() + node.get_h();
}

int HValue::operator()(const SearchNode &node) const
{
    return node.get_h();
}

template<typename V>
VOpenSet<V>::VOpenSet() : OpenSet(), _size(0) {}

template<typename V>
void VOpenSet<V>::push(const SearchNode &node, bool is_preferred) {
    _size++;
    if (is_preferred) {
        Store &store = preferred_open[val(node)];
        store.push_back(node.get_state_id().hash());
    } else {
        Store &store = open[val(node)];
        store.push_back(node.get_state_id().hash());
    }
}

template<typename V>
StateID VOpenSet<V>::pop() {
    assert(_size > 0);
    _size--;
    int id;
    if (!preferred_open.empty()) {
        std::map<int, Store>::iterator b = preferred_open.begin();
        Store &store = b->second;
        id = store.front();
        store.pop_front();
        if (store.empty()) {
            preferred_open.erase(b);
        }
    } else {
        std::map<int, Store>::iterator b = open.begin();
        Store &store = b->second;
        id = store.front();
        store.pop_front();
        if (store.empty()) {
            open.erase(b);
        }
    }
    return StateID(id);
}


template<typename V>
StateID VOpenSet<V>::top() {
    assert(_size > 0);
    _size--;
    int id;
    if (!preferred_open.empty()) {
        std::map<int, Store>::iterator b = preferred_open.begin();
        Store &store = b->second;
        id = store.front();
        store.pop_front();
    } else {
        std::map<int, Store>::iterator b = open.begin();
        Store &store = b->second;
        id = store.front();
    }
    return StateID(id);
}

template<typename V>
size_t VOpenSet<V>::size() const {
    return _size;
}

template<typename V>
bool VOpenSet<V>::empty() const {
    return size() == 0;
}

template<typename V>
void VOpenSet<V>::clear() {
    open.clear();
    preferred_open.clear();
}

template<typename V>
bool VOpenSet<V>::reopen(const SearchNode &node, const SearchNode &, int new_g ) const
{
    return node.get_g() > new_g;
}

template<typename V>
void VOpenSet<V>::add_options_to_parser(OptionParser &) {
}

#endif

