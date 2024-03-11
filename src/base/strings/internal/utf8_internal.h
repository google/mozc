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

#ifndef MOZC_BASE_STRINGS_INTERNAL_UTF8_INTERNAL_H_
#define MOZC_BASE_STRINGS_INTERNAL_UTF8_INTERNAL_H_

#include <array>
#include <cstdint>

namespace mozc::utf8_internal {

inline constexpr int kMaxByteSize = 4;
inline constexpr int kCharsInByte = 256;
inline constexpr char32_t kReplacementCharacter = 0xfffd;

// Table of UTF-8 character lengths, based on the first byte.
// Values for trailing bytes are set to 1 to continue processing at the next
// byte.
inline constexpr std::array<uint_fast8_t, 256> kUtf8LenTbl = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x00-0x0f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x10-0x1f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x20-0x2f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x30-0x3f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x40-0x4f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x50-0x5f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x60-0x6f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x70-0x7f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x80-0x8f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x90-0x9f
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0xa0-0xaf
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0xb0-0xbf
    // C0, C1 are disallowed in UTF-8.
    1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // 0xc0-0xcf
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // 0xd0-0xdf
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // 0xe0-0xef
    // F5-FF are disallowed in UTF-8.
    4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0xf0-0xff
};

class EncodeResult {
 public:
  using Buffer = std::array<char, kMaxByteSize>;

  constexpr const char* data() const { return bytes_.data(); }
  constexpr uint_fast8_t size() const { return count_; }

 private:
  friend EncodeResult Encode(char32_t);

  uint_fast8_t count_;
  Buffer bytes_;
};

class DecodeResult {
 public:
  DecodeResult() = default;

  static constexpr DecodeResult Continue(const char32_t cp,
                                         const uint_fast8_t bytes_seen) {
    return DecodeResult{cp, true, bytes_seen};
  }

  static inline DecodeResult Error(const uint_fast8_t bytes_seen) {
    return DecodeResult{kReplacementCharacter, false, bytes_seen};
  }

  // Indicates that the decoded position is the `end` sentinel.
  static inline DecodeResult Sentinel() { return DecodeResult{0, false, 0}; }
  bool IsSentinel() const { return bytes_seen_ == 0; }

  constexpr char32_t code_point() const { return code_point_; }
  constexpr bool ok() const { return ok_; }
  constexpr uint_fast8_t bytes_seen() const { return bytes_seen_; }

 private:
  constexpr DecodeResult(const char32_t cp, const bool ok,
                         const uint_fast8_t bytes_seen)
      : code_point_(cp), ok_(ok), bytes_seen_(bytes_seen) {}

  char32_t code_point_;  // Decoded code point. kReplacementCharacter if error.
  bool ok_;              // True if the UTF-8 string is valid.
  uint_fast8_t bytes_seen_;  // Number of processed bytes. 0-4 bytes.
};

// Returns the byte length of a single UTF-8 character based on the leading
// byte.
constexpr uint_fast8_t OneCharLen(const char c) {
  return utf8_internal::kUtf8LenTbl[static_cast<uint_fast8_t>(c)];
}

// Encodes the Unicode code point cp in UTF-8 and returns the encoder itself.
// If cp is not a valid code point in Unicode, it replaces it with U+FFFD.
EncodeResult Encode(char32_t cp);

// Decodes a single UTF-8 character and returns the result.
// Returns `DecodeResult::Sentinel()` if the given range is empty.
// REQUIRES: [it, last) to be a valid range.
DecodeResult Decode(const char* ptr, const char* last);

// Implementations

}  // namespace mozc::utf8_internal

#endif  // MOZC_BASE_STRINGS_INTERNAL_UTF8_INTERNAL_H_
