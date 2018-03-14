
#ifndef PIC_LANDMARK_H
#define PIC_LANDMARK_H

#include "../conjunction_max_heuristic.h"
#include "landmark_factory.h"
#include "landmark_graph.h"

#include <set>

struct LMEntry {
    int level;
    std::set<size_t> landmark;
    std::set<size_t> necessary;

    std::set<size_t> first_achievers;

    LMEntry()
    {
        level = -1;
    }
};

struct LMOp {
    int handled;
    int op_id;
    size_t preconditions;
    size_t effect;
    LMOp()
    {
        handled = -1;
    }
};

class PiCLandmark : public LandmarkFactory
{
private:
    virtual void generate_landmarks();
    virtual void calc_achievers();
protected:
    ConjunctionMaxHeuristic &h;
    std::vector<LMEntry> conjunction_levels;
    std::vector<LMOp> counter_preconditions;
    std::vector<LandmarkNode *> lm_nodes;

    void call_janitor();
    void enqueue_relevant_counters(size_t conj_id, bool decr_pre,
                              std::vector<size_t> &queue);
    void build_landmarks();
    void add_lm_node(size_t conj_id);
    void build_landmark_graph();
public:
    PiCLandmark(const Options &opts, ConjunctionMaxHeuristic &h);
    
    static void add_options_to_parser(OptionParser &parser);
};



#endif

