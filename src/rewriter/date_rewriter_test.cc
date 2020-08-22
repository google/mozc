// Copyright 2010-2020, Google Inc.
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
#include <memory>

#include "base/clock.h"
#include "base/clock_mock.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "dictionary/dictionary_mock.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"

namespace mozc {
namespace {

void Expect2Results(const std::vector<std::string> &src,
                    const std::string &exp1, const std::string &exp2) {
  EXPECT_EQ(2, src.size());
  EXPECT_NE(src[0], src[1]);
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(src[i] == exp1 || src[i] == exp2);
  }
}

void Expect3Results(const std::vector<std::string> &src,
                    const std::string &exp1, const std::string &exp2,
                    const std::string &exp3) {
  EXPECT_EQ(3, src.size());
  EXPECT_NE(src[0], src[1]);
  EXPECT_NE(src[1], src[2]);
  EXPECT_NE(src[2], src[0]);
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(src[i] == exp1 || src[i] == exp2 || src[i] == exp3);
  }
}

void Expect4Results(const std::vector<std::string> &src,
                    const std::string &exp1, const std::string &exp2,
                    const std::string &exp3, const std::string &exp4) {
  EXPECT_EQ(4, src.size());
  EXPECT_NE(src[0], src[1]);
  EXPECT_NE(src[0], src[2]);
  EXPECT_NE(src[0], src[3]);
  EXPECT_NE(src[1], src[2]);
  EXPECT_NE(src[1], src[3]);
  EXPECT_NE(src[2], src[3]);
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(src[i] == exp1 || src[i] == exp2 || src[i] == exp3 ||
                src[i] == exp4);
  }
}

void Expect5Results(const std::vector<std::string> &src,
                    const std::string &exp1, const std::string &exp2,
                    const std::string &exp3, const std::string &exp4,
                    const std::string &exp5) {
  EXPECT_EQ(5, src.size());
  EXPECT_NE(src[0], src[1]);
  EXPECT_NE(src[0], src[2]);
  EXPECT_NE(src[0], src[3]);
  EXPECT_NE(src[0], src[4]);
  EXPECT_NE(src[1], src[2]);
  EXPECT_NE(src[1], src[3]);
  EXPECT_NE(src[1], src[4]);
  EXPECT_NE(src[2], src[3]);
  EXPECT_NE(src[2], src[4]);
  EXPECT_NE(src[3], src[4]);
  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(src[i] == exp1 || src[i] == exp2 || src[i] == exp3 ||
                src[i] == exp4 || src[i] == exp5);
  }
}

void InitCandidate(const std::string &key, const std::string &value,
                   Segment::Candidate *candidate) {
  candidate->content_key = key;
  candidate->value = value;
  candidate->content_value = value;
}

void AppendSegment(const std::string &key, const std::string &value,
                   Segments *segments) {
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  InitCandidate(key, value, seg->add_candidate());
}

void InitSegment(const std::string &key, const std::string &value,
                 Segments *segments) {
  segments->Clear();
  AppendSegment(key, value, segments);
}

void InsertCandidate(const std::string &key, const std::string &value,
                     const int position, Segment *segment) {
  Segment::Candidate *cand = segment->insert_candidate(position);
  cand->content_key = key;
  cand->value = value;
  cand->content_value = value;
}

int CountDescription(const Segments &segments, const std::string &description) {
  int num = 0;
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    if (segments.segment(0).candidate(i).description == description) {
      ++num;
    }
  }
  return num;
}

bool ContainCandidate(const Segments &segments, const std::string &candidate) {
  const Segment &segment = segments.segment(0);

  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (candidate == segment.candidate(i).value) {
      return true;
    }
  }
  return false;
}

std::string GetNthCandidateValue(const Segments &segments, const int n) {
  const Segment &segment = segments.segment(0);
  return segment.candidate(n).value;
}

