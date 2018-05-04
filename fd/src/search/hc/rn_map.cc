
#include "rn_map.h"

#include "uc_clause_extraction.h"

#include "../globals.h"
#include "../state_registry.h"
#include "../state.h"
#include "../state_id.h"

#include "../option_parser.h"
#include "../plugin.h"

void StateRNMap::operator()(UCHeuristic *h, StateID state_id)
{
    State state = g_state_registry->lookup_state(state_id);
#ifndef NDEBUG
    int res = h->simple_traversal(state);
    assert(res == Heuristic::DEAD_END);
#else
    h->simple_traversal(state);
#endif
}

void StateRNMap::operator()(UCHeuristic *h, StateID state_id, int g_value)
{
    State state = g_state_registry->lookup_state(state_id);
#ifndef NDEBUG
    int res = h->simple_traversal(state,g_value);
    assert(res == Heuristic::DEAD_END);
#else
    h->simple_traversal(state,g_value);
#endif
}

void StateRNMap::operator()(UCHeuristic *,
                            const std::unordered_set<StateID> &in,
                            std::unordered_set<StateID> &out)
{
    for (std::unordered_set<StateID>::const_iterator it = in.begin(); it != in.end(); it++) {
        out.insert(*it);
    }
    assert(in == out);
}



void ClauseRNMap::operator()(UCHeuristic *h, StateID clause_id)
{
    GreedyStateMinClauseStore &store = *dynamic_cast<GreedyStateMinClauseStore *>
                                       (h->get_clause_store());
    const GreedyStateMinClauseStore::Clause &clause = store[clause_id.hash()];
    std::vector<unsigned> exploration;
    std::vector<int> subset;
    subset.resize(h->num_conjunctions(), 0);
    for (uint i = 0; i < h->num_conjunctions(); i++) {
        h->get_conjunction(i).clear();
    }
    std::vector<int> abstr_state;
    abstr_state.resize(g_variable_domain.size(), -1);
    for (const std::pair<int, int> &fact : clause) {
        abstr_state[fact.first] = fact.second;
    }
    for (uint var = 0; var < g_variable_domain.size(); var++) {
        if (abstr_state[var] != -1) {
            const std::vector<unsigned> &conjs = h->get_fact_conj_relation(
                    h->get_fact_id(var, abstr_state[var]));
            for (const unsigned & conj : conjs) {
                Conjunction &c = h->get_conjunction(conj);
                if (++subset[conj] == c.fluent_size) {
                    c.check_and_update(0, NULL);
                    exploration.push_back(conj);
                }
            }
            continue;
        }
        for (int val = 0; val < g_variable_domain[var]; val++) {
            const std::vector<unsigned> &conjs = h->get_fact_conj_relation(
                    h->get_fact_id(var, val));
            for (const unsigned & conj : conjs) {
                Conjunction &c = h->get_conjunction(conj);
                if (++subset[conj] == c.fluent_size) {
                    c.check_and_update(0, NULL);
                    exploration.push_back(conj);
                }
            }
        }
    }
    for (uint i = 0; i < h->num_counters(); i++) {
        ActionEffectCounter &counter = h->get_counter(i);
        counter.unsatisfied_preconditions = counter.preconditions;
        if (counter.unsatisfied_preconditions == 0) {
            Conjunction &conj = *counter.effect;
            if (conj.check_and_update(0, NULL)) {
                exploration.push_back(conj.id);
            }
        }
    }

#ifndef NDEBUG
    assert(h->simple_traversal_wrapper(exploration, 0) == Heuristic::DEAD_END);
#else
    h->simple_traversal_wrapper(exploration, 0);
#endif
}

void ClauseRNMap::operator()(UCHeuristic *h, StateID clause_id, int g_value)
{
    GreedyStateMinClauseStore &store = *dynamic_cast<GreedyStateMinClauseStore *>
                                       (h->get_clause_store());
    const GreedyStateMinClauseStore::Clause &clause = store[clause_id.hash()];
    std::vector<unsigned> exploration;
    std::vector<int> subset;
    subset.resize(h->num_conjunctions(), 0);
    for (uint i = 0; i < h->num_conjunctions(); i++) {
        h->get_conjunction(i).clear();
    }
    std::vector<int> abstr_state;
    abstr_state.resize(g_variable_domain.size(), -1);
    for (const std::pair<int, int> &fact : clause) {
        abstr_state[fact.first] = fact.second;
    }
    for (uint var = 0; var < g_variable_domain.size(); var++) {
        if (abstr_state[var] != -1) {
            const std::vector<unsigned> &conjs = h->get_fact_conj_relation(
                    h->get_fact_id(var, abstr_state[var]));
            for (const unsigned & conj : conjs) {
                Conjunction &c = h->get_conjunction(conj);
                if (++subset[conj] == c.fluent_size) {
                    c.check_and_update(0, NULL);
                    exploration.push_back(conj);
                }
            }
            continue;
        }
        for (int val = 0; val < g_variable_domain[var]; val++) {
            const std::vector<unsigned> &conjs = h->get_fact_conj_relation(
                    h->get_fact_id(var, val));
            for (const unsigned & conj : conjs) {
                Conjunction &c = h->get_conjunction(conj);
                if (++subset[conj] == c.fluent_size) {
                    c.check_and_update(0, NULL);
                    exploration.push_back(conj);
                }
            }
        }
    }
    for (uint i = 0; i < h->num_counters(); i++) {
        ActionEffectCounter &counter = h->get_counter(i);
        counter.unsatisfied_preconditions = counter.preconditions;
        if (counter.unsatisfied_preconditions == 0) {
            Conjunction &conj = *counter.effect;
            if (conj.check_and_update(0, NULL)) {
                exploration.push_back(conj.id);
            }
        }
    }

#ifndef NDEBUG
    assert(h->simple_traversal_wrapper(exploration, 0, g_value) == Heuristic::DEAD_END);
#else
    h->simple_traversal_wrapper(exploration, 0, g_value);
#endif
}

void ClauseRNMap::operator()(UCHeuristic *h,
                            const std::unordered_set<StateID> &in,
                            std::unordered_set<StateID> &out)
{
    for (std::unordered_set<StateID>::const_iterator it = in.begin(); it != in.end(); it++) {
        out.insert(StateID(h->find_clause(g_state_registry->lookup_state(*it))));
    }
}


static RNMap *_parse_state(OptionParser &parser) {
    if (!parser.dry_run()) {
        return new StateRNMap();
    }
    return NULL;
}

static RNMap *_parse_clause(OptionParser &parser) {
    if (!parser.dry_run()) {
        return new ClauseRNMap();
    }
    return NULL;
}

static Plugin<RNMap> _plugin_state("state", _parse_state);
static Plugin<RNMap> _plugin_clause("clause", _parse_clause);

