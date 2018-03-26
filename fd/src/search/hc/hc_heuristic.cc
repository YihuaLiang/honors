#include "hc_heuristic.h"

#include "conjunction_operations.h"

#include "../globals.h"
#include "../operator.h"
#include "../state.h"
#include "../option_parser.h"
#include "../plugin.h"

#include <cassert>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sstream>

#include <map>

#define DUMP_CONJUNCTIONS_ 0

using namespace std;
using namespace fluent_op;

#define TIME_ANALYSIS 0

#if TIME_ANALYSIS
static clock_t G_t1 = 0;
static clock_t G_start = 0;
static void G_start_timer()
{
  G_start = clock();
}
static clock_t G_get_duration()
{
  return clock() - G_start;
}
#endif

#define FILTER_NEGATED_ATOMS 0

#if FILTER_NEGATED_ATOMS
std::vector<std::vector<bool> > g_is_negated_atom;
#endif


void My_hash_combine(std::size_t &h, const std::size_t &v)
{
  // from boost
  h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2);
}


void Conflict::merge(const Fact &fact)
{
  Fluent fluent;
  fluent.insert(fact);
  merge(fluent);
}

void Conflict::merge(const Conflict &conj)
{
  merge(conj.fluent);
}

void Conflict::merge(const Fluent &fluent)
{
  /* merge the current conjunction stored insided with the given conjunction */
  // update the fluent set by inserting all elements contained in conj
  this->fluent.insert(fluent.begin(), fluent.end());
}

// construction and destruction
HCHeuristic::HCHeuristic(const Options &opts)
  : Heuristic(opts),
    c_dump_conjunction_set(opts.get<bool>("dump_conjunctions")),
    c_minimize_counter_set(false),
    c_dual_task(opts.get<bool>("dual")),
    //c_minimize_counter_set(opts.get<bool>("inverse")),
    //c_inverse_task(opts.get<bool>("inverse")),
    _comp(this),
    _unique_conjunctions(_comp),
    c_update_mutex(opts.get<bool>("update_mutex"))
{
  max_number_fluents = -1;
  //if (opts.contains("c")) {
  //    max_number_fluents = (unsigned) opts.get<int>("c");
  //}
  max_ratio_repr_counters = -1;
  //if (opts.contains("x")) {
  //    max_ratio_repr_counters = opts.get<float>("x");
  //    if (max_ratio_repr_counters >= 0 && max_ratio_repr_counters < 1) {
  //        max_ratio_repr_counters = 1;
  //    }
  //}
  current_fluents = 0;
  max_num_counters = -1;

  hard_size_limit = true;
  if (opts.contains("hard_size_limit")) {
    hard_size_limit = opts.get<bool>("hard_size_limit");
  }

  _m = opts.get<int>("m");

  __load_from_file = opts.get<string>("file");

  prune_counter_precondition = opts.get<bool>("prune_pre");

  early_termination = opts.get<bool>("early_term");
}

HCHeuristic::HCHeuristic(const HCHeuristic &h)
  : Heuristic(h),
    c_dump_conjunction_set(h.c_dump_conjunction_set),
    c_minimize_counter_set(h.c_minimize_counter_set),
    c_dual_task(h.c_dual_task),
    _comp(this),
    _unique_conjunctions(_comp),
    variable_offset(h.variable_offset),
    conjunction_offset(h.conjunction_offset),
    _fluents(h._fluents),
    actions(h.actions),
    m_true_id(h.m_true_id),
    m_goal_id(h.m_goal_id),
    m_goal_counter(h.m_goal_counter),
    facts_to_add(h.facts_to_add),
    facts_to_del(h.facts_to_del),
    _num_counters(h._num_counters),
    early_termination(h.early_termination),
    facts_conjunctions_relation(h.facts_conjunctions_relation),
    current_fluents(h.current_fluents),
    max_number_fluents(h.max_number_fluents),
    max_num_counters(h.max_num_counters),
    max_ratio_repr_counters(h.max_ratio_repr_counters),
    counter_preconditions(h.counter_preconditions),
    conjunction_achievers(h.conjunction_achievers),
    m_results_in_mutex(h.m_results_in_mutex),
    hard_size_limit(h.hard_size_limit),
    _m(h._m),
    __load_from_file(h.__load_from_file),
    c_update_mutex(h.c_update_mutex)
    //,
    //  m_fact_to_mutex(h.m_fact_to_mutex),
    //  m_mutex_size(h.m_mutex_size),
    //  m_mutex(h.m_mutex),
    //  m_mutex_subset(h.m_mutex_subset)
{ //Initialize h
  for (unsigned cid = 0; cid < h.conjunctions.size(); cid++) {
    conjunctions.push_back(h.conjunctions[cid]);
  }
  for (unsigned c = 0; c < h.counters.size(); c++) {
    counters.push_back(h.counters[c]);
    counters[c].effect = &conjunctions[counters[c].effect->id];
  }
  for (unsigned cid = 0; cid < conjunctions.size(); cid++) {
    for (unsigned i = 0; i < conjunctions[cid].triggered_counters.size(); i++) {
      conjunctions[cid].triggered_counters[i] = &counters[conjunctions[cid].triggered_counters[i]->id];
    }
  }
  facts_to_counters.resize(conjunction_offset);
  for (unsigned fid = 0; fid < conjunction_offset; fid++) {
    facts_to_counters[fid].resize(h.facts_to_counters[fid].size());
    for (unsigned i = 0; i < h.facts_to_counters[fid].size(); i++) {
      facts_to_counters[fid][i] = &counters[h.facts_to_counters[fid][i]->id];
    }
  }
  for (unsigned i = 0; i < conjunctions.size(); i++) {
    _unique_conjunctions.insert(i);
  }
}

int HCHeuristic::get_number_of_conjunctions() const
{
  return current_fluents;
}

int HCHeuristic::get_number_of_counters() const
{
  return counters.size();
}

double HCHeuristic::get_counter_ratio() const
{
  return (double) counters.size() / _num_counters;
}

bool HCHeuristic::dump_fact_pddl(int var, int val,
                                 std::ostream &out, bool sep, bool negated) const
{
  const string &descr = g_fact_names[var][val];
  int i = descr.find(" ");
  if (i != 4) {
    if (!negated) {
      return false;
    }
    if (sep) {
      out << ", ";
    }
    out << "!";
  } else if (sep) {
    out << ", ";
  }
  out << g_fact_names[var][val].substr(i + 1); // cut atom
  return true;
}

void HCHeuristic::dump_state_pddl(const State &state,
                                  std::ostream &out) const
{
  out << "[";
  bool sep = false;
  for (uint var = 0; var < g_variable_domain.size(); var++) {
    if (dump_fact_pddl(var, state[var], out, sep)) {
      sep = true;
    }
  }
  out << "]";
}

void HCHeuristic::dump_fluent_pddl(const Fluent &pi,
                                   std::ostream &out,
                                   bool lala) const
{
  if (lala)
    out << "[";
  bool sep = false;
  for (Fluent::iterator it = pi.begin(); it != pi.end(); it++) {
    if (dump_fact_pddl(it->first, it->second, out, sep)) {
      sep = true;
    }
  }
  if (lala)
    out << "]";
}

void HCHeuristic::dump_conjunctions_pddl(int from, int to,
                                         bool breakes, bool with_cost, bool with_id, std::ostream &out) const
{
  bool sep = false;
  for (int i = from; i < to; i++) {
    if (sep) {
      out << ", ";
      if (breakes) {
        out << endl;
      }
    }
    if (with_id) {
      out << i << ". ";
    }
    dump_fluent_pddl(_fluents[i], out);
    if (with_cost) {
      out << ": " << conjunctions[i].cost;
    }
    sep = true;
  }
}

void HCHeuristic::dump_facts_pddl(bool breakes, bool with_cost,
                                  std::ostream &out) const
{
  dump_conjunctions_pddl(0, conjunction_offset, breakes, with_cost, false,
                         out);
}

void HCHeuristic::dump_c_pddl(bool breakes, bool with_id,
                              bool with_cost,
                              std::ostream &out) const
{
  dump_conjunctions_pddl(conjunction_offset, conjunctions.size(), breakes,
                         with_cost, with_id, out);
  if (breakes) {
    cout << endl;
  }
}

void HCHeuristic::dump_fluent_set_pddl(const vector<unsigned>
                                       &fluent_ids,
                                       std::ostream &out, int from, int to) const
{
  int i = 0;
  vector<unsigned>::const_iterator
    it = fluent_ids.begin(),
    itEnd = fluent_ids.end();
  bool sep = false;

  while (i < from && it != itEnd) {
    i++;
    it++;
  }

  out << "{";
  while ((to < 0 || i < to) && it != itEnd) {
    if (sep) {
      out << ", ";
    }
    dump_fluent_pddl(_fluents[*it], out);
    sep = true;
    i++;
    it++;
  }
  out << "}";
}

void HCHeuristic::load_conjunctions_from_file(std::string path)
{
  ifstream infile;
  infile.open(path.c_str());
  if (!infile.good()) {
    return;
  }
  cout << "Going to load conjunctions from file \"" << path << "\"" << endl;
  while (!infile.eof()) {
    string line;
    getline(infile, line);
    Conflict confl;
    for (uint i = 0; i < g_fact_names.size(); i++) {
      for (uint j = 0; j < g_fact_names[i].size(); j++) {
        if (line.find(g_fact_names[i][j]) != string::npos) {
          Fact repr_f = get_fact(i, j);
          unsigned fid = get_fact_id(repr_f);
          confl.merge(_fluents[fid]);
        }
      }
    }
    if (confl.get_fluent().size() > 1) {
      add_conflict(confl);
    }
  }
  infile.close();
  dump_compilation_information(cout);
}

void HCHeuristic::dump_conjunctions_to_file(std::string path) const
{
  ofstream outfile;
  outfile.open(path.c_str());
  dump_conjunctions_to_ostream(outfile);
  outfile.close();
}

void HCHeuristic::dump_conjunctions_to_ostream(ostream &out) const
{
  for (uint i = conjunction_offset; i < conjunctions.size(); i++) {
    bool sep = false;
    for (Fluent::iterator fact = _fluents[i].begin();
         fact != _fluents[i].end(); fact++) {
      if (sep) {
        out << " ";
      }
      out << g_fact_names[fact->first][fact->second];
      sep = true;
    }
    out << endl;
  }
}

void HCHeuristic::dump_counter(size_t counter_id,
                               ostream &out) const
{
  const ActionEffectCounter &counter = counters[counter_id];
  out << "[Counter " << counter_id << "] " << endl;
  out << "Base operator: ";
  //g_operators[actions[counter.action].operator_no].dump();
  out << "Effect: ";
  dump_fluent_pddl(_fluents[counter.effect->id], out);
  out << endl;
  //if (store_counter_precondition) {
  //    out << "Precondition: ";
  //    dump_fluent_set_pddl(counter_preconditions[counter_id], out);
  //    out << endl;
  //}
  //out << "[/" << counter_id << "]" << endl;
}



