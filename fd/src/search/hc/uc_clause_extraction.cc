
#include "uc_clause_extraction.h"

#include "../globals.h"
#include "../state.h"
#include "../option_parser.h"
#include "../plugin.h"
//here will also need g_value
GreedyStateMinClauseStore::GreedyStateMinClauseStore()
{
    fact_to_clause.resize(g_variable_domain.size());
    for (uint var = 0; var < fact_to_clause.size(); var++) {
        fact_to_clause[var].resize(g_variable_domain[var]);
    }
}

const GreedyStateMinClauseStore::Clause &GreedyStateMinClauseStore::operator[](unsigned i) const
{
    return clauses[i];
}

unsigned GreedyStateMinClauseStore::store(const Clause &clause)
{
    clauses.push_back(clause);
    for (uint i = 0; i < clause.size(); i++) {
        fact_to_clause[clause[i].first][clause[i].second].push_back(
            clause_sizes.size());
    }
    clause_sizes.push_back(clause.size());
    return clause_sizes.size() - 1;
}

unsigned GreedyStateMinClauseStore::find(HCHeuristic *, const State &state)
{
    std::vector<unsigned> subset;
    subset.resize(clause_sizes.size(), 0);
    for (uint var = 0; var < g_variable_domain.size(); var++) {
        for (unsigned c : fact_to_clause[var][state[var]]) {
            if (++subset[c] == clause_sizes[c]) {
                return c;
            }
        }
    }
    return NO_MATCH;
}

size_t GreedyStateMinClauseStore::size() const
{
    return clause_sizes.size();
}






GreedyMinVarUCClauseExtraction::GreedyMinVarUCClauseExtraction(
    const Options &opts) : UCClauseExtraction(opts) {}

