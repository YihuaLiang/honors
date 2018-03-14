#ifndef FLUENT_SET_UTILITIES_H
#define FLUENT_SET_UTILITIES_H

#include "../globals.h"

typedef std::pair<int, int> Fluent;
typedef std::vector<Fluent> FluentSet;

std::ostream &operator<<(std::ostream &os, const Fluent &p);
std::ostream &operator<<(std::ostream &os, const FluentSet &fs);

bool inconsistent(const FluentSet &a);
bool are_mutex(const FluentSet &a, const FluentSet &b);
bool are_mutex(const FluentSet &fs, const Fluent &single);

void recursive_get_m_sets(int m, int num_included, int current_var_index,
                          FluentSet &current, std::vector<FluentSet > &subsets,
                          const FluentSet &superset);

void get_m_sets(int m, std::vector<FluentSet > &subsets, const FluentSet &superset);
void get_all_subsets(std::vector<FluentSet> &subsets, const FluentSet &superset);
bool has_subset(const FluentSet &superset, const FluentSet &subset);
bool have_intersection(const FluentSet &a, const FluentSet &b);
void get_combined_fluent_set(const FluentSet &to_add, FluentSet &result);

#endif
