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

#include "languages/pinyin/pinyin_context.h"

#include <PyZy/Const.h>
#include <PyZy/InputContext.h>
#include <PyZy/Variant.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "languages/pinyin/session_config.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace pinyin {

// This test depends on libpyzy and conversion results of libpyzy depends on
// installed dictionary and input history. We expect some conversion (e.g.
// "nihao" should be "你好"), but it is NOT ensured. For the reason, this test
// can be unstalbe.
//
// TODO(hsumita): Create a test dictionary to libpyzy.
// TODO(hsumita): Add incognito mode to libpyzy.
// TODO(hsumita): Add a test case for ClearCandidateFromHistory.

namespace {
// "你好"
const char *kNihao = "\xE4\xBD\xA0\xE5\xA5\xBD";
// "你"
const char *kNi = "\xE4\xBD\xA0";
// "好"
const char *kHao = "\xE5\xA5\xBD";
}  // namespace

class PinyinContextTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    PyZy::InputContext::init(FLAGS_test_tmpdir, FLAGS_test_tmpdir);

    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);

    session_config_.reset(new SessionConfig);
    session_config_->full_width_word_mode = false;
    session_config_->full_width_punctuation_mode = true;
    session_config_->simplified_chinese_mode = true;

    context_.reset(new PinyinContext(*session_config_));
  }

  virtual void TearDown() {
    PyZy::InputContext::finalize();

    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  void InsertCharacterChars(const string &chars) {
    for (size_t i = 0; i < chars.size(); ++i) {
      EXPECT_TRUE(context_->Insert(chars[i]));
    }
  }

  bool FindCandidateIndex(const string &expected_candidate, size_t *index) {
    Candidate candidate;
    for (size_t i = 0; context_->GetCandidate(i, &candidate); ++i) {
      if (candidate.text == expected_candidate) {
        *index = i;
        return true;
      }
    }

    LOG(ERROR) << "Can't find candidate index";
    return false;
  }

  void CheckTextAccessors(const string &commit_text,
                          const string &input_text,
                          const string &selected_text,
                          const string &conversion_text,
                          const string &rest_text,
                          const string &auxiliary_text) {
    EXPECT_EQ(commit_text, context_->commit_text());
    EXPECT_EQ(input_text, context_->input_text());
    EXPECT_EQ(selected_text, context_->selected_text());
    EXPECT_EQ(conversion_text, context_->conversion_text());
    EXPECT_EQ(rest_text, context_->rest_text());
    EXPECT_EQ(auxiliary_text, context_->auxiliary_text());
  }

  PyZy::InputContext *GetRawContext() const {
    return context_->context_.get();
  }

  scoped_ptr<SessionConfig> session_config_;
  scoped_ptr<PinyinContext> context_;
};

TEST_F(PinyinContextTest, InsertAndClear) {
  {
    SCOPED_TRACE("Initial state");
    CheckTextAccessors("", "", "", "", "", "");
    EXPECT_EQ("", context_->auxiliary_text());
  }

  {
    SCOPED_TRACE("Input nihao and check accessors");
    InsertCharacterChars("nihao");

    CheckTextAccessors("", "nihao", "", kNihao, "", "ni hao|");
    EXPECT_EQ(0, context_->focused_candidate_index());
    EXPECT_EQ(5, context_->cursor());
    EXPECT_TRUE(context_->HasCandidate(0));

    Candidate candidate;
    ASSERT_TRUE(context_->GetCandidate(0, &candidate));
    EXPECT_EQ(kNihao, candidate.text);
  }

  {
    SCOPED_TRACE("Clear state");
    context_->Clear();
    CheckTextAccessors("", "", "", "", "", "");
    EXPECT_EQ(0, context_->focused_candidate_index());
    EXPECT_EQ(0, context_->cursor());
    EXPECT_FALSE(context_->HasCandidate(0));
  }
}

