// Copyright 2010-2012, Google Inc.
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
#include <vector>

#include "base/scoped_ptr.h"
#include "base/util.h"
#include "languages/pinyin/punctuation_context.h"
#include "languages/pinyin/punctuation_table.h"
#include "languages/pinyin/session_config.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::_;

namespace mozc {
namespace pinyin {
namespace punctuation {

namespace {
class MockTable : public PunctuationTableInterface {
 public:
  MockTable() {
  }
  virtual ~MockTable() {}

  MOCK_CONST_METHOD2(GetCandidates,
                     bool(char key, vector<string> *candidates));
  MOCK_CONST_METHOD1(GetDefaultCandidates,
                     void(vector<string> *candidates));
  MOCK_CONST_METHOD2(GetDirectCommitTextForSimplifiedChinese,
                     bool(char key, string *commit_text));
  MOCK_CONST_METHOD2(GetDirectCommitTextForTraditionalChinese,
                     bool(char key, string *commit_text));
};

string Repeat(const string &str, int n) {
  string result;
  for (int i = 0; i < n; ++i) {
    result.append(str);
  }
  return result;
}
}  // namespace

class PunctuationContextTest : public testing::Test {
 protected:
  virtual void SetUp() {
    session_config_.reset(new SessionConfig);
    session_config_->full_width_word_mode = false;
    session_config_->full_width_punctuation_mode = true;
    session_config_->simplified_chinese_mode = true;

    context_.reset(new PunctuationContext(*session_config_));
    table_.reset(new MockTable);
    context_->table_ = table_.get();

    dummy_candidates_.clear();
    dummy_candidates_.push_back("\xEF\xBC\x81");  // "·"
    dummy_candidates_.push_back("\xEF\xBC\x8C");  // "，"
    dummy_candidates_.push_back("\xE3\x80\x82");  // "。"

    dummy_commit_text_.assign("__dummy_commit_text__");

    // Default behaviors of table_
    EXPECT_CALL(*table_, GetCandidates(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(dummy_candidates_),
                              Return(true)));
    EXPECT_CALL(*table_, GetDefaultCandidates(_))
        .WillRepeatedly(SetArgPointee<0>(dummy_candidates_));
    EXPECT_CALL(*table_, GetDirectCommitTextForSimplifiedChinese('!', _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(dummy_commit_text_),
                              Return(true)));
    EXPECT_CALL(*table_, GetDirectCommitTextForTraditionalChinese('!', _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(dummy_commit_text_),
                              Return(true)));
  }

  virtual void TearDown() {
  }

  void InsertCharacterChars(const string &chars) {
    for (size_t i = 0; i < chars.size(); ++i) {
      context_->Insert(chars[i]);
    }
  }

  void CheckComposition(const string &input_text,
                        const string &selected_text,
                        const string &rest_text) {
    EXPECT_EQ(input_text, context_->input_text());
    EXPECT_EQ(selected_text, context_->selected_text());
    EXPECT_EQ("", context_->conversion_text());
    EXPECT_EQ(rest_text, context_->rest_text());
    EXPECT_EQ(Util::CharsLen(selected_text), context_->cursor());
  }

  void CheckCandidates(const vector<string> &candidates,
                       size_t focused_candidate_index,
                       const string &auxiliary_text) {
    vector<string> actual_candidates;
    const size_t candidates_size = GetCandidatesSize();
    ASSERT_EQ(candidates.size(), candidates_size);
    for (size_t i = 0; i < candidates_size; ++i) {
      Candidate candidate;
      ASSERT_TRUE(context_->GetCandidate(i, &candidate));
      EXPECT_EQ(candidates[i], candidate.text);
    }

    EXPECT_EQ(auxiliary_text, context_->auxiliary_text());
    EXPECT_EQ(focused_candidate_index, context_->focused_candidate_index());
  }

  void CheckResult(const string &commit_text) {
    EXPECT_EQ(commit_text, context_->commit_text());
  }

