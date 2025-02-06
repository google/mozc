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

#include "rewriter/date_rewriter.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/clock.h"
#include "base/clock_mock.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "converter/segments_matchers.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_token.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

namespace mozc {
namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::Token;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Not;
using ::testing::StrEq;
using ::testing::Values;

void InitCandidate(const absl::string_view key, const absl::string_view value,
                   Segment::Candidate *candidate) {
  candidate->content_key = std::string(key);
  candidate->value = std::string(value);
  candidate->content_value = std::string(value);
}

void AppendSegment(const absl::string_view key, const absl::string_view value,
                   Segments *segments) {
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  InitCandidate(key, value, seg->add_candidate());
}

void InitSegment(const absl::string_view key, const absl::string_view value,
                 Segments *segments) {
  segments->Clear();
  AppendSegment(key, value, segments);
}

void InsertCandidate(const absl::string_view key, const absl::string_view value,
                     const int position, Segment *segment) {
  Segment::Candidate *cand = segment->insert_candidate(position);
  cand->content_key = std::string(key);
  cand->value = std::string(value);
  cand->content_value = std::string(value);
}

Matcher<const Segment::Candidate *> ValueIs(absl::string_view value) {
  return Field(&Segment::Candidate::value, value);
}

// A matcher to test if a candidate has the given value and description.
Matcher<const Segment::Candidate *> ValueAndDescAre(absl::string_view value,
                                                    absl::string_view desc) {
  return Pointee(AllOf(Field(&Segment::Candidate::value, value),
                       Field(&Segment::Candidate::description, desc)));
}

// An action that invokes a DictionaryInterface::Callback with the token whose
// value is set to the given one.
struct InvokeCallbackWithUserDictionaryToken {
  template <class T>
  void operator()(absl::string_view key, T,
                  DictionaryInterface::Callback *callback) {
    const Token token(key, value, MockDictionary::kDefaultCost,
                      MockDictionary::kDefaultPosId,
                      MockDictionary::kDefaultPosId, Token::USER_DICTIONARY);
    callback->OnToken(key, key, token);
  }

  std::string value;
};

}  // namespace

class DateRewriterTest : public testing::TestWithTempUserProfile {};