bool IsStringContained(const std::string &key,
                       const std::vector<std::string> &container) {
  for (size_t i = 0; i < container.size(); ++i) {
    if (key == container[i]) {
      return true;
    }
  }
  return false;
}

bool AllElementsAreSame(const std::string &key,
                        const std::vector<std::string> &container) {
  for (size_t i = 0; i < container.size(); ++i) {
    if (key != container[i]) {
      return false;
    }
  }
  return true;
}

// "2011-04-18 15:06:31 (Mon)" UTC
const uint64 kTestSeconds = 1303139191uLL;
// micro seconds. it is random value.
const uint32 kTestMicroSeconds = 588377u;

}  // namespace

class DateRewriterTest : public ::testing::Test {
 private:
  const mozc::testing::ScopedTmpUserProfileDirectory scoped_tmp_profile_dir_;
};

TEST_F(DateRewriterTest, DateRewriteTest) {
  std::unique_ptr<ClockMock> mock_clock(
      new ClockMock(kTestSeconds, kTestMicroSeconds));
  Clock::SetClockForUnitTest(mock_clock.get());

  DateRewriter rewriter;
  Segments segments;
  const ConversionRequest request;

  {
    InitSegment("きょう", "今日", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(segments, "今日の日付"));
    EXPECT_TRUE(ContainCandidate(segments, "平成23年4月18日"));
    EXPECT_TRUE(ContainCandidate(segments, "2011年4月18日"));
    EXPECT_TRUE(ContainCandidate(segments, "2011-04-18"));
    EXPECT_TRUE(ContainCandidate(segments, "2011/04/18"));
    EXPECT_TRUE(ContainCandidate(segments, "月曜日"));
  }
  {
    InitSegment("あした", "明日", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(segments, "明日の日付"));
    EXPECT_TRUE(ContainCandidate(segments, "平成23年4月19日"));
    EXPECT_TRUE(ContainCandidate(segments, "2011年4月19日"));
    EXPECT_TRUE(ContainCandidate(segments, "2011-04-19"));
    EXPECT_TRUE(ContainCandidate(segments, "2011/04/19"));
    EXPECT_TRUE(ContainCandidate(segments, "火曜日"));
  }
  {
    InitSegment("きのう", "昨日", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(segments, "昨日の日付"));
    EXPECT_TRUE(ContainCandidate(segments, "平成23年4月17日"));
    EXPECT_TRUE(ContainCandidate(segments, "2011年4月17日"));
    EXPECT_TRUE(ContainCandidate(segments, "2011-04-17"));
    EXPECT_TRUE(ContainCandidate(segments, "2011/04/17"));
    EXPECT_TRUE(ContainCandidate(segments, "日曜日"));
  }
  {
    InitSegment("あさって", "明後日", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(segments, "明後日の日付"));
    EXPECT_TRUE(ContainCandidate(segments, "平成23年4月20日"));
    EXPECT_TRUE(ContainCandidate(segments, "2011年4月20日"));
    EXPECT_TRUE(ContainCandidate(segments, "2011-04-20"));
    EXPECT_TRUE(ContainCandidate(segments, "2011/04/20"));
    EXPECT_TRUE(ContainCandidate(segments, "水曜日"));
  }
  {
    InitSegment("にちじ", "日時", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(1, CountDescription(segments, "現在の日時"));
    EXPECT_TRUE(ContainCandidate(segments, "2011/04/18 15:06"));
  }
  {
    InitSegment("いま", "今", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(3, CountDescription(segments, "現在の時刻"));
    EXPECT_TRUE(ContainCandidate(segments, "15:06"));
    EXPECT_TRUE(ContainCandidate(segments, "15時06分"));
    EXPECT_TRUE(ContainCandidate(segments, "午後3時6分"));
  }

  // DateRewrite candidate order check.
  {
    // This parameter is copied from date_rewriter.cc.
    const size_t kMinimumDateCandidateIdx = 3;
    const char *kTodayCandidate[] = {"2011/04/18", "2011-04-18",
                                     "2011年4月18日",
                                     "平成23年"
                                     "4月18日",
                                     "月曜日"};

    // If initial count of candidate is 1, date rewrited candidate start from 1.
    // "きょう", "今日"
    InitSegment("きょう", "今日", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(segments, "今日の日付"));
    size_t offset = 1;
    for (int rel_cand_idx = 0; rel_cand_idx < arraysize(kTodayCandidate);
         ++rel_cand_idx) {
      EXPECT_EQ(kTodayCandidate[rel_cand_idx],
                GetNthCandidateValue(segments, rel_cand_idx + offset));
    }

    // If initial count of candidate is 5 and target candidate is located at
    // index 4, date rewrited candidate start from 5.
    // "きょう", "今日"
    InitSegment("きょう", "今日", &segments);

    // Inserts no meaning candidates into segment.
    InsertCandidate("Candidate1", "Candidate1", 0, segments.mutable_segment(0));
    InsertCandidate("Candidate2", "Candidate2", 0, segments.mutable_segment(0));
    InsertCandidate("Candidate3", "Candidate3", 0, segments.mutable_segment(0));
    InsertCandidate("Candidate4", "Candidate4", 0, segments.mutable_segment(0));

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(segments, "今日の日付"));

    offset = 5;
    for (int rel_cand_idx = 0; rel_cand_idx < arraysize(kTodayCandidate);
         ++rel_cand_idx) {
      EXPECT_EQ(kTodayCandidate[rel_cand_idx],
                GetNthCandidateValue(segments, rel_cand_idx + offset));
    }

    // If initial count of candidate is 5 and target candidate is located at
    // index 0, date rewrited candidate start from kMinimumDateCandidateIdx.
    // "きょう", "今日"
    InitSegment("きょう", "今日", &segments);

    // Inserts no meaning candidates into segment.
    InsertCandidate("Candidate1", "Candidate1", 1, segments.mutable_segment(0));
    InsertCandidate("Candidate2", "Candidate2", 1, segments.mutable_segment(0));
    InsertCandidate("Candidate3", "Candidate3", 1, segments.mutable_segment(0));
    InsertCandidate("Candidate4", "Candidate4", 1, segments.mutable_segment(0));

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(segments, "今日の日付"));

    for (int rel_cand_idx = 0; rel_cand_idx < arraysize(kTodayCandidate);
         ++rel_cand_idx) {
      EXPECT_EQ(kTodayCandidate[rel_cand_idx],
                GetNthCandidateValue(segments,
                                     rel_cand_idx + kMinimumDateCandidateIdx));
    }
  }

  Clock::SetClockForUnitTest(nullptr);
}

