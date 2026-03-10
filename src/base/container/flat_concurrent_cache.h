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

#ifndef MOZC_BASE_CONTAINER_FLAT_CONCURRENT_CACHE_H_
#define MOZC_BASE_CONTAINER_FLAT_CONCURRENT_CACHE_H_

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "absl/base/internal/spinlock.h"
#include "absl/hash/hash.h"

namespace mozc {

// A memory-efficient, thread-safe, fixed-capacity associative cache designed
// for low-latency lookups and inserts.
// - Data is organized into independent buckets to minimize lock contention.
// - Bucket-Level Locking to provide thread safety.
// - Pseudo-LRU Eviction: Each bucket manages its own eviction independently
//   using a 1-byte aging clock mechanism. When a bucket is full, the entry with
//   the smallest clock value is evicted to make room.
//

template <class Key, class Value, class Hash = absl::Hash<Key>,
          class Equal = std::equal_to<Key>, uint8_t kGroupSize = 9>
class FlatConcurrentCache {
 public:
  explicit FlatConcurrentCache(size_t n)
      : num_buckets_(CalculateBuckets(n)),
        mask_(num_buckets_ - 1),
        buckets_(std::make_unique<Bucket[]>(num_buckets_)) {}

  using SpinLock = absl::base_internal::SpinLock;              // NOLINT
  using SpinLockHolder = absl::base_internal::SpinLockHolder;  // NOLINT

  FlatConcurrentCache(const FlatConcurrentCache&) = delete;
  FlatConcurrentCache(FlatConcurrentCache&&) = delete;
  FlatConcurrentCache& operator=(const FlatConcurrentCache&) = delete;
  FlatConcurrentCache& operator=(FlatConcurrentCache&&) = delete;

  bool Lookup(const Key& key, Value* value) const {
    auto [hash, bucket] = GetBucket(key);
    SpinLockHolder l(&bucket->mu);
    if (int i = bucket->FindIndex(key, hash, equal_); i != -1) {
      *value = *bucket->value[i];
      bucket->UpdateClock(i);
      return true;
    }
    return false;
  }

  // `value` accepts both references and movable objects.
  void Insert(const Key& key, Value&& value) {
    auto [hash, bucket] = GetBucket(key);
    SpinLockHolder l(&bucket->mu);

    // Update the value when key is found.
    if (int i = bucket->FindIndex(key, hash, equal_); i != -1) {
      bucket->value[i] =
          std::forward<Value>(value);  // std::optional::operator=
      bucket->UpdateClock(i);
      return;
    }

    // Evicts the oldest value and insert new value.
    const int index = bucket->Evict();
    bucket->subhash[index] = hash;
    bucket->key[index] = key;  // std::optional::operator=
    bucket->value[index] = std::forward<Value>(value);
    bucket->access_clock[index] = bucket->access_clock_val;
  }

  void Clear() {
    for (size_t i = 0; i < num_buckets_; ++i) {
      SpinLockHolder l(&buckets_[i].mu);
      buckets_[i].Clear();
    }
  }

  template <typename Functor>
  void ForEach(const Functor& functor) const {
    for (size_t i = 0; i < num_buckets_; ++i) {
      const Bucket& bucket = buckets_[i];
      SpinLockHolder l(&bucket.mu);
      for (int j = 0; j < bucket.num; ++j) {
        functor(*bucket.key[j], *bucket.value[j]);
      }
    }
  }

 private:
  struct Bucket {
    mutable SpinLock mu;
    uint8_t subhash[kGroupSize];
    uint8_t num = 0;
    uint8_t access_clock[kGroupSize];
    uint8_t access_clock_val = 0;
    std::optional<Key> key[kGroupSize];
    std::optional<Value> value[kGroupSize];

    void Clear() {
      for (int i = 0; i < num; ++i) {
        key[i].reset();
        value[i].reset();
      }
      num = 0;
      access_clock_val = 0;
    }

    int FindIndex(const Key& k, uint8_t hash, const Equal& equal) const {
      for (int i = 0; i < num; ++i) {
        // key[i] is valid because of `num`.
        if (subhash[i] == hash && equal(k, *key[i])) return i;
      }
      return -1;
    }

    void UpdateClock(int index) {
      // Halve all the access_clock values
      if (++access_clock_val == 255) {
        access_clock_val = 128;
        for (int i = 0; i < num; ++i) access_clock[i] >>= 1;
      }
      access_clock[index] = access_clock_val;
    }

    int Evict() {
      if (num < kGroupSize) {
        return num++;
      }
      // finds the index with the lowest access_clock.
      int min_idx = 0;
      for (int i = 1; i < num; ++i) {
        if (access_clock[i] < access_clock[min_idx]) min_idx = i;
      }
      key[min_idx].reset();
      value[min_idx].reset();
      return min_idx;
    }
  };

  // Determines the optimal number of buckets based on the requested capacity.
  // This function rounds the initial bucket count up to the nearest power of
  // two.
  static size_t CalculateBuckets(size_t n) {
    const size_t target = (n + kGroupSize - 1) / kGroupSize;
    if (target <= 1) return 1;
    size_t pow2 = 1;
    while (pow2 < target) {
      pow2 <<= 1;
    }
    return pow2;
  }

  std::pair<uint8_t, Bucket*> GetBucket(const Key& key) const {
    const size_t hash = hasher_(key);
    const uint8_t subhash = static_cast<uint8_t>(hash & 0xff);
    const size_t rotated_h = (hash >> 8) | (hash << (sizeof(hash) * 8 - 8));
    return std::make_pair(subhash, &buckets_[rotated_h & mask_]);
  }

  const Hash hasher_ = Hash();
  const Equal equal_ = Equal();
  const size_t num_buckets_;
  const size_t mask_;
  const std::unique_ptr<Bucket[]> buckets_;
};
}  // namespace mozc
#endif  // #ifndef MOZC_BASE_CONTAINER_FLAT_MAP_H_
