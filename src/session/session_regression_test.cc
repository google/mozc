// Copyright 2010-2021, Google Inc.
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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "composer/key_parser.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/engine.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/request_test_util.h"
#include "session/ime_context.h"
#include "session/session.h"
#include "session/session_handler.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

ABSL_DECLARE_FLAG(bool, use_history_rewriter);

namespace mozc {
namespace {

std::string GetComposition(const commands::Command &command) {
  if (!command.output().has_preedit()) {
    return "";
  }

  std::string preedit;
  for (size_t i = 0; i < command.output().preedit().segment_size(); ++i) {
    preedit.append(command.output().preedit().segment(i).value());
  }
  return preedit;
}

void InitSessionToPrecomposition(session::Session *session) {
#ifdef _WIN32
  // Session is created with direct mode on Windows
  // Direct status
  commands::Command command;
  session->IMEOn(&command);
#endif  // _WIN32
}

}  // namespace

class SessionRegressionTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    orig_use_history_rewriter_ = absl::GetFlag(FLAGS_use_history_rewriter);
    absl::SetFlag(&FLAGS_use_history_rewriter, true);

    // Note: engine must be created after setting all the flags, as it
    // internally depends on global flags, e.g., for creation of rewriters.
    std::unique_ptr<Engine> engine = MockDataEngineFactory::Create().value();

    // Clear previous data just in case. It should work without this clear,
    // however the reality is Windows environment has a flacky test issue.
    engine->ClearUserHistory();
    engine->ClearUserPrediction();
    engine->Wait();

    handler_ = std::make_unique<SessionHandler>(std::move(engine));
    ResetSession();
    CHECK(session_.get());
  }

  void TearDown() override {
    // just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);

    absl::SetFlag(&FLAGS_use_history_rewriter, orig_use_history_rewriter_);
  }

  bool SendKey(const std::string &key, commands::Command *command) {
    command->Clear();
    command->mutable_input()->set_type(commands::Input::SEND_KEY);
    if (!KeyParser::ParseKey(key, command->mutable_input()->mutable_key())) {
      return false;
    }
    return session_->SendKey(command);
  }

  bool SendKeyWithContext(const std::string &key,
                          const commands::Context &context,
                          commands::Command *command) {
    command->Clear();
    *command->mutable_input()->mutable_context() = context;
    command->mutable_input()->set_type(commands::Input::SEND_KEY);
    if (!KeyParser::ParseKey(key, command->mutable_input()->mutable_key())) {
      return false;
    }
    return session_->SendKey(command);
  }

  bool SendCommandWithId(commands::SessionCommand::CommandType type, int id,
                         commands::Command *command) {
    command->Clear();
    command->mutable_input()->set_type(commands::Input::SEND_COMMAND);
    command->mutable_input()->mutable_command()->set_type(type);
    command->mutable_input()->mutable_command()->set_id(id);
    return session_->SendCommand(command);
  }

  void InsertCharacterChars(const std::string &chars,
                            commands::Command *command) {
    constexpr uint32_t kNoModifiers = 0;
    for (size_t i = 0; i < chars.size(); ++i) {
      command->clear_input();
      command->clear_output();
      commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
      key_event->set_key_code(chars[i]);
      key_event->set_modifiers(kNoModifiers);
      session_->InsertCharacter(command);
    }
  }

  void ResetSession() {
    session_ = handler_->NewSession();
    commands::Request request;
    auto table = std::make_shared<composer::Table>();
    table->InitializeWithRequestAndConfig(request, config_);
    session_->SetTable(table);
  }

  const testing::MockDataManager data_manager_;
  bool orig_use_history_rewriter_;
  std::unique_ptr<SessionHandler> handler_;
  std::unique_ptr<session::Session> session_;
  config::Config config_;
};

TEST_F(SessionRegressionTest, ConvertToTransliterationWithMultipleSegments) {
  InitSessionToPrecomposition(session_.get());

  commands::Command command;
  InsertCharacterChars("liie", &command);

  // Convert
  command.Clear();
  session_->Convert(&command);
  {  // Check the conversion #1
    const commands::Output &output = command.output();
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidate_window());

    const commands::Preedit &conversion = output.preedit();
    ASSERT_LE(2, conversion.segment_size());
    EXPECT_EQ(conversion.segment(0).value(), "ぃ");
  }

  // TranslateHalfASCII
  command.Clear();
  session_->TranslateHalfASCII(&command);
  {  // Check the conversion #2
    const commands::Output &output = command.output();
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidate_window());

    const commands::Preedit &conversion = output.preedit();
    ASSERT_EQ(conversion.segment_size(), 2);
    EXPECT_EQ(conversion.segment(0).value(), "li");
  }
}

