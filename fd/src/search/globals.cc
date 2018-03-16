#include "globals.h"

#include "axioms.h"
#include "causal_graph.h"
#include "domain_transition_graph.h"
#include "heuristic.h"
#include "int_packer.h"
#include "legacy_causal_graph.h"
#include "operator.h"
#include "rng.h"
#include "state.h"
#include "state_registry.h"
#include "successor_generator.h"
#include "timer.h"
#include "utilities.h"

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <vector>
#include <sstream>
using namespace std;

#include <ext/hash_map>
using namespace __gnu_cxx;

static const int PRE_FILE_VERSION = 3;


// TODO: This needs a proper type and should be moved to a separate
//       mutexes.cc file or similar, accessed via something called
//       g_mutexes. (Right now, the interface is via global function
//       are_mutex, which is at least better than exposing the data
//       structure globally.)

static vector<vector<set<pair<int, int> > > > g_inconsistent_facts;

bool test_goal(const State &state) {
    for (int i = 0; i < g_goal.size(); i++) {
        if (state[g_goal[i].first] != g_goal[i].second) {
            return false;
        }
    }
    return true;
}

int calculate_plan_cost(const vector<const Operator *> &plan) {
    // TODO: Refactor: this is only used by save_plan (see below)
    //       and the SearchEngine classes and hence should maybe
    //       be moved into the SearchEngine (along with save_plan).
    int plan_cost = 0;
    for (int i = 0; i < plan.size(); i++) {
        plan_cost += plan[i]->get_cost();
    }
    return plan_cost;
}

void save_plan(const vector<const Operator *> &plan, int iter) {
    // TODO: Refactor: this is only used by the SearchEngine classes
    //       and hence should maybe be moved into the SearchEngine.
    ofstream outfile;
    if (iter == 0) {
        outfile.open(g_plan_filename.c_str(), ios::out);
    } else {
        ostringstream out;
        out << g_plan_filename << "." << iter;
        outfile.open(out.str().c_str(), ios::out);
    }
    for (int i = 0; i < plan.size(); i++) {
        cout << plan[i]->get_name() << " (" << plan[i]->get_cost() << ")" << endl;
        outfile << "(" << plan[i]->get_name() << ")" << endl;
    }
    outfile.close();
    int plan_cost = calculate_plan_cost(plan);
    ofstream statusfile;
    statusfile.open("plan_numbers_and_cost", ios::out | ios::app);
    statusfile << iter << " " << plan_cost << endl;
    statusfile.close();
    cout << "Plan length: " << plan.size() << " step(s)." << endl;
    cout << "Plan cost: " << plan_cost << endl;
}

bool peek_magic(istream &in, string magic) {
    string word;
    in >> word;
    bool result = (word == magic);
    for (int i = word.size() - 1; i >= 0; i--)
        in.putback(word[i]);
    return result;
}

void check_magic(istream &in, string magic) {
    string word;
    in >> word;
    if (word != magic) {
        cout << "Failed to match magic word '" << magic << "'." << endl;
        cout << "Got '" << word << "'." << endl;
        if (magic == "begin_version") {
            cerr << "Possible cause: you are running the planner "
                 << "on a preprocessor file from " << endl
                 << "an older version." << endl;
        }
        exit_with(EXIT_INPUT_ERROR);
    }
}

void read_and_verify_version(istream &in) {
    int version;
    check_magic(in, "begin_version");
    in >> version;
    check_magic(in, "end_version");
    if (version != PRE_FILE_VERSION) {
        cerr << "Expected preprocessor file version " << PRE_FILE_VERSION
             << ", got " << version << "." << endl;
        cerr << "Exiting." << endl;
        exit_with(EXIT_INPUT_ERROR);
    }
}

void read_metric(istream &in) {
    check_magic(in, "begin_metric");
    in >> g_use_metric;
    check_magic(in, "end_metric");
}

void read_variables(istream &in) {
    int count;
    in >> count;
    for (int i = 0; i < count; i++) {
        check_magic(in, "begin_variable");
        string name;
        in >> name;
        g_variable_name.push_back(name);
        int layer;
        in >> layer;
        g_axiom_layers.push_back(layer);
        int range;
        in >> range;
        g_variable_domain.push_back(range);
        in >> ws;
        vector<string> fact_names(range);
        for (size_t i = 0; i < fact_names.size(); i++)
            getline(in, fact_names[i]);
        g_fact_names.push_back(fact_names);
        check_magic(in, "end_variable");
    }
}

