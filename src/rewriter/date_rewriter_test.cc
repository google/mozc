// Copyright 2010-2014, Google Inc.
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

#include "base/clock_mock.h"
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

void Expect2Results(const vector<string> &src,
                    const string &exp1,
                    const string &exp2) {
  EXPECT_EQ(2, src.size());
  EXPECT_NE(src[0], src[1]);
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(src[i] == exp1 || src[i] == exp2);
  }
}

void Expect3Results(const vector<string> &src,
                    const string &exp1,
                    const string &exp2,
                    const string &exp3) {
  EXPECT_EQ(3, src.size());
  EXPECT_NE(src[0], src[1]);
  EXPECT_NE(src[1], src[2]);
  EXPECT_NE(src[2], src[0]);
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(src[i] == exp1 || src[i] == exp2 || src[i] == exp3);
  }
}

void Expect4Results(const vector<string> &src,
                    const string &exp1,
                    const string &exp2,
                    const string &exp3,
                    const string &exp4) {
  EXPECT_EQ(4, src.size());
  EXPECT_NE(src[0], src[1]);
  EXPECT_NE(src[0], src[2]);
  EXPECT_NE(src[0], src[3]);
  EXPECT_NE(src[1], src[2]);
  EXPECT_NE(src[1], src[3]);
  EXPECT_NE(src[2], src[3]);
  for (int i = 0; i < 4; ++i) {
    EXPECT_TRUE(src[i] == exp1 || src[i] == exp2 ||
                src[i] == exp3 || src[i] == exp4);
  }
}

void Expect5Results(const vector<string> &src,
                    const string &exp1,
                    const string &exp2,
                    const string &exp3,
                    const string &exp4,
                    const string &exp5) {
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
    EXPECT_TRUE(src[i] == exp1 || src[i] == exp2 ||
                src[i] == exp3 || src[i] == exp4 || src[i] == exp5);
  }
}

void InitCandidate(const string &key, const string &value,
                   Segment::Candidate *candidate) {
  candidate->content_key = key;
  candidate->value = value;
  candidate->content_value = value;
}

void AppendSegment(const string &key, const string &value,
                   Segments *segments) {
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  InitCandidate(key, value, seg->add_candidate());
}

void InitSegment(const string &key, const string &value,
                 Segments *segments) {
  segments->Clear();
  AppendSegment(key, value, segments);
}

void InitMetaSegment(const string &key, const string &value,
                     Segments *segments) {
  segments->Clear();
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  InitCandidate(key, value, seg->add_meta_candidate());
}

void InsertCandidate(const string &key,
                const string &value,
                const int position,
                Segment *segment) {
  Segment::Candidate *cand = segment->insert_candidate(position);
  cand->content_key = key;
  cand->value = value;
  cand->content_value = value;
}

int CountDescription(const Segments &segments, const string &description) {
  int num = 0;
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    if (segments.segment(0).candidate(i).description == description) {
        ++num;
     }
  }
  return num;
}

bool ContainCandidate(const Segments &segments, const string &candidate) {
  const Segment &segment = segments.segment(0);

  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (candidate == segment.candidate(i).value) {
      return true;
    }
  }
  return false;
}

string GetNthCandidateValue(const Segments &segments, const int n) {
  const Segment &segment = segments.segment(0);
  return segment.candidate(n).value;
}

bool IsStringContained(const string &key, const vector<string> &container) {
  for (size_t i = 0; i < container.size(); ++i) {
    if (key == container[i]) {
      return true;
    }
  }
  return false;
}

bool AllElementsAreSame(const string &key, const vector<string> &container) {
  for (size_t i = 0; i < container.size(); ++i) {
    if (key != container[i]) {
      return false;
    }
  }
  return true;
}

// "2011-04-18 15:06:31 (Mon)" UTC
const uint64 kTestSeconds =  1303139191uLL;
// micro seconds. it is random value.
const uint32 kTestMicroSeconds = 588377u;

}  // namespace

class DateRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);
  }

  virtual void TearDown() {
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);
  }
};

