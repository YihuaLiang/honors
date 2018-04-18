#include "search_space.h"

#include "operator.h"
#include "state.h"
#include "globals.h"

#include <cassert>
#include "search_node_info.h"

using namespace std;
using namespace __gnu_cxx;


SearchNode::SearchNode(StateID state_id_, SearchNodeInfo &info_,
                       OperatorCost cost_type_)
    : state_id(state_id_), info(info_), cost_type(cost_type_) {
    assert(state_id != StateID::no_state);
}

State SearchNode::get_state() const {
    return g_state_registry->lookup_state(state_id);
}

bool SearchNode::is_open() const {
    return info.status == SearchNodeInfo::OPEN;
}

bool SearchNode::is_closed(unsigned ) const {
    return info.status == SearchNodeInfo::CLOSED;
}

bool SearchNode::is_dead_end() const {
    return info.status == SearchNodeInfo::DEAD_END;
}

bool SearchNode::is_new() const {
    return info.status == SearchNodeInfo::NEW;
}

int SearchNode::get_g() const {
    return info.g;
}

int SearchNode::get_h() const {
    return info.h;
}

void SearchNode::open_initial(int h) {
    assert(info.status == SearchNodeInfo::NEW);
    info.status = SearchNodeInfo::OPEN;
    info.g = 0;
    info.h = h;
    info.parent_state_id = StateID::no_state;
    info.creating_operator = 0;
    info.open_succ = 0;
}

void SearchNode::open(int h, const SearchNode &parent_node,
                      const Operator *parent_op) {
    assert(info.status == SearchNodeInfo::NEW);
    info.status = SearchNodeInfo::OPEN;
    info.g = parent_node.info.g + get_adjusted_action_cost(*parent_op, cost_type);
    info.h = h;
    info.parent_state_id = parent_node.get_state_id();
    info.creating_operator = parent_op;
    info.open_succ = 0;
}

void SearchNode::reopen(const SearchNode &parent_node,
                        const Operator *parent_op) {
    assert(info.status == SearchNodeInfo::OPEN || // not new
           info.status == SearchNodeInfo::CLOSED);

    // The latter possibility is for inconsistent heuristics, which
    // may require reopening closed nodes.
    info.status = SearchNodeInfo::OPEN;//set status
    info.g = parent_node.info.g + get_adjusted_action_cost(*parent_op, cost_type);
    info.parent_state_id = parent_node.get_state_id();
    info.creating_operator = parent_op;
}

// like reopen, except doesn't change status
void SearchNode::update_parent(const SearchNode &parent_node,
                               const Operator *parent_op) {
    assert(info.status == SearchNodeInfo::OPEN ||
           info.status == SearchNodeInfo::CLOSED);
    // The latter possibility is for inconsistent heuristics, which
    // may require reopening closed nodes.
    info.g = parent_node.info.g + get_adjusted_action_cost(*parent_op, cost_type);
    info.parent_state_id = parent_node.get_state_id();
    info.creating_operator = parent_op;
}

void SearchNode::increase_h(int h) {
    assert(h >= info.h);
    info.h = h;
}

void SearchNode::close(unsigned ) {
    info.status = SearchNodeInfo::CLOSED;
}

void SearchNode::mark_as_dead_end() {
    info.status = SearchNodeInfo::DEAD_END;
}

bool SearchNode::is_flagged() const
{
    return info.flag;
}

void SearchNode::set_flag(bool flag)
{
    info.flag = flag ? 1 : 0;
}

bool SearchNode::is_u_flagged() const
{
  return info.u_flag;
}

void SearchNode::set_u_flag(bool flag)
{
  info.u_flag = flag ? 1 : 0;
}

unsigned SearchNode::get_revision() const
{
    return info.revision;
}

void SearchNode::set_revision(unsigned x)
{
    info.revision = x;
}

bool SearchNode::add_parent(const State &state) {
    std::pair<std::unordered_set<StateID>::iterator, bool> elem = info.parents.insert(state.get_id());
    return elem.second;
}

const std::unordered_set<StateID> &SearchNode::get_all_parents() const {
    return info.parents;
}

const std::vector<StateID> &SearchNode::get_all_successors() const {
    return info.successors;
}

std::vector<StateID> &SearchNode::get_all_successors() {
    return info.successors;
}

void SearchNode::dump() const {
    cout << state_id << ": ";
    g_state_registry->lookup_state(state_id).dump_fdr();
    if (info.creating_operator) {
        cout << " created by " << info.creating_operator->get_name()
             << " from " << info.parent_state_id << endl;
    } else {
        cout << " no parent" << endl;
    }
}

SearchSpace::SearchSpace(OperatorCost cost_type_)
    : cost_type(cost_type_) {
}

SearchNodeInfo &SearchSpace::lookup(unsigned sid) {
    if (sid >= m_infos.size()) {
        m_infos.resize(sid + 1);
    }
    return m_infos[sid];
}

SearchNodeInfo &SearchSpace::lookup(const State &state) {
    return lookup(state.get_id().hash());
}

SearchNode SearchSpace::get_node(const State &state) {
    return SearchNode(state.get_id(), lookup(state), cost_type);//not actually found the stored node 
}

//overload operator for priority queue, the smaller the better
bool operator<(const SearchNode node1, const SearchNode node2){
    int h1 = node1.get_h();
    int h2 = node2.get_h();
    //int g1 = this->get_g();
    //int g2 = node1.get_g();

    return h1 > h2;//only consider h
    //return (h2 + g2) > (h1 + g1);
}
SearchNode& SearchNode::operator=(const SearchNode & node1)
{
    state_id = node1.state_id;
    cost_type = node1.cost_type;
    info = node1.info;    
    return *this;
}

void SearchSpace::trace_path(const State &goal_state,
                             vector<const Operator *> &path) {
    State current_state = goal_state;
    assert(path.empty());
    for (;;) {
        const SearchNodeInfo &info = lookup(current_state);
        const Operator *op = info.creating_operator;
        if (op == 0) {
            assert(info.parent_state_id == StateID::no_state);
            break;
        }
        path.push_back(op);
        current_state = g_state_registry->lookup_state(info.parent_state_id);
    }
    reverse(path.begin(), path.end());
}

void SearchSpace::dump() const {
    //for (PerStateInformation<SearchNodeInfo>::const_iterator it =
    //         search_node_infos.begin(g_state_registry);
    //     it != search_node_infos.end(g_state_registry); ++it) {
    //    StateID id = *it;
    //    State s = g_state_registry->lookup_state(id);
    //    const SearchNodeInfo &node_info = search_node_infos[s];
    //    cout << id << ": ";
    //    s.dump_fdr();
    //    if (node_info.creating_operator && node_info.parent_state_id != StateID::no_state) {
    //        cout << " created by " << node_info.creating_operator->get_name()
    //             << " from " << node_info.parent_state_id << endl;
    //    } else {
    //        cout << "has no parent" << endl;
    //    }
    //}
}

void SearchSpace::statistics() const {
    cout << "Number of registered states: " << g_state_registry->size() << endl;
}

void SearchSpace::clear() {
    m_infos.clear();
}

