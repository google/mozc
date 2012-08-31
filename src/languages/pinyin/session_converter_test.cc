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

#include "base/util.h"
#include "languages/pinyin/pinyin_constant.h"
#include "languages/pinyin/pinyin_context_interface.h"
#include "languages/pinyin/session_config.h"
#include "languages/pinyin/session_converter.h"
#include "session/commands.pb.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"

using ::testing::Invoke;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgPointee;
using ::testing::_;

namespace mozc {
namespace pinyin {

namespace {
const char *kCommitText = "\xE4\xBD\xA0\xE5\xA5\xBD";  // "你好"
const char *kInputText = "input";
const char *kSelectedText = "\xE9\x80\x89\xE5\x87\xBA";  // "选出"
const char *kConversionText = "\xE5\x8F\x98\xE6\x8D\xA2";  // "变换"
const char *kRestText = "\xE6\xAE\x8B\xE4\xBD\x99";  // "残余"
const char *kAuxiliaryText = "auxiliary";
const char *kCandidateText = "\xE5\x80\x99\xE9\x80\x89" "%d";  // "候选"
const size_t kFocusedCandidateIndex = 6;
const size_t kCandidatesSize = 7;
const size_t kPageSize = 5;

class MockContext : public PinyinContextInterface {
 public:
  MockContext() : candidates_size_(0) {}
  virtual ~MockContext() {}

  MOCK_METHOD1(Insert, bool(char ch));
  MOCK_METHOD0(Commit, void());
  MOCK_METHOD0(CommitPreedit, void());
  MOCK_METHOD0(Clear, void());
  MOCK_METHOD0(ClearCommitText, void());

  MOCK_METHOD0(MoveCursorRight,  bool());
  MOCK_METHOD0(MoveCursorLeft, bool());
  MOCK_METHOD0(MoveCursorRightByWord, bool());
  MOCK_METHOD0(MoveCursorLeftByWord, bool());
  MOCK_METHOD0(MoveCursorToBeginning, bool());
  MOCK_METHOD0(MoveCursorToEnd, bool());

  MOCK_METHOD1(SelectCandidate, bool(size_t index));
  MOCK_METHOD1(FocusCandidate, bool(size_t index));
  MOCK_METHOD0(FocusCandidatePrev, bool());
  MOCK_METHOD0(FocusCandidateNext, bool());
  MOCK_METHOD1(ClearCandidateFromHistory, bool(size_t index));

  MOCK_METHOD0(RemoveCharBefore, bool());
  MOCK_METHOD0(RemoveCharAfter, bool());
  MOCK_METHOD0(RemoveWordBefore, bool());
  MOCK_METHOD0(RemoveWordAfter, bool());

  MOCK_METHOD0(ReloadConfig, void());
  MOCK_METHOD1(SwitchContext, void(int mode));

  MOCK_CONST_METHOD0(commit_text, const string &());
  MOCK_CONST_METHOD0(input_text, const string &());
  MOCK_CONST_METHOD0(selected_text, const string &());
  MOCK_CONST_METHOD0(conversion_text, const string &());
  MOCK_CONST_METHOD0(rest_text, const string &());
  MOCK_CONST_METHOD0(auxiliary_text, const string &());

  MOCK_CONST_METHOD0(cursor, size_t());
  MOCK_CONST_METHOD0(focused_candidate_index, size_t());

  virtual bool HasCandidate(size_t index) {
    return index < candidates_size_;
  }

  virtual bool GetCandidate(size_t index, Candidate *candidate) {
    if (!HasCandidate(index)) {
      return false;
    }
    candidate->text = Util::StringPrintf(kCandidateText, index);
    return true;
  }

  virtual size_t PrepareCandidates(size_t required_size) {
    return min(candidates_size_, required_size);
  }

  void set_candidates_size(size_t size) {
    candidates_size_ = size;
  }

 private:
  size_t candidates_size_;
};

class MockPunctuationContext : public punctuation::PunctuationContext {
 public:
  explicit MockPunctuationContext(const SessionConfig &session_config)
      : punctuation::PunctuationContext(session_config) {}
  virtual ~MockPunctuationContext() {}

