
#ifndef TARJAN_OPEN_LIST_H
#define TARJAN_OPEN_LIST_H

#include "tarjan_search_space.h"
#include "../operator.h"
#include "../state_id.h"
#include "../option_parser.h"

#include <vector>
#include <map>
#include <string>

namespace tarjan_dfs {
class OpenList {
 public:
  virtual void insert(const SearchNode &node, const Operator *op, bool preferred) = 0;
  virtual void post_process() {}
  virtual StateID front_state() const = 0;
  virtual const Operator *front_operator() const = 0;
  virtual void pop_front() = 0;
  virtual bool empty() const = 0;
};

class OpenListFactory {
 public:
  OpenListFactory(const Options &) {}
  virtual OpenList *create() = 0;
};


class OpenListFifo : public OpenList {
  std::vector<std::pair<const Operator *, StateID> > open;
 public:
  virtual void insert(const SearchNode &node, const Operator *op, bool preferred);
  virtual StateID front_state() const;
  virtual const Operator *front_operator() const;
  virtual void pop_front();
  virtual bool empty() const;
};

class OpenListFactoryFifo : public OpenListFactory {
 public:
 OpenListFactoryFifo(const Options &opts) : OpenListFactory(opts) {}
  virtual OpenList *create() { return new OpenListFifo; }
};


#if 0
class OpenListHTies : public OpenList {
 protected:
  struct t_open_element {
    bool preferred;
    int h;
    StateID id;
    const Operator *op;
  t_open_element(bool pref, int h, StateID id, const Operator *op)
  : preferred(pref), h(h), id(id), op(op) {}
    bool operator<(const t_open_element &o) const {
      return (!preferred && o.preferred)
        || (preferred == o.preferred && (h > o.h ||
                                         (h == o.h && id.hash() > o.id.hash())));
    }
    bool operator==(const t_open_element &o) const {
      return id.hash() == o.id.hash();
    }
  };
  std::vector<t_open_element> m_open;
 public:
  virtual void insert(const SearchNode &node, const Operator *op, bool preferred);
  virtual void post_process();
  virtual StateID front_state() const;
  virtual const Operator *front_operator() const;
  virtual void pop_front();
  virtual bool empty() const;
};

class OpenListFactoryHTies : public OpenListFactory {
 public:
  virtual OpenList *create()
  {
    return new OpenListHTies();
  }
};
#endif


template<typename t_key, typename t_key_create>
  class OpenListMap : public OpenList {
 public:
  struct t_elem {
    StateID id;
    const Operator *op;
  t_elem(const StateID &id, const Operator *op)
  : id(id), op(op){}
    bool operator<(const t_elem &e) const {
      return id.hash() < e.id.hash();
    }
    bool operator==(const t_elem &e) const {
      return id.hash() == e.id.hash();
    }
  };
  typedef std::vector<t_elem> List;
  typedef std::map<t_key, List> Open;
 protected:
  t_key_create key;
  typename OpenListMap<t_key, t_key_create>::Open m_open;
  typename OpenListMap<t_key, t_key_create>::Open m_open_preferred;
  size_t m_size;
 public:
 OpenListMap() : m_size(0) {}
  virtual void insert(const SearchNode &node, const Operator *op, bool preferred)
  {
    m_size++;
    if (preferred) {
      typename OpenListMap<t_key, t_key_create>::List &l = this->m_open_preferred[this->key(node, op)];
      l.push_back(t_elem(node.get_state_id(), op));
    } else {
      typename OpenListMap<t_key, t_key_create>::List &l = this->m_open[this->key(node, op)];
      l.push_back(t_elem(node.get_state_id(), op));
    }
  }
  virtual StateID front_state() const
  {
    if (!this->m_open_preferred.empty()) {
      return this->m_open_preferred.begin()->second.back().id;
    }
    return this->m_open.begin()->second.back().id;
  }
  virtual const Operator *front_operator() const
  {
    if (!this->m_open_preferred.empty()) {
      return this->m_open_preferred.begin()->second.back().op;
    }
    return this->m_open.begin()->second.back().op;
  }
  virtual void pop_front()
  {
    assert(m_size > 0);
    m_size--;
    if (!this->m_open_preferred.empty()) {
      typename OpenListMap<t_key, t_key_create>::Open::iterator it = this->m_open_preferred.begin();
      typename OpenListMap<t_key, t_key_create>::List &l = it->second;
      l.pop_back();
      if (l.empty()) {
        this->m_open_preferred.erase(it);
      }
      return;
    }
    typename OpenListMap<t_key, t_key_create>::Open::iterator it = this->m_open.begin();
    typename OpenListMap<t_key, t_key_create>::List &l = it->second;
    l.pop_back();
    if (l.empty()) {
      this->m_open.erase(it);
    }
  }
  virtual bool empty() const
  {
    return m_size == 0;
  }
};


namespace tarjan_open_list_h {
  struct t_key_h_create {
    int operator()(const SearchNode &node, const Operator *) {
      return node.get_h();
    }
  };
  struct t_key_h_id_create {
    std::pair<int, int> operator()(const SearchNode &node, const Operator *) {
      return std::make_pair(node.get_h(), node.get_state_id().hash());
    }
  };
  template<typename t_key, typename t_key_create>
    class factory : public OpenListFactory  {
  public:
  factory(const Options &opts) : OpenListFactory(opts)
        {}
    virtual OpenList *create()
    {
      return new OpenListMap<t_key, t_key_create>();
    }
  };
  inline OpenListFactory *parse(OptionParser &parser) {
    std::vector<std::string> ties;
    ties.push_back("arbitrary"); // 0
    ties.push_back("state_id"); // 1
    parser.add_enum_option("tiebreaking", ties, "", "state_id");
    Options opts = parser.parse();
    if (!parser.dry_run()) {
      switch (opts.get_enum("tiebreaking")) {
      case (0):
        return new factory<int, t_key_h_create>(opts);
      case (1):
        return new factory<std::pair<int, int>, t_key_h_id_create>(opts);
      }
    }
    return NULL;
  }
}
}

#endif