void HCHeuristic::initialize()
{//called
  cout << "Initializing h^max(Pi^C) heuristic ..." << endl;
  h_name = "HC_heuristic";
#if FILTER_NEGATED_ATOMS
  std::string negatedatom = "NegatedAtom";
  g_is_negated_atom.resize(g_variable_domain.size());
  for (uint var = 0; var < g_variable_domain.size(); var++) {
    g_is_negated_atom[var].resize(g_variable_domain[var], false);
    for (int val = 0; val < g_variable_domain[var]; val++) {
      const std::string &fact_name = g_fact_names[var][val];
      if (fact_name.size() < negatedatom.size()) {
        continue;
      }
      std::string substr = fact_name.substr(0, negatedatom.size());
      g_is_negated_atom[var][val] = substr.compare(negatedatom) == 0;
    }
  }
#endif

  ::verify_no_axioms();
  
  heuristic = 0;
  evaluator_value = 0;
  //int accumultor = 0;
  //for (uint i = 0 ; i < g_operators.size() ; i ++)
 // {
  //  if(g_operators[i].get_cost() == 0) accumultor++;
  //}
  //cout<<"0 cost action"<<accumultor<<endl;
  compile_strips();//learn the strips

  //dump_compiled_task_to_file();
  //exit(0);

  if (c_minimize_counter_set) {
    std::cout << "Checking for subsumed counters ..." << std::endl;
    unsigned num_subsumed_counters = 0;
    std::vector<unsigned> hit;
    hit.resize(counters.size());
    std::vector<bool> is_subsumed;
    is_subsumed.resize(counters.size(), false);
    for (unsigned i = 0; i < counters.size(); i++) {
      for (unsigned j = 0; j < counters.size(); j++) {
        hit[j] = 0;
      }
      for (unsigned pre : counter_preconditions[i]) {
        const std::vector<ActionEffectCounter *> &tc =
          conjunctions[pre].triggered_counters;
        for (ActionEffectCounter * c : tc) {
          if (c->effect == counters[i].effect &&
              ++hit[c->id] == counter_preconditions[c->id].size()) {
            if (hit[c->id] < counter_preconditions[i].size()
                || c->id < i) {
              is_subsumed[i] = true;
              num_subsumed_counters++;
              break;
            }
          }
        }
        if (is_subsumed[i]) {
          break;
        }
      }
    }
    std::cout << "Subsumed counters: " << num_subsumed_counters << std::endl;
    if (num_subsumed_counters > 0) {
      std::cout << "Updating data structures ..." << std::flush;
      for (unsigned fact = 0; fact < facts_to_counters.size(); fact++) {
        unsigned i = 0;
        unsigned j = 0;
        for (; i < facts_to_counters[fact].size(); i++) {
          if (!is_subsumed[facts_to_counters[fact][i]->id]) {
            facts_to_counters[fact][j] = facts_to_counters[fact][i];
            j++;
          }
        }
        if (j < facts_to_counters[fact].size()) {
          facts_to_counters[fact].erase(facts_to_counters[fact].begin() + j,
                                        facts_to_counters[fact].end());
        }
      }

      for (unsigned conj = 0; conj < conjunctions.size(); conj++) {
        unsigned i = 0;
        unsigned j = 0;
        for (; i < conjunction_achievers[conj].size(); i++) {
          if (!is_subsumed[conjunction_achievers[conj][i]]) {
            conjunction_achievers[conj][j] = conjunction_achievers[conj][i];
            j++;
          }
        }
        if (j < conjunction_achievers[conj].size()) {
          conjunction_achievers[conj].erase(conjunction_achievers[conj].begin() + j,
                                            conjunction_achievers[conj].end());
        }

        i = 0;
        j = 0;
        std::vector<ActionEffectCounter *> &tc = conjunctions[conj].triggered_counters;
        for (; i < tc.size(); i++) {
          if (!is_subsumed[tc[i]->id]) {
            tc[j] = tc[i];
            j++;
          }
        }
        if (j < tc.size()) {
          tc.erase(tc.begin() + j, tc.end());
        }
      }
      std::cout << "done" << std::endl;
    }
  }

  // unsigned mutex_id = 0;
  // m_fact_to_mutex.resize(conjunction_offset);
  // std::pair<int, int> f0;
  // std::pair<int, int> f1;
  // for (f0.first = 0; f0.first < (int) g_variable_domain.size() - 1; f0.first++) {
  //   for (f0.second = 0; f0.second < g_variable_domain[f0.first]; f0.second++) {
  //     for (f1.first = f0.first + 1; f1.first < (int) g_variable_domain.size(); f1.first++) {
  //       for (f1.second = 0; f1.second < g_variable_domain[f1.first]; f1.second++) {
  //         if (::are_mutex(f0, f1)) {
  //           m_fact_to_mutex[get_fact_id(f0)].push_back(mutex_id);
  //           m_fact_to_mutex[get_fact_id(f1)].push_back(mutex_id);
  //           m_mutex_size.push_back(2);
  //           m_mutex.push_back(Fluent());
  //           m_mutex.back().insert(f0);
  //           m_mutex.back().insert(f1);
  //           mutex_id++;
  //         }
  //       }
  //     }
  //   }
  // }
  // m_mutex_subset.resize(mutex_id);
}

void HCHeuristic::remove_conjunctions(const std::vector<unsigned> &ids)
{
  std::vector<int> _subset; _subset.resize(conjunctions.size(), 0);
  unsigned new_id = 0;
  std::vector<unsigned> new_conj_ids; new_conj_ids.resize(conjunctions.size(), -1);
  std::vector<bool> removed; removed.resize(conjunctions.size(), false);

#ifndef NDEBUG
  std::cout << "getting conjunctions to remove..." << std::endl;
#endif
  for (unsigned index = 0; index < ids.size(); index++) {
    const unsigned &cid = ids[index];
    if (cid >= conjunction_offset && cid != m_true_id && cid != m_goal_id) {
      removed[cid] = true;
    }
  }
  for (unsigned index = 0; index < ids.size(); index++) {
    const unsigned &cid = ids[index];
    Conjunction &conj = conjunctions[cid];
    for (unsigned i = 0; i < conj.triggered_counters.size(); i++) {
      conj.triggered_counters[i]->preconditions--;
      if (prune_counter_precondition) {
        unsigned counter_id = conj.triggered_counters[i]->id;
        counter_preconditions[counter_id].erase(
                                                std::find(counter_preconditions[counter_id].begin(), counter_preconditions[counter_id].end(), cid));
        std::fill(_subset.begin(), _subset.end(), 0);
        for (Fluent::const_iterator f = _fluents[cid].begin(); f != _fluents[cid].end(); f++) {
          for (const unsigned &cid2 : facts_conjunctions_relation[get_fact_id(*f)]) {
            if (!removed[cid2] && ++_subset[cid2] == conjunctions[cid2].fluent_size) {
              if (!std::count(counter_preconditions[counter_id].begin(),
                              counter_preconditions[counter_id].end(),
                              cid2)) {
                add_counter_precondition(&counters[counter_id], &conjunctions[cid2]);
              }
            }
          }
        }
      }
    }
  }
  _subset.clear();

#ifndef NDEBUG
  std::cout << "getting new conjunction ids..." << std::endl;
#endif
  for (unsigned cid = 0; cid < conjunctions.size(); cid++) {
    if (!removed[cid]) {
      if (cid == m_true_id) {
        m_true_id = new_id;
      }
      if (cid == m_goal_id) {
        m_goal_id = new_id;
      }
      new_conj_ids[cid] = new_id++;
    } 
  }
    
#ifndef NDEBUG
  std::cout << "going to remove " << (conjunctions.size() - new_id) << " conjunctions" << std::endl;
#endif

  if (new_id < conjunctions.size()) {
    current_fluents = new_id - conjunction_offset - 2;

#ifndef NDEBUG
    std::cout << "finding counters to be removed..." << std::endl;
#endif
    new_id = 0;
    for (unsigned i = 0; i < counters.size(); i++) {
      if (!removed[counters[i].effect->id]) {
        if (i == m_goal_counter) {
          m_goal_counter = new_id;
        }
        counters[i].id = new_id++;
      }
    }
#ifndef NDEBUG
    std::cout << "going to remove " << (counters.size() - new_id) << " counters" << std::endl;
    std::cout << "updating triggered counters..." << std::endl;
#endif
    for (unsigned i = 0; i < conjunctions.size(); i++) {
      if (removed[i]) {
        continue;
      }
      std::vector<ActionEffectCounter*> old;
      old.swap(conjunctions[i].triggered_counters);
      for (unsigned j = 0; j < old.size(); j++) {
        if (!removed[old[j]->effect->id]) {
          conjunctions[i].triggered_counters.push_back(&counters[old[j]->id]);
        }
      }
    }
#ifndef NDEBUG
    std::cout << "updating facts to counters..." << std::endl;
#endif
    for (unsigned i = 0; i < facts_to_counters.size(); i++) {
      std::vector<ActionEffectCounter*> old;
      old.swap(facts_to_counters[i]);
      for (unsigned j = 0; j < old.size(); j++) {
        if (!removed[old[j]->effect->id]) {
          facts_to_counters[i].push_back(&counters[old[j]->id]);
        }
      }
    }
#ifndef NDEBUG
    std::cout << "updating facts conjunctions relation..." << std::endl;
#endif
    for (unsigned i = 0; i < facts_conjunctions_relation.size(); i++) {
      std::vector<unsigned> old;
      old.swap(facts_conjunctions_relation[i]);
      for (unsigned j = 0; j < old.size(); j++) {
        if (!removed[old[j]]) {
          facts_conjunctions_relation[i].push_back(new_conj_ids[old[j]]);
        }
      }
    }

#ifndef NDEBUG
    std::cout << "removing counters..." << std::endl;
#endif
    new_id = 0;
    for (unsigned i = 0; i < counters.size(); i++) {
      assert(counters[i].effect->id < removed.size());
      if (!removed[counters[i].effect->id]) {
        assert(counters[i].id <= i);
        assert(counters[i].effect->id < new_conj_ids.size());
        assert(!removed[counters[i].effect->id]);
        assert(new_conj_ids[counters[i].effect->id] < conjunctions.size());
        new_id++;
        counters[i].effect = &conjunctions[new_conj_ids[counters[i].effect->id]];
        if (i != counters[i].id) {
          if (prune_counter_precondition) {
            std::iter_swap(counter_preconditions.begin() + counters[i].id, counter_preconditions.begin() + i);
          }
          counters[counters[i].id] = counters[i];
        }
      }
    }
    counters.resize(new_id);

    if (prune_counter_precondition) {
      counter_preconditions.resize(new_id);
#ifndef NDEBUG
      std::cout << "updating counter preconditions..." << std::endl;
#endif
      for (unsigned i = 0; i < counter_preconditions.size(); i++) {
        for (unsigned j = 0; j < counter_preconditions[i].size(); j++) {
          assert(counter_preconditions[i][j] < new_conj_ids.size());
          assert(!removed[counter_preconditions[i][j]]);
          assert(new_conj_ids[counter_preconditions[i][j]] < conjunctions.size());
          counter_preconditions[i][j] = new_conj_ids[counter_preconditions[i][j]];
        }
      }
    }

#ifndef NDEBUG
    std::cout << "removing conjunctions..." << std::endl;
#endif
    new_id = 0;
    for (unsigned i = 0; i < conjunctions.size(); i++) {
      if (!removed[i]) {
        new_id++;
        conjunctions[i].id = new_conj_ids[i];
        if (new_conj_ids[i] != i) {
          std::iter_swap(_fluents.begin() + conjunctions[i].id, _fluents.begin() + i);
          std::iter_swap(conjunction_achievers.begin() + conjunctions[i].id, conjunction_achievers.begin() + i);
          conjunctions[conjunctions[i].id] = conjunctions[i];
        }
      }
    }
    _unique_conjunctions.clear();
    for (unsigned cid = 0; cid < new_id; cid++) {
      _unique_conjunctions.insert(cid);
    }
    conjunctions.resize(new_id);
    conjunction_achievers.resize(new_id);
    _fluents.resize(new_id);
  }
#ifndef NDEBUG
  std::cout << "updated all data structures" << std::endl;
#endif
  dump_compilation_information();
}