  size_t GetCandidatesSize() {
    size_t size = 0;
    for (; context_->HasCandidate(size); ++size) {}
    return size;
  }

  scoped_ptr<MockTable> table_;
  scoped_ptr<SessionConfig> session_config_;
  scoped_ptr<PunctuationContext> context_;
  vector<string> dummy_candidates_;
  string dummy_commit_text_;
  const vector<string> empty_candidates_;
};

TEST_F(PunctuationContextTest, Insert) {
  {
    SCOPED_TRACE("Directly commit text (Success)");
    context_->Clear();
    EXPECT_TRUE(context_->Insert('!'));
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult(dummy_commit_text_);
  }

  {
    SCOPED_TRACE("Directly commit text (Failed)");
    context_->Clear();
    EXPECT_CALL(*table_, GetDirectCommitTextForSimplifiedChinese('!', _))
        .WillOnce(Return(false));

    EXPECT_TRUE(context_->Insert('!'));
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult("!");
  }

  {  // English Mode
    context_->Clear();

    {
      SCOPED_TRACE("Turn on English mode");
      EXPECT_TRUE(context_->Insert('`'));
      CheckComposition("`", dummy_candidates_[0], "");
      CheckCandidates(dummy_candidates_, 0, "`|");
      CheckResult("");
    }

    {
      SCOPED_TRACE("Insert a character on English mode");
      EXPECT_TRUE(context_->Insert('!'));
      CheckComposition("!", dummy_candidates_[0], "");
      CheckCandidates(dummy_candidates_, 0, "!|");
      CheckResult("");
    }

    {
      SCOPED_TRACE("Insert a additional character on English mode");
      EXPECT_TRUE(context_->Insert('!'));
      CheckComposition("!!", Repeat(dummy_candidates_[0], 2), "");
      CheckCandidates(dummy_candidates_, 0, "!!|");
      CheckResult("");
    }
  }

  context_->Clear();

  {
    SCOPED_TRACE("Insert a invalid character");
    EXPECT_FALSE(context_->Insert(' '));
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult("");
  }
}

TEST_F(PunctuationContextTest, Commit) {
  InsertCharacterChars("`!!");
  context_->MoveCursorLeft();
  context_->FocusCandidate(1);

  {
    SCOPED_TRACE("Commit");
    context_->Commit();
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult(dummy_candidates_[1] + dummy_candidates_[0]);
  }

  InsertCharacterChars("`!!");
  context_->MoveCursorLeft();
  context_->FocusCandidate(1);

  {
    SCOPED_TRACE("CommitPreedit");
    context_->CommitPreedit();
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult("!!");
  }
}

TEST_F(PunctuationContextTest, MoveCursor) {
  const string &candidate = dummy_candidates_[0];

  InsertCharacterChars("`");

  {
    SCOPED_TRACE("Moves cursor left [`]");
    context_->MoveCursorLeft();
    CheckComposition("`", "", candidate);
    CheckCandidates(empty_candidates_, 0, "|`");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Moves cursor left and does not make sense [`]");
    context_->MoveCursorLeft();
    CheckComposition("`", "", candidate);
    CheckCandidates(empty_candidates_, 0, "|`");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Moves cursor right [`]");
    context_->MoveCursorRight();
    CheckComposition("`", candidate, "");
    CheckCandidates(dummy_candidates_, 0, "`|");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Moves cursor right and does not make sense [`]");
    context_->MoveCursorRight();
    CheckComposition("`", candidate, "");
    CheckCandidates(dummy_candidates_, 0, "`|");
    CheckResult("");
  }

  context_->Clear();
  InsertCharacterChars("`!!!!");

  {
    SCOPED_TRACE("Moves cursor left [!!!!]");
    context_->MoveCursorLeft();
    CheckComposition("!!!!", Repeat(candidate, 3), Repeat(candidate, 1));
    CheckCandidates(dummy_candidates_, 0, "!!!|!");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Moves cursor left by word [!!!!]");
    context_->MoveCursorLeftByWord();
    CheckComposition("!!!!", Repeat(candidate, 2), Repeat(candidate, 2));
    CheckCandidates(dummy_candidates_, 0, "!!|!!");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Moves cursor to beginning [!!!!]");
    context_->MoveCursorToBeginning();
    CheckComposition("!!!!", Repeat(candidate, 0), Repeat(candidate, 4));
    CheckCandidates(empty_candidates_, 0, "|!!!!");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Moves cursor left and does not make sense [!!!!]");
    context_->MoveCursorLeftByWord();
    CheckComposition("!!!!", Repeat(candidate, 0), Repeat(candidate, 4));
    CheckCandidates(empty_candidates_, 0, "|!!!!");
    CheckResult("");
  }


  {
    SCOPED_TRACE("Moves cursor right [!!!!]");
    context_->MoveCursorRight();
    CheckComposition("!!!!", Repeat(candidate, 1), Repeat(candidate, 3));
    CheckCandidates(dummy_candidates_, 0, "!|!!!");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Moves cursor right by word [!!!!]");
    context_->MoveCursorRightByWord();
    CheckComposition("!!!!", Repeat(candidate, 2), Repeat(candidate, 2));
    CheckCandidates(dummy_candidates_, 0, "!!|!!");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Moves cursor to end [!!!!]");
    context_->MoveCursorToEnd();
    CheckComposition("!!!!", Repeat(candidate, 4), Repeat(candidate, 0));
    CheckCandidates(dummy_candidates_, 0, "!!!!|");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Moves cursor right and does not make sense [!!!!]");
    context_->MoveCursorRightByWord();
    CheckComposition("!!!!", Repeat(candidate, 4), Repeat(candidate, 0));
    CheckCandidates(dummy_candidates_, 0, "!!!!|");
    CheckResult("");
  }
}

TEST_F(PunctuationContextTest, FocusCandidateIndex) {
  {
    SCOPED_TRACE("There are no candidates");
    context_->FocusCandidate(10);
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult("");
  }

  InsertCharacterChars("`");

  {
    SCOPED_TRACE("Default candidates");
    context_->FocusCandidate(1);
    CheckComposition("`", dummy_candidates_[1], "");
    CheckCandidates(dummy_candidates_, 1, "`|");
    CheckResult("");
  }

  context_->Clear();
  InsertCharacterChars("`!");

  {
    SCOPED_TRACE("Normal candidates for [!]");
    context_->FocusCandidate(2);
    CheckComposition("!", dummy_candidates_[2], "");
    CheckCandidates(dummy_candidates_, 2, "!|");
    CheckResult("");
  }

  context_->Clear();
  InsertCharacterChars("`!!");

  {
    SCOPED_TRACE("Normal candidates for [!!]");
    context_->FocusCandidate(1);
    CheckComposition("!!", dummy_candidates_[0] + dummy_candidates_[1], "");
    CheckCandidates(dummy_candidates_, 1, "!!|");
    CheckResult("");
  }

  context_->Clear();
  InsertCharacterChars("`!!");
  context_->FocusCandidate(1);
  context_->MoveCursorLeft();

  {
    SCOPED_TRACE("Normal candidates for [!|!]");
    context_->FocusCandidate(2);
    CheckComposition("!!", dummy_candidates_[2], dummy_candidates_[1]);
    CheckCandidates(dummy_candidates_, 2, "!|!");
    CheckResult("");
  }
}

TEST_F(PunctuationContextTest, SelectCandidate) {
  InsertCharacterChars("`");

  {
    SCOPED_TRACE("Select a non-exist candidate [`]");
    EXPECT_FALSE(context_->SelectCandidate(100));
    CheckComposition("`", dummy_candidates_[0], "");
    CheckCandidates(dummy_candidates_, 0, "`|");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Select a 2nd default candidate and commit [`]");
    EXPECT_TRUE(context_->SelectCandidate(1));
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult(dummy_candidates_[1]);
  }

  InsertCharacterChars("`!");

  {
    SCOPED_TRACE("Select a 1st candidate and commit [!]");
    EXPECT_TRUE(context_->SelectCandidate(2));
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult(dummy_candidates_[2]);
  }

  InsertCharacterChars("`!!");

  {
    SCOPED_TRACE("Select a 3rd candidate and commit [!!]");
    EXPECT_TRUE(context_->SelectCandidate(2));
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult(dummy_candidates_[0] + dummy_candidates_[2]);
  }

  InsertCharacterChars("`!!");
  context_->MoveCursorLeft();

  {
    SCOPED_TRACE("Select a 2nd candidate and commit [!|!]");
    EXPECT_TRUE(context_->SelectCandidate(1));
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult(dummy_candidates_[1] + dummy_candidates_[0]);
  }
}

TEST_F(PunctuationContextTest, Remove) {
  const string candidate = dummy_candidates_[0];

  {
    SCOPED_TRACE("Removes a character from empty context");
    EXPECT_FALSE(context_->RemoveCharBefore());
    EXPECT_FALSE(context_->RemoveCharAfter());
    EXPECT_FALSE(context_->RemoveWordBefore());
    EXPECT_FALSE(context_->RemoveWordAfter());
  }

  context_->Clear();
  InsertCharacterChars("`");

  {
    SCOPED_TRACE("Removes a next character and doesn't make sense [`]");
    EXPECT_FALSE(context_->RemoveCharAfter());
    CheckComposition("`", candidate, "");
    CheckCandidates(dummy_candidates_, 0, "`|");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Removes a previous character [`]");
    EXPECT_TRUE(context_->RemoveCharBefore());
    CheckComposition("", "", "");
    CheckCandidates(empty_candidates_, 0, "");
    CheckResult("");
  }

  context_->Clear();
  InsertCharacterChars("`0123456789");
  for (size_t i = 0; i < 5; ++i) {
    context_->MoveCursorLeft();
  }

  {
    SCOPED_TRACE("Removes a previous character [01234|56789]");
    EXPECT_TRUE(context_->RemoveCharBefore());
    CheckComposition("012356789", Repeat(candidate, 4), Repeat(candidate, 5));
    CheckCandidates(dummy_candidates_, 0, "0123|56789");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Removes a next character [0123|56789]");
    EXPECT_TRUE(context_->RemoveCharAfter());
    CheckComposition("01236789", Repeat(candidate, 4), Repeat(candidate, 4));
    CheckCandidates(dummy_candidates_, 0, "0123|6789");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Removes a previous word [0123|6789]");
    EXPECT_TRUE(context_->RemoveWordBefore());
    CheckComposition("0126789", Repeat(candidate, 3), Repeat(candidate, 4));
    CheckCandidates(dummy_candidates_, 0, "012|6789");
    CheckResult("");
  }

  {
    SCOPED_TRACE("Removes a next word [012|6789]");
    EXPECT_TRUE(context_->RemoveWordAfter());
    CheckComposition("012789", Repeat(candidate, 3), Repeat(candidate, 3));
    CheckCandidates(dummy_candidates_, 0, "012|789");
    CheckResult("");
  }
}

TEST_F(PunctuationContextTest, Config) {
  session_config_->full_width_word_mode = false;
  session_config_->full_width_punctuation_mode = true;
  session_config_->simplified_chinese_mode = true;

  {  // Full width punctuation with simplified chinese
    context_->Clear();
    EXPECT_CALL(*table_, GetDirectCommitTextForSimplifiedChinese('!', _))
        .Times(1).WillOnce(Return(true));
    EXPECT_TRUE(context_->Insert('!'));
  }

  {  // Full width punctuation with traditional chinese
    session_config_->simplified_chinese_mode = false;
    context_->Clear();
    EXPECT_CALL(*table_, GetDirectCommitTextForTraditionalChinese('!', _))
        .Times(1).WillOnce(Return(true));
    EXPECT_TRUE(context_->Insert('!'));
  }

  {  // Half width punctuation
    session_config_->full_width_punctuation_mode = false;
    context_->Clear();
    EXPECT_CALL(*table_, GetDirectCommitTextForSimplifiedChinese('!', _))
        .Times(0);
    EXPECT_CALL(*table_, GetDirectCommitTextForTraditionalChinese('!', _))
        .Times(0);
    EXPECT_TRUE(context_->Insert('!'));
  }

  {
    SCOPED_TRACE("Full width word");
    session_config_->full_width_word_mode = false;
    context_->Clear();
    EXPECT_TRUE(context_->Insert('!'));
    CheckResult("!");
  }

  {
    SCOPED_TRACE("Half width word");
    session_config_->full_width_word_mode = true;
    context_->Clear();
    EXPECT_TRUE(context_->Insert('!'));
    CheckResult("\xEF\xBC\x81");  // "！"
  }
}

TEST_F(PunctuationContextTest, ToggleQuotes) {
  const char *kOpenSingleQuote = "\xE2\x80\x98";  // "‘"
  const char *kCloseSingleQuote = "\xE2\x80\x99";  // "’"
  const char *kOpenDoubleQuote = "\xE2\x80\x9C";  // "“"
  const char *kCloseDoubleQuote = "\xE2\x80\x9D";  // "”"

  EXPECT_CALL(*table_, GetDirectCommitTextForSimplifiedChinese('\'', _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(kOpenSingleQuote), Return(true)));
  EXPECT_CALL(*table_, GetDirectCommitTextForSimplifiedChinese('"', _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(kOpenDoubleQuote), Return(true)));
  EXPECT_CALL(*table_, GetDirectCommitTextForSimplifiedChinese('a', _))
      .WillRepeatedly(DoAll(SetArgPointee<1>("a"), Return(true)));

  context_->Insert('\'');
  EXPECT_EQ(kOpenSingleQuote, context_->commit_text());
  context_->Insert('\'');
  EXPECT_EQ(kCloseSingleQuote, context_->commit_text());
  context_->Insert('\'');
  EXPECT_EQ(kOpenSingleQuote, context_->commit_text());

  context_->Insert('"');
  EXPECT_EQ(kOpenDoubleQuote, context_->commit_text());
  context_->Insert('"');
  EXPECT_EQ(kCloseDoubleQuote, context_->commit_text());
  context_->Insert('"');
  EXPECT_EQ(kOpenDoubleQuote, context_->commit_text());

  context_->ClearAll();
  // Opening quotes should be commited.
  context_->Insert('\'');
  EXPECT_EQ(kOpenSingleQuote, context_->commit_text());
  context_->Insert('"');
  EXPECT_EQ(kOpenDoubleQuote, context_->commit_text());

  context_->Insert('a');
  ASSERT_EQ("a", context_->commit_text());
  // Closing quotes should be commited.
  context_->Insert('\'');
  EXPECT_EQ(kCloseSingleQuote, context_->commit_text());
  context_->Insert('"');
  EXPECT_EQ(kCloseDoubleQuote, context_->commit_text());
}

TEST_F(PunctuationContextTest, PeriodAfterDigit) {
  const char *kDot = "\xE3\x80\x82";  // "。"

  EXPECT_CALL(*table_, GetDirectCommitTextForSimplifiedChinese('.', _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(kDot), Return(true)));

  context_->Insert('.');
  EXPECT_EQ(kDot, context_->commit_text());

  context_->UpdatePreviousCommitText("0");
  context_->Insert('.');
  EXPECT_EQ(".", context_->commit_text());
}

}  // namespace punctuation
}  // namespace pinyin
}  // namespace mozc
