
#include "tarjan_dfs.h"

#include "../globals.h"

#include "../state_registry.h"
#include "../successor_generator.h"

#include "../option_parser.h"
#include "../plugin.h"

#include "../heuristic.h"
#include "../heuristic_refiner.h"

#include "../pruning_method.h"

#include "../timer.h"

#include <memory>
#include <unordered_set>
#include <algorithm>

//namespace std {
//  ostream &operator<<(ostream &out, const TarjanDFS::t_open_element &open) {
//    return out << "(" << open.preferred << ", " << open.h << ", " << open.id.hash() << ")";
//  }
//}

namespace tarjan_dfs {

  TarjanDFS::TarjanDFS(const Options &opts)
    : SearchEngine(opts),
      m_heuristic_refiner(opts.get_list<HeuristicRefiner *>("refiner")),
      c_unsath_new(!m_heuristic_refiner.empty() && opts.get<int>("u_new") > 0),
      c_unsath_new_full(!m_heuristic_refiner.empty() && opts.get<int>("u_new") == 2),
      c_unsath_open(!m_heuristic_refiner.empty() && opts.get<int>("u_open") > 0),
      c_unsath_open_full(!m_heuristic_refiner.empty() && opts.get<int>("u_open") == 2),
      c_unsath_bwprop(!m_heuristic_refiner.empty() && opts.get<int>("u_bprop") > 0),
      c_unsath_bwprop_full(!m_heuristic_refiner.empty() && opts.get<int>("u_bprop") == 2),
      c_unsath_search(!m_heuristic_refiner.empty() && opts.get<bool>("u_search") && c_unsath_new && c_unsath_new_full),
      c_refine_initial(!m_heuristic_refiner.empty() && opts.get<bool>("u_refine_initial_state")),
      c_unsath_use_plan(!m_heuristic_refiner.empty() && opts.get<bool>("u_use_plan")),
      c_keep_u_consistent(opts.get<bool>("u_consistent")),
      //c_delete_states(!m_heuristic_refiner.empty() && opts.get<bool>("u_delete_states")),
      c_delete_states(false), // currently not working
      c_unsath_compute_recognized_neighbors(false),
      m_current_id(0),
      m_pruning_method(opts.get<PruningMethod*>("pruning")),
      m_heuristics(opts.get_list<Heuristic *>("eval")),
      m_preferred(opts.contains("preferred") ? opts.get_list<Heuristic *>("preferred")
                  : std::vector<Heuristic *>()),
      m_cached_h(NULL),
      m_factory(opts.get<OpenListFactory*>("open_list")), // TODO
      m_unsath_refine(!m_heuristic_refiner.empty() && opts.get<bool>("u_refine")),
      m_next_states_print(1),
      m_next_state_print_multiplyer(2),
      m_smallest_h(std::numeric_limits<int>::max()),
      m_largest_depth(0),
      m_current_depth(0)
  {
    for (uint i = 0; i < m_heuristic_refiner.size(); i++) {
      m_underlying_heuristics.insert(m_heuristic_refiner[i]->get_heuristic());
    }

    if (m_unsath_refine) {
      for (uint i = 0; i < m_heuristic_refiner.size(); i++) {
        c_unsath_compute_recognized_neighbors |=
          m_heuristic_refiner[i]->dead_end_learning_requires_recognized_neighbors();
      }
    }
  }

  void TarjanDFS::print_progress_information() const
  {
      printf("[cd = %d, ld=%d, h = %d, registered=%zu, expanded=%d, open_states=%zu, dead_ends=%d, t=%.3f]\n",
             m_current_depth,
             m_largest_depth,
             m_smallest_h,
             g_state_registry->size(),
             m_search_progress.get_expanded(),
             m_open_states,
             m_search_progress.get_dead_ends(),
             g_timer());
  }
    
  void TarjanDFS::progress_information()
  {
    if (g_state_registry->size() >= m_next_states_print) {
      m_next_states_print = m_next_states_print * m_next_state_print_multiplyer;
      print_progress_information();
    }
  }
    
