#include "fluent_set_utilities.h"

#include <algorithm>
#include <iostream>

bool inconsistent(const FluentSet &a) {
  for (int i = 0; i < a.size(); i++) {
    for (int j = i + 1; j < a.size(); j++) {
      if(are_mutex(a[i], a[j])) {
        return true;
      }
    }
  }
  return false;
}

bool
are_mutex(const FluentSet &a, const FluentSet &b) {
  for (int i = 0; i < a.size(); i++) {
    for (int j = 0; j < b.size(); j++) {
      if(are_mutex(a[i], b[j])) {
        return true;
      }
    }
  }
  return false;
}

bool
are_mutex(const FluentSet &fs, const Fluent &single) {
  for (int i = 0; i < fs.size(); i++) {
    if(are_mutex(fs[i], single)) {
       return true;
    }
  }
  return false;
}

// find all size m or less subsets of superset
void recursive_get_m_sets(int m, int num_included, int current_var_index,
                          FluentSet &current, std::vector<FluentSet > &subsets,
                          const FluentSet &superset) {

  if (num_included == m) {
    subsets.push_back(current);
    return;
  }

  if (current_var_index == superset.size()) {
    if (num_included != 0) {
      subsets.push_back(current);
    }
    return;
  }

  // include current fluent in the set
  current.push_back(superset[current_var_index]);
  recursive_get_m_sets(m, num_included + 1, current_var_index + 1, current, subsets, superset);
  current.pop_back();

  // don't include current fluent in set
  recursive_get_m_sets(m, num_included, current_var_index + 1, current, subsets, superset);
}

void get_m_sets(int m, std::vector<FluentSet > &subsets,
                const FluentSet &superset) {
  FluentSet empty;
  recursive_get_m_sets(m, 0, 0, empty, subsets, superset);
}


void get_all_subsets(std::vector<FluentSet> &subsets, const FluentSet &superset) {
  get_m_sets(superset.size(), subsets, superset);
}

bool has_subset(const FluentSet &superset, const FluentSet &subset) {

  for(FluentSet::const_iterator it = subset.begin(); it != subset.end(); it++) {
    if(std::find(superset.begin(), superset.end(), *it) == superset.end()) {
      return false;
    }
  }
  return true;
}

bool have_intersection(const FluentSet &a, const FluentSet &b) {
  for(FluentSet::const_iterator ita = a.begin(); ita != a.end(); ita++) {
    for(FluentSet::const_iterator itb = b.begin(); itb != b.end(); itb++) {
      if( *ita == *itb ) {
        return true;
      }
    }
  }
  return false;
}

void get_combined_fluent_set(const FluentSet &to_add, FluentSet &result) {
  for(FluentSet::const_iterator it = to_add.begin(); it != to_add.end(); it++) {
    if(std::find(result.begin(), result.end(), *it) == result.end()) {
      result.push_back(*it);
    }
  }
  std::sort(result.begin(), result.end());
}


std::ostream & operator<<(std::ostream &os, const Fluent &p) {
  return os << g_fact_names[p.first][p.second];
}

std::ostream & operator<<(std::ostream &os, const FluentSet &fs) {
  os << "[";
  for(int i = 0; i < fs.size(); i++) {
    os << fs[i];
    if(i != (fs.size()-1)) {
      os << " ";
    }
  }
  os << "]";
  return os;
}

