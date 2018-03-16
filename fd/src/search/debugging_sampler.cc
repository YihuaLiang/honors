
#include "debugging_sampler.h"

#include "option_parser.h"
#include "plugin.h"

#include "state.h"
#include "state_id.h"
#include "operator.h"
#include "successor_generator.h"
#include "search_space.h"
#include "globals.h"

#include <ctime>

using namespace std;

#define EPSILON 0.0001

HSample::HSample(const Options &opts)
    : SearchEngine(opts),
      gen(opts.get<int>("seed")),
      current_samples(0),
      num_samples(opts.get<int>("n")),
      solved(false),
      max_depth(opts.get<int>("depth")),
      heuristics(opts.get_list<Heuristic*>("evals"))
{
    num_clocks.resize(heuristics.size(), 0);
    initialization.resize(heuristics.size(), 0);
    average.resize(heuristics.size(), 0);
    average_old.resize(heuristics.size(), 0);
    dead_ends.resize(heuristics.size(), 0);
    values.resize(heuristics.size(), 0);
}

void HSample::initialize()
{
    State init = g_initial_state();
    SearchNode n = search_space.get_node(init);
    n.open_initial(0);
    for (size_t i = 0; i < heuristics.size(); i++) {
        clock_t start = clock();
        heuristics[i]->evaluate(init);
        initialization[i] = (double) (clock() - start) / CLOCKS_PER_SEC;
    }
}

void HSample::print_line()
{
    cout << "["
        << "n = " << current_samples;
    for (size_t i = 0; i < heuristics.size(); i++) {
        cout << ", h" << i << " = " << (1.0  / average[i]);
    }
    cout << "]" << endl;
}

void HSample::check_and_print()
{
    double max = 0;
    for (size_t i = 0; i < heuristics.size(); i++) {
        average[i] = (((double) num_clocks[i] / CLOCKS_PER_SEC) / current_samples);
        if (fabs(average[i] - average_old[i]) > max) {
            max = fabs(average[i] - average_old[i]);
        }
    }
    if (max > EPSILON) {
        for (size_t i = 0; i < heuristics.size(); i++) {
            average_old[i] = average[i];
        }
        print_line();
    }
}

int HSample::step()
{
    if (current_samples++ == num_samples) {
        print_line();
        cout << "Sampled " << num_samples << " states" << endl;
        cout << "## Time spent on heuristic initialization: " << endl;
        for (size_t i = 0; i < heuristics.size(); i++) {
            cout << "h" << i << ": " << initialization[i] << endl;
        }
        cout << "## Time spent on heuristic computation: " << endl;
        for (size_t i = 0; i < heuristics.size(); i++) {
            cout << "h" << i << ": " << ((double) num_clocks[i] / CLOCKS_PER_SEC) << endl;
        }
        cout << "## Average heuristic value: " << endl;
        for (size_t i = 0; i < heuristics.size(); i++) {
            cout << "h" << i << ": " << dead_ends[i]
                << " (" << ((double) values[i] / (num_samples - dead_ends[i])) << ")" << endl;
        }
        cout << "## Number of dead ends (relative): " << endl;
        for (size_t i = 0; i < heuristics.size(); i++) {
            cout << "h" << i << ": " << dead_ends[i]
                << " (" << ((double) dead_ends[i] / num_samples) << ")" << endl;
        }
        return FAILED;
    }

    delete(g_state_registry);
    g_state_registry = new StateRegistry();

    int d = 0;
    int max = max_depth < 0 ? gen.next31() : gen.next(max_depth);
    StateID sid = g_initial_state().get_id();

    while (d++ < max) {
        State s = g_state_registry->lookup_state(sid);
        vector<const Operator*> ops;
        g_successor_generator->generate_applicable_ops(s, ops);
        if (ops.size() == 0) {
            break;
        }
        int i = gen.next(ops.size());
        State succ = g_state_registry->get_successor_state(s, *ops[i]);
        sid = succ.get_id();
    }

    State state = g_state_registry->lookup_state(sid);
    for (size_t i = 0; i < heuristics.size(); i++) {
        clock_t start = clock();
        heuristics[i]->evaluate(state);
        num_clocks[i] += (clock() - start);
        if (heuristics[i]->is_dead_end() || heuristics[i]->get_heuristic() == numeric_limits<int>::max()) {
            dead_ends[i]++;
        }
        else {
            values[i] += heuristics[i]->get_heuristic();
        }
    }

    check_and_print();

    return IN_PROGRESS;
}

static SearchEngine *_parse(OptionParser &parser) {
    SearchEngine::add_options_to_parser(parser);
    parser.add_option<int>("seed", "", "0");
    parser.add_option<int>("n", "", "100000");
    parser.add_option<int>("depth", "", "1000");
    parser.add_list_option<Heuristic*>("evals");
    Options opts = parser.parse();
    if (!parser.dry_run()) {
        return new HSample(opts);
    }
    return NULL;
}

static Plugin<SearchEngine> _eng("hsample", _parse);

