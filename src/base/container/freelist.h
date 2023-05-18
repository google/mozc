// Copyright 2010-2021, Google Inc.
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

#ifndef MOZC_BASE_CONTAINER_FREELIST_H_
#define MOZC_BASE_CONTAINER_FREELIST_H_

#include <cstddef>
#include <memory>
#include <vector>

namespace mozc {

// This class runs unneeded T's constructor along with the memory chunk
// allocation.
// Please do take care to use this class.
template <class T>
class FreeList {
 public:
  explicit FreeList(size_t size)
      : current_index_(0), chunk_index_(0), size_(size) {}

  FreeList(FreeList&&) = default;
  FreeList& operator=(FreeList&&) = default;

  void Reset() { chunk_index_ = current_index_ = 0; }

  void Free() {
    if (pool_.size() > 1) {
      pool_.resize(1);
    }
    current_index_ = 0;
    chunk_index_ = 0;
  }

  T* Alloc() {
    if (current_index_ >= size_) {
      chunk_index_++;
      current_index_ = 0;
    }

    if (chunk_index_ == pool_.size()) {
      pool_.push_back(std::make_unique<T[]>(size_));
    }

    T* r = pool_[chunk_index_].get() + current_index_;
    current_index_++;
    return r;
  }

  constexpr size_t size() const { return size_; }

 private:
  std::vector<std::unique_ptr<T[]>> pool_;
  size_t current_index_;
  size_t chunk_index_;
  size_t size_;
};

template <class T>
class ObjectPool {
 public:
  explicit ObjectPool(int size) : freelist_(size) {}
  ObjectPool(ObjectPool&&) = default;
  ObjectPool& operator=(ObjectPool&&) = default;

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
    return freelist_.Alloc();
  }

  void Release(T* ptr) { released_.push_back(ptr); }

  constexpr size_t size() const { return freelist_.size(); }

 private:
  std::vector<T*> released_;
  FreeList<T> freelist_;
};

}  // namespace mozc

#endif  // MOZC_BASE_CONTAINER_FREELIST_H_