TEST_F(DateRewriterTest, ADToERA) {
  DateRewriter rewriter;
  std::vector<std::string> results;
  const ConversionRequest request;

  results.clear();
  rewriter.AdToEra(0, &results);
  EXPECT_EQ(results.size(), 0);

  // AD.645 is "大化元(年)"
  results.clear();
  rewriter.AdToEra(645, &results);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0], "大化元");

  // AD.646 is "大化2(年)" or "大化二(年)"
  results.clear();
  rewriter.AdToEra(646, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results, "大化2", "大化二");

  // AD.1976 is "昭和51(年)" or "昭和五十一(年)"
  results.clear();
  rewriter.AdToEra(1976, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results, "昭和51", "昭和五十一");

  // AD.1989 is "昭和64(年)" or "昭和六四(年)" or "平成元(年)"
  results.clear();
  rewriter.AdToEra(1989, &results);
  Expect3Results(results, "昭和64", "昭和六十四", "平成元");

  // AD.1990 is "平成2(年)" or "平成(二)年"
  results.clear();
  rewriter.AdToEra(1990, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results, "平成2", "平成二");

  // 2 courts era.
  // AD.1331 "元徳3(年)" or "元弘元(年)"
  results.clear();
  rewriter.AdToEra(1331, &results);
  Expect3Results(results, "元徳3", "元徳三", "元弘元");

  // AD.1393 "明徳4(年)" or "明徳四(年)"
  results.clear();
  rewriter.AdToEra(1393, &results);
  Expect2Results(results, "明徳4", "明徳四");

  // AD.1375
  // South: "文中4(年)" or "文中四(年)", "天授元(年)"
  // North: "応安8(年)" or "応安八(年)", "永和元(年)"
  results.clear();
  rewriter.AdToEra(1375, &results);
  // just checks number.
  EXPECT_EQ(results.size(), 6);

  // AD.1332
  // South: "元弘2(年)" or "元弘二(年)"
  // North: "正慶元(年)", "元徳4(年)" or "元徳四(年)"
  results.clear();
  rewriter.AdToEra(1332, &results);
  EXPECT_EQ(results.size(), 5);
  Expect5Results(results, "元弘2", "元弘二", "正慶元", "元徳4", "元徳四");
  // AD.1333
  // South: "元弘3" or "元弘三(年)"
  // North: "正慶2" or "正慶二(年)"
  results.clear();
  rewriter.AdToEra(1333, &results);
  Expect4Results(results, "元弘3", "元弘三", "正慶2", "正慶二");

  // AD.1334
  // South: "元弘4" or "元弘四(年)", "建武元"
  // North: "正慶3" or "正慶三(年)", "建武元(deduped)"
  results.clear();
  rewriter.AdToEra(1334, &results);
  EXPECT_EQ(results.size(), 5);
  Expect5Results(results, "元弘4", "元弘四", "建武元", "正慶三", "正慶3");

  // AD.1997
  // "平成九年"
  results.clear();
  rewriter.AdToEra(1997, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results, "平成9", "平成九");

  // AD.2011
  // "平成二十三年"
  results.clear();
  rewriter.AdToEra(2011, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results, "平成23", "平成二十三");

  // AD.2019
  // Show both "平成三十一年", "令和元年" when month is not specified.
  results.clear();
  rewriter.AdToEra(2019, 0, &results);
  EXPECT_EQ(results.size(), 3);
  Expect3Results(results, "令和元", "平成31", "平成三十一");

  // Changes the era depending on the month.
  for (int m = 1; m <= 4; ++m) {
    results.clear();
    rewriter.AdToEra(2019, m, &results);
    EXPECT_EQ(results.size(), 2);
    Expect2Results(results, "平成31", "平成三十一");
  }

  for (int m = 5; m <= 12; ++m) {
    results.clear();
    rewriter.AdToEra(2019, m, &results);
    EXPECT_EQ(results.size(), 1);
    EXPECT_EQ(results[0], "令和元");
  }

  // AD.2020
  results.clear();
  rewriter.AdToEra(2020, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results, "令和2", "令和二");

  // AD.2030
  results.clear();
  rewriter.AdToEra(2030, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results, "令和12", "令和十二");

  // AD.1998
  // "平成十年" or "平成10年"
  results.clear();
  rewriter.AdToEra(1998, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results, "平成10", "平成十");

  // Negative Test
  // Too big number or negative number input are expected false return
  results.clear();
  EXPECT_TRUE(rewriter.AdToEra(2020, &results));
  EXPECT_TRUE(rewriter.AdToEra(2100, &results));
  EXPECT_FALSE(rewriter.AdToEra(2201, &results));
  EXPECT_FALSE(rewriter.AdToEra(-100, &results));
}

