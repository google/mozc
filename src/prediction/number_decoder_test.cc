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

#include <initializer_list>
#include <iterator>
#include <string>
#include <vector>

#include "testing/gmock.h"
#include "testing/gunit.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using ::testing::UnorderedElementsAreArray;

struct TestParam {
  explicit TestParam(absl::string_view key) : key(key) {}
  TestParam(const absl::string_view key,
            const std::initializer_list<NumberDecoder::Result> results)
      : key(key), expected(results) {}

  template <typename Sink>
  friend void AbslStringify(Sink &sink, const TestParam &param) {
    sink.Append(param.key);
  }

  absl::string_view key;
  std::vector<NumberDecoder::Result> expected;
};

TestParam AllConsumed(const absl::string_view key,
                      const std::initializer_list<absl::string_view> results) {
  TestParam param(key);
  for (const absl::string_view &result : results) {
    param.expected.emplace_back(key.size(), std::string(result));
  }
  return param;
}

class NumberDecoderTest : public ::testing::TestWithParam<TestParam> {};

TEST_P(NumberDecoderTest, Decode) {
  NumberDecoder decoder;
  EXPECT_THAT(decoder.Decode(GetParam().key),
              UnorderedElementsAreArray(GetParam().expected));
}

INSTANTIATE_TEST_SUITE_P(
    Units, NumberDecoderTest,
    ::testing::Values(AllConsumed("ぜろ", {"0"}), AllConsumed("いち", {"1"}),
                      AllConsumed("に", {"2"}), AllConsumed("さん", {"3"}),
                      AllConsumed("し", {"4"}), AllConsumed("ご", {"5"}),
                      AllConsumed("ろく", {"6"}), AllConsumed("なな", {"7"}),
                      AllConsumed("はち", {"8"}),
                      AllConsumed("きゅう", {"9"})));

INSTANTIATE_TEST_SUITE_P(SmallDigits, NumberDecoderTest,
                         ::testing::Values(AllConsumed("じゅう", {"10"}),
                                           AllConsumed("ひゃく", {"100"}),
                                           AllConsumed("せん", {"1000"})));

INSTANTIATE_TEST_SUITE_P(
    BigDigits, NumberDecoderTest,
    ::testing::Values(AllConsumed("まん", {}), AllConsumed("おく", {}),
                      AllConsumed("ちょう", {}), AllConsumed("けい", {}),
                      AllConsumed("がい", {}), AllConsumed("いちまん", {"1万"}),
                      AllConsumed("いちおく", {"1億"}),
                      TestParam{"いっちょう", {{6, "1"}, {15, "1兆"}}},
                      TestParam{"いっけい", {{6, "1"}, {12, "1京"}}},
                      AllConsumed("いちがい", {"1垓"})));

INSTANTIATE_TEST_SUITE_P(Suffix, NumberDecoderTest,
                         ::testing::Values(TestParam("にせんち", {{3, "2"}}),
                                           TestParam("にちょうめ", {{3, "2"}}),
                                           TestParam("さんちょうめ",
                                                     {{6, "3"}})));

INSTANTIATE_TEST_SUITE_P(
    Others, NumberDecoderTest,
    ::testing::Values(
        TestParam("ぜろに", {{6, "0"}}), TestParam("にいち", {{3, "2"}}),
        TestParam("にぜろ", {{3, "2"}}), TestParam("にこ", {{3, "2"}}),
        TestParam("にこにこ", {{3, "2"}}),
        TestParam("にさんじゅう", {{3, "2"}}),
        TestParam("にさんひゃく", {{3, "2"}}),
        TestParam("にじゅう", {{3, "2"}, {12, "20"}}),
        TestParam("にじゅうさん", {{18, "23"}}),
        TestParam("にじゅうさんぜん", {{18, "23"}}),
        TestParam("にせん", {{3, "2"}, {9, "2000"}}),
        TestParam("にせんの", {{3, "2"}, {9, "2000"}}),
        TestParam("にせんさん", {{15, "2003"}}),
        TestParam("にせんさんせん", {{15, "2003"}}),
        TestParam("いちまんにせん", {{15, "1万2"}, {21, "1万2000"}}),
        TestParam("いちまんにせんさん", {{27, "1万2003"}}),
        TestParam("いちおくさんぜんまん", {{30, "1億3000万"}}),
        TestParam("いちまんさんおく", {{18, "1万3"}}),
        TestParam("いちまんさんまん", {{18, "1万3"}}),
        TestParam("いちおくにせん", {{15, "1億2"}, {21, "1億2000"}}),
        TestParam("にちょう", {{3, "2"}, {12, "2兆"}}),
        TestParam("にちょうせ", {{3, "2"}, {12, "2兆"}}),
        TestParam("にちょうせん", {{12, "2兆"}, {18, "2兆1000"}}),
        TestParam("じゅうにちょう", {{12, "12"}, {21, "12兆"}}),
        TestParam("じゅうにちょうの", {{12, "12"}, {21, "12兆"}}),
        TestParam("じゅうにちょうでも", {{12, "12"}, {21, "12兆"}}),
        TestParam("じゅうにちょうめ", {{12, "12"}}),
        TestParam("じゅうにちょうめの", {{12, "12"}}),
        TestParam("にちゃん", {{3, "2"}}),
        TestParam("にちゃんねる", {{3, "2"}}),
        TestParam("じゅうにちゃん", {{12, "12"}}),
        TestParam("ごごう", {{3, "5"}}), TestParam("じゅうごう", {{9, "10"}}),
        TestParam("じゅうさんちーむ", {{9, "10"}, {15, "13"}}),
        TestParam("にじゅうさんちーむ", {{12, "20"}, {18, "23"}}),
        TestParam("にじゅうまんさんぜん", {{30, "20万3000"}, {24, "20万3"}}),
        TestParam("せんにひゃくおくさんぜんよんひゃくまんごせんろっぴゃく",
                  {{27 * 3, "1200億3400万5600"}})));

INSTANTIATE_TEST_SUITE_P(InvalidSequences, NumberDecoderTest,
                         ::testing::Values(TestParam("よせん", {}),
                                           TestParam("しけい", {}),
                                           TestParam("しせん", {}),
                                           TestParam("くせん", {}),
                                           TestParam("しがいせん", {})));

TEST(NumberDecoderRandomTest, Random) {
  constexpr absl::string_view kKeys[] = {
      "ぜろ",     "いち",      "いっ",   "に",     "さん",     "し",
      "よん",     "ご",        "ろく",   "ろっ",   "なな",     "しち",
      "はち",     "はっ",      "きゅう", "きゅー", "く",       "じゅう",
      "じゅー",   "じゅっ",    "ひゃく", "ひゃっ", "びゃく",   "びゃっ",
      "ぴゃく",   "ぴゃっ",    "せん",   "ぜん",   "まん",     "おく",
      "おっ",     "ちょう",    "けい",   "がい",   "にちょう", "にちょうめ",
      "にちゃん", "さんちーむ"};

  constexpr int kTestSize = 1000;
  constexpr int kKeySize = 10;
  NumberDecoder decoder;
  absl::BitGen gen;
  for (int try_count = 0; try_count < kTestSize; ++try_count) {
    std::string key;
    for (int i = 0; i < kKeySize; ++i) {
      absl::StrAppend(&key, kKeys[absl::Uniform(gen, 0u, std::size(kKeys))]);
    }
    // Check the key does not cause any failure.
    EXPECT_NO_FATAL_FAILURE(
        { std::vector<NumberDecoder::Result> results = decoder.Decode(key); });
  }
}

}  // namespace
}  // namespace mozc
