#ifndef GLOBALS_H
#define GLOBALS_H

#include <iosfwd>
#include <string>
#include <vector>
#include <set>

class Axiom;
class AxiomEvaluator;
class CausalGraph;
class DomainTransitionGraph;
class IntPacker;
class LegacyCausalGraph;
class Operator;
class RandomNumberGenerator;
class State;
class SuccessorGenerator;
class Timer;
class StateRegistry;

bool test_goal(const State &state);
void save_plan(const std::vector<const Operator *> &plan, int iter);
int calculate_plan_cost(const std::vector<const Operator *> &plan);

void read_everything(std::istream &in);
void dump_everything();

bool is_unit_cost();
bool has_axioms();
void verify_no_axioms();
bool has_conditional_effects();
void verify_no_conditional_effects();
void verify_no_axioms_no_conditional_effects();

void check_magic(std::istream &in, std::string magic);

bool are_mutex(const std::pair<int, int> &a, const std::pair<int, int> &b);

// Michael: added for bisimulation
//extern std::vector<bool> g_goal_leading_labels;
void read_goal_leading_actions(std::vector<bool>& labels);
void write_goal_leading_actions(const std::vector<bool>& labels, int sz);

// Peter: added for finding approximation of caught label sets
void mark_relevant_operators(
		int& num_states,
		std::vector<std::set<int> >& touching_operators,
		std::vector<std::set<int> >& touched_states,
		std::vector<bool>& labels,
		int& num_actions);
void handle_marked_operators(
		std::vector<std::set<int> >& touched_states,
		int& op_no,
		std::vector<std::set<int> >& touching_operators,
		int& num_states_done,
		std::vector<bool>& labels,
		int& num_actions);


extern bool g_use_metric;
extern int g_min_action_cost;
extern int g_max_action_cost;

// TODO: The following five belong into a new Variable class.
extern std::vector<std::string> g_variable_name;
extern std::vector<int> g_variable_domain;
extern std::vector<std::vector<std::string> > g_fact_names;
extern std::vector<int> g_axiom_layers;
extern std::vector<int> g_default_axiom_values;

extern IntPacker *g_state_packer;
// This vector holds the initial values *before* the axioms have been evaluated.
// Use the state registry to obtain the real initial state.
extern std::vector<int> g_initial_state_data;
// TODO The following function returns the initial state that is registered
//      in g_state_registry. This is only a short-term solution. In the
//      medium term, we should get rid of the global registry.
extern const State &g_initial_state();
extern std::vector<std::pair<int, int> > g_goal;

extern std::vector<Operator> g_operators;
extern std::vector<Operator> g_axioms;
extern AxiomEvaluator *g_axiom_evaluator;
extern SuccessorGenerator *g_successor_generator;
extern std::vector<DomainTransitionGraph *> g_transition_graphs;
extern CausalGraph *g_causal_graph;
extern LegacyCausalGraph *g_legacy_causal_graph;
extern Timer g_timer;
extern std::string g_plan_filename;
extern RandomNumberGenerator g_rng;
// Only one global object for now. Could later be changed to use one instance
// for each problem in this case the method State::get_id would also have to be
// changed.
extern StateRegistry *g_state_registry;



#endif