void read_mutexes(istream &in) {
    g_inconsistent_facts.resize(g_variable_domain.size());
    for (size_t i = 0; i < g_variable_domain.size(); ++i)
        g_inconsistent_facts[i].resize(g_variable_domain[i]);

    int num_mutex_groups;
    in >> num_mutex_groups;

    /* NOTE: Mutex groups can overlap, in which case the same mutex
       should not be represented multiple times. The current
       representation takes care of that automatically by using sets.
       If we ever change this representation, this is something to be
       aware of. */

    for (size_t i = 0; i < num_mutex_groups; ++i) {
        check_magic(in, "begin_mutex_group");
        int num_facts;
        in >> num_facts;
        vector<pair<int, int> > invariant_group;
        invariant_group.reserve(num_facts);
        for (size_t j = 0; j < num_facts; ++j) {
            int var, val;
            in >> var >> val;
            invariant_group.push_back(make_pair(var, val));
        }
        check_magic(in, "end_mutex_group");
        for (size_t j = 0; j < invariant_group.size(); ++j) {
            const pair<int, int> &fact1 = invariant_group[j];
            int var1 = fact1.first, val1 = fact1.second;
            for (size_t k = 0; k < invariant_group.size(); ++k) {
                const pair<int, int> &fact2 = invariant_group[k];
                int var2 = fact2.first;
                if (var1 != var2) {
                    /* The "different variable" test makes sure we
                       don't mark a fact as mutex with itself
                       (important for correctness) and don't include
                       redundant mutexes (important to conserve
                       memory). Note that the preprocessor removes
                       mutex groups that contain *only* redundant
                       mutexes, but it can of course generate mutex
                       groups which lead to *some* redundant mutexes,
                       where some but not all facts talk about the
                       same variable. */
                    g_inconsistent_facts[var1][val1].insert(fact2);
                }
            }
        }
    }
}

void read_goal(istream &in) {
    check_magic(in, "begin_goal");
    int count;
    in >> count;
    if (count < 1) {
        cerr << "Task has no goal condition!" << endl;
        exit_with(EXIT_INPUT_ERROR);
    }
    for (int i = 0; i < count; i++) {
        int var, val;
        in >> var >> val;
        g_goal.push_back(make_pair(var, val));
    }
    check_magic(in, "end_goal");
}

void dump_goal() {
    cout << "Goal Conditions:" << endl;
    for (int i = 0; i < g_goal.size(); i++)
        cout << "  " << g_variable_name[g_goal[i].first] << ": "
             << g_goal[i].second << " (PDDL: " << g_fact_names[g_goal[i].first][g_goal[i].second]
             << endl;
}

void read_operators(istream &in) {
    int count;
    in >> count;
    for (int i = 0; i < count; i++)
      g_operators.push_back(Operator(in, false, i));
}

void read_axioms(istream &in) {
    int count;
    in >> count;
    for (int i = 0; i < count; i++)
      g_axioms.push_back(Operator(in, true, g_operators.size() + i));

    g_axiom_evaluator = new AxiomEvaluator;
}

void read_everything(istream &in) {
    cout << "reading input... [t=" << g_timer << "]" << endl;
    read_and_verify_version(in);
    read_metric(in);
    read_variables(in);
    read_mutexes(in);
    g_initial_state_data.resize(g_variable_domain.size());
    check_magic(in, "begin_state");
    for (int i = 0; i < g_variable_domain.size(); i++) {
        in >> g_initial_state_data[i];
    }
    check_magic(in, "end_state");
    g_default_axiom_values = g_initial_state_data;

    read_goal(in);
    read_operators(in);
    read_axioms(in);
    check_magic(in, "begin_SG");
    g_successor_generator = read_successor_generator(in);
    check_magic(in, "end_SG");
    DomainTransitionGraph::read_all(in);
    g_legacy_causal_graph = new LegacyCausalGraph(in);

    cout << "done reading input! [t=" << g_timer << "]" << endl;

    // NOTE: causal graph is computed from the problem specification,
    // so must be built after the problem has been read in.

    cout << "building causal graph..." << flush;
    g_causal_graph = new CausalGraph;
    cout << "done! [t=" << g_timer << "]" << endl;

    cout << "packing state variables..." << flush;
    assert(!g_variable_domain.empty());
    g_state_packer = new IntPacker(g_variable_domain);
    cout << "Variables: " << g_variable_domain.size() << endl;
    cout << "Bytes per state: "
         << g_state_packer->get_num_bins() *
            g_state_packer->get_bin_size_in_bytes() << endl;
    cout << "done! [t=" << g_timer << "]" << endl;

    // NOTE: state registry stores the sizes of the state, so must be
    // built after the problem has been read in.
    g_state_registry = new StateRegistry;

    cout << "done initalizing global data [t=" << g_timer << "]" << endl;

    cout << "... has axioms: " << has_axioms() << endl;
    cout << "... has conditional effects: " << has_conditional_effects() << endl;


    //dump_everything();
}

