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

#include "base/hash.h"

#include <cstdint>
#include <limits>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

constexpr uint32_t kFingerPrint32Seed = 0xfd12deff;
constexpr uint32_t kFingerPrintSeed0 = 0x6d6f;
constexpr uint32_t kFingerPrintSeed1 = 0x7a63;

uint32_t ToUint32(char a, char b, char c, char d) {
  return uint32_t{a} + (uint32_t{b} << 8) + (uint32_t{c} << 16) +
         (uint32_t{d} << 24);
}

template <class T>
void Mix(T& a, T& b, T& c) {
  a -= b;
  a -= c;
  a ^= (c >> 13);
  b -= c;
  b -= a;
  b ^= (a << 8);
  c -= a;
  c -= b;
  c ^= (b >> 13);
  a -= b;
  a -= c;
  a ^= (c >> 12);
  b -= c;
  b -= a;
  b ^= (a << 16);
  c -= a;
  c -= b;
  c ^= (b >> 5);
  a -= b;
  a -= c;
  a ^= (c >> 3);
  b -= c;
  b -= a;
  b ^= (a << 10);
  c -= a;
  c -= b;
  c ^= (b >> 15);
}

uint32_t Fingerprint32WithSeed(absl::string_view str, uint32_t seed) {
  DCHECK_LE(str.size(), std::numeric_limits<uint32_t>::max());
  const uint32_t str_len = static_cast<uint32_t>(str.size());
  uint32_t a = 0x9e3779b9;
  uint32_t b = a;
  uint32_t c = seed;

  while (str.size() >= 12) {
    a += ToUint32(str[0], str[1], str[2], str[3]);
    b += ToUint32(str[4], str[5], str[6], str[7]);
    c += ToUint32(str[8], str[9], str[10], str[11]);
    Mix(a, b, c);
    str.remove_prefix(12);
  }

  c += str_len;
  switch (str.size()) {
    case 11:
      c += uint32_t{str[10]} << 24;
      [[fallthrough]];
    case 10:
      c += uint32_t{str[9]} << 16;
      [[fallthrough]];
    case 9:
      c += uint32_t{str[8]} << 8;
      [[fallthrough]];
    case 8:
      b += uint32_t{str[7]} << 24;
      [[fallthrough]];
    case 7:
      b += uint32_t{str[6]} << 16;
      [[fallthrough]];
    case 6:
      b += uint32_t{str[5]} << 8;
      [[fallthrough]];
    case 5:
      b += uint32_t{str[4]};
      [[fallthrough]];
    case 4:
      a += uint32_t{str[3]} << 24;
      [[fallthrough]];
    case 3:
      a += uint32_t{str[2]} << 16;
      [[fallthrough]];
    case 2:
      a += uint32_t{str[1]} << 8;
      [[fallthrough]];
    case 1:
      a += uint32_t{str[0]};
      break;
  }
  Mix(a, b, c);

  return c;
}
}  // namespace

uint32_t Fingerprint32(absl::string_view str) {
  return Fingerprint32WithSeed(str, kFingerPrint32Seed);
}

uint64_t Fingerprint(absl::string_view str) {
  return FingerprintWithSeed(str, kFingerPrintSeed0);
}

uint64_t FingerprintWithSeed(absl::string_view str, uint32_t seed) {
  const uint32_t hi = Fingerprint32WithSeed(str, seed);
  const uint32_t lo = Fingerprint32WithSeed(str, kFingerPrintSeed1);
  uint64_t result = static_cast<uint64_t>(hi) << 32 | static_cast<uint64_t>(lo);
  if ((hi == 0) && (lo < 2)) {
    result ^= 0x130f9bef94a0a928uLL;
  }
  return result;
}

}  // namespace mozc
