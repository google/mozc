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

// This is a test with the actual converter.  So the result of the
// conversion may differ from previous versions.

#include <string>
#include "base/base.h"
#include "base/util.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "rewriter/rewriter_interface.h"
#include "session/internal/keymap.h"
#include "session/japanese_session_factory.h"
#include "session/key_parser.h"
#include "session/session.h"
#include "session/session_converter_interface.h"
#include "session/session_handler.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

DECLARE_string(test_tmpdir);
DECLARE_bool(use_history_rewriter);

namespace mozc {

namespace {
string GetComposition(const commands::Command &command) {
  if (!command.output().has_preedit()) {
    return "";
  }

  string preedit;
  for (size_t i = 0; i < command.output().preedit().segment_size(); ++i) {
    preedit.append(command.output().preedit().segment(i).value());
  }
  return preedit;
}

void InitSessionToPrecomposition(session::Session* session) {
#ifdef OS_WINDOWS
  // Session is created with direct mode on Windows
  // Direct status
  commands::Command command;
  session->IMEOn(&command);
#endif  // OS_WINDOWS
}
}  // anonymous namespace

class SessionRegressionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    session::SessionFactoryManager::SetSessionFactory(&session_factory_);

    orig_use_history_rewriter_ = FLAGS_use_history_rewriter;
    FLAGS_use_history_rewriter = true;
    RewriterFactory::SetRewriter(NULL);

    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    // TOOD(all): Add a test for the case where
    // use_realtime_conversion is true.
    config.set_use_realtime_conversion(false);
    config::ConfigHandler::SetConfig(config);
    handler_.reset(new SessionHandler());
    session_.reset(dynamic_cast<session::Session *>(handler_->NewSession()));
    CHECK(session_.get());
  }

  virtual void TearDown() {
    // just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);

    FLAGS_use_history_rewriter = orig_use_history_rewriter_;
  }

  bool SendKey(const string &key, commands::Command *command) {
    command->Clear();
    command->mutable_input()->set_type(commands::Input::SEND_KEY);
    if (!KeyParser::ParseKey(key, command->mutable_input()->mutable_key())) {
      return false;
    }
    return session_->SendKey(command);
  }

  void InsertCharacterChars(const string &chars,
                            commands::Command *command) {
    const uint32 kNoModifiers = 0;
    for (int i = 0; i < chars.size(); ++i) {
      command->clear_input();
      command->clear_output();
      commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
      key_event->set_key_code(chars[i]);
      key_event->set_modifiers(kNoModifiers);
      session_->InsertCharacter(command);
    }
  }

  bool orig_use_history_rewriter_;
  scoped_ptr<SessionHandler> handler_;
  scoped_ptr<session::Session> session_;
  session::JapaneseSessionFactory session_factory_;
};


TEST_F(SessionRegressionTest, ConvertToTransliterationWithMultipleSegments) {
  InitSessionToPrecomposition(session_.get());

  commands::Command command;
  InsertCharacterChars("like", &command);

  // Convert
  command.Clear();
  session_->Convert(&command);
  {  // Check the conversion #1
    const commands::Output &output = command.output();
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(2, conversion.segment_size());
    // "ぃ"
    EXPECT_EQ("\xE3\x81\x83", conversion.segment(0).value());
    // "家"
    EXPECT_EQ("\xE5\xAE\xB6", conversion.segment(1).value());
  }

  // TranslateHalfASCII
  command.Clear();
  session_->TranslateHalfASCII(&command);
  {  // Check the conversion #2
    const commands::Output &output = command.output();
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(2, conversion.segment_size());
    EXPECT_EQ("li", conversion.segment(0).value());
  }
}

TEST_F(SessionRegressionTest,
       ExitTemporaryAlphanumModeAfterCommitingSugesstion) {
  // This is a regression test against http://b/2977131.
  {
    InitSessionToPrecomposition(session_.get());
    commands::Command command;
    InsertCharacterChars("NFL", &command);
    EXPECT_EQ(commands::HALF_ASCII, command.output().status().mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());  // obsolete

    EXPECT_TRUE(SendKey("F10", &command));
    ASSERT_FALSE(command.output().has_candidates());
    EXPECT_FALSE(command.output().has_result());

    EXPECT_TRUE(SendKey("a", &command));
    EXPECT_FALSE(command.output().has_candidates());
#if OS_MACOSX
    // The MacOS default short cut of F10 is DisplayAsHalfAlphanumeric.
    // It does not start the conversion so output does not have any result.
    EXPECT_FALSE(command.output().has_result());
#else
    EXPECT_TRUE(command.output().has_result());
#endif
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());  // obsolete
  }
}

TEST_F(SessionRegressionTest, HistoryLearning) {
  InitSessionToPrecomposition(session_.get());
  commands::Command command;
  string candidate1;
  string candidate2;

  {  // First session.  Second candidate is commited.
    InsertCharacterChars("kanji", &command);

    command.Clear();
    session_->Convert(&command);
    candidate1 = GetComposition(command);

    command.Clear();
    session_->ConvertNext(&command);
    candidate2 = GetComposition(command);
    EXPECT_NE(candidate1, candidate2);

    command.Clear();
    session_->Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_EQ(candidate2, command.output().result().value());
  }
  {  // Second session.  The previous second candidate should be promoted.
    command.Clear();
    InsertCharacterChars("kanji", &command);

    command.Clear();
    session_->Convert(&command);
    EXPECT_NE(candidate1, GetComposition(command));
    EXPECT_EQ(candidate2, GetComposition(command));
  }
}