  MOCK_METHOD0(Clear, void());
  MOCK_METHOD0(ClearAll, void());
  MOCK_METHOD1(UpdatePreviousCommitText, void(const string &text));
};
}  // namespace

class SessionConverterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    converter_.reset(new SessionConverter(session_config_));

    // MockContext objects convert a ASCII-sequence to full-width and upper
    // case.
    context_ = new MockContext;
    punctuation_context_ = new MockPunctuationContext(session_config_);

    converter_->pinyin_context_.reset(context_);
    converter_->direct_context_.reset(new MockContext);
    converter_->english_context_.reset(new MockContext);
    converter_->punctuation_context_.reset(punctuation_context_);
    converter_->context_ = context_;

    ClearMockVariables();
  }

  virtual void TearDown() {
  }

  void InsertCharacterChars(const string &chars) {
    for (size_t i = 0; i < chars.size(); ++i) {
      commands::KeyEvent key_event;
      key_event.set_key_code(chars[i]);
      converter_->Insert(key_event);
    }
  }

  void ClearMockVariables() {
    candidates_.clear();
    input_text_.clear();
    selected_text_.clear();
    conversion_text_.clear();
    rest_text_.clear();
    auxiliary_text_.clear();
    commit_text_.clear();
  }

  void SetUpMockForFillOutput(bool has_input, bool has_result,
                              bool has_conversion, bool has_candidates,
                              bool has_auxiliary_text) {
    ClearMockVariables();

    if (has_result) {
      commit_text_.assign(kCommitText);
    }
    EXPECT_CALL(*context_, commit_text()).
        WillRepeatedly(ReturnRef(commit_text_));

    if (has_input) {
      input_text_.assign(kInputText);
    }

    if (has_conversion) {
      selected_text_.assign(kSelectedText);
      conversion_text_.assign(kConversionText);
      rest_text_.assign(kRestText);
    }

    EXPECT_CALL(*context_, input_text()).WillRepeatedly(ReturnRef(input_text_));
    EXPECT_CALL(*context_, selected_text())
        .WillRepeatedly(ReturnRef(selected_text_));
    EXPECT_CALL(*context_, conversion_text())
        .WillRepeatedly(ReturnRef(conversion_text_));
    EXPECT_CALL(*context_, rest_text()).WillRepeatedly(ReturnRef(rest_text_));

    if (has_candidates) {
      context_->set_candidates_size(kCandidatesSize);
      EXPECT_CALL(*context_, focused_candidate_index())
          .WillRepeatedly(Return(kFocusedCandidateIndex));
    } else {
      context_->set_candidates_size(0);
      EXPECT_CALL(*context_, focused_candidate_index())
          .WillRepeatedly(Return(0));
    }

    if (has_auxiliary_text) {
      auxiliary_text_.assign(kAuxiliaryText);
    }
    EXPECT_CALL(*context_, auxiliary_text())
        .WillRepeatedly(ReturnRef(auxiliary_text_));
  }

