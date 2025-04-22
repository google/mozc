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
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/absl_nullability.h"
#include "testing/friend_test.h"

namespace mozc {

// This class runs unneeded T's constructor along with the memory chunk
// allocation.
// Please do take care to use this class.
template <class T>
class FreeList {
 public:
  using value_type = T;
  // FreeList currently only supports std::allocator.
  using allocator_type = std::allocator<T>;
  using allocator_traits = std::allocator_traits<std::allocator<T>>;
  using reference = T&;
  using const_reference = const T&;
  using size_type = typename allocator_traits::size_type;
  using difference_type = typename allocator_traits::difference_type;
  using pointer = typename allocator_traits::pointer;
  using const_pointer = typename allocator_traits::const_pointer;

  static_assert(!std::is_const_v<T>, "FreeList can't hold a const type.");

  explicit FreeList(const size_type chunk_size) : chunk_size_(chunk_size) {}

  FreeList(FreeList&& other) noexcept
      : pool_(std::move(other.pool_)),
        next_in_chunk_(other.next_in_chunk_),
        chunk_size_(other.chunk_size_),
        allocator_(std::move(other.allocator_)) {
    static_assert(std::is_nothrow_move_constructible_v<decltype(pool_)>);
    // Only need to clear the pool because Destroy is no-op if the pool is
    // empty.
    other.pool_.clear();
  }

  FreeList& operator=(FreeList&& other) noexcept {
    static_assert(std::is_nothrow_move_assignable_v<decltype(pool_)>);
    // Destroy `this` freelist and move `other` over.
    Destroy();
    pool_ = std::move(other.pool_);
    next_in_chunk_ = other.next_in_chunk_;
    chunk_size_ = other.chunk_size_;
    static_assert(
        allocator_traits::propagate_on_container_move_assignment::value,
        "std::allocator is supposed to propagate on move assignments.");
    allocator_ = std::move(other.allocator_);
    other.pool_.clear();
    return *this;
  }

  ~FreeList() { Destroy(); }

  void Free() {
    Destroy();
    pool_.clear();
    next_in_chunk_ = std::numeric_limits<size_type>::max();
  }

  T* absl_nonnull Alloc() {
    if (next_in_chunk_ >= chunk_size_) {
      next_in_chunk_ = 0;
      // Allocate the chunk with the allocate and delay the constructions until
      // the objects are actually requested.
      pool_.push_back(allocator_traits::allocate(allocator_, chunk_size_));
    }

    // Default construct T.
    T* absl_nonnull ptr = pool_.back() + next_in_chunk_++;
    allocator_traits::construct(allocator_, ptr);
    return ptr;
  }

  constexpr bool empty() const { return size() == 0; }
  constexpr size_type size() const {
    if (pool_.empty()) {
      return 0;
    } else {
      return (pool_.size() - 1) * chunk_size_ + next_in_chunk_;
    }
  }
  constexpr size_type capacity() const { return pool_.size() * chunk_size_; }
  constexpr size_type chunk_size() const { return chunk_size_; }

  allocator_type get_allocator() const { return allocator_; }

  void swap(FreeList& other) noexcept {
    static_assert(std::is_nothrow_swappable_v<decltype(pool_)>);
    using std::swap;
    swap(pool_, other.pool_);
    swap(next_in_chunk_, other.next_in_chunk_);
    swap(chunk_size_, other.chunk_size_);
    if constexpr (allocator_traits::propagate_on_container_swap::value) {
      swap(allocator_, other.allocator_);
    }
  }

  friend void swap(FreeList& lhs, FreeList& rhs) noexcept { lhs.swap(rhs); }

 private:
  // Destroys the freelist so it's ready to be destructed or overwritten by
  // move.
  void Destroy() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
      // Destruct all elements. Skip this entirely if T is trivially
      // destructible.
      if (!pool_.empty()) {
        for (auto it = pool_.begin(); it != pool_.end() - 1; ++it) {
          for (difference_type i = 0; i < chunk_size_; ++i) {
            allocator_traits::destroy(allocator_, *it + i);
          }
        }
        for (difference_type i = 0; i < next_in_chunk_; ++i) {
          allocator_traits::destroy(allocator_, pool_.back() + i);
        }
      }
    }

    // Deallocate the chunks in the pool.
    for (T* chunk : pool_) {
      allocator_traits::deallocate(allocator_, chunk, chunk_size_);
    }
  }

  std::vector<T*> pool_;
  size_type next_in_chunk_ = std::numeric_limits<size_type>::max();
  size_type chunk_size_;
  allocator_type allocator_;
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
  FRIEND_TEST(SegmentsTest, BasicTest);

  std::vector<T*> released_;
  FreeList<T> freelist_;
};

}  // namespace mozc

#endif  // MOZC_BASE_CONTAINER_FREELIST_H_
