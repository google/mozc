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

#ifndef MOZC_BASE_BITARRAY_H_
#define MOZC_BASE_BITARRAY_H_

#include <string.h>   // memset
#include "base/port.h"

namespace mozc {

class BitArray {
 public:
  // Specify the size of bit vector
  explicit BitArray(uint32 size)
      : array_(new uint32[1 + (size >> 5)]), size_(size) {
    memset(reinterpret_cast<char *>(array_), 0, 4 * (1 + (size >> 5)));
  }

  ~BitArray() {
    delete [] array_;
    array_ = NULL;
  }

  // get true/false of |index|
  inline bool get(uint32 index) const {
    return static_cast<bool>(
        (array_[(index >> 5)] >> (index & 0x0000001F)) & 0x00000001);
  }

  // set |index| as true
  inline void set(uint32 index) {
    array_[(index >> 5)] |= 0x00000001 << (index & 0x0000001F);
  }

  // set |index| as false
  inline void clear(uint32 index) {
    array_[(index >> 5)] &= ~(0x00000001 << (index & 0x0000001F));
  }

  // return the body of bit vector
  const char *array() const {
    return reinterpret_cast<const char *>(array_);
  }

  // return the required buffer size for saving the bit vector
  size_t array_size() const {
    return 4 * (1 + (size_ >> 5));
  }

  // return number of bit(s)
  size_t size() const {
    return size_;
  }

  // immutable acessor
  static bool GetValue(const char *array, uint32 index) {
    const uint32 *uarray = reinterpret_cast<const uint32 *>(array);
    return static_cast<bool>(
        (uarray[(index >> 5)] >> (index & 0x0000001F)) & 0x00000001);
  }

 private:
  uint32 *array_;
  size_t size_;
};
}  // namespace mozc
#endif  // MOZC_BITARRAY_H_
