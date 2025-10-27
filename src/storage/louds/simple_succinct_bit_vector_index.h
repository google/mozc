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

#ifndef MOZC_STORAGE_LOUDS_SIMPLE_SUCCINCT_BIT_VECTOR_INDEX_H_
#define MOZC_STORAGE_LOUDS_SIMPLE_SUCCINCT_BIT_VECTOR_INDEX_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace mozc {
namespace storage {
namespace louds {

// This is simple(naive) C++ implementation of succinct bit vector.
class SimpleSuccinctBitVectorIndex {
 public:
  // The default chunk_size is 32.
  SimpleSuccinctBitVectorIndex()
      : data_(nullptr),
        length_(0),
        chunk_size_(32),
        lb0_cache_increment_(1),
        lb1_cache_increment_(1) {}

  // chunk_size is in bytes, and must be greater than or equal to 4
  // and power of 2, at the moment, although we may relax the restriction
  // in future if necessary.
  explicit SimpleSuccinctBitVectorIndex(int chunk_size)
      : data_(nullptr),
        length_(0),
        chunk_size_(chunk_size),
        lb0_cache_increment_(1),
        lb1_cache_increment_(1) {}

  SimpleSuccinctBitVectorIndex(const SimpleSuccinctBitVectorIndex&) = delete;
  SimpleSuccinctBitVectorIndex& operator=(const SimpleSuccinctBitVectorIndex&) =
      delete;

  SimpleSuccinctBitVectorIndex(SimpleSuccinctBitVectorIndex&&) = default;
  SimpleSuccinctBitVectorIndex& operator=(SimpleSuccinctBitVectorIndex&&) =
      default;

  // Initializes the index. This class doesn't have the ownership of the memory
  // pointed by data, so it is caller's responsibility to manage its life time.
  // The 'data' needs to be aligned to 32-bits.
  void Init(const uint8_t* data, int length, size_t lb0_cache_size,
            size_t lb1_cache_size);

  void Init(const uint8_t* data, int length) { Init(data, length, 0, 0); }

  // Resets the internal state, especially releases the allocated memory
  // for the index used internally.
  void Reset();

  // Returns the bit at the index in data. The index in a byte is as follows;
  // MSB|XXXXXXXX|LSB
  //     76543210
  int Get(int index) const { return (data_[index / 8] >> (index % 8)) & 1; }

  // Returns the number of 0-bit in [0, n) bits of data.
  int Rank0(int n) const { return n - Rank1(n); }

  // Returns the number of 1-bit in [0, n) bits of data.
  int Rank1(int n) const;

  // Returns the position of n-th 0-bit on the data. (n is 1-origin).
  // Returned index is 0-origin.
  int Select0(int n) const;

  // Returns the position of n-th 1-bit in the data. (n is 1-origin).
  // Returned index is 0-origin.
  int Select1(int n) const;

  int GetNum1Bits() const { return index_.back(); }
  int GetNum0Bits() const { return 8 * length_ - index_.back(); }

 private:
  // The order of members is optimized to minimize the padding size.
  const uint8_t* data_;
  int length_;
  int chunk_size_;
  std::vector<int> index_;
  std::vector<const int*> lb0_cache_;
  int lb0_cache_increment_;
  int lb1_cache_increment_;
  std::vector<const int*> lb1_cache_;
};

}  // namespace louds
}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_LOUDS_SIMPLE_SUCCINCT_BIT_VECTOR_INDEX_H_
