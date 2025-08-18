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

#ifndef MOZC_BASE_RANDOM_H_
#define MOZC_BASE_RANDOM_H_

#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>

#include "absl/random/random.h"

namespace mozc {

// Random is a utility class to generate random sequences.
// This class can work as an absl UBRG and be passed to absl distribution
// functions (absl::Uniform, absl::Bernoulli, etc) so you don't need a separate
// absl::BitGen.
class Random {
 public:
  using result_type = absl::BitGen::result_type;

  Random() = default;
  // Construct using an existing BitGen, std::seed_eq, or any other value that
  // absl::BitGen can construct from.
  template <typename Rng>
    requires(!std::same_as<std::remove_cvref_t<Rng>, Random>)
  explicit Random(Rng&& rng) : bitgen_(std::forward<Rng>(rng)) {}

  // Disallow copy, allow move.
  Random(const Random&) = delete;
  Random& operator=(const Random&) = delete;
  Random(Random&&) = default;
  Random& operator=(Random&&) = default;

  // Pass through the underlying absl::BitGen UBRG attributes.
  static constexpr result_type min() { return absl::BitGen::min(); }
  static constexpr result_type max() { return absl::BitGen::max(); }
  result_type operator()() { return bitgen_(); }

  // Generates a random valid utf-8 sequence with len codepoints [lo, hi].
  std::string Utf8String(size_t len, char32_t lo, char32_t hi);
  // Generates a random valid utf-8 sequence with [1, len_max] codepoints [lo,
  // hi].
  std::string Utf8StringRandomLen(size_t len_max, char32_t lo, char32_t hi);
  // Generates a random binary ([0, 0xff]) string of size bytes.
  // Note that NUL can be in the middle of the sequence.
  // Use Utf8String for valid random string generations.
  std::string ByteString(size_t size);

 private:
  absl::BitGen bitgen_;
};

}  // namespace mozc

#endif  // MOZC_BASE_RANDOM_H_
