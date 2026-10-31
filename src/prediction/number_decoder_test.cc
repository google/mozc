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
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc::prediction {
namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Field;
using ::testing::Not;
using ::testing::ResultOf;
using ::testing::UnorderedElementsAreArray;

struct TestParam {
  explicit TestParam(absl::string_view key) : key(key) {}
  TestParam(const absl::string_view key,
            const std::initializer_list<NumberDecoderResult> results)
      : key(key), expected(results) {}

  template <typename Sink>
  friend void AbslStringify(Sink& sink, const TestParam& param) {
    sink.Append(param.key);
  }

  absl::string_view key;
  std::vector<NumberDecoderResult> expected;
};

TestParam AllConsumed(
    const absl::string_view key,
    const std::initializer_list<std::pair<absl::string_view, int>> results) {
  TestParam param(key);
  for (const auto& result : results) {
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

  const dictionary::PosMatcher& pos_matcher() const {
    return modules_->GetPosMatcher();
  }

 private:
  std::unique_ptr<engine::Modules> modules_;
};

// Returns true if |value| consists only of numerals (Arabic digits and the
// big-digit kanji), i.e., it is a bare number rather than a number with a
// counter suffix.
bool IsBareNumber(absl::string_view value) {
  for (const std::string& ch : Util::SplitStringToUtf8Chars(value)) {
    if (!(Util::GetScriptType(ch) == Util::NUMBER || ch == "万" || ch == "億" ||
          ch == "兆" || ch == "京" || ch == "垓")) {
      return false;
    }
  }
  return true;
}

// Succeeds if decoding |key| yields |expected| as a candidate that consumes
// the entire |key|. |is_counter_suffix| of the result must be consistent with
// |expected| (true for "45回", false for "45" or "1万"). On failure, the actual
// results are included in the message.
::testing::AssertionResult HasFullKeyCandidate(const NumberDecoder& decoder,
                                               absl::string_view key,
                                               absl::string_view expected) {
  const bool expected_is_counter_suffix = !IsBareNumber(expected);
  const std::vector<NumberDecoderResult> results = decoder.Decode(key);
  for (const NumberDecoderResult& r : results) {
    if (r.consumed_key_byte_len == key.size() && r.candidate == expected &&
        r.is_counter_suffix == expected_is_counter_suffix) {
      return ::testing::AssertionSuccess();
    }
  }
  return ::testing::AssertionFailure()
         << "key=" << key << ", expected=" << expected << ", actual=["
         << absl::StrJoin(results, ", ", absl::StreamFormatter()) << "]";
}

// Succeeds if decoding |key| yields |value| as a candidate, regardless of how
// much of |key| is consumed.
::testing::AssertionResult HasAnyCandidate(const NumberDecoder& decoder,
                                           absl::string_view key,
                                           absl::string_view value) {
  const std::vector<NumberDecoderResult> results = decoder.Decode(key);
  for (const NumberDecoderResult& r : results) {
    if (r.candidate == value) {
      return ::testing::AssertionSuccess();
    }
  }
  return ::testing::AssertionFailure()
         << "key=" << key << ", value=" << value << ", actual=["
         << absl::StrJoin(results, ", ", absl::StreamFormatter()) << "]";
}

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
    ::testing::Values(
        TestParam("にせんち", {{3, "2", 1},
                               {12, "2センチ", 1, true},
                               {12, "2cm", 1, true}}),
        TestParam("にちょうめ", {{3, "2", 1}, {15, "2丁目", 1, true}}),
        // emits_bare_number_prefix == false: the single-token bare prefix
        // ("10", "3") is suppressed; only the counter candidate is emitted.
        TestParam("じゅうにん", {{15, "10人", 2, true}}),
        TestParam("さんにん", {{12, "3人", 1, true}}),
        TestParam("にせんと", {{12, "2セント", 1, true}}),
        TestParam("さんちょうめ", {{6, "3", 1}, {18, "3丁目", 1, true}}),
        // "がい" is both 垓 and 階. Exactly one "3階" must be emitted.
        TestParam("さんがい", {{12, "3垓", 21}, {12, "3階", 1, true}}),
        // 垓 is rejected after 万 / 億; "1万階" must still be emitted exactly
        // once ("がい" stays unconsumed and is handled by Decode()).
        TestParam("いちまんがい", {{12, "1万", 5}, {18, "1万階", 5, true}}),
        // 階 does not voice after 億 ("いちおくかい"), so only "1億" remains.
        TestParam("いちおくがい", {{12, "1億", 9}}),
        // 10回 requires "じゅっかい"; "じゅう" + "かい" is not 20回 nor "20".
        TestParam("にじゅうかい", {{3, "2", 1}})));

INSTANTIATE_TEST_SUITE_P(
    Others, NumberDecoderTest,
    ::testing::Values(
        TestParam("ぜろに", {{6, "0", 1}}), TestParam("にいち", {{3, "2", 1}}),
        TestParam("にぜろ", {{3, "2", 1}}),
        TestParam("にこ", {{6, "2個", 1, true}, {6, "2戸", 1, true}}),
        TestParam("にこにこ", {}), TestParam("にさんじゅう", {{3, "2", 1}}),
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
        TestParam("じゅうにちょうめ", {{12, "12", 2}, {24, "12丁目", 2, true}}),
        TestParam("じゅうにちょうめの", {{12, "12", 2}}),
        TestParam("にちゃん", {{3, "2", 1}}),
        TestParam("にちゃんねる", {{3, "2", 1}}),
        TestParam("じゅうにちゃん", {{12, "12", 2}}),
        TestParam("ごごう",
                  {{3, "5", 1}, {9, "5号", 1, true}, {9, "5合", 1, true}}),
        TestParam("じゅうごう",
                  {{9, "10", 2}, {15, "10号", 2, true}, {15, "10合", 2, true}}),
        TestParam("じゅうさんちーむ", {{9, "10", 2}, {15, "13", 2}}),
        TestParam("にじゅうさんちーむ", {{12, "20", 2}, {18, "23", 2}}),
        TestParam("にじゅうまんさんぜん",
                  {{30, "20万3000", 6}, {24, "20万3", 6}}),
        TestParam("せんにひゃくおくさんぜんよんひゃくまんごせんろっぴゃく",
                  {{27 * 3, "1200億3400万5600", 12}})));

INSTANTIATE_TEST_SUITE_P(
    InvalidSequences, NumberDecoderTest,
    ::testing::Values(TestParam("よせん", {}), TestParam("しけい", {}),
                      TestParam("しせん", {}), TestParam("くせん", {}),
                      TestParam("しがいせん", {}), TestParam("いっ", {}),
                      TestParam("ろっ", {}), TestParam("はっ", {}),
                      TestParam("じゅっ", {}), TestParam("ひゃっ", {}),
                      TestParam("いっまん", {}), TestParam("ろっまん", {}),
                      TestParam("びゃく", {}), TestParam("ぴゃく", {}),
                      TestParam("ぜん", {}), TestParam("さんひゃく", {}),
                      TestParam("さんぴゃく", {}), TestParam("ろくひゃく", {}),
                      TestParam("ろくぴゃく", {}), TestParam("はちひゃく", {}),
                      TestParam("しちひゃく", {}), TestParam("ごぜん", {}),
                      TestParam("さんせん", {}), TestParam("しちせん", {}),
                      TestParam("しちまん", {}), TestParam("にかいて", {}),
                      TestParam("にいく", {}), TestParam("しごと", {}),
                      // Consecutive unit digits without a following small
                      // digit, or with し/よ/く/sokuon, are not a boundary.
                      TestParam("にさんま", {}), TestParam("にし", {}),
                      TestParam("によ", {}), TestParam("にいっかい", {}),
                      // Sokuon-tail partials are suppressed while typing.
                      TestParam("にじゅっ", {}), TestParam("さんびゃっ", {}),
                      TestParam("ごはん", {}), TestParam("じゅうしょ", {}),
                      TestParam("じゅうよう", {}), TestParam("しよう", {}),
                      TestParam("ごうかく", {}), TestParam("ようかい", {}),
                      TestParam("にっき", {}), TestParam("にゃんこ", {}),
                      TestParam("にゅうがく", {}), TestParam("にほんご", {}),
                      TestParam("さんどいっち", {}),
                      TestParam("いつかいく", {}), TestParam("ひとりごと", {}),
                      TestParam("ふつかよい", {}), TestParam("にーと", {}),
                      TestParam("にぇ", {}), TestParam("ぜろうん", {}),
                      // Trailing "く" is not 9 ("にじゅうく" is 二重苦).
                      TestParam("じゅうく", {}), TestParam("にじゅうく", {}),
                      // Standalone "じっ" + counter collides with 実〜 words.
                      TestParam("じっけん", {}), TestParam("じっさい", {}),
                      TestParam("じっこう", {}), TestParam("じったい", {}),
                      // "せん" + パ行 counter collides with 先輩 / 戦犯.
                      TestParam("せんぱい", {}), TestParam("せんぱん", {})));

TEST_F(NumberDecoderTest, CounterSuffixAndEuphony) {
  const NumberDecoder decoder(pos_matcher());
  // 杯 (C10: はい / ばい / ぱい)
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっぱい", "1杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にはい", "2杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんばい", "3杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よんはい", "4杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ごはい", "5杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ろっぱい", "6杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ななはい", "7杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "はっぱい", "8杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "はちはい", "8杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "きゅうはい", "9杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅっぱい", "10杯"));
  // Standalone "じっ" + counter is suppressed (see "じっけん" below).
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "じっぱい", "10杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にじっぱい", "20杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひゃっぱい", "100杯"));
  // 300杯 is "さんびゃっぱい"; "さんびゃくばい" is 300倍 only.
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "さんびゃくばい", "300杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんびゃくばい", "300倍"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんびゃっぱい", "300杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ごひゃっぱい", "500杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "せんばい", "1000杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いちまんばい", "1万杯"));

  // Reject invalid sandhi for 杯
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "さんぱい", "3杯"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "さんはい", "3杯"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "よんぱい", "4杯"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "よんばい", "4杯"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "にばい", "2杯"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "いちぱい", "1杯"));

  // 分 (C11: ふん / ぷん)
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっぷん", "1分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にふん", "2分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんぷん", "3分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よんぷん", "4分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よんふん", "4分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ごふん", "5分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ろっぷん", "6分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ななふん", "7分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "はっぷん", "8分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "きゅうふん", "9分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅっぷん", "10分"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "じっぷん", "10分"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんじっぷん", "30分"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "さんふん", "3分"));

  // Other counter classes & Wago
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっぽん", "1本"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんぼん", "3本"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっぴき", "1匹"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんびき", "3匹"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっぱく", "1泊"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんぱく", "3泊"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんぺん", "3編"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんぺん", "3返"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっぽ", "1歩"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんぽ", "3歩"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いちふくろ", "1袋"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんぶくろ", "3袋"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅっぷくろ", "10袋"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんわ", "3羽"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんば", "3羽"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ろっぱ", "6羽"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっかい", "1回"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にかい", "2回"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ろっかい", "6回"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よんじゅうごかい", "45回"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんがい", "3階"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっこ", "1個"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にこ", "2個"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ごひゃっこ", "500個"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いちきろ", "1キロ"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ろくきろ", "6キロ"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ろっきろ", "6キロ"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いちけーす", "1ケース"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっけーす", "1ケース"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっさい", "1歳"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にじゅうごさい", "25歳"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "はっさい", "8歳"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅっさい", "10歳"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっさつ", "1冊"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いちしあい", "1試合"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっしあい", "1試合"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いちせだい", "1世代"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いっせだい", "1世代"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にしゅうかん", "2週間"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ごじゅうぱーせんと", "50%"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんびゃくまい", "300枚"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんど", "3度"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にじゅうごど", "25度"));

  // Homophone suppression for single-digit counter suffixes
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "ごはん", "5版"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "いっぱん", "1版"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "にほん", "2本"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "いちど", "1度"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "にど", "2度"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "いちぶ", "1部"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "よわり", "4割"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "よだん", "4段"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "みばん", "3晩"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にじゅうにほん", "22本"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よじょう", "4畳"));

  // C3/C4 & Wago
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひとり", "1人"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ふたり", "2人"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんにん", "3人"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よにん", "4人"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よんにん", "4人"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうよにん", "14人"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よねん", "4年"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よんねん", "4年"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よえん", "4円"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よんえん", "4円"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "れいじ", "0時"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よじ", "4時"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よじ", "4次"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "しちじ", "7時"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "くじ", "9時"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "しがつ", "4月"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "しちがつ", "7月"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "くがつ", "9月"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ついたち", "1日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ふつか", "2日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "みっか", "3日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よっか", "4日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "とおか", "10日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうよっか", "14日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうよんにち", "14日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "はつか", "20日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にじゅうよっか", "24日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にじゅうよんにち", "24日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひとつ", "1つ"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ふたつ", "2つ"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ここのつ", "9つ"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひとはこ", "1箱"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ふたはこ", "2箱"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひとばん", "1晩"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "よばん", "4晩"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "みへや", "3部屋"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ごへや", "5部屋"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひとさじ", "1匙"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひとふり", "1振り"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひとしずく", "1雫"));

  // "じっ" requires a preceding digit; "じゅっ" does not.
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅっけん", "10件"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にじっけん", "20件"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にじっぷん", "20分"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "じっけん", "10件"));
  // "せん" + パ行 counters are allowed only in compound numbers.
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にせんぱい", "2000杯"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にせんぱん", "2000版"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "せんぱい", "1000杯"));
  EXPECT_FALSE(HasFullKeyCandidate(decoder, "せんぱん", "1000版"));
  // Trailing "く" (9) is valid only before a counter suffix.
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうくにち", "19日"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうく", "19"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "にじゅうく", "29"));

  // ConversionRequest overload returns counter suffix results with
  // counter_suffix_word_id and suppresses false positives like "にかいて".
  const ConversionRequest req_45kai =
      ConversionRequestBuilder().SetKey("よんじゅうごかい").Build();
  // "45回": digit_num = 2, so wcost = 2500 + 1000 * (1 + 2).
  // lid is kanji_number_id because Util::GetScriptType("45回") is
  // UNKNOWN_SCRIPT (mixed script), the same as "1万".
  EXPECT_THAT(decoder.Decode(req_45kai),
              Contains(AllOf(
                  Field(&Result::key, "よんじゅうごかい"),
                  Field(&Result::value, "45回"), Field(&Result::wcost, 5500),
                  Field(&Result::lid, pos_matcher().GetKanjiNumberId()),
                  Field(&Result::rid, pos_matcher().GetCounterSuffixWordId()),
                  ResultOf(
                      [](const Result& r) {
                        return (r.attributes &
                                converter::Attribute::PARTIALLY_KEY_CONSUMED) !=
                               0;
                      },
                      false))));
  // The bare number prefix is still emitted as a partial candidate.
  EXPECT_THAT(
      decoder.Decode(req_45kai),
      Contains(AllOf(Field(&Result::key, "よんじゅうご"),
                     Field(&Result::value, "45"), Field(&Result::wcost, 3000),
                     Field(&Result::lid, pos_matcher().GetNumberId()),
                     Field(&Result::rid, pos_matcher().GetNumberId()))));

  // Counter suffix candidates for the same key are ranked by table order, not
  // by code point: "5話" (C1) < "5羽" < "5把" (C14 "羽,把").
  {
    const ConversionRequest req_gowa =
        ConversionRequestBuilder().SetKey("ごわ").Build();
    std::vector<Result> gowa_results;
    for (const Result& r : decoder.Decode(req_gowa)) {
      if (r.rid == pos_matcher().GetCounterSuffixWordId()) {
        gowa_results.push_back(r);
      }
    }
    ASSERT_EQ(gowa_results.size(), 3);
    EXPECT_EQ(gowa_results[0].value, "5話");
    EXPECT_EQ(gowa_results[1].value, "5羽");
    EXPECT_EQ(gowa_results[2].value, "5把");
    // digit_num = 1: 1000 * (1 + 1) + 2500 + 100 * rank.
    EXPECT_EQ(gowa_results[0].wcost, 4500);
    EXPECT_EQ(gowa_results[1].wcost, 4600);
    EXPECT_EQ(gowa_results[2].wcost, 4700);
  }
  {
    // Comma-separated surfaces keep their order: 台 > 代 > 題.
    std::vector<std::string> values;
    for (const Result& r :
         decoder.Decode(ConversionRequestBuilder().SetKey("にだい").Build())) {
      if (r.rid == pos_matcher().GetCounterSuffixWordId()) {
        values.push_back(r.value);
      }
    }
    EXPECT_THAT(values, ::testing::ElementsAre("2台", "2代", "2題"));
  }
  {
    // "3階" is emitted from HandleBigDigitEntry; it is the first counter
    // result so it gets no rank penalty.
    EXPECT_THAT(
        decoder.Decode(ConversionRequestBuilder().SetKey("さんがい").Build()),
        Contains(
            AllOf(Field(&Result::value, "3階"), Field(&Result::wcost, 4500))));
  }

  // Reordered readings: 倍 > 杯, % > パーセント, 対 > 体, 竿 > 棹.
  {
    auto first_counter = [&](absl::string_view key) {
      for (const Result& r :
           decoder.Decode(ConversionRequestBuilder().SetKey(key).Build())) {
        if (r.rid == pos_matcher().GetCounterSuffixWordId()) {
          return r.value;
        }
      }
      return std::string();
    };
    EXPECT_EQ(first_counter("ひゃくばい"), "100倍");
    EXPECT_EQ(first_counter("ごじゅうぱーせんと"), "50%");
    EXPECT_EQ(first_counter("さんたい"), "3対");
    EXPECT_EQ(first_counter("ひとさお"), "1竿");
  }

  // Non-standard 連濁 after 百 / 6 is not generated.
  EXPECT_FALSE(HasAnyCandidate(decoder, "ひゃくぼん", "100本"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "ひゃくびき", "100匹"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "ろくば", "6羽"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "ひゃくば", "100羽"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひゃくほん", "100本"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひゃっぽん", "100本"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ろくわ", "6羽"));
  // Standalone special readings restricted to compounds.
  EXPECT_FALSE(HasAnyCandidate(decoder, "くにち", "9日"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "よはこ", "4箱"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "よはり", "4針"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "よわ", "4羽"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうくにち", "19日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうよはこ", "14箱"));
  // "せん" + counter homophone guards (専業, 専門, 選手, 繊維, 宣言, 先週,
  // 戦闘, 戦場); compounds are still generated.
  for (const auto& [key, rejected] :
       {std::pair{"せんぎょう", "1000行"}, std::pair{"せんもん", "1000問"},
        std::pair{"せんしゅ", "1000種"}, std::pair{"せんい", "1000位"},
        std::pair{"せんげん", "1000軒"}, std::pair{"せんしゅう", "1000週"},
        std::pair{"せんとう", "1000頭"}, std::pair{"せんじょう", "1000条"}}) {
    EXPECT_FALSE(HasAnyCandidate(decoder, key, rejected));
  }
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にせんしゅう", "2000週"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にせんもん", "2000問"));

  // Counter suffix candidates are suppressed for typing-corrected keys, while
  // bare numbers are kept.
  ConversionRequest::Options options;
  options.use_already_typing_corrected_key = true;
  const ConversionRequest req_45kai_corrected = ConversionRequestBuilder()
                                                    .SetOptions(options)
                                                    .SetKey("よんじゅうごかい")
                                                    .Build();
  EXPECT_THAT(decoder.Decode(req_45kai_corrected),
              AllOf(Not(Contains(Field(&Result::value, "45回"))),
                    Contains(Field(&Result::value, "45"))));

  const ConversionRequest req_nikaite =
      ConversionRequestBuilder().SetKey("にかいて").Build();
  EXPECT_TRUE(decoder.Decode(req_nikaite).empty());
}

TEST_F(NumberDecoderTest, AllRulesCovered) {
  const NumberDecoder decoder(pos_matcher());
  // 1. Internal number euphony
  constexpr std::pair<absl::string_view, absl::string_view> kInternalCases[] = {
      {"しじゅう", "40"},        {"しちじゅう", "70"},
      {"いっぴゃく", "100"},     {"にひゃく", "200"},
      {"さんびゃく", "300"},     {"よんひゃく", "400"},
      {"ごひゃく", "500"},       {"ろっぴゃく", "600"},
      {"ななひゃく", "700"},     {"はっぴゃく", "800"},
      {"きゅうひゃく", "900"},   {"いっせん", "1000"},
      {"にせん", "2000"},        {"さんぜん", "3000"},
      {"よんせん", "4000"},      {"ごせん", "5000"},
      {"ろくせん", "6000"},      {"ななせん", "7000"},
      {"はっせん", "8000"},      {"きゅうせん", "9000"},
      {"いちまん", "1万"},       {"いちおく", "1億"},
      {"いっちょう", "1兆"},     {"ろくちょう", "6兆"},
      {"はっちょう", "8兆"},     {"じゅっちょう", "10兆"},
      {"じっちょう", "10兆"},    {"ひゃくちょう", "100兆"},
      {"ひゃっちょう", "100兆"}, {"いっけい", "1京"},
      {"ろっけい", "6京"},       {"はっけい", "8京"},
      {"じゅっけい", "10京"},    {"ひゃっけい", "100京"},
      {"いちがい", "1垓"},
  };
  for (const auto& [key, expected] : kInternalCases) {
    EXPECT_TRUE(HasFullKeyCandidate(decoder, key, expected));
  }

  // 2. C1/C2 Counter Suffixes (unchanged and loanword units)
  constexpr std::pair<absl::string_view, absl::string_view> kC1C2Cases[] = {
      {"いちまい", "1枚"},
      {"じゅうよまい", "14枚"},
      {"にだい", "2台"},
      {"にだい", "2代"},
      {"にだい", "2題"},
      {"じゅうよだい", "14台"},
      {"さんばん", "3番"},
      {"じゅうよばん", "14番"},
      {"よんごう", "4号"},
      {"さんど", "3度"},
      {"にじゅういちど", "21度"},
      {"ぜろびょうかん", "0秒間"},
      {"ごびょう", "5秒"},
      {"ろくめい", "6名"},
      {"ななわり", "7割"},
      {"じゅうよわり", "14割"},
      {"はちれつ", "8列"},
      {"じゅうよれつ", "14列"},
      {"きゅうぎょう", "9行"},
      {"じゅうちょうめ", "10丁目"},
      {"にばんち", "2番地"},
      {"さんぶ", "3部"},
      {"にじゅういちぶ", "21部"},
      {"よじょう", "4条"},
      {"よじょう", "4畳"},
      {"よんじょう", "4畳"},
      {"いっしょう", "1勝"},
      {"いっしょう", "1章"},
      {"はっしょう", "8章"},
      {"じゅっしょう", "10章"},
      {"いっとん", "1トン"},
      {"じゅっとん", "10トン"},
      {"いちわ", "1話"},
      {"ごわ", "5話"},
      {"ごわ", "5羽"},
      {"にもん", "2問"},
      {"いちくみ", "1組"},
      {"いちしゅるい", "1種類"},
      {"いっしゅるい", "1種類"},
      {"じゅうしゅるい", "10種類"},
      {"いちしゅ", "1種"},
      {"いっしゅ", "1種"},
      {"いちせだい", "1世代"},
      {"いっせだい", "1世代"},
      {"じゅうせだい", "10世代"},
      {"いちせたい", "1世帯"},
      {"いっせたい", "1世帯"},
      {"じゅうせたい", "10世帯"},
      {"さんだんかい", "3段階"},
      {"よんだん", "4段"},
      {"じゅうよだん", "14段"},
      {"ごへや", "5部屋"},
      {"じゅうへや", "10部屋"},
      {"じゅっぺや", "10部屋"},
      {"にじっぺや", "20部屋"},
      {"いちい", "1位"},
      {"にじゅうごい", "25位"},
      {"ぜろみり", "0ミリ"},
      {"にせんち", "2センチ"},
      {"ひゃくめーとる", "100メートル"},
      {"ごひゃくぐらむ", "500グラム"},
      {"にりっとる", "2リットル"},
      {"じゅうとん", "10トン"},
      {"ひゃくどる", "100ドル"},
      {"ごじゅっせんと", "50セント"},
      {"ごじゅうせんと", "50セント"},
      {"いちせんと", "1セント"},
      {"いっせんと", "1セント"},
      {"ごじゅうゆーろ", "50ユーロ"},
      {"ろくじゅうよんびっと", "64ビット"},
      {"はちばいと", "8バイト"},
  };
  for (const auto& [key, expected] : kC1C2Cases) {
    EXPECT_TRUE(HasFullKeyCandidate(decoder, key, expected));
  }

  // 3. C3/C4 Counter Suffixes (人, 年, 円, 月, 日, 時, 次)
  constexpr std::pair<absl::string_view, absl::string_view> kC3C4Cases[] = {
      {"さんにん", "3人"},
      {"よにん", "4人"},
      {"よんにん", "4人"},
      {"しちにん", "7人"},
      {"くにん", "9人"},
      {"にじゅうににん", "22人"},
      {"いちねんかん", "1年間"},
      {"よねんかん", "4年間"},
      {"よんねんかん", "4年間"},
      {"よねん", "4年"},
      {"よんねん", "4年"},
      {"くねん", "9年"},
      {"ぜろえん", "0円"},
      {"よえん", "4円"},
      {"よんえん", "4円"},
      {"ななえん", "7円"},
      {"いちがつ", "1月"},
      {"しがつ", "4月"},
      {"しちがつ", "7月"},
      {"くがつ", "9月"},
      {"よっかかん", "4日間"},
      {"じゅうよっかかん", "14日間"},
      {"にじゅうよっかかん", "24日間"},
      {"よっか", "4日"},
      {"じゅうよっか", "14日"},
      {"にじゅうよっか", "24日"},
      {"いちにちかん", "1日間"},
      {"よんにちかん", "4日間"},
      {"じゅうよんにちかん", "14日間"},
      {"いちにち", "1日"},
      {"じゅうににち", "12日"},
      {"じゅうよんにち", "14日"},
      {"じゅうしちにち", "17日"},
      {"じゅうくにち", "19日"},
      {"にじゅうよんにち", "24日"},
      {"よじかん", "4時間"},
      {"しちじかん", "7時間"},
      {"くじかん", "9時間"},
      {"ぜろじ", "0時"},
      {"よじ", "4時"},
      {"しちじ", "7時"},
      {"くじ", "9時"},
      {"いちじ", "1次"},
      {"にじ", "2次"},
      {"よじ", "4次"},
      {"しちじ", "7次"},
  };
  for (const auto& [key, expected] : kC3C4Cases) {
    EXPECT_TRUE(HasFullKeyCandidate(decoder, key, expected));
  }

  // 4. C5/C6/C7/C8 Counter Suffixes (Ka-row, Pa-row, and rendaku)
  constexpr std::pair<absl::string_view, absl::string_view> kC5ToC8Cases[] = {
      {"いっかい", "1回"},
      {"いっかい", "1階"},
      {"ろっかい", "6回"},
      {"はっかい", "8回"},
      {"はちかい", "8回"},
      {"じゅっかい", "10回"},
      {"ひゃっかい", "100回"},
      {"いちおっかい", "1億回"},
      {"さんがい", "3階"},
      {"せんがい", "1000階"},
      {"いっこ", "1個"},
      {"いっこ", "1戸"},
      {"いっかん", "1巻"},
      {"いっかん", "1缶"},
      {"にせんかん", "2000巻"},
      {"いっけん", "1件"},
      {"いっき", "1期"},
      {"ななき", "7期"},
      {"ななき", "7機"},
      {"さんき", "3基"},
      {"ろっき", "6期"},
      {"じゅっき", "10期"},
      {"にじゅうななき", "27期"},
      {"いっけん", "1軒"},
      {"さんげん", "3軒"},
      {"いっかげつ", "1ヶ月"},
      {"いっかげつ", "1か月"},
      {"ろっかげつ", "6ヶ月"},
      {"いっかしょ", "1箇所"},
      {"いっかしょ", "1か所"},
      {"いっかこく", "1か国"},
      {"いっきょく", "1曲"},
      {"いっきょく", "1局"},
      {"にせんきょく", "2000局"},
      {"いっこう", "1校"},
      {"いっこう", "1項"},
      {"にじゅうごこう", "25校"},
      {"いちぺーじ", "1ページ"},
      {"いっぺーじ", "1ページ"},
      {"いっぱーせんと", "1パーセント"},
      {"いっぱーせんと", "1%"},
      {"ごじゅうぱーせんと", "50%"},
      {"いちきろ", "1キロ"},
      {"ろくきろ", "6キロ"},
      {"ろっきろ", "6キロ"},
      {"じゅっきろ", "10キロ"},
      {"いちかっぷ", "1カップ"},
      {"いっかっぷ", "1カップ"},
      {"いちけーす", "1ケース"},
      {"いっけーす", "1ケース"},
      {"いちこーす", "1コース"},
      {"いっこーす", "1コース"},
      {"いちくらす", "1クラス"},
      {"いっくらす", "1クラス"},
      {"いちぱっく", "1パック"},
      {"いっぱっく", "1パック"},
      {"いちぽいんと", "1ポイント"},
      {"いっぽいんと", "1ポイント"},
      {"いちぽんど", "1ポンド"},
      {"いっぽんど", "1ポンド"},
      {"じゅっぽんど", "10ポンド"},
  };
  for (const auto& [key, expected] : kC5ToC8Cases) {
    EXPECT_TRUE(HasFullKeyCandidate(decoder, key, expected));
  }

  // 5. C9 Counter Suffixes (Sa-row, Ta-row, Sha-row)
  constexpr std::pair<absl::string_view, absl::string_view> kC9Cases[] = {
      {"いっさい", "1歳"},        {"いっさい", "1才"},
      {"ろくさい", "6歳"},        {"はっさい", "8歳"},
      {"じゅっさい", "10歳"},     {"いっさつ", "1冊"},
      {"いったい", "1体"},        {"はったい", "8体"},
      {"いったい", "1対"},        {"はちたい", "8対"},
      {"いっつう", "1通"},        {"いってんぽ", "1店舗"},
      {"じゅってんぽ", "10店舗"}, {"いってん", "1点"},
      {"いっちゃく", "1着"},      {"いっとう", "1頭"},
      {"いっとう", "1等"},        {"いっとう", "1棟"},
      {"にじゅうごとう", "25頭"}, {"いっしつ", "1室"},
      {"いっしょう", "1勝"},      {"いちしあい", "1試合"},
      {"いっしあい", "1試合"},    {"じゅうしあい", "10試合"},
      {"じゅっしあい", "10試合"}, {"いっしき", "1式"},
      {"にじゅうにしき", "22式"}, {"いっしゃ", "1社"},
      {"いっしゃ", "1車"},        {"いっしゅうかん", "1週間"},
      {"いっしゅう", "1週"},      {"いっしょく", "1色"},
      {"いっしょく", "1食"},      {"いっせき", "1席"},
      {"いっせき", "1隻"},        {"いっそく", "1足"},
      {"いっそく", "1束"},        {"ひゃくしーしー", "100cc"},
      {"いちしーと", "1シート"},  {"いっしーと", "1シート"},
  };
  for (const auto& [key, expected] : kC9Cases) {
    EXPECT_TRUE(HasFullKeyCandidate(decoder, key, expected));
  }

  // 6. C10-C14 Counter Suffixes (Ha-row and Wa-row three-way alternation)
  constexpr std::pair<absl::string_view, absl::string_view> kHaWaCases[] = {
      {"いっぱい", "1杯"},
      {"にはい", "2杯"},
      {"さんばい", "3杯"},
      {"にばい", "2倍"},
      {"いっぱい", "1敗"},
      {"にはい", "2敗"},
      {"さんばい", "3敗"},
      {"いっぽん", "1本"},
      {"さんぼん", "3本"},
      {"よんほん", "4本"},
      {"いっぴき", "1匹"},
      {"にひき", "2匹"},
      {"さんびき", "3匹"},
      {"いっぴょう", "1票"},
      {"にひょう", "2票"},
      {"さんびょう", "3票"},
      {"いっぱこ", "1箱"},
      {"さんばこ", "3箱"},
      {"よんはこ", "4箱"},
      {"いっぱり", "1針"},
      {"さんばり", "3針"},
      {"よんはり", "4針"},
      {"にじゅういっぱん", "21版"},
      {"にじゅういっぱん", "21班"},
      {"さんばん", "3版"},
      {"さんばん", "3班"},
      {"にはん", "2版"},
      {"にじゅうごはん", "25版"},
      {"いっぴん", "1品"},
      {"にひん", "2品"},
      {"さんびん", "3品"},
      {"いっぽう", "1報"},
      {"いっぽう", "1包"},
      {"いっぽう", "1砲"},
      {"にほう", "2報"},
      {"さんぼう", "3報"},
      {"にじゅうごほう", "25報"},
      {"いちふくろ", "1袋"},
      {"さんぶくろ", "3袋"},
      {"じゅっぷくろ", "10袋"},
      {"いちふさ", "1房"},
      {"さんぶさ", "3房"},
      {"じゅっぷさ", "10房"},
      {"いっぷんかん", "1分間"},
      {"にふんかん", "2分間"},
      {"さんぷんかん", "3分間"},
      {"よんぷんかん", "4分間"},
      {"いっぷん", "1分"},
      {"にふん", "2分"},
      {"さんぷん", "3分"},
      {"よんぷん", "4分"},
      {"いっぱく", "1泊"},
      {"いっぱく", "1拍"},
      {"にはく", "2泊"},
      {"さんぱく", "3泊"},
      {"いっぱつ", "1発"},
      {"にはつ", "2発"},
      {"さんぱつ", "3発"},
      {"さんばつ", "3発"},
      {"いっぺん", "1編"},
      {"いっぺん", "1辺"},
      {"いっぺん", "1返"},
      {"いっぺん", "1遍"},
      {"にへん", "2編"},
      {"さんぺん", "3編"},
      {"さんべん", "3編"},
      {"よんぺん", "4編"},
      {"いっぽ", "1歩"},
      {"にほ", "2歩"},
      {"さんぽ", "3歩"},
      {"よんぽ", "4歩"},
      {"せんぼ", "1000歩"},
      {"いちわ", "1羽"},
      {"いちわ", "1把"},
      {"いっぱ", "1羽"},
      {"いっぱ", "1把"},
      {"さんば", "3羽"},
      {"さんば", "3把"},
      {"にせんば", "2000羽"},
      {"ろっぱ", "6羽"},
      {"じゅっぱ", "10羽"},
  };
  for (const auto& [key, expected] : kHaWaCases) {
    EXPECT_TRUE(HasFullKeyCandidate(decoder, key, expected));
  }

  // 7. Standalone Wago numeral rules
  constexpr std::pair<absl::string_view, absl::string_view> kWagoStandalone[] =
      {
          {"ひとり", "1人"},       {"ふたり", "2人"},
          {"ついたち", "1日"},     {"いっぴ", "1日"},
          {"ふつか", "2日"},       {"ふつかかん", "2日間"},
          {"みっか", "3日"},       {"みっかかん", "3日間"},
          {"いつか", "5日"},       {"いつかかん", "5日間"},
          {"むいか", "6日"},       {"むいかかん", "6日間"},
          {"なのか", "7日"},       {"なのかかん", "7日間"},
          {"ようか", "8日"},       {"ようかかん", "8日間"},
          {"よおか", "8日"},       {"よおかかん", "8日間"},
          {"ここのか", "9日"},     {"ここのかかん", "9日間"},
          {"とおか", "10日"},      {"とおかかん", "10日間"},
          {"とうか", "10日"},      {"とうかかん", "10日間"},
          {"はつか", "20日"},      {"はつかかん", "20日間"},
          {"ひとつ", "1つ"},       {"ふたつ", "2つ"},
          {"みっつ", "3つ"},       {"よっつ", "4つ"},
          {"いつつ", "5つ"},       {"むっつ", "6つ"},
          {"ななつ", "7つ"},       {"やっつ", "8つ"},
          {"ここのつ", "9つ"},     {"れいじ", "0時"},
          {"れいじかん", "0時間"},
      };
  for (const auto& [key, expected] : kWagoStandalone) {
    EXPECT_TRUE(HasFullKeyCandidate(decoder, key, expected));
  }

  // 8. Wago Prefix Rules ("ひと" / "ふた" / "み" / "よ" + all 38 counters)
  constexpr std::pair<absl::string_view, absl::string_view> kAllWagoPrefix[] = {
      {"ひとはこ", "1箱"},       {"ふたはこ", "2箱"},
      {"ひとばん", "1晩"},       {"ふたばん", "2晩"},
      {"よばん", "4晩"},         {"ひとへや", "1部屋"},
      {"ふたへや", "2部屋"},     {"みへや", "3部屋"},
      {"よへや", "4部屋"},       {"ひとふくろ", "1袋"},
      {"ふたふくろ", "2袋"},     {"ひとつぶ", "1粒"},
      {"ふたつぶ", "2粒"},       {"ひとさら", "1皿"},
      {"ふたさら", "2皿"},       {"ひとやま", "1山"},
      {"ふたやま", "2山"},       {"ひときれ", "1切れ"},
      {"ひときれ", "1切"},       {"ふたきれ", "2切れ"},
      {"ひとくち", "1口"},       {"ふたくち", "2口"},
      {"ひとつぼ", "1坪"},       {"ひとつぼ", "1壺"},
      {"ふたつぼ", "2坪"},       {"ひとかたまり", "1塊"},
      {"ふたかたまり", "2塊"},   {"ひとまき", "1巻"},
      {"ふたまき", "2巻"},       {"ひとふさ", "1房"},
      {"ふたふさ", "2房"},       {"ひとかけ", "1掛け"},
      {"ふたかけ", "2掛け"},     {"ひとにぎり", "1握り"},
      {"ふたにぎり", "2握り"},   {"ひとたば", "1束"},
      {"ふたたば", "2束"},       {"ひとえだ", "1枝"},
      {"ふたえだ", "2枝"},       {"ひとかぶ", "1株"},
      {"ふたかぶ", "2株"},       {"ひとけた", "1桁"},
      {"ふたけた", "2桁"},       {"ひとおけ", "1桶"},
      {"ふたおけ", "2桶"},       {"ひとむね", "1棟"},
      {"ふたむね", "2棟"},       {"ひとたる", "1樽"},
      {"ふたたる", "2樽"},       {"ひとすじ", "1筋"},
      {"ふたすじ", "2筋"},       {"ひとかん", "1缶"},
      {"ふたかん", "2缶"},       {"ひとびん", "1瓶"},
      {"ふたびん", "2瓶"},       {"ひとぱっく", "1パック"},
      {"ふたぱっく", "2パック"}, {"ひとつまみ", "1つまみ"},
      {"ふたつまみ", "2つまみ"}, {"ひとさじ", "1匙"},
      {"ふたさじ", "2匙"},       {"ひとふり", "1振り"},
      {"ひとふり", "1振"},       {"ふたふり", "2振り"},
      {"ひとくし", "1串"},       {"ふたくし", "2串"},
      {"ひとしずく", "1雫"},     {"ふたしずく", "2雫"},
      {"ひとかご", "1籠"},       {"ふたかご", "2籠"},
      {"ひとはち", "1鉢"},       {"ふたはち", "2鉢"},
      {"ひとはり", "1針"},       {"ふたはり", "2針"},
      {"ひとさし", "1差し"},     {"ふたさし", "2差し"},
      {"ひとさお", "1棹"},       {"ひとさお", "1竿"},
      {"ふたさお", "2棹"},       {"ひとわん", "1椀"},
      {"ふたわん", "2椀"},       {"ひとかま", "1釜"},
      {"ひとかま", "1窯"},       {"ふたかま", "2釜"},
  };
  for (const auto& [key, expected] : kAllWagoPrefix) {
    EXPECT_TRUE(HasFullKeyCandidate(decoder, key, expected));
  }

  // 9. Additional counters from the system dictionary
  constexpr std::pair<absl::string_view, absl::string_view> kDictionaryCases[] =
      {
          // Surface variants added to existing rules.
          {"さんかげつ", "3カ月"},
          {"さんかげつ", "3ヵ月"},
          {"さんかしょ", "3ヶ所"},
          {"さんかこく", "3カ国"},
          {"さんぺん", "3篇"},
          {"さんぺーじ", "3頁"},
          {"さんせんち", "3cm"},
          {"さんみり", "3mm"},
          {"さんきろ", "3km"},
          {"さんきろ", "3kg"},
          {"さんてん", "3店"},
          {"さんだん", "3弾"},
          {"さんど", "3℃"},
          {"さんごう", "3合"},
          {"さんかん", "3冠"},
          {"にじょう", "2乗"},
          {"いっしゅう", "1周"},
          {"にしゅ", "2首"},
          // C7.
          {"さんかいめ", "3回目"},
          {"いっかいせん", "1回戦"},
          {"にかいぶん", "2回分"},
          {"にかいだて", "2階建て"},
          {"いっきせい", "1期生"},
          {"いっきゅう", "1級"},
          {"さんきゅう", "3級"},
          {"いっきゅう", "1球"},
          {"にきろめーとる", "2km"},
          {"ごきろぐらむ", "5キログラム"},
          // C1 derived forms.
          {"にばんめ", "2番目"},
          {"じゅうよばんめ", "14番目"},
          {"にばんて", "2番手"},
          {"さんばんせん", "3番線"},
          {"さんだいめ", "3代目"},
          {"にまいめ", "2枚目"},
          {"にどめ", "2度目"},
          {"いちねんせい", "1年生"},
          {"よねんせい", "4年生"},
          {"にせんにじゅうごねんど", "2025年度"},
          {"はちじゅうねんだい", "80年代"},
          {"さんねんめ", "3年目"},
          {"ごねんぶり", "5年ぶり"},
          {"じゅうねんご", "10年後"},
          {"にせんにじゅうごねんばん", "2025年版"},
          // 時 / 次 derived forms.
          {"さんじはん", "3時半"},
          {"よじはん", "4時半"},
          {"くじはん", "9時半"},
          {"さんじすぎ", "3時過ぎ"},
          {"さんじすぎ", "3時すぎ"},
          {"ごじごろ", "5時頃"},
          {"じゅうじだい", "10時台"},
          {"にじげん", "2次元"},
          {"さんじげん", "3次元"},
          {"よじげん", "4次元"},
          // 人 derived forms.
          {"いちにんまえ", "1人前"},
          {"ににんまえ", "2人前"},
          {"さんにんぐみ", "3人組"},
          {"よにんぐみ", "4人組"},
          {"いちにちめ", "1日目"},
          {"じゅういちにちめ", "11日目"},
          {"ふつかめ", "2日目"},
          {"みっかめ", "3日目"},
          {"とおかめ", "10日目"},
          {"ひとつめ", "1つ目"},
          {"みっつめ", "3つ目"},
          {"ここのつめ", "9つ目"},
          {"さんごうせん", "3号線"},
          {"にごうき", "2号機"},
          // C9.
          {"にしゅうめ", "2周目"},
          {"じゅっしゅうねん", "10周年"},
          {"にじゅういっせいき", "21世紀"},
          {"さんさいじ", "3歳児"},
          {"さんとうぶん", "3等分"},
          {"にせい", "2世"},
          {"ななせい", "7世"},
          {"いっさく", "1作"},
          {"にさく", "2作"},
          {"いっせつ", "1節"},
          {"にそう", "2層"},
          {"さんそう", "3層"},
          {"にたく", "2択"},
          {"さんたく", "3択"},
          {"いっせっと", "1セット"},
          {"さんせっと", "3セット"},
          // Other C1.
          {"ごいんち", "5インチ"},
          {"いちめん", "1面"},
          {"さんめん", "3面"},
          {"いちれい", "1例"},
          {"さんりょう", "3両"},
          {"にりん", "2輪"},
          {"よんりん", "4輪"},
          {"さんれん", "3連"},
          {"いちわく", "1枠"},
          {"にまく", "2幕"},
          {"さんとおり", "3通り"},
          {"ひととおり", "1通り"},
          {"ふたとおり", "2通り"},
          {"さんらうんど", "3ラウンド"},
          {"ろくふぃーと", "6フィート"},
          {"ごまいる", "5マイル"},
          {"せんうぉん", "1000ウォン"},
          {"ごじゅうへいほうめーとる", "50平方メートル"},
          {"ひゃくせんちめーとる", "100cm"},
          {"ごみりめーとる", "5mm"},
          {"ひゃくみりりっとる", "100ml"},
          {"ごみりぐらむ", "5mg"},
          {"にぎが", "2GB"},
          {"ごひゃくめが", "500MB"},
          {"れいてん", "0点"},
          {"れいど", "0度"},
      };
  for (const auto& [key, expected] : kDictionaryCases) {
    EXPECT_TRUE(HasFullKeyCandidate(decoder, key, expected));
  }
}

TEST_F(NumberDecoderTest, RejectedRules) {
  const NumberDecoder decoder(pos_matcher());
  // ===========================================================================
  // Comprehensive Negative Tests (Ungrammatical Euphony & Homophone Guards)
  // ===========================================================================
  constexpr std::pair<absl::string_view, absl::string_view> kRejectedCases[] = {
      // --- 9.1 Invalid internal number euphony ---
      // Tens place: "いち", geminated forms, "よ", and "く" are invalid
      {"いちじゅう", "10"},
      {"いっじゅう", "10"},
      {"ろっじゅう", "60"},
      {"はっじゅう", "80"},
      {"よじゅう", "40"},
      {"くじゅう", "90"},
      // Hundreds place: 1/6/8 require geminated "ぴゃく", 3 requires voiced
      // "びゃく", others require voiceless "ひゃく"
      {"いちひゃく", "100"},
      {"いちびゃく", "100"},
      {"いっびゃく", "100"},
      {"にびゃく", "200"},
      {"にぴゃく", "200"},
      {"さんひゃく", "300"},
      {"さんぴゃく", "300"},
      {"よんびゃく", "400"},
      {"よんぴゃく", "400"},
      {"しひゃく", "400"},
      {"よひゃく", "400"},
      {"ごびゃく", "500"},
      {"ごぴゃく", "500"},
      {"ろくひゃく", "600"},
      {"ろくびゃく", "600"},
      {"ろくぴゃく", "600"},
      {"ろっひゃく", "600"},
      {"ろっびゃく", "600"},
      {"ななびゃく", "700"},
      {"ななぴゃく", "700"},
      {"しちひゃく", "700"},
      {"はちひゃく", "800"},
      {"はちびゃく", "800"},
      {"はちぴゃく", "800"},
      {"はっひゃく", "800"},
      {"はっびゃく", "800"},
      {"きゅうびゃく", "900"},
      {"きゅうぴゃく", "900"},
      {"くひゃく", "900"},
      // Thousands place: 1/8 require geminated ("いっせん"/"はっせん"), 3
      // requires voiced ("さんぜん"), others require voiceless ("せん")
      {"いちせん", "1000"},
      {"いちぜん", "1000"},
      {"いっぜん", "1000"},
      {"にぜん", "2000"},
      {"さんせん", "3000"},  // 参戦
      {"よんぜん", "4000"},
      {"しせん", "4000"},  // 視線 / 死線
      {"よせん", "4000"},  // 予選
      {"ごぜん", "5000"},  // 午前
      {"ろっせん", "6000"},
      {"ろくぜん", "6000"},
      {"ななぜん", "7000"},
      {"しちせん", "7000"},
      {"はちせん", "8000"},
      {"はちぜん", "8000"},
      {"はっぜん", "8000"},
      {"きゅうぜん", "9000"},
      {"くせん", "9000"},  // 苦戦
      // 万, 億, 兆, 京, 垓
      {"いっまん", "1万"},
      {"ろっまん", "6万"},
      {"はっまん", "8万"},
      {"じゅっまん", "10万"},
      {"ひゃっまん", "100万"},
      {"しちまん", "7万"},
      {"よまん", "4万"},
      {"くまん", "9万"},
      {"いっおく", "1億"},
      {"ろっおく", "6億"},
      {"はっおく", "8億"},
      {"じゅっおく", "10億"},
      {"しちおく", "7億"},
      {"いちおっ", "1億"},  // Standalone trailing sokuon is invalid
      {"いちちょう", "1兆"},
      {"ろっちょう", "6兆"},
      {"はちちょう", "8兆"},
      {"じゅうちょう", "10兆"},
      {"しちちょう", "7兆"},
      // 6京, 8京, and 100京 take geminated forms only ("ろっけい", "はっけい",
      // "ひゃっけい").
      {"いちけい", "1京"},
      {"ろくけい", "6京"},
      {"はちけい", "8京"},
      {"じゅうけい", "10京"},
      {"ひゃくけい", "100京"},
      {"しけい", "4京"},  // 死刑
      {"しちけい", "7京"},
      {"いっがい", "1垓"},
      {"ろっがい", "6垓"},
      {"はっがい", "8垓"},
      {"しちがい", "7垓"},

      // --- 9.2 C1/C2: Unchanged counters (sokuon, special readings,
      // homophones) ---
      {"いっまい", "1枚"},
      {"ろっまい", "6枚"},
      {"はっまい", "8枚"},
      {"じゅっまい", "10枚"},
      {"しまい", "4枚"},  // 姉妹
      {"くまい", "9枚"},
      {"いっだい", "1台"},
      {"しだい", "4台"},  // 次第
      {"くだい", "9台"},
      {"いっばん", "1番"},
      {"しばん", "4番"},
      {"くばん", "9番"},
      {"いっごう", "1号"},
      {"しごう", "4号"},
      {"よごう", "4号"},
      {"くごう", "9号"},
      {"いっびょう", "1秒"},
      {"しびょう", "4秒"},
      {"よびょう", "4秒"},  // 余病
      {"くびょう", "9秒"},
      {"いっめい", "1名"},
      {"しめい", "4名"},  // 氏名 / 使命
      {"よめい", "4名"},  // 余命
      {"くめい", "9名"},
      {"いっわり", "1割"},
      {"しわり", "4割"},
      {"くわり", "9割"},
      {"いっれつ", "1列"},
      {"しれつ", "4列"},  // 熾烈
      {"くれつ", "9列"},
      {"いっぎょう", "1行"},
      {"しぎょう", "4行"},  // 始業
      {"よぎょう", "4行"},
      {"くぎょう", "9行"},  // 苦行
      {"いっちょうめ", "1丁目"},
      {"いっばんち", "1番地"},
      {"いっじょう", "1畳"},
      {"しじょう", "4畳"},  // 市場 / 史上
      {"くじょう", "9畳"},  // 苦情
      {"くじょう", "9条"},
      {"いっわ", "1話"},
      {"しわ", "4話"},  // 皺
      {"よわ", "4話"},  // 夜話
      {"くわ", "9話"},  // 鍬
      {"いっもん", "1問"},
      {"しもん", "4問"},  // 指紋
      {"よもん", "4問"},
      {"くもん", "9問"},  // 苦悶
      {"いっくみ", "1組"},
      {"しくみ", "4組"},  // 仕組み
      {"よくみ", "4組"},
      {"くくみ", "9組"},
      {"いっだん", "1段"},
      {"しだん", "4段"},  // 師団
      {"くだん", "9段"},  // 件
      {"いっい", "1位"},
      {"しい", "4位"},  // 私意
      {"よい", "4位"},  // 良い
      {"くい", "9位"},  // 杭 / 悔い
      // Standalone "よ" (4) before 枚, 台, 番, 割, 列, and 段 is suppressed to
      // avoid homophone collisions (compound numbers like 14枚 "じゅうよまい"
      // are allowed).
      {"よまい", "4枚"},
      {"よだい", "4台"},  // 四大
      {"よばん", "4番"},  // 夜番 (only "4晩" is generated)
      {"よわり", "4割"},  // 弱り
      {"よれつ", "4列"},  // 余列
      {"よだん", "4段"},  // 余談 / 予断
      // Standalone "いちど", "にど", "いちぶ", and "ごい" are restricted to
      // compound numbers to avoid competing with common words (一度, 二度,
      // 一部,
      // 語彙).
      {"いちど", "1度"},
      {"にど", "2度"},
      {"いちぶ", "1部"},
      {"にぶ", "2部"},
      {"ごぶ", "5部"},
      {"ごい", "5位"},

      // --- 9.3 C3/C4: 人, 年, 円, 月, 日, 時, 次 ---
      // Standalone "いちにん" (1人) and "ににん" (2人) are not generated; only
      // Wago "ひとり"/"ふたり" and compound numbers like "じゅういちにん"
      // (11人)
      // are.
      {"いちにん", "1人"},  // 一任
      {"ににん", "2人"},
      {"いっにん", "1人"},
      {"しにん", "4人"},  // 死人
      {"いっねん", "1年"},
      {"しねん", "4年"},  // 思念
      {"いっえん", "1円"},
      {"しえん", "4円"},  // 支援
      {"しちえん", "7円"},
      {"くえん", "9円"},  // クエン酸
      // Colloquial/misread "よんがつ", "なながつ", and "きゅうがつ".
      {"ぜろがつ", "0月"},
      {"よんがつ", "4月"},
      {"よがつ", "4月"},
      {"なながつ", "7月"},
      {"きゅうがつ", "9月"},
      {"いっがつ", "1月"},
      {"じゅっがつ", "10月"},
      {"ひゃくがつ", "100月"},
      {"せんがつ", "1000月"},
      // Standalone day-count readings "ににち", "さんにち", "よんにち", etc.
      // are
      // not generated ("じゅうよんにち" [14日] and "にじゅうよんにち" [24日]
      // are); "じゅうなのか" / "じゅうようか" are not supported.
      {"ににち", "2日"},
      {"さんにち", "3日"},
      {"よんにち", "4日"},
      {"ごにち", "5日"},
      {"ろくにち", "6日"},
      {"はちにち", "8日"},
      {"じゅうにち", "10日"},
      {"ににちかん", "2日間"},
      {"さんにちかん", "3日間"},
      {"ごにちかん", "5日間"},
      {"ろくにちかん", "6日間"},
      {"はちにちかん", "8日間"},
      {"じゅうにちかん", "10日間"},
      {"しにち", "4日"},
      {"よにち", "4日"},
      {"いっにち", "1日"},
      {"よんっか", "4日"},
      {"いっか", "1日"},  // 一家 / 一課
      {"じゅうなのか", "17日"},
      {"じゅうようか", "18日"},
      // 4時間 takes "よじかん" only.
      {"よんじかん", "4時間"},
      {"よんじ", "4時"},  // 四字
      {"よんじ", "4次"},
      {"しじ", "4時"},  // 指示 / 支持
      {"しじ", "4次"},
      {"いっじ", "1時"},
      {"じゅっじ", "10時"},
      {"ぜろじ", "0次"},  // Only "0時" is valid
      {"くじ", "9次"},    // Only "9時" is valid ("9次" uses "きゅうじ")

      // --- 9.4 C5-C8: Ka-row and Pa-row counters + rendaku ---
      // 6 takes geminated forms only before カ行 counters ("ろっかい",
      // "ろっかげつ").
      {"いちかい", "1回"},
      {"いちかい", "1階"},
      {"ろくかい", "6回"},
      {"ろくかい", "6階"},
      {"じゅうかい", "10回"},  // 十戒 / 重解
      {"じゅうかい", "10階"},
      {"しかい", "4回"},  // 司会 / 視界
      {"よかい", "4回"},
      {"くかい", "9回"},  // 苦界
      {"いちこ", "1個"},
      {"ろくこ", "6個"},
      {"じゅうこ", "10個"},
      {"しこ", "4個"},
      {"よこ", "4個"},  // 横
      {"くこ", "9個"},
      {"いちかん", "1巻"},
      {"ろくかん", "6巻"},
      {"じゅうかん", "10巻"},
      {"しかん", "4巻"},       // 史観
      {"せんかん", "1000巻"},  // 戦艦 (compound "にせんかん" [2000巻] is valid)
      {"いちけん", "1件"},
      {"ろくけん", "6件"},
      {"じゅうけん", "10件"},
      {"しけん", "4件"},  // 試験
      {"よけん", "4件"},  // 予見
      {"いちかげつ", "1ヶ月"},
      {"ろくかげつ", "6ヶ月"},
      {"じゅうかげつ", "10ヶ月"},
      {"いちかしょ", "1箇所"},
      {"ろくかしょ", "6箇所"},
      {"いちかこく", "1か国"},
      {"ろくかこく", "6か国"},
      {"いちきょく", "1曲"},
      {"せんきょく", "1000局"},  // 選挙区
      {"いちこう", "1校"},
      {"ごこう", "5校"},       // 後光
      {"せんこう", "1000校"},  // 選考 / 専攻
      // C8 voiced ("がい", "げん") applies only to 3, 千, 万 (and not to 回/件)
      {"さんがい", "3回"},
      {"さんげん", "3件"},
      {"いちがい", "1階"},  // Only "1垓"
      {"にがい", "2階"},    // 苦い
      {"よんがい", "4階"},
      {"ごがい", "5階"},  // 号外
      {"ろくがい", "6階"},
      {"なながい", "7階"},
      {"はちがい", "8階"},
      {"きゅうがい", "9階"},
      {"じゅうがい", "10階"},
      // 連濁 "がい" is not generated after 百 ("ひゃくがい" collides with
      // 百害).
      {"ひゃくがい", "100階"},
      {"さんびゃくがい", "300階"},
      {"にげん", "2軒"},    // 二元
      {"よんげん", "4軒"},  // 四元
      {"ごげん", "5軒"},    // 語源
      {"ろくげん", "6軒"},
      {"じゅうげん", "10軒"},

      // --- 9.5 C9: Sa-row, Ta-row, and Sha-row counters ---
      // Non-geminated "いち" before Sa/Ta-row counters is limited to the
      // counters listed in number_decoder_data.inc.
      {"いちさい", "1歳"},
      {"ろっさい", "6歳"},
      {"じゅうさい", "10歳"},  // 重罪
      {"いちさつ", "1冊"},
      {"ろっさつ", "6冊"},
      {"じゅうさつ", "10冊"},  // 銃殺
      {"いちたい", "1体"},
      {"ろったい", "6体"},
      {"じゅうたい", "10体"},  // 渋滞 / 重体
      {"はったい", "8対"},     // Only "8体" ("8対" uses "はちたい")
      {"いちつう", "1通"},
      {"ろっつう", "6通"},
      {"じゅうつう", "10通"},
      {"いちてんぽ", "1店舗"},
      {"ろってんぽ", "6店舗"},
      {"じゅうてんぽ", "10店舗"},
      {"いちてん", "1点"},
      {"ろってん", "6点"},
      {"じゅうてん", "10点"},  // 重点 / 充填
      {"いちちゃく", "1着"},
      {"ろっちゃく", "6着"},
      {"じゅうちゃく", "10着"},
      {"いちとう", "1頭"},
      {"ごとう", "5頭"},  // 後藤 / 五島 (compound "にじゅうごとう" is valid)
      {"ろっとう", "6頭"},
      {"じゅうとう", "10頭"},  // 充当
      {"せんとう", "1000頭"},  // 戦闘 / 銭湯 / 先頭
      {"いちしつ", "1室"},
      {"ろっしつ", "6室"},
      {"じゅうしつ", "10室"},
      {"ろっしょう", "6勝"},
      {"じゅうしょう", "10勝"},  // 重傷 / 重症
      {"ろっしあい", "6試合"},
      {"いちしき", "1式"},
      {"にしき", "2式"},  // 錦 (compound "にじゅうにしき" [22式] is valid)
      {"ろっしき", "6式"},
      {"いちしゃ", "1社"},
      {"ろっしゃ", "6社"},
      {"じゅうしゃ", "10社"},  // 従者
      {"いちしゅうかん", "1週間"},
      {"ろっしゅうかん", "6週間"},
      {"じゅうしゅうかん", "10週間"},
      {"いちしゅう", "1週"},
      {"ろっしゅう", "6週"},
      {"じゅうしゅう", "10週"},
      {"いちしょく", "1色"},
      {"ろっしょく", "6色"},
      {"じゅうしょく", "10色"},  // 住職 / 重職
      {"いちせき", "1席"},
      {"ろっせき", "6席"},
      {"じゅうせき", "10席"},  // 重責
      {"いちそく", "1足"},
      {"ろっそく", "6足"},
      {"じゅうそく", "10足"},  // 充足

      // --- 9.6 C10-C14: Ha-row and Wa-row three-way alternation counters ---
      // C10: 杯, 敗, 本, 匹, 票, 箱, 針, 版, 品, 報
      {"いちはい", "1杯"},
      {"いちばい", "1杯"},
      {"いちぱい", "1杯"},
      {"にばい", "2杯"},  // Only "2倍"
      {"にばい", "2敗"},
      {"にぱい", "2杯"},
      {"さんはい", "3杯"},
      {"さんぱい", "3杯"},  // 参拝 / 惨敗
      {"さんぱい", "3敗"},
      {"よんばい", "4杯"},
      {"よんぱい", "4杯"},
      {"ごばい", "5杯"},
      {"ごぱい", "5杯"},
      {"ろくはい", "6杯"},
      {"ろくばい", "6杯"},
      {"ななばい", "7杯"},
      {"ななぱい", "7杯"},
      {"はちばい", "8杯"},
      {"きゅうばい", "9杯"},
      {"きゅうぱい", "9杯"},
      {"じゅうはい", "10杯"},
      {"じゅうばい", "10杯"},
      {"いちほん", "1本"},
      {"いちぼん", "1本"},
      {"にほん", "2本"},  // 日本 (compound "にじゅうにほん" [22本] is valid)
      {"にぼん", "2本"},
      {"にぽん", "2本"},
      {"さんほん", "3本"},
      {"さんぽん", "3本"},
      {"よんぼん", "4本"},
      {"よんぽん", "4本"},
      {"ごぼん", "5本"},
      {"ごぽん", "5本"},
      {"ろくほん", "6本"},
      {"ろくぼん", "6本"},
      {"じゅうほん", "10本"},
      {"じゅうぼん", "10本"},
      {"いちひき", "1匹"},
      {"にびき", "2匹"},
      {"にぴき", "2匹"},
      {"さんひき", "3匹"},
      {"さんぴき", "3匹"},
      {"よんびき", "4匹"},
      {"よんぴき", "4匹"},
      {"ろくひき", "6匹"},
      {"じゅうひき", "10匹"},
      {"いちひょう", "1票"},
      {"さんひょう", "3票"},
      {"さんぴょう", "3票"},
      {"よんびょう", "4票"},
      {"よんぴょう", "4票"},
      {"いちはこ", "1箱"},
      {"さんはこ", "3箱"},
      {"さんぱこ", "3箱"},
      {"ろくはこ", "6箱"},
      {"じゅうはこ", "10箱"},
      {"いっぱん", "1版"},  // 一般
      {"いっぱん", "1班"},
      {"ごはん", "5版"},  // ご飯
      {"ごはん", "5班"},
      {"ごほう", "5報"},  // 誤報
      // C11: 分, 分間 (voiced "ぶん" is never valid; "ふん" is invalid after
      // 1/3/6/10/100)
      {"いちふん", "1分"},
      {"いちぶん", "1分"},
      {"にぶん", "2分"},
      {"にぷん", "2分"},
      {"さんふん", "3分"},
      {"さんぶん", "3分"},  // 三分
      {"よんぶん", "4分"},
      {"ごぶん", "5分"},
      {"ごぷん", "5分"},
      // 6分 takes "ろっぷん" only.
      {"ろくふん", "6分"},
      {"ろくぶん", "6分"},
      {"ななぶん", "7分"},
      {"ななぷん", "7分"},
      {"はちぶん", "8分"},
      {"きゅうぶん", "9分"},
      {"きゅうぷん", "9分"},
      {"じゅうふん", "10分"},
      {"じゅうぶん", "10分"},  // 十分
      {"ひゃくふん", "100分"},
      // C12: 泊, 拍, 発, 編, 辺, 返, 遍, 歩
      {"いちはく", "1泊"},
      {"にぱく", "2泊"},
      {"さんはく", "3泊"},
      {"さんばく", "3泊"},
      {"ごぱく", "5泊"},
      {"ろくはく", "6泊"},
      {"じゅうはく", "10泊"},
      {"いちはつ", "1発"},
      {"にぱつ", "2発"},
      {"さんはつ", "3発"},
      {"ごぱつ", "5発"},
      {"ろくはつ", "6発"},
      {"じゅうはつ", "10発"},
      {"いちへん", "1編"},
      {"にぺん", "2編"},
      {"さんへん", "3編"},
      {"ごぺん", "5編"},
      {"ろくへん", "6編"},
      {"じゅうへん", "10編"},
      {"いちほ", "1歩"},
      {"にぽ", "2歩"},
      {"さんほ", "3歩"},
      {"ごぽ", "5歩"},
      {"ろくほ", "6歩"},
      {"じゅうほ", "10歩"},
      // C14: 羽, 把
      {"にば", "2羽"},  // 二場
      {"にぱ", "2羽"},
      {"さんぱ", "3羽"},  // 三派
      {"よんば", "4羽"},
      {"よんぱ", "4羽"},
      {"ごば", "5羽"},
      {"ごぱ", "5羽"},
      {"ななば", "7羽"},
      {"ななぱ", "7羽"},
      {"きゅうば", "9羽"},  // 急場
      {"きゅうぱ", "9羽"},
      {"せんば", "1000羽"},  // 船場 / 仙波 (compound "にせんば" is valid)

      // --- 9.7 Wago numeral restrictions and homophone collision guards ---
      // Standalone Wago numerals do not combine with tens
      {"じゅうひとり", "11人"},
      {"じゅうふたり", "12人"},
      {"にじゅうひとり", "21人"},
      {"じゅうひとつ", "11つ"},
      {"じゅうふたつ", "12つ"},
      {"とおつ", "10つ"},
      {"じゅうふつか", "12日"},
      {"じゅうみっか", "13日"},
      {"じゅういつか", "15日"},
      {"じゅうむいか", "16日"},
      // 3 ("み") and 4 ("よ") are disallowed except for 部屋 and 晩 to prevent
      // collisions with common words like "みかん" (蜜柑), "みかけ" (見掛け),
      // "みはり" (見張り), "みふり" (身振り), and "みさお" (操).
      {"みばん", "3晩"},      // 三番
      {"みかん", "3缶"},      // 蜜柑
      {"みかけ", "3掛け"},    // 見掛け
      {"みはり", "3針"},      // 見張り
      {"みふり", "3振り"},    // 身振り
      {"みさお", "3棹"},      // 操
      {"みやま", "3山"},      // 深山
      {"みすじ", "3筋"},      // 三筋
      {"みはこ", "3箱"},      // 3箱 uses "さんばこ"
      {"みふくろ", "3袋"},    // 3袋 uses "さんぶくろ"
      {"みつぶ", "3粒"},      // 3粒 uses "さんつぶ"
      {"みさら", "3皿"},      // 3皿 uses "さんさら"
      {"みきれ", "3切れ"},    // 3切れ uses "さんきれ"
      {"みくち", "3口"},      // 3口 uses "さんくち"
      {"みつぼ", "3坪"},      // 3坪 uses "さんつぼ"
      {"みかたまり", "3塊"},  // 3塊
      {"みまき", "3巻"},      // 3巻 uses "さんまき" / "さんかん"
      {"みふさ", "3房"},      // 3房 uses "さんぶさ"
      {"みにぎり", "3握り"},  // 3握り
      {"みたば", "3束"},      // 3束 uses "さんたば" / "さんそく"
      {"みえだ", "3枝"},      // 3枝
      {"みかぶ", "3株"},      // 3株 uses "さんかぶ"
      {"みけた", "3桁"},      // 3桁 uses "さんけた"
      {"よけた", "4桁"},      // 4桁 uses "よんけた" (collides with 避けた)
      {"みおけ", "3桶"},      // 3桶
      {"みむね", "3棟"},      // 3棟 uses "さんむね" / "さんとう"
      {"みたる", "3樽"},      // 3樽
      {"みびん", "3瓶"},      // 3瓶
      {"みぱっく", "3パック"},
      {"みつまみ", "3つまみ"},
      {"みさじ", "3匙"},
      {"みくし", "3串"},
      {"みしずく", "3雫"},
      {"みかご", "3籠"},
      {"みはち", "3鉢"},
      {"みさし", "3差し"},
      {"みわん", "3椀"},
      {"みかま", "3釜"},

      // --- 9.9 Homophone guards for counters from the system dictionary ---
      {"きゅうきゅう", "9級"},    // 救急
      {"きゅうしゅう", "9周"},    // 九州
      {"きゅうしゅう", "9週"},    // 九州
      {"いっせい", "1世"},        // 一斉
      {"さんせい", "3世"},        // 賛成
      {"はっせい", "8世"},        // 発生
      {"きゅうせい", "9世"},      // 急性
      {"せんせい", "1000世"},     // 先生
      {"さんさく", "3作"},        // 散策
      {"きゅうさく", "9作"},      // 久作
      {"いっそう", "1層"},        // 一層
      {"はっそう", "8層"},        // 発送
      {"せんそう", "1000層"},     // 戦争
      {"せんたく", "1000択"},     // 選択
      {"ごめん", "5面"},          // ごめん
      {"せんめん", "1000面"},     // 洗面
      {"ごれい", "5例"},          // 御礼
      {"せんれい", "1000例"},     // 洗礼
      {"ごりょう", "5両"},        // 御料
      {"きゅうりょう", "9両"},    // 給料
      {"じゅうりょう", "10両"},   // 重量
      {"せんりょう", "1000両"},   // 占領
      {"せんれん", "1000連"},     // 洗練
      {"いちにんぐみ", "1人組"},  // 1人組 is not idiomatic
      {"ににちめ", "2日目"},      // 2日目 is "ふつかめ"
  };
  for (const auto& [key, rejected] : kRejectedCases) {
    EXPECT_FALSE(HasAnyCandidate(decoder, key, rejected));
  }

  // 9.8 kStopSuffixEntries longest-match guards. Each stop suffix must (a)
  // produce the intended full-key counter candidate and (b) not produce even a
  // partial bare number that swallows the suffix's leading kana ("12", "14",
  // "15", "19", "2000", "3兆").
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうにん", "10人"));
  // 10日 is "とおか"; "にち" after "じゅう" is multi-digit only.
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅういちにち", "11日"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひとにぎり", "1握り"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうしあい", "10試合"));
  // C9 counters take "じゅっ" (not "じゅう") for 10.
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんしき", "3式"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんしつ", "3室"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんしゃ", "3社"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうしゅ", "10種"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしょう", "10章"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんしょう", "3勝"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんしょく", "3色"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうごう", "10号"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうくみ", "10組"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "ひとくち", "1口"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "じゅうくらす", "10クラス"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にせんち", "2センチ"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "にせんと", "2セント"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "いちおくたーぶ", "1オクターブ"));
  EXPECT_TRUE(HasFullKeyCandidate(decoder, "さんちょうめ", "3丁目"));

  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうにん", "12"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうにち", "12"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうにぎり", "12"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしあい", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしき", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしつ", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしゃ", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしゅ", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしょう", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしょく", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうごう", "15"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうくみ", "19"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうくち", "19"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうくらす", "19"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしな", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしりんぐ", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしん", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうしーべると", "14"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうくだり", "19"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "じゅうくろーな", "19"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "にせんち", "2000"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "にせんと", "2000"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "いちおくたーぶ", "1億"));
  EXPECT_FALSE(HasAnyCandidate(decoder, "さんちょうめ", "3兆"));
}

