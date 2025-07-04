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

#include "base/container/flat_multimap.h"

#include <functional>

#include "absl/strings/string_view.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(FlatMultimapTest, EqualSpan) {
  constexpr auto kMultimap = CreateFlatMultimap<int, absl::string_view>({
      {1, "one"},
      {3, "three"},
      {5, "five"},
      {1, "ichi"},
      {3, "san"},
      {5, "go"},
  });

  EXPECT_THAT(kMultimap.EqualSpan(0), IsEmpty());
  EXPECT_THAT(
      kMultimap.EqualSpan(1),
      UnorderedElementsAre(Pair(Eq(1), Eq("one")), Pair(Eq(1), Eq("ichi"))));
  EXPECT_THAT(kMultimap.EqualSpan(2), IsEmpty());
  EXPECT_THAT(
      kMultimap.EqualSpan(3),
      UnorderedElementsAre(Pair(Eq(3), Eq("three")), Pair(Eq(3), Eq("san"))));
  EXPECT_THAT(kMultimap.EqualSpan(4), IsEmpty());
  EXPECT_THAT(
      kMultimap.EqualSpan(5),
      UnorderedElementsAre(Pair(Eq(5), Eq("five")), Pair(Eq(5), Eq("go"))));
  EXPECT_THAT(kMultimap.EqualSpan(6), IsEmpty());
}

TEST(FlatMultimapTest, CustomCompare) {
  constexpr auto kMultimap =
      CreateFlatMultimap<int, absl::string_view, std::greater<>>({
          {1, "one"},
          {3, "three"},
          {5, "five"},
          {1, "ichi"},
          {3, "san"},
          {5, "go"},
      });

  EXPECT_THAT(kMultimap.EqualSpan(0), IsEmpty());
  EXPECT_THAT(
      kMultimap.EqualSpan(1),
      UnorderedElementsAre(Pair(Eq(1), Eq("one")), Pair(Eq(1), Eq("ichi"))));
  EXPECT_THAT(kMultimap.EqualSpan(2), IsEmpty());
  EXPECT_THAT(
      kMultimap.EqualSpan(3),
      UnorderedElementsAre(Pair(Eq(3), Eq("three")), Pair(Eq(3), Eq("san"))));
  EXPECT_THAT(kMultimap.EqualSpan(4), IsEmpty());
  EXPECT_THAT(
      kMultimap.EqualSpan(5),
      UnorderedElementsAre(Pair(Eq(5), Eq("five")), Pair(Eq(5), Eq("go"))));
  EXPECT_THAT(kMultimap.EqualSpan(6), IsEmpty());
}

}  // namespace
}  // namespace mozc
