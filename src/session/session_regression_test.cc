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

// This is a test with the actual converter.  So the result of the
// conversion may differ from previous versions.

#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "rewriter/rewriter_interface.h"
#include "session/commands.pb.h"
#include "session/internal/ime_context.h"
#include "session/internal/keymap.h"
#include "session/japanese_session_factory.h"
#include "session/key_parser.h"
#include "session/candidates.pb.h"
#include "session/session.h"
#include "session/session_factory_manager.h"
#include "session/session_converter_interface.h"
#include "session/session_handler.h"
#include "session/request_test_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

#ifdef OS_ANDROID
#include "base/mmap.h"
#include "base/singleton.h"
#include "data_manager/android/android_data_manager.h"
#endif

DECLARE_string(test_srcdir);
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
#ifdef OS_WIN
  // Session is created with direct mode on Windows
  // Direct status
  commands::Command command;
  session->IMEOn(&command);
#endif  // OS_WIN
}

#ifdef OS_ANDROID
// In actual libmozc.so usage, the dictionary data will be given via JNI call
// because only Java side code knows where the data is.
// On native code unittest, we cannot do it, so instead we mmap the files
// and use it.
// Note that this technique works here because the no other test code doesn't
// link to this binary.
// TODO(hidehiko): Get rid of this hack by refactoring Engine/DataManager
// related code.
class AndroidInitializer {
 private:
  AndroidInitializer() {
    string dictionary_data_path = FileUtil::JoinPath(
        FLAGS_test_srcdir, "embedded_data/dictionary_data");
    CHECK(dictionary_mmap_.Open(dictionary_data_path.c_str(), "r"));
    mozc::android::AndroidDataManager::SetDictionaryData(
        dictionary_mmap_.begin(), dictionary_mmap_.size());

    string connection_data_path = FileUtil::JoinPath(
        FLAGS_test_srcdir, "embedded_data/connection_data");
    CHECK(connection_mmap_.Open(connection_data_path.c_str(), "r"));
    mozc::android::AndroidDataManager::SetConnectionData(
        connection_mmap_.begin(), connection_mmap_.size());
    LOG(ERROR) << "mmap data initialized.";
  }

  friend class Singleton<AndroidInitializer>;

  Mmap dictionary_mmap_;
  Mmap connection_mmap_;

  DISALLOW_COPY_AND_ASSIGN(AndroidInitializer);
};
#endif  // OS_ANDROID

}  // anonymous namespace

class SessionRegressionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);

    orig_use_history_rewriter_ = FLAGS_use_history_rewriter;
    FLAGS_use_history_rewriter = true;

#ifdef OS_ANDROID
    Singleton<AndroidInitializer>::get();
#endif

    // Note: engine must be created after setting all the flags, as it
    // internally depends on global flags, e.g., for creation of rewriters.
    engine_.reset(EngineFactory::Create());

    session_factory_.reset(new session::JapaneseSessionFactory(engine_.get()));
    session::SessionFactoryManager::SetSessionFactory(session_factory_.get());

    config::ConfigHandler::GetDefaultConfig(&config_);
    // TOOD(all): Add a test for the case where
    // use_realtime_conversion is true.
    config_.set_use_realtime_conversion(false);
    config::ConfigHandler::SetConfig(config_);
    handler_.reset(new SessionHandler());
    ResetSession();
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
    session_.reset(static_cast<session::Session *>(handler_->NewSession()));
    commands::Request request;
    table_.reset(new composer::Table());
    table_->InitializeWithRequestAndConfig(request, config_);
    session_->SetTable(table_.get());
  }

  bool orig_use_history_rewriter_;
  scoped_ptr<EngineInterface> engine_;
  scoped_ptr<SessionHandler> handler_;
  scoped_ptr<session::Session> session_;
  scoped_ptr<composer::Table> table_;
  scoped_ptr<session::JapaneseSessionFactory> session_factory_;
  config::Config config_;
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

  command.Clear();
  session_->Undo(&command);
  EXPECT_EQ(kYoroshikuString, GetComposition(command));
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
TEST_F(SessionRegressionTest, ConsistencyBetweenPredictionAndSuggesion) {
  const char kKey[] = "aio";

  commands::Request request;
  commands::RequestForUnitTest::FillMobileRequest(&request);
  session_->SetRequest(&request);

  InitSessionToPrecomposition(session_.get());
  commands::Command command;

  command.Clear();
  InsertCharacterChars(kKey, &command);
  EXPECT_EQ(1, command.output().preedit().segment_size());
  const string suggestion_first_candidate =
      command.output().all_candidate_words().candidates(0).value();

  command.Clear();
  session_->CommitFirstSuggestion(&command);
  const string suggestion_commit_result = command.output().result().value();

  InitSessionToPrecomposition(session_.get());
  command.Clear();
  InsertCharacterChars(kKey, &command);
  command.Clear();
  session_->PredictAndConvert(&command);
  const string prediction_first_candidate =
      command.output().all_candidate_words().candidates(0).value();

  command.Clear();
  session_->Commit(&command);
  const string prediction_commit_result = command.output().result().value();

  EXPECT_EQ(suggestion_first_candidate, suggestion_commit_result);
  EXPECT_EQ(suggestion_first_candidate, prediction_first_candidate);
  EXPECT_EQ(suggestion_first_candidate, prediction_commit_result);
}