TEST_F(DateRewriterTest, DateRewriteTest) {
  ClockMock mock_clock(ParseTimeOrDie("2011-04-18T15:06:31Z"));
  Clock::SetClockForUnitTest(&mock_clock);

  DateRewriter rewriter;
  Segments segments;
  const ConversionRequest request;

  {
    InitSegment("きょう", "今日", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    constexpr absl::string_view kDesc = "今日の日付";
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0),
                CandidatesAreArray({
                    ValueAndDescAre("今日", ""),
                    ValueAndDescAre("2011/04/18", kDesc),
                    ValueAndDescAre("2011-04-18", kDesc),
                    ValueAndDescAre("2011年4月18日", kDesc),
                    ValueAndDescAre("平成23年4月18日", kDesc),
                    ValueAndDescAre("月曜日", kDesc),
                }));
  }
  {
    InitSegment("あした", "明日", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    constexpr absl::string_view kDesc = "明日の日付";
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0),
                CandidatesAreArray({
                    ValueAndDescAre("明日", ""),
                    ValueAndDescAre("2011/04/19", kDesc),
                    ValueAndDescAre("2011-04-19", kDesc),
                    ValueAndDescAre("2011年4月19日", kDesc),
                    ValueAndDescAre("平成23年4月19日", kDesc),
                    ValueAndDescAre("火曜日", kDesc),
                }));
  }
  {
    InitSegment("きのう", "昨日", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    constexpr absl::string_view kDesc = "昨日の日付";
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0),
                CandidatesAreArray({
                    ValueAndDescAre("昨日", ""),
                    ValueAndDescAre("2011/04/17", kDesc),
                    ValueAndDescAre("2011-04-17", kDesc),
                    ValueAndDescAre("2011年4月17日", kDesc),
                    ValueAndDescAre("平成23年4月17日", kDesc),
                    ValueAndDescAre("日曜日", kDesc),
                }));
  }
  {
    InitSegment("あさって", "明後日", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    constexpr absl::string_view kDesc = "明後日の日付";
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0),
                CandidatesAreArray({
                    ValueAndDescAre("明後日", ""),
                    ValueAndDescAre("2011/04/20", kDesc),
                    ValueAndDescAre("2011-04-20", kDesc),
                    ValueAndDescAre("2011年4月20日", kDesc),
                    ValueAndDescAre("平成23年4月20日", kDesc),
                    ValueAndDescAre("水曜日", kDesc),
                }));
  }
  const std::pair<std::string, std::string> kCurrentDateTimeKeyValues[]{
      {"にちじ", "日時"},
      {"なう", "ナウ"},
  };
  for (const auto &[key, value] : kCurrentDateTimeKeyValues) {
    InitSegment(key, value, &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0),
                CandidatesAreArray({
                    ValueAndDescAre(value, ""),
                    ValueAndDescAre("2011/04/18 15:06", "現在の日時"),
                }));
  }
  const std::pair<std::string, std::string> kCurrentTimeKeyValues[]{
      {"いま", "今"},
      {"じこく", "時刻"},
  };
  for (const auto &[key, value] : kCurrentTimeKeyValues) {
    InitSegment(key, value, &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    constexpr absl::string_view kDesc = "現在の時刻";
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0), CandidatesAreArray({
                                         ValueAndDescAre(value, ""),
                                         ValueAndDescAre("15:06", kDesc),
                                         ValueAndDescAre("15時06分", kDesc),
                                         ValueAndDescAre("午後3時6分", kDesc),
                                     }));
  }

  // Tests for insert positions.
  {
    constexpr absl::string_view kDesc = "今日の日付";

    // If the segment contains only one candidate, the date rewriter adds
    // candidates after it. This case is already tested above; see the case for
    // "きょう", "今日".

    // If the segment contains 5 candidates and the rewriter target is at
    // index 4, the rewriter adds candidates after it.
    InitSegment("きょう", "今日", &segments);
    // Push front 4 stub candidates so that "今日" is positioned at index 4.
    InsertCandidate("Candidate1", "Candidate1", 0, segments.mutable_segment(0));
    InsertCandidate("Candidate2", "Candidate2", 0, segments.mutable_segment(0));
    InsertCandidate("Candidate3", "Candidate3", 0, segments.mutable_segment(0));
    InsertCandidate("Candidate4", "Candidate4", 0, segments.mutable_segment(0));

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0),
                CandidatesAreArray({
                    ValueAndDescAre("Candidate4", ""),
                    ValueAndDescAre("Candidate3", ""),
                    ValueAndDescAre("Candidate2", ""),
                    ValueAndDescAre("Candidate1", ""),
                    ValueAndDescAre("今日", ""),
                    // The candidates generated by the date rewriter.
                    ValueAndDescAre("2011/04/18", kDesc),
                    ValueAndDescAre("2011-04-18", kDesc),
                    ValueAndDescAre("2011年4月18日", kDesc),
                    ValueAndDescAre("平成23年4月18日", kDesc),
                    ValueAndDescAre("月曜日", kDesc),
                }));

    // If the segment contains 5 candidates and the rewriter target is at
    // index 0, the rewriter adds candidates at index 3.
    InitSegment("きょう", "今日", &segments);
    InsertCandidate("Candidate1", "Candidate1", 1, segments.mutable_segment(0));
    InsertCandidate("Candidate2", "Candidate2", 1, segments.mutable_segment(0));
    InsertCandidate("Candidate3", "Candidate3", 1, segments.mutable_segment(0));
    InsertCandidate("Candidate4", "Candidate4", 1, segments.mutable_segment(0));

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0),
                CandidatesAreArray({
                    ValueAndDescAre("今日", ""),
                    ValueAndDescAre("Candidate4", ""),
                    ValueAndDescAre("Candidate3", ""),
                    // The candidates generated by the date rewriter.
                    ValueAndDescAre("2011/04/18", kDesc),
                    ValueAndDescAre("2011-04-18", kDesc),
                    ValueAndDescAre("2011年4月18日", kDesc),
                    ValueAndDescAre("平成23年4月18日", kDesc),
                    ValueAndDescAre("月曜日", kDesc),
                    ValueAndDescAre("Candidate2", ""),
                    ValueAndDescAre("Candidate1", ""),
                }));
  }

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(DateRewriterTest, ADToERA) {
  EXPECT_TRUE(DateRewriter::AdToEra(0, 1).empty());

  // AD.645 is "大化元(年)"
  EXPECT_THAT(DateRewriter::AdToEra(645, 1), ElementsAre("大化元"));

  // AD.646 is "大化2(年)" or "大化二(年)"
  EXPECT_THAT(DateRewriter::AdToEra(646, 1), ElementsAre("大化2", "大化二"));

  // AD.1976 is "昭和51(年)" or "昭和五十一(年)"
  EXPECT_THAT(DateRewriter::AdToEra(1976, 1),
              ElementsAre("昭和51", "昭和五十一"));

  // AD.1989 is "昭和64(年)" or "昭和六四(年)" or "平成元(年)"
  EXPECT_THAT(DateRewriter::AdToEra(1989, 1),
              ElementsAre("平成元", "昭和64", "昭和六十四"));

  // AD.1990 is "平成2(年)" or "平成(二)年"
  EXPECT_THAT(DateRewriter::AdToEra(1990, 1), ElementsAre("平成2", "平成二"));

  // 2 courts era.
  // AD.1331 "元徳3(年)" or "元弘元(年)"
  EXPECT_THAT(DateRewriter::AdToEra(1331, 1),
              ElementsAre("元弘元", "元徳3", "元徳三"));

  // AD.1393 "明徳4(年)" or "明徳四(年)"
  EXPECT_THAT(DateRewriter::AdToEra(1393, 1), ElementsAre("明徳4", "明徳四"));

  // AD.1375
  // South: "文中4(年)" or "文中四(年)", "天授元(年)"
  // North: "応安8(年)" or "応安八(年)", "永和元(年)"
  EXPECT_THAT(
      DateRewriter::AdToEra(1375, 1),
      ElementsAre("天授元", "文中4", "文中四", "永和元", "応安8", "応安八"));

  // AD.1332
  // South: "元弘2(年)" or "元弘二(年)"
  // North: "正慶元(年)", "元徳4(年)" or "元徳四(年)"
  EXPECT_THAT(DateRewriter::AdToEra(1332, 1),
              ElementsAre("元弘2", "元弘二", "正慶元", "元徳4", "元徳四"));

  // AD.1333
  // South: "元弘3" or "元弘三(年)"
  // North: "正慶2" or "正慶二(年)"
  EXPECT_THAT(DateRewriter::AdToEra(1333, 1),
              ElementsAre("元弘3", "元弘三", "正慶2", "正慶二"));

  // AD.1334
  // South: "元弘4" or "元弘四(年)", "建武元"
  // North: "正慶3" or "正慶三(年)", "建武元(deduped)"
  EXPECT_THAT(DateRewriter::AdToEra(1334, 1),
              ElementsAre("建武元", "元弘4", "元弘四", "正慶3", "正慶三"));

  // AD.1997
  // "平成九年"
  EXPECT_THAT(DateRewriter::AdToEra(1997, 1), ElementsAre("平成9", "平成九"));

  // AD.2011
  // "平成二十三年"
  EXPECT_THAT(DateRewriter::AdToEra(2011, 1),
              ElementsAre("平成23", "平成二十三"));

  // AD.2019
  // Show both "平成三十一年", "令和元年" when month is not specified.
  EXPECT_THAT(DateRewriter::AdToEra(2019, 0),
              ElementsAre("令和元", "平成31", "平成三十一"));

  // Changes the era depending on the month.
  for (int m = 1; m <= 4; ++m) {
    EXPECT_THAT(DateRewriter::AdToEra(2019, m),
                ElementsAre("平成31", "平成三十一"));
  }
  for (int m = 5; m <= 12; ++m) {
    EXPECT_THAT(DateRewriter::AdToEra(2019, m), ElementsAre("令和元"));
  }

  // AD.2020
  EXPECT_THAT(DateRewriter::AdToEra(2020, 1), ElementsAre("令和2", "令和二"));

  // AD.2030
  EXPECT_THAT(DateRewriter::AdToEra(2030, 1),
              ElementsAre("令和12", "令和十二"));

  // AD.1998
  // "平成十年" or "平成10年"
  EXPECT_THAT(DateRewriter::AdToEra(1998, 1), ElementsAre("平成10", "平成十"));

  // Negative Test
  // Too big number or negative number input are expected false return
  EXPECT_FALSE(DateRewriter::AdToEra(2020, 1).empty());
  EXPECT_FALSE(DateRewriter::AdToEra(2100, 1).empty());
  EXPECT_TRUE(DateRewriter::AdToEra(2201, 1).empty());
  EXPECT_TRUE(DateRewriter::AdToEra(-100, 1).empty());
}

