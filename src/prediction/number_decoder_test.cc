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

#include "prediction/number_decoder.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "absl/random/random.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

namespace mozc {
namespace {

TEST(NumberDecoderTest, Decode) {
  struct TestData {
    const std::string key;
    const std::vector<NumberDecoder::Result> expected;
    TestData(const std::string k, const std::vector<NumberDecoder::Result> rs)
        : key(k), expected(rs) {}
    static TestData CreateForAllConsumed(const std::string k,
                                         const std::vector<std::string> cs) {
      std::vector<NumberDecoder::Result> expected;
      for (const auto &e : cs) {
        expected.emplace_back(k.size(), e);
      }
      return TestData(k, expected);
    }
  };
  const TestData kTestDataList[] = {
      // Units
      TestData::CreateForAllConsumed("ぜろ", {"0"}),
      TestData::CreateForAllConsumed("いち", {"1"}),
      TestData::CreateForAllConsumed("に", {"2"}),
      TestData::CreateForAllConsumed("さん", {"3"}),
      TestData::CreateForAllConsumed("し", {"4"}),
      TestData::CreateForAllConsumed("ご", {"5"}),
      TestData::CreateForAllConsumed("ろく", {"6"}),
      TestData::CreateForAllConsumed("なな", {"7"}),
      TestData::CreateForAllConsumed("はち", {"8"}),
      TestData::CreateForAllConsumed("きゅう", {"9"}),
      // Small digits
      TestData::CreateForAllConsumed("じゅう", {"10"}),
      TestData::CreateForAllConsumed("ひゃく", {"100"}),
      TestData::CreateForAllConsumed("せん", {"1000"}),
      // Big digits
      TestData::CreateForAllConsumed("まん", {}),
      TestData::CreateForAllConsumed("おく", {}),
      TestData::CreateForAllConsumed("ちょう", {}),
      TestData::CreateForAllConsumed("けい", {}),
      TestData::CreateForAllConsumed("がい", {}),
      TestData::CreateForAllConsumed("いちまん", {"1万"}),
      TestData::CreateForAllConsumed("いちおく", {"1億"}),
      TestData("いっちょう", {{6, "1"}, {15, "1兆"}}),
      TestData("いっけい", {{6, "1"}, {12, "1京"}}),
      TestData::CreateForAllConsumed("いちがい", {"1垓"}),
      // Suffix
      TestData("にせんち", {{3, "2"}}),
      TestData("にちょうめ", {{3, "2"}}),
      TestData("さんちょうめ", {{6, "3"}}),
      // Others
      TestData("ぜろに", {{6, "0"}}),
      TestData("にいち", {{3, "2"}}),
      TestData("にぜろ", {{3, "2"}}),
      TestData("にこ", {{3, "2"}}),
      TestData("にこにこ", {{3, "2"}}),
      TestData("にさんじゅう", {{3, "2"}}),
      TestData("にさんひゃく", {{3, "2"}}),
      TestData("にじゅう", {{3, "2"}, {12, "20"}}),
      TestData("にじゅうさん", {{18, "23"}}),
      TestData("にじゅうさんぜん", {{18, "23"}}),
      TestData("にせん", {{3, "2"}, {9, "2000"}}),
      TestData("にせんの", {{3, "2"}, {9, "2000"}}),
      TestData("にせんさん", {{15, "2003"}}),
      TestData("にせんさんせん", {{15, "2003"}}),
      TestData("いちまんにせん", {{15, "1万2"}, {21, "1万2000"}}),
      TestData("いちまんにせんさん", {{27, "1万2003"}}),
      TestData("いちおくさんぜんまん", {{30, "1億3000万"}}),
      TestData("いちまんさんおく", {{18, "1万3"}}),
      TestData("いちまんさんまん", {{18, "1万3"}}),
      TestData("いちおくにせん", {{15, "1億2"}, {21, "1億2000"}}),
      TestData("にちょう", {{3, "2"}, {12, "2兆"}}),
      TestData("にちょうせ", {{3, "2"}, {12, "2兆"}}),
      TestData("にちょうせん", {{12, "2兆"}, {18, "2兆1000"}}),
      TestData("じゅうにちょう", {{12, "12"}, {21, "12兆"}}),
      TestData("じゅうにちょうの", {{12, "12"}, {21, "12兆"}}),
      TestData("じゅうにちょうでも", {{12, "12"}, {21, "12兆"}}),
      TestData("じゅうにちょうめ", {{12, "12"}}),
      TestData("じゅうにちょうめの", {{12, "12"}}),
      TestData("にちゃん", {{3, "2"}}),
      TestData("にちゃんねる", {{3, "2"}}),
      TestData("じゅうにちゃん", {{12, "12"}}),
      TestData("ごごう", {{3, "5"}}),
      TestData("じゅうごう", {{9, "10"}}),
      TestData("じゅうさんちーむ", {{9, "10"}, {15, "13"}}),
      TestData("にじゅうさんちーむ", {{12, "20"}, {18, "23"}}),
      TestData("にじゅうまんさんぜん", {{30, "20万3000"}, {24, "20万3"}}),
      TestData("せんにひゃくおくさんぜんよんひゃくまんごせんろっぴゃく",
               {{27 * 3, "1200億3400万5600"}}),
  };

  mozc::NumberDecoder decoder;
  for (const auto &data : kTestDataList) {
    std::vector<mozc::NumberDecoder::Result> results;
    const bool ret = decoder.Decode(data.key, &results);
    EXPECT_EQ(!data.expected.empty(), ret) << data.key;
    EXPECT_THAT(results, ::testing::UnorderedElementsAreArray(data.expected))
        << data.key;
  }
}

TEST(NumberDecoderTest, Random) {
  const std::vector<std::string> kKeys = {
      "ぜろ",   "いち",   "いっ",   "に",     "さん",   "し",     "よん",
      "ご",     "ろく",   "ろっ",   "なな",   "しち",   "はち",   "はっ",
      "きゅう", "きゅー", "く",     "じゅう", "じゅー", "じゅっ", "ひゃく",
      "ひゃっ", "びゃく", "びゃっ", "ぴゃく", "ぴゃっ", "せん",   "ぜん",
      "まん",   "おく",   "おっ",   "ちょう", "けい",   "がい",
  };

  constexpr int kTestSize = 1000;
  constexpr int kKeySize = 10;
  absl::BitGen gen;
  mozc::NumberDecoder decoder;
  std::vector<mozc::NumberDecoder::Result> results;
  for (int try_count = 0; try_count < kTestSize; ++try_count) {
    std::vector<int> indices(kKeySize);
    std::generate(std::begin(indices), std::end(indices), [&]() {
      return absl::Uniform(gen, 0, static_cast<int>(kKeys.size()));
    });
    std::vector<std::string> key_vector;
    absl::c_transform(indices, std::back_inserter(key_vector),
                      [&](const auto &i) { return kKeys[i]; });
    std::string key = absl::StrJoin(key_vector, "");
    // Check the key does not cause any failure.
    decoder.Decode(key, &results);
  }
}

}  // namespace
}  // namespace mozc