TEST_F(DateRewriterTest, DateRewriteTest) {
  scoped_ptr<ClockMock> mock_clock(
      new ClockMock(kTestSeconds, kTestMicroSeconds));
  Util::SetClockHandler(mock_clock.get());

  DateRewriter rewriter;
  Segments segments;
  const ConversionRequest request;

  // "きょう/今日/今日の日付"
  {
    InitSegment("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
                "\xE4\xBB\x8A\xE6\x97\xA5", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(
        segments,
        "\xE4\xBB\x8A\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));

    // "平成23年4月18日"
    EXPECT_TRUE(ContainCandidate(segments,
        "\xE5\xB9\xB3\xE6\x88\x90" "23" "\xE5\xB9\xB4"
        "4" "\xE6\x9C\x88"
        "18" "\xE6\x97\xA5"));
    // "2011年4月18日"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011" "\xE5\xB9\xB4"
        "4" "\xE6\x9C\x88"
        "18" "\xE6\x97\xA5"));
    // "2011-04-18"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011-04-18"));
    // "2011/04/18"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011/04/18"));
    // "月曜日"
    EXPECT_TRUE(ContainCandidate(segments,
        "\xE6\x9C\x88" "\xE6\x9B\x9C\xE6\x97\xA5"));
  }

  // "あした/明日/明日の日付"
  {
    InitSegment("\xE3\x81\x82\xE3\x81\x97\xE3\x81\x9F",
                "\xE6\x98\x8E\xE6\x97\xA5", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(
        segments,
        "\xE6\x98\x8E\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));

    // "平成23年4月19日"
    EXPECT_TRUE(ContainCandidate(segments,
        "\xE5\xB9\xB3\xE6\x88\x90" "23" "\xE5\xB9\xB4"
        "4" "\xE6\x9C\x88"
        "19" "\xE6\x97\xA5"));
    // "2011年4月19日"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011" "\xE5\xB9\xB4"
        "4" "\xE6\x9C\x88"
        "19" "\xE6\x97\xA5"));
    // "2011-04-19"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011-04-19"));
    // "2011/04/19"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011/04/19"));
    // "火曜日"
    EXPECT_TRUE(ContainCandidate(segments,
        "\xE7\x81\xAB" "\xE6\x9B\x9C\xE6\x97\xA5"));
  }

  // "きのう/昨日/昨日の日付"
  {
    InitSegment("\xE3\x81\x8D\xE3\x81\xAE\xE3\x81\x86",
                "\xE6\x98\xA8\xE6\x97\xA5", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(
        segments,
        "\xE6\x98\xA8\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));

    // "平成23年4月17日"
    EXPECT_TRUE(ContainCandidate(segments,
        "\xE5\xB9\xB3\xE6\x88\x90" "23" "\xE5\xB9\xB4"
        "4" "\xE6\x9C\x88"
        "17" "\xE6\x97\xA5"));
    // "2011年4月17日"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011" "\xE5\xB9\xB4"
        "4" "\xE6\x9C\x88"
        "17" "\xE6\x97\xA5"));
    // "2011-04-17"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011-04-17"));
    // "2011/04/17"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011/04/17"));
    // "日曜日"
    EXPECT_TRUE(ContainCandidate(segments,
        "\xE6\x97\xA5" "\xE6\x9B\x9C\xE6\x97\xA5"));
  }

  // "あさって/明後日/明後日の日付"
  {
    InitSegment("\xE3\x81\x82\xE3\x81\x95\xE3\x81\xA3\xE3\x81\xA6",
                "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(
        segments,
        "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));

    // "平成23年4月20日"
    EXPECT_TRUE(ContainCandidate(segments,
        "\xE5\xB9\xB3\xE6\x88\x90" "23" "\xE5\xB9\xB4"
        "4" "\xE6\x9C\x88"
        "20" "\xE6\x97\xA5"));
    // "2011年4月20日"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011" "\xE5\xB9\xB4"
        "4" "\xE6\x9C\x88"
        "20" "\xE6\x97\xA5"));
    // "2011-04-20"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011-04-20"));
    // "2011/04/20"
    EXPECT_TRUE(ContainCandidate(segments,
        "2011/04/20"));
    // "水曜日"
    EXPECT_TRUE(ContainCandidate(segments,
        "\xE6\xB0\xB4" "\xE6\x9B\x9C\xE6\x97\xA5"));
  }

  // "にちじ/日時/現在の日時"
  {
    InitSegment("\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\x98",
                "\xE6\x97\xA5\xE6\x99\x82", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(1, CountDescription(
        segments,
        "\xE7\x8F\xBE\xE5\x9C\xA8\xE3\x81\xAE\xE6\x97\xA5\xE6\x99\x82"));

    // "2011/04/18 15:06"
    EXPECT_TRUE(ContainCandidate(segments, "2011/04/18 15:06"));
  }

  // "いま/今/現在の時刻"
  {
    InitSegment("\xE3\x81\x84\xE3\x81\xBE",
                "\xE4\xBB\x8A", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(3, CountDescription(
        segments,
        "\xE7\x8F\xBE\xE5\x9C\xA8\xE3\x81\xAE\xE6\x99\x82\xE5\x88\xBB"));

    // "15:06"
    EXPECT_TRUE(ContainCandidate(segments,
        "15:06"));
    // "15時06分"
    EXPECT_TRUE(ContainCandidate(segments,
        "15" "\xE6\x99\x82" "06" "\xE5\x88\x86"));
    // "午後3時6分"
    EXPECT_TRUE(ContainCandidate(segments,
        "\xE5\x8D\x88\xE5\xBE\x8C" "3" "\xE6\x99\x82" "6" "\xE5\x88\x86"));
  }

  // DateRewrite candidate order check.
  {
    // This parameter is copied from date_rewriter.cc.
    const size_t kMinimumDateCandidateIdx = 3;

    const char* kTodayCandidate[] = {
      "2011/04/18",
      "2011-04-18",
      // "2011年4月18日"
      "2011\xE5\xB9\xB4" "4\xE6\x9C\x88" "18\xE6\x97\xA5",
      // "平成23年4月18日"
      "\xE5\xB9\xB3\xE6\x88\x90" "23\xE5\xB9\xB4"
      "4\xE6\x9C\x88" "18\xE6\x97\xA5",
      // "月曜日"
      "\xE6\x9C\x88\xE6\x9B\x9C\xE6\x97\xA5"};

    // If initial count of candidate is 1, date rewrited candidate start from 1.
    // "きょう", "今日"
    InitSegment("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
                "\xE4\xBB\x8A\xE6\x97\xA5", &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(5, CountDescription(
        segments,
        // "今日の日付"
        "\xE4\xBB\x8A\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));
    size_t offset = 1;
    for (int rel_cand_idx = 0; rel_cand_idx < arraysize(kTodayCandidate);
         ++rel_cand_idx) {
      EXPECT_EQ(kTodayCandidate[rel_cand_idx],
                GetNthCandidateValue(segments, rel_cand_idx + offset));
    }

    // If initial count of candidate is 5 and target candidate is located at
    // index 4, date rewrited candidate start from 5.
    // "きょう", "今日"
    InitSegment("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
                "\xE4\xBB\x8A\xE6\x97\xA5", &segments);

    // Inserts no meaning candidates into segment.
    InsertCandidate("Candidate1", "Candidate1", 0, segments.mutable_segment(0));
    InsertCandidate("Candidate2", "Candidate2", 0, segments.mutable_segment(0));
    InsertCandidate("Candidate3", "Candidate3", 0, segments.mutable_segment(0));
    InsertCandidate("Candidate4", "Candidate4", 0, segments.mutable_segment(0));

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    // "今日の日付"
    EXPECT_EQ(5, CountDescription(
        segments,
        "\xE4\xBB\x8A\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));

    offset = 5;
    for (int rel_cand_idx = 0; rel_cand_idx < arraysize(kTodayCandidate);
         ++rel_cand_idx) {
      EXPECT_EQ(kTodayCandidate[rel_cand_idx],
                GetNthCandidateValue(segments, rel_cand_idx + offset));
    }

    // If initial count of candidate is 5 and target candidate is located at
    // index 0, date rewrited candidate start from kMinimumDateCandidateIdx.
    // "きょう", "今日"
    InitSegment("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
                "\xE4\xBB\x8A\xE6\x97\xA5", &segments);

    // Inserts no meaning candidates into segment.
    InsertCandidate("Candidate1", "Candidate1", 1, segments.mutable_segment(0));
    InsertCandidate("Candidate2", "Candidate2", 1, segments.mutable_segment(0));
    InsertCandidate("Candidate3", "Candidate3", 1, segments.mutable_segment(0));
    InsertCandidate("Candidate4", "Candidate4", 1, segments.mutable_segment(0));

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    // "今日の日付"
    EXPECT_EQ(5, CountDescription(
        segments,
        "\xE4\xBB\x8A\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));

    for (int rel_cand_idx = 0; rel_cand_idx < arraysize(kTodayCandidate);
         ++rel_cand_idx) {
      EXPECT_EQ(kTodayCandidate[rel_cand_idx],
                GetNthCandidateValue(segments,
                                     rel_cand_idx + kMinimumDateCandidateIdx));
    }
  }

  Util::SetClockHandler(NULL);
}

TEST_F(DateRewriterTest, ADToERA) {
  DateRewriter rewriter;
  vector<string> results;
  const ConversionRequest request;

  results.clear();
  rewriter.AdToEra(0, &results);
  EXPECT_EQ(results.size(), 0);

  // AD.645 is "大化元(年)"
  results.clear();
  rewriter.AdToEra(645, &results);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0],
            "\xE5\xA4\xA7\xE5\x8C\x96\xE5\x85\x83");

  // AD.646 is "大化2(年)" or "大化二(年)"
  results.clear();
  rewriter.AdToEra(646, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results,
            "\xE5\xA4\xA7\xE5\x8C\x96" "2",
            "\xE5\xA4\xA7\xE5\x8C\x96\xE4\xBA\x8C");


  // AD.1976 is "昭和51(年)" or "昭和五十一(年)"
  results.clear();
  rewriter.AdToEra(1976, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results,
            "\xE6\x98\xAD\xE5\x92\x8C" "51",
            "\xE6\x98\xAD\xE5\x92\x8C\xE4\xBA\x94\xE5\x8D\x81\xE4\xB8\x80");

  // AD.1989 is "昭和64(年)" or "昭和六四(年)" or "平成元(年)"
  results.clear();
  rewriter.AdToEra(1989, &results);
  Expect3Results(results,
                 "\xE6\x98\xAD\xE5\x92\x8C" "64",
                 "\xE6\x98\xAD\xE5\x92\x8C\xE5\x85\xAD\xE5\x8D\x81\xE5\x9B\x9B",
                 "\xE5\xB9\xB3\xE6\x88\x90\xE5\x85\x83");

  // AD.1990 is "平成2(年)" or "平成(二)年"
  results.clear();
  rewriter.AdToEra(1990, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results,
            "\xE5\xB9\xB3\xE6\x88\x90" "2",
            "\xE5\xB9\xB3\xE6\x88\x90\xE4\xBA\x8C");

  // 2 courts era.
  // AD.1331 "元徳3(年)" or "元弘元(年)"
  results.clear();
  rewriter.AdToEra(1331, &results);
  Expect3Results(results,
                 "\xE5\x85\x83\xE5\xBE\xB3" "3",
                 "\xE5\x85\x83\xE5\xBE\xB3\xE4\xB8\x89",
                 "\xE5\x85\x83\xE5\xBC\x98\xE5\x85\x83");

  // AD.1393 "明徳4(年)" or "明徳四(年)"
  results.clear();
  rewriter.AdToEra(1393, &results);
  Expect2Results(results,
            "\xE6\x98\x8E\xE5\xBE\xB3" "4",
            "\xE6\x98\x8E\xE5\xBE\xB3\xE5\x9B\x9B");

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
  Expect5Results(results,
                 "\xE5\x85\x83\xE5\xBC\x98" "2",
                 "\xE5\x85\x83\xE5\xBC\x98\xE4\xBA\x8C",
                 "\xE6\xAD\xA3\xE6\x85\xB6\xE5\x85\x83",
                 "\xE5\x85\x83\xE5\xBE\xB3" "4",
                 "\xE5\x85\x83\xE5\xBE\xB3\xE5\x9B\x9B");
  // AD.1333
  // South: "元弘3" or "元弘三(年)"
  // North: "正慶2" or "正慶二(年)"
  results.clear();
  rewriter.AdToEra(1333, &results);
  Expect4Results(results,
                 "\xE5\x85\x83\xE5\xBC\x98" "3",
                 "\xE5\x85\x83\xE5\xBC\x98\xE4\xB8\x89",
                 "\xE6\xAD\xA3\xE6\x85\xB6" "2",
                 "\xE6\xAD\xA3\xE6\x85\xB6\xE4\xBA\x8C");

  // AD.1334
  // South: "元弘4" or "元弘四(年)", "建武元"
  // North: "正慶3" or "正慶三(年)", "建武元(deduped)"
  results.clear();
  rewriter.AdToEra(1334, &results);
  EXPECT_EQ(results.size(), 5);
  Expect5Results(results,
                 "\xE5\x85\x83\xE5\xBC\x98" "4",
                 "\xE5\x85\x83\xE5\xBC\x98\xE5\x9B\x9B",
                 "\xE5\xBB\xBA\xE6\xAD\xA6\xE5\x85\x83",
                 "\xE6\xAD\xA3\xE6\x85\xB6\xE4\xB8\x89",
                 "\xE6\xAD\xA3\xE6\x85\xB6" "3");

  // AD.1997
  // "平成九年"
  results.clear();
  rewriter.AdToEra(1997, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results,
                 "\xE5\xB9\xB3\xE6\x88\x90" "9",
                 "\xE5\xB9\xB3\xE6\x88\x90\xE4\xB9\x9D");

  // AD.2011
  // "平成二十三年"
  results.clear();
  rewriter.AdToEra(2011, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(
      results,
      "\xE5\xB9\xB3\xE6\x88\x90" "23",
      "\xE5\xB9\xB3\xE6\x88\x90\xE4\xBA\x8C\xE5\x8D\x81\xE4\xB8\x89");

  // AD.1998
  // "平成十年" or "平成10年"
  results.clear();
  rewriter.AdToEra(1998, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results,
                 "\xE5\xB9\xB3\xE6\x88\x90" "10",
                 "\xE5\xB9\xB3\xE6\x88\x90\xE5\x8D\x81");

  // Negative Test
  // Too big number or negative number input are expected false return
  results.clear();
  EXPECT_FALSE(rewriter.AdToEra(2100, &results));
  EXPECT_FALSE(rewriter.AdToEra(-100, &results));
}

TEST_F(DateRewriterTest, ERAToAD) {
  DateRewriter rewriter;
  vector<string> results, descriptions;
  const ConversionRequest request;
  // "1234", "１２３４", "一二三四"
  const int kNumYearRepresentation = 3;

  results.clear();
  descriptions.clear();
  rewriter.EraToAd("", &results, &descriptions);
  EXPECT_EQ(0, results.size());
  EXPECT_EQ(0, descriptions.size());

  // "たいか1ねん" is "645年" or "６４５年" or "六四五年"
  // "大化1年"
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("\xE3\x81\x9F\xE3\x81\x84\xE3\x81\x8B"
                               "1" "\xE3\x81\xAD\xE3\x82\x93",
                               &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  Expect3Results(results,
                 "645\xE5\xB9\xB4",
                 "\xE5\x85\xAD\xE5\x9B\x9B\xE4\xBA\x94\xE5\xB9\xB4",
                 "\xEF\xBC\x96\xEF\xBC\x94\xEF\xBC\x95\xE5\xB9\xB4");
  EXPECT_TRUE(AllElementsAreSame("\xE5\xA4\xA7\xE5\x8C\x96" "1" "\xE5\xB9\xB4",
                                 descriptions));

  // "たいか2ねん" is "646年" or "６４６年" or "六四六年"
  // "大化2年"
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("\xE3\x81\x9F\xE3\x81\x84\xE3\x81\x8B"
                               "2" "\xE3\x81\xAD\xE3\x82\x93",
                               &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  Expect3Results(results,
                 "646\xE5\xB9\xB4",
                 "\xEF\xBC\x96\xEF\xBC\x94\xEF\xBC\x96\xE5\xB9\xB4",
                 "\xE5\x85\xAD\xE5\x9B\x9B\xE5\x85\xAD\xE5\xB9\xB4");
  EXPECT_TRUE(AllElementsAreSame("\xE5\xA4\xA7\xE5\x8C\x96" "2" "\xE5\xB9\xB4",
                                 descriptions));

  // "しょうわ2ねん" is AD.1313 or AD.1927
  // "1313年", "１３１３年", "一三一三年"
  // "1927年", "１９２７年", "一九二七年"
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("\xE3\x81\x97\xE3\x82\x87"
                               "\xE3\x81\x86\xE3\x82\x8F"
                               "2"
                               "\xE3\x81\xAD\xE3\x82\x93",
                               &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation * 2, results.size());
  EXPECT_EQ(kNumYearRepresentation * 2, descriptions.size());

  for (int i = 0; i < kNumYearRepresentation; ++i) {
    // "正和2年"
    EXPECT_EQ("\xE6\xAD\xA3\xE5\x92\x8C" "2" "\xE5\xB9\xB4", descriptions[i]);
    // "昭和2年"
    EXPECT_EQ("\xE6\x98\xAD\xE5\x92\x8C" "2" "\xE5\xB9\xB4",
              descriptions[i + kNumYearRepresentation]);
  }
  vector<string> first(results.begin(),
                       results.begin() + kNumYearRepresentation);
  vector<string> second(results.begin() + kNumYearRepresentation,
                        results.end());
  EXPECT_TRUE(IsStringContained("1313" "\xE5\xB9\xB4", first));
  EXPECT_TRUE(IsStringContained("\xEF\xBC\x91\xEF\xBC\x93\xEF\xBC\x91"
                                "\xEF\xBC\x93\xE5\xB9\xB4", first));
  EXPECT_TRUE(IsStringContained("\xE4\xB8\x80\xE4\xB8\x89\xE4\xB8\x80"
                                "\xE4\xB8\x89\xE5\xB9\xB4", first));
  EXPECT_TRUE(IsStringContained("1927" "\xE5\xB9\xB4", second));
  EXPECT_TRUE(IsStringContained("\xEF\xBC\x91\xEF\xBC\x99\xEF\xBC\x92"
                                "\xEF\xBC\x97\xE5\xB9\xB4", second));
  EXPECT_TRUE(IsStringContained("\xE4\xB8\x80\xE4\xB9\x9D\xE4\xBA\x8C"
                                "\xE4\xB8\x83\xE5\xB9\xB4", second));

  // North court test
  // "げんとく1ねん" is AD.1329
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("\xE3\x81\x92\xE3\x82\x93"
                               "\xE3\x81\xA8\xE3\x81\x8F"
                               "1"
                               "\xE3\x81\xAD\xE3\x82\x93",
                               &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  EXPECT_TRUE(IsStringContained("1329" "\xE5\xB9\xB4", results));
  // "めいとく3ねん" is AD.1392
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("\xE3\x82\x81\xE3\x81\x84"
                               "\xE3\x81\xA8\xE3\x81\x8F"
                               "3"
                               "\xE3\x81\xAD\xE3\x82\x93",
                               &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  EXPECT_TRUE(IsStringContained("1392" "\xE5\xB9\xB4", results));
  // "けんむ1ねん" is AD.1334 (requires dedupe)
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("\xE3\x81\x91\xE3\x82\x93\xE3\x82\x80"
                               "1"
                               "\xE3\x81\xAD\xE3\x82\x93",
                               &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  EXPECT_TRUE(IsStringContained("1334" "\xE5\xB9\xB4", results));

  // Big number test
  // "昭和80年" is AD.2005
  // "しょうわ80ねん"
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("\xE3\x81\x97\xE3\x82\x87"
                               "\xE3\x81\x86\xE3\x82\x8F"
                               "80"
                               "\xE3\x81\xAD\xE3\x82\x93",
                               &results, &descriptions));
  EXPECT_TRUE(IsStringContained("2005" "\xE5\xB9\xB4", results));
  // "大正101年" is AD.2012
  // "たいしょう101ねん"
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("\xE3\x81\x9F\xE3\x81\x84"
                               "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86"
                               "101" "\xE3\x81\xAD\xE3\x82\x93",
                               &results, &descriptions));
  EXPECT_TRUE(IsStringContained("2012" "\xE5\xB9\xB4", results));

  // "元年" test
  // "へいせいがんねん" is AD.1989
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("\xE3\x81\xB8\xE3\x81\x84"
                               "\xE3\x81\x9B\xE3\x81\x84"
                               "\xE3\x81\x8C\xE3\x82\x93"
                               "\xE3\x81\xAD\xE3\x82\x93"
                               , &results, &descriptions));
  EXPECT_EQ(kNumYearRepresentation, results.size());
  EXPECT_EQ(kNumYearRepresentation, descriptions.size());
  // "1989年"
  EXPECT_TRUE(IsStringContained("1989" "\xE5\xB9\xB4", results));
  // "１９８９年"
  EXPECT_TRUE(IsStringContained("\xEF\xBC\x91\xEF\xBC\x99"
                                "\xEF\xBC\x98\xEF\xBC\x99"
                                "\xE5\xB9\xB4", results));
  // "一九八九年"
  EXPECT_TRUE(IsStringContained("\xE4\xB8\x80\xE4\xB9\x9D"
                                "\xE5\x85\xAB\xE4\xB9\x9D"
                                "\xE5\xB9\xB4", results));
  // description is "平成元年"
  EXPECT_TRUE(AllElementsAreSame("\xE5\xB9\xB3\xE6\x88\x90"
                                 "\xE5\x85\x83\xE5\xB9\xB4",
                                 descriptions));

  // "しょうわがんねん" is AD.1926 or AD.1312
  results.clear();
  descriptions.clear();
  EXPECT_TRUE(rewriter.EraToAd("\xE3\x81\x97\xE3\x82\x87"
                               "\xE3\x81\x86\xE3\x82\x8F"
                               "\xE3\x81\x8C\xE3\x82\x93"
                               "\xE3\x81\xAD\xE3\x82\x93"
                               , &results, &descriptions));
  EXPECT_EQ(2 * kNumYearRepresentation, results.size());
  EXPECT_EQ(2 * kNumYearRepresentation, descriptions.size());
  // "1926年"
  EXPECT_TRUE(IsStringContained("1926" "\xE5\xB9\xB4", results));
  // "１９２６年"
  EXPECT_TRUE(IsStringContained("\xEF\xBC\x91\xEF\xBC\x99"
                                "\xEF\xBC\x92\xEF\xBC\x96"
                                "\xE5\xB9\xB4", results));
  // "一九二六年"
  EXPECT_TRUE(IsStringContained("\xE4\xB8\x80\xE4\xB9\x9D"
                                "\xE4\xBA\x8C\xE5\x85\xAD"
                                "\xE5\xB9\xB4", results));
  // "1312年"
  EXPECT_TRUE(IsStringContained("1312" "\xE5\xB9\xB4", results));
  // "１３１２年"
  EXPECT_TRUE(IsStringContained("\xEF\xBC\x91\xEF\xBC\x93"
                                "\xEF\xBC\x91\xEF\xBC\x92"
                                "\xE5\xB9\xB4", results));
  // "一三一二年"
  EXPECT_TRUE(IsStringContained("\xE4\xB8\x80\xE4\xB8\x89"
                                "\xE4\xB8\x80\xE4\xBA\x8C"
                                "\xE5\xB9\xB4", results));

  // Negative Test
  // 0 or negative number input are expected false return
  results.clear();
  descriptions.clear();
  EXPECT_FALSE(rewriter.EraToAd("\xE3\x81\x97\xE3\x82\x87"
                                "\xE3\x81\x86\xE3\x82\x8F"
                                "-1"
                                "\xE3\x81\xAD\xE3\x82\x93",
                                &results, &descriptions));
  EXPECT_FALSE(rewriter.EraToAd("\xE3\x81\x97\xE3\x82\x87"
                                "\xE3\x81\x86\xE3\x82\x8F"
                                "0"
                                "\xE3\x81\xAD\xE3\x82\x93",
                                &results, &descriptions));
  EXPECT_FALSE(rewriter.EraToAd(""
                                "0"
                                "\xE3\x81\xAD\xE3\x82\x93",
                                &results, &descriptions));
  EXPECT_EQ(0, results.size());
  EXPECT_EQ(0, descriptions.size());
}

TEST_F(DateRewriterTest, ConvertTime) {
  DateRewriter rewriter;
  vector<string> results;
  const ConversionRequest request;

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(0, 0, &results));

  // "0時00分, 午前0時00分"
  Expect3Results(results,
                 "0:00",
                 "0"
                 "\xE6\x99\x82"
                 "00"
                 "\xE5\x88\x86",
                 "\xE5\x8D\x88\xE5\x89\x8D"
                 "0"
                 "\xE6\x99\x82"
                 "0"
                 "\xE5\x88\x86");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(9, 9, &results));

  // "9時09分, 午前9時09分"
  Expect3Results(results,
                 "9:09",
                 "9"
                 "\xE6\x99\x82"
                 "09"
                 "\xE5\x88\x86",
                 "\xE5\x8D\x88\xE5\x89\x8D"
                 "9"
                 "\xE6\x99\x82"
                 "9"
                 "\xE5\x88\x86");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(11, 59, &results));

  // "11時59分, 午前11時59分"
  Expect3Results(results,
                 "11:59",
                 "11"
                 "\xE6\x99\x82"
                 "59"
                 "\xE5\x88\x86",
                 "\xE5\x8D\x88\xE5\x89\x8D"
                 "11"
                 "\xE6\x99\x82"
                 "59"
                 "\xE5\x88\x86");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(12, 0, &results));

  // "12時00分, 午後0時0分"
  Expect3Results(results,
                 "12:00",
                 "12"
                 "\xE6\x99\x82"
                 "00"
                 "\xE5\x88\x86",
                 "\xE5\x8D\x88\xE5\xBE\x8C"
                 "0"
                 "\xE6\x99\x82"
                 "0"
                 "\xE5\x88\x86");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(12, 1, &results));

  // "12時01分, 午後0時1分"
  Expect3Results(results,
                 "12:01",
                 "12"
                 "\xE6\x99\x82"
                 "01"
                 "\xE5\x88\x86",
                 "\xE5\x8D\x88\xE5\xBE\x8C"
                 "0"
                 "\xE6\x99\x82"
                 "1"
                 "\xE5\x88\x86");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(19, 23, &results));

  // "19時23分, 午後7時23分"
  Expect3Results(results,
                 "19:23",
                 "19"
                 "\xE6\x99\x82"
                 "23"
                 "\xE5\x88\x86",
                 "\xE5\x8D\x88\xE5\xBE\x8C"
                 "7"
                 "\xE6\x99\x82"
                 "23"
                 "\xE5\x88\x86");

  results.clear();
  EXPECT_TRUE(rewriter.ConvertTime(25, 23, &results));

  // "25時23分, 午前1時23分"
  Expect3Results(results,
                 "25:23",
                 "25"
                 "\xE6\x99\x82"
                 "23"
                 "\xE5\x88\x86",
                 "\xE5\x8D\x88\xE5\x89\x8D"
                 "1"
                 "\xE6\x99\x82"
                 "23"
                 "\xE5\x88\x86");

  results.clear();

  // "18:30,18時30分、18時半、午後6時30分、午後6時半"
  // And the order of results is must be above.
  EXPECT_TRUE(rewriter.ConvertTime(18, 30, &results));
  ASSERT_EQ(5, results.size());
  EXPECT_EQ("18:30", results[0]);
  EXPECT_EQ("\x31\x38\xE6\x99\x82\x33\x30\xE5\x88\x86", results[1]);
  EXPECT_EQ("\x31\x38\xE6\x99\x82\xE5\x8D\x8A", results[2]);
  EXPECT_EQ("\xE5\x8D\x88\xE5\xBE\x8C\x36\xE6\x99\x82\x33\x30\xE5\x88\x86",
            results[3]);
  EXPECT_EQ("\xE5\x8D\x88\xE5\xBE\x8C\x36\xE6\x99\x82\xE5\x8D\x8A", results[4]);
  results.clear();

  EXPECT_FALSE(rewriter.ConvertTime(-10, 20, &results));
  EXPECT_FALSE(rewriter.ConvertTime(10, -20, &results));
  EXPECT_FALSE(rewriter.ConvertTime(80, 20, &results));
  EXPECT_FALSE(rewriter.ConvertTime(20, 80, &results));
  EXPECT_FALSE(rewriter.ConvertTime(30, 80, &results));
}

TEST_F(DateRewriterTest, ConvertDateTest) {
  DateRewriter rewriter;
  vector<string> results;

  results.clear();
  EXPECT_TRUE(rewriter.ConvertDateWithYear(2011, 4, 17, &results));
  ASSERT_EQ(3, results.size());
  // "2011年4月17日"
  EXPECT_TRUE(IsStringContained("2011" "\xE5\xB9\xB4"
                                "4" "\xE6\x9C\x88"
                                "17" "\xE6\x97\xA5",
                                results));
  EXPECT_TRUE(IsStringContained("2011-04-17", results));
  EXPECT_TRUE(IsStringContained("2011/04/17", results));

  // January, March, May, July, Auguest, October, December has 31 days
  // April, June, September, November has 30 days
  // February is dealt as a special case, see below
  const struct {
    int month;
    int days;
  } month_days_test_data[] = {
    { 1, 31 },
    { 3, 31 },
    { 4, 30 },
    { 5, 31 },
    { 6, 30 },
    { 7, 31 },
    { 8, 31 },
    { 9, 30 },
    { 10, 31 },
    { 11, 30 },
    { 12, 31 }
  };

  for (size_t i = 0; i < arraysize(month_days_test_data); ++i) {
    EXPECT_TRUE(rewriter.ConvertDateWithYear(
        2001,
        month_days_test_data[i].month,
        month_days_test_data[i].days,
        &results));
    EXPECT_FALSE(rewriter.ConvertDateWithYear(
        2001,
        month_days_test_data[i].month,
        month_days_test_data[i].days+1,
        &results));
    ASSERT_TRUE(rewriter.ConvertDateWithoutYear(
        month_days_test_data[i].month,
        month_days_test_data[i].days,
        &results));
    ASSERT_FALSE(rewriter.ConvertDateWithoutYear(
        month_days_test_data[i].month,
        month_days_test_data[i].days + 1,
        &results));
  }

  // 4 dividable year is leap year.
  results.clear();
  EXPECT_TRUE(rewriter.ConvertDateWithYear(2004, 2, 29, &results));
  ASSERT_EQ(3, results.size());
  // "2004年2月29日"
  EXPECT_TRUE(IsStringContained("2004" "\xE5\xB9\xB4"
                                "2" "\xE6\x9C\x88"
                                "29" "\xE6\x97\xA5",
                                results));
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
  // "2000年2月29日"
  EXPECT_TRUE(IsStringContained("2000" "\xE5\xB9\xB4"
                                "2" "\xE6\x9C\x88"
                                "29" "\xE6\x97\xA5",
                                results));
  EXPECT_TRUE(IsStringContained("2000-02-29", results));
  EXPECT_TRUE(IsStringContained("2000/02/29", results));

  EXPECT_FALSE(rewriter.ConvertDateWithYear(0, 1, 1, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 13, 1, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 1, 41, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 13, 41, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 0, 1, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 1, 0, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithYear(2000, 0, 0, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithoutYear(13, 1, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithoutYear(1, 41, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithoutYear(13, 41, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithoutYear(0, 1, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithoutYear(1, 0, &results));
  EXPECT_FALSE(rewriter.ConvertDateWithoutYear(0, 0, &results));
}

TEST_F(DateRewriterTest, NumberRewriterTest) {
  Segments segments;
  DateRewriter rewriter;
  const commands::Request &request = commands::Request::default_instance();
  const composer::Composer composer(NULL, &request);
  const ConversionRequest conversion_request(&composer, &request);

  // 0101 is expected 3 time candidate and 2 date candidates
  InitSegment("0101", "0101", &segments);
  EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));
  // "時刻"
  EXPECT_EQ(3, CountDescription(segments,
                                "\xE6\x99\x82\xE5\x88\xBB"));
  // "午前1時1分"
  EXPECT_TRUE(ContainCandidate(
      segments,
      "\xE5\x8D\x88\xE5\x89\x8D\x31\xE6\x99\x82\x31\xE5\x88\x86"));
  // "1時01分"
  EXPECT_TRUE(ContainCandidate(
      segments,
      "\x31\xE6\x99\x82\x30\x31\xE5\x88\x86"));
  EXPECT_TRUE(ContainCandidate(segments, "1:01"));

  // "日付"
  EXPECT_EQ(2, CountDescription(
      segments,
      "\xE6\x97\xA5\xE4\xBB\x98"));
  // "1月1日"
  EXPECT_TRUE(ContainCandidate(
      segments,
      "\x31\xE6\x9C\x88\x31\xE6\x97\xA5"));
  EXPECT_TRUE(ContainCandidate(segments, "01/01"));

  // 1830 is expected 5 time candidate and 0 date candidates
  InitSegment("1830", "1830", &segments);
  EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));
  // "時刻"
  EXPECT_EQ(5, CountDescription(segments,
                                "\xE6\x99\x82\xE5\x88\xBB"));
  // "18時半"
  EXPECT_TRUE(ContainCandidate(segments,
                               "\x31\x38\xE6\x99\x82\xE5\x8D\x8A"));
  // "午後6時30分"
  EXPECT_TRUE(ContainCandidate(
      segments,
      "\xE5\x8D\x88\xE5\xBE\x8C\x36\xE6\x99\x82\x33\x30\xE5\x88\x86"));
  // "18時30分"
  EXPECT_TRUE(ContainCandidate(segments,
                               "\x31\x38\xE6\x99\x82\x33\x30\xE5\x88\x86"));
  // "午後6時半"
  EXPECT_TRUE(ContainCandidate(
      segments,
      "\xE5\x8D\x88\xE5\xBE\x8C\x36\xE6\x99\x82\xE5\x8D\x8A"));
  EXPECT_TRUE(ContainCandidate(segments, "18:30"));

  // "日付"
  EXPECT_EQ(0, CountDescription(segments, "\xE6\x97\xA5\xE4\xBB\x98"));

  // Especially for mobile, look at meta candidates' value, too.
  // "あかあか" on 12keys layout will be transliterated to "1212".
  InitMetaSegment(
      "\xE3\x81\x82\xE3\x81\x8B\xE3\x81\x82\xE3\x81\x8B", "1212", &segments);
  EXPECT_TRUE(rewriter.Rewrite(conversion_request, &segments));
  // "時刻"
  EXPECT_EQ(3, CountDescription(segments,
                                "\xE6\x99\x82\xE5\x88\xBB"));
  // "午後0時12分"
  EXPECT_TRUE(ContainCandidate(
      segments,
      "\xE5\x8d\x88\xE5\xBE\x8C\x30\xE6\x99\x82\x31\x32\xE5\x88\x86"));
  // "12時12分"
  EXPECT_TRUE(ContainCandidate(
      segments,
      "\x31\x32\xE6\x99\x82\x31\x32\xE5\x88\x86"));
  EXPECT_TRUE(ContainCandidate(segments, "12:12"));

  // "日付"
  EXPECT_EQ(2, CountDescription(
      segments,
      "\xE6\x97\xA5\xE4\xBB\x98"));
  // "12月12日"
  EXPECT_TRUE(ContainCandidate(
      segments,
      "\x31\x32\xE6\x9C\x88\x31\x32\xE6\x97\xA5"));
  EXPECT_TRUE(ContainCandidate(segments, "12/12"));

  // Invalid date or time number expect false return
  InitSegment("9999", "9999", &segments);
  EXPECT_FALSE(rewriter.Rewrite(conversion_request, &segments));
}

TEST_F(DateRewriterTest, NumberRewriterFromRawInputTest) {
  Segments segments;
  DateRewriter rewriter;

  composer::Table table;
  table.AddRule("222", "c", "");
  table.AddRule("3", "d", "");

  composer::Composer composer(&table, NULL);
  const ConversionRequest conversion_request(&composer, NULL);

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
  commands::Request input;
  DateRewriter rewriter;

  {
    input.set_mixed_conversion(true);
    const ConversionRequest request(NULL, &input);
    EXPECT_EQ(RewriterInterface::ALL, rewriter.capability(request));
  }

  {
    input.set_mixed_conversion(false);
    const ConversionRequest request(NULL, &input);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter.capability(request));
  }
}

TEST_F(DateRewriterTest, RewriteYearTest) {
  DateRewriter rewriter;
  Segments segments;
  const ConversionRequest request;

  InitSegment("2010", "2010", &segments);
  // "年"
  AppendSegment("nenn", "\xE5\xB9\xB4", &segments);

  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  // "平成22"
  EXPECT_TRUE(ContainCandidate(segments, "\xE5\xB9\xB3\xE6\x88\x90\x32\x32"));
}

// This test treats the situation that if UserHistoryRewriter or other like
// Rewriter moves up a candidate which is actually a number but can not be
// converted integer easily.
TEST_F(DateRewriterTest, RelationWithUserHistoryRewriterTest) {
  DateRewriter rewriter;
  Segments segments;
  const ConversionRequest request;

  // "二千十一"
  InitSegment("2011",
              "\xE4\xBA\x8C\xE5\x8D\x83\xE5\x8D\x81\xE4\xB8\x80",
              &segments);
  // "年"
  AppendSegment("nenn", "\xE5\xB9\xB4", &segments);

  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  // "平成23"
  EXPECT_TRUE(ContainCandidate(segments, "\xE5\xB9\xB3\xE6\x88\x90\x32\x33"));
}

}  // namespace mozc