  void CheckOutput(const commands::Output &output,
                   bool has_input, bool has_result, bool has_conversion,
                   bool has_candidates, bool has_auxiliary_text) {
    // Currently has_input itself does not make sense on this method.

    if (has_result) {
      ASSERT_TRUE(output.has_result());
      EXPECT_EQ(kCommitText, output.result().value());
      EXPECT_EQ(commands::Result::STRING, output.result().type());
    } else {
      EXPECT_FALSE(output.has_result());
    }

    if (has_conversion) {
      ASSERT_TRUE(output.has_preedit());
      const commands::Preedit &preedit = output.preedit();
      ASSERT_EQ(3, preedit.segment_size());

      EXPECT_EQ(selected_text_, preedit.segment(0).value());
      EXPECT_EQ(Util::CharsLen(selected_text_),
                preedit.segment(0).value_length());
      EXPECT_EQ(conversion_text_, preedit.segment(1).value());
      EXPECT_EQ(Util::CharsLen(conversion_text_),
                preedit.segment(1).value_length());
      EXPECT_EQ(rest_text_, preedit.segment(2).value());
      EXPECT_EQ(Util::CharsLen(rest_text_), preedit.segment(2).value_length());

      EXPECT_EQ(commands::Preedit::Segment::UNDERLINE,
                preedit.segment(0).annotation());
      EXPECT_EQ(commands::Preedit::Segment::HIGHLIGHT,
                preedit.segment(1).annotation());
      EXPECT_EQ(commands::Preedit::Segment::UNDERLINE,
                preedit.segment(2).annotation());
      EXPECT_EQ(Util::CharsLen(kSelectedText), preedit.highlighted_position());
      EXPECT_EQ(Util::CharsLen(kSelectedText), preedit.cursor());
    } else {
      EXPECT_FALSE(output.has_preedit());
    }

    if (has_candidates || has_auxiliary_text) {
      ASSERT_TRUE(output.has_candidates());
      const commands::Candidates &candidates = output.candidates();

      EXPECT_EQ(commands::Candidates::HORIZONTAL, candidates.direction());
      EXPECT_EQ(commands::MAIN, candidates.display_type());
      EXPECT_EQ(Util::CharsLen(selected_text_), candidates.position());

      // It is very high cost to get a accurate candidates size, so we set
      // a non-zero dummy value on SessionConverter.
      const size_t kDummyCandidatesSize = 0xFFFFFFFF;
      EXPECT_EQ(kDummyCandidatesSize, candidates.size());

      if (has_candidates) {
        EXPECT_EQ(kFocusedCandidateIndex, candidates.focused_index());
        EXPECT_EQ(kCandidatesSize - kPageSize, candidates.candidate_size());
        EXPECT_EQ(kDummyCandidatesSize, candidates.size());

        for (size_t i = 0; i < kCandidatesSize - kPageSize; ++i) {
          const commands::Candidates::Candidate &c = candidates.candidate(i);
          const size_t candidate_index = i + kPageSize;
          EXPECT_EQ(candidate_index, c.id());
          EXPECT_EQ(candidate_index, c.index());
          EXPECT_EQ(Util::StringPrintf(kCandidateText, candidate_index),
                    c.value());
          EXPECT_TRUE(c.has_annotation());
          EXPECT_EQ(Util::StringPrintf("%d", i + 1), c.annotation().shortcut());
        }
      } else {
        EXPECT_FALSE(candidates.has_focused_index());
        EXPECT_EQ(0, candidates.candidate_size());
      }

      if (has_auxiliary_text) {
        ASSERT_TRUE(candidates.has_footer());
        const commands::Footer &footer = candidates.footer();
        EXPECT_EQ(kAuxiliaryText, footer.label());
        ASSERT_TRUE(footer.has_index_visible());
        EXPECT_FALSE(footer.index_visible());
      } else {
        EXPECT_FALSE(candidates.has_footer());
      }
    } else {
      EXPECT_FALSE(output.has_candidates());
    }
  }

  PinyinContextInterface *GetCurrentContext() {
    return converter_->context_;
  }

  MockContext* context_;
  MockPunctuationContext *punctuation_context_;
  scoped_ptr<SessionConverter> converter_;

  // variables for mock
  vector<Candidate> candidates_;
  SessionConfig session_config_;
  string input_text_;
  string selected_text_;
  string conversion_text_;
  string rest_text_;
  string auxiliary_text_;
  string commit_text_;
};

TEST_F(SessionConverterTest, IsConverterActive) {
  string input_text;
  EXPECT_CALL(*context_, input_text()).WillOnce(ReturnRef(input_text));
  EXPECT_FALSE(converter_->IsConverterActive());
  input_text = "a";
  EXPECT_CALL(*context_, input_text()).WillOnce(ReturnRef(input_text));
  EXPECT_TRUE(converter_->IsConverterActive());
}