  bool TarjanDFS::trigger_refiner(const State &state, bool &success)
  {
    unsigned tmp;
    success = false;
    for (uint i = 0; i < m_heuristic_refiner.size(); i++) {
      if (m_heuristic_refiner[i]->get_heuristic()->is_dead_end()) {
        success = true;
        break;
      }
      if (!m_heuristic_refiner[i]->dead_end_learning_requires_full_component()
          && !m_heuristic_refiner[i]->dead_end_learning_requires_recognized_neighbors()) {
        tmp = m_heuristic_refiner[i]->learn_unrecognized_dead_end(state);
        if (tmp == HeuristicRefiner::SOLVED && c_unsath_use_plan) {
          m_heuristic_refiner[i]->get_partial_plan(m_plan);
          std::reverse(m_plan.begin(), m_plan.end());
          return true;
        } else if (tmp != HeuristicRefiner::FAILED) {
          success = true;
          break;
        }
      }
    }
    return false;
  }

  Heuristic *TarjanDFS::check_dead_end(const State &state, bool full)
  {
    int maxh = 0;
    Heuristic *res = NULL;
    for (std::set<Heuristic *>::iterator it = m_underlying_heuristics.begin();
         it != m_underlying_heuristics.end(); it++) {
      Heuristic *h = *it;
      assert(h != NULL);
      if (full) {
        h->evaluate(state);
      } else {
        h->reevaluate(state);
      }
      if (h->is_dead_end()) {
        maxh = -1;
        res = h;
        break;
      } else if (maxh >= 0 && h->get_value() > maxh)  {
        maxh = h->get_value();
        res = h;
      }
    }
    return res;
  }

  bool TarjanDFS::evaluate(const State &state, bool &u)
  {
    m_search_progress.inc_evaluated_states(1);
    u = false;
    m_cached_h = NULL;
    int maxh = 0;

    if (c_unsath_new) {
      Heuristic *x = check_dead_end(state, c_unsath_new_full);
      if (x && x->is_dead_end()) {
        maxh = -1;
        m_cached_h = x;
        u = true;
      } else if (c_unsath_search && x && x->get_value() > maxh) {
        maxh = x->get_value();
        m_cached_h = x;
      }
    }

    if (maxh != -1) {
      for (uint i = 0; i < m_heuristics.size(); i++) {
        m_search_progress.inc_evaluations(1);
        m_heuristics[i]->evaluate(state);
        if (m_heuristics[i]->is_dead_end()) {
          maxh = -1;
          m_cached_h = m_heuristics[i];
          break;
        }
        if (m_heuristics[i]->get_value() > maxh) {
          maxh = m_heuristics[i]->get_value();
          m_cached_h = m_heuristics[i];
        }
      }
    }

    bool res = false;

    if (maxh == -1 && !u && call_u_refinement()) {
      if (!c_unsath_new || !c_unsath_new_full) {
        check_dead_end(state, true);
      }
      res = trigger_refiner(state, u); 
    }

    return res;
  }

  void TarjanDFS::get_preferred_operators(const State &state,
                                          std::set<const Operator *> &result)
  {
    if (m_preferred.size() > 0) {
      m_search_progress.inc_evaluations(m_preferred.size());
      std::vector<const Operator *> ops;
      for (uint i = 0; i < m_preferred.size(); i++) {
        m_preferred[i]->evaluate(state);
        m_preferred[i]->get_preferred_operators(ops);
      }
      if (ops.size() > 0) {
        result.insert(ops.begin(), ops.end());
      }
      ops.clear();
    }
  }

  void TarjanDFS::initialize()
  {
    // TODO kind of hacky...
    // (have to initialize the heuristics)
    for (Heuristic *h : m_underlying_heuristics) {
      h->evaluate(g_initial_state());
    }

    
    //m_next_states_print = 1;
    //m_smallest_h = std::numeric_limits<int>::max();
    //m_largest_g = 0;

    //  State init = g_initial_state();
    //  if (evaluate(init)) {
    //    std::cout << "Solved in initial state!" << std::endl;
    //    //m_solved = true;
    //  } else if (!m_cached_h || !m_cached_h->is_dead_end()) {
    //    if (m_cached_h) {
    //      m_search_progress.add_heuristic(m_cached_h);
    //      m_search_progress.get_initial_h_values();
    //    }
    //    SearchNode node = search_space.get_node(init);
    //    node.open_initial(m_cached_h ? m_cached_h->get_value() : 0);
    //    //m_open_set->push(node, false);
    //    m_next_state_id = init.get_id();
    //    m_open_states++;
    //  } else {
    //    std::cout << "Initial state is a dead end!" << std::endl;
    //  }
  }

