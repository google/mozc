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

#include "base/container/flat_map.h"

#include <functional>

#include "absl/strings/string_view.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

using ::testing::Eq;
using ::testing::IsNull;
using ::testing::Pointee;

TEST(FlatMapTest, FindOrNull) {
  constexpr auto kMap = CreateFlatMap<int, absl::string_view>({
      {1, "one"},
      {3, "three"},
      {5, "five"},
  });

  EXPECT_THAT(kMap.FindOrNull(0), IsNull());
  EXPECT_THAT(kMap.FindOrNull(1), Pointee(Eq("one")));
  EXPECT_THAT(kMap.FindOrNull(2), IsNull());
  EXPECT_THAT(kMap.FindOrNull(3), Pointee(Eq("three")));
  EXPECT_THAT(kMap.FindOrNull(4), IsNull());
  EXPECT_THAT(kMap.FindOrNull(5), Pointee(Eq("five")));
  EXPECT_THAT(kMap.FindOrNull(6), IsNull());
}

TEST(FlatMapTest, CustomerCompare) {
  constexpr auto kMap = CreateFlatMap<int, absl::string_view, std::greater<>>({
      {1, "one"},
      {3, "three"},
      {5, "five"},
  });

  EXPECT_THAT(kMap.FindOrNull(0), IsNull());
  EXPECT_THAT(kMap.FindOrNull(1), Pointee(Eq("one")));
  EXPECT_THAT(kMap.FindOrNull(2), IsNull());
  EXPECT_THAT(kMap.FindOrNull(3), Pointee(Eq("three")));
  EXPECT_THAT(kMap.FindOrNull(4), IsNull());
  EXPECT_THAT(kMap.FindOrNull(5), Pointee(Eq("five")));
  EXPECT_THAT(kMap.FindOrNull(6), IsNull());
}

}  // namespace
}  // namespace mozc
