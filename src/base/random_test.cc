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
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "base/util.h"
#include "testing/gunit.h"
#include "absl/algorithm/container.h"
#include "absl/random/random.h"

namespace mozc {
namespace {

template <typename C>
bool ContainsSameValues(const C& buf) {
  const auto first = buf.front();
  return absl::c_all_of(buf,
                        [first](const auto v) -> bool { return v == first; });
}

TEST(TestRandom, Seed) {
  {
    Random r0(std::seed_seq{}), r1(std::seed_seq{});
    EXPECT_EQ(r0.Utf8String(100, 0, 256), r1.Utf8String(100, 0, 256));
  }

  {
    Random r0(absl::BitGen(std::seed_seq{})), r1(absl::BitGen(std::seed_seq{}));
    EXPECT_EQ(r0.ByteString(100), r1.ByteString(100));
  }
}

TEST(TestRandom, URBG) {
  Random r;
  constexpr size_t size = 1024;
  std::vector<uint32_t> buf(size);
  absl::c_generate(buf,
                   [&r]() -> uint32_t { return absl::Uniform<uint32_t>(r); });
  EXPECT_FALSE(ContainsSameValues(buf));
}

TEST(TestRandom, Utf8String) {
  Random r;

  EXPECT_TRUE(r.Utf8String(0, 0, 0).empty());

  // Sufficiently large so it's highly unlikely to have only one value.
  constexpr size_t len = 1024;
  constexpr char32_t lo = 0x1000, hi = 0x7000;
  const std::string s = r.Utf8String(len, lo, hi);
  EXPECT_TRUE(Util::IsValidUtf8(s));
  EXPECT_EQ(Util::CharsLen(s), len);

  const std::u32string codepoints = Util::Utf8ToUtf32(s);
  EXPECT_FALSE(ContainsSameValues(codepoints));
  EXPECT_TRUE(absl::c_all_of(
      codepoints, [](char32_t c) -> bool { return lo <= c && c <= hi; }));
}

TEST(TestRandom, Utf8StringRandomLen) {
  Random r;

  constexpr char32_t lo = 0x1000, hi = 0x7000;
  constexpr size_t kLengths[] = {1, 2, 100};
  for (const size_t len : kLengths) {
    const std::string s = r.Utf8StringRandomLen(len, lo, hi);
    EXPECT_TRUE(Util::IsValidUtf8(s));
    const size_t out_len = Util::CharsLen(s);
    EXPECT_GE(out_len, 1);
    EXPECT_LE(out_len, len);
  }

  constexpr int kCount = 100;
  constexpr size_t kLenMax = 256;
  std::vector<size_t> result_sizes(kCount);
  for (int i = 0; i < kCount; ++i) {
    const std::string s = r.Utf8StringRandomLen(kLenMax, lo, hi);
    EXPECT_TRUE(Util::IsValidUtf8(s));
    const size_t len = Util::CharsLen(s);
    EXPECT_GE(len, 1);
    EXPECT_LE(len, kLenMax);
    result_sizes[i] = len;
  }
  EXPECT_TRUE(absl::c_any_of(result_sizes,
                             [](size_t len) -> bool { return len < kLenMax; }));
}

TEST(TestRandom, ByteString) {
  Random r;

  EXPECT_TRUE(r.ByteString(0).empty());

  constexpr size_t size = 1024;
  const std::string s = r.ByteString(size);
  EXPECT_EQ(s.size(), size);
  EXPECT_FALSE(ContainsSameValues(s));
}

}  // namespace
}  // namespace mozc