TEST_F(SessionConverterTest, Insert) {
  const string commit_text;
  EXPECT_CALL(*context_, commit_text()).WillRepeatedly(ReturnRef(commit_text));

  commands::KeyEvent key_event;
  key_event.set_key_code('a');
  EXPECT_CALL(*context_, Insert('a')).WillOnce(Return(true));
  EXPECT_TRUE(converter_->Insert(key_event));
  EXPECT_CALL(*context_, Insert('a')).WillOnce(Return(false));
  EXPECT_FALSE(converter_->Insert(key_event));

  key_event.add_modifier_keys(commands::KeyEvent::SHIFT);
  EXPECT_CALL(*context_, Insert('A')).WillOnce(Return(true));
  EXPECT_TRUE(converter_->Insert(key_event));
  EXPECT_CALL(*context_, Insert('A')).WillOnce(Return(false));
  EXPECT_FALSE(converter_->Insert(key_event));

  key_event.add_modifier_keys(commands::KeyEvent::CTRL);
  EXPECT_CALL(*context_, Insert(_)).Times(0);
  EXPECT_FALSE(converter_->Insert(key_event));
}

TEST_F(SessionConverterTest, Clear) {
  EXPECT_CALL(*context_, Clear()).Times(1);
  EXPECT_CALL(*punctuation_context_, ClearAll()).Times(1);
  converter_->Clear();
}

TEST_F(SessionConverterTest, Commit) {
  const string kText = kCommitText;

  EXPECT_CALL(*context_, Commit()).Times(1);
  EXPECT_CALL(*context_, commit_text()).WillOnce(ReturnRef(kText));
  EXPECT_CALL(*punctuation_context_,
              UpdatePreviousCommitText(kText)).Times(1);
  converter_->Commit();

  EXPECT_CALL(*context_, CommitPreedit()).Times(1);
  EXPECT_CALL(*context_, commit_text()).WillOnce(ReturnRef(kText));
  EXPECT_CALL(*punctuation_context_,
              UpdatePreviousCommitText(kText)).Times(1);
  converter_->CommitPreedit();
}

TEST_F(SessionConverterTest, Remove) {
  EXPECT_CALL(*context_, RemoveCharBefore()).WillOnce(Return(true));
  EXPECT_TRUE(converter_->RemoveCharBefore());
  EXPECT_CALL(*context_, RemoveCharBefore()).WillOnce(Return(false));
  EXPECT_FALSE(converter_->RemoveCharBefore());

  EXPECT_CALL(*context_, RemoveCharAfter()).WillOnce(Return(true));
  EXPECT_TRUE(converter_->RemoveCharAfter());
  EXPECT_CALL(*context_, RemoveCharAfter()).WillOnce(Return(false));
  EXPECT_FALSE(converter_->RemoveCharAfter());

  EXPECT_CALL(*context_, RemoveWordBefore()).WillOnce(Return(true));
  EXPECT_TRUE(converter_->RemoveWordBefore());
  EXPECT_CALL(*context_, RemoveWordBefore()).WillOnce(Return(false));
  EXPECT_FALSE(converter_->RemoveWordBefore());

  EXPECT_CALL(*context_, RemoveWordAfter()).WillOnce(Return(true));
  EXPECT_TRUE(converter_->RemoveWordAfter());
  EXPECT_CALL(*context_, RemoveWordAfter()).WillOnce(Return(false));
  EXPECT_FALSE(converter_->RemoveWordAfter());
}

TEST_F(SessionConverterTest, MoveCursor) {
  EXPECT_CALL(*context_, MoveCursorLeft()).WillOnce(Return(true));
  EXPECT_TRUE(converter_->MoveCursorLeft());
  EXPECT_CALL(*context_, MoveCursorLeft()).WillOnce(Return(false));
  EXPECT_FALSE(converter_->MoveCursorLeft());

  EXPECT_CALL(*context_, MoveCursorRight()).WillOnce(Return(true));
  EXPECT_TRUE(converter_->MoveCursorRight());
  EXPECT_CALL(*context_, MoveCursorRight()).WillOnce(Return(false));
  EXPECT_FALSE(converter_->MoveCursorRight());

  EXPECT_CALL(*context_, MoveCursorLeftByWord()).WillOnce(Return(true));
  EXPECT_TRUE(converter_->MoveCursorLeftByWord());
  EXPECT_CALL(*context_, MoveCursorLeftByWord()).WillOnce(Return(false));
  EXPECT_FALSE(converter_->MoveCursorLeftByWord());

  EXPECT_CALL(*context_, MoveCursorRightByWord()).WillOnce(Return(true));
  EXPECT_TRUE(converter_->MoveCursorRightByWord());
  EXPECT_CALL(*context_, MoveCursorRightByWord()).WillOnce(Return(false));
  EXPECT_FALSE(converter_->MoveCursorRightByWord());

  EXPECT_CALL(*context_, MoveCursorToBeginning()).WillOnce(Return(true));
  EXPECT_TRUE(converter_->MoveCursorToBeginning());
  EXPECT_CALL(*context_, MoveCursorToBeginning()).WillOnce(Return(false));
  EXPECT_FALSE(converter_->MoveCursorToBeginning());

  EXPECT_CALL(*context_, MoveCursorToEnd()).WillOnce(Return(true));
  EXPECT_TRUE(converter_->MoveCursorToEnd());
  EXPECT_CALL(*context_, MoveCursorToEnd()).WillOnce(Return(false));
  EXPECT_FALSE(converter_->MoveCursorToEnd());
}

