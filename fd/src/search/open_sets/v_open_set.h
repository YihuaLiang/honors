
#ifndef V_OPEN_SETS_H
#define V_OPEN_SETS_H

#include "open_set.h"
#include "../state_id.h"

#include <map>
#include <list>

class SearchNode;
class Options;
class OptionParser;

struct GPlusHValue {
    int operator()(const SearchNode &node) const;
};

struct HValue {
    int operator()(const SearchNode &node) const;
};


template<typename V>
class VOpenSet : public OpenSet
{
protected:
    typedef std::list<int> Store;
    typedef std::map<int, Store> OpenList;
    OpenList open;
    OpenList preferred_open;
    size_t _size;
    V val;//It has a V type value
public:
    VOpenSet();
    virtual void push(const SearchNode &node, bool is_preferred);
    virtual StateID top();
    virtual StateID pop();
    virtual size_t size() const;
    virtual bool empty() const;
    virtual void clear();
    virtual bool reopen(const SearchNode &node, const SearchNode &parent, int new_g) const;
    static void add_options_to_parser(OptionParser &parser);
};

class AstarOpenSet : public VOpenSet<GPlusHValue> {
public:
    AstarOpenSet() : VOpenSet<GPlusHValue>() {}//consider g+h
    static void add_options_to_parser(OptionParser &parser)
    {
        VOpenSet<GPlusHValue>::add_options_to_parser(parser);
    }
};

class GreedyOpenSet : public VOpenSet<HValue> {
public:
    GreedyOpenSet() : VOpenSet<HValue>() {}//only consider h
    static void add_options_to_parser(OptionParser &parser)
    {
        VOpenSet<HValue>::add_options_to_parser(parser);
    }
};

#include "v_open_set.cc"

#endif
