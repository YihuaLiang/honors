
#include "uc_heuristic.h"

#include "../globals.h"
#include "../state.h"
#include "../operator_cost.h"
#include "../option_parser.h"
#include "../plugin.h"
#include "../utilities.h"

#include "../state_registry.h"

#include <vector>
#include <cstdio>

void ClauseLearningStatistics::dump() const {
    std::cout << "Number of uC Evaluations: " << num_total_evaluations << std::endl;
    std::cout << "Number of hC Computations: " << num_hc_evaluations << std::endl;
    std::cout << "Number of hC recognized dead ends: " << num_hc_dead_ends << std::endl;
    std::cout << "Number of clause extractions: " << num_clause_extractions << std::endl;
    printf("Time spent on clause matching: %.5fs\n", t_clause_matching);
    printf("Time spent on clause extraction: %.5fs\n", t_clause_extraction);
    printf("Time spent on hC computations: %.5fs\n", t_hc_evaluation);
}

UCHeuristic::UCHeuristic(const Options &opts)
    : HCHeuristic(opts),
      c_eval_hc(opts.get<bool>("eval_hc")),
      c_reeval_hc(opts.get<bool>("reeval_hc")),
      m_clause_store(NULL),
      m_clause_extraction(NULL)
{
    cost_type = OperatorCost::ONE;
    max_ratio_repr_counters = opts.get<float>("x");
    if (opts.contains("clauses")) {
        m_clause_extraction = opts.get<UCClauseExtraction *>("clauses");
        m_clause_store = m_clause_extraction->get_store();
    }
}

UCHeuristic::UCHeuristic(const UCHeuristic &uc)
  : HCHeuristic(uc),
    c_eval_hc(uc.c_eval_hc),
    c_reeval_hc(uc.c_reeval_hc),
    m_clause_store(NULL),
    m_clause_extraction(NULL)
{
}

void UCHeuristic::hc_evaluate(const State &state) {
    if (heuristic == NOT_INITIALIZED) {
        initialize();
    }
    heuristic = HCHeuristic::compute_heuristic(state);
    evaluator_value = heuristic;
}

unsigned UCHeuristic::find_clause(const State &state) {
    m_stats.start();
    unsigned res = m_clause_store ? m_clause_store->find(this, state) : ClauseStore::NO_MATCH;
    m_stats.end(m_stats.t_clause_matching);
    return res;
}

bool UCHeuristic::clause_matches(const State &state) {
    return find_clause(state) != ClauseStore::NO_MATCH;
}

int UCHeuristic::compute_heuristic(const State &state) {
    m_stats.num_total_evaluations++;
    if (clause_matches(state)) {
        return DEAD_END;
    }
    int res = 0;
    if (c_eval_hc) {
        m_stats.start();
        m_stats.num_hc_evaluations++;
        hc_evaluate(state);
        res = heuristic;
        m_stats.end(m_stats.t_hc_evaluation);
        if (res == DEAD_END) {
            m_stats.num_hc_dead_ends++;
            refine_clauses(state, false);
        }
    }
    return res;
}

void UCHeuristic::reevaluate(const State &state) {
    if (heuristic == NOT_INITIALIZED) {
        initialize();
    }
    heuristic = 0;
    if (c_reeval_hc) {
        evaluate(state);
    } else if (clause_matches(state)) {
        heuristic = DEAD_END;
        evaluator_value = DEAD_END;
    }
}

void UCHeuristic::refine_clauses(const std::vector<State> &dead_ends)
{
    for (const State & s : dead_ends) {
        refine_clauses(s);
    }
}

void UCHeuristic::refine_clauses(const State &state, bool comp)
{
    if (!m_clause_extraction || clause_matches(state)) {
        return;
    }
    m_stats.num_clause_extractions++;
    m_stats.start();
    if (comp) {
        hc_evaluate(state);
    }
    assert(is_dead_end());
    m_clause_extraction->refine(this, state);
    m_stats.end(m_stats.t_clause_extraction);
}

void UCHeuristic::statistics() const
{
  //dump_compilation_information();
  HCHeuristic::dump_statistics(std::cout);
    printf("Number of Learned Clauses: %zu\n", m_clause_store ? m_clause_store->size() : 0);
    m_stats.dump();
}

void UCHeuristic::add_options_to_parser(OptionParser &parser)
{
    HCHeuristic::add_options_to_parser(parser);
    parser.add_option<float>("x", "", "1.0");
    parser.add_option<UCClauseExtraction *>("clauses", "", "", OptionFlags(false));
    parser.add_option<bool>("eval_hc", "", "true");
    parser.add_option<bool>("reeval_hc", "", "false");
}

ClauseStore *UCHeuristic::get_clause_store() {
    return m_clause_store;
}

void UCHeuristic::add_default_options(Options &opts)
{
  HCHeuristic::add_default_options(opts);
  opts.set<float>("x", -1);
  opts.set<bool>("eval_hc", true);
  opts.set<bool>("reeval_hc", false);
}

static Heuristic *_parse(OptionParser &parser)
{
    UCHeuristic::add_options_to_parser(parser);
    Options opts = parser.parse();
    if (!parser.dry_run()) {
        return new UCHeuristic(opts);
    }
    return NULL;
}

static Plugin<Heuristic> _plugin_uc("uc", _parse);