TEST_F(SessionConverterTest, SelectCandidate) {
  context_->set_candidates_size(8);
  EXPECT_CALL(*context_, focused_candidate_index())
      .WillRepeatedly(Return(6));

  EXPECT_CALL(*context_, SelectCandidate(7)).WillOnce(Return(true));
  EXPECT_TRUE(converter_->SelectCandidateOnPage(2));
  EXPECT_CALL(*context_, SelectCandidate(_)).Times(0);
  EXPECT_FALSE(converter_->SelectCandidateOnPage(4));

  EXPECT_CALL(*context_, SelectCandidate(6)).WillOnce(Return(true));
  EXPECT_TRUE(converter_->SelectFocusedCandidate());
}

TEST_F(SessionConverterTest, FocusCandidate) {
  context_->set_candidates_size(8);
  EXPECT_CALL(*context_, focused_candidate_index()).WillRepeatedly(Return(6));

  EXPECT_CALL(*context_, FocusCandidate(1)).WillOnce(Return(true));
  EXPECT_TRUE(converter_->FocusCandidate(1));
  EXPECT_CALL(*context_, FocusCandidate(_)).Times(0);
  EXPECT_FALSE(converter_->FocusCandidate(10));

  EXPECT_CALL(*context_, FocusCandidate(7)).WillOnce(Return(true));
  EXPECT_TRUE(converter_->FocusCandidateOnPage(2));
  EXPECT_CALL(*context_, FocusCandidate(_)).Times(0);
  EXPECT_FALSE(converter_->FocusCandidateOnPage(4));

  EXPECT_CALL(*context_, FocusCandidate(7)).WillOnce(Return(true));
  EXPECT_TRUE(converter_->FocusCandidateNext());
  EXPECT_CALL(*context_, FocusCandidate(7)).WillOnce(Return(false));
  EXPECT_FALSE(converter_->FocusCandidateNext());

  EXPECT_CALL(*context_, FocusCandidate(5)).WillOnce(Return(true));
  EXPECT_TRUE(converter_->FocusCandidatePrev());
  EXPECT_CALL(*context_, FocusCandidate(5)).WillOnce(Return(false));
  EXPECT_FALSE(converter_->FocusCandidatePrev());

  // FocusCandidateNextPage

  context_->set_candidates_size(10);
  EXPECT_CALL(*context_, focused_candidate_index()).WillRepeatedly(Return(6));
  EXPECT_CALL(*context_, FocusCandidate(_)).Times(0);
  EXPECT_FALSE(converter_->FocusCandidateNextPage());

  context_->set_candidates_size(11);
  EXPECT_CALL(*context_, focused_candidate_index()).WillRepeatedly(Return(6));
  EXPECT_CALL(*context_, FocusCandidate(10)).WillOnce(Return(true));
  EXPECT_TRUE(converter_->FocusCandidateNextPage());
  EXPECT_CALL(*context_, FocusCandidate(10)).WillOnce(Return(false));
  EXPECT_FALSE(converter_->FocusCandidateNextPage());

  context_->set_candidates_size(13);
  EXPECT_CALL(*context_, focused_candidate_index()).WillRepeatedly(Return(6));
  EXPECT_CALL(*context_, FocusCandidate(11)).WillOnce(Return(true));
  EXPECT_TRUE(converter_->FocusCandidateNextPage());
  EXPECT_CALL(*context_, FocusCandidate(11)).WillOnce(Return(false));
  EXPECT_FALSE(converter_->FocusCandidateNextPage());

  // FocusCandidatePrevPage

  context_->set_candidates_size(4);
  EXPECT_CALL(*context_, focused_candidate_index()).WillRepeatedly(Return(3));
  EXPECT_CALL(*context_, FocusCandidate(_)).Times(0);
  EXPECT_FALSE(converter_->FocusCandidatePrevPage());

  context_->set_candidates_size(8);
  EXPECT_CALL(*context_, focused_candidate_index()).WillRepeatedly(Return(6));
  EXPECT_CALL(*context_, FocusCandidate(1)).WillOnce(Return(true));
  EXPECT_TRUE(converter_->FocusCandidatePrevPage());
  EXPECT_CALL(*context_, FocusCandidate(1)).WillOnce(Return(false));
  EXPECT_FALSE(converter_->FocusCandidatePrevPage());
}

