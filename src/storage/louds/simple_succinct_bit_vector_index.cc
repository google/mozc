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

#include "storage/louds/simple_succinct_bit_vector_index.h"

#include <algorithm>
#include <iterator>
#include <vector>
#include "base/base.h"
#include "base/iterator_adapter.h"
#include "base/logging.h"

namespace mozc {
namespace storage {
namespace louds {

namespace {
#ifdef __GNUC__
// TODO(hidehiko): Support XMM and 64-bits popcount for 64bits architectures.
inline int BitCount1(uint32 x) {
  return __builtin_popcount(x);
}
#else
int BitCount1(uint32 x) {
  x = ((x & 0xaaaaaaaa) >> 1) + (x & 0x55555555);
  x = ((x & 0xcccccccc) >> 2) + (x & 0x33333333);
  x = ((x >> 4) + x) & 0x0f0f0f0f;
  x = (x >> 8) + x;
  x = ((x >> 16) + x) & 0x3f;
  return x;
}
#endif

inline int BitCount0(uint32 x) {
  // Flip all bits, and count 1-bits.
  return BitCount1(~x);
}

inline bool IsPowerOfTwo(int value) {
  // value & -value is the well-known idiom to take the lowest 1-bit in
  // value, so value & ~(value & -value) clears the lowest 1-bit in value.
  // If the result is 0, value has only one 1-bit i.e. it is power of two.
  return value != 0 && (value & ~(value & -value)) == 0;
}

// Returns 1-bits in the data[0] ... data[length - 1].
int Count1Bits(const uint32 *data, int length) {
  int num_bits = 0;
  for (; length > 0; ++data, --length) {
    num_bits += BitCount1(*data);
  }
  return num_bits;
}

// Stores index (the camulative number of the 1-bits from begin of each chunk).
void InitIndex(
    const uint8 *data, int length, int chunk_size, vector<int> *index) {
  DCHECK_GE(chunk_size, 4);
  DCHECK(IsPowerOfTwo(chunk_size)) << chunk_size;
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
    num_bits += Count1Bits(
        reinterpret_cast<const uint32 *>(data),
        min(chunk_size / 4, remaining_num_words));
  }
  index->push_back(num_bits);

  CHECK_EQ(chunk_length + 1, index->size());
}
}  // namespace

void SimpleSuccinctBitVectorIndex::Init(const uint8 *data, int length) {
  data_ = data;
  length_ = length;
  InitIndex(data, length, chunk_size_, &index_);
}

void SimpleSuccinctBitVectorIndex::Reset() {
  data_ = NULL;
  length_ = 0;
  index_.clear();
}

int SimpleSuccinctBitVectorIndex::Rank1(int n) const {
  // Look up pre-computed 1-bits for the preceding chunks.
  const int num_chunks = n / (chunk_size_ * 8);
  int result = index_[n / (chunk_size_ * 8)];

  // Count 1-bits for remaining "words".
  result += Count1Bits(
      reinterpret_cast<const uint32 *>(data_ + num_chunks * chunk_size_),
      (n / 8 - num_chunks * chunk_size_) / 4);

  // Count 1-bits for remaining "bits".
  if (n % 32 > 0) {
    const int index = n / 32;
    const int shift = 32 - n % 32;
    result +=
        BitCount1(reinterpret_cast<const uint32 *>(data_)[index] << shift);
  }

  return result;
}

namespace {

// Simple adapter to convert from 1-bit index to 0-bit index.
class ZeroBitAdapter : public AdapterBase<int> {
 public:
  // Needs to be default constructive to create invalid iterator.
  ZeroBitAdapter() {}

  ZeroBitAdapter(const vector<int>* index, int chunk_size)
      : index_(index), chunk_size_(chunk_size) {
  }

  template<typename Iter>
  value_type operator()(Iter iter) const {
    // The number of 0-bits
    //   = (total num bits) - (1-bits)
    //   = (chunk_size [bytes] * 8 [bits/byte] * (iter's position) - (1-bits)
    return chunk_size_ * 8 * distance(index_->begin(), iter) - *iter;
  }

 private:
  const vector<int> *index_;
  int chunk_size_;
};

}  // namespace

int SimpleSuccinctBitVectorIndex::Select0(int n) const {
  DCHECK_GT(n, 0);

  // Binary search on chunks.
  ZeroBitAdapter adapter(&index_, chunk_size_);
  const vector<int>::const_iterator iter = lower_bound(
      MakeIteratorAdapter(index_.begin(), adapter),
      MakeIteratorAdapter(index_.end(), adapter),
      n).base();
  const int chunk_index = distance(index_.begin(), iter) - 1;
  DCHECK_GE(chunk_index, 0);
  n -= chunk_size_ * 8 * chunk_index - index_[chunk_index];

  // Linear search on remaining "words"
  const uint32 *ptr =
      reinterpret_cast<const uint32 *>(data_) + chunk_index * chunk_size_ / 4;
  while (true) {
    const int bit_count = BitCount0(*ptr);
    if (bit_count >= n) {
      break;
    }
    n -= bit_count;
    ++ptr;
  }

  int index = (ptr - reinterpret_cast<const uint32 *>(data_)) * 32;
  for (uint32 word = ~(*ptr); n > 0; word >>= 1, ++index) {
    n -= (word & 1);
  }

  // Index points to the "next bit" of the target one.
  // Thus, subtract one to adjust.
  return index - 1;
}

int SimpleSuccinctBitVectorIndex::Select1(int n) const {
  DCHECK_GT(n, 0);

  // Binary search on chunks.
  const vector<int>::const_iterator iter =
      lower_bound(index_.begin(), index_.end(), n);
  const int chunk_index = distance(index_.begin(), iter) - 1;
  DCHECK_GE(chunk_index, 0);
  n -= index_[chunk_index];

  // Linear search on remaining "words"
  const uint32 *ptr =
      reinterpret_cast<const uint32 *>(data_) + chunk_index * chunk_size_ / 4;
  while (true) {
    const int bit_count = BitCount1(*ptr);
    if (bit_count >= n) {
      break;
    }
    n -= bit_count;
    ++ptr;
  }

  int index = (ptr - reinterpret_cast<const uint32 *>(data_)) * 32;
  for (uint32 word = *ptr; n > 0; word >>= 1, ++index) {
    n -= (word & 1);
  }

  // Index points to the "next bit" of the target one.
  // Thus, subtract one to adjust.
  return index - 1;
}

}  // namespace louds
}  // namespace storage
}  // namespace mozc
