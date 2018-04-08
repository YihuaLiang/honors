
#ifndef DFS_OPEN_SETS_H
#define DFS_OPEN_SETS_H

#include "open_set.h"
#include "../state_id.h"
#include "../search_space.h"
#include "../rng.h"

#include <cassert>
#include <list>
#include <map>

class SearchNode;
class Options;
class OptionParser;

#if 0
class DefaultDFSOpenList : public OpenSet {
  struct t_key {
    bool preferred;
    int g;
    int h;
  t_key(bool p, int g, int h) : preferred(p), g(g), h(h) {}
    bool operator<(const t_key &x) const {
      return (preferred && !x.preferred) ||
        (preferred == x.preferred && g > x.g) || 
        (preferred == x.preferred && g == x.g && h < x.h);
    }
  };
  typedef std::list<int> Store;
  typedef std::map<t_key, Store> Open;

  Open m_open_list;
  size_t m_size;
 public:
 DefaultDFSOpenList() : OpenSet(), m_size(0) {}
    virtual void push(const SearchNode &node, bool is_preferred) 
    {
      m_size++;
      t_key k(is_preferred, node.get_depth(), node.get_h());
      Store &store = m_open_list[k];
      store.push_back(node.get_state_id().hash());
    }
    virtual StateID pop()
    {
      m_size--;
      typename Open::iterator it = m_open_list.begin();
      Store &store = it->second;
      int res = store.front();
      store.pop_front();
      if (store.empty()) {
        m_open_list.erase(it);
      }
      return StateID(res);
    }
    virtual size_t size() const
    {
      return m_size;
    }
    virtual bool empty() const
    {
      return m_size == 0;
    }
    virtual void clear()
    {
      m_open_list.clear();
      m_size = 0;
    }
};
#endif

template<typename t_key, typename t_key_constructor>
  class DFSOpenSet : public OpenSet
{
 protected:
  t_key_constructor m_constr;

  typedef std::list<int> Store;
  typedef std::map<t_key, typename DFSOpenSet<t_key, t_key_constructor>::Store> OpenList;
  std::list<DFSOpenSet<t_key, t_key_constructor>::OpenList> open_list;//a list of list of int
  std::list<DFSOpenSet<t_key, t_key_constructor>::OpenList> preferred_open_list;
  size_t _size;
  bool newdepth;
  const bool preferred_enabled;
 public:
 DFSOpenSet(bool preferred)
   : OpenSet(),
    _size(0),
    newdepth(true),
    preferred_enabled(preferred) {}
  virtual void push(const SearchNode &node, bool is_preferred)
  {
    if (this->newdepth) {
      this->open_list.push_front(typename DFSOpenSet<t_key, t_key_constructor>::OpenList());
      if (this->preferred_enabled) {
        this->preferred_open_list.push_front(typename DFSOpenSet<t_key, t_key_constructor>::OpenList());
      }
      this->newdepth = false;
    }
    if (this->preferred_enabled && is_preferred) {
      this->preferred_open_list.front()[this->m_constr(node)].push_back(node.get_state_id().hash());
    } else {
      this->open_list.front()[this->m_constr(node)].push_back(node.get_state_id().hash());
    }
    this->_size++;
  }
  virtual StateID pop()
  {
    assert(this->_size > 0);
    assert(!this->preferred_enabled || this->preferred_open_list.size() == this->open_list.size());
    assert(this->open_list.size() > 0);
    this->_size--;
    this->newdepth = true;
    int id = -1;
    if (this->preferred_enabled && this->preferred_open_list.front().size() > 0) {
      typename DFSOpenSet<t_key, t_key_constructor>::OpenList::iterator b = this->preferred_open_list.front().begin();
      typename DFSOpenSet<t_key, t_key_constructor>::Store &store = b->second;
      id = store.front();//a int list
      store.pop_front();
      if (store.empty()) {
        this->preferred_open_list.front().erase(b);
      }
    } else {
      typename DFSOpenSet<t_key, t_key_constructor>::OpenList::iterator b = this->open_list.front().begin();
      typename DFSOpenSet<t_key, t_key_constructor>::Store &store = b->second;
      id = store.front();
      store.pop_front();
      if (store.empty()) {
        this->open_list.front().erase(b);
      }
    }
    if (this->open_list.front().empty() && (!this->preferred_enabled || this->preferred_open_list.front().empty())) {
      this->open_list.pop_front();
      if (this->preferred_enabled) {
        this->preferred_open_list.pop_front();
      }
    }
    return StateID(id);
  }
  //for doing priority
  virtual StateID top(){
    assert(this->_size > 0);
    assert(!this->preferred_enabled || this->preferred_open_list.size() == this->open_list.size());
    assert(this->open_list.size() > 0);
    this->_size--;
    this->newdepth = true;
    int id = -1;
    if (this->preferred_enabled && this->preferred_open_list.front().size() > 0) {
      typename DFSOpenSet<t_key, t_key_constructor>::OpenList::iterator b = this->preferred_open_list.front().begin();
      typename DFSOpenSet<t_key, t_key_constructor>::Store &store = b->second;
      id = store.front();//a int list
    } else {
      typename DFSOpenSet<t_key, t_key_constructor>::OpenList::iterator b = this->open_list.front().begin();
      typename DFSOpenSet<t_key, t_key_constructor>::Store &store = b->second;
      id = store.front();
    }
    return StateID(id);
  }