struct EraToAdTestData {
  std::string key;
  std::vector<std::pair<std::string, std::string>> results;

  EraToAdTestData WithSuffix() const {
    EraToAdTestData with_suffix;
    with_suffix.key = key + "ねん";
    for (const auto &result : results) {
      with_suffix.results.push_back(
          std::make_pair(result.first + "年", result.second + "年"));
    }
    return with_suffix;
  }
} era_to_ad_test_data[] = {
    {"たいか1", {{"六四五", "大化1"}, {"６４５", "大化1"}, {"645", "大化1"}}},
    {"たいか2", {{"六四六", "大化2"}, {"６４６", "大化2"}, {"646", "大化2"}}},
    // "しょうわ2ねん" is AD.1313 or AD.1927.
    {"しょうわ2",
     {{"一三一三", "正和2"},
      {"１３１３", "正和2"},
      {"1313", "正和2"},
      {"一九二七", "昭和2"},
      {"１９２７", "昭和2"},
      {"1927", "昭和2"}}},
    // North court tests.
    {"げんとく1",
     {{"一三二九", "元徳1"}, {"１３２９", "元徳1"}, {"1329", "元徳1"}}},
    {"めいとく3",
     {{"一三九二", "明徳3"}, {"１３９２", "明徳3"}, {"1392", "明徳3"}}},
    {"けんむ1",
     {{"一三三四", "建武1"}, {"１３３４", "建武1"}, {"1334", "建武1"}}},
    // Big number tests.
    {"しょうわ80",
     {{"一三九一", "正和80"},
      {"１３９１", "正和80"},
      {"1391", "正和80"},
      {"二〇〇五", "昭和80"},
      {"２００５", "昭和80"},
      {"2005", "昭和80"}}},
    {"たいしょう101",
     {{"二〇一二", "大正101"}, {"２０１２", "大正101"}, {"2012", "大正101"}}},
    // "元年" test.
    {"れいわがん",
     {{"二〇一九", "令和元"}, {"２０１９", "令和元"}, {"2019", "令和元"}}},
    {"へいせいがん",
     {{"一九八九", "平成元"}, {"１９８９", "平成元"}, {"1989", "平成元"}}},
    // "しょうわがんねん" is AD.1926 or AD.1312.
    {"しょうわがん",
     {{"一三一二", "正和元"},
      {"１３１２", "正和元"},
      {"1312", "正和元"},
      {"一九二六", "昭和元"},
      {"１９２６", "昭和元"},
      {"1926", "昭和元"}}},
};

class EraToAdTest : public ::testing::TestWithParam<EraToAdTestData> {};
INSTANTIATE_TEST_SUITE_P(DateRewriterTest, EraToAdTest,
                         ::testing::ValuesIn(era_to_ad_test_data));

TEST_P(EraToAdTest, WithSuffix) {
  const auto data = GetParam().WithSuffix();
  EXPECT_EQ(DateRewriter::EraToAd(data.key), data.results);
}

TEST_P(EraToAdTest, WithoutSuffix) {
  const auto &data = GetParam();
  EXPECT_EQ(DateRewriter::EraToAd(data.key), data.results);
}

TEST_F(DateRewriterTest, EraToAdEmpty) {
  EXPECT_TRUE(DateRewriter::EraToAd("").empty());
}

// Negative Tests.
// 0 or negative number input are expected false return.
TEST_F(DateRewriterTest, EraToAdNegative) {
  EXPECT_TRUE(DateRewriter::EraToAd("しょうわ-1ねん").empty());
  EXPECT_TRUE(DateRewriter::EraToAd("しょうわ-1").empty());
  EXPECT_TRUE(DateRewriter::EraToAd("しょうわ0ねん").empty());
  EXPECT_TRUE(DateRewriter::EraToAd("しょうわ0").empty());
  EXPECT_TRUE(DateRewriter::EraToAd("0ねん").empty());
  EXPECT_TRUE(DateRewriter::EraToAd("0").empty());
}

TEST_F(DateRewriterTest, ConvertTime) {
  EXPECT_THAT(DateRewriter::ConvertTime(0, 0),
              ElementsAre("0:00", "0時00分", "午前0時0分"));
  EXPECT_THAT(DateRewriter::ConvertTime(9, 9),
              ElementsAre("9:09", "9時09分", "午前9時9分"));
  EXPECT_THAT(DateRewriter::ConvertTime(11, 59),
              ElementsAre("11:59", "11時59分", "午前11時59分"));
  EXPECT_THAT(DateRewriter::ConvertTime(12, 0),
              ElementsAre("12:00", "12時00分", "午後0時0分"));
  EXPECT_THAT(DateRewriter::ConvertTime(12, 1),
              ElementsAre("12:01", "12時01分", "午後0時1分"));
  EXPECT_THAT(DateRewriter::ConvertTime(19, 23),
              ElementsAre("19:23", "19時23分", "午後7時23分"));
  EXPECT_THAT(DateRewriter::ConvertTime(25, 23),
              ElementsAre("25:23", "25時23分", "午前1時23分"));

  // "18:30,18時30分、18時半、午後6時30分、午後6時半"
  // And the order of results is must be above.
  EXPECT_THAT(
      DateRewriter::ConvertTime(18, 30),
      ElementsAre("18:30", "18時30分", "18時半", "午後6時30分", "午後6時半"));

  EXPECT_TRUE(DateRewriter::ConvertTime(-10, 20).empty());
  EXPECT_TRUE(DateRewriter::ConvertTime(10, -20).empty());
  EXPECT_TRUE(DateRewriter::ConvertTime(80, 20).empty());
  EXPECT_TRUE(DateRewriter::ConvertTime(20, 80).empty());
  EXPECT_TRUE(DateRewriter::ConvertTime(30, 80).empty());
}