  bool TarjanDFS::call_u_refinement() const {
    return m_unsath_refine && (c_refine_initial || m_open_states > 0);
  }

  TarjanDFS::SearchStatus TarjanDFS::step()
  {
    for (unsigned i = 0; i < m_heuristic_refiner.size(); i++) {
      m_heuristic_refiner[i]->refine_offline();
    }
    bool irl = false;
    if (evaluate(g_initial_state(), irl)) {
      std::reverse(m_plan.begin(), m_plan.end());
      set_plan(m_plan);
      return SOLVED;
    } else if (m_cached_h && m_cached_h->is_dead_end()) {
      m_search_progress.inc_dead_ends();
      if (irl) {
        m_search_progress.inc_u_recognized_dead_ends();
      }
      std::cout << "Initial state is a dead end!" << std::endl;
      return FAILED;
    }

    SearchNode node = m_search_space[g_initial_state()];
    node.set_h(m_cached_h ? m_cached_h->get_heuristic() : 0);
    m_open_states = 1;
    m_current_depth = 1;
    bool solved = recursive_depth_first_search(g_initial_state(), 1) == R_SOLVED;
    print_progress_information();
    if (solved) {
      std::cout << "Solution found!" << std::endl;
      std::reverse(m_plan.begin(), m_plan.end());
      set_plan(m_plan);
      return SOLVED;
    }
    std::cout << "Completely explored state space!" << std::endl;
    return FAILED;
  }