TEST_F(DateRewriterTest, ERAToAD) {
  DateRewriter rewriter;
  std::vector<std::string> results, descriptions;
  const ConversionRequest request;
  // "1234", "１２３４", "一二三四"
  const int kNumYearRepresentation = 3;

  results.clear();
  descriptions.clear();
  rewriter.EraToAd("", &results, &descriptions);
  EXPECT_EQ(0, results.size());
  EXPECT_EQ(0, descriptions.size());

  // "たいか1ねん" is "645年" or "６４５年" or "六四五年"
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("たいか1ねん", &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  Expect3Results(results, "645年", "六四五年", "６４５年");
  EXPECT_TRUE(AllElementsAreSame("大化1年", descriptions));

  // "たいか2ねん" is "646年" or "６４６年" or "六四六年"
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("たいか2ねん", &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  Expect3Results(results, "646年", "６４６年", "六四六年");
  EXPECT_TRUE(AllElementsAreSame("大化2年", descriptions));

  // "しょうわ2ねん" is AD.1313 or AD.1927
  // "1313年", "１３１３年", "一三一三年"
  // "1927年", "１９２７年", "一九二七年"
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("しょうわ2ねん", &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation * 2, results.size());
  EXPECT_EQ(kNumYearRepresentation * 2, descriptions.size());

  for (int i = 0; i < kNumYearRepresentation; ++i) {
    EXPECT_EQ("正和2年", descriptions[i]);
    EXPECT_EQ("昭和2年", descriptions[i + kNumYearRepresentation]);
  }
  std::vector<std::string> first(results.begin(),
                                 results.begin() + kNumYearRepresentation);
  std::vector<std::string> second(results.begin() + kNumYearRepresentation,
                                  results.end());
  EXPECT_TRUE(IsStringContained("1313年", first));
  EXPECT_TRUE(IsStringContained("１３１３年", first));
  EXPECT_TRUE(IsStringContained("一三一三年", first));
  EXPECT_TRUE(IsStringContained("1927年", second));
  EXPECT_TRUE(IsStringContained("１９２７年", second));
  EXPECT_TRUE(IsStringContained("一九二七年", second));

  // North court test
  // "げんとく1ねん" is AD.1329
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("げんとく1ねん", &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  EXPECT_TRUE(IsStringContained("1329年", results));
  // "めいとく3ねん" is AD.1392
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("めいとく3ねん", &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  EXPECT_TRUE(IsStringContained("1392年", results));
  // "けんむ1ねん" is AD.1334 (requires dedupe)
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("けんむ1ねん", &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  EXPECT_TRUE(IsStringContained("1334年", results));

  // Big number test
  // "昭和80年" is AD.2005
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("しょうわ80ねん", &results, &descriptions));
  EXPECT_TRUE(IsStringContained("2005年", results));
  // "大正101年" is AD.2012
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("たいしょう101ねん", &results, &descriptions));
  EXPECT_TRUE(IsStringContained("2012年", results));

  // "元年" test
  // "れいわがんねん" is AD.2019
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("れいわがんねん", &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  EXPECT_TRUE(IsStringContained("2019年", results));
  EXPECT_TRUE(IsStringContained("２０１９年", results));
  EXPECT_TRUE(IsStringContained("二〇一九年", results));
  EXPECT_TRUE(AllElementsAreSame("令和元年", descriptions));

  // "元年" test
  // "へいせいがんねん" is AD.1989
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("へいせいがんねん", &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  EXPECT_TRUE(IsStringContained("1989年", results));
  EXPECT_TRUE(IsStringContained("１９８９年", results));
  EXPECT_TRUE(IsStringContained("一九八九年", results));
  EXPECT_TRUE(AllElementsAreSame("平成元年", descriptions));

  // "しょうわがんねん" is AD.1926 or AD.1312
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("しょうわがんねん", &results, &descriptions));
  EXPECT_EQ(2 * kNumYearRepresentation, results.size());
  EXPECT_EQ(2 * kNumYearRepresentation, descriptions.size());
  EXPECT_TRUE(IsStringContained("1926年", results));
  EXPECT_TRUE(IsStringContained("１９２６年", results));
  EXPECT_TRUE(IsStringContained("一九二六年", results));
  EXPECT_TRUE(IsStringContained("1312年", results));
  EXPECT_TRUE(IsStringContained("１３１２年", results));
  EXPECT_TRUE(IsStringContained("一三一二年", results));

  // Negative Test
  // 0 or negative number input are expected false return
  results.clear();
  descriptions.clear();
  EXPECT_FALSE(rewriter.EraToAd("しょうわ-1ねん", &results, &descriptions));
  EXPECT_FALSE(rewriter.EraToAd("しょうわ0ねん", &results, &descriptions));
  EXPECT_FALSE(rewriter.EraToAd("0ねん", &results, &descriptions));
  EXPECT_EQ(0, results.size());
  EXPECT_EQ(0, descriptions.size());
}

TEST_F(DateRewriterTest, ConvertTime) {
  DateRewriter rewriter;
  std::vector<std::string> results;
  const ConversionRequest request;

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(0, 0, &results));
  Expect3Results(results, "0:00", "0時00分", "午前0時0分");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(9, 9, &results));
  Expect3Results(results, "9:09", "9時09分", "午前9時9分");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(11, 59, &results));
  Expect3Results(results, "11:59", "11時59分", "午前11時59分");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(12, 0, &results));
  Expect3Results(results, "12:00", "12時00分", "午後0時0分");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(12, 1, &results));
  Expect3Results(results, "12:01", "12時01分", "午後0時1分");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(19, 23, &results));
  Expect3Results(results, "19:23", "19時23分", "午後7時23分");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(25, 23, &results));
  Expect3Results(results, "25:23", "25時23分", "午前1時23分");

  results.clear();

  // "18:30,18時30分、18時半、午後6時30分、午後6時半"
  // And the order of results is must be above.
  EXPECT_TRUE(rewriter.ConvertTime(18, 30, &results));
  ASSERT_EQ(5, results.size());
  EXPECT_EQ("18:30", results[0]);
  EXPECT_EQ("18時30分", results[1]);
  EXPECT_EQ("18時半", results[2]);
  EXPECT_EQ("午後6時30分", results[3]);
  EXPECT_EQ("午後6時半", results[4]);
  results.clear();

  EXPECT_FALSE(rewriter.ConvertTime(-10, 20, &results));
  EXPECT_FALSE(rewriter.ConvertTime(10, -20, &results));
  EXPECT_FALSE(rewriter.ConvertTime(80, 20, &results));
  EXPECT_FALSE(rewriter.ConvertTime(20, 80, &results));
  EXPECT_FALSE(rewriter.ConvertTime(30, 80, &results));
}