Fact HCHeuristic::get_fact(int var, int val) const
{
  return make_pair(var, val);
}

unsigned HCHeuristic::get_fact_id(int var, int val) const
{
  return variable_offset[var] + val;
}

unsigned HCHeuristic::get_fact_id(const Fact &fact) const
{
  return variable_offset[fact.first] + fact.second;
}

void HCHeuristic::create_unit_conjunction(int var,
                                          int val,  Fluent &fluent) const
{
  fluent.insert(get_fact(var, val));
}

void HCHeuristic::add_counter_precondition(ActionEffectCounter
                                           *counter,
                                           Conjunction *conj)
{
  bool dominated = false;
  if (prune_counter_precondition) {
    if (counter_preconditions.size() <= counter->id) {
      counter_preconditions.resize(counter->id + 1);
    }
    vector<unsigned>::iterator it = counter_preconditions[counter->id].begin();
    while (it != counter_preconditions[counter->id].end()) {
      if (is_subset(_fluents[conj->id], _fluents[*it])) {
        dominated = true;
        break;
      } else if (is_subset(_fluents[*it], _fluents[conj->id])) {
        conjunctions[*it].triggered_counters.erase(std::find(
                                                             conjunctions[*it].triggered_counters.begin(),
                                                             conjunctions[*it].triggered_counters.end(),
                                                             counter));
        it = counter_preconditions[counter->id].erase(it);
        counter->preconditions--;
      } else {
        it++;
      }
    }
    if (!dominated) {
      counter_preconditions[counter->id].push_back(conj->id);
    }
  }
  if (!dominated) {
    conj->triggered_counters.push_back(counter);
    counter->preconditions++;
  }
}

/*
 *  Compiling Fast Downwards planning task to STRIPS encoding and
 *  building graph for hmax computation
 *
 */
void HCHeuristic::compile_strips()
{
  cout << "Constructing Pi^C for h^max computation ..." << endl;

  conjunction_offset = 0;
  variable_offset.resize(g_variable_domain.size());
  for (uint var = 0; var < g_variable_domain.size(); var++) {
    variable_offset[var] = conjunction_offset;
    conjunction_offset += g_variable_domain[var];
  }

  facts_conjunctions_relation.resize(conjunction_offset);

  // Build a conjunction for every fact (pair<variable, value>)
  unsigned conj_id = 0;
  //conjunctions.reserve(conjunction_offset);
  _fluents.reserve(conjunction_offset);//set size
  for (uint var = 0; var < g_variable_domain.size(); var++) {
    for (int value = 0; value < g_variable_domain[var]; value++) {
      Fluent fluent;
      create_unit_conjunction(var, value, fluent);// will be insert to fluent
      //conjunctions.push_back(Conjunction(conj_id, fluent));
      conjunctions.push_back(Conjunction(conj_id, 1));//add this conjunction
      conjunction_achievers.push_back(std::vector<unsigned>());
      facts_conjunctions_relation[conj_id].push_back(conj_id);
      //_unique_conjunctions.insert(conj_id);
      _fluents.push_back(fluent);//fluent will be put into _fluents then _fluents are C
      _unique_conjunctions.insert(conj_id);
      conj_id++;//set the id for new conjunction
    }
  }

  m_true_id = conjunctions.size();
  conjunctions.push_back(Conjunction(m_true_id, 0));
  conjunction_achievers.push_back(std::vector<unsigned>());
  _fluents.push_back(Fluent());

  // Build strips actions
  actions.reserve(g_operators.size());
  //counters.reserve(g_operators.size() * 3);

  facts_to_add.resize(conjunction_offset);
  facts_to_del.resize(conjunction_offset);
  facts_to_counters.resize(conjunction_offset);

  unsigned action_id = 0;
  //cout<<"Cost_type "<<cost_type<<endl;s
  for (uint i = 0; i < g_operators.size(); i++) {
    
    const Operator &op = g_operators[i];
    //cout<<"Op base cost: "<<op.get_cost()<<endl;
    int base_cost = get_adjusted_cost(op);
    //cout<<"Adjusted base cost: "<<base_cost<<endl;
    const std::vector<Prevail> &pre = op.get_prevail();
    const std::vector<PrePost> &eff = op.get_pre_post();

    StripsAction action;
    action.base_cost = base_cost;
    action.operator_no = i;

    Fluent &precondition = action.precondition;
    Fluent &add_effect = action.add_effect;
    Fluent &del_effect = action.del_effect;

    for (uint i = 0; i < pre.size(); i++) {
      precondition.insert(get_fact(pre[i].var,
                                   pre[i].prev));
    }

    for (uint i = 0; i < eff.size(); i++) {
      if (eff[i].pre != -1) {
        Fact fact = get_fact(eff[i].var, eff[i].pre);
        precondition.insert(fact);
        del_effect.insert(fact);
        facts_to_del[get_fact_id(fact)].push_back(action_id);
      }
    }

    for (uint i = 0; i < eff.size(); i++) {
      int var = eff[i].var;
      int add = eff[i].post;

      if (eff[i].cond.empty()) {
        if (eff[i].pre == -1) {
          for (int val = 0; val < g_variable_domain[var]; val++) {
            if (val != add) {
              del_effect.insert(get_fact(var, val));
              facts_to_del[get_fact_id(var, val)].push_back(action_id);
            }
          }
        }
      }
    }


    if (c_dual_task) {
      del_effect.swap(precondition);
    }
    
    for (uint i = 0; i < eff.size(); i++) {
      int var = eff[i].var;
      int add = eff[i].post;

      if (eff[i].cond.empty()) {
        Fact fact = get_fact(var, add);
        add_effect.insert(fact);
        facts_to_add[get_fact_id(fact)].push_back(action_id);
        create_counter(action_id, get_fact_id(fact), base_cost, precondition);
      }
    }

    if (add_effect.size() > 0) {
      action.aid = action_id;
      actions.push_back(action);
      action_id++;
    }

    for (uint i = 0; i < eff.size(); i++) {
      int var = eff[i].var;
      int add = eff[i].post;

      if (!eff[i].cond.empty()) {
        Fact fact = get_fact(var, add);
        StripsAction condaction(action);

        Fluent *cond_pre = NULL;
        Fluent *cond_del = NULL;
        if (c_dual_task) {
          cond_pre = &condaction.del_effect;
          cond_del = &condaction.precondition;
        } else {
          cond_pre = &condaction.precondition;
          cond_del = &condaction.del_effect;
        }
        cond_del->clear();
        
        for (uint j = 0; j < eff[i].cond.size(); j++) {
          int pre_var = eff[i].cond[j].var;
          int pre_val = eff[i].cond[j].prev;
          cond_pre->insert(get_fact(pre_var, pre_val));
        }
        std::vector<int> isinpre; isinpre.resize(g_variable_domain.size(), -1);
        for (const auto &f : *cond_pre) {
          isinpre[f.first] = f.second;
        }
        condaction.add_effect.insert(fact);
        for (const auto &f : condaction.add_effect) {
          facts_to_add[get_fact_id(f)].push_back(action_id);
          if (isinpre[f.first] == -1) {
            for (int val = 0; val < g_variable_domain[f.first]; val++) {
              if (val != f.second) {
                cond_del->insert(get_fact(f.first, val));
              }
            }
          } else {
            cond_del->insert(get_fact(f.first, isinpre[f.first]));
          }
        }
        create_counter(action_id, get_fact_id(fact), base_cost, condaction.precondition);//for every effect create a effectcounter
        condaction.aid = action_id;
        actions.push_back(condaction);
        action_id++;
      }
    }
  }

  m_results_in_mutex.resize(num_facts());
  for (unsigned fid = 0; fid < num_facts(); fid++) {
    Fact fact = *get_fluent(fid).begin();
    for (unsigned aid = 0; aid < num_actions(); aid++) {
      const StripsAction &action = get_action(aid);
      if (!action.add_effect.count(fact)
          && !action.del_effect.count(fact)
          && !action.precondition.count(fact)) {
        //bool mutex = false;
        for (Fluent::iterator pre = action.precondition.begin();
             pre != action.precondition.end();
             pre++) {
          if (are_mutex(fact, *pre)) {
            m_results_in_mutex[fid].push_back(aid);
            //mutex = true;
            break;
          }
        }
        //if (!mutex) {
        //    for (Fluent::iterator added = action.add_effect.begin();
        //            added != action.add_effect.end();
        //            added++) {
        //        if (are_mutex(fact, *added)) {
        //            m_results_in_mutex[fid].push_back(aid);
        //            mutex = true;
        //            break;
        //        }
        //    }
        //}
      }
    }
  }

  m_goal_id = m_true_id + 1;
  conjunctions.push_back(Conjunction(m_goal_id, 0));
  conjunction_achievers.push_back(std::vector<unsigned>());
  _fluents.push_back(Fluent());

  //counter_preconditions.resize(counters.size());
  //conj_to_counters.resize(conjunctions.size());

  if (!c_dual_task) {
    m_goal_counter = counters.size();
    for (uint i = 0; i < g_goal.size(); i++) {
      _fluents[m_goal_id].insert(g_goal[i]);
    }
    create_counter(-1, m_goal_id, 1, _fluents[m_goal_id]);
  }

  _num_counters = counters.size();

  set_size_limit_ratio(max_ratio_repr_counters);

  cout << "Parsed Pi into "
       << conjunction_offset << " facts, "
       << actions.size() << " actions, and "
       << counters.size() << " counters." << endl;
  //for (uint i=0; i < actions.size() ; i++){ 
  //    cout<<"base_cost: "<<actions[i].base_cost<<endl;
  //}
  if (_m > 1) {
    cout << "Adding <= " << _m <<
      " sized conjunctions to C until limit is reached..."
         << endl;
    Fluent empty;
    add_subsets_m(empty, true, _m);
    dump_compilation_information();
  }

  load_conjunctions_from_file(__load_from_file);
}


#if 0
void HCHeuristic::create_inverse_actions_handle_no_precondition(
                                                                unsigned op_num,
                                                                unsigned &strips_action_id,
                                                                const Fluent &pre,
                                                                Fluent add,
                                                                Fluent del,
                                                                int cost,
                                                                const std::vector<std::pair<int, int> > &no_prec,
                                                                unsigned i
                                                                )
{
  if (i == no_prec.size()) {
    actions.push_back(StripsAction(
                                   op_num,
                                   pre,
                                   add,
                                   del,
                                   cost,
                                   strips_action_id));
    for (const Fact & f : del) {
      facts_to_del[get_fact_id(f)].push_back(strips_action_id);
    }
    for (const Fact & f : add) {
      facts_to_add[get_fact_id(f)].push_back(strips_action_id);
      create_counter(get_fact_id(f), cost, pre);
    }
    strips_action_id++;
  } else {
    for (int val = 0; val < g_variable_domain[no_prec[i].first]; val++) {
      if (val != no_prec[i].second) {
        add.insert(get_fact(no_prec[i].first, val));
        del.insert(get_fact(no_prec[i].first, no_prec[i].second));
      }
      create_inverse_actions_handle_no_precondition(
                                                    op_num,
                                                    strips_action_id,
                                                    pre,
                                                    add,
                                                    del,
                                                    cost,
                                                    no_prec,
                                                    i + 1);
    }
  }
}
#endif