TEST_F(SessionRegressionTest,
       ExitTemporaryAlphanumModeAfterCommittingSugesstion) {
  // This is a regression test against http://b/2977131.
  {
    InitSessionToPrecomposition(session_.get());
    commands::Command command;
    InsertCharacterChars("NFL", &command);
    EXPECT_EQ(command.output().status().mode(), commands::HALF_ASCII);
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);  // obsolete

    EXPECT_TRUE(SendKey("F10", &command));
    ASSERT_FALSE(command.output().has_candidate_window());
    EXPECT_FALSE(command.output().has_result());

    EXPECT_TRUE(SendKey("a", &command));
#if __APPLE__
    // The MacOS default short cut of F10 is DisplayAsHalfAlphanumeric.
    // It does not start the conversion so output does not have any result.
    EXPECT_FALSE(command.output().has_result());
#else   // __APPLE__
    EXPECT_TRUE(command.output().has_result());
#endif  // __APPLE__
    EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);  // obsolete
  }
}

TEST_F(SessionRegressionTest, HistoryLearning) {
  InitSessionToPrecomposition(session_.get());
  commands::Command command;
  std::string candidate1;
  std::string candidate2;

  {  // First session.  Second candidate is committed.
    InsertCharacterChars("kanji", &command);

    command.Clear();
    session_->Convert(&command);
    candidate1 = GetComposition(command);

    command.Clear();
    session_->ConvertNext(&command);
    candidate2 = GetComposition(command);
    EXPECT_NE(candidate2, candidate1);

    command.Clear();
    session_->Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_EQ(command.output().result().value(), candidate2);
  }

  {  // Second session.  The previous second candidate should be promoted.
    command.Clear();
    InsertCharacterChars("kanji", &command);

    command.Clear();
    session_->Convert(&command);
    EXPECT_NE(GetComposition(command), candidate1);
    EXPECT_EQ(GetComposition(command), candidate2);
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
  const std::string candidate1 = GetComposition(command);

  command.Clear();
  session_->ConvertNext(&command);
  const std::string candidate2 = GetComposition(command);
  EXPECT_NE(candidate2, candidate1);

  command.Clear();
  session_->Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_EQ(command.output().result().value(), candidate2);

  command.Clear();
  session_->Undo(&command);
  EXPECT_NE(GetComposition(command), candidate1);
  EXPECT_EQ(GetComposition(command), candidate2);
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
  const std::string kYoroshikuString = "よろしく";

  command.Clear();
  session_->PredictAndConvert(&command);
  EXPECT_EQ(command.output().preedit().segment_size(), 1);

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
  EXPECT_EQ(GetComposition(command), kYoroshikuString);
  EXPECT_GE(yoroshiku_id, 0);

  command.Clear();
  session_->Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_EQ(command.output().result().value(), kYoroshikuString);

  command.Clear();
  session_->Undo(&command);
  EXPECT_EQ(GetComposition(command), kYoroshikuString);
}

// This test is to check the consistency between the result of prediction and
// suggestion.
// Following 4 values are expected to be the same.
// - The 1st candidate of prediction.
// - The result of CommitFirstSuggestion for prediction candidate.
// - The 1st candidate of suggestion.
// - The result of CommitFirstSuggestion for suggestion candidate.
//
// BACKGROUND:
// Previously there was a restriction on the result of prediction
// and suggestion.
// Currently the restriction is removed. This test checks that the logic
// works well or not.
TEST_F(SessionRegressionTest, ConsistencyBetweenPredictionAndSuggestion) {
  constexpr char kKey[] = "aio";

  commands::Request request;
  request_test_util::FillMobileRequest(&request);
  session_->SetRequest(request);

  InitSessionToPrecomposition(session_.get());
  commands::Command command;

  command.Clear();
  InsertCharacterChars(kKey, &command);
  EXPECT_EQ(command.output().preedit().segment_size(), 1);
  const std::string suggestion_first_candidate =
      command.output().all_candidate_words().candidates(0).value();

  command.Clear();
  session_->CommitFirstSuggestion(&command);
  const std::string suggestion_commit_result =
      command.output().result().value();

  InitSessionToPrecomposition(session_.get());
  command.Clear();
  InsertCharacterChars(kKey, &command);
  command.Clear();
  session_->PredictAndConvert(&command);
  const std::string prediction_first_candidate =
      command.output().all_candidate_words().candidates(0).value();

  command.Clear();
  session_->Commit(&command);
  const std::string prediction_commit_result =
      command.output().result().value();

  EXPECT_EQ(suggestion_commit_result, suggestion_first_candidate);
  EXPECT_EQ(prediction_first_candidate, suggestion_first_candidate);
  EXPECT_EQ(prediction_commit_result, suggestion_first_candidate);
}

TEST_F(SessionRegressionTest, AutoConversionTest) {
  // Default mode
  {
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());

    constexpr char kInputKeys[] = "123456.7";
    for (size_t i = 0; kInputKeys[i]; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code(kInputKeys[i]);
      key_event->set_key_string(std::string(1, kInputKeys[i]));
      session_->InsertCharacter(&command);
    }

    EXPECT_EQ(session_->context().state(), session::ImeContext::COMPOSITION);
  }

  // Auto conversion with KUTEN
  {
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config.set_use_auto_conversion(true);
    session_->SetConfig(config);

    constexpr char kInputKeys[] = "aiueo.";
    for (size_t i = 0; i < kInputKeys[i]; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code(kInputKeys[i]);
      key_event->set_key_string(std::string(1, kInputKeys[i]));
      session_->InsertCharacter(&command);
    }

    EXPECT_EQ(session_->context().state(), session::ImeContext::CONVERSION);
  }

  // Auto conversion with KUTEN, but do not convert in numerical input
  {
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());
    config::Config config = config::ConfigHandler::GetCopiedConfig();
    config.set_use_auto_conversion(true);
    session_->SetConfig(config);

    constexpr char kInputKeys[] = "1234.";
    for (size_t i = 0; i < kInputKeys[i]; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code(kInputKeys[i]);
      key_event->set_key_string(std::string(1, kInputKeys[i]));
      session_->InsertCharacter(&command);
    }

    EXPECT_EQ(session_->context().state(), session::ImeContext::COMPOSITION);
  }
}

