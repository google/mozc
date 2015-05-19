// Copyright 2010-2014, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_BASE_STL_UTIL_H_
#define MOZC_BASE_STL_UTIL_H_

#include <functional>

namespace mozc {

template <class ForwardIterator>
void STLDeleteContainerPointers(ForwardIterator begin,
                                ForwardIterator end) {
  while (begin != end) {
    ForwardIterator temp = begin;
    ++begin;
    delete *temp;
  }
}

template <class T>
void STLDeleteElements(T *container) {
  if (!container) {
    return;
  }
  STLDeleteContainerPointers(container->begin(), container->end());
  container->clear();
}

// Comparator functor based of a member.
// Key: a function object to return key from the given value.
// Comparator: a base comparator function object.
template <typename Key, typename Comparator>
class OrderBy {
 public:
  OrderBy() {
  }

  OrderBy(const Key &key, const Comparator &comparator)
      : key_(key), comparator_(comparator) {
  }

  template<typename T>
  bool operator()(const T &lhs, const T &rhs) const {
    return comparator_(key_(lhs), key_(rhs));
  }

 private:
  Key key_;
  Comparator comparator_;
};

// Function object to return pair<T1, T2>::first.
struct FirstKey {
  template<typename T>
  const typename T::first_type &operator()(const T &value) const {
    return value.first;
  }
};

// Function object to return pair<T1, T2>::second.
struct SecondKey {
  template<typename T>
  const typename T::second_type &operator()(const T &value) const {
    return value.second;
  }
};

// Simple wrapper of binary predicator in functional.
template<template<typename T> class Comparator>
struct FunctionalComparator {
  template<typename T>
  bool operator()(const T &lhs, const T &rhs) const {
    return Comparator<T>()(lhs, rhs);
  }
};
typedef FunctionalComparator<equal_to> EqualTo;
typedef FunctionalComparator<not_equal_to> NotEqualTo;
typedef FunctionalComparator<less> Less;
typedef FunctionalComparator<greater> Greater;
typedef FunctionalComparator<less_equal> LessEqual;
typedef FunctionalComparator<greater_equal> GreaterEqual;

}  // namespace mozc

#endif  // MOZC_BASE_STL_UTIL_H_
