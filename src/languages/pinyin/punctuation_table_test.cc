// Copyright 2010-2013, Google Inc.
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

#include <cctype>
#include <string>
#include <vector>

#include "languages/pinyin/punctuation_table.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace pinyin {
namespace punctuation {

TEST(PunctuationTableTest, GetCandidates) {
  const PunctuationTable *table = Singleton<PunctuationTable>::get();

  for (char ch = 0; ch < 128; ++ch) {
    vector<string> candidates;
    if (isgraph(ch)) {
      EXPECT_TRUE(table->GetCandidates(ch, &candidates));
      EXPECT_LT(0, candidates.size());
    } else {
      EXPECT_FALSE(table->GetCandidates(ch, &candidates));
      EXPECT_TRUE(candidates.empty());
    }
  }

  // It is too hard to test all cases, so I test only some queries.

  vector<string> candidates;
  table->GetCandidates('!', &candidates);
  EXPECT_EQ(4, candidates.size());
  EXPECT_EQ(candidates[0], "\xEF\xBC\x81");  // "！"
  EXPECT_EQ(candidates[1], "\xEF\xB9\x97");  // "﹗"
  EXPECT_EQ(candidates[2], "\xE2\x80\xBC");  // "‼"
  EXPECT_EQ(candidates[3], "\xE2\x81\x89");  // "⁉"

  candidates.clear();
  table->GetCandidates('0', &candidates);
  EXPECT_EQ(2, candidates.size());
  EXPECT_EQ(candidates[0], "\xEF\xBC\x90");  // "０"
  EXPECT_EQ(candidates[1], "0");

  candidates.clear();
  table->GetCandidates('P', &candidates);
  EXPECT_EQ(2, candidates.size());
  EXPECT_EQ(candidates[0], "\xEF\xBC\xB0");  // "Ｐ"
  EXPECT_EQ(candidates[1], "P");

  candidates.clear();
  table->GetCandidates('a', &candidates);
  EXPECT_EQ(2, candidates.size());
  EXPECT_EQ(candidates[0], "\xEF\xBD\x81");  // "ａ"
  EXPECT_EQ(candidates[1], "a");

  candidates.clear();
  table->GetCandidates('~', &candidates);
  EXPECT_EQ(3, candidates.size());
  EXPECT_EQ(candidates[0], "\xEF\xBD\x9E");  // "～"
  EXPECT_EQ(candidates[1], "\xEF\xB9\x8B");  // "﹋"
  EXPECT_EQ(candidates[2], "\xEF\xB9\x8C");  // "﹌"
}

TEST(PunctuationTableTest, GetDefaultCandidates) {
  PunctuationTable *table = Singleton<PunctuationTable>::get();
  vector<string> candidates;

  table->GetDefaultCandidates(&candidates);
  EXPECT_EQ(10, candidates.size());
  EXPECT_EQ("\xC2\xB7"    , candidates[0]);  // "·"
  EXPECT_EQ("\xEF\xBC\x8C", candidates[1]);  // "，"
  EXPECT_EQ("\xE3\x80\x82", candidates[2]);  // "。"
  EXPECT_EQ("\xE3\x80\x8C", candidates[3]);  // "「"
  EXPECT_EQ("\xE3\x80\x8D", candidates[4]);  // "」"
  EXPECT_EQ("\xE3\x80\x81", candidates[5]);  // "、"
  EXPECT_EQ("\xEF\xBC\x9A", candidates[6]);  // "："
  EXPECT_EQ("\xEF\xBC\x9B", candidates[7]);  // "；"
  EXPECT_EQ("\xEF\xBC\x9F", candidates[8]);  // "？"
  EXPECT_EQ("\xEF\xBC\x81", candidates[9]);  // "！"
}

TEST(PunctuationTableTest, GetDirectCommitText) {
  PunctuationTable *table = Singleton<PunctuationTable>::get();
  string commit_text;

  // It is too hard to test all cases, so I test only some queries.

  {  // Simplified chinese test
    commit_text.clear();
    EXPECT_TRUE(table->GetDirectCommitTextForSimplifiedChinese(
        '!', &commit_text));
    EXPECT_EQ("\xEF\xBC\x81", commit_text);  // "！"

    commit_text.clear();
    EXPECT_TRUE(table->GetDirectCommitTextForSimplifiedChinese(
        '^', &commit_text));
    EXPECT_EQ("\xE2\x80\xA6\xE2\x80\xA6", commit_text);  // "……"

    commit_text.clear();
    EXPECT_TRUE(table->GetDirectCommitTextForSimplifiedChinese(
        '~', &commit_text));
    EXPECT_EQ("\xEF\xBD\x9E", commit_text);  // "～"
  }

  {  // Traditional chinese test
    commit_text.clear();
    EXPECT_TRUE(table->GetDirectCommitTextForTraditionalChinese(
        '!', &commit_text));
    EXPECT_EQ("\xEF\xBC\x81", commit_text);  // "！"

    commit_text.clear();
    EXPECT_TRUE(table->GetDirectCommitTextForTraditionalChinese(
        '<', &commit_text));
    EXPECT_EQ("\xEF\xBC\x8C", commit_text);  // "，"

    commit_text.clear();
    EXPECT_TRUE(table->GetDirectCommitTextForTraditionalChinese(
        '[', &commit_text));
    EXPECT_EQ("\xE3\x80\x8C", commit_text);  // "「"
  }

  {  // Failure test
    const char *kDummyText = "dummy_text";
    commit_text.assign(kDummyText);
    EXPECT_FALSE(table->GetDirectCommitTextForSimplifiedChinese(
        'a', &commit_text));
    EXPECT_EQ(kDummyText, commit_text);

    EXPECT_FALSE(table->GetDirectCommitTextForSimplifiedChinese(
        'A', &commit_text));
    EXPECT_EQ(kDummyText, commit_text);

    EXPECT_FALSE(table->GetDirectCommitTextForSimplifiedChinese(
        '0', &commit_text));
    EXPECT_EQ(kDummyText, commit_text);

    EXPECT_FALSE(table->GetDirectCommitTextForSimplifiedChinese(
        '#', &commit_text));
    EXPECT_EQ(kDummyText, commit_text);
  }
}

}  // namespace punctuation
}  // namespace pinyin
}  // namespace mozc
