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

#include <string>

#include "converter/segments.h"
#include "rewriter/number_rewriter.h"
#include "testing/base/public/gunit.h"

namespace mozc {

namespace {
// const char kFullKanjiDescription[]  = "[全] 漢数字";
// const char kFullArabicDescription[] = "[全] 数字";
// const char kHalfArabicDescription[] = "[半] 数字";
// const char kOldKanjiDescription[]  = "[全] 大字";
// const char kMaruNumberDescription[] = "[全] 丸数字 <機種依存文字>";
// const char kRomanCapitalDescription[] = "[全] ローマ数字(大文字) <機種依存文字>";
// const char kRomanNoCapitalDescription[] = "[全] ローマ数字(小文字) <機種依存文字>";

const char kFullKanjiDescription[]  = "[\xE5\x85\xA8] \xE6\xBC\xA2\xE6\x95\xB0\xE5\xAD\x97";
const char kFullArabicDescription[] = "[\xE5\x85\xA8] \xE6\x95\xB0\xE5\xAD\x97";
const char kHalfArabicDescription[] = "[\xE5\x8D\x8A] \xE6\x95\xB0\xE5\xAD\x97";
const char kOldKanjiDescription[]  = "[\xE5\x85\xA8] \xE5\xA4\xA7\xE5\xAD\x97";
const char kMaruNumberDescription[] = "[\xE5\x85\xA8] \xE4\xB8\xB8\xE6\x95\xB0\xE5\xAD\x97 "
    "<\xE6\xA9\x9F\xE7\xA8\xAE\xE4\xBE\x9D\xE5\xAD\x98\xE6\x96\x87\xE5\xAD\x97>";
const char kRomanCapitalDescription[] = "[\xE5\x85\xA8] "
    "\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x9E\xE6\x95\xB0\xE5\xAD\x97"
    "(\xE5\xA4\xA7\xE6\x96\x87\xE5\xAD\x97) "
    "<\xE6\xA9\x9F\xE7\xA8\xAE\xE4\xBE\x9D\xE5\xAD\x98\xE6\x96\x87\xE5\xAD\x97>";
const char kRomanNoCapitalDescription[] = "[\xE5\x85\xA8] "
    "\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x9E\xE6\x95\xB0\xE5\xAD\x97"
    "(\xE5\xB0\x8F\xE6\x96\x87\xE5\xAD\x97) "
    "<\xE6\xA9\x9F\xE7\xA8\xAE\xE4\xBE\x9D\xE5\xAD\x98\xE6\x96\x87\xE5\xAD\x97>";

bool FindValue(const Segment &segment, const string &value) {
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == value) {
      return true;
    }
  }
  return false;
}
}  // namespace