TEST_F(DateRewriterTest, ConvertDateTest) {
  DateRewriter rewriter;
  std::vector<std::string> results;

  results.clear();
  EXPECT_TRUE(rewriter.ConvertDateWithYear(2011, 4, 17, &results));
  ASSERT_EQ(3, results.size());
  EXPECT_TRUE(IsStringContained("2011年4月17日", results));
  EXPECT_TRUE(IsStringContained("2011-04-17", results));
  EXPECT_TRUE(IsStringContained("2011/04/17", results));

  // January, March, May, July, Auguest, October, December has 31 days
  // April, June, September, November has 30 days
  // February is dealt as a special case, see below
  const struct {
    int month;
    int days;
  } month_days_test_data[] = {{1, 31},  {3, 31},  {4, 30}, {5, 31},
                              {6, 30},  {7, 31},  {8, 31}, {9, 30},
                              {10, 31}, {11, 30}, {12, 31}};

  for (size_t i = 0; i < arraysize(month_days_test_data); ++i) {
    EXPECT_TRUE(
        rewriter.ConvertDateWithYear(2001, month_days_test_data[i].month,
                                     month_days_test_data[i].days, &results));
    EXPECT_FALSE(rewriter.ConvertDateWithYear(
        2001, month_days_test_data[i].month, month_days_test_data[i].days + 1,
        &results));
  }

  // 4 dividable year is leap year.
  results.clear();
  EXPECT_TRUE(rewriter.ConvertDateWithYear(2004, 2, 29, &results));
  ASSERT_EQ(3, results.size());
  EXPECT_TRUE(IsStringContained("2004年2月29日", results));
  EXPECT_TRUE(IsStringContained("2004-02-29", results));
  EXPECT_TRUE(IsStringContained("2004/02/29", results));

  // Non 4 dividable year is not leap year.
  EXPECT_FALSE(rewriter.ConvertDateWithYear(1999, 2, 29, &results));

  // However, 100 dividable year is not leap year.
  EXPECT_FALSE(rewriter.ConvertDateWithYear(1900, 2, 29, &results));

  // Furthermore, 400 dividable year is leap year.
  results.clear();
  EXPECT_TRUE(rewriter.ConvertDateWithYear(2000, 2, 29, &results));
  ASSERT_EQ(3, results.size());
  EXPECT_TRUE(IsStringContained("2000年2月29日", results));
  EXPECT_TRUE(IsStringContained("2000-02-29", results));
  EXPECT_TRUE(IsStringContained("2000/02/29", results));

  EXPECT_FALSE(rewriter.ConvertDateWithYear(0, 1, 1, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 13, 1, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 1, 41, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 13, 41, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 0, 1, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 1, 0, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 0, 0, &results));
}