TEST_F(DateRewriterTest, ConvertDateTest) {
  DateRewriter rewriter;

  EXPECT_THAT(DateRewriter::ConvertDateWithYear(2011, 4, 17),
              ElementsAre("2011/04/17", "2011-04-17", "2011年4月17日"));

  // January, March, May, July, August, October, December has 31 days April,
  // June, September, November has 30 days February is dealt as a special case,
  // see below:
  const struct {
    int month;
    int days;
  } month_days_test_data[] = {{1, 31},  {3, 31},  {4, 30}, {5, 31},
                              {6, 30},  {7, 31},  {8, 31}, {9, 30},
                              {10, 31}, {11, 30}, {12, 31}};
  for (const auto &test_case : month_days_test_data) {
    std::vector<std::string> results;
    EXPECT_TRUE(DateRewriter::ConvertDateWithYear(2001, test_case.month,
                                                  test_case.days, &results));
    EXPECT_FALSE(rewriter.ConvertDateWithYear(2001, test_case.month,
                                              test_case.days + 1, &results));
  }

  // 4 dividable year is leap year.
  EXPECT_THAT(DateRewriter::ConvertDateWithYear(2004, 2, 29),
              ElementsAre("2004/02/29", "2004-02-29", "2004年2月29日"));

  // Non 4 dividable year is not leap year.
  std::vector<std::string> results;
  EXPECT_FALSE(DateRewriter::ConvertDateWithYear(1999, 2, 29, &results));

  // However, 100 dividable year is not leap year.
  EXPECT_FALSE(DateRewriter::ConvertDateWithYear(1900, 2, 29, &results));

  // Furthermore, 400 dividable year is leap year.
  EXPECT_THAT(DateRewriter::ConvertDateWithYear(2000, 2, 29),
              ElementsAre("2000/02/29", "2000-02-29", "2000年2月29日"));

  EXPECT_FALSE(DateRewriter::ConvertDateWithYear(0, 1, 1, &results));
  EXPECT_FALSE(DateRewriter::ConvertDateWithYear(2000, 13, 1, &results));
  EXPECT_FALSE(DateRewriter::ConvertDateWithYear(2000, 1, 41, &results));
  EXPECT_FALSE(DateRewriter::ConvertDateWithYear(2000, 13, 41, &results));
  EXPECT_FALSE(DateRewriter::ConvertDateWithYear(2000, 0, 1, &results));
  EXPECT_FALSE(DateRewriter::ConvertDateWithYear(2000, 1, 0, &results));
  EXPECT_FALSE(DateRewriter::ConvertDateWithYear(2000, 0, 0, &results));
}