  TarjanDFS::RecursionResult TarjanDFS::recursive_depth_first_search(const State &state, size_t min_delete)
  {
    if (test_goal(state)) {
      return R_SOLVED;
    }

    SearchNode node = m_search_space[state];

    if (node.get_h() == Heuristic::DEAD_END) {
      return R_FAILED;
    }

    m_open_states--;

    if (c_unsath_open) {
      Heuristic *h = check_dead_end(state, c_unsath_open_full);
      if (h && h->is_dead_end()) {
        m_search_progress.inc_dead_ends();
        m_search_progress.inc_u_recognized_dead_ends();
        node.set_h(Heuristic::DEAD_END);
        return R_FAILED;
      }
    }

    node.set_idx_and_lowlink(m_current_id++);

    if (call_u_refinement()) {
      node.set_onstack();
      m_stack.push_back(state.get_id());
      m_recognized_neighbors_offset.push_back(m_recognized_neighbors.size());
    }

    m_search_progress.inc_expanded();
    m_smallest_h = m_smallest_h < node.get_h() ? m_smallest_h : node.get_h();
    m_largest_depth = m_largest_depth > m_current_depth ? m_largest_depth : m_current_depth;
    progress_information();
    
    std::unique_ptr<OpenList> open = std::unique_ptr<OpenList>(m_factory->create());
    std::vector<const Operator *> ops;
    g_successor_generator->generate_applicable_ops(state, ops);
    m_pruning_method->prune_operators(state, ops);
    std::set<const Operator *> preferred_ops;
    get_preferred_operators(state, preferred_ops);

    //std::cout << "state#" << state.get_id().hash() << ":" << std::endl;
    m_search_progress.inc_generated(ops.size());

    bool u_consistent = true;
    bool de_u;
    for (uint i = 0; i < ops.size(); i++) {
      State succ = g_state_registry->get_successor_state(state, *ops[i]);
      SearchNode succ_node = m_search_space[succ];
      //std::cout << "op#" << i << " (" << ops[i]->get_name() << ") => " << succ.get_id() << std::endl;
      if (!succ_node.is_set_index()) {
        if (!succ_node.is_h_defined()) {
          m_open_states++;
          if (evaluate(succ, de_u)) {
            m_plan.push_back(ops[i]);
            return R_SOLVED;
          }
          succ_node.set_h(m_cached_h ? m_cached_h->get_value() : 0);
          if (succ_node.get_h() == Heuristic::DEAD_END) {
            m_search_progress.inc_dead_ends();
            m_open_states--;
            if (de_u) {
              m_search_progress.inc_u_recognized_dead_ends();
            } else if (c_keep_u_consistent) {
              succ_node.set_flag();
            }
          }
        }
        if (succ_node.get_h() != Heuristic::DEAD_END) {
          open->insert(succ_node, ops[i], preferred_ops.count(ops[i]));
        } 
      } else if (succ_node.is_onstack()) {
        node.update_lowlink(succ_node.get_idx());
        assert(succ_node.get_h() != Heuristic::DEAD_END);
      }

      if (call_u_refinement() && succ_node.get_h() == Heuristic::DEAD_END) {
        m_recognized_neighbors.push_back(succ.get_id());
        u_consistent = u_consistent && !succ_node.is_flagged();
      }
    }

    open->post_process();
    //std::cout << "state#" << state.get_id().hash() << " has " << open.size() << " open successors" << std::endl;

    bool u_is_recognized = false;
    RecursionResult r_res = R_FAILED;
    while (!open->empty()) {
      //std::cout << "(" << state.get_id().hash()
      //          << ", " << open.size() << ") -> " << open.back().id.hash() << std::endl;
      State succ = g_state_registry->lookup_state(open->front_state());
      SearchNode succ_node = m_search_space[succ];
      if (!succ_node.is_set_index()) {
        m_current_depth++;
        r_res = recursive_depth_first_search(succ, g_state_registry->size());
        m_current_depth--;
        node.update_lowlink(succ_node.get_lowlink());
        if (r_res == R_SOLVED) {
          m_plan.push_back(open->front_operator());
          return R_SOLVED;
        } else if (r_res == R_UPDATED) {
          assert(!succ_node.is_flagged());
          if (c_unsath_bwprop) {
            bool dead = node.get_idx() >= succ_node.get_lowlink();
            if (!dead) {
              Heuristic *h = check_dead_end(state, c_unsath_bwprop_full);
              dead = h && h->is_dead_end();
            }
            if (dead) {
              u_is_recognized = true;
            
              open->pop_front();
              while (!open->empty()) {
                State succ = g_state_registry->lookup_state(open->front_state());
                SearchNode succ_node = m_search_space[succ];
                if (!succ_node.is_set_index()) {
                  if (succ_node.get_h() != Heuristic::DEAD_END) {
                    succ_node.set_h(Heuristic::DEAD_END);
                    m_search_progress.inc_u_recognized_dead_ends();
                    m_open_states--;
                  }
                } else if (succ_node.is_onstack()) {
                  node.update_lowlink(succ_node.get_idx());
                }
                open->pop_front();
              }

              //if (call_u_refinement() && node.get_idx() == node.get_lowlink()) {
              //  int i = m_stack.size() - 1;
              //  for (; i >= 0; i--) {
              //    SearchNode cnode = m_search_space[m_stack[i]];
              //    cnode.set_onstack(false);
              //    cnode.set_h(Heuristic::DEAD_END);
              //    if (m_stack[i] == state.get_id()) {
              //      break;
              //    }
              //  }
              //  m_stack.erase(m_stack.begin() + i, m_stack.end());
              //  m_recognized_neighbors.erase(m_recognized_neighbors.begin() + m_recognized_neighbors_offset[i], m_recognized_neighbors.end());
              //  m_recognized_neighbors_offset.resize(i);

              //  if (c_delete_states) {
              //    m_search_space.shrink(min_delete);
              //    g_state_registry->shrink(min_delete);
              //  }
              //}
              break;
            } 
          }
        } else {
          assert(r_res == R_FAILED);
          if (c_keep_u_consistent && succ_node.is_flagged()) {
            assert(succ_node.get_h() == Heuristic::DEAD_END);
            u_consistent = false;
          }
        }
      } else if (succ_node.is_onstack()) {
        node.update_lowlink(succ_node.get_idx());
        assert(succ_node.get_h() != Heuristic::DEAD_END);
      }

      if (call_u_refinement() && succ_node.get_h() == Heuristic::DEAD_END) {
        m_recognized_neighbors.push_back(succ.get_id());
        u_consistent = u_consistent && !succ_node.is_flagged();
      }
      open->pop_front();
    }

    r_res = R_FAILED;

    //  if (!call_u_refinement() && node.get_idx() == node.get_lowlink()) {
    //    std::cout << node.get_idx() << " (state#"  << node.get_state_id().hash() << ") => "
    //              << m_open_states << std::endl;
    //  }
    if (call_u_refinement() && node.get_idx() == node.get_lowlink()) {
      bool do_refinement = !u_is_recognized && (!c_keep_u_consistent || u_consistent);

      std::vector<State> component;
      std::unordered_set<StateID> recn;
      int i = m_stack.size() - 1;
      for (; i >= 0; i--) {
        component.push_back(g_state_registry->lookup_state(m_stack[i]));
        //component.insert(component.begin(), m_stack[i]);
        m_search_space[m_stack[i]].set_onstack(false);
        if (m_stack[i] == state.get_id()) {
          break;
        }
      }
      m_stack.erase(m_stack.begin() + i, m_stack.end());
      i = m_recognized_neighbors_offset[i];
      m_recognized_neighbors_offset.resize(m_stack.size());

      if (do_refinement && c_unsath_compute_recognized_neighbors) {
        for (unsigned j = i; j < m_recognized_neighbors.size(); j++) {
          recn.insert(m_recognized_neighbors[j]);
        }
      }
      m_recognized_neighbors.erase(m_recognized_neighbors.begin() + i, m_recognized_neighbors.end());

      if (do_refinement) {
        for (std::set<Heuristic *>::iterator it = m_underlying_heuristics.begin();
             it != m_underlying_heuristics.end(); it++) {
          (*it)->clear_cache();
        }
        for (uint i = 0; !u_is_recognized && i < m_heuristic_refiner.size(); i++) {
          assert (!m_heuristic_refiner[i]->get_heuristic()->is_dead_end());
          HeuristicRefiner::RefinementResult refine = \
            m_heuristic_refiner[i]->learn_unrecognized_dead_ends(component,
                                                                 recn);
          if (refine != HeuristicRefiner::FAILED) {
            u_is_recognized = true;
          }
        }

        recn.clear();

        if (!u_is_recognized) {
          m_unsath_refine = false;
        }
      }

      for (unsigned j = 0; j < component.size(); j++) {
        SearchNode node = m_search_space[component[j]];
        node.set_h(Heuristic::DEAD_END);
        if (!u_is_recognized) {
          node.set_flag();
        } else {
          m_search_progress.inc_u_recognized_dead_ends();
        }
      }
      component.clear();

      if (c_delete_states) {
        m_search_space.shrink(min_delete);
        g_state_registry->shrink(min_delete);
      }

      if (u_is_recognized) {
        r_res = R_UPDATED;
      }
    }

    return r_res;
  }

