
#ifndef DEBUGGING_SAMPLER_H
#define DEBUGGING_SAMPLER_H

#include "search_engine.h"
#include "heuristic.h"
#include "rng.h"

#include <vector>

class Options;

class HSample : public SearchEngine
{
protected:
    RandomNumberGenerator gen;
    int current_samples;
    int num_samples;
    bool solved;
    int max_depth;
    std::vector<Heuristic*> heuristics;
    std::vector<size_t> num_clocks;
    std::vector<double> initialization;
    std::vector<int> values;
    std::vector<size_t> dead_ends;


    std::vector<double> average;
    std::vector<double> average_old;
    virtual void check_and_print();

    virtual void print_line();
    virtual void initialize();
    virtual int step();
public:
    HSample(const Options &opts);
};

#endif