TEST_F(DateRewriterTest, NumberRewriterTest) {
  Segments segments;
  DateRewriter rewriter;
  const commands::Request request;
  const config::Config config;
  const composer::Composer composer(request, config);
  const ConversionRequest conversion_request =
      ConversionRequestBuilder().SetComposer(composer).Build();

  // Not targets of rewrite.
  const char *kNonTargetCases[] = {
      "",      "0",      "1",   "01234", "00000",  // Invalid number of digits.
      "hello", "123xyz",                           // Not numbers.
      "660",   "999",    "3400"                    // Neither date nor time.
  };
  for (const char *input : kNonTargetCases) {
    InitSegment(input, input, &segments);
    EXPECT_FALSE(rewriter.Rewrite(conversion_request, &segments))
        << "Input: " << input << "\nSegments: " << segments.DebugString();
  }

// Macro for {"M/D", "日付"}
#define DATE(month, day) {#month "/" #day, "日付"}

// Macro for {"M月D日", "日付"}
#define KANJI_DATE(month, day) {#month "月" #day "日", "日付"}

// Macro for {"H:M", "時刻"}
#define TIME(hour, minute) {#hour ":" #minute, "時刻"}

// Macro for {"H時M分", "時刻"}
#define KANJI_TIME(hour, minute) {#hour "時" #minute "分", "時刻"}

// Macro for {"H時半", "時刻"}
#define KANJI_TIME_HAN(hour) {#hour "時半", "時刻"}

// Macro for {"午前H時M分", "時刻"}
#define GOZEN(hour, minute) {"午前" #hour "時" #minute "分", "時刻"}

// Macro for {"午後H時M分", "時刻"}
#define GOGO(hour, minute) {"午後" #hour "時" #minute "分", "時刻"}

// Macro for {"午前H時半", "時刻"}
#define GOZEN_HAN(hour) {"午前" #hour "時半", "時刻"}

// Macro for {"午後H時半", "時刻"}
#define GOGO_HAN(hour) {"午後" #hour "時半", "時刻"}

  // Targets of rewrite.
  using ValueAndDescription = std::pair<const char *, const char *>;
  const std::vector<ValueAndDescription> kTestCases[] = {
      // Two digits.
      {
          {"00", ""},
          KANJI_TIME(0, 0),
          GOZEN(0, 0),
          GOGO(0, 0),
      },
      {
          {"01", ""},
          KANJI_TIME(0, 1),
          GOZEN(0, 1),
          GOGO(0, 1),
      },
      {
          {"10", ""},
          KANJI_TIME(1, 0),
          GOZEN(1, 0),
          GOGO(1, 0),
      },
      {
          {"11", ""},
          DATE(1, 1),
          KANJI_DATE(1, 1),
          KANJI_TIME(1, 1),
          GOZEN(1, 1),
          GOGO(1, 1),
      },

      // Three digits.
      {
          {"000", ""},
          TIME(0, 00),
          KANJI_TIME(0, 00),
          GOZEN(0, 00),
          GOGO(0, 00),
      },
      {
          {"001", ""},
          TIME(0, 01),
          KANJI_TIME(0, 01),
          GOZEN(0, 01),
          GOGO(0, 01),
      },
      {
          {"010", ""},
          TIME(0, 10),
          KANJI_TIME(0, 10),
          GOZEN(0, 10),
          GOGO(0, 10),
      },
      {
          {"011", ""},
          TIME(0, 11),
          KANJI_TIME(0, 11),
          GOZEN(0, 11),
          GOGO(0, 11),
      },
      {
          {"100", ""},
          TIME(1, 00),
          KANJI_TIME(1, 00),
          KANJI_TIME(10, 0),
          GOZEN(1, 00),
          GOGO(1, 00),
          GOZEN(10, 0),
          GOGO(10, 0),
      },
      {
          {"101", ""},
          DATE(10, 1),
          TIME(1, 01),
          KANJI_DATE(10, 1),
          KANJI_TIME(1, 01),
          KANJI_TIME(10, 1),
          GOZEN(1, 01),
          GOGO(1, 01),
          GOZEN(10, 1),
          GOGO(10, 1),
      },
      {
          {"110", ""},
          DATE(1, 10),
          TIME(1, 10),
          KANJI_DATE(1, 10),
          KANJI_TIME(1, 10),
          KANJI_TIME(11, 0),
          GOZEN(1, 10),
          GOGO(1, 10),
          GOZEN(11, 0),
          GOGO(11, 0),
      },
      {
          {"111", ""},
          DATE(1, 11),
          DATE(11, 1),
          TIME(1, 11),
          KANJI_DATE(1, 11),
          KANJI_DATE(11, 1),
          KANJI_TIME(1, 11),
          KANJI_TIME(11, 1),
          GOZEN(1, 11),
          GOGO(1, 11),
          GOZEN(11, 1),
          GOGO(11, 1),
      },
      {
          {"130", ""},
          DATE(1, 30),
          TIME(1, 30),
          KANJI_DATE(1, 30),
          KANJI_TIME(1, 30),
          KANJI_TIME_HAN(1),
          KANJI_TIME(13, 0),
          GOZEN(1, 30),
          GOZEN_HAN(1),
          GOGO(1, 30),
          GOGO_HAN(1),
      },

      // Four digits.
      {
          {"0000", ""},
          TIME(00, 00),
          KANJI_TIME(00, 00),
      },
      {
          {"0010", ""},
          TIME(00, 10),
          KANJI_TIME(00, 10),
      },
      {
          {"0100", ""},
          TIME(01, 00),
          KANJI_TIME(01, 00),
      },
      {
          {"1000", ""},
          TIME(10, 00),
          KANJI_TIME(10, 00),
          GOZEN(10, 00),
          GOGO(10, 00),
      },
      {
          {"0011", ""},
          TIME(00, 11),
          KANJI_TIME(00, 11),
      },
      {
          {"0101", ""},
          DATE(01, 01),
          TIME(01, 01),
          KANJI_TIME(01, 01),
      },
      {
          {"1001", ""},
          DATE(10, 01),
          TIME(10, 01),
          KANJI_TIME(10, 01),
          GOZEN(10, 01),
          GOGO(10, 01),
      },
      {
          {"0110", ""},
          DATE(01, 10),
          TIME(01, 10),
          KANJI_TIME(01, 10),
      },
      {
          {"1010", ""},
          DATE(10, 10),
          TIME(10, 10),
          KANJI_DATE(10, 10),
          KANJI_TIME(10, 10),
          GOZEN(10, 10),
          GOGO(10, 10),
      },
      {
          {"1100", ""},
          TIME(11, 00),
          KANJI_TIME(11, 00),
          GOZEN(11, 00),
          GOGO(11, 00),
      },
      {
          {"0111", ""},
          DATE(01, 11),
          TIME(01, 11),
          KANJI_TIME(01, 11),
      },
      {
          {"1011", ""},
          DATE(10, 11),
          TIME(10, 11),
          KANJI_DATE(10, 11),
          KANJI_TIME(10, 11),
          GOZEN(10, 11),
          GOGO(10, 11),
      },
      {
          {"1101", ""},
          DATE(11, 01),
          TIME(11, 01),
          KANJI_TIME(11, 01),
          GOZEN(11, 01),
          GOGO(11, 01),
      },
      {
          {"1110", ""},
          DATE(11, 10),
          TIME(11, 10),
          KANJI_DATE(11, 10),
          KANJI_TIME(11, 10),
          GOZEN(11, 10),
          GOGO(11, 10),
      },
      {
          {"1111", ""},
          DATE(11, 11),
          TIME(11, 11),
          KANJI_DATE(11, 11),
          KANJI_TIME(11, 11),
          GOZEN(11, 11),
          GOGO(11, 11),
      },
      {
          {"0030", ""},
          TIME(00, 30),
          KANJI_TIME(00, 30),
      },
      {
          {"0130", ""},
          DATE(01, 30),
          TIME(01, 30),
          KANJI_TIME(01, 30),
      },
      {
          {"1030", ""},
          DATE(10, 30),
          TIME(10, 30),
          KANJI_DATE(10, 30),
          KANJI_TIME(10, 30),
          KANJI_TIME_HAN(10),
          GOZEN(10, 30),
          GOZEN_HAN(10),
          GOGO(10, 30),
          GOGO_HAN(10),
      },
      {
          {"1130", ""},
          DATE(11, 30),
          TIME(11, 30),
          KANJI_DATE(11, 30),
          KANJI_TIME(11, 30),
          KANJI_TIME_HAN(11),
          GOZEN(11, 30),
          GOZEN_HAN(11),
          GOGO(11, 30),
          GOGO_HAN(11),
      },
      {
          {"1745", ""},
          TIME(17, 45),
          KANJI_TIME(17, 45),
      },
      {
          {"2730", ""},
          TIME(27, 30),
          KANJI_TIME(27, 30),
          KANJI_TIME_HAN(27),
      },
  };

#undef DATE
#undef GOGO
#undef GOGO_HAN
#undef GOZEN
#undef GOZEN_HAN
#undef KANJI_DATE
#undef KANJI_TIME
#undef KANJI_TIME_HAN
#undef TIME

  constexpr auto ValueAndDescAre =
      [](absl::string_view value,
         absl::string_view desc) -> Matcher<const Segment::Candidate *> {
    return Pointee(AllOf(Field(&Segment::Candidate::value, value),
                         Field(&Segment::Candidate::description, desc)));
  };
  for (const auto &test_case : kTestCases) {
    // Convert expected outputs to matchers.
    std::vector<Matcher<const Segment::Candidate *>> matchers;
    for (const auto &[value, desc] : test_case) {
      matchers.push_back(ValueAndDescAre(value, desc));
    }

    InitSegment(test_case[0].first, test_case[0].first, &segments);
    EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));
    ASSERT_EQ(1, segments.segments_size());
    EXPECT_THAT(segments.segment(0), CandidatesAreArray(matchers));
  }
}

