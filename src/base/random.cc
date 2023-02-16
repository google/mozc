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

#include "base/random.h"

#include <cstddef>
#include <random>
#include <string>
#include <utility>

#include "base/util.h"
#include "absl/algorithm/container.h"
#include "absl/random/random.h"

namespace mozc {

Random::Random(std::seed_seq &&seed)
    : bitgen_(std::forward<std::seed_seq>(seed)) {}

Random::Random(absl::BitGen &&gen) : bitgen_(std::move(gen)) {}

std::string Random::Utf8String(size_t len, char32_t lo, char32_t hi) {
  std::string result;
  result.reserve(len);
  for (size_t i = 0; i < len; ++i) {
    Util::Ucs4ToUtf8Append(absl::Uniform(absl::IntervalClosed, bitgen_, lo, hi),
                           &result);
  }
  return result;
}

std::string Random::Utf8StringRandomLen(size_t len_max, char32_t lo,
                                        char32_t hi) {
  return Utf8String(absl::Uniform(absl::IntervalClosed, bitgen_, 1u, len_max),
                    lo, hi);
}

std::string Random::ByteString(size_t size) {
  std::string result;
  result.resize(size);
  absl::c_generate(
      result, [&]() -> char { return absl::Uniform<unsigned char>(bitgen_); });
  return result;
}

}  // namespace mozc
