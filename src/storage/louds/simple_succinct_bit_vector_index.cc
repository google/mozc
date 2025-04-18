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

#include "storage/louds/simple_succinct_bit_vector_index.h"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "base/bits.h"

namespace mozc {
namespace storage {
namespace louds {
namespace {

// An iterator adaptor that gives the view of 1-bit index as 0-bit index.
class ZeroBitIndexIterator {
 public:
  using difference_type = ptrdiff_t;
  using value_type = int;
  using pointer = const int *;
  using reference = const int &;
  using iterator_category = std::forward_iterator_tag;

  ZeroBitIndexIterator(absl::Span<const int> index, int chunk_size,
                       const int *ptr)
      : data_{index.data()}, chunk_size_{chunk_size}, ptr_{ptr} {}

  const int *ptr() const { return ptr_; }

  ZeroBitIndexIterator &operator++() {
    ++ptr_;
    return *this;
  }

  friend bool operator!=(const ZeroBitIndexIterator &x,
                         const ZeroBitIndexIterator &y) {
    return x.ptr_ != y.ptr_;
  }

  int operator*() const {
    // The number of 0-bits
    //   = (total num bits) - (1-bits)
    //   = (chunk_size [bytes] * 8 [bits/byte] * (ptr's offset) - (1-bits)
    return chunk_size_ * 8 * (ptr_ - data_) - *ptr_;
  }