TEST_F(DateRewriterTest, NumberRewriterFromRawInputTest) {
  Segments segments;
  DateRewriter rewriter;

  composer::Table table;
  table.AddRule("222", "c", "");
  table.AddRule("3", "d", "");
  const commands::Request request;
  const config::Config config;
  composer::Composer composer(table, request, config);

  // Key sequence : 2223
  // Preedit : cd
  // In this case date/time candidates should be created from 2223.
  {
    InitSegment("cd", "cd", &segments);
    composer.Reset();
    composer.InsertCharacter("2223");
    const ConversionRequest conv_request =
        ConversionRequestBuilder().SetComposer(composer).Build();

    EXPECT_TRUE(rewriter.Rewrite(conv_request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0), ContainsCandidate(ValueIs("22:23")));
  }

  // Key sequence : 2223
  // Preedit : 1111
  // Meta candidate(HALF_ASCII)
  // Preedit should be prioritized over key sequence.
  {
    InitSegment("1111", "1111", &segments);
    composer.Reset();
    composer.InsertCharacter("2223");
    const ConversionRequest conv_request =
        ConversionRequestBuilder().SetComposer(composer).Build();
    EXPECT_TRUE(rewriter.Rewrite(conv_request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0), ContainsCandidate(ValueIs("11:11")));
    EXPECT_THAT(segments.segment(0), Not(ContainsCandidate(ValueIs("22:23"))));
  }

  // Key sequence : 2223
  // Preedit : cd
  // HALF_ASCII meta candidate: 1111
  // In this case meta candidates should be prioritized.
  {
    InitSegment("cd", "cd", &segments);
    Segment::Candidate *meta_candidate =
        segments.mutable_conversion_segment(0)->add_meta_candidate();
    meta_candidate->value = "1111";
    composer.InsertCharacter("2223");
    composer.Reset();
    const ConversionRequest conv_request =
        ConversionRequestBuilder().SetComposer(composer).Build();
    EXPECT_TRUE(rewriter.Rewrite(conv_request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0), ContainsCandidate(ValueIs("11:11")));
    EXPECT_THAT(segments.segment(0), Not(ContainsCandidate(ValueIs("22:23"))));
  }
}

TEST_F(DateRewriterTest, MobileEnvironmentTest) {
  commands::Request request;
  DateRewriter rewriter;

  {
    request.set_mixed_conversion(true);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter.capability(convreq), RewriterInterface::ALL);
  }

  {
    request.set_mixed_conversion(false);
    const ConversionRequest convreq =
        ConversionRequestBuilder().SetRequest(request).Build();
    EXPECT_EQ(rewriter.capability(convreq), RewriterInterface::CONVERSION);
  }
}

TEST_F(DateRewriterTest, ConsecutiveDigitsInsertPositionTest) {
  commands::Request request;
  const config::Config config;
  const composer::Composer composer(request, config);

  // Init an instance of Segments for this test.
  Segments test_segments;
  InitSegment("1234", "1234", &test_segments);
  InsertCandidate("cand1", "cand1", 1, test_segments.mutable_segment(0));
  InsertCandidate("cand2", "cand2", 2, test_segments.mutable_segment(0));

  // Test case where results are inserted after the top candidate.
  {
    request.set_special_romanji_table(
        commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII);
    const ConversionRequest conversion_request = ConversionRequestBuilder()
                                                     .SetComposer(composer)
                                                     .SetRequest(request)
                                                     .Build();

    DateRewriter rewriter;
    Segments segments = test_segments;
    EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));

    // Verify that the top candidate wasn't modified and the next two were
    // moved to last.
    const auto &segment = segments.segment(0);
    const auto cand_size = segment.candidates_size();
    ASSERT_LT(3, cand_size);
    EXPECT_EQ(segment.candidate(0).value, "1234");
    EXPECT_EQ(segment.candidate(cand_size - 2).value, "cand1");
    EXPECT_EQ(segment.candidate(cand_size - 1).value, "cand2");
  }

  // Test case where results are inserted after the last candidate.
  {
    request.set_special_romanji_table(
        commands::Request::TOGGLE_FLICK_TO_HIRAGANA);
    const ConversionRequest conversion_request = ConversionRequestBuilder()
                                                     .SetComposer(composer)
                                                     .SetRequest(request)
                                                     .Build();

    DateRewriter rewriter;
    Segments segments = test_segments;
    EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));

    // Verify that the first three candidate weren't moved.
    const auto &segment = segments.segment(0);
    const auto cand_size = segment.candidates_size();
    ASSERT_LT(3, cand_size);
    EXPECT_EQ(segment.candidate(0).value, "1234");
    EXPECT_EQ(segment.candidate(1).value, "cand1");
    EXPECT_EQ(segment.candidate(2).value, "cand2");
  }
}