TEST_F(NumberDecoderTest, Random) {
  constexpr absl::string_view kKeys[] = {
      "ぜろ", "いち", "いっ", "に", "さん", "し", "よん", "ご", "ろく", "ろっ",
      "なな", "しち", "はち", "はっ", "きゅう", "きゅー", "く", "じゅう",
      "じゅー", "じゅっ", "じっ", "ひゃく", "ひゃっ", "びゃく", "びゃっ",
      "ぴゃく", "ぴゃっ", "せん", "ぜん", "まん", "おく", "おっ", "ちょう",
      "けい", "がい", "にちょう", "にちょうめ", "にちゃん", "さんちーむ",
      // Counter suffixes, stop suffixes, and Wago readings.
      "かい", "ほん", "ぱい", "にん", "にち", "せんち", "ごう", "ひと", "ふた",
      "つ", "っか", "かん"};

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
    for (const Result& result : decoder.Decode(request)) {
      EXPECT_TRUE(absl::StartsWith(key, result.key));
      if (result.rid == pos_matcher().GetCounterSuffixWordId()) {
        // Counter suffix / Wago candidates always consume the whole key.
        EXPECT_EQ(result.key, key);
        EXPECT_FALSE(result.attributes &
                     converter::Attribute::PARTIALLY_KEY_CONSUMED);
      }
      if (result.key.size() < key.size()) {
        EXPECT_TRUE(result.attributes &
                    converter::Attribute::PARTIALLY_KEY_CONSUMED);
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
