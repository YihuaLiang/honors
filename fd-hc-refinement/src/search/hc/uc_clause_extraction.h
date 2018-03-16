
#ifndef UC_CLAUSE_LEARNING_H
#define UC_CLAUSE_LEARNING_H

#include "hc_heuristic.h"

#include <vector>

class Options;
class State;

class ClauseStore {
public:
    static constexpr const unsigned NO_MATCH = (unsigned) -1;
    //const Clause &operator[](unsigned i) const;
    //virtual unsigned store(const Clause &clause) = 0;
    virtual unsigned find(HCHeuristic *h, const State &state) = 0;
    virtual bool empty() const
    {
        return size() == 0;
    }
    virtual size_t size() const = 0;
};

class GreedyStateMinClauseStore : public ClauseStore {
protected:
    std::vector<std::vector<std::pair<int, int> > > clauses;
    std::vector<unsigned> clause_sizes;
    std::vector<std::vector<std::vector<unsigned> > > fact_to_clause;
public:
    typedef std::vector<std::pair<int, int> > Clause;
    GreedyStateMinClauseStore();
    const Clause &operator[](unsigned i) const;
    unsigned store(const Clause &clause);
    virtual unsigned find(HCHeuristic *h, const State &state);
    virtual size_t size() const;
};

class UCClauseExtraction {
public:
    UCClauseExtraction(const Options &) {}
    virtual void refine(HCHeuristic *h, const State &state) = 0;
    virtual ClauseStore *get_store() = 0;
};

class NoClauseLearning : public UCClauseExtraction {
public:
    NoClauseLearning(const Options &opts) : UCClauseExtraction(opts) {}
    virtual void refine(HCHeuristic *, const State &) {
        return;
    }
    virtual ClauseStore *get_store()
    {
        return NULL;
    }
};

class GreedyMinVarUCClauseExtraction : public UCClauseExtraction {
protected:
    GreedyStateMinClauseStore store;
    std::vector<int> _state_conjs;
    bool project_var(HCHeuristic *h, int var, int orig_val);
public:
    GreedyMinVarUCClauseExtraction(const Options &opts);
    virtual void refine(HCHeuristic *h, const State &state);
    virtual ClauseStore *get_store() {
        return &store;
    }
};


class CPGClauseStore : public ClauseStore {
    unsigned m_num_clauses;
    std::vector<std::vector<unsigned> > m_clauses;
    std::vector<int> __subset;
    std::vector<bool> __matched;
public:
    CPGClauseStore();
    void add(unsigned conj_id, unsigned clause_id);
    virtual unsigned find(HCHeuristic *h, const State &state);
    std::vector<unsigned> &get_clauses(unsigned conj_id);
    virtual size_t size() const;
    void new_clause() { m_num_clauses += 1; }
};

class CPGClauseLearning : public UCClauseExtraction {
    CPGClauseStore store;
    std::vector<bool> marked;
public:
    CPGClauseLearning(const Options &opts);
    void collect(HCHeuristic *h, unsigned counter_id, unsigned clause_id);
    virtual void refine(HCHeuristic *h, const State &state);
    virtual ClauseStore *get_store();
};



#endif