TEST_F(DateRewriterTest, ConsecutiveDigitsFromMetaCandidates) {
  commands::Request request;
  const config::Config config;
  const composer::Composer composer(request, config);
  const ConversionRequest conversion_request =
      ConversionRequestBuilder().SetComposer(composer).Build();

  Segments segments;
  InitSegment("nisen", "にせん", &segments);

  Segment *segment = segments.mutable_conversion_segment(0);
  segment->add_meta_candidate()->value = "２０００";

  DateRewriter rewriter;
  EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));
  EXPECT_THAT(segments.segment(0), ContainsCandidate(ValueIs("20:00")));
}

TEST_F(DateRewriterTest, ConsecutiveDigitsWithMinusSign) {
  commands::Request request;
  const config::Config config;
  const composer::Composer composer(request, config);
  const ConversionRequest conversion_request =
      ConversionRequestBuilder().SetComposer(composer).Build();

  // Init an instance of Segments for this test.
  Segments segments;
  InitSegment("-123", "−１２３", &segments);

  Segment *segment = segments.mutable_conversion_segment(0);
  // Hiragana: ー is prolonged sound mark (U+2212)
  segment->add_meta_candidate()->value = "ー１２３";
  // Half Ascii: - is hyphen-minus (U+002D)
  segment->add_meta_candidate()->value = "-123";
  // Full Ascii: − is minus (U+30FC)
  segment->add_meta_candidate()->value = "−１２３";
  // Half Katakana: ｰ is half prolonged sound mark (U+FF70)
  segment->add_meta_candidate()->value = "ｰ123";

  // No rewrite is expected.
  DateRewriter rewriter;
  EXPECT_FALSE(rewriter.Rewrite(conversion_request, &segments));
}

TEST_F(DateRewriterTest, ConsecutiveDigitsInsertPositionWithHistory) {
  commands::Request request;
  const config::Config config;
  const composer::Composer composer(request, config);
  const ConversionRequest conversion_request =
      ConversionRequestBuilder().SetComposer(composer).Build();

  Segments segments;

  // If there's a history segment containing N candidates where N is greater
  // than the number of candidates in the current conversion segment, crash
  // happened in Segment::insert_candidate().  This is a regression test for
  // it.

  // History segment
  InitSegment("hist", "hist", &segments);
  Segment *seg = segments.mutable_segment(0);
  InsertCandidate("hist1", "hist1", 1, seg);
  InsertCandidate("hist2", "hist2", 1, seg);
  InsertCandidate("hist3", "hist3", 1, seg);
  seg->set_segment_type(Segment::HISTORY);

  // Conversion segment
  AppendSegment("11", "11", &segments);
  seg = segments.mutable_segment(1);
  InsertCandidate("cand1", "cand1", 1, seg);
  InsertCandidate("cand2", "cand2", 2, seg);

  // Rewrite is successful with a history segment.
  DateRewriter rewriter;
  EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));
  ASSERT_LT(3, segments.conversion_segment(0).candidates_size());
}

TEST_F(DateRewriterTest, ExtraFormatTest) {
  ClockMock clock(ParseTimeOrDie("2011-04-18T15:06:31Z"));
  Clock::SetClockForUnitTest(&clock);

  MockDictionary dictionary;
  EXPECT_CALL(dictionary,
              LookupExact(StrEq(DateRewriter::kExtraFormatKey), _, _))
      .WillOnce(InvokeCallbackWithUserDictionaryToken{"{YEAR}{MONTH}{DATE}"});

  MockConverter converter;
  DateRewriter rewriter(dictionary);

  Segments segments;
  InitSegment("きょう", "今日", &segments);

  const ConversionRequest request;
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));

  ASSERT_EQ(segments.segments_size(), 1);
  constexpr absl::string_view kDesc = "今日の日付";
  EXPECT_THAT(segments.segment(0),
              CandidatesAreArray({
                  ValueAndDescAre("今日", ""),
                  ValueAndDescAre("20110418", kDesc),  // Custom format
                  ValueAndDescAre("2011/04/18", kDesc),
                  ValueAndDescAre("2011-04-18", kDesc),
                  ValueAndDescAre("2011年4月18日", kDesc),
                  ValueAndDescAre("平成23年4月18日", kDesc),
                  ValueAndDescAre("月曜日", kDesc),
              }));
  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(DateRewriterTest, ExtraFormatSyntaxTest) {
  ClockMock clock(ParseTimeOrDie("2011-04-18T15:06:31Z"));
  Clock::SetClockForUnitTest(&clock);

  auto syntax_test = [](const absl::string_view input,
                        const absl::string_view output) {
    MockDictionary dictionary;
    EXPECT_CALL(dictionary,
                LookupExact(StrEq(DateRewriter::kExtraFormatKey), _, _))
        .WillOnce(InvokeCallbackWithUserDictionaryToken{std::string(input)});
    MockConverter converter;
    DateRewriter rewriter(dictionary);
    Segments segments;
    InitSegment("きょう", "今日", &segments);
    const ConversionRequest request;
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    ASSERT_EQ(segments.segments_size(), 1);
    EXPECT_THAT(segments.segment(0),
                ContainsCandidate(Field(&Segment::Candidate::value, output)));
  };

  syntax_test("%", "%");    // Single % (illformat)
  syntax_test("%%", "%%");  // Double
  syntax_test("%Y", "%Y");  // %Y remains as-is.
  syntax_test("{{}", "{");  // {{} is converted to {.
  syntax_test("{{}}}", "{}}");
  syntax_test("{}", "{}");
  syntax_test("{{}YEAR}", "{YEAR}");
  syntax_test("{MOZC}", "{MOZC}");  // invalid keyword.
  syntax_test("{year}", "{year}");  // upper case only.

  // If the format is empty, it is ignored.
  // "2011/04/18" is the default first conversion.
  syntax_test("", "2011/04/18");
  Clock::SetClockForUnitTest(nullptr);
}

