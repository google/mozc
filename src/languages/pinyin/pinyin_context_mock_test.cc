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

#include "languages/pinyin/pinyin_context_mock.h"

#include <string>
#include <vector>

#include "base/util.h"
#include "languages/pinyin/pinyin_context_interface.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

// TODO(hsumita): Check return value of member functions of mock.

namespace mozc {
namespace pinyin {
namespace {
void InsertCharacterChars(const string &chars,
                          PinyinContextInterface *context) {
  for (size_t i = 0; i < chars.size(); ++i) {
    context->Insert(chars[i]);
  }
}

// Wrapper function. It takes only half width ASCII characters.
string ToFullWidthAscii(const string &half_width) {
  string full_width;
  Util::HalfWidthAsciiToFullWidthAscii(half_width, &full_width);
  return full_width;
}
}  // namespace

class PinyinContextMockTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  size_t GetCandidatesSize(PinyinContextInterface *context) {
    size_t size = 0;
    for (; context->HasCandidate(size); ++size) {}
    return size;
  }
};

TEST_F(PinyinContextMockTest, InsertTest) {
  PinyinContextMock context;

  {  // Insert "nihao" and commit without select a candidate.
    context.Clear();

    // Does nothing.
    context.Insert('A');
    EXPECT_EQ("", context.input_text());

    InsertCharacterChars("nihao", &context);
    EXPECT_EQ("", context.commit_text());
    EXPECT_EQ("nihao", context.input_text());
    EXPECT_EQ("auxiliary_text_nihao", context.auxiliary_text());
    EXPECT_EQ("", context.selected_text());
    EXPECT_EQ(ToFullWidthAscii("NIHAO"), context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(5, context.cursor());
    EXPECT_EQ(0, context.focused_candidate_index());
    ASSERT_EQ(5, GetCandidatesSize(&context));

    const string kBaseCandidate = "NIHAO";
    size_t size = kBaseCandidate.size();
    for (size_t i = 0; i < size; ++i) {
      Candidate candidate;
      EXPECT_TRUE(context.GetCandidate(i, &candidate));
      EXPECT_EQ(ToFullWidthAscii(kBaseCandidate.substr(0, size - i)),
                candidate.text);
    }

    // Does nothing.
    context.Insert('A');
    EXPECT_EQ("nihao", context.input_text());
  }
}

TEST_F(PinyinContextMockTest, CommitTest) {
  PinyinContextMock context;

  {  // Insert "nihao" and commit without select a candidate.
    context.Clear();
    InsertCharacterChars("nihao", &context);

    context.Commit();
    EXPECT_EQ("nihao", context.commit_text());
    EXPECT_EQ("", context.input_text());
    EXPECT_EQ("", context.auxiliary_text());
    EXPECT_EQ("", context.selected_text());
    EXPECT_EQ("", context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(0, context.cursor());
    EXPECT_EQ(0, context.focused_candidate_index());
    EXPECT_EQ(0, context.candidates_size());
  }

  {  // Insert "nihao" and select "NIHAO"
    context.Clear();
    InsertCharacterChars("nihao", &context);

    context.SelectCandidate(0);
    EXPECT_EQ(ToFullWidthAscii("NIHAO"), context.commit_text());
    EXPECT_EQ("", context.input_text());
    EXPECT_EQ("", context.auxiliary_text());
    EXPECT_EQ("", context.selected_text());
    EXPECT_EQ("", context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(0, context.cursor());
    EXPECT_EQ(0, context.focused_candidate_index());
    EXPECT_EQ(0, context.candidates_size());
  }

  {  // Insert "nihao", select "NI", focus "HA", commit "NIhao".
    context.Clear();
    InsertCharacterChars("nihao", &context);

    context.SelectCandidate(3);
    EXPECT_EQ("", context.commit_text());
    EXPECT_EQ("nihao", context.input_text());
    EXPECT_EQ("auxiliary_text_hao", context.auxiliary_text());
    EXPECT_EQ(ToFullWidthAscii("NI"), context.selected_text());
    EXPECT_EQ(ToFullWidthAscii("HAO"), context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(5, context.cursor());
    EXPECT_EQ(0, context.focused_candidate_index());
    EXPECT_EQ(3, context.candidates_size());

    context.FocusCandidate(1);
    EXPECT_EQ("", context.commit_text());
    EXPECT_EQ("nihao", context.input_text());
    EXPECT_EQ("auxiliary_text_hao", context.auxiliary_text());
    EXPECT_EQ(ToFullWidthAscii("NI"), context.selected_text());
    EXPECT_EQ(ToFullWidthAscii("HA"), context.conversion_text());
    EXPECT_EQ("o", context.rest_text());
    EXPECT_EQ(5, context.cursor());
    EXPECT_EQ(1, context.focused_candidate_index());
    EXPECT_EQ(3, context.candidates_size());

    context.Commit();
    EXPECT_EQ(ToFullWidthAscii("NI") + "hao", context.commit_text());
    EXPECT_EQ("", context.input_text());
    EXPECT_EQ("", context.auxiliary_text());
    EXPECT_EQ("", context.selected_text());
    EXPECT_EQ("", context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(0, context.cursor());
    EXPECT_EQ(0, context.focused_candidate_index());
    EXPECT_EQ(0, context.candidates_size());
  }

  {  // insert "nihao", select "NI", commit preedit.
    context.Clear();

    InsertCharacterChars("nihao", &context);

    context.SelectCandidate(3);

    context.CommitPreedit();
    EXPECT_EQ("nihao", context.commit_text());
    EXPECT_EQ("", context.input_text());
    EXPECT_EQ("", context.auxiliary_text());
    EXPECT_EQ("", context.selected_text());
    EXPECT_EQ("", context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(0, context.cursor());
    EXPECT_EQ(0, context.focused_candidate_index());
    EXPECT_EQ(0, context.candidates_size());
  }
}

TEST_F(PinyinContextMockTest, MultiSegmentConversionTest) {
  PinyinContextMock context;

  context.Clear();

  InsertCharacterChars("abc", &context);

  context.MoveCursorLeft();
  EXPECT_EQ("", context.commit_text());
  EXPECT_EQ("abc", context.input_text());
  EXPECT_EQ("auxiliary_text_ab", context.auxiliary_text());
  EXPECT_EQ("", context.selected_text());
  EXPECT_EQ("ab", context.conversion_text());
  EXPECT_EQ("c", context.rest_text());
  EXPECT_EQ(2, context.cursor());
  EXPECT_EQ(0, context.focused_candidate_index());
  EXPECT_EQ(2, context.candidates_size());

  context.SelectCandidate(1);
  EXPECT_EQ("", context.commit_text());
  EXPECT_EQ("abc", context.input_text());
  EXPECT_EQ("auxiliary_text_b", context.auxiliary_text());
  EXPECT_EQ(ToFullWidthAscii("A"), context.selected_text());
  EXPECT_EQ("b", context.conversion_text());
  EXPECT_EQ("c", context.rest_text());
  EXPECT_EQ(2, context.cursor());
  EXPECT_EQ(0, context.focused_candidate_index());
  EXPECT_EQ(1, context.candidates_size());

  context.Commit();
  EXPECT_EQ(ToFullWidthAscii("A") + "bc", context.commit_text());
  EXPECT_EQ("", context.input_text());
  EXPECT_EQ("", context.auxiliary_text());
  EXPECT_EQ("", context.selected_text());
  EXPECT_EQ("", context.conversion_text());
  EXPECT_EQ("", context.rest_text());
  EXPECT_EQ(0, context.cursor());
  EXPECT_EQ(0, context.focused_candidate_index());
  EXPECT_EQ(0, context.candidates_size());
}

TEST_F(PinyinContextMockTest, FocusTest) {
  PinyinContextMock context;

  context.Clear();

  InsertCharacterChars("nihao", &context);
  EXPECT_EQ(ToFullWidthAscii("NIHAO"), context.conversion_text());
  EXPECT_EQ("", context.rest_text());
  EXPECT_EQ(0, context.focused_candidate_index());
  EXPECT_EQ(5, context.candidates_size());

  context.FocusCandidate(4);
  EXPECT_EQ(ToFullWidthAscii("N"), context.conversion_text());
  EXPECT_EQ("ihao", context.rest_text());
  EXPECT_EQ(4, context.focused_candidate_index());

  context.FocusCandidate(100);
  EXPECT_EQ(ToFullWidthAscii("N"), context.conversion_text());
  EXPECT_EQ("ihao", context.rest_text());
  EXPECT_EQ(4, context.focused_candidate_index());

  context.FocusCandidate(0);
  EXPECT_EQ(ToFullWidthAscii("NIHAO"), context.conversion_text());
  EXPECT_EQ("", context.rest_text());
  EXPECT_EQ(0, context.focused_candidate_index());
}

TEST_F(PinyinContextMockTest, CursorTest) {
  PinyinContextMock context;
  InsertCharacterChars("nihao", &context);

  { // Moving test
    EXPECT_EQ(ToFullWidthAscii("NIHAO"), context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(5, context.cursor());
    EXPECT_EQ(5, context.candidates_size());

    // Nothing occurs.
    context.MoveCursorRight();
    EXPECT_EQ(ToFullWidthAscii("NIHAO"), context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(5, context.cursor());
    EXPECT_EQ(5, context.candidates_size());

    // Nothing occurs.
    context.MoveCursorRightByWord();
    EXPECT_EQ(ToFullWidthAscii("NIHAO"), context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(5, context.cursor());
    EXPECT_EQ(5, context.candidates_size());

    context.MoveCursorLeft();
    EXPECT_EQ("niha", context.conversion_text());
    EXPECT_EQ("o", context.rest_text());
    EXPECT_EQ(4, context.cursor());
    EXPECT_EQ(4, context.candidates_size());

    context.MoveCursorLeftByWord();
    EXPECT_EQ("nih", context.conversion_text());
    EXPECT_EQ("ao", context.rest_text());
    EXPECT_EQ(3, context.cursor());
    EXPECT_EQ(3, context.candidates_size());

    context.MoveCursorLeftByWord();
    EXPECT_EQ("", context.conversion_text());
    EXPECT_EQ("nihao", context.rest_text());
    EXPECT_EQ(0, context.cursor());
    EXPECT_EQ(0, context.candidates_size());

    // Nothing occurs
    context.MoveCursorLeft();
    EXPECT_EQ("", context.conversion_text());
    EXPECT_EQ("nihao", context.rest_text());
    EXPECT_EQ(0, context.cursor());
    EXPECT_EQ(0, context.candidates_size());

    // Nothing occurs
    context.MoveCursorLeftByWord();
    EXPECT_EQ("", context.conversion_text());
    EXPECT_EQ("nihao", context.rest_text());
    EXPECT_EQ(0, context.cursor());
    EXPECT_EQ(0, context.candidates_size());

    context.MoveCursorRight();
    EXPECT_EQ("n", context.conversion_text());
    EXPECT_EQ("ihao", context.rest_text());
    EXPECT_EQ(1, context.cursor());
    EXPECT_EQ(1, context.candidates_size());

    context.MoveCursorRightByWord();
    EXPECT_EQ("nih", context.conversion_text());
    EXPECT_EQ("ao", context.rest_text());
    EXPECT_EQ(3, context.cursor());
    EXPECT_EQ(3, context.candidates_size());

    context.MoveCursorRightByWord();
    EXPECT_EQ(ToFullWidthAscii("NIHAO"), context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(5, context.cursor());
    EXPECT_EQ(5, context.candidates_size());

    context.MoveCursorLeftByWord();
    EXPECT_EQ("nih", context.conversion_text());
    EXPECT_EQ("ao", context.rest_text());
    EXPECT_EQ(3, context.cursor());
    EXPECT_EQ(3, context.candidates_size());

    context.MoveCursorToBeginning();
    EXPECT_EQ("", context.conversion_text());
    EXPECT_EQ("nihao", context.rest_text());
    EXPECT_EQ(0, context.cursor());
    EXPECT_EQ(0, context.candidates_size());

    context.MoveCursorRightByWord();
    EXPECT_EQ("nih", context.conversion_text());
    EXPECT_EQ("ao", context.rest_text());
    EXPECT_EQ(3, context.cursor());
    EXPECT_EQ(3, context.candidates_size());

    context.MoveCursorToEnd();
    EXPECT_EQ(ToFullWidthAscii("NIHAO"), context.conversion_text());
    EXPECT_EQ("", context.rest_text());
    EXPECT_EQ(5, context.cursor());
    EXPECT_EQ(5, context.candidates_size());
  }

  context.Clear();
  InsertCharacterChars("nihao", &context);

  {  // Confirms focused_candidate_index when cursor is not moved.
    context.FocusCandidate(1);
    ASSERT_EQ(1, context.focused_candidate_index());

    context.MoveCursorRight();
    EXPECT_EQ(1, context.focused_candidate_index());

    context.MoveCursorLeft();
    EXPECT_EQ(0, context.focused_candidate_index());

    context.FocusCandidate(2);
    ASSERT_EQ(2, context.focused_candidate_index());

    context.MoveCursorRight();
    EXPECT_EQ(0, context.focused_candidate_index());
  }
}

TEST_F(PinyinContextMockTest, RemoveTest) {
  PinyinContextMock context;

  // Nothing occurs
  context.RemoveCharBefore();
  EXPECT_EQ("", context.input_text());
  EXPECT_EQ("", context.auxiliary_text());
  EXPECT_EQ(0, context.cursor());
  EXPECT_EQ(0, context.candidates_size());

  // Nothing occurs
  context.RemoveCharAfter();
  EXPECT_EQ("", context.input_text());
  EXPECT_EQ("", context.auxiliary_text());
  EXPECT_EQ(0, context.cursor());
  EXPECT_EQ(0, context.candidates_size());

  InsertCharacterChars("nihao", &context);

  context.RemoveCharBefore();
  EXPECT_EQ("niha", context.input_text());
  EXPECT_EQ("auxiliary_text_niha", context.auxiliary_text());
  EXPECT_EQ(4, context.cursor());
  EXPECT_EQ(4, context.candidates_size());

  context.RemoveWordBefore();
  EXPECT_EQ("nih", context.input_text());
  EXPECT_EQ("auxiliary_text_nih", context.auxiliary_text());
  EXPECT_EQ(3, context.cursor());
  EXPECT_EQ(3, context.candidates_size());

  context.MoveCursorToBeginning();
  EXPECT_EQ("nih", context.input_text());
  EXPECT_EQ("", context.auxiliary_text());
  EXPECT_EQ(0, context.cursor());
  EXPECT_EQ(0, context.candidates_size());

  // Nothing occurs
  context.RemoveCharBefore();
  EXPECT_EQ("nih", context.input_text());
  EXPECT_EQ("", context.auxiliary_text());
  EXPECT_EQ(0, context.cursor());
  EXPECT_EQ(0, context.candidates_size());

  // Nothing occurs
  context.RemoveWordBefore();
  EXPECT_EQ("nih", context.input_text());
  EXPECT_EQ("", context.auxiliary_text());
  EXPECT_EQ(0, context.cursor());
  EXPECT_EQ(0, context.candidates_size());

  context.RemoveCharAfter();
  EXPECT_EQ("ih", context.input_text());
  EXPECT_EQ("", context.auxiliary_text());
  EXPECT_EQ(0, context.cursor());
  EXPECT_EQ(0, context.candidates_size());

  context.RemoveWordAfter();
  EXPECT_EQ("", context.input_text());
  EXPECT_EQ("", context.auxiliary_text());
  EXPECT_EQ(0, context.cursor());
  EXPECT_EQ(0, context.candidates_size());

  InsertCharacterChars("nihao", &context);
  EXPECT_EQ("nihao", context.input_text());
  EXPECT_EQ("auxiliary_text_nihao", context.auxiliary_text());
  EXPECT_EQ(5, context.cursor());
  EXPECT_EQ(5, context.candidates_size());

  context.RemoveWordBefore();
  EXPECT_EQ("nih", context.input_text());
  EXPECT_EQ("auxiliary_text_nih", context.auxiliary_text());
  EXPECT_EQ(3, context.cursor());
  EXPECT_EQ(3, context.candidates_size());

  context.RemoveWordBefore();
  EXPECT_EQ("", context.input_text());
  EXPECT_EQ("", context.auxiliary_text());
  EXPECT_EQ(0, context.cursor());
  EXPECT_EQ(0, context.candidates_size());
}

TEST_F(PinyinContextMockTest, FocusCandidateIndex) {
  PinyinContextMock context;

  {  // Insert
    context.Clear();

    InsertCharacterChars("nihao", &context);
    ASSERT_EQ(5, context.candidates_size());

    context.FocusCandidate(1);
    ASSERT_EQ(1, context.focused_candidate_index());

    context.Insert('a');
    EXPECT_EQ(0, context.focused_candidate_index());
  }

  {  // FocusCandidate*
    context.Clear();
    InsertCharacterChars("nihao", &context);

    ASSERT_EQ(5, context.candidates_size());
    context.FocusCandidate(1);
    EXPECT_EQ(1, context.focused_candidate_index());
    context.MoveCursorLeft();
    EXPECT_EQ(4, context.cursor());
    EXPECT_EQ(0, context.focused_candidate_index());

    ASSERT_EQ(4, context.candidates_size());
    context.FocusCandidate(1);
    EXPECT_EQ(1, context.focused_candidate_index());
    context.MoveCursorLeftByWord();
    EXPECT_EQ(3, context.cursor());
    EXPECT_EQ(0, context.focused_candidate_index());

    ASSERT_EQ(3, context.candidates_size());
    context.FocusCandidate(1);
    EXPECT_EQ(1, context.focused_candidate_index());
    context.MoveCursorRight();
    EXPECT_EQ(4, context.cursor());
    EXPECT_EQ(0, context.focused_candidate_index());

    ASSERT_EQ(4, context.candidates_size());
    context.FocusCandidate(1);
    EXPECT_EQ(1, context.focused_candidate_index());
    context.MoveCursorRightByWord();
    EXPECT_EQ(5, context.cursor());
    EXPECT_EQ(0, context.focused_candidate_index());
  }

  {  // Remove*
    context.Clear();
    InsertCharacterChars("abcdefgh", &context);

    // abcdefgh -> abcdefg
    ASSERT_EQ(8, context.candidates_size());
    context.FocusCandidate(1);
    EXPECT_EQ(1, context.focused_candidate_index());
    context.RemoveCharBefore();
    EXPECT_EQ("abcdefg", context.input_text());
    EXPECT_EQ("auxiliary_text_abcdefg", context.auxiliary_text());
    EXPECT_EQ(0, context.focused_candidate_index());

    // abcdefg -> abcdef
    ASSERT_EQ(7, context.candidates_size());
    context.FocusCandidate(1);
    EXPECT_EQ(1, context.focused_candidate_index());
    context.RemoveWordBefore();
    EXPECT_EQ("abcdef", context.input_text());
    EXPECT_EQ("auxiliary_text_abcdef", context.auxiliary_text());
    EXPECT_EQ(0, context.focused_candidate_index());

    context.MoveCursorLeft();
    context.MoveCursorLeft();

    // abcd|ef -> abcd|f
    // focus_candidate_index should not be changed.
    ASSERT_EQ(4, context.candidates_size());
    context.FocusCandidate(1);
    EXPECT_EQ(1, context.focused_candidate_index());
    context.RemoveCharAfter();
    EXPECT_EQ("abcdf", context.input_text());
    EXPECT_EQ("auxiliary_text_abcd", context.auxiliary_text());
    EXPECT_EQ(1, context.focused_candidate_index());

    // abcd|f -> abcd
    // focus_candidate_index should not be changed.
    ASSERT_EQ(4, context.candidates_size());
    context.FocusCandidate(1);
    EXPECT_EQ(1, context.focused_candidate_index());
    context.RemoveWordAfter();
    EXPECT_EQ("abcd", context.input_text());
    EXPECT_EQ("auxiliary_text_abcd", context.auxiliary_text());
    EXPECT_EQ(1, context.focused_candidate_index());
  }
}

TEST_F(PinyinContextMockTest, ClearTest) {
  PinyinContextMock context;

  {  // Prepare condition
    InsertCharacterChars("abcd", &context);
    context.MoveCursorLeft();
    context.SelectCandidate(2);
    context.FocusCandidate(1);
    ASSERT_EQ("abcd", context.input_text());
    EXPECT_EQ("auxiliary_text_bc", context.auxiliary_text());
    ASSERT_EQ(ToFullWidthAscii("A"), context.selected_text());
    ASSERT_EQ("b", context.conversion_text());
    ASSERT_EQ("cd", context.rest_text());
    ASSERT_EQ(1, context.focused_candidate_index());
    ASSERT_EQ(3, context.cursor());
    ASSERT_EQ(2, context.candidates_size());
  }

  {
    context.ClearCommitText();
    EXPECT_EQ("abcd", context.input_text());
    EXPECT_EQ("auxiliary_text_bc", context.auxiliary_text());
    EXPECT_EQ(ToFullWidthAscii("A"), context.selected_text());
    EXPECT_EQ("b", context.conversion_text());
    EXPECT_EQ("cd", context.rest_text());
    EXPECT_EQ(1, context.focused_candidate_index());
    EXPECT_EQ(3, context.cursor());
    EXPECT_EQ(2, context.candidates_size());
  }

  {
    context.Clear();
    ASSERT_EQ("", context.input_text());
    EXPECT_EQ("", context.auxiliary_text());
    ASSERT_EQ("", context.selected_text());
    ASSERT_EQ("", context.conversion_text());
    ASSERT_EQ("", context.rest_text());
    ASSERT_EQ(0, context.focused_candidate_index());
    ASSERT_EQ(0, context.cursor());
    ASSERT_EQ(0, context.candidates_size());
  }

  {  // Prepare condition
    InsertCharacterChars("abc", &context);
    context.CommitPreedit();
    ASSERT_EQ("", context.input_text());
    ASSERT_EQ("abc", context.commit_text());
  }

  {
    context.ClearCommitText();
    ASSERT_EQ("", context.input_text());
    ASSERT_EQ("", context.commit_text());
  }
}

TEST_F(PinyinContextMockTest, ClearCandidateFromHistory) {
  PinyinContextMock context;

  InsertCharacterChars("abc", &context);
  ASSERT_EQ(3, context.candidates_size());

  context.FocusCandidate(1);
  ASSERT_EQ(1, context.focused_candidate_index());

  context.ClearCandidateFromHistory(1);
  EXPECT_EQ(2, GetCandidatesSize(&context));

  Candidate candidate;
  EXPECT_TRUE(context.GetCandidate(0, &candidate));
  EXPECT_EQ(ToFullWidthAscii("ABC"), candidate.text);
  EXPECT_TRUE(context.GetCandidate(1, &candidate));
  EXPECT_EQ(ToFullWidthAscii("A"), candidate.text);

  EXPECT_EQ("abc", context.input_text());
  EXPECT_EQ("", context.selected_text());
  EXPECT_EQ(ToFullWidthAscii("ABC"), context.conversion_text());
  EXPECT_EQ("", context.rest_text());
  EXPECT_EQ(0, context.focused_candidate_index());
}

TEST_F(PinyinContextMockTest, ReloadConfig) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  config.mutable_pinyin_config()->set_double_pinyin(false);
  config::ConfigHandler::SetConfig(config);

  PinyinContextMock context;
  InsertCharacterChars("abc", &context);
  ASSERT_EQ("abc", context.input_text());

  config.mutable_pinyin_config()->set_double_pinyin(true);
  config::ConfigHandler::SetConfig(config);

  // If config::PinyinConfig::double_pinyin is changed, any of information of
  // context will be cleared.
  context.ReloadConfig();
  EXPECT_EQ("", context.input_text());
}

}  // namespace pinyin
}  // namespace mozc