TEST_F(SessionRegressionTest, TransliterationIssue2330463) {
  {
    ResetSession();
    commands::Command command;

    InsertCharacterChars("[],.", &command);
    command.Clear();
    SendKey("F8", &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "｢｣､｡");
  }

  {
    ResetSession();
    commands::Command command;

    InsertCharacterChars("[g],.", &command);
    command.Clear();
    SendKey("F8", &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "｢g｣､｡");
  }

  {
    ResetSession();
    commands::Command command;

    InsertCharacterChars("[a],.", &command);
    command.Clear();
    SendKey("F8", &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "｢ｱ｣､｡");
  }
}

TEST_F(SessionRegressionTest, TransliterationIssue6209563) {
  {  // Romaji mode
    ResetSession();
    commands::Command command;

    InsertCharacterChars("tt", &command);
    command.Clear();
    SendKey("F10", &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "tt");
  }

  {  // Kana mode
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());
    config::Config config = config::ConfigHandler::GetCopiedConfig();
    config.set_preedit_method(config::Config::KANA);

    // Inserts "ち" 5 times
    for (int i = 0; i < 5; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code('a');
      key_event->set_key_string("ち");
      session_->InsertCharacter(&command);
    }

    command.Clear();
    SendKey("F10", &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "aaaaa");
  }
}

TEST_F(SessionRegressionTest, CommitT13nSuggestion) {
  // This is the test for http://b/6934881.
  // Pending char chunk remains after committing transliteration.
  commands::Request request;
  request_test_util::FillMobileRequest(&request);
  session_->SetRequest(request);

  InitSessionToPrecomposition(session_.get());

  commands::Command command;
  InsertCharacterChars("ssh", &command);
  EXPECT_EQ(GetComposition(command), "っｓｈ");

  constexpr int kHiraganaId = -1;
  SendCommandWithId(commands::SessionCommand::SUBMIT_CANDIDATE, kHiraganaId,
                    &command);

  EXPECT_TRUE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());

  EXPECT_EQ(command.output().result().value(), "っｓｈ");
}

TEST_F(SessionRegressionTest, DeleteCandidateFromHistory) {
  commands::Request request;
  request_test_util::FillMobileRequest(&request);
  session_->SetRequest(request);

  InitSessionToPrecomposition(session_.get());

  commands::Command command;
  int target_id = 1;  // ID of deletation tareget

  // 1. Type "aiu" and check 2nd candidate, which is our deleteation target.
  InsertCharacterChars("aiu", &command);
  const std::string target_word =
      command.output().candidate_window().candidate(1).value();

  // 2. Submit the 2nd candidate to be deleted from history.
  SendCommandWithId(commands::SessionCommand::SUBMIT_CANDIDATE, target_id,
                    &command);
  target_id = 0;  // ID of the deletion target is changed after submit.

  InsertCharacterChars("aiu", &command);
  {
    auto candidate = command.output().candidate_window().candidate(0);
    EXPECT_EQ(candidate.id(), target_id);
    EXPECT_EQ(candidate.value(), target_word);
  }
  {
    auto candidate = command.output().candidate_window().candidate(1);
    EXPECT_NE(candidate.id(), target_id);
    EXPECT_NE(candidate.value(), target_word);
  }

  // 3. Delete the above candidate from the history.
  SendCommandWithId(commands::SessionCommand::DELETE_CANDIDATE_FROM_HISTORY,
                    target_id, &command);
  target_id =
      1;  // ID of the deletion target is reverted after history deletion.

  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_GT(command.output().candidate_window().candidate_size(), 0);
  {
    auto candidate = command.output().candidate_window().candidate(0);
    EXPECT_NE(candidate.id(), target_id);
    EXPECT_NE(candidate.value(), target_word);
  }
  {
    auto candidate = command.output().candidate_window().candidate(1);
    EXPECT_EQ(candidate.id(), target_id);
    EXPECT_EQ(candidate.value(), target_word);
  }
}

}  // namespace mozc
