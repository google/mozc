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

#include "base/unverified_sha1.h"

#include <algorithm>
#include <climits>  // for CHAR_BIT
#include <cstdint>
#include <type_traits>

#include "base/logging.h"

namespace mozc {
namespace internal {
namespace {

const size_t kNumDWordsOfDigest = 5;

// See 4.1.1 SHA-1 Functions
// http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf
uint32_t f(uint32_t t, uint32_t x, uint32_t y, uint32_t z) {
  if (t < 20) {
    // Note: The logic here was originally defined as
    //   return (x & y) | ((~x) & z);
    // in FIPS 180-1 but revised as follows in FIPS 180-2.
    return (x & y) ^ ((~x) & z);
  } else if (t < 40) {
    return x ^ y ^ z;
  } else if (t < 60) {
    // Note: The logic here was originally defined as
    //   return (x & y) | (x & z) | (y & z);
    // in FIPS 180-1 but revised as follows in FIPS 180-2.
    return (x & y) ^ (x & z) ^ (y & z);
  } else {
    return x ^ y ^ z;
  }
}

// See 3.2 Operations on Words
// http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf
template <size_t N>
uint32_t ROTL(uint32_t x) {
  const size_t kUint32Bits = sizeof(uint32_t) * CHAR_BIT;
  static_assert(N < kUint32Bits, "Too large rotation sise.");
  return (x << N) | (x >> (kUint32Bits - N));
}

// See 4.2.1 SHA-1 Constants
// http://csrc.nist.gov/publications/fips/fips180-4/fips-180-4.pdf
uint32_t K(uint32_t t) {
  if (t < 20) {
    return 0x5a827999;
  } else if (t < 40) {
    return 0x6ed9eba1;
  } else if (t < 60) {
    return 0x8f1bbcdc;
  } else {
    return 0xca62c1d6;
  }
}

std::string AsByteStream(const uint32_t (&H)[kNumDWordsOfDigest]) {
  std::string str;
  str.resize(sizeof(H));
  for (size_t i = 0; i < kNumDWordsOfDigest; ++i) {
    const size_t base_index = i * sizeof(uint32_t);
    const uint32_t value = H[i];
    // Note that the following conversion is required simply because SHA1
    // algorithm is defined on big-endian. The following conversion is
    // purely arithmetic thus should be applicable regardless of the
    // endianness of the target processor.
    str[base_index + 0] = static_cast<uint8_t>((value & 0xff000000) >> 24);
    str[base_index + 1] = static_cast<uint8_t>((value & 0x00ff0000) >> 16);
    str[base_index + 2] = static_cast<uint8_t>((value & 0x0000ff00) >> 8);
    str[base_index + 3] = static_cast<uint8_t>((value & 0x000000ff) >> 0);
  }
  return str;
}

// Implements 5.1 Padding the Message / 5.1.1 SHA-1, SHA-224 and SHA-256
class PaddedMessageIterator {
 public:
  // SHA1 uses 64-byte (512-bit) message block.
  static constexpr size_t kMessageBlockBytes = 64;
  // The original data length in bit is stored as 8-byte-length data.
  static constexpr size_t kDataBitLengthBytes = sizeof(uint64_t);

  explicit PaddedMessageIterator(absl::string_view source)
      : source_(source),
        num_total_message_(GetTotalMessageCount(source.size())),
        message_index_(0) {}

  bool HasMessage() const { return message_index_ < num_total_message_; }

