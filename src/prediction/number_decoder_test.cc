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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/random/random.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "converter/candidate.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::prediction {
namespace {

using ::testing::UnorderedElementsAreArray;

struct TestParam {
  explicit TestParam(absl::string_view key) : key(key) {}
  TestParam(const absl::string_view key,
            const std::initializer_list<NumberDecoderResult> results)
      : key(key), expected(results) {}

  template <typename Sink>
  friend void AbslStringify(Sink &sink, const TestParam &param) {
    sink.Append(param.key);
  }

  absl::string_view key;
  std::vector<NumberDecoderResult> expected;
};

TestParam AllConsumed(
    const absl::string_view key,
    const std::initializer_list<std::pair<absl::string_view, int>> results) {
  TestParam param(key);
  for (const auto &result : results) {
    param.expected.emplace_back(key.size(), std::string(result.first),
                                result.second);
  }
  return param;
}

class NumberDecoderTest : public ::testing::TestWithParam<TestParam> {
 public:
  NumberDecoderTest()
      : modules_(engine::Modules::Create(
                     std::make_unique<testing::MockDataManager>())
                     .value()) {}

  const dictionary::PosMatcher &pos_matcher() const {
    return modules_->GetPosMatcher();
  }

 private:
  std::unique_ptr<engine::Modules> modules_;
};

TEST_P(NumberDecoderTest, Decode) {
  const NumberDecoder decoder(pos_matcher());
  EXPECT_THAT(decoder.Decode(GetParam().key),
              UnorderedElementsAreArray(GetParam().expected));
}

INSTANTIATE_TEST_SUITE_P(Units, NumberDecoderTest,
                         ::testing::Values(AllConsumed("ぜろ", {{"0", 1}}),
                                           AllConsumed("いち", {{"1", 1}}),
                                           AllConsumed("に", {{"2", 1}}),
                                           AllConsumed("さん", {{"3", 1}}),
                                           AllConsumed("し", {{"4", 1}}),
                                           AllConsumed("ご", {{"5", 1}}),
                                           AllConsumed("ろく", {{"6", 1}}),
                                           AllConsumed("なな", {{"7", 1}}),
                                           AllConsumed("はち", {{"8", 1}}),
                                           AllConsumed("きゅう", {{"9", 1}})));

INSTANTIATE_TEST_SUITE_P(SmallDigits, NumberDecoderTest,
                         ::testing::Values(AllConsumed("じゅう", {{"10", 2}}),
                                           AllConsumed("ひゃく", {{"100", 3}}),
                                           AllConsumed("せん", {{"1000", 4}})));

INSTANTIATE_TEST_SUITE_P(
    BigDigits, NumberDecoderTest,
    ::testing::Values(AllConsumed("まん", {}), AllConsumed("おく", {}),
                      AllConsumed("ちょう", {}), AllConsumed("けい", {}),
                      AllConsumed("がい", {}),
                      AllConsumed("いちまん", {{"1万", 5}}),
                      AllConsumed("いちおく", {{"1億", 9}}),
                      TestParam{"いっちょう", {{6, "1", 1}, {15, "1兆", 13}}},
                      TestParam{"いっけい", {{6, "1", 1}, {12, "1京", 17}}},
                      AllConsumed("いちがい", {{"1垓", 21}})));

INSTANTIATE_TEST_SUITE_P(
    Suffix, NumberDecoderTest,
    ::testing::Values(TestParam("にせんち", {{3, "2", 1}}),
                      TestParam("にちょうめ", {{3, "2", 1}}),
                      TestParam("さんちょうめ", {{6, "3", 1}})));

INSTANTIATE_TEST_SUITE_P(
    Others, NumberDecoderTest,
    ::testing::Values(
        TestParam("ぜろに", {{6, "0", 1}}), TestParam("にいち", {{3, "2", 1}}),
        TestParam("にぜろ", {{3, "2", 1}}), TestParam("にこ", {{3, "2", 1}}),
        TestParam("にこにこ", {{3, "2", 1}}),
        TestParam("にさんじゅう", {{3, "2", 1}}),
        TestParam("にさんひゃく", {{3, "2", 1}}),
        TestParam("にじゅう", {{3, "2", 1}, {12, "20", 2}}),
        TestParam("にじゅうさん", {{18, "23", 2}}),
        TestParam("にじゅうさんぜん", {{18, "23", 2}}),
        TestParam("にせん", {{3, "2", 1}, {9, "2000", 4}}),
        TestParam("にせんの", {{3, "2", 1}, {9, "2000", 4}}),
        TestParam("にせんさん", {{15, "2003", 4}}),
        TestParam("にせんさんせん", {{15, "2003", 4}}),
        TestParam("いちまんにせん", {{15, "1万2", 5}, {21, "1万2000", 5}}),
        TestParam("いちまんにせんさん", {{27, "1万2003", 5}}),
        TestParam("いちおくさんぜんまん", {{30, "1億3000万", 9}}),
        TestParam("いちまんさんおく", {{18, "1万3", 5}}),
        TestParam("いちまんさんまん", {{18, "1万3", 5}}),
        TestParam("いちおくにせん", {{15, "1億2", 9}, {21, "1億2000", 9}}),
        TestParam("にちょう", {{3, "2", 1}, {12, "2兆", 13}}),
        TestParam("にちょうせ", {{3, "2", 1}, {12, "2兆", 13}}),
        TestParam("にちょうせん", {{12, "2兆", 13}, {18, "2兆1000", 13}}),
        TestParam("じゅうにちょう", {{12, "12", 2}, {21, "12兆", 14}}),
        TestParam("じゅうにちょうの", {{12, "12", 2}, {21, "12兆", 14}}),
        TestParam("じゅうにちょうでも", {{12, "12", 2}, {21, "12兆", 14}}),
        TestParam("じゅうにちょうめ", {{12, "12", 2}}),
        TestParam("じゅうにちょうめの", {{12, "12", 2}}),
        TestParam("にちゃん", {{3, "2", 1}}),
        TestParam("にちゃんねる", {{3, "2", 1}}),
        TestParam("じゅうにちゃん", {{12, "12", 2}}),
        TestParam("ごごう", {{3, "5", 1}}),
        TestParam("じゅうごう", {{9, "10", 2}}),
        TestParam("じゅうさんちーむ", {{9, "10", 2}, {15, "13", 2}}),
        TestParam("にじゅうさんちーむ", {{12, "20", 2}, {18, "23", 2}}),
        TestParam("にじゅうまんさんぜん",
                  {{30, "20万3000", 6}, {24, "20万3", 6}}),
        TestParam("せんにひゃくおくさんぜんよんひゃくまんごせんろっぴゃく",
                  {{27 * 3, "1200億3400万5600", 12}})));

INSTANTIATE_TEST_SUITE_P(InvalidSequences, NumberDecoderTest,
                         ::testing::Values(TestParam("よせん", {}),
                                           TestParam("しけい", {}),
                                           TestParam("しせん", {}),
                                           TestParam("くせん", {}),
                                           TestParam("しがいせん", {})));

TEST_F(NumberDecoderTest, Random) {
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
  const NumberDecoder decoder(pos_matcher());
  absl::BitGen gen;
  for (int try_count = 0; try_count < kTestSize; ++try_count) {
    std::string key;
    for (int i = 0; i < kKeySize; ++i) {
      absl::StrAppend(&key, kKeys[absl::Uniform(gen, 0u, std::size(kKeys))]);
    }
    // Check the key does not cause any failure.
    EXPECT_NO_FATAL_FAILURE(
        { std::vector<NumberDecoderResult> results = decoder.Decode(key); });

    const ConversionRequest request =
        ConversionRequestBuilder().SetKey(key).Build();
    for (const Result &result : decoder.Decode(request)) {
      EXPECT_TRUE(absl::StartsWith(key, result.key));
      if (result.key.size() < key.size()) {
        EXPECT_TRUE(result.candidate_attributes &
                    converter::Candidate::PARTIALLY_KEY_CONSUMED);
        EXPECT_EQ(result.consumed_key_size, Util::CharsLen(result.key));
        EXPECT_GE(result.wcost, 1000);
        EXPECT_NE(result.lid, 0);
        EXPECT_NE(result.rid, 0);
      }
    }
  }
}

}  // namespace
}  // namespace mozc::prediction
