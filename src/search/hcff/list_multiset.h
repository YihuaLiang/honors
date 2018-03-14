#ifndef LIST_MULTISET_H
#define LIST_MULTISET_H

#include <list>

// union of two multisets
// each object represented max(#A, #B) times in result

template<typename T>
void multiset_union(std::list<std::pair<T, int> > &alist,
		    const std::list<std::pair<T, int> > &other) {
  typename std::list<std::pair<T, int> >::iterator it1 = alist.begin();
  typename std::list<std::pair<T, int> >::const_iterator it2 = other.begin();
  
  while ((it1 != alist.end()) && (it2 != other.end())) {
    if (it1->first < it2->first) {
      ++it1;
    }
    else if (it1->first > it2->first) {
      alist.insert(it1, *it2);
      ++it2;
    }
    else {
      it1->second = std::max(it1->second, it2->second);
      ++it1;
      ++it2;
    }
  }
  alist.insert(it1, it2, other.end());
}

// sum union of two multisets
// each object represented sum(#A, #B) times in result

template<typename T>
void multiset_sum_union(std::list<std::pair<T, int> > &alist,
		    const std::list<std::pair<T, int> > &other) {
  typename std::list<std::pair<T, int> >::iterator it1 = alist.begin();
  typename std::list<std::pair<T, int> >::const_iterator it2 = other.begin();
  
  while ((it1 != alist.end()) && (it2 != other.end())) {
    if (it1->first < it2->first) {
      ++it1;
    }
    else if (it1->first > it2->first) {
      alist.insert(it1, *it2);
      ++it2;
    }
    else {
      it1->second = it1->second + it2->second;
      ++it1;
      ++it2;
    }
  }
  alist.insert(it1, it2, other.end());
}

// increase count of object in set by 1

template<typename T>
void multiset_insert(std::list<std::pair<T, int> > &alist, 
		     const T &val) {

  typename std::list<std::pair<T, int> >::iterator it1 = alist.begin();
  
  while (it1 != alist.end()) {
    if (it1->first > val) {
      alist.insert(it1, std::make_pair(val, 1));
      return;
    }
    else if (it1->first < val) {
      ++it1;
    }
    else {
      it1->second++;
      return;
    }
  }
  alist.insert(it1, std::make_pair(val, 1));
}

#endif