bool HCHeuristic::fluent_in_state(const Fluent &pi,
                                  const State &state) const
{
  if (pi.empty()) {
    return false;
  }
  for (Fluent::iterator f = pi.begin(); f != pi.end(); f++) {
    if (state[f->first] != f->second) {
      return false;
    }
  }
  return true;
}

// heuristic computation
void HCHeuristic::enqueue_if_necessary(Conjunction *conj,
                                       ActionEffectCounter * /*counter*/, int cost)
{
  //if (conj->check_and_update(cost, counter)) {
  //    queue.push(cost, conj);
  //}
  if (conj->cost == -1 || conj->cost > cost) {
    conj->cost = cost;
    queue.push(cost, conj);
  }
}

void HCHeuristic::clear_exploration_queue()
{
  queue.clear();

  for (uint cid = 0; cid < conjunctions.size(); cid++) {
    conjunctions[cid].cost = -1;
  }

  for (uint i = 0; i < counters.size(); i++) {
    ActionEffectCounter &counter = counters[i];
    counter.unsatisfied_preconditions = counter.preconditions;
  }
}

void HCHeuristic::setup_exploration_queue_state(
                                                const State &state)
{
  for (uint cid = 0; cid < conjunctions.size(); cid++) {
    if (fluent_in_state(_fluents[cid], state)) {
      enqueue_if_necessary(&conjunctions[cid], NULL, 0);
    }
  }
  enqueue_if_necessary(&conjunctions[m_true_id], NULL, 0);
}

/*
 * Computation of hmax
 *
 */
void HCHeuristic::relaxed_exploration()
{
  int distance;
  Conjunction *conj = NULL;
  while (!queue.empty()) {
    pair<int, Conjunction *> top_pair = queue.pop();
    distance = top_pair.first;
    conj = top_pair.second;
    if (conj->cost < distance) {
      continue;
    }
    //if (termination_option == EXPLORE_GOAL_COST &&
    //    goal_cost_max >= 0 && current_cost > goal_cost_max) {
    //    break;
    //}
    if (early_termination && conj->id == m_goal_id) {
      //if (termination_option == EARLY_TERMINATION) {
      break;
      //} else if (termination_option == EXPLORE_GOAL_COST) {
      //    goal_cost_max = current_cost;
      //}
    }
    const vector<ActionEffectCounter *> &triggered_counters =
      conj->triggered_counters;
    for (uint i = 0; i < triggered_counters.size(); i++) {
      ActionEffectCounter *counter = triggered_counters[i];
      if (--counter->unsatisfied_preconditions == 0) {
        counter->cost = conj->cost;
        enqueue_if_necessary(counter->effect,
                             counter, counter->base_cost + counter->cost);
      }
    }
  }
}

int HCHeuristic::priority_traversal(const State &state)
{
  clear_exploration_queue();
  setup_exploration_queue_state(state);
  relaxed_exploration();
  return conjunctions[m_goal_id].is_achieved() ? conjunctions[m_goal_id].cost :
    DEAD_END;
}


int HCHeuristic::simple_traversal_setup(const std::vector<std::pair<int, int> >
                                        &state,
                                        std::vector<unsigned> &exploration)
{
  for (size_t i = 0; i < num_conjunctions(); i++) {
    get_conjunction(i).clear();
  }

  vector<int> subset;
  subset.resize(num_conjunctions(), 0);
  vector<pair<int, int> >::const_iterator it = state.begin();
  // TODO ASSUMPTION state is sorted in ascending order!!!!!!!
  //for (int var = g_variable_domain.size() - 1; var >= 0; var--) {
  for (unsigned var = 0; var < g_variable_domain.size(); var++) {
    if (it != state.end() && it->first == (int) var) {
      const vector<unsigned> &conjs = get_fact_conj_relation(get_fact_id(*it));
      for (size_t j = 0; j < conjs.size(); j++) {
        Conjunction &conj = get_conjunction(conjs[j]);
        if (++subset[conj.id] == conj.fluent_size) {
          conj.check_and_update(0, NULL);
          exploration.push_back(conj.id);
        }
      }
      it++;
    } else {
      for (int val = 0; val < g_variable_domain[var]; val++) {
        const vector<unsigned> &conjs = get_fact_conj_relation(get_fact_id(var, val));
        for (size_t j = 0; j < conjs.size(); j++) {
          Conjunction &conj = get_conjunction(conjs[j]);
          if (++subset[conj.id] == conj.fluent_size) {
            conj.check_and_update(0, NULL);
            exploration.push_back(conj.id);
          }
        }
      }
    }
  }
  subset.clear();
  conjunctions[m_true_id].check_and_update(0, NULL);
  exploration.push_back(m_true_id);

  for (uint i = 0; i < counters.size(); i++) {
    ActionEffectCounter &counter = counters[i];
    counter.unsatisfied_preconditions = counter.preconditions;
  }

  return exploration.size();
}

//int HCHeuristic::simple_traversal_setup(const vector<State> &states,
//        std::vector<int> &exploration)
//{
//    int lvl0 = 0;
//
//    exploration.clear();
//    exploration.reserve(conjunctions.size());
//
//    for (size_t i = 0; i < conjunctions.size(); i++) {
//        conjunctions[i].clear();
//    }
//
//    for (size_t j = 0; j < states.size(); j++) {
//        const State &state = states[j];
//        for (uint i = 0; i < conjunctions.size(); i++) {
//            Conjunction &conj = conjunctions[i];
//            if (!conj.is_achieved() && fluent_in_state(_fluents[i], state)) {
//                conj.check_and_update(0, NULL);
//                exploration.push_back(i);
//            }
//        }
//    }
//
//    lvl0 = exploration.size();
//
//    for (uint i = 0; i < counters.size(); i++) {
//        ActionEffectCounter &counter = counters[i];
//        counter.unsatisfied_preconditions = counter.preconditions;
//        if (counter.unsatisfied_preconditions == 0) {
//            if (counter.effect->check_and_update(1, NULL)) {
//                exploration.push_back(counter.effect->id);
//            }
//        }
//    }
//
//    return lvl0;
//}

// here we will need a the g value
int HCHeuristic::simple_traversal_setup(const State &state,
                                        std::vector<unsigned> &exploration)
{
  exploration.clear();
  exploration.reserve(conjunctions.size());
  //control the size of vector
  if (c_dual_task) {
    std::cout << "goal:" << std::endl;
    Fluent goal;
    for (uint i = 0; i < g_goal.size(); i++) {
      goal.insert(g_goal[i]);
    }
    for (uint i = 0; i < _fluents.size(); i++) {
      Conjunction &conj = conjunctions[i];
      conj.clear();
      if (i != m_true_id && i != m_goal_id && fluent_op::are_disjoint(goal, _fluents[i])) {
        conj.check_and_update(0, NULL);
        exploration.push_back(i);
        dump_fluent_pddl(_fluents[i]);
        std::cout << std::endl;
      }
    }
  } else {
    for (uint i = 0; i < conjunctions.size(); i++) {
      Conjunction &conj = conjunctions[i];
      conj.clear();//set cost to -1
      if (fluent_in_state(_fluents[i], state)) {
        conj.check_and_update(0, NULL);//its in the state
        exploration.push_back(i);//add back to queue
        //exploration only contains id
      }
    }
  }
  
  conjunctions[m_true_id].check_and_update(0, NULL);//set up and put in the last conjunction?
  exploration.push_back(m_true_id);

  for (uint i = 0; i < counters.size(); i++) { //for every effect
    ActionEffectCounter &counter = counters[i];
    counter.unsatisfied_preconditions = counter.preconditions;//all preconditons unsatisfied
    counter.cost = 0;//Initial to be zero
  }

  return exploration.size();
}

/******modified******/
//for every existing conjunction it will provide a h value ---> thats why it doesn't have a early termination
//
int HCHeuristic::simple_traversal_wrapper(
                                          std::vector<unsigned> &exploration, int lvl0)
{
  if (early_termination && conjunctions[m_goal_id].is_achieved()) {
    return 0;
  }
  //could simply store the values in the vector as exploration
  
  //int g = 0;//replace this with G_value
 
  unsigned i = 0;
  unsigned border = lvl0;
  int level = 0;
  int goal_level = 0;
  while (i < exploration.size()) {
    if (i == border) {//enlarge the exploration, when it go through one level
      border = exploration.size();
      //based on unit cost
      level += 1; //add one level -- the distance 
      //cout << "level enlarge"<<endl;
    }
    unsigned conj_id = exploration[i++]; 
    // think of how to store the cost properly
    const vector<ActionEffectCounter *> &triggered_counters =
      conjunctions[conj_id].triggered_counters;//The actions could be executed
    for (uint j = 0; j < triggered_counters.size(); j++) {
      ActionEffectCounter *counter = triggered_counters[j];//take the first one out 
      if (--counter->unsatisfied_preconditions > 0) { //there must be one satisfied so 1 should be minused
        continue; //can't be used -- jump
      }
      //counter is action --- the cost should inherit from the conj_id
      counter->cost = level;

      //if(level == 0) counter->cost = level;(init to be g)  //otherwise use the accummulated cost 
      //else counter->cost = conjunctions[conj_id].cost

      if (!counter->effect->is_achieved()) {//cost <0 ---> has not been explored 
        //directly add the base cost here
        
        //line 2:  if( counter->cost + actions[counter->action_id].base_cost+g > b) return DEAD_END;
        //if not early termination then continue -> jump the dead end???
        //counter->effect->check_and_update(counter->cost + actions[counter->action_id].base_cost, NULL);//add the cost , this will store the result
        
        counter->effect->check_and_update(level+1,NULL);
        //update the conjunction, the cost are stored in the conjunction
        exploration.push_back(counter->effect->id);//push in new counter
        if (m_goal_id == counter->effect->id) {
          goal_level = level + 1; // goal is achieved so it could be returned

          //stop here; goal_level = counter->cost + actions[counter->action_id].base_cost;
          
          if (early_termination) {
            return goal_level;
          }
        }
      }
    }
  }

  return conjunctions[m_goal_id].is_achieved() ? goal_level : DEAD_END; // goal level means the depth of goal 
  // here need to modified
}
//reload
int HCHeuristic::simple_traversal_wrapper(
                                          std::vector<unsigned> &exploration, int lvl0,int g_value)
{
  if (early_termination && conjunctions[m_goal_id].is_achieved()) {
    return 0;
  }
  //could simply store the values in the vector as exploration

  unsigned i = 0;
  unsigned border = lvl0;
  int level = 0;
  int goal_level = 0;
  while (i < exploration.size()) {
    if (i == border) {//enlarge the exploration, when it go through one level
      border = exploration.size();
      //based on unit cost
      level += 1; //add one level -- the distance 
      //cout << "level enlarge"<<endl;
    }
    unsigned conj_id = exploration[i++]; 
    // think of how to store the cost properly
    const vector<ActionEffectCounter *> &triggered_counters =
      conjunctions[conj_id].triggered_counters;//The actions could be executed
    for (uint j = 0; j < triggered_counters.size(); j++) {
      ActionEffectCounter *counter = triggered_counters[j];//take the first one out 
      if (--counter->unsatisfied_preconditions > 0) { //there must be one satisfied so 1 should be minused
        continue; //can't be used -- jump
      }
      //Initialize the pre-cost
      if(level == 0) counter->cost = g_value;//init to be g, otherwise use the accummulated cost 
      else counter->cost = conjunctions[conj_id].cost;
      //line 2:  
      if( counter->cost + actions[counter->action_id].base_cost > bound) {
        continue;//follow the can't achieve part
      }
      //counter is action --- the cost should inherit from the conj_id
      //counter->cost = level;
      if (!counter->effect->is_achieved()) {//cost <0 ---> has not been explored 
        //directly add the base cost here
        //if not early termination then continue -> jump the dead end???
        counter->effect->check_and_update(counter->cost + actions[counter->action_id].base_cost, NULL);//add the cost , this will store the result
        //counter->effect->check_and_update(level+1,NULL);

        //update the conjunction, the cost are stored in the conjunction
        exploration.push_back(counter->effect->id);//push in new counter
        if (m_goal_id == counter->effect->id) {
          //goal_level = level + 1; // goal is achieved so it could be returned
          goal_level = counter->cost + actions[counter->action_id].base_cost;          
          if (early_termination) {
            return goal_level;
          }
        }
      }
    }
  }
  return conjunctions[m_goal_id].is_achieved() ? goal_level : DEAD_END; // goal level means the depth of goal 
  // here need to modified
}