 private:
  const int *data_;
  int chunk_size_;
  const int *ptr_;
};

inline int BitCount0(uint32_t x) {
  // Flip all bits, and count 1-bits.
  return std::popcount(~x);
}

// Returns 1-bits in the data to length words.
int Count1Bits(const uint8_t *data, int length) {
  int num_bits = 0;
  for (; length > 0; --length) {
    num_bits += std::popcount(LoadUnalignedAdvance<uint32_t>(data));
  }
  return num_bits;
}

// Stores index (the cumulative number of the 1-bits from begin of each chunk).
void InitIndex(const uint8_t *data, int length, int chunk_size,
               std::vector<int> *index) {
  DCHECK_GE(chunk_size, 4);
  DCHECK(std::has_single_bit<uint32_t>(chunk_size)) << chunk_size;
  DCHECK_EQ(length % 4, 0);

  index->clear();

  // Count the number of chunks with ceiling.
  const int chunk_length = (length + chunk_size - 1) / chunk_size;

  // Reserve the memory including a sentinel.
  index->reserve(chunk_length + 1);

  int num_bits = 0;
  for (int remaining_num_words = length / 4; remaining_num_words > 0;
       data += chunk_size, remaining_num_words -= chunk_size / 4) {
    index->push_back(num_bits);
    num_bits += Count1Bits(data, std::min(chunk_size / 4, remaining_num_words));
  }
  index->push_back(num_bits);

  CHECK_EQ(chunk_length + 1, index->size());
}

void InitLowerBound0Cache(absl::Span<const int> index, int chunk_size,
                          size_t increment, size_t size,
                          std::vector<const int *> *cache) {
  DCHECK_GT(increment, 0);
  cache->clear();
  cache->reserve(size + 2);
  cache->push_back(index.data());
  for (size_t i = 1; i <= size; ++i) {
    const int target_index = increment * i;
    const int *ptr =
        std::lower_bound(ZeroBitIndexIterator(index, chunk_size, index.data()),
                         ZeroBitIndexIterator(index, chunk_size,
                                              index.data() + index.size()),
                         target_index)
            .ptr();
    cache->push_back(ptr);
  }
  cache->push_back(index.data() + index.size());
}

void InitLowerBound1Cache(absl::Span<const int> index, int chunk_size,
                          size_t increment, size_t size,
                          std::vector<const int *> *cache) {
  DCHECK_GT(increment, 0);
  cache->clear();
  cache->reserve(size + 2);
  cache->push_back(index.data());
  for (size_t i = 1; i <= size; ++i) {
    const int target_index = increment * i;
    const int *ptr = std::lower_bound(index.data(), index.data() + index.size(),
                                      target_index);
    cache->push_back(ptr);
  }
  cache->push_back(index.data() + index.size());
}

}  // namespace

void SimpleSuccinctBitVectorIndex::Init(const uint8_t *data, int length,
                                        size_t lb0_cache_size,
                                        size_t lb1_cache_size) {
  data_ = data;
  length_ = length;
  InitIndex(data, length, chunk_size_, &index_);

  // TODO(noriyukit): Currently, we simply use uniform increment width for lower
  // bound cache.  Nonuniform increment width may improve performance.
  lb0_cache_increment_ =
      lb0_cache_size == 0 ? GetNum0Bits() : GetNum0Bits() / lb0_cache_size;
  if (lb0_cache_increment_ == 0) {
    lb0_cache_increment_ = 1;
  }
  InitLowerBound0Cache(index_, chunk_size_, lb0_cache_increment_,
                       lb0_cache_size, &lb0_cache_);

  lb1_cache_increment_ =
      lb1_cache_size == 0 ? GetNum1Bits() : GetNum1Bits() / lb1_cache_size;
  if (lb1_cache_increment_ == 0) {
    lb1_cache_increment_ = 1;
  }
  InitLowerBound1Cache(index_, chunk_size_, lb1_cache_increment_,
                       lb1_cache_size, &lb1_cache_);
}

void SimpleSuccinctBitVectorIndex::Reset() {
  data_ = nullptr;
  length_ = 0;
  index_.clear();
  lb0_cache_increment_ = 1;
  lb0_cache_.clear();
  lb1_cache_increment_ = 1;
  lb1_cache_.clear();
}

int SimpleSuccinctBitVectorIndex::Rank1(int n) const {
  // Look up pre-computed 1-bits for the preceding chunks.
  const int num_chunks = n / (chunk_size_ * 8);
  int result = index_[n / (chunk_size_ * 8)];

  // Count 1-bits for remaining "words".
  result += Count1Bits(data_ + num_chunks * chunk_size_,
                       (n / 8 - num_chunks * chunk_size_) / 4);

  // Count 1-bits for remaining "bits".
  if (n % 32 > 0) {
    const int offset = 4 * (n / 32);
    const int shift = 32 - n % 32;
    result += std::popcount(LoadUnaligned<uint32_t>(data_ + offset) << shift);
  }

  return result;
}

int SimpleSuccinctBitVectorIndex::Select0(int n) const {
  DCHECK_GT(n, 0);

  // Narrow down the range of |index_| on which lower bound is performed.
  int lb0_cache_index = n / lb0_cache_increment_;
  if (lb0_cache_index > lb0_cache_.size() - 2) {
    lb0_cache_index = lb0_cache_.size() - 2;
  }
  DCHECK_GE(lb0_cache_index, 0);

  // Binary search on chunks.
  const int *chunk_ptr =
      std::lower_bound(ZeroBitIndexIterator(index_, chunk_size_,
                                            lb0_cache_[lb0_cache_index]),
                       ZeroBitIndexIterator(index_, chunk_size_,
                                            lb0_cache_[lb0_cache_index + 1]),
                       n)
          .ptr();
  const int chunk_index = (chunk_ptr - index_.data()) - 1;
  DCHECK_GE(chunk_index, 0);
  n -= chunk_size_ * 8 * chunk_index - index_[chunk_index];

  // Linear search on remaining "words"
  const int offset = (chunk_index * chunk_size_) & ~int{3};
  const uint8_t *ptr = data_ + offset;
  while (true) {
    const int bit_count = BitCount0(LoadUnaligned<uint32_t>(ptr));
    if (bit_count >= n) {
      break;
    }
    n -= bit_count;
    ptr += 4;
  }

  int index = (ptr - data_) * 8;
  for (uint32_t word = ~LoadUnaligned<uint32_t>(ptr); n > 0;
       word >>= 1, ++index) {
    n -= (word & 1);
  }

  // Index points to the "next bit" of the target one.
  // Thus, subtract one to adjust.
  return index - 1;
}

int SimpleSuccinctBitVectorIndex::Select1(int n) const {
  DCHECK_GT(n, 0);

  // Narrow down the range of |index_| on which lower bound is performed.
  int lb1_cache_index = n / lb1_cache_increment_;
  if (lb1_cache_index > lb1_cache_.size() - 2) {
    lb1_cache_index = lb1_cache_.size() - 2;
  }
  DCHECK_GE(lb1_cache_index, 0);

  // Binary search on chunks.
  const int *chunk_ptr = std::lower_bound(lb1_cache_[lb1_cache_index],
                                          lb1_cache_[lb1_cache_index + 1], n);
  const int chunk_index = (chunk_ptr - index_.data()) - 1;
  DCHECK_GE(chunk_index, 0);
  n -= index_[chunk_index];

  // Linear search on remaining "words"
  const int offset = (chunk_index * chunk_size_) & ~int{3};
  const uint8_t *ptr = data_ + offset;
  while (true) {
    const int bit_count = std::popcount(LoadUnaligned<uint32_t>(ptr));
    if (bit_count >= n) {
      break;
    }
    n -= bit_count;
    ptr += 4;
  }

  int index = (ptr - data_) * 8;
  for (uint32_t word = LoadUnaligned<uint32_t>(ptr); n > 0;
       word >>= 1, ++index) {
    n -= (word & 1);
  }

  // Index points to the "next bit" of the target one.
  // Thus, subtract one to adjust.
  return index - 1;
}

}  // namespace louds
}  // namespace storage
}  // namespace mozc
