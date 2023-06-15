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
#include <limits>
#include <new>
#include <type_traits>
#include <utility>
#include <vector>

namespace mozc {

// This class runs unneeded T's constructor along with the memory chunk
// allocation.
// Please do take care to use this class.
template <class T>
class FreeList {
 public:
  explicit FreeList(const size_t chunk_size) : chunk_size_(chunk_size) {}

  FreeList(FreeList&& other) noexcept
      : pool_(std::move(other.pool_)),
        next_in_chunk_(other.next_in_chunk_),
        chunk_size_(other.chunk_size_) {
    static_assert(std::is_nothrow_move_constructible_v<decltype(pool_)>);
    // Only need to clear the pool because both DestructPools and
    // DeallocatePools are no-op if the pool is empty.
    other.pool_.clear();
  }
  FreeList& operator=(FreeList&& other) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<decltype(pool_)>);
    if (this != &other) {
      // Destroy `this` freelist and move `other` over.
      Destroy();
      pool_ = std::move(other.pool_);
      next_in_chunk_ = other.next_in_chunk_;
      chunk_size_ = other.chunk_size_;
      other.pool_.clear();
    }
    return *this;
  }

  ~FreeList() { Destroy(); }

  void Free() {
    Destroy();
    pool_.clear();
    next_in_chunk_ = std::numeric_limits<size_t>::max();
  }

  T* Alloc() {
    if (next_in_chunk_ >= chunk_size_) {
      next_in_chunk_ = 0;
      // Allocate the chunk with operator new to delay construction until the
      // object is actually requested.
      static_assert(
          alignof(T) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
          "FreeList doesn't support alignment bigger than std::max_align_t.");
      pool_.push_back(reinterpret_cast<std::byte*>(
          ::operator new(sizeof(T) * chunk_size_)));
    }

    // Default construct T with placement new.
    return new (ObjectStorageAt(pool_.back(), next_in_chunk_++)) T;
  }

  constexpr bool empty() const { return size() == 0; }
  constexpr size_t size() const {
    if (pool_.empty()) {
      return 0;
    } else {
      return (pool_.size() - 1) * chunk_size_ + next_in_chunk_;
    }
  }
  constexpr size_t capacity() const { return pool_.size() * chunk_size_; }
  constexpr size_t chunk_size() const { return chunk_size_; }

  void swap(FreeList& other) noexcept {
    static_assert(std::is_nothrow_swappable_v<decltype(pool_)>);
    using std::swap;
    swap(pool_, other.pool_);
    swap(next_in_chunk_, other.next_in_chunk_);
    swap(chunk_size_, other.chunk_size_);
  }

  friend void swap(FreeList& lhs, FreeList& rhs) noexcept { lhs.swap(rhs); }

 private:
  // Destructs all objects in the pool.
  void DestructAllObjects() {
    // Skip it entirely if T is trivially destructible.
    if constexpr (!std::is_trivially_destructible_v<T>) {
      // Destruct all elements.
      if (!pool_.empty()) {
        for (auto it = pool_.begin(); it != pool_.end() - 1; ++it) {
          for (size_t j = 0; j < chunk_size_; ++j) {
            ObjectAt(*it, j)->~T();
          }
        }
        for (size_t i = 0; i < next_in_chunk_; ++i) {
          ObjectAt(pool_.back(), i)->~T();
        }
      }
    }
  }
  // Deallocates chunks in the pool. It doesn't actually clear the pool.
  void DeallocateChunks() {
    for (auto it = pool_.begin(); it != pool_.end(); ++it) {
      ::operator delete(*it);
    }
  }
  // Destroys the freelist so it's ready to be destructed or overwritten by
  // move.
  void Destroy() {
    DestructAllObjects();
    DeallocateChunks();
  }
  // Returns a pointer to a byte array in the pool where the n-th object of T
  // should occupy.
  std::byte* ObjectStorageAt(std::byte* chunk, size_t n) {
    return chunk + n * sizeof(T);
  }
  // Returns a pointer to T at index n in the byte array.
  T* ObjectAt(std::byte* chunk, size_t n) {
    return std::launder(reinterpret_cast<T*>(ObjectStorageAt(chunk, n)));
  }

  // Using std::byte here instead of T because std::byte allows storage reuse.
  std::vector<std::byte*> pool_;
  size_t next_in_chunk_ = std::numeric_limits<size_t>::max();
  size_t chunk_size_;
};

template <class T>
class ObjectPool {
 public:
  explicit ObjectPool(const int chunk_size) : freelist_(chunk_size) {}
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

  constexpr bool empty() const { return size() == 0; }
  constexpr size_t size() const { return freelist_.size() - released_.size(); }
  constexpr size_t capacity() const { return freelist_.capacity(); }
  constexpr size_t chunk_size() const { return freelist_.chunk_size(); }

  void swap(ObjectPool& other) noexcept {
    static_assert(std::is_nothrow_swappable_v<decltype(released_)>);
    static_assert(std::is_nothrow_swappable_v<decltype(freelist_)>);
    using std::swap;
    swap(released_, other.released_);
    swap(freelist_, other.freelist_);
  }

  friend void swap(ObjectPool& lhs, ObjectPool& rhs) noexcept { lhs.swap(rhs); }

 private:
  std::vector<T*> released_;
  FreeList<T> freelist_;
};

}  // namespace mozc

#endif  // MOZC_BASE_CONTAINER_FREELIST_H_
