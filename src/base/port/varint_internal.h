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

#ifndef MOZC_BASE_PORT_VARINT_INTERNAL_H_
#define MOZC_BASE_PORT_VARINT_INTERNAL_H_

#include <cstdint>

namespace mozc {
namespace port {
namespace internal {

// Fallback implementation of Varint for environments where gloop is
// unavailable.
class Varint {
 public:
  // Maximum length of varint encoding of uint32.
  static constexpr int kMax32 = 5;

  // Encodes a 32-bit unsigned integer into ptr.
  // ptr must point to a buffer of length at least kMax32.
  // Returns pointer to the byte just past the last encoded byte.
  static char* Encode32(char* sptr, uint32_t v) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(sptr);
    while (v >= 0x80) {
      *ptr++ = static_cast<uint8_t>((v & 0x7F) | 0x80);
      v >>= 7;
    }
    *ptr++ = static_cast<uint8_t>(v);
    return reinterpret_cast<char*>(ptr);
  }

  // Scans next varint from ptr and stores in output.
  // ptr must point to a buffer of length at least kMax32.
  // Returns pointer just past last read byte, or nullptr on failure.
  static const char* Parse32(const char* sptr, uint32_t* output) {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(sptr);
    uint32_t b = *ptr++;
    if (b < 0x80) {
      *output = b;
      return reinterpret_cast<const char*>(ptr);
    }
    uint32_t result = b & 0x7F;

    b = *ptr++;
    result |= (b & 0x7F) << 7;
    if (b < 0x80) {
      *output = result;
      return reinterpret_cast<const char*>(ptr);
    }

    b = *ptr++;
    result |= (b & 0x7F) << 14;
    if (b < 0x80) {
      *output = result;
      return reinterpret_cast<const char*>(ptr);
    }

    b = *ptr++;
    result |= (b & 0x7F) << 21;
    if (b < 0x80) {
      *output = result;
      return reinterpret_cast<const char*>(ptr);
    }

    b = *ptr++;
    result |= (b & 0x7F) << 28;
    if (b < 0x10) {
      *output = result;
      return reinterpret_cast<const char*>(ptr);
    }

    return nullptr;
  }
};

}  // namespace internal
}  // namespace port
}  // namespace mozc

#endif  // MOZC_BASE_PORT_VARINT_INTERNAL_H_