TEST_F(PinyinContextTest, SelectAndCommit) {
  InsertCharacterChars("nihao");

  {
    SCOPED_TRACE("Commit");
    context_->Commit();
    // commit_text should be "nihao" because we don't call SelectCandidate().
    CheckTextAccessors("nihao", "", "", "", "", "");
  }

  context_->Clear();
  InsertCharacterChars("nihao");

  {
    SCOPED_TRACE("Select first candidate");
    context_->SelectCandidate(0);
    CheckTextAccessors(kNihao, "", "", "", "", "");
  }

  context_->Clear();
  InsertCharacterChars("nihao");

  {
    SCOPED_TRACE("Select partially and commit");
    size_t ni_index;
    ASSERT_TRUE(FindCandidateIndex(kNi, &ni_index));
    context_->SelectCandidate(ni_index);
    CheckTextAccessors("", "nihao", kNi, kHao, "", "hao|");

    context_->Commit();
    CheckTextAccessors(string(kNi) + "hao", "", "", "", "", "");
  }

  context_->Clear();
  InsertCharacterChars("nihao");

  {
    SCOPED_TRACE("Select partially and commit preedit");
    size_t ni_index;
    ASSERT_TRUE(FindCandidateIndex(kNi, &ni_index));
    context_->SelectCandidate(ni_index);
    CheckTextAccessors("", "nihao", kNi, kHao, "", "hao|");

    context_->CommitPreedit();
    CheckTextAccessors("nihao", "", "", "", "", "");
  }
}

TEST_F(PinyinContextTest, CommitText) {
  InsertCharacterChars("nihao");
  context_->CommitPreedit();
  ASSERT_EQ("nihao", context_->commit_text());

  {
    SCOPED_TRACE("Clear commit text by Clear()");
    context_->Clear();
    CheckTextAccessors("", "", "", "", "", "");
  }

  context_->Clear();
  InsertCharacterChars("nihao");
  context_->CommitPreedit();
  ASSERT_EQ("nihao", context_->commit_text());

  {
    SCOPED_TRACE("Don't clear commit text by other functions");
    InsertCharacterChars("nihao");
    CheckTextAccessors("nihao", "nihao", "", kNihao, "", "ni hao|");
  }
}

TEST_F(PinyinContextTest, ClearTest) {
  InsertCharacterChars("nihao");
  ASSERT_EQ("nihao", context_->input_text());
  ASSERT_EQ("", context_->commit_text());

  {
    context_->ClearCommitText();
    EXPECT_EQ("nihao", context_->input_text());
    EXPECT_EQ("", context_->commit_text());
  }

  {
    context_->Clear();
    EXPECT_EQ("", context_->input_text());
    EXPECT_EQ("", context_->commit_text());
  }

  InsertCharacterChars("nihao");
  context_->CommitPreedit();
  ASSERT_EQ("", context_->input_text());
  ASSERT_EQ("nihao", context_->commit_text());

  {
    context_->ClearCommitText();
    EXPECT_EQ("", context_->input_text());
    EXPECT_EQ("", context_->commit_text());
  }

  InsertCharacterChars("nihao");
  context_->CommitPreedit();
  ASSERT_EQ("", context_->input_text());
  ASSERT_EQ("nihao", context_->commit_text());

  {
    context_->Clear();
    EXPECT_EQ("", context_->input_text());
    EXPECT_EQ("", context_->commit_text());
  }
}

TEST_F(PinyinContextTest, FocusCandidate) {
  InsertCharacterChars("nihao");
  ASSERT_TRUE(context_->HasCandidate(2));
  ASSERT_EQ(0, context_->focused_candidate_index());

  {
    SCOPED_TRACE("Focus 3rd candidate");
    EXPECT_TRUE(context_->FocusCandidate(2));
    EXPECT_EQ(2, context_->focused_candidate_index());
  }

  {
    SCOPED_TRACE("Focus 100th candidate and fail");
    EXPECT_FALSE(context_->FocusCandidate(99));
    EXPECT_EQ(2, context_->focused_candidate_index());
  }
}

TEST_F(PinyinContextTest, MoveCursor) {
  InsertCharacterChars("nihao");

  {
    SCOPED_TRACE("Move cursor left");
    context_->MoveCursorLeft();
    CheckTextAccessors("", "nihao", "", "ni ha|o", "", "ni ha|o");
    EXPECT_EQ(4, context_->cursor());
  }

  {
    SCOPED_TRACE("Move cursor left by word");
    context_->MoveCursorLeftByWord();
    CheckTextAccessors("", "nihao", "", "ni|hao", "", "ni|hao");
    EXPECT_EQ(2, context_->cursor());
  }

  {
    SCOPED_TRACE("Move cursor to beginning");
    context_->MoveCursorToBeginning();
    CheckTextAccessors("", "nihao", "", "", "nihao", "");
    EXPECT_EQ(0, context_->cursor());
  }

  {
    SCOPED_TRACE("Move cursor right");
    context_->MoveCursorRight();
    CheckTextAccessors("", "nihao", "", "n|ihao", "", "n|ihao");
    EXPECT_EQ(1, context_->cursor());
  }

  {
    // In current implementation of libpyzy, RemoveWordAfter removes all
    // characters after cursor.
    SCOPED_TRACE("Move cursor right by word");
    context_->MoveCursorRightByWord();
    CheckTextAccessors("", "nihao", "", kNihao, "", "ni hao|");
    EXPECT_EQ(5, context_->cursor());
  }

  context_->MoveCursorLeftByWord();
  ASSERT_EQ(2, context_->cursor());

  {
    SCOPED_TRACE("Move cursor to end");
    context_->MoveCursorToEnd();
    CheckTextAccessors("", "nihao", "", kNihao, "", "ni hao|");
    EXPECT_EQ(5, context_->cursor());
  }
}