bool GreedyMinVarUCClauseExtraction::project_var(HCHeuristic *h, int var,
        int orig_val)
{
    std::vector<unsigned> exploration;
    exploration.reserve(h->num_conjunctions());

    // Add all values of the given variable to the current state
    for (int val = 0; val < g_variable_domain[var]; val++) {
        if (val == orig_val) {
            continue;
        }
        unsigned fid = h->get_fact_id(var, val);
        const std::vector<unsigned> &conjs = h->get_fact_conj_relation(fid);
        for (unsigned cid : conjs) {
            _state_conjs[cid]++;
            Conjunction &conj = h->get_conjunction(cid);

            // check whether all facts of the conjunctions are now contained
            // in the current state
            // however, if we have already achieved the conjunction in the
            // previous iteration of the hmax computation, then there is
            // nothing left to do
            if (!conj.is_achieved()
                && _state_conjs[cid] == conj.fluent_size) {
                conj.check_and_update(0, NULL);
                exploration.push_back(cid);
            }
        }
    }

    // check whether the state is still a dead end
    bool is_dead = h->simple_traversal_wrapper(exploration,
                   0) == Heuristic::DEAD_END;

    if (!is_dead) {
        // Have to revert all changes made during relaxed traversal
        // - all conjunctions that are contained in the exploration queue
        //  were unreachable before
        for (unsigned cid : exploration) {
            Conjunction &conj = h->get_conjunction(cid);
            conj.clear();
            const std::vector<ActionEffectCounter *> &triggered = conj.triggered_counters;
            for (size_t i = 0; i < triggered.size(); i++) {
                triggered[i]->unsatisfied_preconditions++;
            }
        }
        // Remove the facts from the current state that were added before
        for (int val = 0; val < g_variable_domain[var]; val++) {
            if (val == orig_val) {
                continue;
            }
            unsigned fid = h->get_fact_id(var, val);
            const std::vector<unsigned> &conjs = h->get_fact_conj_relation(fid);
            for (int cid : conjs) {
                _state_conjs[cid]--;
            }
        }
    }
    exploration.clear();
    return is_dead;
}
//reload
bool GreedyMinVarUCClauseExtraction::project_var(HCHeuristic *h, int var,
        int orig_val, int g_value)
{
    std::vector<unsigned> exploration;
    exploration.reserve(h->num_conjunctions());

    // Add all values of the given variable to the current state
    for (int val = 0; val < g_variable_domain[var]; val++) {
        if (val == orig_val) {
            continue;
        }
        unsigned fid = h->get_fact_id(var, val);
        const std::vector<unsigned> &conjs = h->get_fact_conj_relation(fid);
        for (unsigned cid : conjs) {
            _state_conjs[cid]++;
            Conjunction &conj = h->get_conjunction(cid);

            // check whether all facts of the conjunctions are now contained
            // in the current state
            // however, if we have already achieved the conjunction in the
            // previous iteration of the hmax computation, then there is
            // nothing left to do
            if (!conj.is_achieved()
                && _state_conjs[cid] == conj.fluent_size) {
                conj.check_and_update(0, NULL);
                exploration.push_back(cid);
            }
        }
    }

    // check whether the state is still a dead end
    //reload
    bool is_dead = h->simple_traversal_wrapper(exploration,
                   0,g_value) == Heuristic::DEAD_END;
    if (!is_dead) {
        // Have to revert all changes made during relaxed traversal
        // - all conjunctions that are contained in the exploration queue
        //  were unreachable before
        for (unsigned cid : exploration) {
            Conjunction &conj = h->get_conjunction(cid);
            conj.clear();
            const std::vector<ActionEffectCounter *> &triggered = conj.triggered_counters;
            for (size_t i = 0; i < triggered.size(); i++) {
                triggered[i]->unsatisfied_preconditions++;
            }
        }
        // Remove the facts from the current state that were added before
        for (int val = 0; val < g_variable_domain[var]; val++) {
            if (val == orig_val) {
                continue;
            }
            unsigned fid = h->get_fact_id(var, val);
            const std::vector<unsigned> &conjs = h->get_fact_conj_relation(fid);
            for (int cid : conjs) {
                _state_conjs[cid]--;
            }
        }
    }
    exploration.clear();
    return is_dead;
}
void GreedyMinVarUCClauseExtraction::refine(HCHeuristic *h,
        const State &state)
{
    assert(h->is_dead_end());
    _state_conjs.resize(h->num_conjunctions(), 0);
    for (uint var = 0; var < g_variable_domain.size(); var++) {
        const std::vector<unsigned> &conjs = h->get_fact_conj_relation(
                h->get_fact_id(var, state[var]));
        for (int cid : conjs) {
            _state_conjs[cid]++;
        }
    }

    // try to project away each variable
    // if not possible, add it to the nogood
    GreedyStateMinClauseStore::Clause clause;

    bool _term = h->set_early_termination(false);
    for (int var = g_variable_domain.size() - 1; var >= 0; var--) {
        if (!project_var(h, var, state[var])) {
            clause.push_back(std::make_pair(var, state[var]));
        }
    }
    h->set_early_termination(_term);


    store.store(clause);

    _state_conjs.clear();
}
//reload
void GreedyMinVarUCClauseExtraction::refine(HCHeuristic *h,
        const State &state, int g_value)
{
    assert(h->is_dead_end());
    _state_conjs.resize(h->num_conjunctions(), 0);
    for (uint var = 0; var < g_variable_domain.size(); var++) {
        const std::vector<unsigned> &conjs = h->get_fact_conj_relation(
                h->get_fact_id(var, state[var]));
        for (int cid : conjs) {
            _state_conjs[cid]++;
        }
    }
    // try to project away each variable
    // if not possible, add it to the nogood
    GreedyStateMinClauseStore::Clause clause;

    bool _term = h->set_early_termination(false);
    for (int var = g_variable_domain.size() - 1; var >= 0; var--) {
        if (!project_var(h, var, state[var], g_value)) {
            clause.push_back(std::make_pair(var, state[var]));
        }
    }
    h->set_early_termination(_term);


    store.store(clause);

    _state_conjs.clear();
}
static UCClauseExtraction *_parse_statemin(OptionParser &parser) {
    Options opts = parser.parse();
    if (!parser.dry_run()) {
        return new GreedyMinVarUCClauseExtraction(opts);
    }
    return NULL;
}

static UCClauseExtraction *_parse_none(OptionParser &parser) {
    Options opts = parser.parse();
    if (!parser.dry_run()) {
        return new NoClauseLearning(opts);
    }
    return NULL;
}
//use var to statemin refine
static Plugin<UCClauseExtraction> _var("statemin", _parse_statemin);
static Plugin<UCClauseExtraction> _none("none", _parse_none);

