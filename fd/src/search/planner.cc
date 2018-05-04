#include "globals.h"
#include "operator.h"
#include "option_parser.h"
#include "ext/tree_util.hh"
#include "timer.h"
#include "utilities.h"
#include "search_engine.h"

#include "state.h"
#include "state_registry.h"

#include <iostream>
#include <fstream>
#include <new>

#include <vector>
#include <fstream>

using namespace std;

int main(int argc, const char **argv) {
    register_event_handlers();

    if (argc < 2) {
        cout << OptionParser::usage(argv[0]) << endl;
        exit_with(EXIT_INPUT_ERROR);
    }

    if (string(argv[1]).compare("--help") != 0)
        read_everything(cin);

    SearchEngine *engine = 0;

    // The command line is parsed twice: once in dry-run mode, to
    // check for simple input errors, and then in normal mode.
    bool unit_cost = is_unit_cost();
    try {
        OptionParser::parse_cmd_line(argc, argv, true, unit_cost);
        engine = OptionParser::parse_cmd_line(argc, argv, false, unit_cost);
    } catch (ArgError &error) {
        cerr << error << endl;
        OptionParser::usage(argv[0]);
        exit_with(EXIT_INPUT_ERROR);
    } catch (ParseError &error) {
        cerr << error << endl;
        exit_with(EXIT_INPUT_ERROR);
    }

    Timer search_timer;
    engine->search();
    search_timer.stop();
    g_timer.stop();

    engine->save_plan_if_necessary();
    engine->statistics();
    cout << "Search time: " << search_timer << endl;
    cout << "Total time: " << g_timer << endl;
    // ofstream my_time_file;
    // my_time_file.open("result-time.txt",ios::app);
    // my_time_file<<g_timer<<" ";
    
#ifdef CHECK_EXPANDED_DEAD_ENDS
    size_t expanded_dead_ends = 0;
    if (engine->found_solution()) {
        std::streambuf* cout_sbuf = std::cout.rdbuf(); // save original sbuf
        std::ofstream  fout("/dev/null");
        std::cout.rdbuf(fout.rdbuf());
        SearchEngine *engine_check = 0;
        for (size_t i = 1; i < engine->explored_states.size(); i++) {
            bool unit_cost = is_unit_cost();
            for (size_t var = 0; var < g_variable_domain.size(); var++) {
                g_initial_state_data[var] = engine->explored_states[i][var];
            }
            g_state_registry = new StateRegistry;
            try {
                OptionParser::parse_cmd_line(argc, argv, true, unit_cost);
                engine_check = OptionParser::parse_cmd_line(argc, argv, false, unit_cost);
            } catch (ArgError &error) {
                cerr << error << endl;
                OptionParser::usage(argv[0]);
                exit_with(EXIT_INPUT_ERROR);
            } catch (ParseError &error) {
                cerr << error << endl;
                exit_with(EXIT_INPUT_ERROR);
            }
            engine_check->search();
            if (!engine_check->found_solution()) {
                expanded_dead_ends += 1;
            }
            delete g_state_registry;
        }
        std::cout.rdbuf(cout_sbuf);
    } else {
        expanded_dead_ends = engine->explored_states.size();
    }
    double percent = (double) expanded_dead_ends
        / (double) engine->explored_states.size();
    cout << "Expanded dead ends: " << expanded_dead_ends << "/"
        << engine->explored_states.size() << " (" << 100 * percent << ")"
        << endl;
#endif

    if (engine->found_solution()) {
        // my_time_file<<"1"<<endl;
        // my_time_file.close();
        exit_with(EXIT_PLAN_FOUND);
    } else {
        // my_time_file<<"0"<<endl;
        // my_time_file.close();
        exit_with(EXIT_UNSOLVED_INCOMPLETE);
    }
}