void dump_everything() {
    cout << "Use metric? " << g_use_metric << endl;
    cout << "Min Action Cost: " << g_min_action_cost << endl;
    cout << "Max Action Cost: " << g_max_action_cost << endl;
    // TODO: Dump the actual fact names.
    cout << "Variables (" << g_variable_name.size() << "):" << endl;
    for (int i = 0; i < g_variable_name.size(); i++)
        cout << "  " << g_variable_name[i]
             << " (range " << g_variable_domain[i] << ")" << endl;
    State initial_state = g_initial_state();
    cout << "Initial State (PDDL):" << endl;
    initial_state.dump_pddl();
    cout << "Initial State (FDR):" << endl;
    initial_state.dump_fdr();
    dump_goal();
    /*
    cout << "Successor Generator:" << endl;
    g_successor_generator->dump();
    for(int i = 0; i < g_variable_domain.size(); i++)
      g_transition_graphs[i]->dump();
    */
}

bool is_unit_cost() {
    return g_min_action_cost == 1 && g_max_action_cost == 1;
}

bool has_axioms() {
    return !g_axioms.empty();
}

void verify_no_axioms() {
    if (has_axioms()) {
        cerr << "Heuristic does not support axioms!" << endl << "Terminating."
             << endl;
        exit_with(EXIT_UNSUPPORTED);
    }
}

static int get_first_conditional_effects_op_id() {
    for (int i = 0; i < g_operators.size(); i++) {
        const vector<PrePost> &pre_post = g_operators[i].get_pre_post();
        for (int j = 0; j < pre_post.size(); j++) {
            const vector<Prevail> &cond = pre_post[j].cond;
            if (!cond.empty())
                return i;
        }
    }
    return -1;
}

bool has_conditional_effects() {
    return get_first_conditional_effects_op_id() != -1;
}

void verify_no_conditional_effects() {
    int op_id = get_first_conditional_effects_op_id();
    if (op_id != -1) {
            cerr << "Heuristic does not support conditional effects "
                 << "(operator " << g_operators[op_id].get_name() << ")" << endl
                 << "Terminating." << endl;
            exit_with(EXIT_UNSUPPORTED);
    }
}

void verify_no_axioms_no_conditional_effects() {
    verify_no_axioms();
    verify_no_conditional_effects();
}

bool are_mutex(const pair<int, int> &a, const pair<int, int> &b) {
    if (a.first == b.first) // same variable: mutex iff different value
        return a.second != b.second;
    return bool(g_inconsistent_facts[a.first][a.second].count(b));
}

const State &g_initial_state() {
    return g_state_registry->get_initial_state();
}


// Michael: added for bisimulation
//vector<bool> g_goal_leading_labels;

void read_goal_leading_actions(vector<bool>& labels) {
    /* Read actions from separate file. */
	assert(labels.size()==0);
	labels.resize(g_operators.size(),false);

    cout << "Reading actions from file..." << endl;
    ifstream myfile("goal.leading");
    int no_operators = -1;
    if (myfile.is_open()) {
        ifstream &in = myfile;
        check_magic(in, "begin_operators");
        in >> no_operators;

        for (int i = 0; i < no_operators; i++) {
            int op_index;
            in >> op_index;
            labels[op_index] = true;
        }
        check_magic(in, "end_operators");
        myfile.close();
        cout << "done" << endl;
    } else {
        cout << "Unable to open actions file!" << endl;
        exit(1);
    }
    cout << "Read goal leading actions "<< no_operators << " out of total number of actions " << g_operators.size() << endl;
}