TEST_F(DateRewriterTest, NumberRewriterTest) {
  Segments segments;
  DateRewriter rewriter;
  const commands::Request request;
  const config::Config config;
  const composer::Composer composer(nullptr, &request, &config);
  const ConversionRequest conversion_request(&composer, &request, &config);

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
#define DATE(month, day) \
  { #month "/" #day, "日付" }

// Macro for {"M月D日", "日付"}
#define KANJI_DATE(month, day) \
  { #month "月" #day "日", "日付" }

// Macro for {"H:M", "時刻"}
#define TIME(hour, minute) \
  { #hour ":" #minute, "時刻" }

// Macro for {"H時M分", "時刻"}
#define KANJI_TIME(hour, minute) \
  { #hour "時" #minute "分", "時刻" }

// Macro for {"H時半", "時刻"}
#define KANJI_TIME_HAN(hour) \
  { #hour "時半", "時刻" }

// Macro for {"午前H時M分", "時刻"}
#define GOZEN(hour, minute) \
  { "午前" #hour "時" #minute "分", "時刻" }

// Macro for {"午後H時M分", "時刻"}
#define GOGO(hour, minute) \
  { "午後" #hour "時" #minute "分", "時刻" }

// Macro for {"午前H時半", "時刻"}
#define GOZEN_HAN(hour) \
  { "午前" #hour "時半", "時刻" }

// Macro for {"午後H時半", "時刻"}
#define GOGO_HAN(hour) \
  { "午後" #hour "時半", "時刻" }

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

  for (const auto &test_case : kTestCases) {
    InitSegment(test_case[0].first, test_case[0].first, &segments);
    EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));
    ASSERT_EQ(1, segments.segments_size());
    const auto &segment = segments.segment(0);
    EXPECT_EQ(test_case.size(), segment.candidates_size())
        << segment.DebugString();
    if (test_case.size() != segment.candidates_size()) {
      continue;
    }
    for (std::size_t i = 0; i < test_case.size(); ++i) {
      EXPECT_EQ(test_case[i].first, segment.candidate(i).value)
          << "Value of " << i << "-th candidate is unexpected:\n"
          << segment.DebugString();
      EXPECT_EQ(test_case[i].second, segment.candidate(i).description)
          << "Description of " << i << "-th candidate is unexpected:\n"
          << segment.DebugString();
    }
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
  composer::Composer composer(&table, &request, &config);
  ConversionRequest conversion_request;
  conversion_request.set_composer(&composer);

  // Key sequence : 2223
  // Preedit : cd
  // In this case date/time candidates should be created from 2223.
  {
    InitSegment("cd", "cd", &segments);
    composer.Reset();
    composer.InsertCharacter("2223");
    EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "22:23"));
  }

  // Key sequence : 2223
  // Preedit : 1111
  // Meta candidate(HALF_ASCII)
  // Preedit should be prioritized over key sequence.
  {
    InitSegment("1111", "1111", &segments);
    composer.Reset();
    composer.InsertCharacter("2223");
    EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "11:11"));
    EXPECT_FALSE(ContainCandidate(segments, "22:23"));
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
    EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "11:11"));
    EXPECT_FALSE(ContainCandidate(segments, "22:23"));
  }
}

TEST_F(DateRewriterTest, MobileEnvironmentTest) {
  ConversionRequest convreq;
  commands::Request request;
  convreq.set_request(&request);
  DateRewriter rewriter;

  {
    request.set_mixed_conversion(true);
    EXPECT_EQ(RewriterInterface::ALL, rewriter.capability(convreq));
  }

  {
    request.set_mixed_conversion(false);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter.capability(convreq));
  }
}

TEST_F(DateRewriterTest, RewriteYearTest) {
  DateRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  InitSegment("2010", "2010", &segments);
  AppendSegment("nenn", "年", &segments);
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  EXPECT_TRUE(ContainCandidate(segments, "平成22"));
}

// This test treats the situation that if UserHistoryRewriter or other like
// Rewriter moves up a candidate which is actually a number but can not be
// converted integer easily.
TEST_F(DateRewriterTest, RelationWithUserHistoryRewriterTest) {
  DateRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  InitSegment("2011", "二千十一", &segments);
  AppendSegment("nenn", "年", &segments);
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  EXPECT_TRUE(ContainCandidate(segments, "平成23"));
}

TEST_F(DateRewriterTest, ConsecutiveDigitsInsertPositionTest) {
  commands::Request request;
  const config::Config config;
  const composer::Composer composer(nullptr, &request, &config);
  const ConversionRequest conversion_request(&composer, &request, &config);

  // Init an instance of Segments for this test.
  Segments test_segments;
  InitSegment("1234", "1234", &test_segments);
  InsertCandidate("cand1", "cand1", 1, test_segments.mutable_segment(0));
  InsertCandidate("cand2", "cand2", 2, test_segments.mutable_segment(0));

  // Test case where results are inserted after the top candidate.
  {
    request.set_special_romanji_table(
        commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII);

    DateRewriter rewriter;
    Segments segments;
    segments.CopyFrom(test_segments);
    EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));

    // Verify that the top candidate wans't modified and the next two were moved
    // to last.
    const auto &segment = segments.segment(0);
    const auto cand_size = segment.candidates_size();
    ASSERT_LT(3, cand_size);
    EXPECT_EQ("1234", segment.candidate(0).value);
    EXPECT_EQ("cand1", segment.candidate(cand_size - 2).value);
    EXPECT_EQ("cand2", segment.candidate(cand_size - 1).value);
  }

  // Test case where results are inserted after the last candidate.
  {
    request.set_special_romanji_table(
        commands::Request::TWELVE_KEYS_TO_HIRAGANA);

    DateRewriter rewriter;
    Segments segments;
    segments.CopyFrom(test_segments);
    EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));

    // Verify that the first three candidate weren't moved.
    const auto &segment = segments.segment(0);
    const auto cand_size = segment.candidates_size();
    ASSERT_LT(3, cand_size);
    EXPECT_EQ("1234", segment.candidate(0).value);
    EXPECT_EQ("cand1", segment.candidate(1).value);
    EXPECT_EQ("cand2", segment.candidate(2).value);
  }
}

TEST_F(DateRewriterTest, ConsecutiveDigitsInsertPositionWithHistory) {
  commands::Request request;
  const config::Config config;
  const composer::Composer composer(nullptr, &request, &config);
  const ConversionRequest conversion_request(&composer, &request, &config);

  Segments segments;

  // If there's a history segment containing N candidates where N is greater
  // than the number of candidates in the current conversion segment, crash
  // happened in Segment::insert_candidate().  This is a regression test for it.

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

}  // namespace mozc