  virtual size_t size() const
  {
    return this->_size;
  }

  virtual bool empty() const
  {
    return this->size() == 0;
  }

  virtual void clear()
  {
    this->open_list.clear();
    this->preferred_open_list.clear();
  }

  static void add_options_to_parser(OptionParser &)
  {}
};

template<typename t_key, typename t_key_constructor>
  class DFSRandomTiesOpenSet : public DFSOpenSet<t_key, t_key_constructor>
{
 protected:
  RandomNumberGenerator rng;
 public:
 DFSRandomTiesOpenSet(bool preferred, int seed = 1734)
   : DFSOpenSet<t_key, t_key_constructor>(preferred), rng(seed) {}

  virtual void push(const SearchNode &node, bool is_preferred)
  {
    this->_size++;
    if (this->newdepth) {
      this->open_list.push_front(typename DFSOpenSet<t_key, t_key_constructor>::OpenList());
      if (this->preferred_enabled) {
        this->preferred_open_list.push_front(typename DFSOpenSet<t_key, t_key_constructor>::OpenList());
      }
      this->newdepth = false;
    }
    typename DFSOpenSet<t_key, t_key_constructor>::Store *store = NULL;
    if (this->preferred_enabled && is_preferred) {
      store = &this->preferred_open_list.front()[this->m_constr(node)];
    } else {
      store = &this->open_list.front()[this->m_constr(node)];
    }
    typename DFSOpenSet<t_key, t_key_constructor>::Store::iterator pos = std::next(store->begin(),
                                    (store->empty() ? 0 : rng.next32() % store->size()) + 1);
    store->insert(pos, node.get_state_id().hash());
  }

};

namespace dfs_open_set {
  struct t_key_constructor_const {
    int operator()(const SearchNode &) const
    {
      return 0;
    }
  };
  
  struct t_key_constructor_h {
    int operator()(const SearchNode &node) const
    {
      return node.get_h();
    }
  };
  struct t_key_constructor_h_id {
    std::pair<int, int> operator()(const SearchNode &node) const
    {
      return std::pair<int, int>(node.get_h(), node.get_state_id().hash());
    }
  };

  class dfs_rnd_ties : public DFSRandomTiesOpenSet<int, t_key_constructor_const> {
  public:
  dfs_rnd_ties(bool preferred, int seed = 1734) : DFSRandomTiesOpenSet<int, t_key_constructor_const>(preferred, seed) {}
  };

  class dfs_h_ties : public DFSOpenSet<int, t_key_constructor_h> {
  public:
  dfs_h_ties(bool preferred) : DFSOpenSet<int, t_key_constructor_h>(preferred) {}
  };

  class dfs_h_rnd_ties : public DFSRandomTiesOpenSet<int, t_key_constructor_h> {
  public:
  dfs_h_rnd_ties(bool preferred, int seed = 1734) : DFSRandomTiesOpenSet<int, t_key_constructor_h>(preferred, seed) {}
  };

  class dfs_h_id_ties : public DFSOpenSet<std::pair<int, int>, t_key_constructor_h_id> {
  public:
  dfs_h_id_ties(bool preferred) : DFSOpenSet<std::pair<int, int>, t_key_constructor_h_id>(preferred) {}
  };

  void add_options_to_parser(OptionParser &parser);
  OpenSet *parse(const Options &opts);
}

#endif
