// Copyright 2010-2011, Google Inc.
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

#include "base/base.h"

namespace mozc {

// Bloom filter
class ExistenceFilter {
  class BlockBitmap;

 public:

  struct Header {
    uint32 m;
    uint32 n;
    int k;
  };

  // 'm' is the number of bits in the bit vector
  // 'n' is the number of values that will be stored
  // 'k' is the number of hash values to use per insert/lookup
  // k must be less than 8
  ExistenceFilter(uint32 m, uint32 n, int k);

  static ExistenceFilter* CreateOptimal(size_t size_in_bytes,
                                        uint32 estimated_insertions);

  ~ExistenceFilter();

  void Clear();

  // Inserts a hash value into the filter
  // We generate 'k' separate internal hash values
  void Insert(uint64 hash);

  // Checks if the given 'hash' was previously inserted int the filter
  // It may return some false positives
  bool Exists(uint64 hash) const;

  // Returns the size (in bytes) of the bloom filter
  size_t Size() const;

  // Returns the minimum required size of the filter in bytes
  // under the given error rate and number of elements
  static size_t MinFilterSizeInBytesForErrorRate(float error_rate,
                                                 size_t num_elements);

  void Write(char **buf, size_t *size);

  static bool ReadHeader(const char *buf, Header* header);

  // Read Existence filter from buf[]
  // Note that the returned ExsitenceFilter is immutable filter.
  // Any mutable operations will destroy buf[].
  static ExistenceFilter* Read(const char *buf, size_t size);

 private:
  // private constructor for ExistenceFilter::Read();
  ExistenceFilter(uint32 m, uint32 n, int k, bool is_mutable);

  static ExistenceFilter *CreateImmutableExietenceFilter(uint32 m,
                                                         uint32 n,
                                                         int k);

  // Rotate the value in 'original' by 'num_bits'
  static uint64 RotateLeft64(uint64 original, int num_bits);

  static inline uint32 BitsToWords(uint32 bits) {
    uint32 words = (bits + 31) >> 5;
    if (bits > 0 && words == 0) {
      words = 1 << (32 - 5);  // possible overflow
    }
    return words;
  }

  scoped_ptr<BlockBitmap> rep_;  // points to bitmap
  const uint32 vec_size_;  // size of bitmap (in bits)
  const bool is_power_of_two_;  // true if vec_size_ is a power of two
  const uint32 expected_nelts_;  // expected number of inserts
  const int32 num_hashes_;  // number of hashes per lookup
};
}  // namespace mozc

#endif  // MOZC_STORAGE_EXISTENCE_FILTER_H_