/******modified******/
int HCHeuristic::simple_traversal(const State &state)
{
  vector<unsigned> exploration;
  int x = simple_traversal_setup(state, exploration);
  //put in the size of exploration
  int res = simple_traversal_wrapper(exploration, x);
  exploration.clear();
  return res;
}

//reload
int HCHeuristic::simple_traversal(const State &state, int g_value)
{
  vector<unsigned> exploration;
  int x = simple_traversal_setup(state, exploration);
  //put in the size of exploration
  int res = simple_traversal_wrapper(exploration, x, g_value);
  exploration.clear();
  return res;
}
int HCHeuristic::compute_heuristic(const State &state)
{
  std::vector<unsigned> prec;
  //catch it successfully
  //cout<<"Heuristic catch the bound "<<bound<<endl;
  if (c_dual_task) {
    std::cout << "initial state: " << std::endl;
    m_goal_counter = counters.size();
    counters.push_back(ActionEffectCounter(-1, m_goal_counter, &conjunctions[m_goal_id], 1));

    Fluent goal;
    for (uint var = 0; var < g_variable_domain.size(); var++) {
      goal.insert(std::make_pair(var, state[var]));
    }
    for (uint i = 0; i < _fluents.size(); i++) {
      if (i != m_true_id && i != m_goal_id && fluent_op::are_disjoint(_fluents[i], goal)) {
        dump_fluent_pddl(_fluents[i]);
        std::cout << std::endl;
        prec.push_back(i);
        conjunctions[i].triggered_counters.push_back(&counters[m_goal_counter]);
        counters[m_goal_counter].preconditions++;
      }
    }
    if (prec.empty()) {
      prec.push_back(m_true_id);
    }
  }
  int res = simple_traversal(state);
  if (c_dual_task) {
    for (unsigned x : prec) {
      conjunctions[x].triggered_counters.pop_back();
    }
    counters.pop_back();
  }
  return res == DEAD_END ? res : res - 1;
}
//**reload*****
int HCHeuristic::compute_heuristic(const State &state,int g_value)
{
  std::vector<unsigned> prec;
  //catch it successfully
  //cout<<"Heuristic catch the bound "<<bound<<endl;
  if (c_dual_task) {
    std::cout << "initial state: " << std::endl;
    m_goal_counter = counters.size();
    counters.push_back(ActionEffectCounter(-1, m_goal_counter, &conjunctions[m_goal_id], 1));

    Fluent goal;
    for (uint var = 0; var < g_variable_domain.size(); var++) {
      goal.insert(std::make_pair(var, state[var]));
    }
    for (uint i = 0; i < _fluents.size(); i++) {
      if (i != m_true_id && i != m_goal_id && fluent_op::are_disjoint(_fluents[i], goal)) {
        dump_fluent_pddl(_fluents[i]);
        std::cout << std::endl;
        prec.push_back(i);
        conjunctions[i].triggered_counters.push_back(&counters[m_goal_counter]);
        counters[m_goal_counter].preconditions++;
      }
    }
    if (prec.empty()) {
      prec.push_back(m_true_id);
    }
  }
  int res = simple_traversal(state, g_value);
  if (c_dual_task) {
    for (unsigned x : prec) {
      conjunctions[x].triggered_counters.pop_back();
    }
    counters.pop_back();
  }
  return res == DEAD_END ? res : res - 1;
}

#if 0
bool HCHeuristic::contains_mutex(const Fluent &f1,
                                 const Fluent &f2)
{
  std::fill(m_mutex_subset.begin(), m_mutex_subset.end(), 0);
  int oldvar = -1;
  Fluent::const_iterator it1 = f1.begin(), it1End = f1.end();
  Fluent::const_iterator it2 = f2.begin(), it2End = f2.end();
  while (it1 != it1End && it2 != it2End) {
    if (it1->first < it2->first) {
      if (oldvar == it1->first) {
        return true;
      }
      oldvar = it1->first;
      for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it1)]) {
        if (++m_mutex_subset[x] == m_mutex_size[x]) {
          return true;
        }
      }
      it1++;
    } else if (it1->first == it2->first) {
      if (it1->second != it2->second) {
        return true;
      }
      if (oldvar == it1->first) {
        return true;
      }
      oldvar = it1->first;
      for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it1)]) {
        if (++m_mutex_subset[x] == m_mutex_size[x]) {
          return true;
        }
      }
      it1++;
      it2++;
    } else {
      if (oldvar == it2->first) {
        return true;
      }
      oldvar = it2->first;
      for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it2)]) {
        if (++m_mutex_subset[x] == m_mutex_size[x]) {
          return true;
        }
      }
      it2++;
    }
  }
  while (it1 != it1End) {
    if (it1->first == oldvar) {
      return true;
    }
    oldvar = it1->first;
    for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it1)]) {
      if (++m_mutex_subset[x] == m_mutex_size[x]) {
        return true;
      }
    }
    it1++;
  }
  while (it2 != it2End) {
    if (it2->first == oldvar) {
      return true;
    }
    oldvar = it2->first;
    for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it2)]) {
      if (++m_mutex_subset[x] == m_mutex_size[x]) {
        return true;
      }
    }
    it2++;
  }
  return false;
}

bool HCHeuristic::contains_mutex(const Fluent &fluent)
{
  std::fill(m_mutex_subset.begin(), m_mutex_subset.end(), 0);
  int oldvar = -1;
  Fluent::const_iterator it1 = fluent.begin(), it1End = fluent.end();
  while (it1 != it1End) {
    if (oldvar == it1->first) {
      return true;
    }
    oldvar = it1->first;
    for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it1)]) {
      if (++m_mutex_subset[x] == m_mutex_size[x]) {
        return true;
      }
    }
    it1++;
  }
  return false;
}

bool HCHeuristic::extract_mutex(const Fluent &f1,
                                const Fluent &f2,
                                Conflict &mutex)
{
  std::fill(m_mutex_subset.begin(), m_mutex_subset.end(), 0);
  std::pair<int, int> pf(-1, -1);
  Fluent::const_iterator it1 = f1.begin(), it1End = f1.end();
  Fluent::const_iterator it2 = f2.begin(), it2End = f2.end();
  while (it1 != it1End && it2 != it2End) {
    if (it1->first < it2->first) {
      if (pf.first == it1->first) {
        mutex.merge(pf);
        mutex.merge(*it1);
        return true;
      }
      for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it1)]) {
        if (++m_mutex_subset[x] == m_mutex_size[x]) {
          mutex.merge(m_mutex[x]);
          return true;
        }
      }
      pf = *it1;
      it1++;
    } else if (it1->first == it2->first) {
      if (it1->second != it2->second) {
        mutex.merge(*it1);
        mutex.merge(*it2);
        return true;
      }
      if (pf.first == it1->first) {
        mutex.merge(pf);
        mutex.merge(*it1);
        return true;
      }
      for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it1)]) {
        if (++m_mutex_subset[x] == m_mutex_size[x]) {
          mutex.merge(m_mutex[x]);
          return true;
        }
      }
      pf = *it1;
      it1++;
      it2++;
    } else {
      if (pf.first == it2->first) {
        mutex.merge(pf);
        mutex.merge(*it2);
        return true;
      }
      for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it2)]) {
        if (++m_mutex_subset[x] == m_mutex_size[x]) {
          mutex.merge(m_mutex[x]);
          return true;
        }
      }
      pf = *it2;
      it2++;
    }
  }
  while (it1 != it1End) {
    if (it1->first == pf.first) {
      mutex.merge(pf);
      mutex.merge(*it1);
      return true;
    }
    for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it1)]) {
      if (++m_mutex_subset[x] == m_mutex_size[x]) {
        mutex.merge(m_mutex[x]);
        return true;
      }
    }
    pf = *it1;
    it1++;
  }
  while (it2 != it2End) {
    if (it2->first == pf.first) {
      mutex.merge(pf);
      mutex.merge(*it2);
      return true;
    }
    for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it2)]) {
      if (++m_mutex_subset[x] == m_mutex_size[x]) {
        mutex.merge(m_mutex[x]);
        return true;
      }
    }
    pf = *it2;
    it2++;
  }
  return false;
}

bool HCHeuristic::extract_mutex(const Fluent &fluent,
                                Conflict &mutex)
{
  std::fill(m_mutex_subset.begin(), m_mutex_subset.end(), 0);
  std::pair<int, int> oldf(-1, -1);
  Fluent::const_iterator it1 = fluent.begin(), it1End = fluent.end();
  while (it1 != it1End) {
    if (oldf.first == it1->first) {
      mutex.merge(oldf);
      mutex.merge(*it1);
      return true;
    }
    oldf = *it1;
    for (const unsigned &x : m_fact_to_mutex[get_fact_id(*it1)]) {
      if (++m_mutex_subset[x] == m_mutex_size[x]) {
        mutex.merge(m_mutex[x]);
        return true;
      }
    }
    it1++;
  }
  return false;
}
#else
bool HCHeuristic::contains_mutex(const Fluent &f1, const Fluent &f2)
{
  for (Fluent::const_iterator i = f1.begin(); i != f1.end(); i++) {
    for (Fluent::const_iterator j = f2.begin(); j != f2.end(); j++) {
      if (are_mutex(*i, *j)) {
        return true;
      }
    }
  }
  return false;
}

