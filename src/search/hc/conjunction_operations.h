#ifndef DEDET_SET_OP_H
#define DEDET_SET_OP_H

#include <set>

namespace fluent_op {
    /**
     * result will be set to some x in set1 with x in set2 (if existing)
     * returns true if there is some x in set1 so that x in set2,
     *          false otherwise
     */
    bool intersection_not_empty(const std::set<std::pair<int, int> > &set1,
                                const std::set<std::pair<int, int> > &set2,
                                std::pair<int, int> &result);
    /**
     * returns true if there is some x in set1 so that x in set2,
     *          false otherwise
     */
    bool intersection_not_empty(const std::set<std::pair<int, int> > &set1,
                                const std::set<std::pair<int, int> > &set2);
    /**
     * every x in set1 with x in set2 will be added to result
     */
    void intersection(const std::set<std::pair<int, int> > &set1,
                      const std::set<std::pair<int, int> > &set2,
                      std::set<std::pair<int, int> > &result);
    /**
     * returns true if for every x in set1, it is x not in set2
     */
    bool are_disjoint(const std::set<std::pair<int, int> > &set1,
                      const std::set<std::pair<int, int> > &set2);
    /**
     * returns true if for every x in set1, it is x in set2
     */
    bool is_subset(const std::set<std::pair<int, int> > &set1,
                   const std::set<std::pair<int, int> > &set2);
    /**
     * every x in set1 with x not in set2 will be added to result
     * returns true if there is some x in set1 with x not in set2, and
     *          false otherwise
     * (i.e., returns true iff intersection of set1 and set2 is non-empty)
     */
    // get the minus set return at the result 
    bool set_minus(const std::set<std::pair<int, int> > &set1,
                   const std::set<std::pair<int, int> > &set2,
                   std::set<std::pair<int, int> > &result);

    bool set_minus(const std::set<std::pair<int, int> > &set1,
                   const std::set<std::pair<int, int> > &set2,
                   const std::set<std::pair<int, int> > &set3,
                   std::set<std::pair<int, int> > &result);


    /**
     * returns the number of x in set1 s.t. x not in set2
     *
     */
    int count_set_minus(const std::set<std::pair<int, int> > &set1,
                   const std::set<std::pair<int, int> > &set2);
}

#endif