TEST_F(SessionRegressionTest, AutoConversionTest) {
  // Default mode
  {
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());

    const char kInputKeys[] = "123456.7";
    for (size_t i = 0; kInputKeys[i]; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code(kInputKeys[i]);
      key_event->set_key_string(string(1, kInputKeys[i]));
      session_->InsertCharacter(&command);
    }

    EXPECT_EQ(session::ImeContext::COMPOSITION, session_->context().state());
  }

  // Auto conversion with KUTEN
  {
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_use_auto_conversion(true);
    config::ConfigHandler::SetConfig(config);
    session_->ReloadConfig();

    const char kInputKeys[] = "aiueo.";
    for (size_t i = 0; i < kInputKeys[i]; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code(kInputKeys[i]);
      key_event->set_key_string(string(1, kInputKeys[i]));
      session_->InsertCharacter(&command);
    }

    EXPECT_EQ(session::ImeContext::CONVERSION, session_->context().state());
  }

  // Auto conversion with KUTEN, but do not convert in numerical input
  {
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_use_auto_conversion(true);
    config::ConfigHandler::SetConfig(config);
    session_->ReloadConfig();

    const char kInputKeys[] = "1234.";
    for (size_t i = 0; i < kInputKeys[i]; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code(kInputKeys[i]);
      key_event->set_key_string(string(1, kInputKeys[i]));
      session_->InsertCharacter(&command);
    }

    EXPECT_EQ(session::ImeContext::COMPOSITION, session_->context().state());
  }
}

TEST_F(SessionRegressionTest, Transliteration_Issue2330463) {
  {
    ResetSession();
    commands::Command command;

    InsertCharacterChars("[],.", &command);
    command.Clear();
    SendKey("F8", &command);
    // "｢｣､｡"
    EXPECT_EQ("\357\275\242\357\275\243\357\275\244\357\275\241",
              command.output().preedit().segment(0).value());
  }

  {
    ResetSession();
    commands::Command command;

    InsertCharacterChars("[g],.", &command);
    command.Clear();
    SendKey("F8", &command);
    // "｢g｣､｡"
    EXPECT_EQ("\357\275\242\147\357\275\243\357\275\244\357\275\241",
              command.output().preedit().segment(0).value());
  }

  {
    ResetSession();
    commands::Command command;

    InsertCharacterChars("[a],.", &command);
    command.Clear();
    SendKey("F8", &command);
    // "｢ｱ｣､｡"
    EXPECT_EQ("\357\275\242\357\275\261\357\275\243\357\275\244\357\275\241",
              command.output().preedit().segment(0).value());
  }
}

TEST_F(SessionRegressionTest, Transliteration_Issue6209563) {
  {  // Romaji mode
    ResetSession();
    commands::Command command;

    InsertCharacterChars("tt", &command);
    command.Clear();
    SendKey("F10", &command);
    EXPECT_EQ("tt", command.output().preedit().segment(0).value());
  }

  {  // Kana mode
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_preedit_method(config::Config::KANA);

    // Inserts "ち" 5 times
    for (int i = 0; i < 5; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code('a');
      key_event->set_key_string("\xE3\x81\xA1");  // "ち"
      session_->InsertCharacter(&command);
    }

    command.Clear();
    SendKey("F10", &command);
    EXPECT_EQ("aaaaa", command.output().preedit().segment(0).value());
  }
}

TEST_F(SessionRegressionTest, CommitT13nSuggestion) {
  // This is the test for http://b/6934881.
  // Pending char chunk remains after committing transliteration.
  commands::Request request;
  commands::RequestForUnitTest::FillMobileRequest(&request);
  session_->SetRequest(&request);

  InitSessionToPrecomposition(session_.get());

  commands::Command command;
  InsertCharacterChars("ssh", &command);
  // "っｓｈ"
  EXPECT_EQ("\xE3\x81\xA3\xEF\xBD\x93\xEF\xBD\x88", GetComposition(command));

  command.Clear();
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::SUBMIT_CANDIDATE);
  const int kHiraganaId = -1;
  command.mutable_input()->mutable_command()->set_id(kHiraganaId);
  session_->SendCommand(&command);

  EXPECT_TRUE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());

  // "っｓｈ"
  EXPECT_EQ("\xE3\x81\xA3\xEF\xBD\x93\xEF\xBD\x88",
            command.output().result().value());
}
}  // namespace mozc