  void FillNextMessage(
      absl::string_view::value_type dest[kMessageBlockBytes]) const {
    // 5.1.1 SHA-1, SHA-224 and SHA-256
    static_assert(CHAR_BIT == 8, "Assuming 1 byte == 8 bit");

    const size_t base_index = message_index_ * kMessageBlockBytes;
    size_t cursor = 0;
    if (base_index < source_.size()) {
      const size_t rest = source_.size() - base_index;
      if (rest >= kMessageBlockBytes) {
        source_.copy(dest, kMessageBlockBytes, base_index);
        return;
      }
      DCHECK_GT(kMessageBlockBytes, rest);
      source_.copy(dest, rest, base_index);
      cursor = rest;
    }

    // Put the end-of-data marker.
    if ((base_index + cursor) == source_.size()) {
      const uint8_t kEndOfDataMarker = 0x80;
      dest[cursor] = kEndOfDataMarker;
      ++cursor;
    }

    // Hereafter, we will fill the original data length (excluding the padding
    // data) in bit as 8-byte block at the end of the last message block.
    // Until then, all the byte will be filled with 0x00.

    const size_t kMessageBlockZeroFillLimit =
        kMessageBlockBytes - kDataBitLengthBytes;

    if (cursor > kMessageBlockZeroFillLimit) {
      // The current message block does not have enough room to store 8-byte
      // length data. The data length will be stored into the next message.
      // Until then, fill 0x00.
      memset(dest + cursor, 0x00, kMessageBlockBytes - cursor);
      return;
    }

    // Fill 0x00 for padding.
    memset(dest + cursor, 0x00, kMessageBlockZeroFillLimit - cursor);

    // Store the original data bit-length into the last 8-byte of this message.
    const uint64_t bit_length = source_.size() * 8;
    for (size_t i = 0; i < 8; ++i) {
      // Big-endian is required.
      const size_t shift = (7 - i) * 8;
      const size_t pos = kMessageBlockZeroFillLimit + i;
      DCHECK_LT(pos, kMessageBlockBytes);
      dest[pos] = static_cast<uint8_t>((bit_length >> shift) & 0xff);
    }
  }

  void MoveNext() { ++message_index_; }

 private:
  // Returns the total message count.
  static size_t GetTotalMessageCount(size_t original_message_size) {
    // In short, the total data size is always larger than the original data
    // size because of:
    // - 1 byte marker for end-of-data.
    // - (if any) 0x00 byte sequence to pad each message as 64-byte.
    // - 8 byte integer to store the original data size in bit.
    // At minimum, we need additional 9 bytes.
    const size_t kEndOfDataMarkerBytes = 1;
    const size_t minimum_size =
        original_message_size + kEndOfDataMarkerBytes + kDataBitLengthBytes;
    // TODO(yukawa): Fix subtle overflow case.
    return (minimum_size + kMessageBlockBytes - 1) / kMessageBlockBytes;
  }

  const absl::string_view source_;
  const size_t num_total_message_;
  size_t message_index_;
};

// Converts a character to uint32 bit pattern by prepending 0's while keeping
// the original bit pattern in lowest 8 bits.
uint32_t CharToUint32(char c) {
  // In case char is signed, we need to first convert c to uint8; see the
  // following example:
  //
  // signed char c = -1;
  // uint32_t a = static_cast<uint32_t>(c);
  // uint32_t b = static_cast<uint32_t>(static_cast<uint8_t>(c));
  //
  // Result:
  //   a == 4294967295  (converted through -1 of 32-bit integer)
  //   b == 255
  return static_cast<uint32_t>(static_cast<uint8_t>(c));
}

std::string MakeDigestImpl(absl::string_view source) {
  // 5.3 Setting the Initial Hash Value / 5.3.1 SHA-1

  // 6.1.1 SHA-1 Preprocessing
  uint32_t H[kNumDWordsOfDigest] = {
      0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0,
  };

  // 6.1.2 SHA-1 Hash Computation
  for (PaddedMessageIterator it(source); it.HasMessage(); it.MoveNext()) {
    absl::string_view::value_type message[64];
    it.FillNextMessage(message);

    uint32_t W[80];  // Message schedule.
    for (size_t i = 0; i < 16; ++i) {
      const size_t base_index = i * 4;
      W[i] = (CharToUint32(message[base_index + 0]) << 24) |
             (CharToUint32(message[base_index + 1]) << 16) |
             (CharToUint32(message[base_index + 2]) << 8) |
             (CharToUint32(message[base_index + 3]) << 0);
    }
    for (size_t t = 16; t < 80; ++t) {
      W[t] = ROTL<1>(W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16]);
    }

    uint32_t a = H[0];
    uint32_t b = H[1];
    uint32_t c = H[2];
    uint32_t d = H[3];
    uint32_t e = H[4];

    for (size_t t = 0; t < 80; ++t) {
      const uint32_t T = ROTL<5>(a) + f(t, b, c, d) + e + W[t] + K(t);
      e = d;
      d = c;
      c = ROTL<30>(b);
      b = a;
      a = T;
    }

    H[0] += a;
    H[1] += b;
    H[2] += c;
    H[3] += d;
    H[4] += e;
  }

  return AsByteStream(H);
}

}  // namespace

std::string UnverifiedSHA1::MakeDigest(absl::string_view source) {
  return MakeDigestImpl(source);
}

}  // namespace internal
}  // namespace mozc
