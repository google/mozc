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

#ifndef MOZC_BASE_HASH_H_
#define MOZC_BASE_HASH_H_

#include <concepts>
#include <cstdint>

#include "absl/hash/internal/city.h"
#include "absl/strings/string_view.h"

namespace mozc {

// New fingerprint functions based on CityHash.
// https://github.com/google/cityhash
// Currently, it uses absl::hash_internal. Since internal implementation may be
// updated or deleted in the future, we have compatibility checks in the unit
// tests. If Abseil switches to a different hash function, we will need to fork
// it ourselves.

inline uint64_t CityFingerprint(absl::string_view str) {
  return ::absl::hash_internal::CityHash64(str.data(), str.size());
}

inline uint64_t CityFingerprintWithSeed(absl::string_view str, uint64_t seed) {
  return ::absl::hash_internal::CityHash64WithSeed(str.data(), str.size(),
                                                   seed);
}

inline uint32_t CityFingerprint32(absl::string_view str) {
  return ::absl::hash_internal::CityHash32(str.data(), str.size());
}

// Legacy Fingerprint Functions
// These are about 5-7 times slower than CityFingerprint.
// Do not use legacy Fingerprint in new code.
// TODO(taku): Rename them with LegacyFingerprintXX.
// TODO(all): Migrate to CityFingerprint except for the case where the
// fingerprint of user-data is stored.

// Calculates 64-bit fingerprint.
uint64_t Fingerprint(absl::string_view str);
uint64_t FingerprintWithSeed(absl::string_view str, uint32_t seed);

// Calculates 32-bit fingerprint.
uint32_t Fingerprint32(absl::string_view str);

}  // namespace mozc

#endif  // MOZC_BASE_HASH_H_
