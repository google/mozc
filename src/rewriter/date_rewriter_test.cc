// Copyright 2010, Google Inc.
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

#include "converter/segments.h"
#include "rewriter/date_rewriter.h"
#include "testing/base/public/gunit.h"

namespace mozc {

namespace {
void Expect2Results(const vector<string> &src,
                    const string& exp1,
                    const string& exp2) {
  EXPECT_EQ(src.size(), 2);
  EXPECT_NE(src[0], src[1]);
  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(src[i] == exp1 || src[i] == exp2);
  }
}

void Expect3Results(const vector<string> &src,
                    const string& exp1,
                    const string& exp2,
                    const string& exp3) {
  EXPECT_EQ(src.size(), 3);
  EXPECT_NE(src[0], src[1]);
  EXPECT_NE(src[1], src[2]);
  EXPECT_NE(src[2], src[0]);
  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(src[i] == exp1 || src[i] == exp2 || src[i] == exp3);
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
}  // namespace

// TODO(taku): want to test the contents.
TEST(DateRewriteTest, DateRewriteTest) {
  DateRewriter rewriter;
  Segments segments;

  // "きょう/今日/今日の日付"
  {
    InitSegment("\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
                "\xE4\xBB\x8A\xE6\x97\xA5", &segments);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    EXPECT_EQ(4, CountDescription(
        segments,
        "\xE4\xBB\x8A\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));
  }

  // "あした/明日/明日の日付"
  {
    InitSegment("\xE3\x81\x82\xE3\x81\x97\xE3\x81\x9F",
                "\xE6\x98\x8E\xE6\x97\xA5", &segments);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    EXPECT_EQ(4, CountDescription(
        segments,
        "\xE6\x98\x8E\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));
  }

  // "きのう/昨日/昨日の日付"
  {
    InitSegment("\xE3\x81\x8D\xE3\x81\xAE\xE3\x81\x86",
                "\xE6\x98\xA8\xE6\x97\xA5", &segments);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    EXPECT_EQ(4, CountDescription(
        segments,
        "\xE6\x98\xA8\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));
  }

  // "あさって/明後日/明後日の日付"
  {
    InitSegment("\xE3\x81\x82\xE3\x81\x95\xE3\x81\xA3\xE3\x81\xA6",
                "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5", &segments);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    EXPECT_EQ(4, CountDescription(
        segments,
        "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5"
        "\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98"));
  }

  // "にちじ/日時/現在の日時"
  {
    InitSegment("\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\x98",
                "\xE6\x97\xA5\xE6\x99\x82", &segments);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    EXPECT_EQ(1, CountDescription(
        segments,
        "\xE7\x8F\xBE\xE5\x9C\xA8\xE3\x81\xAE\xE6\x97\xA5\xE6\x99\x82"));
  }

  // "いま/今/現在の時刻"
  {
    InitSegment("\xE3\x81\x84\xE3\x81\xBE",
                "\xE4\xBB\x8A", &segments);
    EXPECT_TRUE(rewriter.Rewrite(&segments));
    EXPECT_EQ(3, CountDescription(
        segments,
        "\xE7\x8F\xBE\xE5\x9C\xA8\xE3\x81\xAE\xE6\x99\x82\xE5\x88\xBB"));
  }
}

TEST(DateRewriterTest, ADToERA) {
  DateRewriter rewriter;
  vector<string> results;

  results.clear();
  rewriter.ADtoERA(0, &results);
  EXPECT_EQ(results.size(), 0);

  // AD.645 is 大化元(年)
  results.clear();
  rewriter.ADtoERA(645, &results);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0],
            "\xE5\xA4\xA7\xE5\x8C\x96\xE5\x85\x83");

  // AD.646 is 大化2(年)
  results.clear();
  rewriter.ADtoERA(646, &results);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0],
            "\xE5\xA4\xA7\xE5\x8C\x96"
            "2");


  // AD.1976 is 昭和51(年)
  results.clear();
  rewriter.ADtoERA(1976, &results);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0],
            "\xE6\x98\xAD\xE5\x92\x8C"
            "51");

  // AD.1989 is 昭和64(年) or 平成元(年)
  results.clear();
  rewriter.ADtoERA(1989, &results);
  Expect2Results(results,
                 "\xE6\x98\xAD\xE5\x92\x8C"
                 "64",
                 "\xE5\xB9\xB3\xE6\x88\x90\xE5\x85\x83");

  // AD.1990 is 平成2(年)
  results.clear();
  rewriter.ADtoERA(1990, &results);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0],
            "\xE5\xB9\xB3\xE6\x88\x90"
            "2");

  // 2 courts era.
  // AD.1331 元徳3(年) or 元弘元(年)
  results.clear();
  rewriter.ADtoERA(1331, &results);
  Expect2Results(results,
                 "\xE5\x85\x83\xE5\xBE\xB3"
                 "3",
                 "\xE5\x85\x83\xE5\xBC\x98\xE5\x85\x83");

  // AD.1393 明徳4(年)
  results.clear();
  rewriter.ADtoERA(1393, &results);
  EXPECT_EQ(results[0],
            "\xE6\x98\x8E\xE5\xBE\xB3"
            "4");

  // AD.1375
  // South: 文中4(年), 天授元(年)
  // North: 応安8(年), 永和元(年)
  results.clear();
  rewriter.ADtoERA(1375, &results);
  // just checks number.
  EXPECT_EQ(results.size(), 4);

  // AD.1332
  // South: 元弘2(年)
  // North: 正慶元(年), 元徳4(年)
  results.clear();
  rewriter.ADtoERA(1332, &results);
  EXPECT_EQ(results.size(), 3);
  Expect3Results(results,
                 "\xE5\x85\x83\xE5\xBC\x98"
                 "2",
                 "\xE6\xAD\xA3\xE6\x85\xB6\xE5\x85\x83",
                 "\xE5\x85\x83\xE5\xBE\xB3"
                 "4");
  // AD.1333
  // South: 元弘3
  // North: 正慶2
  results.clear();
  rewriter.ADtoERA(1333, &results);
  Expect2Results(results,
                 "\xE5\x85\x83\xE5\xBC\x98"
                 "3",
                 "\xE6\xAD\xA3\xE6\x85\xB6"
                 "2");

  // AD.1334
  // South: 元弘4, 建武元
  // North: 正慶3, 建武元(deduped)
  results.clear();
  rewriter.ADtoERA(1334, &results);
  EXPECT_EQ(results.size(), 3);
  Expect3Results(results,
                 "\xE5\x85\x83\xE5\xBC\x98"
                 "4",
                 "\xE5\xBB\xBA\xE6\xAD\xA6\xE5\x85\x83",
                 "\xE6\xAD\xA3\xE6\x85\xB6"
                 "3");
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
