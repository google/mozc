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

#ifndef MOZC_BASE_UNVERIFIED_AES256_H_
#define MOZC_BASE_UNVERIFIED_AES256_H_

#include <cstddef>
#include <cstdint>

namespace mozc {
namespace internal {

// Note that this implementation is kept just for the backward compatibility
// so that we can read previously obfuscated data.
// !!! Not FIPS-certified.
// !!! Performance optimization is not well considered.
// !!! Side-channel attack is not well considered.
// TODO(team): Consider to remove this class and stop doing obfuscation.
class UnverifiedAES256 {
 public:
  UnverifiedAES256() = delete;
  UnverifiedAES256(const UnverifiedAES256&) = delete;
  UnverifiedAES256& operator=(const UnverifiedAES256&) = delete;

  static constexpr size_t kKeyBytes = 32;    // 256 bit
  static constexpr size_t kBlockBytes = 16;  // 128 bit
  static constexpr size_t kKeyScheduleBytes = 240;

  // Does AES256 CBC transformation.
  // CAVEATS: See the above comment.
  static void TransformCBC(const uint8_t (&key)[kKeyBytes],
                           const uint8_t (&iv)[kBlockBytes], uint8_t* block,
                           size_t block_count);

  // Does AES256 CBC inverse transformation.
  // CAVEATS: See the above comment.
  static void InverseTransformCBC(const uint8_t (&key)[kKeyBytes],
                                  const uint8_t (&iv)[kBlockBytes],
                                  uint8_t* block, size_t block_count);

 protected:
  // Does AES256 ECB transformation.
  // CAVEATS: See the above comment.
  static void TransformECB(const uint8_t (&w)[kKeyScheduleBytes],
                           uint8_t block[kBlockBytes]);

  // Does AES256 ECB inverse transformation.
  // CAVEATS: See the above comment.
  static void InverseTransformECB(const uint8_t (&w)[kKeyScheduleBytes],
                                  uint8_t block[kBlockBytes]);

  // Declared as protected for unit test.
  static void MakeKeySchedule(const uint8_t (&key)[kKeyBytes],
                              uint8_t w[kKeyScheduleBytes]);
  static void SubBytes(uint8_t block[kBlockBytes]);
  static void InvSubBytes(uint8_t block[kBlockBytes]);
  static void MixColumns(uint8_t block[kBlockBytes]);
  static void InvMixColumns(uint8_t block[kBlockBytes]);
  static void ShiftRows(uint8_t block[kBlockBytes]);
  static void InvShiftRows(uint8_t block[kBlockBytes]);
};

}  // namespace internal
}  // namespace mozc

#endif  // MOZC_BASE_UNVERIFIED_AES256_H_
