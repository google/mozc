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

#include "base/container/flat_set.h"

#include <functional>

#include "absl/strings/string_view.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

TEST(FlatSetTest, Contains) {
  constexpr auto kSet = CreateFlatSet<absl::string_view>({
      "one",
      "three",
      "five",
  });

  EXPECT_FALSE(kSet.contains("zero"));
  EXPECT_TRUE(kSet.contains("one"));
  EXPECT_FALSE(kSet.contains("two"));
  EXPECT_TRUE(kSet.contains("three"));
  EXPECT_FALSE(kSet.contains("four"));
  EXPECT_TRUE(kSet.contains("five"));
  EXPECT_FALSE(kSet.contains("six"));
}

TEST(FlatSetTest, CustomCompare) {
  constexpr auto kSet = CreateFlatSet<absl::string_view, std::greater<>>({
      "one",
      "three",
      "five",
  });

  EXPECT_FALSE(kSet.contains("zero"));
  EXPECT_TRUE(kSet.contains("one"));
  EXPECT_FALSE(kSet.contains("two"));
  EXPECT_TRUE(kSet.contains("three"));
  EXPECT_FALSE(kSet.contains("four"));
  EXPECT_TRUE(kSet.contains("five"));
  EXPECT_FALSE(kSet.contains("six"));
}

TEST(FlatSetTest, SingleElement) {
  // The uniqueness verification used to read past the end of the array for a
  // single-element set, which failed the constant evaluation.
  constexpr auto kSet = CreateFlatSet<int>({42});

  EXPECT_TRUE(kSet.contains(42));
  EXPECT_FALSE(kSet.contains(0));
}

#if defined(EXPECT_DEATH)
TEST(FlatSetDeathTest, DuplicateEntries) {
  // Runtime construction with duplicate entries hits LOG(FATAL). Compile-time
  // construction with duplicate entries fails the build instead.
  EXPECT_DEATH(CreateFlatSet<int>({1, 1, 2, 3, 4}), "Duplicate entry found");
  // The uniqueness verification used to stop at the middle of the sorted
  // array, missing duplicates in the second half.
  EXPECT_DEATH(CreateFlatSet<int>({1, 2, 3, 4, 4}), "Duplicate entry found");
}
#endif  // defined(EXPECT_DEATH)

}  // namespace
}  // namespace mozc
