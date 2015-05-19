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
#include "languages/pinyin/direct_context.h"
#include "languages/pinyin/session_config.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace pinyin {
namespace direct {

namespace {
testing::AssertionResult CheckContext(const char *expected_commit_text_expr,
                                      const char *actual_context_expr,
                                      const string &expected_commit_text,
                                      DirectContext *actual_context) {
  vector<string> error_messages;

  if (!actual_context->input_text().empty()) {
    error_messages.push_back("input_text is not empty.");
  }
  if (!actual_context->selected_text().empty()) {
    error_messages.push_back("selected_text is not empty.");
  }
  if (!actual_context->conversion_text().empty()) {
    error_messages.push_back("conversion_text is not empty.");
  }
  if (!actual_context->rest_text().empty()) {
    error_messages.push_back("rest_text is not empty.");
  }
  if (!actual_context->auxiliary_text().empty()) {
    error_messages.push_back("auxiliary_text is not empty.");
  }
  if (actual_context->cursor() != 0) {
    error_messages.push_back(Util::StringPrintf("invalid value. cursor: %d",
                                                actual_context->cursor()));
  }
  if (actual_context->focused_candidate_index() != 0) {
    error_messages.push_back(Util::StringPrintf(
        "invalid value. focused_candidate_index: %d",
        actual_context->focused_candidate_index()));
  }
  if (actual_context->HasCandidate(0)) {
    error_messages.push_back("invalid value. there are some candidates.");
  }

  if (expected_commit_text != actual_context->commit_text()) {
    error_messages.push_back(Util::StringPrintf(
        "commit_text is not valid.\n"
        "Expected: %s\n"
        "Actual: %s",
        expected_commit_text.c_str(), actual_context->commit_text().c_str()));
  }

  if (!error_messages.empty()) {
    string error_message;
    Util::JoinStrings(error_messages, "\n", &error_message);
    return testing::AssertionFailure() << error_message;
  }

  return testing::AssertionSuccess();
}

#define EXPECT_VALID_CONTEXT(expected_commit_text) \
  EXPECT_PRED_FORMAT2(CheckContext, expected_commit_text, context_.get())
}  // namespace

class DirectContextTest : public testing::Test {
 protected:
  virtual void SetUp() {
    session_config_.reset(new SessionConfig);
    session_config_->full_width_word_mode = false;
    session_config_->full_width_punctuation_mode = true;
    session_config_->simplified_chinese_mode = true;
    context_.reset(new DirectContext(*session_config_));
  }

  virtual void TearDown() {
  }

  scoped_ptr<SessionConfig> session_config_;
  scoped_ptr<DirectContext> context_;
};

TEST_F(DirectContextTest, Insert) {
  EXPECT_TRUE(context_->commit_text().empty());

  context_->Insert('a');
  EXPECT_VALID_CONTEXT("a");

  context_->Insert('b');
  EXPECT_VALID_CONTEXT("b");

  context_->ClearCommitText();
  EXPECT_VALID_CONTEXT("");

  context_->Insert('a');
  EXPECT_VALID_CONTEXT("a");

  context_->Clear();
  EXPECT_VALID_CONTEXT("");
}

TEST_F(DirectContextTest, HalfOrFullWidthInsert) {
  session_config_->full_width_word_mode = true;
  context_->Insert('a');
  // "ａ"
  EXPECT_VALID_CONTEXT("\xEF\xBD\x81");

  session_config_->full_width_word_mode = false;

  context_->Insert('a');
  EXPECT_VALID_CONTEXT("a");
}

}  // namespace direct
}  // namespace pinyin
}  // namespace mozc
