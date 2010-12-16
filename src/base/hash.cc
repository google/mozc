// Copyright 2010, Google Inc.
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

#include <string.h>
#include <string>
#include "base/util.h"
#include "base/base.h"

namespace {
static const uint32 kFingerPrint32Seed = 0xfd12deff;
static const uint32 kFingerPrintSeed0 = 0x6d6f;
static const uint32 kFingerPrintSeed1 = 0x7a63;
}

namespace mozc {
#define mix(a,b,c) { \
  a -= b; a -= c; a ^= (c >> 13); \
  b -= c; b -= a; b ^= (a << 8); \
  c -= a; c -= b; c ^= (b >> 13); \
  a -= b; a -= c; a ^= (c >> 12);  \
  b -= c; b -= a; b ^= (a << 16); \
  c -= a; c -= b; c ^= (b >> 5); \
  a -= b; a -= c; a ^= (c >> 3);  \
  b -= c; b -= a; b ^= (a << 10); \
  c -= a; c -= b; c ^= (b >> 15); \
}

uint32 Util::Fingerprint32(const string &key) {
  return Fingerprint32WithSeed(key.data(), key.size(),
                               kFingerPrint32Seed);
}

uint32 Util::Fingerprint32(const char *str, size_t length) {
  return Fingerprint32WithSeed(str, length,
                               kFingerPrint32Seed);
}

uint32 Util::Fingerprint32(const char *str) {
  return Fingerprint32WithSeed(str, strlen(str),
                               kFingerPrint32Seed);
}

uint32 Util::Fingerprint32WithSeed(const string &key,
                                   uint32 seed) {
  return Fingerprint32WithSeed(key.data(), key.size(),
                               seed);
}

uint32 Util::Fingerprint32WithSeed(const char *str,
                                   size_t length,
                                   uint32 seed) {
  uint32 len = static_cast<uint32>(length);
  uint32 a = 0x9e3779b9;
  uint32 b = a;
  uint32 c = seed;

  while (len >= 12) {
    a += (str[0] + ((uint32)str[1] << 8) + ((uint32)str[2] << 16)
          + ((uint32)str[3] << 24));
    b += (str[4] + ((uint32)str[5] << 8) + ((uint32)str[6] << 16)
          + ((uint32)str[7] << 24));
    c += (str[8] + ((uint32)str[9] << 8) + ((uint32)str[10] << 16)
          + ((uint32)str[11] << 24));
    mix(a, b, c);
    str += 12;
    len -= 12;
  }

  c += static_cast<uint32>(length);
  switch (len) {
    case 11: c += ((uint32)str[10] << 24);
    case 10: c += ((uint32)str[9] << 16);
    case 9 : c += ((uint32)str[8] << 8);
    case 8 : b += ((uint32)str[7] << 24);
    case 7 : b += ((uint32)str[6] << 16);
    case 6 : b += ((uint32)str[5] << 8);
    case 5 : b += str[4];
    case 4 : a += ((uint32)str[3] << 24);
    case 3 : a += ((uint32)str[2] << 16);
    case 2 : a += ((uint32)str[1] << 8);
    case 1 : a += str[0];
  }
  mix(a, b, c);

  return c;
}

uint32 Util::Fingerprint32WithSeed(const char *str,
                                   uint32 seed) {
  return Fingerprint32WithSeed(str, strlen(str), seed);
}

uint32 Util::Fingerprint32WithSeed(uint32 num, uint32 seed) {
  const char* str = reinterpret_cast<const char*>(&num);
  return Fingerprint32WithSeed(str, sizeof(num), seed);
}

uint64 Util::Fingerprint(const string &key) {
  return Fingerprint(key.data(), key.size());
}

uint64 Util::Fingerprint(const char *str, size_t length) {
  return FingerprintWithSeed(str, length, kFingerPrintSeed0);
}

uint64 Util::FingerprintWithSeed(const string &str, uint32 seed) {
  return FingerprintWithSeed(str.data(), str.size(), seed);
}

uint64 Util::FingerprintWithSeed(const char *str, size_t length, uint32 seed) {
  const uint32 hi = Fingerprint32WithSeed(str, length, seed);
  const uint32 lo = Fingerprint32WithSeed(str, length, kFingerPrintSeed1);
  uint64 result = static_cast<uint64>(hi) << 32 | static_cast<uint64>(lo);
  if ((hi == 0) && (lo < 2)) {
    result ^= GG_ULONGLONG(0x130f9bef94a0a928);
  }
  return result;
}
}  // namespace mozc