TEST_F(PinyinContextTest, UnselectCandidates) {
  InsertCharacterChars("nihao");
  ASSERT_EQ("nihao", context_->input_text());
  ASSERT_TRUE(context_->selected_text().empty());

  size_t ni_index;
  ASSERT_TRUE(FindCandidateIndex(kNi, &ni_index));

  context_->SelectCandidate(ni_index);
  ASSERT_EQ(kNi, context_->selected_text());
  context_->MoveCursorLeft();
  EXPECT_TRUE(context_->selected_text().empty());

  context_->SelectCandidate(ni_index);
  ASSERT_EQ(kNi, context_->selected_text());
  context_->MoveCursorRight();
  EXPECT_TRUE(context_->selected_text().empty());

  context_->SelectCandidate(ni_index);
  ASSERT_EQ(kNi, context_->selected_text());
  context_->MoveCursorLeftByWord();
  EXPECT_TRUE(context_->selected_text().empty());

  context_->SelectCandidate(ni_index);
  ASSERT_EQ(kNi, context_->selected_text());
  context_->MoveCursorRightByWord();
  EXPECT_TRUE(context_->selected_text().empty());

  context_->SelectCandidate(ni_index);
  ASSERT_EQ(kNi, context_->selected_text());
  context_->MoveCursorToBeginning();
  EXPECT_TRUE(context_->selected_text().empty());

  context_->SelectCandidate(ni_index);
  ASSERT_EQ(kNi, context_->selected_text());
  context_->MoveCursorToEnd();
  EXPECT_TRUE(context_->selected_text().empty());
}

TEST_F(PinyinContextTest, RemoveCharacters) {
  InsertCharacterChars("haohao");
  ASSERT_EQ("haohao", context_->input_text());

  {
    SCOPED_TRACE("Remove a previous character.");
    context_->RemoveCharBefore();
    EXPECT_EQ("haoha", context_->input_text());
  }

  {
    SCOPED_TRACE("Remove a previous word.");
    context_->RemoveWordBefore();
    EXPECT_EQ("hao", context_->input_text());
  }

  context_->MoveCursorToBeginning();
  ASSERT_EQ(0, context_->cursor());

  {
    SCOPED_TRACE("Remove a next character.");
    context_->RemoveCharAfter();
    EXPECT_EQ("ao", context_->input_text());
  }

  {
    // In current implementation of libpyzy, RemoveWordAfter removes all
    // characters after cursor.
    SCOPED_TRACE("Remove a next word.");
    context_->RemoveWordAfter();
    EXPECT_EQ("", context_->input_text());
  }
}

namespace {
const uint32 kIncompletePinyinOption = PINYIN_INCOMPLETE_PINYIN;
const uint32 kCorrectPinyinOption = PINYIN_CORRECT_ALL;
const uint32 kFuzzyPinyinOption =
    PINYIN_FUZZY_C_CH |
    PINYIN_FUZZY_Z_ZH |
    PINYIN_FUZZY_S_SH |
    PINYIN_FUZZY_L_N |
    PINYIN_FUZZY_F_H |
    PINYIN_FUZZY_K_G |
    PINYIN_FUZZY_G_K |
    PINYIN_FUZZY_AN_ANG |
    PINYIN_FUZZY_ANG_AN |
    PINYIN_FUZZY_EN_ENG |
    PINYIN_FUZZY_ENG_EN |
    PINYIN_FUZZY_IN_ING |
    PINYIN_FUZZY_ING_IN;
}  // namespace