struct RewriteAdData {
  std::vector<std::pair<std::string, std::string>> segments;
  size_t segment_index = 0;
  std::string candidate;
  std::string resized_key;
};

class RewriteAdTest : public DateRewriterTest,
                      public ::testing::WithParamInterface<RewriteAdData> {};

INSTANTIATE_TEST_SUITE_P(
    DateRewriterTest, RewriteAdTest,
    Values(
        // One segment, the most basic case.
        RewriteAdData{{{"へいせい23ねん", "平成23年"}}, 0, "2011年"},
        RewriteAdData{{{"2011ねん", "2011年"}}, 0, "平成23年"},
        // The `value` should be ignored when rewriting.
        RewriteAdData{{{"へいせい23ねん", "兵勢23年"}}, 0, "2011年"},
        RewriteAdData{{{"へいせい23ねん", "兵勢23念"}}, 0, "2011年"},
        RewriteAdData{{{"2011ねん", "2011念"}}, 0, "平成23年"},
        // Invalid era name.
        RewriteAdData{{{"ああ23ねん", "ああ23年"}}, 0, ""},
        // One segment, with preceding and following segments.
        RewriteAdData{{{"きょうは", "今日は"},
                       {"へいせい23ねん", "平成23年"},
                       {"です", "です"}},
                      1,
                      "2011年"},
        RewriteAdData{
            {{"きょうは", "今日は"}, {"2011ねん", "2011年"}, {"です", "です"}},
            1,
            "平成23年"},
        // The "年" suffix in the following segment. They don't need resizing,
        // and the result shouldn't contain the "年" suffix.
        RewriteAdData{{{"へいせい23", "平成23"}, {"ねん", "年"}}, 0, "2011"},
        RewriteAdData{{{"2011", "2011"}, {"ねん", "年"}}, 0, "平成23"},
        RewriteAdData{{{"2011", "二千十一"}, {"ねん", "年"}}, 0, "平成23"},
        RewriteAdData{{{"きょうは", "今日は"},
                       {"へいせい23", "平成23"},
                       {"ねん", "年"},
                       {"です", "です"}},
                      1,
                      "2011"},
        RewriteAdData{{{"きょうは", "今日は"},
                       {"2011", "2011"},
                       {"ねん", "年"},
                       {"です", "です"}},
                      1,
                      "平成23"},
        // Multiple segments.
        RewriteAdData{{{"へいせい", "平成"}, {"23ねん", "23年"}},
                      0,
                      "",
                      "へいせい23ねん"},
        RewriteAdData{{{"へいせい", "平成"}, {"23", "23"}, {"ねん", "年"}},
                      0,
                      "",
                      "へいせい23ねん"},
        // Reject more than 3 segments.
        RewriteAdData{
            {{"へい", "平"}, {"せい", "成"}, {"23", "23"}, {"ねん", "年"}}},
        // The `value` should be ignored when merging too.
        RewriteAdData{{{"へいせい", "兵勢"}, {"23ねん", "23年"}},
                      0,
                      "",
                      "へいせい23ねん"},
        // Multiple segments with preceding and following segments.
        RewriteAdData{{{"きょうは", "今日は"},
                       {"へいせい", "平成"},
                       {"23", "23"},
                       {"ねん", "年"},
                       {"です", "です"}},
                      1,
                      "",
                      "へいせい23ねん"},
        // Extra characters in the segment of "nen".
        RewriteAdData{
            {{"へいせい23ねんです", "平成23年です"}}, 0, "", "へいせい23ねん"},
        RewriteAdData{
            {{"きょうは", "今日は"}, {"へいせい23ねんです", "平成23年です"}},
            1,
            "",
            "へいせい23ねん"},
        RewriteAdData{{{"きょうは", "今日は"},
                       {"へいせい", "平成"},
                       {"23", "23"},
                       {"ねんです", "年です"}},
                      1,
                      "",
                      "へいせい23ねん"}));

TEST_P(RewriteAdTest, MockConverter) {
  const RewriteAdData &data = GetParam();
  MockDictionary dictionary;
  DateRewriter rewriter(dictionary);
  Segments segments;
  for (const auto &[key, value] : data.segments) {
    AppendSegment(key, value, &segments);
  }
  const ConversionRequest request;

  std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
      rewriter.CheckResizeSegmentsRequest(request, segments);

  if (data.resized_key.empty()) {
    // Resize is not expected.
    EXPECT_FALSE(resize_request.has_value());

    if (data.candidate.empty()) {
      // Rewrite is not expected.
      EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    } else {
      // Rewrite is expected.
      EXPECT_TRUE(rewriter.Rewrite(request, &segments));
      const Segment &segment = segments.segment(data.segment_index);
      EXPECT_THAT(segment, ContainsCandidate(ValueIs(data.candidate)));
    }

  } else {
    // Resize is expected.
    EXPECT_TRUE(resize_request.has_value());
    EXPECT_EQ(resize_request->segment_index, data.segment_index);
    EXPECT_EQ(resize_request->segment_sizes[0],
              Util::CharsLen(data.resized_key));
  }
}

// Test if `Segments::set_resized(true)` prevents merging segments.
TEST_F(DateRewriterTest, RewriteAdResizedSegments) {
  MockDictionary dictionary;
  DateRewriter rewriter(dictionary);
  Segments segments;
  InitSegment("へいせい", "平成", &segments);
  AppendSegment("23", "23", &segments);
  AppendSegment("ねん", "年", &segments);
  const ConversionRequest request;
  segments.set_resized(true);

  std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
      rewriter.CheckResizeSegmentsRequest(request, segments);
  EXPECT_FALSE(resize_request.has_value());

  segments.set_resized(false);
  resize_request = rewriter.CheckResizeSegmentsRequest(request, segments);
  EXPECT_TRUE(resize_request.has_value());
}

}  // namespace mozc