TEST_F(SessionRegressionTest, Undo) {
  InitSessionToPrecomposition(session_.get());

  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session_->set_client_capability(capability);

  commands::Command command;
  InsertCharacterChars("kanji", &command);

  command.Clear();
  session_->Convert(&command);
  const string candidate1 = GetComposition(command);

  command.Clear();
  session_->ConvertNext(&command);
  const string candidate2 = GetComposition(command);
  EXPECT_NE(candidate1, candidate2);

  command.Clear();
  session_->Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_EQ(candidate2, command.output().result().value());

  command.Clear();
  session_->Undo(&command);
  EXPECT_NE(candidate1, GetComposition(command));
  EXPECT_EQ(candidate2, GetComposition(command));
}

// TODO(hsumita): This test may be moved to session_test.cc.
// New converter mock is required if move this test.
TEST_F(SessionRegressionTest, ConverterHandleHistorySegmentAfterUndo) {
  // This is an unittest against http://b/3427618
  InitSessionToPrecomposition(session_.get());

  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session_->set_client_capability(capability);

  commands::Command command;
  InsertCharacterChars("kiki", &command);
  // "危機"
  const char *kKikiString = "\xE5\x8D\xB1\xE6\xA9\x9F";

  command.Clear();
  session_->Convert(&command);
  EXPECT_EQ(1, command.output().preedit().segment_size());

  // Candidate contain "危機" or NOT
  bool is_convert_success = false;
  for (size_t i = 0; i < 10; ++i) {
    if (GetComposition(command) == kKikiString) {
      is_convert_success = true;
      break;
    }

    command.Clear();
    session_->ConvertNext(&command);
  }
  EXPECT_TRUE(is_convert_success);
  EXPECT_EQ(kKikiString, GetComposition(command));

  command.Clear();
  session_->Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_EQ(kKikiString, command.output().result().value());

  Segments segments;
  session_->context().converter().GetSegments(&segments);
  EXPECT_EQ(1, segments.history_segments_size());
  EXPECT_EQ(0, segments.conversion_segments_size());
  EXPECT_EQ(kKikiString, segments.segment(0).candidate(0).value);

  command.Clear();
  InsertCharacterChars("ippatsu", &command);
  // "一髪"
  const string kIppatsuString = "\xE4\xB8\x80\xE9\xAB\xAA";

  command.Clear();
  session_->Convert(&command);
  const string candidate = GetComposition(command);
  // "一髪"
  EXPECT_EQ(kIppatsuString, candidate);

  command.Clear();
  session_->Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_EQ(kIppatsuString, command.output().result().value());

  command.Clear();
  session_->Undo(&command);

  session_->context().converter().GetSegments(&segments);
  EXPECT_EQ(1, segments.history_segments_size());
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(kKikiString, segments.segment(0).candidate(0).value);
  // Confirm the result contains suggestion which consider history segments.
  EXPECT_EQ(candidate, GetComposition(command));
}

// TODO(hsumita): This test may be moved to session_test.cc.
// New converter mock is required if move this test.
TEST_F(SessionRegressionTest, PredictionAfterUndo) {
  // This is a unittest against http://b/3427619
  InitSessionToPrecomposition(session_.get());

  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session_->set_client_capability(capability);

  commands::Command command;
  InsertCharacterChars("yoroshi", &command);
  // "よろしく"
  const string kYoroshikuString =
      "\xe3\x82\x88\xe3\x82\x8d\xe3\x81\x97\xe3\x81\x8f";

  command.Clear();
  session_->PredictAndConvert(&command);
  EXPECT_EQ(1, command.output().preedit().segment_size());

  // Candidate contain "よろしく" or NOT
  int yoroshiku_id = -1;
  for (size_t i = 0; i < 10; ++i) {
    if (GetComposition(command) == kYoroshikuString) {
      yoroshiku_id = i;
      break;
    }

    command.Clear();
    session_->ConvertNext(&command);
  }
  EXPECT_EQ(kYoroshikuString, GetComposition(command));
  EXPECT_GE(yoroshiku_id, 0);

  command.Clear();
  session_->Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_EQ(kYoroshikuString, command.output().result().value());

  Segments segments;
  session_->context().converter().GetSegments(&segments);
  EXPECT_EQ(1, segments.history_segments_size());
  EXPECT_EQ(0, segments.conversion_segments_size());
  EXPECT_EQ(kYoroshikuString, segments.history_segment(0).candidate(0).value);

  command.Clear();
  session_->Undo(&command);

  session_->context().converter().GetSegments(&segments);
  EXPECT_EQ(0, segments.history_segments_size());
  EXPECT_EQ(1, segments.conversion_segments_size());
  // Confirm the result contains suggestion candidates from "よろし"
  EXPECT_EQ(kYoroshikuString,
            segments.conversion_segment(0).candidate(yoroshiku_id).value);
  EXPECT_EQ(kYoroshikuString, GetComposition(command));
}

}  // namespace mozc
