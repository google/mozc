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

#ifndef MOZC_STORAGE_EXISTENCE_FILTER_H_
#define MOZC_STORAGE_EXISTENCE_FILTER_H_

#include <cstdint>
#include <memory>

#include "base/port.h"

namespace mozc {
namespace storage {
namespace internal {

class BlockBitmap;
constexpr uint32_t BitsToWords(uint32_t bits);

}  // namespace internal

// Bloom filter
class ExistenceFilter {
 public:
  struct Header {
    uint32_t m;
    uint32_t n;
    int k;
  };

  // 'm' is the number of bits in the bit vector
  // 'n' is the number of values that will be stored
  // 'k' is the number of hash values to use per insert/lookup
  // k must be less than 8
  ExistenceFilter(uint32_t m, uint32_t n, int k);
  ExistenceFilter(const ExistenceFilter &) = delete;
  ExistenceFilter &operator=(const ExistenceFilter &) = delete;
  ~ExistenceFilter() = default;

  static ExistenceFilter *CreateOptimal(size_t size_in_bytes,
                                        uint32_t estimated_insertions);

  void Clear();

  // Inserts a hash value into the filter
  // We generate 'k' separate internal hash values
  void Insert(uint64_t hash);

  // Checks if the given 'hash' was previously inserted int the filter
  // It may return some false positives
  bool Exists(uint64_t hash) const;

  // Returns the size (in bytes) of the bloom filter
  constexpr size_t Size() const {
    return (internal::BitsToWords(vec_size_) * sizeof(uint32_t));
  }

  // Returns the minimum required size of the filter in bytes
  // under the given error rate and number of elements
  static size_t MinFilterSizeInBytesForErrorRate(float error_rate,
                                                 size_t num_elements);

  void Write(char **buf, size_t *size);

  static bool ReadHeader(const char *buf, Header *header);

  // Read Existence filter from buf[]
  // Note that the returned ExsitenceFilter is immutable filter.
  // Any mutable operations will destroy buf[].
  static ExistenceFilter *Read(const char *buf, size_t size);

 private:
  // private constructor for ExistenceFilter::Read();
  ExistenceFilter(uint32_t m, uint32_t n, int k, bool is_mutable);

  std::unique_ptr<internal::BlockBitmap> rep_;  // points to bitmap
  const uint32_t vec_size_;                     // size of bitmap (in bits)
  const uint32_t expected_nelts_;               // expected number of inserts
  const int32_t num_hashes_;                    // number of hashes per lookup
};

namespace internal {

class BlockBitmap {
 public:
  BlockBitmap(uint32_t length, bool is_mutable);
  BlockBitmap(const BlockBitmap &) = delete;
  BlockBitmap &operator=(const BlockBitmap &) = delete;
  ~BlockBitmap();

  void Clear();

  constexpr bool Get(uint32_t index) const {
    const uint32_t bindex = index >> kBlockShift;
    const uint32_t windex = (index & kBlockMask) >> 5;
    const uint32_t bitpos = index & 31;
    return (block_[bindex][windex] >> bitpos) & 1;
  }

  constexpr void Set(uint32_t index) const {
    const uint32_t bindex = index >> kBlockShift;
    const uint32_t windex = (index & kBlockMask) >> 5;
    const uint32_t bitpos = index & 31;
    block_[bindex][windex] |= (static_cast<uint32_t>(1) << bitpos);
  }

  // REQUIRES: "iter" is zero, or was set by a preceding call
  // to GetMutableFragment().
  //
  // This allows caller to peek into and write to the underlying bitmap
  // as a series of non-empty fragments in whole number of 4-byte words.
  // If the entire bitmap has been exhausted, return false.  Otherwise,
  // return true and point caller to the next non-empty fragment.
  //
  // Usage:
  //    char** ptr;
  //    size_t bytes;
  //    for (uint32_t iter = 0; bm.GetMutableFragment(&iter, &ptr, &bytes); )
  //    {
  //      Process(*ptr, bytes);
  //    }
  bool GetMutableFragment(uint32_t *iter, char ***ptr, size_t *size);

 private:
  static constexpr int kBlockShift = 21;  // 2^21 bits == 256KB block
  static constexpr int kBlockBits = 1 << kBlockShift;
  static constexpr int kBlockMask = kBlockBits - 1;
  static constexpr int kBlockBytes = kBlockBits >> 3;
  static constexpr int kBlockWords = kBlockBits >> 5;

  // Array of blocks. Each block has kBlockBits region except for last block.
  uint32_t **block_;
  uint32_t num_blocks_;
  uint32_t bytes_in_last_;
  const bool is_mutable_;
};

constexpr uint32_t BitsToWords(uint32_t bits) {
  uint32_t words = (bits + 31) >> 5;
  if (bits > 0 && words == 0) {
    words = 1 << (32 - 5);  // possible overflow
  }
  return words;
}

}  // namespace internal
}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_EXISTENCE_FILTER_H_
