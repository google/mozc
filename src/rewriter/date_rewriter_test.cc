// Copyright 2010-2011, Google Inc.
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

#include "base/clock_mock.h"
#include "base/util.h"
#include "converter/segments.h"
#include "rewriter/date_rewriter.h"
#include "testing/base/public/gunit.h"

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

void InitSegment(const string &key, const string &value,
                 Segments *segments) {
  segments->Clear();
  Segment *seg = segments->add_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  seg->set_key(key);
  candidate->content_key = key;
  candidate->value = value;
  candidate->content_value = value;
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

// "2011-04-18 15:06:31 (Mon)"
const uint64 kTestSeconds = 1303164391uLL;
// micro seconds. it is random value.
const uint32 kTestMicroSeconds = 588377u;
}  // namespace

TEST(DateRewriteTest, DateRewriteTest) {
  scoped_ptr<ClockMock> mock_clock(
      new ClockMock(kTestSeconds, kTestMicroSeconds));
  Util::SetClockHandler(mock_clock.get());

  DateRewriter rewriter;
  Segments segments;

  // "きょう/今日/今日の日付"
  {
    InitSegment("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
                "\xE4\xBB\x8A\xE6\x97\xA5", &segments);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
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
    EXPECT_TRUE(rewriter.Rewrite(&segments));
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
    EXPECT_TRUE(rewriter.Rewrite(&segments));
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
    EXPECT_TRUE(rewriter.Rewrite(&segments));
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
    EXPECT_TRUE(rewriter.Rewrite(&segments));
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
    EXPECT_TRUE(rewriter.Rewrite(&segments));
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

  Util::SetClockHandler(NULL);
}

TEST(DateRewriterTest, ADToERA) {
  DateRewriter rewriter;
  vector<string> results;

  results.clear();
  rewriter.ADtoERA(0, &results);
  EXPECT_EQ(results.size(), 0);

  // AD.645 is "大化元(年)"
  results.clear();
  rewriter.ADtoERA(645, &results);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0],
            "\xE5\xA4\xA7\xE5\x8C\x96\xE5\x85\x83");

  // AD.646 is "大化2(年)" or "大化二(年)"
  results.clear();
  rewriter.ADtoERA(646, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results,
            "\xE5\xA4\xA7\xE5\x8C\x96" "2",
            "\xE5\xA4\xA7\xE5\x8C\x96\xE4\xBA\x8C");


  // AD.1976 is "昭和51(年)" or "昭和五十一(年)"
  results.clear();
  rewriter.ADtoERA(1976, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results,
            "\xE6\x98\xAD\xE5\x92\x8C" "51",
            "\xE6\x98\xAD\xE5\x92\x8C\xE4\xBA\x94\xE5\x8D\x81\xE4\xB8\x80");

  // AD.1989 is "昭和64(年)" or "昭和六四(年)" or "平成元(年)"
  results.clear();
  rewriter.ADtoERA(1989, &results);
  Expect3Results(results,
                 "\xE6\x98\xAD\xE5\x92\x8C" "64",
                 "\xE6\x98\xAD\xE5\x92\x8C\xE5\x85\xAD\xE5\x8D\x81\xE5\x9B\x9B",
                 "\xE5\xB9\xB3\xE6\x88\x90\xE5\x85\x83");

  // AD.1990 is "平成2(年)" or "平成(二)年"
  results.clear();
  rewriter.ADtoERA(1990, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results,
            "\xE5\xB9\xB3\xE6\x88\x90" "2",
            "\xE5\xB9\xB3\xE6\x88\x90\xE4\xBA\x8C");

  // 2 courts era.
  // AD.1331 "元徳3(年)" or "元弘元(年)"
  results.clear();
  rewriter.ADtoERA(1331, &results);
  Expect3Results(results,
                 "\xE5\x85\x83\xE5\xBE\xB3" "3",
                 "\xE5\x85\x83\xE5\xBE\xB3\xE4\xB8\x89",
                 "\xE5\x85\x83\xE5\xBC\x98\xE5\x85\x83");

  // AD.1393 "明徳4(年)" or "明徳四(年)"
  results.clear();
  rewriter.ADtoERA(1393, &results);
  Expect2Results(results,
            "\xE6\x98\x8E\xE5\xBE\xB3" "4",
            "\xE6\x98\x8E\xE5\xBE\xB3\xE5\x9B\x9B");

  // AD.1375
  // South: "文中4(年)" or "文中四(年)", "天授元(年)"
  // North: "応安8(年)" or "応安八(年)", "永和元(年)"
  results.clear();
  rewriter.ADtoERA(1375, &results);
  // just checks number.
  EXPECT_EQ(results.size(), 6);

  // AD.1332
  // South: "元弘2(年)" or "元弘二(年)"
  // North: "正慶元(年)", "元徳4(年)" or "元徳四(年)"
  results.clear();
  rewriter.ADtoERA(1332, &results);
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
  rewriter.ADtoERA(1333, &results);
  Expect4Results(results,
                 "\xE5\x85\x83\xE5\xBC\x98" "3",
                 "\xE5\x85\x83\xE5\xBC\x98\xE4\xB8\x89",
                 "\xE6\xAD\xA3\xE6\x85\xB6" "2",
                 "\xE6\xAD\xA3\xE6\x85\xB6\xE4\xBA\x8C");

  // AD.1334
  // South: "元弘4" or "元弘四(年)", "建武元"
  // North: 2正慶3" or "正慶三(年)", "建武元(deduped)"
  results.clear();
  rewriter.ADtoERA(1334, &results);
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
  rewriter.ADtoERA(1997, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results,
                 "\xE5\xB9\xB3\xE6\x88\x90" "9",
                 "\xE5\xB9\xB3\xE6\x88\x90\xE4\xB9\x9D");

  // AD.2011
  // "平成二十三年"
  results.clear();
  rewriter.ADtoERA(2011, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(
      results,
      "\xE5\xB9\xB3\xE6\x88\x90" "23",
      "\xE5\xB9\xB3\xE6\x88\x90\xE4\xBA\x8C\xE5\x8D\x81\xE4\xB8\x89");

  // AD.1998
  // "平成十年" or "平成10年"
  results.clear();
  rewriter.ADtoERA(1998, &results);
  EXPECT_EQ(results.size(), 2);
  Expect2Results(results,
                 "\xE5\xB9\xB3\xE6\x88\x90" "10",
                 "\xE5\xB9\xB3\xE6\x88\x90\xE5\x8D\x81");

  // Negative Test
  // Too big number or negative number input are expected false return
  results.clear();
  EXPECT_FALSE(rewriter.ADtoERA(2100, &results));
  EXPECT_FALSE(rewriter.ADtoERA(-100, &results));
}

TEST(DateRewriterTest, ConvertTime) {
  DateRewriter rewriter;
  vector<string> results;

  results.clear();
  rewriter.ConvertTime(0, 0, &results);

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
  rewriter.ConvertTime(9, 9, &results);

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
  rewriter.ConvertTime(11, 59, &results);

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
  rewriter.ConvertTime(12, 0, &results);

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
  rewriter.ConvertTime(12, 1, &results);

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
  rewriter.ConvertTime(19, 23, &results);

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
}
}  // namespace mozc
