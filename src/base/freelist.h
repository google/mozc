// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_BASE_FREELIST_H_
#define MOZC_BASE_FREELIST_H_

#include <vector>

#include "base/port.h"

namespace mozc {

// Note: The program will be crashed in the following code;
//   FreeList<int> freelist(10);
//   freelist.Alloc(5);
//   freelist.set_size(100);
//   int* array = freelist.Alloc(10);
//   array[8] = 0;  // crashed.
// Also this class runs unneeded T's constructor along with the memory chunk
// allocation.
// Please do take care to use this class.
template <class T>
class FreeList {
 public:
  explicit FreeList(size_t size)
      : current_index_(0), chunk_index_(0), size_(size) {}

  ~FreeList() {
    for (size_t i = 0; i < pool_.size(); ++i) {
      delete[] pool_[i];
    }
  }

  void Reset() { chunk_index_ = current_index_ = 0; }

  void Free() {
    for (size_t i = 1; i < pool_.size(); ++i) {
      delete[] pool_[i];
    }
    if (pool_.size() > 1) {
      pool_.resize(1);
    }
    current_index_ = 0;
    chunk_index_ = 0;
  }

  T* Alloc() { return Alloc(static_cast<size_t>(1)); }

  T* Alloc(size_t len) {
    if ((current_index_ + len) >= size_) {
      chunk_index_++;
      current_index_ = 0;
    }

    if (chunk_index_ == pool_.size()) {
      pool_.push_back(new T[size_]);
    }

    T* r = pool_[chunk_index_] + current_index_;
    current_index_ += len;
    return r;
  }

  void set_size(size_t size) { size_ = size; }

 private:
  std::vector<T*> pool_;
  size_t current_index_;
  size_t chunk_index_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(FreeList);
};

template <class T>
class ObjectPool {
 public:
  explicit ObjectPool(int size) : freelist_(size) {}

  ~ObjectPool() {}

  void Free() {
    released_.clear();
    freelist_.Free();
  }

  T* Alloc() {
    if (!released_.empty()) {
      T* result = released_.back();
      released_.pop_back();
      return result;
    }
    return freelist_.Alloc(1);
  }

  void Release(T* ptr) { released_.push_back(ptr); }

  void set_size(int size) { freelist_.set_size(size); }

 private:
  std::vector<T*> released_;
  FreeList<T> freelist_;

  DISALLOW_COPY_AND_ASSIGN(ObjectPool);
};

}  // namespace mozc

#endif  // MOZC_BASE_FREELIST_H_
