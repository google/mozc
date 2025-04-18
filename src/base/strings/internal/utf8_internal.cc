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

#include "base/strings/internal/utf8_internal.h"

#include <array>
#include <cstdint>
#include <iterator>
#include <type_traits>

namespace mozc::utf8_internal {
namespace {

static_assert(std::is_unsigned_v<char>);

constexpr int kShift = 6;
constexpr char kTrailingMask = (1 << kShift) - 1;

struct ByteBoundary {
  char min;
  char max;
};

constexpr ByteBoundary GenerateSecondByteBoundary(int n) {
  // From Unicode 15.0 §3.9 Table 3-7: Well-Formed UTF-8 Byte Sequences
  if (n < 0xc2 || 0xf4 < n) {
    // Not a valid leading byte.
    return {0xff, 0x00};
  } else if (n == 0xe0) {
    return {0xa0, 0xbf};
  } else if (n == 0xed) {
    return {0x80, 0x9f};
  } else if (n == 0xf0) {
    return {0x90, 0xbf};
  } else if (n == 0xf4) {
    return {0x80, 0x8f};
  } else {
    // C2..DF, E1..EC, EE..EF, F1..F3
    return {0x80, 0xbf};
  }
}

constexpr std::array<ByteBoundary, kCharsInByte> GenerateSecondByteBoundary() {
  std::array<ByteBoundary, kCharsInByte> result = {};
  for (int i = 0; i < kCharsInByte; ++i) {
    result[i] = GenerateSecondByteBoundary(i);
  }
  return result;
}

constexpr std::array<ByteBoundary, kCharsInByte> kSecondByteBoundaries =
    GenerateSecondByteBoundary();

// IsUtf8TrailingByte returns true if the given UTF-8 codeunit is a trailing
// byte. Use IsValidSecondByte for the second byte as.
constexpr bool IsTrailingByte(char c) { return (c & 0xc0) == 0x80; }

// Checks if the second byte is valid based on the leading byte.
// Pass the total bytes needed (including the leading byte) if known at compile
// time.
template <int Needed = -1>
constexpr bool IsValidSecondByte(const char leading_byte,
                                 const char second_byte) {
  const ByteBoundary boundary = kSecondByteBoundaries[leading_byte];
  return boundary.min <= second_byte && second_byte <= boundary.max;
}

// Specialization for Needed = 2.
template <>
constexpr bool IsValidSecondByte<2>(const char leading_byte,
                                    const char second_byte) {
  return IsTrailingByte(second_byte);
}

DecodeResult HandleBufferTooShort(const char* ptr, const char* last,
                                  const int needed) {
  // If the buffer is not long enough, stop processing and return error.
  // The Unicode Standard requires implementations to check each byte
  // regardless, so we're just performing the check here.
  const char leading_byte = *ptr++;
  if (ptr == last || !IsValidSecondByte(leading_byte, *ptr)) {
    return DecodeResult::Error(1);
  }
  int seen = 2;
  while (++ptr != last) {
    if (!IsTrailingByte(*ptr)) {
      return DecodeResult::Error(seen);
    }
    ++seen;
  }

  return DecodeResult::Error(seen);
}

inline char32_t AppendTrailingByte(char32_t base, char byte) {
  return (base << kShift) + (byte & kTrailingMask);
}

template <int Needed>
DecodeResult DecodeSequence(const char* ptr, const char mask) {
  // By using a template parameter, we force the compiler to check the value for
  // Needed and optimize each case at compile time.
  static_assert(1 < Needed && Needed <= kMaxByteSize);

  const char leading_byte = *ptr++;
  // Handle the leading byte.
  char32_t base = leading_byte & mask;
  // Decode the second byte.
  if (!IsValidSecondByte<Needed>(leading_byte, *ptr)) {
    return DecodeResult::Error(1);
  }
  base = AppendTrailingByte(base, *ptr++);
  for (int i = 2; i < Needed; ++i) {
    // Third and fourth bytes are always within [0x80, 0xbf].
    if (!IsTrailingByte(*ptr)) {
      return DecodeResult::Error(i);
    }
    base = AppendTrailingByte(base, *ptr++);
  }
  return DecodeResult::Continue(base, Needed);
}

}  // namespace

EncodeResult EncodeResult::Ascii(const char32_t cp) {
  EncodeResult result;
  result.count_ = 1;
  result.bytes_[0] = static_cast<char>(cp);
  return result;
}

EncodeResult EncodeResult::EncodeSequence(char32_t cp, uint_fast8_t count,
                                          char offset) {
  EncodeResult result;
  result.count_ = count + 1;  // count_ in the result is the byte length.
  auto it = result.bytes_.begin();
  *it++ = static_cast<char>((cp >> (kShift * count)) + offset);
  while (count > 0) {
    const char temp = static_cast<char>(cp >> (kShift * (count - 1)));
    *it++ = 0x80 | (temp & 0x3f);
    --count;
  }
  return result;
}

EncodeResult Encode(const char32_t cp) {
  // This is a naive UTF-8 encoder based on the WHATWG Encoding standard.
  // https://encoding.spec.whatwg.org/#utf-8-encoder
  if (cp <= 0x7f) {
    return EncodeResult::Ascii(cp);
  } else if (cp <= 0x7ff) {
    return EncodeResult::EncodeSequence(cp, 1, 0xc0);
  } else if (cp <= 0xffff) {
    return EncodeResult::EncodeSequence(cp, 2, 0xe0);
  } else if (cp <= 0x10ffff) {
    return EncodeResult::EncodeSequence(cp, 3, 0xf0);
  } else {
    // Unicode 15.0 §3.4 Characters and Encoding D9
    // "Unicode codespace: A range of integers from 0 to 0x10FFFF."
    // §3.9 UTF-32 D90
    // "Any UTF-32 code unit greater than 0010FFFF16 is ill-formed."
    return Encode(kReplacementCharacter);
  }
}

DecodeResult Decode(const char* ptr, const char* last) {
  if (ptr == last) {
    return DecodeResult::Sentinel();
  }
  // https://encoding.spec.whatwg.org/#utf-8-decoder
  // Note that bytes needed and bytes seen include the leading byte here.
  if (*ptr < 0x80) {
    // Fast path for ASCII.
    return DecodeResult::Continue(*ptr, 1);
  }
  const int needed = OneCharLen(*ptr);
  if (std::distance(ptr, last) < needed) [[unlikely]] {
    return HandleBufferTooShort(ptr, last, needed);
  }
  if (needed == 3) [[likely]] {
    // The overwhelming majority of UTF-8 characters in Mozc are three bytes
    // long. All full-width romaji alphabets, half- and full-width hiragana,
    // and katakana letters are three bytes. Almost all kanji are three bytes,
    // too, except additional characters defined in JIS X 0213:2004, but they're
    // extremely rare. Using [[likely]] reduces cpu time by about 2%-2.5%
    // with clang (as of June 2023). Practically, it puts this branch in the
    // first non-taken branch and fully unrolls the loops inside. It
    // additionally stops unrolling the other branches so it helps reduce the
    // code size.
    return DecodeSequence<3>(ptr, 0x0f);
  } else if (needed == 2) {
    return DecodeSequence<2>(ptr, 0x1f);
  } else if (needed == 4) {
    return DecodeSequence<4>(ptr, 0x07);
  } else {
    // Trailing and disallowed bytes fall here.
    return DecodeResult::Error(1);
  }
}

}  // namespace mozc::utf8_internal
