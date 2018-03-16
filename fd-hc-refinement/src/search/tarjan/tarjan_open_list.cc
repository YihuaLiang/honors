
#include "tarjan_open_list.h"

#include "../plugin.h"

#include <algorithm>

namespace tarjan_dfs {

void OpenListFifo::insert(const SearchNode &node, const Operator *op, bool )
{
  open.insert(open.begin(), std::make_pair(op, node.get_state_id()));
}
  
StateID OpenListFifo::front_state() const
{
  return open.back().second;
}

const Operator *OpenListFifo::front_operator() const
{
  return open.back().first;
}

void OpenListFifo::pop_front()
{
  open.pop_back();
}

bool OpenListFifo::empty() const
{
  return open.empty();
}

static OpenListFactory *_parse_fifo(OptionParser &parser)
{
  Options opts = parser.parse();
  if (!parser.dry_run()) {
    return new OpenListFactoryFifo(opts);
  }
  return NULL;
}

static Plugin<OpenListFactory> _plugin_fifo("fifo", _parse_fifo);

#if 0

void OpenListHTies::insert(const SearchNode &node, const Operator *op, bool preferred)
{
  m_open.push_back(t_open_element(preferred, node.get_h(), node.get_state_id(), op));
}

void OpenListHTies::post_process()
{
  std::sort(m_open.begin(), m_open.end());
  m_open.erase(std::unique(m_open.begin(), m_open.end()), m_open.end());
}

StateID OpenListHTies::front_state() const
{
  return m_open.back().id;
}

const Operator *OpenListHTies::front_operator() const
{
  return m_open.back().op;
}

void OpenListHTies::pop_front()
{
  m_open.pop_back();
}

bool OpenListHTies::empty() const
{
  return m_open.empty();
}
#endif

static Plugin<OpenListFactory> _plugin_h("h", tarjan_open_list_h::parse);

}
