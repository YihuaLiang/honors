
#ifndef UC_HEURISTIC_H
#define UC_HEURISTIC_H

#include "hc_heuristic.h"

#include "uc_clause_extraction.h"

#include <unordered_set>
#include <vector>
#include <ctime>
#include <string.h>
class UCRefinement;

struct ClauseLearningStatistics {
    clock_t _t;
public:

    size_t num_total_evaluations;
    size_t num_hc_evaluations;

    size_t num_dead_ends;
    size_t num_hc_dead_ends;

    size_t num_clause_extractions;

    double t_clause_matching;
    double t_clause_extraction;

    double t_hc_evaluation;

    ClauseLearningStatistics() :
        num_total_evaluations(0),
        num_hc_evaluations(0),
        num_dead_ends(0),
        num_hc_dead_ends(0),
        num_clause_extractions(0),
        t_clause_matching(0),
        t_clause_extraction(0),
        t_hc_evaluation(0)
    {}

    void start() {
        _t = clock();
    }

    void end(double &res) {
        res += ((double) (clock() - _t)) / ((double) CLOCKS_PER_SEC);
    }

    void dump() const;
};

class UCHeuristic : public HCHeuristic
{
    friend class UCRefinement;
protected:
    const bool c_eval_hc;
    const bool c_reeval_hc;

    ClauseLearningStatistics m_stats;
    ClauseStore *m_clause_store;
    UCClauseExtraction *m_clause_extraction;

    virtual int compute_heuristic(const State &state);
    virtual int compute_heuristic(const State &state, int g_value);

    void hc_evaluate(const State &state);
    void hc_evaluate(const State &state, int g_value);


    void set_dead_end() {
        heuristic = DEAD_END;
        evaluator_value = DEAD_END;
    }
public:
    UCHeuristic(const Options &opts);
    UCHeuristic(const UCHeuristic &h);
    virtual void reevaluate(const State &state);
    //reload
     std::string h_name = "UC_heuristic";
    virtual void reevaluate(const State &state, int g_value);
    unsigned find_clause(const State &state);
    bool clause_matches(const State &state);

    virtual void refine_clauses(const State &dead_end, bool comp = true);
    virtual void refine_clauses(const std::vector<State> &dead_ends);

    virtual void statistics() const;

    ClauseStore *get_clause_store();

    static void add_options_to_parser(OptionParser &parser);
    static void add_default_options(Options &opts);
};

#endif

