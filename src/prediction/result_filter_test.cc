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

#include "prediction/result_filter.h"

#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "prediction/result.h"
#include "testing/gunit.h"

namespace mozc::prediction::filter {

TEST(ResultFilterTest, GetMissSpelledPosition) {
  EXPECT_EQ(GetMissSpelledPosition("", ""), 0);
  EXPECT_EQ(GetMissSpelledPosition("れみおめろん", "レミオロメン"), 3);
  EXPECT_EQ(GetMissSpelledPosition("とーとばっく", "トートバッグ"), 5);
  EXPECT_EQ(GetMissSpelledPosition("おーすとりらあ", "オーストラリア"), 4);
  EXPECT_EQ(GetMissSpelledPosition("おーすとりあ", "おーすとらりあ"), 4);
  EXPECT_EQ(GetMissSpelledPosition("じきそうしょう", "時期尚早"), 7);
}

TEST(ResultFilterTest, RemoveRedundantResultsTest) {
  const std::vector<std::tuple<absl::string_view, absl::string_view, int>>
      kResults = {
          {"とうきょう", "東京", 100},
          {"とうきょう", "TOKYO", 200},
          {"とうきょうと", "東京都", 110},
          {"とうきょう", "東京", 120},
          {"とうきょう", "TOKYO", 120},
          {"とうきょうわん", "東京湾", 120},
          {"とうきょうえき", "東京駅", 130},
          {"とうきょうべい", "東京ベイ", 140},
          {"とうきょうゆき", "東京行", 150},
          {"とうきょうしぶ", "東京支部", 160},
          {"とうきょうてん", "東京店", 170},
          {"とうきょうがす", "東京ガス", 180},
          {"とうきょう!", "東京!", 1100},
          {"とうきょう!?", "東京!?", 1200},
          {"とうきょう", "東京❤", 1300},
          // "とうきょう → 東京宇" is not an actual word, but an emulation of
          // "さかい → (堺, 堺井)" and "いずみ → (泉, 泉水)".
          {"とうきょう", "東京宇", 1400}};

  std::vector<Result> results;
  for (const auto [key, value, cost] : kResults) {
    Result result;
    result.key = key;
    result.value = value;
    result.wcost = cost;
    results.emplace_back(std::move(result));
  }

  RemoveRedundantResults(&results);

  auto find_result_by_value = [](absl::Span<const Result> results,
                                 absl::string_view value) {
    return absl::c_find_if(results, [&](const Result &result) {
             return result.value == value && !result.removed;
           }) != results.end();
  };

  EXPECT_TRUE(find_result_by_value(results, "東京"));
  EXPECT_TRUE(find_result_by_value(results, "東京宇"));

  int prefix_count = 0;
  for (const auto &result : results) {
    if (result.value.starts_with("東京")) {
      ++prefix_count;
    }
  }
  // Should not have same prefix candidates a lot.
  EXPECT_LE(prefix_count, 11);
  // Candidates that predict symbols should not be handled as the redundant
  // candidates.
  const absl::string_view kExpected[] = {
      "東京", "TOKYO", "東京!", "東京!?", "東京❤",
  };
  for (int i = 0; i < std::size(kExpected); ++i) {
    EXPECT_EQ(results[i].value, kExpected[i]);
  }
}

}  // namespace mozc::prediction::filter