bool HCHeuristic::extract_mutex(const Fluent &f1, const Fluent &f2, Conflict &mutex)
{
  for (Fluent::const_iterator i = f1.begin(); i != f1.end(); i++) {
    for (Fluent::const_iterator j = f2.begin(); j != f2.end(); j++) {
      if (are_mutex(*i, *j)) {
        mutex.merge(*i);
        mutex.merge(*j);
        return true;
      }
    }
  }
  return false;
}

bool HCHeuristic::contains_mutex(const Fluent &f)
{
  for (Fluent::const_iterator i = f.begin(); i != f.end(); i++) {
    Fluent::const_iterator j = i;
    while (++j != f.end()) {
      if (are_mutex(*i, *j)) {
        return true;
      }
    }
  }
  return false;
}

bool HCHeuristic::extract_mutex(const Fluent &f, Conflict &mutex)
{
  return extract_mutex(f, mutex.get_fluent());
}
bool HCHeuristic::extract_mutex(const Fluent &f, Fluent &mutex)
{
  for (Fluent::const_iterator i = f.begin(); i != f.end(); i++) {
    Fluent::const_iterator j = i;
    while (++j != f.end()) {
      if (are_mutex(*i, *j)) {
        mutex.insert(*i);
        mutex.insert(*j);
        return true;
      }
    }
  }
  return false;
}
#endif

void HCHeuristic::extract_all_conjunctions(const Fluent &fluent,
                                           std::vector<unsigned> &conjs_unr, std::vector<unsigned> &conjs, int cost) const
{
  vector<int> in_fluent;
  in_fluent.resize(conjunctions.size(), 0);
  for (Fluent::iterator f = fluent.begin(); f != fluent.end(); f++) {
    const vector<unsigned> &f_conjs = facts_conjunctions_relation[get_fact_id(*f)];
    for (uint i = 0; i < f_conjs.size(); i++) {
      unsigned cid = f_conjs[i];
      const Conjunction &conj = conjunctions[cid];
      if (++in_fluent[cid] == conj.fluent_size) {
        if (conj.is_achieved()) {
          conjs_unr.push_back(cid);
        } else if (conj.cost >= cost) {
          conjs.push_back(cid);
        }
      }
    }
  }
  in_fluent.clear();
}


void HCHeuristic::extract_all_conjunctions(const Fluent &fluent,
                                           std::vector<unsigned> &conjs)
{
  vector<int> in_fluent;
  in_fluent.resize(conjunctions.size(), 0);
  for (Fluent::iterator f = fluent.begin(); f != fluent.end(); f++) {
    const vector<unsigned> &f_conjs = facts_conjunctions_relation[get_fact_id(*f)];
    for (uint i = 0; i < f_conjs.size(); i++) {
      unsigned cid = f_conjs[i];
      in_fluent[cid]++;
      if (cid >= conjunction_offset &&
          //in_fluent[cid] == conjunctions[cid].fluent.size()) {
          in_fluent[cid] == conjunctions[cid].fluent_size) {
        conjs.push_back(cid);
      }
    }
  }
  in_fluent.clear();
}


/*
 * Updating conjunction set
 *
 */
bool HCHeuristic::add_subsets_m(const Fluent &/*base*/,
                                bool /*is_goal*/, unsigned to_go)
{
  unsigned current_size = 2;
  unsigned start = 0,
    end = conjunction_offset;
  while (current_size <= to_go) {
    for (unsigned j = start; j < end; j++) {
#if FILTER_NEGATED_ATOMS
      if (g_is_negated_atom[_fluents[j].begin()->first][_fluents[j].begin()->second]) {
        continue;
      }
#endif
      for (unsigned i = 0; i < conjunction_offset; i++) {
        const Fluent &fact = _fluents[i];
        if (_fluents[j].find(*fact.begin()) != _fluents[j].end()) {
          continue;
        }
#if FILTER_NEGATED_ATOMS
        if (g_is_negated_atom[fact.begin()->first][fact.begin()->second]) {
          continue;
        }
#endif
        if (hard_size_limit && exceeded_size_limit()) {
          return false;
        }
        Fluent pi(_fluents[j]);
        pi.insert(*fact.begin());
        add_conflict(Conflict(pi));
      }
    }
    start = end;
    end = conjunctions.size();
    current_size++;
  }
  return true;
}

void HCHeuristic::compute_can_add(std::vector<int> &can_add_fluent,
                                  const Fluent &fluent,
                                  bool mutex_check)
{
  can_add_fluent.resize(actions.size(), 0);
  // can_add_fluent[a] =
  //      -1: deletes some part of conjunction
  //      0: neither adds nor removes part of conjunction
  //      1: adds part of conjunction, but does not remove any part of
  //          conjunction
  for (Fluent::iterator f = fluent.begin(); f != fluent.end(); f++) {
    const vector<unsigned> &del_acts = facts_to_del[get_fact_id(*f)];//The act del the fact *f
    for (uint j = 0; j < del_acts.size(); j++) {//set delet effect to -1
      int aid = del_acts[j];
      can_add_fluent[aid] = -1;
    }
    if (mutex_check) {//require check mutex
      const vector<unsigned> &mutual_exclusive = m_results_in_mutex[get_fact_id(*f)];
      for (uint i = 0; i < mutual_exclusive.size(); i++) {
        int aid = mutual_exclusive[i];
        can_add_fluent[aid] = -1;//set mutex to -1
      }
    }
    const vector<unsigned> &add_acts = facts_to_add[get_fact_id(*f)];
    for (uint i = 0; i < add_acts.size(); i++) {
      int aid = add_acts[i];
      if (can_add_fluent[aid] >= 0) {
        can_add_fluent[aid] = 1;
      }
    }
  }
}

void HCHeuristic::update_triggered_counters(unsigned conj_id)
{
  Conjunction &conj = conjunctions[conj_id];
  const Fluent &pi = _fluents[conj_id];

  // update counters where the current conjunction is in precondition
  // computing the set of existing counters where conj is in precondition
  //const set<int> &rel_counters = it->precondition_of;

  vector<int> comp_counters;
  comp_counters.resize(counters.size(), 0);
  for (Fluent::iterator fact = pi.begin(); fact != pi.end(); fact++) {
    unsigned fact_id = get_fact_id(*fact);
    facts_conjunctions_relation[fact_id].push_back(conj_id);
    for (uint i = 0; i < facts_to_counters[fact_id].size(); i++) {
      ActionEffectCounter *counter = facts_to_counters[fact_id][i];
      if (++comp_counters[counter->id] == conj.fluent_size) {
        add_counter_precondition(counter, &conj);
      }
    }
  }
  comp_counters.clear();
}

void HCHeuristic::create_counter(unsigned action, unsigned conj_id, int cost,
                                 const Fluent &precondition)
{
  unsigned counter_id = counters.size();
  counters.push_back(ActionEffectCounter(action, counter_id,
                                         &conjunctions[conj_id],
                                         cost));
  //if (conj_id >= conjunction_achievers.size()) {
  //    conjunction_achievers.resize(conj_id + 1);
  //}
  conjunction_achievers[conj_id].push_back(counter_id);
  //counter_preconditions.push_back(vector<int>());
  create_counter(counter_id, precondition);
}

template<typename T>
void HCHeuristic::create_counter(unsigned counter_id,
                                 const T &precondition)
{
  ActionEffectCounter &counter = counters[counter_id];

  if (!precondition.empty()) {
    vector<int> subset;
    subset.resize(conjunctions.size(), 0);
    for (typename T::const_iterator it = precondition.begin();
         it != precondition.end();
         it++) {
      unsigned fid = get_fact_id(*it);
      facts_to_counters[fid].push_back(&counter);
      const vector<unsigned> &conjs = facts_conjunctions_relation[fid];
      for (size_t i = 0; i < conjs.size(); i++) {
        const unsigned &cid = conjs[i];
        if (++subset[cid] == conjunctions[cid].fluent_size) {
          add_counter_precondition(&counter, &conjunctions[cid]);
        }
      }
    }
    subset.clear();
  } else {
    add_counter_precondition(&counter, &conjunctions[m_true_id]);
  }
}

void HCHeuristic::add_conflict(const Conflict &confl)
{
  if (contains_mutex(confl.get_fluent())) {
    return;
  }

  unsigned conj_id = conjunctions.size();

  /* Update existing part */
  // add new conjunction to conjunctions set
  //conjunctions.push_back(Conjunction(conj_id, it->get_fluent()));
  // check whether conjunction existed before
  _fluents.push_back(confl.get_fluent());
  auto res = _unique_conjunctions.insert(conj_id);
  //pair<set<int>::iterator, bool> res;
  //res = _unique_conjunctions.insert(conj_id);
  if (!res.second) {
    // conjunction was already contained in conjunction set
    _fluents.pop_back();
    return;
  }

  // create new conjunction
  current_fluents++;
  conjunctions.push_back(Conjunction(conj_id, confl.get_fluent().size()));
  conjunction_achievers.push_back(std::vector<unsigned>());
  // if conjunction is goal, update goal_conjunctions accordingly

  update_triggered_counters(conj_id);

  vector<int> can_add_fluent;
  compute_can_add(can_add_fluent, confl.get_fluent(), true);

  // add counters that add the new conjunction
  for (uint act = 0; act < can_add_fluent.size(); act++) {
    if (can_add_fluent[act] == 1) {
      const StripsAction &action = actions[act];

      //if (contains_mutex(confl.get_fluent(), action.precondition) ||
      //    contains_mutex(confl.get_fluent(), action.add_effect)) {
      //    continue;
      //}

      Fluent precondition;
      // precondition of new action consists of two parts
      //  (a) pre(a) \cup (c \setminus \add(a))
      //  (b) \pi_{c'} for every c' \in C and
      //          c' \subseteq (pre(a) \cup (c \setminus \add(a)))
      precondition.insert(action.precondition.begin(),
                          action.precondition.end());
      set_minus(confl.get_fluent(), action.add_effect, precondition);
      //assert(!contains_mutex(precondition));
      // (contains_mutex(confl.get_fluent(), action.add_effect));
      if (!contains_mutex(precondition)) {
        create_counter(act, conj_id, action.base_cost, precondition);
      }
      precondition.clear();
    }
  }
}