TEST(NumberRewriterTest, BasicTest) {
  NumberRewriter number_rewiter;

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = "012";
  candidate->content_value = "012";

  EXPECT_TRUE(number_rewiter.Rewrite(&segments));

  EXPECT_EQ(11, seg->candidates_size());

  // "012"
  EXPECT_EQ("012", seg->candidate(0).value);
  EXPECT_EQ("012", seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "〇一二"
  EXPECT_EQ("\xE3\x80\x87\xE4\xB8\x80\xE4\xBA\x8C", seg->candidate(1).value);
  EXPECT_EQ("", seg->candidate(1).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(1).description);

  // "０１２"
  EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x91\xEF\xBC\x92", seg->candidate(2).value);
  EXPECT_EQ("", seg->candidate(2).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(2).description);

  // "十二"
  EXPECT_EQ("\xE5\x8D\x81\xE4\xBA\x8C", seg->candidate(3).value);
  EXPECT_EQ("", seg->candidate(3).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(3).description);

  // "壱拾弐"
  EXPECT_EQ("\xE5\xA3\xB1\xE6\x8B\xBE\xE5\xBC\x90", seg->candidate(4).value);
  EXPECT_EQ("", seg->candidate(4).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(4).description);

  // VI
  EXPECT_EQ("\xE2\x85\xAB", seg->candidate(5).value);
  EXPECT_EQ("", seg->candidate(5).content_value);
  EXPECT_EQ(kRomanCapitalDescription, seg->candidate(5).description);

  // vi
  EXPECT_EQ("\xE2\x85\xBB", seg->candidate(6).value);
  EXPECT_EQ("", seg->candidate(6).content_value);
  EXPECT_EQ(kRomanNoCapitalDescription, seg->candidate(6).description);

  // 12 with circle mark
  EXPECT_EQ("\xE2\x91\xAB", seg->candidate(7).value);
  EXPECT_EQ("", seg->candidate(7).content_value);
  EXPECT_EQ(kMaruNumberDescription, seg->candidate(7).description);

  EXPECT_EQ("0xc", seg->candidate(8).value);
  EXPECT_EQ("", seg->candidate(8).content_value);
  // "16進数"
  EXPECT_EQ("[\xE5\x8D\x8A] \x31\x36\xE9\x80\xB2\xE6\x95\xB0",
            seg->candidate(8).description);

  EXPECT_EQ("014", seg->candidate(9).value);
  EXPECT_EQ("", seg->candidate(9).content_value);
  // "8進数"
  EXPECT_EQ("[\xE5\x8D\x8A] \x38\xE9\x80\xB2\xE6\x95\xB0",
            seg->candidate(9).description);

  EXPECT_EQ("0b1100", seg->candidate(10).value);
  EXPECT_EQ("", seg->candidate(10).content_value);
  // "2進数"
  EXPECT_EQ("[\xE5\x8D\x8A] \x32\xE9\x80\xB2\xE6\x95\xB0",
            seg->candidate(10).description);

  seg->clear_candidates();
}

TEST(NumberRewriterTest, OneOfCandidatesIsEmpty) {
  NumberRewriter number_rewiter;

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *first_candidate = seg->add_candidate();
  first_candidate->Init();

  // this candidate should be skipped
  first_candidate->value = "";
  first_candidate->content_value = first_candidate->value;

  Segment::Candidate *second_candidate = seg->add_candidate();
  second_candidate->Init();

  second_candidate->value = "0";
  second_candidate->content_value = second_candidate->value;

  EXPECT_TRUE(number_rewiter.Rewrite(&segments));

  EXPECT_EQ("", seg->candidate(0).value);
  EXPECT_EQ("", seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "0"
  EXPECT_EQ("0", seg->candidate(1).value);
  EXPECT_EQ("0", seg->candidate(1).content_value);
  EXPECT_EQ("", seg->candidate(1).description);

  // "〇"
  EXPECT_EQ("\xE3\x80\x87",
            seg->candidate(2).value);
  EXPECT_EQ("", seg->candidate(2).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(2).description);

  // "０"
  EXPECT_EQ("\xEF\xBC\x90",
            seg->candidate(3).value);
  EXPECT_EQ("", seg->candidate(3).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(3).description);

  // "零"
  EXPECT_EQ("\xE9\x9B\xB6",
            seg->candidate(4).value);
  EXPECT_EQ("", seg->candidate(4).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(4).description);

  seg->clear_candidates();
}

TEST(NumberRewriterTest, RewriteDoesNotHappen) {
  NumberRewriter number_rewiter;

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();

  // タンポポ
  candidate->value = "\xe3\x82\xbf\xe3\x83\xb3\xe3\x83\x9d\xe3\x83\x9d";
  candidate->content_value = candidate->value;

  // Number rewrite should not occur
  EXPECT_FALSE(number_rewiter.Rewrite(&segments));

  // Number of cahdidates should be maintained
  EXPECT_EQ(1, seg->candidates_size());

  seg->clear_candidates();
}

TEST(NumberRewriterTest, NumberIsZero) {
  NumberRewriter number_rewiter;

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = "0";
  candidate->content_value = "0";

  EXPECT_TRUE(number_rewiter.Rewrite(&segments));

  EXPECT_EQ(4, seg->candidates_size());

  // "0"
  EXPECT_EQ("0", seg->candidate(0).value);
  EXPECT_EQ("0", seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "〇"
  EXPECT_EQ("\xE3\x80\x87",
            seg->candidate(1).value);
  EXPECT_EQ("", seg->candidate(1).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(1).description);

  // "０"
  EXPECT_EQ("\xEF\xBC\x90",
            seg->candidate(2).value);
  EXPECT_EQ("", seg->candidate(2).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(2).description);

  // "零"
  EXPECT_EQ("\xE9\x9B\xB6",
            seg->candidate(3).value);
  EXPECT_EQ("", seg->candidate(3).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(3).description);

  seg->clear_candidates();
}

TEST(NumberRewriterTest, NumberIsZeroZero) {
  NumberRewriter number_rewiter;

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = "00";
  candidate->content_value = "00";

  EXPECT_TRUE(number_rewiter.Rewrite(&segments));

  EXPECT_EQ(4, seg->candidates_size());

  // "00"
  EXPECT_EQ("00", seg->candidate(0).value);
  EXPECT_EQ("00", seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "〇〇"
  EXPECT_EQ("\xE3\x80\x87\xE3\x80\x87",
            seg->candidate(1).value);
  EXPECT_EQ("", seg->candidate(1).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(1).description);

  // "００"
  EXPECT_EQ("\xEF\xBC\x90\xEF\xBC\x90",
            seg->candidate(2).value);
  EXPECT_EQ("", seg->candidate(2).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(2).description);

  // "零"
  EXPECT_EQ("\xE9\x9B\xB6",
            seg->candidate(3).value);
  EXPECT_EQ("", seg->candidate(3).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(3).description);

  seg->clear_candidates();
}

TEST(NumberRewriterTest, NumberIs19Digit) {
  NumberRewriter number_rewiter;

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = "1000000000000000000";
  candidate->content_value = "1000000000000000000";

  EXPECT_TRUE(number_rewiter.Rewrite(&segments));

  EXPECT_EQ(10, seg->candidates_size());

  // "1000000000000000000"
  EXPECT_EQ("1000000000000000000", seg->candidate(0).value);
  EXPECT_EQ("1000000000000000000", seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "一〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇"
  EXPECT_EQ("\xE4\xB8\x80\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3"
            "\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80"
            "\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87"
            "\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3"
            "\x80\x87\xE3\x80\x87",
            seg->candidate(1).value);
  EXPECT_EQ("", seg->candidate(1).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(1).description);

  // "１００００００００００００００００００"
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF"
            "\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC"
            "\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF"
            "\xBC\x90\xEF\xBC\x90",
            seg->candidate(2).value);
  EXPECT_EQ("", seg->candidate(2).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(2).description);

  // "1,000,000,000,000,000,000"
  EXPECT_EQ("1,000,000,000,000,000,000", seg->candidate(3).value);
  EXPECT_EQ("", seg->candidate(3).content_value);
  EXPECT_EQ(kHalfArabicDescription, seg->candidate(3).description);

  // "１，０００，０００，０００，０００，０００，０００"
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x90",
            seg->candidate(4).value);
  EXPECT_EQ("", seg->candidate(4).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(4).description);

  // "百京"
  EXPECT_EQ("\xE7\x99\xBE\xE4\xBA\xAC",
            seg->candidate(5).value);
  EXPECT_EQ("", seg->candidate(5).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(5).description);

  // "壱百京",
  EXPECT_EQ("\xE5\xA3\xB1\xE7\x99\xBE\xE4\xBA\xAC",
            seg->candidate(6).value);
  EXPECT_EQ("", seg->candidate(6).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(6).description);

  EXPECT_EQ("0xde0b6b3a7640000", seg->candidate(7).value);
  EXPECT_EQ("", seg->candidate(7).content_value);
  // "16進数"
  EXPECT_EQ("[\xE5\x8D\x8A] \x31\x36\xE9\x80\xB2\xE6\x95\xB0",
            seg->candidate(7).description);

  EXPECT_EQ("067405553164731000000", seg->candidate(8).value);
  EXPECT_EQ("", seg->candidate(8).content_value);
  // "8進数"
  EXPECT_EQ("[\xE5\x8D\x8A] \x38\xE9\x80\xB2\xE6\x95\xB0",
            seg->candidate(8).description);

  EXPECT_EQ("0b"
            "110111100000101101101011001110"
            "100111011001000000000000000000",
            seg->candidate(9).value);
  EXPECT_EQ("", seg->candidate(9).content_value);
  // "2進数"
  EXPECT_EQ("[\xE5\x8D\x8A] \x32\xE9\x80\xB2\xE6\x95\xB0",
            seg->candidate(9).description);

  seg->clear_candidates();
}

TEST(NumberRewriterTest, NumberIs20Digit) {
  NumberRewriter number_rewiter;

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = "10000000000000000000";
  candidate->content_value = "10000000000000000000";

  EXPECT_TRUE(number_rewiter.Rewrite(&segments));

  EXPECT_EQ(8, seg->candidates_size());

  // "10000000000000000000"
  EXPECT_EQ("10000000000000000000", seg->candidate(0).value);
  EXPECT_EQ("10000000000000000000", seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "一〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇〇"
  EXPECT_EQ("\xE4\xB8\x80\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3"
            "\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80"
            "\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87"
            "\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3\x80\x87\xE3"
            "\x80\x87\xE3\x80\x87\xE3\x80\x87",
            seg->candidate(1).value);
  EXPECT_EQ("", seg->candidate(1).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(1).description);

  // "１０００００００００００００００００００"
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF"
            "\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC"
            "\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90\xEF"
            "\xBC\x90\xEF\xBC\x90\xEF\xBC\x90",
            seg->candidate(2).value);
  EXPECT_EQ("", seg->candidate(2).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(2).description);

  // "10,000,000,000,000,000,000"
  EXPECT_EQ("10,000,000,000,000,000,000", seg->candidate(3).value);
  EXPECT_EQ("", seg->candidate(3).content_value);
  EXPECT_EQ(kHalfArabicDescription, seg->candidate(3).description);

  // "１０，０００，０００，０００，０００，０００，０００"
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x8C\xEF\xBC\x90"
            "\xEF\xBC\x90\xEF\xBC\x90",
            seg->candidate(4).value);
  EXPECT_EQ("", seg->candidate(4).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(4).description);

  // "一千京"
  EXPECT_EQ("\xE4\xB8\x80\xE5\x8D\x83\xE4\xBA\xAC",
            seg->candidate(5).value);
  EXPECT_EQ("", seg->candidate(5).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(5).description);

  // "壱千京"
  EXPECT_EQ("\xE5\xA3\xB1\xE5\x8D\x83\xE4\xBA\xAC",
            seg->candidate(6).value);
  EXPECT_EQ("", seg->candidate(6).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(6).description);

  // "壱阡京"
  EXPECT_EQ("\xE5\xA3\xB1\xE9\x98\xA1\xE4\xBA\xAC",
            seg->candidate(7).value);
  EXPECT_EQ("", seg->candidate(7).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(7).description);

  seg->clear_candidates();
}

TEST(NumberRewriterTest, NumberIsGreaterThanUInt64Max) {
  NumberRewriter number_rewiter;

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = "18446744073709551616";  // 2^64
  candidate->content_value = "18446744073709551616";

  EXPECT_TRUE(number_rewiter.Rewrite(&segments));

  EXPECT_EQ(13, seg->candidates_size());

  // "18446744073709551616"
  EXPECT_EQ("18446744073709551616", seg->candidate(0).value);
  EXPECT_EQ("18446744073709551616", seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // "一八四四六七四四〇七三七〇九五五一六一六"
  EXPECT_EQ("\xE4\xB8\x80\xE5\x85\xAB\xE5\x9B\x9B\xE5\x9B\x9B\xE5"
            "\x85\xAD\xE4\xB8\x83\xE5\x9B\x9B\xE5\x9B\x9B\xE3\x80"
            "\x87\xE4\xB8\x83\xE4\xB8\x89\xE4\xB8\x83\xE3\x80\x87"
            "\xE4\xB9\x9D\xE4\xBA\x94\xE4\xBA\x94\xE4\xB8\x80\xE5"
            "\x85\xAD\xE4\xB8\x80\xE5\x85\xAD",
            seg->candidate(1).value);
  EXPECT_EQ("", seg->candidate(1).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(1).description);

  // "１８４４６７４４０７３７０９５５１６１６"
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x98\xEF\xBC\x94\xEF\xBC\x94\xEF"
            "\xBC\x96\xEF\xBC\x97\xEF\xBC\x94\xEF\xBC\x94\xEF\xBC"
            "\x90\xEF\xBC\x97\xEF\xBC\x93\xEF\xBC\x97\xEF\xBC\x90"
            "\xEF\xBC\x99\xEF\xBC\x95\xEF\xBC\x95\xEF\xBC\x91\xEF"
            "\xBC\x96\xEF\xBC\x91\xEF\xBC\x96",
            seg->candidate(2).value);
  EXPECT_EQ("", seg->candidate(2).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(2).description);

  // "18,446,744,073,709,551,616"
  EXPECT_EQ("18,446,744,073,709,551,616", seg->candidate(3).value);
  EXPECT_EQ("", seg->candidate(3).content_value);
  EXPECT_EQ(kHalfArabicDescription, seg->candidate(3).description);

  // "１８，４４６，７４４，０７３，７０９，５５１，６１６"
  EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x98\xEF\xBC\x8C\xEF\xBC\x94"
            "\xEF\xBC\x94\xEF\xBC\x96\xEF\xBC\x8C\xEF\xBC\x97"
            "\xEF\xBC\x94\xEF\xBC\x94\xEF\xBC\x8C\xEF\xBC\x90"
            "\xEF\xBC\x97\xEF\xBC\x93\xEF\xBC\x8C\xEF\xBC\x97"
            "\xEF\xBC\x90\xEF\xBC\x99\xEF\xBC\x8C\xEF\xBC\x95"
            "\xEF\xBC\x95\xEF\xBC\x91\xEF\xBC\x8C\xEF\xBC\x96"
            "\xEF\xBC\x91\xEF\xBC\x96",
            seg->candidate(4).value);
  EXPECT_EQ("", seg->candidate(4).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(4).description);

  // "千八百四十四京六千七百四十四兆"
  // "七百三十七億九百五十五万千六百十六"
  EXPECT_EQ("\xE5\x8D\x83\xE5\x85\xAB\xE7\x99\xBE\xE5\x9B\x9B\xE5"
            "\x8D\x81\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85\xAD\xE5\x8D"
            "\x83\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B\xE5\x8D\x81"
            "\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7\x99\xBE\xE4"
            "\xB8\x89\xE5\x8D\x81\xE4\xB8\x83\xE5\x84\x84\xE4\xB9"
            "\x9D\xE7\x99\xBE\xE4\xBA\x94\xE5\x8D\x81\xE4\xBA\x94"
            "\xE4\xB8\x87\xE5\x8D\x83\xE5\x85\xAD\xE7\x99\xBE\xE5"
            "\x8D\x81\xE5\x85\xAD",
            seg->candidate(5).value);
  EXPECT_EQ("", seg->candidate(5).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(5).description);

  // "一千八百四十四京六千七百四十四兆"
  // "七百三十七億九百五十五万千六百十六"
  EXPECT_EQ("\xE4\xB8\x80\xE5\x8D\x83\xE5\x85\xAB\xE7\x99\xBE\xE5"
            "\x9B\x9B\xE5\x8D\x81\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85"
            "\xAD\xE5\x8D\x83\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B"
            "\xE5\x8D\x81\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7"
            "\x99\xBE\xE4\xB8\x89\xE5\x8D\x81\xE4\xB8\x83\xE5\x84"
            "\x84\xE4\xB9\x9D\xE7\x99\xBE\xE4\xBA\x94\xE5\x8D\x81"
            "\xE4\xBA\x94\xE4\xB8\x87\xE5\x8D\x83\xE5\x85\xAD\xE7"
            "\x99\xBE\xE5\x8D\x81\xE5\x85\xAD",
            seg->candidate(6).value);
  EXPECT_EQ("", seg->candidate(6).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(6).description);

  // "千八百四十四京六千七百四十四兆"
  // "七百三十七億九百五十五万一千六百十六"
  EXPECT_EQ("\xE5\x8D\x83\xE5\x85\xAB\xE7\x99\xBE\xE5\x9B\x9B\xE5"
            "\x8D\x81\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85\xAD\xE5\x8D"
            "\x83\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B\xE5\x8D\x81"
            "\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7\x99\xBE\xE4"
            "\xB8\x89\xE5\x8D\x81\xE4\xB8\x83\xE5\x84\x84\xE4\xB9"
            "\x9D\xE7\x99\xBE\xE4\xBA\x94\xE5\x8D\x81\xE4\xBA\x94"
            "\xE4\xB8\x87\xE4\xB8\x80\xE5\x8D\x83\xE5\x85\xAD\xE7"
            "\x99\xBE\xE5\x8D\x81\xE5\x85\xAD",
            seg->candidate(7).value);
  EXPECT_EQ("", seg->candidate(7).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(7).description);

  // "一千八百四十四京六千七百四十四兆"
  // "七百三十七億九百五十五万一千六百十六"
  EXPECT_EQ("\xE4\xB8\x80\xE5\x8D\x83\xE5\x85\xAB\xE7\x99\xBE\xE5"
            "\x9B\x9B\xE5\x8D\x81\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85"
            "\xAD\xE5\x8D\x83\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B"
            "\xE5\x8D\x81\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7"
            "\x99\xBE\xE4\xB8\x89\xE5\x8D\x81\xE4\xB8\x83\xE5\x84"
            "\x84\xE4\xB9\x9D\xE7\x99\xBE\xE4\xBA\x94\xE5\x8D\x81"
            "\xE4\xBA\x94\xE4\xB8\x87\xE4\xB8\x80\xE5\x8D\x83\xE5"
            "\x85\xAD\xE7\x99\xBE\xE5\x8D\x81\xE5\x85\xAD",
            seg->candidate(8).value);
  EXPECT_EQ("", seg->candidate(8).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(8).description);

  // "壱千八百四拾四京六千七百四拾四兆"
  // "七百参拾七億九百五拾五万壱千六百壱拾六"
  EXPECT_EQ("\xE5\xA3\xB1\xE5\x8D\x83\xE5\x85\xAB\xE7\x99\xBE\xE5"
            "\x9B\x9B\xE6\x8B\xBE\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85"
            "\xAD\xE5\x8D\x83\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B"
            "\xE6\x8B\xBE\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7"
            "\x99\xBE\xE5\x8F\x82\xE6\x8B\xBE\xE4\xB8\x83\xE5\x84"
            "\x84\xE4\xB9\x9D\xE7\x99\xBE\xE4\xBA\x94\xE6\x8B\xBE"
            "\xE4\xBA\x94\xE4\xB8\x87\xE5\xA3\xB1\xE5\x8D\x83\xE5"
            "\x85\xAD\xE7\x99\xBE\xE5\xA3\xB1\xE6\x8B\xBE\xE5\x85"
            "\xAD",
            seg->candidate(9).value);
  EXPECT_EQ("", seg->candidate(9).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(9).description);

  // "壱阡八百四拾四京六阡七百四拾四兆"
  // "七百参拾七億九百五拾五万壱阡六百壱拾六"
  EXPECT_EQ("\xE5\xA3\xB1\xE9\x98\xA1\xE5\x85\xAB\xE7\x99\xBE\xE5"
            "\x9B\x9B\xE6\x8B\xBE\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85"
            "\xAD\xE9\x98\xA1\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B"
            "\xE6\x8B\xBE\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7"
            "\x99\xBE\xE5\x8F\x82\xE6\x8B\xBE\xE4\xB8\x83\xE5\x84"
            "\x84\xE4\xB9\x9D\xE7\x99\xBE\xE4\xBA\x94\xE6\x8B\xBE"
            "\xE4\xBA\x94\xE4\xB8\x87\xE5\xA3\xB1\xE9\x98\xA1\xE5"
            "\x85\xAD\xE7\x99\xBE\xE5\xA3\xB1\xE6\x8B\xBE\xE5\x85"
            "\xAD",
            seg->candidate(10).value);
  EXPECT_EQ("", seg->candidate(10).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(10).description);

  // "壱千八百四拾四京六千七百四拾四兆"
  // "七百参拾七億九百五拾五萬壱千六百壱拾六"
  EXPECT_EQ("\xE5\xA3\xB1\xE5\x8D\x83\xE5\x85\xAB\xE7\x99\xBE\xE5"
            "\x9B\x9B\xE6\x8B\xBE\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85"
            "\xAD\xE5\x8D\x83\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B"
            "\xE6\x8B\xBE\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7"
            "\x99\xBE\xE5\x8F\x82\xE6\x8B\xBE\xE4\xB8\x83\xE5\x84"
            "\x84\xE4\xB9\x9D\xE7\x99\xBE\xE4\xBA\x94\xE6\x8B\xBE"
            "\xE4\xBA\x94\xE8\x90\xAC\xE5\xA3\xB1\xE5\x8D\x83\xE5"
            "\x85\xAD\xE7\x99\xBE\xE5\xA3\xB1\xE6\x8B\xBE\xE5\x85"
            "\xAD",
            seg->candidate(11).value);
  EXPECT_EQ("", seg->candidate(11).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(11).description);

  // "壱阡八百四拾四京六阡七百四拾四兆"
  // "七百参拾七億九百五拾五萬壱阡六百壱拾六"
  EXPECT_EQ("\xE5\xA3\xB1\xE9\x98\xA1\xE5\x85\xAB\xE7\x99\xBE\xE5"
            "\x9B\x9B\xE6\x8B\xBE\xE5\x9B\x9B\xE4\xBA\xAC\xE5\x85"
            "\xAD\xE9\x98\xA1\xE4\xB8\x83\xE7\x99\xBE\xE5\x9B\x9B"
            "\xE6\x8B\xBE\xE5\x9B\x9B\xE5\x85\x86\xE4\xB8\x83\xE7"
            "\x99\xBE\xE5\x8F\x82\xE6\x8B\xBE\xE4\xB8\x83\xE5\x84"
            "\x84\xE4\xB9\x9D\xE7\x99\xBE\xE4\xBA\x94\xE6\x8B\xBE"
            "\xE4\xBA\x94\xE8\x90\xAC\xE5\xA3\xB1\xE9\x98\xA1\xE5"
            "\x85\xAD\xE7\x99\xBE\xE5\xA3\xB1\xE6\x8B\xBE\xE5\x85"
            "\xAD",
            seg->candidate(12).value);
  EXPECT_EQ("", seg->candidate(12).content_value);
  EXPECT_EQ(kOldKanjiDescription, seg->candidate(12).description);

  seg->clear_candidates();
}

TEST(NumberRewriterTest, NumberIsGoogol) {
  NumberRewriter number_rewiter;

  Segments segments;
  Segment *seg = segments.push_back_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();

  // 10^100 as "100000 ... 0"
  string input = "1";
  for (size_t i = 0; i < 100; ++i) {
    input += "0";
  }

  candidate->value = input;
  candidate->content_value = input;

  EXPECT_TRUE(number_rewiter.Rewrite(&segments));

  EXPECT_EQ(6, seg->candidates_size());

  EXPECT_EQ(input, seg->candidate(0).value);
  EXPECT_EQ(input, seg->candidate(0).content_value);
  EXPECT_EQ("", seg->candidate(0).description);

  // 10^100 as "一〇〇〇〇〇 ... 〇"
  string expected2 = "\xE4\xB8\x80";  // "一"
  for (size_t i = 0; i < 100; ++i) {
    expected2 += "\xE3\x80\x87";  // "〇"
  }
  EXPECT_EQ(expected2, seg->candidate(1).value);
  EXPECT_EQ("", seg->candidate(1).content_value);
  EXPECT_EQ(kFullKanjiDescription, seg->candidate(1).description);

  // 10^100 as "１０００００ ... ０"
  string expected3 = "\xEF\xBC\x91";  // "１"
  for (size_t i = 0; i < 100; ++i) {
    expected3 += "\xEF\xBC\x90";  // "０"
  }
  EXPECT_EQ(expected3, seg->candidate(2).value);
  EXPECT_EQ("", seg->candidate(2).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(2).description);

  // 10,000, ... ,000
  string expected1 = "10";
  for (size_t i = 0; i < 100/3; ++i) {
    expected1 += ",000";
  }
  EXPECT_EQ(expected1, seg->candidate(3).value);
  EXPECT_EQ("", seg->candidate(3).content_value);
  EXPECT_EQ(kHalfArabicDescription, seg->candidate(3).description);

  // "１０，０００， ... ，０００"
  string expected4 = "\xEF\xBC\x91\xEF\xBC\x90";  // "１０"
  for (size_t i = 0; i < 100/3; ++i) {
    // "，０００"
    expected4 += "\xEF\xBC\x8C\xEF\xBC\x90\xEF\xBC\x90\xEF\xBC\x90";
  }
  EXPECT_EQ(expected4, seg->candidate(4).value);
  EXPECT_EQ("", seg->candidate(4).content_value);
  EXPECT_EQ(kFullArabicDescription, seg->candidate(4).description);

  EXPECT_EQ("Googol",
            seg->candidate(5).value);
  EXPECT_EQ("", seg->candidate(5).content_value);
  EXPECT_EQ("[\xE5\x8D\x8A]", seg->candidate(5).description);

  seg->clear_candidates();
}


TEST(NumberRewriterTest, SeparatedArabicsTest) {
  NumberRewriter number_rewiter;

  {
    Segments segments;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "123";
    candidate->content_value = "123";
    EXPECT_TRUE(number_rewiter.Rewrite(&segments));
    EXPECT_FALSE(FindValue(segments.segment(0), ",123"));
    // "，１２３"
    EXPECT_FALSE(FindValue(segments.segment(0),
                           "\xef\xbc\x8c\xef\xbc\x91\xef\xbc\x92\xef\xbc\x93"));
  }

  {
    Segments segments;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "999";
    candidate->content_value = "999";
    EXPECT_TRUE(number_rewiter.Rewrite(&segments));
    EXPECT_FALSE(FindValue(segments.segment(0), ",999"));
    // "，９９９"
    EXPECT_FALSE(FindValue(segments.segment(0),
                           "\xef\xbc\x8c\xef\xbc\x99\xef\xbc\x99\xef\xbc\x99"));
  }

  {
    Segments segments;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "1000";
    candidate->content_value = "1000";
    EXPECT_TRUE(number_rewiter.Rewrite(&segments));
    EXPECT_TRUE(FindValue(segments.segment(0), "1,000"));
    // "１，０００"
    EXPECT_TRUE(FindValue(segments.segment(0),
                          "\xef\xbc\x91\xef\xbc\x8c\xef\xbc"
                          "\x90\xef\xbc\x90\xef\xbc\x90"));
  }

  {
    Segments segments;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "0000";
    candidate->content_value = "0000";
    EXPECT_TRUE(number_rewiter.Rewrite(&segments));
    EXPECT_FALSE(FindValue(segments.segment(0), "0,000"));
    // "０，０００"
    EXPECT_FALSE(FindValue(segments.segment(0),
                           "\xef\xbc\x90\xef\xbc\x8c\xef\xbc"
                           "\x90\xef\xbc\x90\xef\xbc\x90"));
  }

  {
    Segments segments;
    Segment *seg = segments.push_back_segment();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "12345678";
    candidate->content_value = "12345678";
    EXPECT_TRUE(number_rewiter.Rewrite(&segments));
    EXPECT_TRUE(FindValue(segments.segment(0), "12,345,678"));
    // "１２，３４５，６７８"
    EXPECT_TRUE(FindValue(segments.segment(0),
                          "\xef\xbc\x91\xef\xbc\x92\xef\xbc"
                          "\x8c\xef\xbc\x93\xef\xbc\x94\xef"
                          "\xbc\x95\xef\xbc\x8c\xef\xbc\x96"
                          "\xef\xbc\x97\xef\xbc\x98"));
  }
}


}  // namespace mozc
