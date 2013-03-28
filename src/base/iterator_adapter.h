// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_BASE_ITERATOR_ADAPTER_H_
#define MOZC_BASE_ITERATOR_ADAPTER_H_

#include <iterator>

namespace mozc {

// In some cases, we'd like to create a thin iterator wrapper to use some
// generic STL algorithms to simplify the code.
// This class template takes to type arguments:
//   BaseIter: the base iterator type, which is wrapperd by this class.
//   Adapter: the thin adapter to somehow convert the original value.
// The iterator category of this class inherits the original iterator's,
// so the time complexity for each algorithm should be kept as is in most cases
// (except the value conversion).
// BaseIter and Adapter have some requirements as follows:
//   BaseIter: the BaseIter is expected to be an iterator defined in the C++
//     standard. Any type of iterator (input, output, forward, bidirectional
//     or randome access) is fine, but wrapped iterator inherits its category.
//   Adapter:
//     The adapter needs to have following types.
//     - value_type
//     - pointer
//     - reference
//     They will be typedef'ed in the IteratorAdapter.
// Note that because of the C++ spec, unnecessary methods in template class
// won't be target of the code generation. So, although IteratorAdapter has
// the definition of all the wrapping methods, they won't cause compile errors
// unless an illegal method (e.g. operator[] for forward iterator based
// IteratorAdapter) is actually instantiated.
//
// Usage:
//   struct FirstAdapter {
//     typedef int value_type;
//     typedef int *pointer;
//     typedef int &reference;
//     template<typename Iter>
//     value_type operator()(Iter iter) const {
//       return iter->first;
//     }
//   };
//   vector<pair<int, double> > data;
//     :
//   // To find the first element whose first is 5.
//   typedef IteratorAdapter<vector<pair<int, double> >, FirstAdapter>
//       FirstIterator;
//   vector<pair<int, double> >::const_iterator iter =
//     find(FirstIterator(data.begin()), FirstIterator(data.end()), 5).base();
//
// Below, two utilities are provided, AdapterBase class and MakeIteratorAdapter
// function. With those utilities, the above example would be simpler
// especially for type definitions:
//
//   struct FirstAdapter : public AdapterBase<int> {
//     template<typename Iter>
//     value_type operator()(Iter iter) const {
//       return iter->first;
//     }
//   };
//   vector<pair<int, double> > data;
//   vector<pair<int, double> >::const_iterator iter =
//     find(MakeIteratorAdapter(data.begin(), FirstAdapter()),
//          MakeIteratorAdapter(data.end(), FirstAdapter()), 5).base();
//
// Note that this class is assignable and copy constructive. Also, note that
// this class is designed for searching or counting purpose. So, it is not
// possible to assign or swap the original values (pointed by original
// iterator).
template<typename BaseIter, typename Adapter>
class IteratorAdapter {
 public:
  // Standard type traits for iterator.
  typedef typename iterator_traits<BaseIter>::iterator_category
      iterator_category;
  typedef typename iterator_traits<BaseIter>::difference_type
      difference_type;
  typedef typename Adapter::value_type value_type;
  typedef typename Adapter::pointer pointer;
  typedef typename Adapter::reference reference;

  IteratorAdapter(BaseIter iter, Adapter adapter)
      : iter_(iter), adapter_(adapter) {
  }

  IteratorAdapter() {
  }

  const BaseIter &base() const { return iter_; }

  value_type operator*() {
    return adapter_(iter_);
  }

  pointer operator->() const { return &(operator*()); }

  IteratorAdapter &operator++() { ++iter_; return *this; }
  IteratorAdapter operator++(int) {
    return IteratorAdapter(iter_++, adapter_);
  }
  IteratorAdapter &operator--() { --iter_; return *this; }
  IteratorAdapter operator--(int) {
    return IteratorAdapter(iter_--, adapter_);
  }

  IteratorAdapter &operator+=(const difference_type &diff) {
    iter_ += diff;
    return *this;
  }
  IteratorAdapter &operator-=(const difference_type &diff) {
    iter_ -= diff;
    return *this;
  }
  IteratorAdapter operator+(const difference_type &diff) const {
    return IteratorAdapter(iter_ + diff, adapter_);
  }
  IteratorAdapter operator-(const difference_type &diff) const {
    return IteratorAdapter(iter_ - diff, adapter_);
  }
  friend IteratorAdapter operator+(
      const difference_type &diff, const IteratorAdapter &iter) {
    return iter + diff;
  }
  difference_type operator-(const IteratorAdapter &other) const {
    return iter_ - other.iter_;
  }

  value_type operator[](const difference_type &diff) const {
    return adapter_(iter_ + diff);
  }

  friend bool operator<(
      const IteratorAdapter &iter1, const IteratorAdapter &iter2) {
    return iter1.iter_ < iter2.iter_;
  }

  friend bool operator>(
      const IteratorAdapter &iter1, const IteratorAdapter &iter2) {
    return iter1.iter_ > iter2.iter_;
  }

  friend bool operator<=(
      const IteratorAdapter &iter1, const IteratorAdapter &iter2) {
    return iter1.iter_ <= iter2.iter_;
  }

  friend bool operator>=(
      const IteratorAdapter &iter1, const IteratorAdapter &iter2) {
    return iter1.iter_ >= iter2.iter_;
  }

  bool operator==(const IteratorAdapter &other) const {
    return iter_ == other.iter_;
  }
  bool operator!=(const IteratorAdapter &other) const {
    return iter_ != other.iter_;
  }

 private:
  BaseIter iter_;
  Adapter adapter_;
};

// Provides the typedefs which Adapter in IteratorAdapter requires.
// We can inherit this struct to create a new Adapter type similar to
// std::unary_function and std::binary_function.
// Usage:
//   struct Data {
//     int field1;
//     double field2;
//   };
//   struct DataAdapter : public AdapterBase<int> {
//     template<Iter>
//     int operator()(Iter iter) const {
//       return iter->field1;
//     }
//   };
template<typename T, typename Pointer = T*, typename Reference = T&>
struct AdapterBase {
  typedef T value_type;
  typedef Pointer pointer;
  typedef Reference reference;
};

// Template function will guess the template parameter types based on its
// arguments. This function can be used to avoid writing the template
// parameters explicitly, similar to std::make_pair.
template<typename BaseIter, typename Adapter>
IteratorAdapter<BaseIter, Adapter> MakeIteratorAdapter(
    BaseIter iter, Adapter adapter) {
  return IteratorAdapter<BaseIter, Adapter>(iter, adapter);
}

}  // namespace mozc

#endif  // MOZC_BASE_ITERATOR_ADAPTER_H_