#if 0
void HCHeuristic::update_mutex()
{
  std::cout << "updating mutex information..." << std::endl;
  simple_traversal(g_initial_state());
  if (!conjunctions[m_goal_id].is_achieved()) {
    std::cout << "initial state is dead end!" << std::endl;
  }

  unsigned new_id = 0;
  for (unsigned counter = 0; counter < counters.size(); counter++) {
    if (counters[counter].id == m_goal_counter) {
      m_goal_counter = new_id;
      counters[counter].id = new_id++;
      counters[counter].unsatisfied_preconditions = 0;
    } else if (counters[counter].unsatisfied_preconditions == 0) {
      counters[counter].id = new_id++;
    }
  }

  std::cout << "finding new mutex..." << std::endl;
  std::vector<bool> subsumed; subsumed.resize(m_mutex.size(), false);
  std::vector<unsigned> idmap; idmap.resize(m_mutex.size());
  new_id = 0;
  for (unsigned cid = 0; cid < conjunctions.size(); cid++) {
    if (!conjunctions[cid].is_achieved() && cid != m_goal_id) {
      bool insert = true;
      bool subsumes_sth = false;
      std::fill(m_mutex_subset.begin(), m_mutex_subset.end(), 0);
      std::fill(subsumed.begin(), subsumed.end(), false);
      for (const Fact &f : _fluents[cid]) {
        for (const unsigned &x : m_fact_to_mutex[get_fact_id(f)]) {
          if (++m_mutex_subset[x] == m_mutex_size[x]) {
            insert = false;
            break;
          }
          if (m_mutex_subset[x] == conjunctions[cid].fluent_size) {
            subsumed[x] = true;
            subsumes_sth = true;
          }
        }
        if (!insert) {
          break;
        }
      }
      if (insert) {
        if (subsumes_sth) {
          unsigned new_mid = 0;
          for (unsigned mid = 0; mid < m_mutex.size(); mid++) {
            if (!subsumed[mid]) {
              if (mid != new_mid) {
                std::iter_swap(m_mutex.begin() + new_mid, m_mutex.begin() + mid);
                m_mutex_size[new_mid] = m_mutex_size[mid];
              }
              idmap[mid] = new_mid++;
            }
          }
          for (unsigned fid = 0; fid < m_fact_to_mutex.size(); fid++) {
            std::vector<unsigned> copy;
            copy.swap(m_fact_to_mutex[fid]);
            for (unsigned i = 0; i < copy.size(); i++) {
              if (!subsumed[copy[i]]) {
                m_fact_to_mutex[fid].push_back(idmap[copy[i]]);
              }
            }
          }
        }
        m_mutex.push_back(_fluents[cid]);
        m_mutex_size.push_back(conjunctions[cid].fluent_size);
        for (const Fact &f : _fluents[cid]) {
          m_fact_to_mutex[get_fact_id(f)].push_back(m_mutex.size() - 1);
        }
        m_mutex_subset.resize(m_mutex.size());
        subsumed.resize(m_mutex.size());
        idmap.resize(m_mutex.size());
      }
    }
    if (cid == m_goal_id) {
      //m_goal_id = new_id;
      conjunctions[m_goal_id].id = new_id++;
    } else if (conjunctions[cid].is_achieved() || cid < conjunction_offset) {
      conjunctions[cid].id = new_id++;
    }
  }

  std::cout << "found " << (conjunctions.size() - new_id) << " new mutexes" << std::endl;
  

  std::cout << "updating fact2X relations..." << std::endl;
  for (unsigned fid = 0; fid < conjunction_offset; fid++) {
    std::vector<unsigned> old;
    old.swap(facts_conjunctions_relation[fid]);
    for (const unsigned &oldcid : old) {
      if (conjunctions[oldcid].is_achieved() || oldcid == m_goal_id || oldcid < conjunction_offset) {
        facts_conjunctions_relation[fid].push_back(conjunctions[oldcid].id);
      }
    }
    std::vector<ActionEffectCounter *> o;
    o.swap(facts_to_counters[fid]);
    for (unsigned i = 0; i < o.size(); i++) {
      if (o[i]->unsatisfied_preconditions == 0) {
        facts_to_counters[fid].push_back(&counters[o[i]->id]);
      }
    }
  }

  std::cout << "updating counter effects..." << std::endl;
  for (unsigned counter = 0; counter < counters.size(); counter++) {
    if (counters[counter].unsatisfied_preconditions == 0) {
      counters[counter].effect = &conjunctions[counters[counter].effect->id];
    }
  }

  std::cout << "updating conjunctions..." << std::endl;
  for (unsigned cid = 0; cid < conjunctions.size(); cid++) {
    Conjunction &conj = conjunctions[cid];
    if (conj.is_achieved()) {
      std::vector<ActionEffectCounter *> old;
      old.swap(conj.triggered_counters);
      for (unsigned i = 0; i < old.size(); i++) {
        if (old[i]->unsatisfied_preconditions == 0) {
          conj.triggered_counters.push_back(&counters[old[i]->id]);
        }
      }
      std::vector<unsigned> ach;
      ach.swap(conjunction_achievers[cid]);
      for (unsigned i = 0; i < ach.size(); i++) {
        ActionEffectCounter &counter = counters[ach[i]];
        if (counter.unsatisfied_preconditions == 0) {
          conjunction_achievers[cid].push_back(counter.id);
        }
      }
    } else {
      conj.triggered_counters.clear();
      conjunction_achievers[cid].clear();
    }
  }

  std::cout << "removing obsolete counters..." << std::endl;
  unsigned new_size = 0;
  for (unsigned counter = 0; counter < counters.size(); counter++) {
    if (counters[counter].unsatisfied_preconditions == 0) {
      unsigned newc = counters[counter].id;
      std::vector<unsigned> old;
      old.swap(counter_preconditions[counter]);
      counter_preconditions[newc].clear();
      for (unsigned i = 0; i < old.size(); i++) {
        counter_preconditions[newc].push_back(conjunctions[old[i]].id);
      }
      if (counters[counter].id != counter) {
        counters[counters[counter].id] = counters[counter];
      }
      new_size++;
    }
  }
  std::cout << "removed " << (counters.size() - new_size) << " counters" << std::endl;
  counters.resize(new_size);

  std::cout << "removing obsolete conjunctions..." << std::endl;
  new_size = conjunction_offset;
  for (unsigned cid = conjunction_offset; cid < conjunctions.size(); cid++) {
    if (cid == m_true_id) {
      assert(conjunctions[cid].is_achieved());
      m_true_id = conjunctions[cid].id;
    }
    if ((conjunctions[cid].is_achieved() || cid == m_goal_id)) {
      if (conjunctions[cid].id != cid) {
        if (cid == m_goal_id) {
          m_goal_id = conjunctions[cid].id;
        }
        std::iter_swap(_fluents.begin() + cid, _fluents.begin() + conjunctions[cid].id);
        conjunctions[conjunctions[cid].id] = conjunctions[cid];
      }
      new_size++;
    }
  }
  std::cout << "removed " << (conjunctions.size() - new_size) << " conjunctions" << std::endl;
  conjunctions.resize(new_size);
  _fluents.resize(new_size);

  for (unsigned cid = new_size; cid < conjunction_offset + current_fluents; cid++) {
    _unique_conjunctions.erase(cid);
  }
  current_fluents = new_size - conjunction_offset;

  std::cout << "done updating mutex information" << std::endl;
}
#endif

bool HCHeuristic::update_c(
                           set<Conflict> &new_conjunctions)
{
  if (new_conjunctions.size() == 0) {
    return true;
  }

  if (exceeded_size_limit()) {
    return false;
  }

  /* Instead of recompiling the whole task, we simply extend the existing
   * compiliation by introducing the new conjunctions, updating the
   * precondition of the existing actions, and adding new actions
   * accordingly */

  bool added_all_conjunctions = true;
  //conjunctions.reserve(conjunctions.size() + new_conjunctions.size());
  for (set<Conflict>::iterator it = new_conjunctions.begin();
       it != new_conjunctions.end(); it++) {
    if (hard_size_limit && exceeded_size_limit()) {
      added_all_conjunctions = false;
      break;
    }
    add_conflict(*it);
  }

  //if (added_all_conjunctions && c_update_mutex) {
  //  update_mutex();
  //}

  return added_all_conjunctions;
}


//template<typename V>
//bool HCHeuristic::update_c_v(
//                           const V &new_conjunctions)
//{
//  if (new_conjunctions.size() == 0) {
//    return true;
//  }
//
//  if (exceeded_size_limit()) {
//    return false;
//  }
//
//  /* Instead of recompiling the whole task, we simply extend the existing
//   * compiliation by introducing the new conjunctions, updating the
//   * precondition of the existing actions, and adding new actions
//   * accordingly */
//
//  bool added_all_conjunctions = true;
//  //conjunctions.reserve(conjunctions.size() + new_conjunctions.size());
//  for (unsigned i = 0; i < new_conjunctions.size(); i++) {
//    if (hard_size_limit && exceeded_size_limit()) {
//      added_all_conjunctions = false;
//      break;
//    }
//    add_conflict(new_conjunctions[i]);
//  }
//
//  //if (added_all_conjunctions && c_update_mutex) {
//  //  update_mutex();
//  //}
//
//  return added_all_conjunctions;
//}


void HCHeuristic::reset_size_limit_ratio()
{
  if (max_ratio_repr_counters < 0) {
    max_num_counters = -1;
  } else {
    max_num_counters = (int)(max_ratio_repr_counters * _num_counters);
  }
}

void HCHeuristic::set_size_limit_ratio(float ratio)
{
  if (ratio < 0) {
    max_num_counters = -1;
  } else {
    max_num_counters = (int)(ratio * _num_counters);
  }
  //std::cout << "SIZE LIMIT SET TO " << ratio << std::endl;
  //std::cout << "MAX#COUNTERS: " << max_num_counters << std::endl;
  //std::cout << _num_counters << std::endl;
  //std::cout << exceeded_size_limit() << std::endl;
}

bool HCHeuristic::reached_max_conjunctions() const
{
  return (max_number_fluents != (unsigned) - 1 &&
          current_fluents >= max_number_fluents);
}

bool HCHeuristic::exceeded_size_limit() const
{
  return (max_number_fluents != (unsigned) - 1 &&
          current_fluents >= max_number_fluents) ||
    (max_num_counters != (unsigned) - 1 &&
     counters.size() >= max_num_counters);
}





string HCHeuristic::get_description() const
{
  return "hc";
}
void HCHeuristic::dump_options(std::ostream &out) const
{
  out << "Maximal size of conjunctions set: ";
  if (max_number_fluents == (unsigned) - 1) {
    out << "unlimited" << endl;
  } else {
    out << max_number_fluents << endl;
  }
  out << "Maximal number of counters: ";
  if (max_num_counters == (unsigned) - 1) {
    out << "unlimited" << endl;
  } else {
    out << max_num_counters << " (ratio: "
        << ((max_num_counters / _num_counters)) << ")"
        << endl;
  }
  cout << "Hard size limit: " << hard_size_limit << endl;
}
void HCHeuristic::dump_statistics(std::ostream &out) const
{
  dump_compilation_information(out);
  if (c_dump_conjunction_set) {
    std::ofstream fout("conjunctions.txt");
    dump_conjunctions_to_ostream(fout);
    fout.close();
  }
#if TIME_ANALYSIS
  out << "["
      << ((double) G_t1 / CLOCKS_PER_SEC)
      << "]" << endl;
#endif
#if DUMP_CONJUNCTIONS_
  //dump_c_pddl(true, false, false);
  std::ofstream fout("pi_fluents.txt");
  dump_conjunctions_to_ostream(fout);
  fout.close();
#endif
}


void HCHeuristic::dump_heuristic_values(std::ostream &out) const
{
  for (size_t i = 0; i < conjunctions.size(); i++) {
    out << "(" << i << ") ";
    dump_fluent_pddl(_fluents[i], out);
    out << ": ";
    if (!conjunctions[i].is_achieved()) {
      out << "infinity" << endl;
    } else {
      out << conjunctions[i].cost << endl;
    }
  }
}

void HCHeuristic::dump_statistics_json(std::ostream &out) const
{
  out << ","
      << "\"pic_size_C\":" << current_fluents << ","
      << "\"pic_max_size_C\":" << max_number_fluents << ","
      << "\"pic_num_facts\":" << conjunction_offset << ","
      << "\"pic_num_actions\":" << actions.size() << ","
      << "\"pic_num_counters\":" << counters.size() << ","
      << "\"pic_num_repr_counters\":" << counters.size() - _num_counters << ","
      << "\"pic_max_num_counters\":" << max_num_counters;
}

