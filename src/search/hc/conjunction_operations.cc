
#include "conjunction_operations.h"

namespace fluent_op
{

bool intersection_not_empty(const std::set<std::pair<int, int> > &set1,
                            const std::set<std::pair<int, int> > &set2,
                            std::pair<int, int> &result)
{
    if (set1.empty() || set2.empty()) {
        return false;
    }

    std::set<std::pair<int, int> >::const_iterator
    it1 = set1.begin(),
    it1End = set1.end();
    std::set<std::pair<int, int> >::const_iterator
    it2 = set2.begin(),
    it2End = set2.end();

    while (it1 != it1End && it2 != it2End) {
        if (*it1 < *it2) {
            it1++;
        } else if (*it1 > *it2) {
            it2++;
        } else {
            result = *it1;
            return true;
        }
    }

    return false;
}

bool intersection_not_empty(const std::set<std::pair<int, int> > &set1,
                            const std::set<std::pair<int, int> > &set2)
{
    if (set1.empty() || set2.empty()) {
        return false;
    }

    std::set<std::pair<int, int> >::const_iterator
    it1 = set1.begin(),
    it1End = set1.end();
    std::set<std::pair<int, int> >::const_iterator
    it2 = set2.begin(),
    it2End = set2.end();

    while (it1 != it1End && it2 != it2End) {
        if (*it1 < *it2) {
            it1++;
        } else if (*it1 > *it2) {
            it2++;
        } else {
            return true;
        }
    }

    return false;
}

void intersection(const std::set<std::pair<int, int> > &set1,
                  const std::set<std::pair<int, int> > &set2,
                  std::set<std::pair<int, int> > &result)
{
    if (set1.empty() || set2.empty()) {
        return;
    }

    std::set<std::pair<int, int> >::const_iterator
    it1 = set1.begin(),
    it1End = set1.end();
    std::set<std::pair<int, int> >::const_iterator
    it2 = set2.begin(),
    it2End = set2.end();

    while (it1 != it1End && it2 != it2End) {
        if (*it1 < *it2) {
            it1++;
        } else if (*it1 > *it2) {
            it2++;
        } else {
            result.insert(*it1);
            it1++;
            it2++;
        }
    }
}

bool are_disjoint(const std::set<std::pair<int, int> > &set1,
                  const std::set<std::pair<int, int> > &set2)
{
    if (set1.empty() || set2.empty()) {
        return true;
    }

    std::set<std::pair<int, int> >::const_iterator
    it1 = set1.begin(),
    it1End = set1.end();
    std::set<std::pair<int, int> >::const_iterator
    it2 = set2.begin(),
    it2End = set2.end();

    while (it1 != it1End && it2 != it2End) {
        if (*it1 < *it2) {
            it1++;
        } else if (*it1 > *it2) {
            it2++;
        } else {
            return false;
        }
    }

    return true;
}

bool is_subset(const std::set<std::pair<int, int> > &set1,
               const std::set<std::pair<int, int> > &set2)
{
    if (set1.empty()) {
        return true;
    }
    if (set2.empty()) {
        return false;
    }

    std::set<std::pair<int, int> >::const_iterator
    it1 = set1.begin(),
    it1End = set1.end();
    std::set<std::pair<int, int> >::const_iterator
    it2 = set2.begin(),
    it2End = set2.end();

    if (*it1 < *it2 || *set1.rbegin() > *set2.rbegin()) {
        return false;
    }

    while (it1 != it1End && it2 != it2End) {
        if (*it1 < *it2) {
            return false;
        } else if (*it1 > *it2) {
            it2++;
        } else {
            it1++;
            it2++;
        }
    }

    return it1 == it1End;
}

bool set_minus(const std::set<std::pair<int, int> > &set1,
               const std::set<std::pair<int, int> > &set2,
               std::set<std::pair<int, int> > &result)
{
    if (set1.empty()) {
        return false;
    }
    if (set2.empty()) {
        result.insert(set1.begin(), set1.end());
        return false;
    }

    std::set<std::pair<int, int> >::const_iterator
    it1 = set1.begin(),
    it1End = set1.end();
    std::set<std::pair<int, int> >::const_iterator
    it2 = set2.begin(),
    it2End = set2.end();

    bool hit = false;
    while (it1 != it1End && it2 != it2End) {
        if (*it1 < *it2) {
            result.insert(*it1);
            it1++;
        } else if (*it1 > *it2) {
            it2++;
        } else {
            hit = true;
            it1++;
            it2++;
        }
    }

    while (it1 != it1End) {
        result.insert(*it1);
        it1++;
    }

    return hit;
}

bool set_minus(const std::set<std::pair<int, int> > &set1,
               const std::set<std::pair<int, int> > &set2,
               const std::set<std::pair<int, int> > &set3,
               std::set<std::pair<int, int> > &result)
{
    if (set1.empty()) {
        return false;
    }
    if (set2.empty()) {
        result.insert(set1.begin(), set1.end());
        return false;
    }

    std::set<std::pair<int, int> >::const_iterator
    it1 = set1.begin(),
    it1End = set1.end();
    std::set<std::pair<int, int> >::const_iterator
    it2 = set2.begin(),
    it2End = set2.end();
    std::set<std::pair<int, int> >::const_iterator
    it3 = set3.begin(),
    it3End = set3.end();

    bool hit = false;

    while (it1 != it1End && it2 != it2End && it3 != it3End) {
        if (*it1 < *it2 && *it1 < *it3) {
            result.insert(*it1);
            it1++;
        } else if (*it1 > *it2) {
            it2++;
        } else if (*it1 > *it3) {
            it3++;
        } else if (*it1 == *it2) {
            hit = true;
            it1++;
            it2++;
        } else if (*it1 == *it3) {
            hit = true;
            it1++;
            it3++;
        }
    }

    while (it1 != it1End && it2 != it2End) {
        if (*it1 < *it2) {
            result.insert(*it1);
            it1++;
        } else if (*it1 > *it2) {
            it2++;
        } else {
            hit = true;
            it1++;
            it2++;
        }
    }

    while (it1 != it1End && it3 != it3End) {
        if (*it1 < *it3) {
            result.insert(*it1);
            it1++;
        } else if (*it1 > *it3) {
            it3++;
        } else {
            hit = true;
            it1++;
            it3++;
        }
    }

    while (it1 != it1End) {
        result.insert(*it1);
        it1++;
    }

    return hit;
}

int count_set_minus(const std::set<std::pair<int, int> > &set1,
                    const std::set<std::pair<int, int> > &set2)
{
    if (set1.empty()) {
        return 0;
    }
    if (set2.empty()) {
        return set1.size();
    }

    std::set<std::pair<int, int> >::const_iterator
    it1 = set1.begin(),
    it1End = set1.end();
    std::set<std::pair<int, int> >::const_iterator
    it2 = set2.begin(),
    it2End = set2.end();

    int counter = 0;
    while (it1 != it1End && it2 != it2End) {
        if (*it1 < *it2) {
            counter++;
            it1++;
        } else if (*it1 > *it2) {
            it2++;
        } else {
            it1++;
            it2++;
        }
    }

    while (it1 != it1End) {
        counter++;
        it1++;
    }

    return counter;
}

}
