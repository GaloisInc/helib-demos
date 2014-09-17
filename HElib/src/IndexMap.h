/* Copyright (C) 2012,2013 IBM Corp.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef _IndexMap
#define _IndexMap
/**
 * @file IndexMap.h
 * @brief Implementation of a map indexed by a dynamic set of integers.
 **/

#include <iostream>
#include <cassert>

#if (__cplusplus>199711L)
#include <unordered_map>
#else
#include <tr1/unordered_map>
#endif

#include "cloned_ptr.h"
#include "IndexSet.h"

using namespace std;

//! @brief Initializing elements in an IndexMap
template < class T > class IndexMapInit {
public:
  //! @brief Initialization function, override with initialization code
  virtual void init(T&) = 0;

  //! @brief Cloning a pointer, override with code to create a fresh copy
  virtual IndexMapInit<T> * clone() const = 0; 
  virtual ~IndexMapInit() {} // ensure that derived destructor is called
};


//! @brief IndexMap<T> implements a generic map indexed by a dynamic index set.
//!
//! Additionally, it allows new elements of the map to be initialized in a
//! flexible manner.
template < class T > class IndexMap {

#if (__cplusplus>199711L)
  std::unordered_map<long, T> map;
#else
  tr1::unordered_map<long, T> map;
#endif

  IndexSet indexSet;
  cloned_ptr< IndexMapInit<T> > init;

public:

  //! @brief The empty map
  IndexMap(); 

  //! @brief A map with an initialization object.
  //! This associates a method for initializing new elements in the map.
  //! When a new index j is added to the index set, an object t of type T is
  //! created using the default constructor for T, after which the function
  //! _init->init(t) is called (t is passed by reference). To use this
  //! feature, you need to derive a subclass of IndexMapInit<T> that defines
  //! the init function. This "helper object" should be created using
  //! operator new, and the pointer is "exclusively owned" by the map object.
  explicit IndexMap(IndexMapInit<T> *_init) : init(_init) { }

  //! @brief Get the underlying index set
  const IndexSet& getIndexSet() const { return indexSet; }

  //! @brief Access functions: will raise an error 
  //! if j does not belong to the current index set 
  T& operator[] (long j) { 
    assert(indexSet.contains(j)); 
    return map[j];
  }
  const T& operator[] (long j) const {
    assert(indexSet.contains(j)); 
    // unordered_map does not support a const [] operator,
    // so we have to artificially strip away the const-ness here
#if (__cplusplus>199711L)
    std::unordered_map<long, T> & map1 = 
      const_cast< std::unordered_map<long, T> & > (map);
#else
    tr1::unordered_map<long, T> & map1 = 
      const_cast< tr1::unordered_map<long, T> & > (map);
#endif
    return map1[j];
  }

  //! @brief Insert indexes to the IndexSet.
  //! Insertion will cause new T objects to be created, using the default
  //! constructor, and possibly initilized via the IndexMapInit<T> pointer.
  void insert(long j) { 
    if (!indexSet.contains(j)) {
      indexSet.insert(j);
      if (!init.null()) init->init(map[j]);
    }
  }
  void insert(const IndexSet& s) { 
    for (long i = s.first(); i <= s.last(); i = s.next(i))
       insert(i);
  }

  //! @brief Delete indexes from IndexSet, may cause objects to be destroyed.
  void remove(long j) { indexSet.remove(j); map.erase(j); }
  void remove(const IndexSet& s) { 
    for (long i = s.first(); i <= s.last(); i = s.next(i))
      map.erase(i);
    indexSet.remove(s);
  }

  void clear() { 
    map.clear();
    indexSet.clear();
  }  


};

//! @brief Comparing maps, by comparing all the elements
template <class T> 
bool operator==(const IndexMap<T>& map1, const IndexMap<T>& map2) {
  if (map1.getIndexSet() != map2.getIndexSet()) return false;
  const IndexSet& s = map1.getIndexSet();
  for (long i = s.first(); i <= s.last(); i = s.next(i)) {
    if (map1[i] == map2[i]) continue;
    return false;
  }
  return true;
}

template <class T> 
bool operator!=(const IndexMap<T>& map1, const IndexMap<T>& map2) {
  return !(map1 == map2);
}

  

#endif