void HCHeuristic::dump_compilation_information(ostream &out) const
{
  out << "Pi^C: "
      << conjunction_offset << " facts, "
      << current_fluents << " conjunctions, "
    //<< m_mutex.size() << " mutexes, "
      << counters.size() << " (" << _num_counters << ") counters, "
      << get_counter_ratio() << " counter ratio"
      << endl;
}

//void sas_operator_prevail(const std::vector<std::vector<unsigned> > &ftoc,
//                          const StripsAction &action,
//                          std::vector<unsigned> &res)
//{
//    Fluent prevail;
//}

void HCHeuristic::generate_sas_operator(
                                        const std::vector<unsigned> &var_ids,
                                        const StripsAction &action,
                                        const std::vector<bool> &ineff_fixed,
                                        const std::vector<bool> &ineff,
                                        std::vector<std::string> &operators)
{
  size_t num_in_eff = 0;
  size_t num_in_pre = 0;
  Fluent prec(action.precondition);
  std::vector<bool> inpre(num_conjunctions(), false);
  for (unsigned cid = 0; cid < ineff.size(); cid++) {
    if (ineff[cid] || ineff_fixed[cid]) {
      num_in_eff++;
      fluent_op::set_minus(get_fluent(cid), action.add_effect, prec);
    }
  }

  if (contains_mutex(prec)) {
    return;
  }

  std::vector<unsigned> subs(num_conjunctions(), 0);
  for (Fluent::iterator it = prec.begin(); it != prec.end(); it++) {
    const std::vector<unsigned> &conjs = get_fact_conj_relation(get_fact_id(*it));
    for (unsigned conj : conjs) {
      if (++subs[conj] == get_fluent(conj).size()) {
        inpre[conj] = true;
        num_in_pre++;
      }
    }
  }

  std::ostringstream out;
  out << "begin_operator" << std::endl;
  out << g_operators[action.operator_no].get_name() << " ";
  bool sep = false;
  for (unsigned cid = num_facts(); cid < ineff.size(); cid++) {
    if (ineff[cid] || ineff_fixed[cid]) {
      if (sep) out << ", ";
      sep = true;
      dump_fluent_pddl(get_fluent(cid), out, true);
    }
  }
  out << std::endl;

  out << num_in_pre << std::endl;
  for (unsigned cid = 0; cid < inpre.size(); cid++) {
    if (inpre[cid]) {
      out << var_ids[cid] << " 1" << std::endl;
    }
  }

  out << num_in_eff << std::endl;
  for (unsigned cid = 0; cid < ineff.size(); cid++) {
    if (ineff[cid] || ineff_fixed[cid]) {
      out << "0 " << var_ids[cid] << " -1 1" << std::endl;
    }
  }

  out << "0" << std::endl
      << "end_operator";

  operators.push_back(out.str());
}

void HCHeuristic::dump_compiled_task_to_file(std::string path)
{
  std::ofstream out;
  out.open(path);

  out << "begin_version" << std::endl
      << "3" << std::endl
      << "end_version" << std::endl
      << "begin_metric" << std::endl
      << "0" << std::endl
      << "end_metric" << std::endl;

  size_t num_variables = 0;
  for (unsigned conj = 0; conj < num_conjunctions(); conj++) {
    if (conj == m_true_id || conj == m_goal_id) {
      continue;
    }
#if FILTER_NEGATED_ATOMS
    if (conj < num_facts() && g_is_negated_atom[_fluents[conj].begin()->first][_fluents[conj].begin()->second]) {
      continue;
    }
#endif
    num_variables++;
  }
  out << num_variables << std::endl;

  std::vector<unsigned> var_ids;
  var_ids.resize(num_conjunctions(), -1);
  unsigned id = 0;
  for (unsigned conj = 0; conj < num_conjunctions(); conj++) {
    if (conj == m_true_id || conj == m_goal_id) {
      continue;
    }
#if FILTER_NEGATED_ATOMS
    if (conj < num_facts() && g_is_negated_atom[_fluents[conj].begin()->first][_fluents[conj].begin()->second]) {
      continue;
    }
#endif
    var_ids[conj] = id;
    out << "begin_variable" << std::endl;
    out << "var" << id << std::endl
        << "-1" << std::endl
        << "2" << std::endl;
    out << "NegatedAtom ";
    dump_fluent_pddl(get_fluent(conj), out, true);
    out << std::endl;
    dump_fluent_pddl(get_fluent(conj), out, true);
    out << std::endl;
    out << "end_variable" << std::endl;
    id++;
  }

  out << "0" << std::endl;
  out << "begin_state" << std::endl;
  State init = g_initial_state();
  for (unsigned conj = 0; conj < num_conjunctions(); conj++) {
    if (var_ids[conj] == (unsigned) -1) {
      continue;
    }
    const Fluent &pi = get_fluent(conj);
    bool i = true;
    for (Fluent::iterator it = pi.begin(); it != pi.end(); it++) {
      if (init[it->first] != it->second) {
        i = false;
        break;
      }
    }
    out << (i ? "1" : "0") << std::endl;
  }
  out << "end_state" << std::endl;
  out << "begin_goal" << std::endl;
  out << counters[m_goal_counter].preconditions << std::endl;
  const std::vector<unsigned> &goal = counter_preconditions[m_goal_counter];
  for (unsigned i = 0; i < goal.size(); i++) {
    out << var_ids[goal[i]] << " 1" << std::endl;
  }
  out << "end_goal" << std::endl;

  std::vector<std::vector<unsigned> > subsumes;
  subsumes.resize(num_conjunctions());
  for (unsigned i = 0; i < num_conjunctions(); i++) {
    if (i == m_true_id || i == m_goal_id) {
      continue;
    }
    std::vector<unsigned> subs(num_conjunctions(), 0);
    for (Fluent::iterator f = get_fluent(i).begin(); f != get_fluent(i).end();
         f++) {
      const std::vector<unsigned> &conjs =
        facts_conjunctions_relation[get_fact_id(*f)];
      for (unsigned conj : conjs) {
        if (++subs[conj] == get_fluent(conj).size()) {
          subsumes[i].push_back(conj);
        }
      }
    }
  }

  std::vector<std::string> operators;
  for (unsigned act = 0; act < num_actions(); act++) {
    std::vector<int> canadd(num_conjunctions(), 0);
    const StripsAction &a = get_action(act);
    for (Fluent::iterator f = a.add_effect.begin(); f != a.add_effect.end(); f++) {
      const std::vector<unsigned> &x = facts_conjunctions_relation[get_fact_id(*f)];
      for (unsigned cid : x) {
        canadd[cid] = 1;
      }
    }
    for (Fluent::iterator f = a.del_effect.begin(); f != a.del_effect.end(); f++) {
      const std::vector<unsigned> &x = facts_conjunctions_relation[get_fact_id(*f)];
      for (unsigned cid : x) {
        canadd[cid] = -1;
      }
    }
    for (unsigned cid = 0; cid < canadd.size(); cid++) {
      if (canadd[cid] == 1) {
        Fluent merged(a.precondition);
        fluent_op::set_minus(get_fluent(cid), a.add_effect, merged);
        if (contains_mutex(merged)) {
          canadd[cid] = -2;
        }
      }
    }

    std::vector<bool> ineff_fix(num_conjunctions(), false);

    std::vector<unsigned> subset(num_conjunctions(), 0);
    for (Fluent::iterator it = a.add_effect.begin(); it != a.add_effect.end(); it++) {
      const std::vector<unsigned> &conjs = get_fact_conj_relation(get_fact_id(*it));
      for (unsigned conj : conjs) {
        if (++subset[conj] == get_fluent(conj).size()) {
          ineff_fix[conj] = true;
        }
      }
    }
    subset.clear();


    std::cout << (act+1) << "/" << num_actions() << ": "
              << g_operators[a.operator_no].get_name() << std::endl;

    std::vector<unsigned> possible_eff;
    std::vector<bool> enabled_eff;
    for (unsigned cid = 0; cid < canadd.size(); cid++) {
      if (canadd[cid] == 1 && !ineff_fix[cid]) {
        possible_eff.push_back(cid);
        enabled_eff.push_back(false);
        //std::cout << "    ";
        //dump_fluent_pddl(get_fluent(cid));
        //std::cout << std::endl;
      }
    }
    //std::cout << enabled_eff.size() << std::endl;

    while (true) {
      bool valid = true;
      std::vector<bool> ineff(num_conjunctions(), false);
      for (uint i = 0; i < enabled_eff.size(); i++) {
        if (enabled_eff[i]) {
          ineff[possible_eff[i]] = true;
        }
      }
      for (uint i = 0; valid && i < enabled_eff.size(); i++) {
        if (enabled_eff[i]) {
          const std::vector<unsigned> &sub = subsumes[possible_eff[i]];
          for (unsigned conj : sub) {
            if (canadd[conj] == 1 && !ineff[conj] && !ineff_fix[conj]) {
              valid = false;
              break;
            }
          }
        }
      }
      if (valid) {
        generate_sas_operator(var_ids, a, ineff_fix, ineff, operators);
      }
      unsigned i = 0;
      while (i < enabled_eff.size() && enabled_eff[i]) {
        enabled_eff[i++] = false;
      }
      if (i == enabled_eff.size()) {
        break;
      }
      enabled_eff[i] = true;
    }
  }

  out << operators.size() << std::endl;
  for (const std::string &op : operators) {
    out << op << std::endl;
  }
  out << "0" << std::endl;

  out.close();
}

void HCHeuristic::add_options_to_parser(OptionParser &parser)
{
  Heuristic::add_options_to_parser(parser);

  //parser.add_option<int>("c", "maximum size of conjunction set C", "-1");
  //parser.add_option<float>("x",
  //                         "maximum size of the compilation Pi^C relative to Pi", "-1");
  parser.add_option<bool>("hard_limit", "hard size limit", "true");
  parser.add_option<int>("m", "adding all conjunctions of size <= m", "0");
  parser.add_option<bool>("prune_pre", "prune precondition", "true");
  parser.add_option<string>("file", "load conjunctions from file", "none");
  parser.add_option<bool>("early_term", "", "false");
  parser.add_option<bool>("dump_conjunctions", "", "false");
  parser.add_option<bool>("dual", "", "false");
  parser.add_option<bool>("update_mutex", "", "false");
}

void HCHeuristic::add_default_options(Options &o)
{
  o.set<int>("cost_type", 0);
  o.set<bool>("dump_conjunctions", false);
  o.set<bool>("update_mutex", false);
  o.set<bool>("hard_size_limit", false);
  o.set<int>("m", 0);
  o.set<std::string>("file", "");
  o.set<bool>("prune_pre", true);
  o.set<bool>("early_term", false);
  o.set<bool>("dual", false);
}

static Heuristic *_parse(OptionParser &parser)
{
  HCHeuristic::add_options_to_parser(parser);

  Options opts = parser.parse();

  if (parser.dry_run()) {
    return 0;
  } else {
    return new HCHeuristic(opts);
  }
}

static Plugin<Heuristic> _plugin("hc", _parse);