TEST_F(PinyinContextTest, ReloadConfig) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config::PinyinConfig *pinyin_config = config.mutable_pinyin_config();

  {  // full pinyin / double pinyin
    pinyin_config->set_double_pinyin(false);
    config::ConfigHandler::SetConfig(config);

    context_->ReloadConfig();
    InsertCharacterChars("nihao");
    EXPECT_EQ(kNihao, context_->conversion_text());

    pinyin_config->set_double_pinyin(true);
    config::ConfigHandler::SetConfig(config);

    context_->ReloadConfig();
    EXPECT_EQ("", context_->input_text());
    InsertCharacterChars("nihk");
    EXPECT_EQ(kNihao, context_->conversion_text());
  }

  const PyZy::InputContext *raw_context = GetRawContext();

  {  // conversion option
    const bool kInput[][2] = {
      {false, false}, {false, true}, {true, false}, {true, true},
    };

    for (size_t i = 0; i < arraysize(kInput); ++i) {
      const bool use_correct = kInput[i][0];
      const bool use_fuzzy = kInput[i][1];
      pinyin_config->set_fuzzy_pinyin(use_fuzzy);
      pinyin_config->set_correct_pinyin(use_correct);
      config::ConfigHandler::SetConfig(config);
      context_->ReloadConfig();
      const uint32 expected =
          kIncompletePinyinOption |
          (use_correct ? kCorrectPinyinOption : 0) |
          (use_fuzzy ? kFuzzyPinyinOption : 0);
      const uint32 actual = raw_context->getProperty(
          PyZy::InputContext::PROPERTY_CONVERSION_OPTION).getUnsignedInt();
      EXPECT_EQ(expected, actual) << use_fuzzy << "," << use_correct;
    }
  }

  {  // double pinyin schema
    ASSERT_EQ(DOUBLE_PINYIN_KEYBOARD_MSPY,
              pinyin_config->double_pinyin_schema());
    ASSERT_EQ(DOUBLE_PINYIN_KEYBOARD_MSPY, raw_context->getProperty(
        PyZy::InputContext::PROPERTY_DOUBLE_PINYIN_SCHEMA).getUnsignedInt());

    pinyin_config->set_double_pinyin_schema(DOUBLE_PINYIN_KEYBOARD_ZRM);
    config::ConfigHandler::SetConfig(config);
    context_->ReloadConfig();
    EXPECT_EQ(DOUBLE_PINYIN_KEYBOARD_ZRM, raw_context->getProperty(
        PyZy::InputContext::PROPERTY_DOUBLE_PINYIN_SCHEMA).getUnsignedInt());
  }

  {  // simplified chinese
    ASSERT_TRUE(session_config_->simplified_chinese_mode);
    ASSERT_TRUE(raw_context->getProperty(
        PyZy::InputContext::PROPERTY_MODE_SIMP).getBool());

    session_config_->simplified_chinese_mode = false;
    context_->ReloadConfig();
    EXPECT_FALSE(raw_context->getProperty(
        PyZy::InputContext::PROPERTY_MODE_SIMP).getBool());
  }
}

TEST_F(PinyinContextTest, FullWidthCommit) {
  session_config_->full_width_word_mode = false;
  InsertCharacterChars("nihao");
  context_->Commit();
  EXPECT_EQ("nihao", context_->commit_text());

  session_config_->full_width_word_mode = true;
  InsertCharacterChars("nihao");
  context_->Commit();
  // "ｎｉｈａｏ"
  EXPECT_EQ("\xEF\xBD\x8E\xEF\xBD\x89\xEF\xBD\x88\xEF\xBD\x81\xEF\xBD\x8F",
            context_->commit_text());
}

TEST_F(PinyinContextTest, InsertNumber_Issue6136903) {
  {  // Half width word mode
    session_config_->full_width_word_mode = false;

    context_->Clear();
    EXPECT_TRUE(context_->Insert('1'));
    EXPECT_EQ("1", context_->commit_text());

    context_->Clear();
    InsertCharacterChars("nihao");
    EXPECT_FALSE(context_->Insert('1'));
  }

  {  // Full width word mode
    session_config_->full_width_word_mode = true;

    context_->Clear();
    EXPECT_TRUE(context_->Insert('1'));
    // "１"
    EXPECT_EQ("\xEF\xBC\x91", context_->commit_text());

    context_->Clear();
    InsertCharacterChars("nihao");
    EXPECT_FALSE(context_->Insert('1'));
  }
}

}  // namespace pinyin
}  // namespace mozc