TEST_F(SessionConverterTest, ClearCandidateFromHistory) {
  context_->set_candidates_size(8);
  EXPECT_CALL(*context_, focused_candidate_index()).WillRepeatedly(Return(6));

  EXPECT_CALL(*context_, ClearCandidateFromHistory(7)).WillOnce(Return(true));
  EXPECT_TRUE(converter_->ClearCandidateFromHistory(2));
  EXPECT_CALL(*context_, ClearCandidateFromHistory(_)).Times(0);
  EXPECT_FALSE(converter_->ClearCandidateFromHistory(10));
}

TEST_F(SessionConverterTest, FillOutputAndPopOutput) {
  // Has Input, Result, Conversion, Candidates, Auxiliary text or not.
  const bool kStates[][5] = {
    {false, false, false, false, false},  // Initial state
    {false, true,  false, false, false},  // Commited state
    {true,  false, true,  true,  true},   // Conversion state
    {true,  false, false, false, true},   // Auxiliary text only (English mode)
  };

  for (int i = 0; i < ARRAYSIZE_UNSAFE(kStates); ++i) {
    const bool *state = kStates[i];

    {
      SCOPED_TRACE(Util::StringPrintf("FillOutput i=%d", i));
      commands::Output output;
      SetUpMockForFillOutput(state[0], state[1], state[2], state[3], state[4]);
      converter_->FillOutput(&output);
      CheckOutput(output, state[0], state[1], state[2], state[3], state[4]);
    }

    {
      SCOPED_TRACE(Util::StringPrintf("FillOutput i=%d", i));
      EXPECT_CALL(*context_, ClearCommitText()).Times(1);
      commands::Output output;
      SetUpMockForFillOutput(state[0], state[1], state[2], state[3], state[4]);
      converter_->PopOutput(&output);
      CheckOutput(output, state[0], state[1], state[2], state[3], state[4]);
    }
  }
}

TEST_F(SessionConverterTest, ReloadConfig) {
  EXPECT_CALL(*context_, ReloadConfig()).Times(1);
  converter_->ReloadConfig();
}

TEST_F(SessionConverterTest, SelectWithNoCandidate_Issue6121366) {
  context_->set_candidates_size(0);
  EXPECT_CALL(*context_, focused_candidate_index())
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*context_, Commit()).Times(1);
  EXPECT_TRUE(converter_->SelectFocusedCandidate());
}

TEST_F(SessionConverterTest, SwitchConversionMode) {
  ASSERT_EQ(context_, GetCurrentContext());

  EXPECT_CALL(*context_, Clear()).Times(1);
  converter_->SwitchContext(PUNCTUATION);
  EXPECT_EQ(punctuation_context_, GetCurrentContext());

  EXPECT_CALL(*punctuation_context_, Clear()).Times(1);
  converter_->SwitchContext(PINYIN);
  EXPECT_EQ(context_, GetCurrentContext());
}

}  // namespace pinyin
}  // namespace mozc