void write_goal_leading_actions(const vector<bool>& labels, int sz) {
    /* Write actions to a file. */
	assert(labels.size() == g_operators.size());

    cout << "Writing actions to file..." << endl;
    ofstream myfile("goal.leading");
    if (myfile.is_open()) {
        ofstream &ou = myfile;
        ou << "begin_operators" << endl;
        ou << sz << endl;
        int num_labels = 0;
        for (int i = 0; i < labels.size(); i++) {
        	if (labels[i]) {
        		ou << i << endl;
        		num_labels++;
        	}
        }
        assert(sz == num_labels);
        ou << "end_operators" << endl;
        myfile.close();
        cout << "done" << endl;
    } else {
        cout << "Unable to open actions file!" << endl;
        exit(1);
    }
}

void mark_relevant_operators(
		int& num_states,
		vector<set<int> >& touching_operators,
		vector<set<int> >& touched_states,
		vector<bool>& labels,
		int& num_actions) {

	/*
	 * Store, how many states are already done, i.e., for which relevant states at least one relevant operator
	 * has been marked. All states without relevant operators are trivially done.
	 */
	int num_states_done = 0;
	for (int i = 0; i < num_states; i++) {
		if (touching_operators[i].empty())
			num_states_done++;
	}

	/*
	 * Check all states. If one has only one relevant operator, mark that and update the status of
	 * all relevant states of that action.
	 */
	for (int i = 0; i < num_states; i++) {
		if (touching_operators[i].size() == 1) {
			int op_no = *(touching_operators[i].begin());
			handle_marked_operators(touched_states, op_no, touching_operators, num_states_done, labels, num_actions);
		}
	}

	/*
	 * Take the first action with highest number of states with only unmarked relevant operators.
	 * Mark that and update the status of all relevant states of that action.
	 */
	while (num_states_done < num_states) {
		size_t max_num = 0;
		int max_index = -1;
		for (int i = 0; i < touched_states.size(); i++) {
			if (!touched_states[i].empty()) {
				size_t num_starting_states = touched_states[i].size();
				if (num_starting_states > max_num) {
					max_num = num_starting_states;
					max_index = i;
				}
			}
		}
		handle_marked_operators(touched_states, max_index, touching_operators, num_states_done, labels, num_actions);
	}
}

void handle_marked_operators(
		vector<set<int> >& touched_states,
		int& op_no,
		vector<set<int> >& touching_operators,
		int& num_states_done,
		vector<bool>& labels,
		int& num_actions) {
	// find all states having this as relevant operator
	set<int> &relevantStates = touched_states[op_no];
	set<int>::iterator relevantStatesIt;
	for (relevantStatesIt = relevantStates.begin(); relevantStatesIt != relevantStates.end(); relevantStatesIt++) {
		// all these states are already done, so that they can be removed from any relevant operator
		int relevantState = *relevantStatesIt;
		set<int> &relevantOperators = touching_operators[relevantState];
		set<int>::iterator relevantOperatorsIt;
		for (relevantOperatorsIt = relevantOperators.begin(); relevantOperatorsIt != relevantOperators.end(); relevantOperatorsIt++) {
			int relevantOperator = *relevantOperatorsIt;
			touched_states[relevantOperator].erase(relevantState);
		}
		// as the state is done, we can remove the set of relevant operators from it
		touching_operators[relevantState].clear();
		num_states_done++;
	}
	labels[op_no] = true;
	num_actions++;
}

bool g_use_metric;
int g_min_action_cost = numeric_limits<int>::max();
int g_max_action_cost = 0;
vector<string> g_variable_name;
vector<int> g_variable_domain;
vector<vector<string> > g_fact_names;
vector<int> g_axiom_layers;
vector<int> g_default_axiom_values;
IntPacker *g_state_packer;
vector<int> g_initial_state_data;
vector<pair<int, int> > g_goal;
vector<Operator> g_operators;
vector<Operator> g_axioms;
AxiomEvaluator *g_axiom_evaluator;
SuccessorGenerator *g_successor_generator;
vector<DomainTransitionGraph *> g_transition_graphs;
CausalGraph *g_causal_graph;
LegacyCausalGraph *g_legacy_causal_graph;

Timer g_timer;
string g_plan_filename = "sas_plan";
RandomNumberGenerator g_rng(2011); // Use an arbitrary default seed.
StateRegistry *g_state_registry = 0;