  void TarjanDFS::statistics() const
  {
    for (uint i = 0; i < m_heuristic_refiner.size(); i++) {
      m_heuristic_refiner[i]->statistics();
    }
    m_pruning_method->print_statistics();
    printf("Open states in open list: %zu state(s).\n", m_open_states);
    m_search_progress.print_statistics();
    printf("Registered: %zu state(s).\n", g_state_registry->size());
  }


  void TarjanDFS::add_options_to_parser(OptionParser &parser)
  {
    parser.add_list_option<Heuristic *>("eval", "", "[]");
    parser.add_list_option<HeuristicRefiner *>("refiner", "", "[]");
    parser.add_list_option<Heuristic *>("preferred", "", "", OptionFlags(false));
    parser.add_option<OpenListFactory*>("open_list", "", "h");
    parser.add_option<PruningMethod*>("pruning", "", "null");
    parser.add_option<bool>("u_refine", "", "true");
    parser.add_option<bool>("u_refine_initial_state", "", "false");
    parser.add_option<int>("u_new", "", "0");
    parser.add_option<int>("u_open", "", "2");
    parser.add_option<int>("u_bprop", "", "2");
    parser.add_option<bool>("u_search", "", "false");
    parser.add_option<bool>("u_use_plan", "", "false");
    parser.add_option<bool>("u_delete_states", "", "false");
    parser.add_option<bool>("u_consistent", "", "false");
    //parser.add_option<bool>("reopen", "", "false");
    //parser.add_option<bool>("useoss", "", "false");
    //parser.add_option<bool>("u_consistency", "", "false");
    //parser.add_option<SearchRestartStrategy*>("restart", "", "none");
    SearchEngine::add_options_to_parser(parser);
  }
}

static SearchEngine *_parse(OptionParser &parser) {
  tarjan_dfs::TarjanDFS::add_options_to_parser(parser);
  Options opts = parser.parse();
  if (!parser.dry_run()) {
    return new tarjan_dfs::TarjanDFS(opts);
  }
  return NULL;
}

static Plugin<SearchEngine> _plugin("tarjan", _parse);
