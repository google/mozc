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

#include "session/session.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "base/strings/assign.h"
#include "base/strings/unicode.h"
#include "base/vlog.h"
#include "composer/composer.h"
#include "composer/key_parser.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "engine/engine.h"
#include "engine/engine_converter.h"
#include "engine/engine_mock.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "request/request_test_util.h"
#include "rewriter/transliteration_rewriter.h"
#include "session/ime_context.h"
#include "session/keymap.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "transliteration/transliteration.h"

namespace mozc {

namespace session {
namespace {

using ::mozc::commands::Request;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SetArgPointee;

void SetSendKeyCommandWithKeyString(const absl::string_view key_string,
                                    commands::Command *command) {
  command->Clear();
  command->mutable_input()->set_type(commands::Input::SEND_KEY);
  commands::KeyEvent *key = command->mutable_input()->mutable_key();
  key->set_key_string(key_string);
}

bool SetSendKeyCommand(const absl::string_view key,
                       commands::Command *command) {
  command->Clear();
  command->mutable_input()->set_type(commands::Input::SEND_KEY);
  return KeyParser::ParseKey(key, command->mutable_input()->mutable_key());
}

bool SendKey(const absl::string_view key, Session *session,
             commands::Command *command) {
  if (!SetSendKeyCommand(key, command)) {
    return false;
  }
  return session->SendKey(command);
}

bool SendKeyWithMode(const absl::string_view key,
                     commands::CompositionMode mode, Session *session,
                     commands::Command *command) {
  if (!SetSendKeyCommand(key, command)) {
    return false;
  }
  command->mutable_input()->mutable_key()->set_mode(mode);
  return session->SendKey(command);
}

bool SendKeyWithModeAndActivated(const absl::string_view key, bool activated,
                                 commands::CompositionMode mode,
                                 Session *session, commands::Command *command) {
  if (!SetSendKeyCommand(key, command)) {
    return false;
  }
  command->mutable_input()->mutable_key()->set_activated(activated);
  command->mutable_input()->mutable_key()->set_mode(mode);
  return session->SendKey(command);
}

bool TestSendKey(const absl::string_view key, Session *session,
                 commands::Command *command) {
  if (!SetSendKeyCommand(key, command)) {
    return false;
  }
  return session->TestSendKey(command);
}

bool TestSendKeyWithMode(const absl::string_view key,
                         commands::CompositionMode mode, Session *session,
                         commands::Command *command) {
  if (!SetSendKeyCommand(key, command)) {
    return false;
  }
  command->mutable_input()->mutable_key()->set_mode(mode);
  return session->TestSendKey(command);
}

bool TestSendKeyWithModeAndActivated(const absl::string_view key,
                                     bool activated,
                                     commands::CompositionMode mode,
                                     Session *session,
                                     commands::Command *command) {
  if (!SetSendKeyCommand(key, command)) {
    return false;
  }
  command->mutable_input()->mutable_key()->set_activated(activated);
  command->mutable_input()->mutable_key()->set_mode(mode);
  return session->TestSendKey(command);
}

bool SendSpecialKey(commands::KeyEvent::SpecialKey special_key,
                    Session *session, commands::Command *command) {
  command->Clear();
  command->mutable_input()->set_type(commands::Input::SEND_KEY);
  command->mutable_input()->mutable_key()->set_special_key(special_key);
  return session->SendKey(command);
}

void SetSendCommandCommand(commands::SessionCommand::CommandType type,
                           commands::Command *command) {
  command->Clear();
  command->mutable_input()->set_type(commands::Input::SEND_COMMAND);
  command->mutable_input()->mutable_command()->set_type(type);
}

bool SendCommand(commands::SessionCommand::CommandType type, Session *session,
                 commands::Command *command) {
  SetSendCommandCommand(type, command);
  return session->SendCommand(command);
}

bool InsertCharacterCodeAndString(const char key_code,
                                  const absl::string_view key_string,
                                  Session *session,
                                  commands::Command *command) {
  command->Clear();
  commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
  key_event->set_key_code(key_code);
  key_event->set_key_string(key_string);
  return session->InsertCharacter(command);
}

Segment::Candidate *AddCandidate(const absl::string_view key,
                                 const absl::string_view value,
                                 Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  strings::Assign(candidate->key, key);
  strings::Assign(candidate->content_key, key);
  strings::Assign(candidate->value, value);
  return candidate;
}

Segment::Candidate *AddMetaCandidate(const absl::string_view key,
                                     const absl::string_view value,
                                     Segment *segment) {
  Segment::Candidate *candidate = segment->add_meta_candidate();
  strings::Assign(candidate->key, key);
  strings::Assign(candidate->content_key, key);
  strings::Assign(candidate->value, value);
  return candidate;
}

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

::testing::AssertionResult EnsurePreedit(const absl::string_view expected,
                                         const commands::Command &command) {
  if (!command.output().has_preedit()) {
    return ::testing::AssertionFailure() << "No preedit.";
  }
  std::string actual;
  for (size_t i = 0; i < command.output().preedit().segment_size(); ++i) {
    actual.append(command.output().preedit().segment(i).value());
  }
  if (expected == actual) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
         << "expected: " << expected << ", actual: " << actual;
}

::testing::AssertionResult EnsureSingleSegment(
    const absl::string_view expected, const commands::Command &command) {
  if (!command.output().has_preedit()) {
    return ::testing::AssertionFailure() << "No preedit.";
  }
  if (command.output().preedit().segment_size() != 1) {
    return ::testing::AssertionFailure()
           << "Not single segment. segment size: "
           << command.output().preedit().segment_size();
  }
  const commands::Preedit::Segment &segment =
      command.output().preedit().segment(0);
  if (!segment.has_value()) {
    return ::testing::AssertionFailure() << "No segment value.";
  }
  const absl::string_view actual = segment.value();
  if (expected == actual) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
         << "expected: " << expected << ", actual: " << actual;
}

::testing::AssertionResult EnsureSingleSegmentAndKey(
    const absl::string_view expected_value,
    const absl::string_view expected_key, const commands::Command &command) {
  if (!command.output().has_preedit()) {
    return ::testing::AssertionFailure() << "No preedit.";
  }
  if (command.output().preedit().segment_size() != 1) {
    return ::testing::AssertionFailure()
           << "Not single segment. segment size: "
           << command.output().preedit().segment_size();
  }
  const commands::Preedit::Segment &segment =
      command.output().preedit().segment(0);
  if (!segment.has_value()) {
    return ::testing::AssertionFailure() << "No segment value.";
  }
  if (!segment.has_key()) {
    return ::testing::AssertionFailure() << "No segment key.";
  }
  const std::string &actual_value = segment.value();
  const std::string &actual_key = segment.key();
  if (expected_value == actual_value && expected_key == actual_key) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure() << "expected_value: " << expected_value
                                       << ", actual_value: " << actual_value
                                       << ", expected_key: " << expected_key
                                       << ", actual_key: " << actual_key;
}

::testing::AssertionResult EnsureResult(const absl::string_view expected,
                                        const commands::Command &command) {
  if (!command.output().has_result()) {
    return ::testing::AssertionFailure() << "No result.";
  }
  if (!command.output().result().has_value()) {
    return ::testing::AssertionFailure() << "No result value.";
  }
  const std::string &actual = command.output().result().value();
  if (expected == actual) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure()
         << "expected: " << expected << ", actual: " << actual;
}

::testing::AssertionResult EnsureResultAndKey(
    const absl::string_view expected_value,
    const absl::string_view expected_key, const commands::Command &command) {
  if (!command.output().has_result()) {
    return ::testing::AssertionFailure() << "No result.";
  }
  if (!command.output().result().has_value()) {
    return ::testing::AssertionFailure() << "No result value.";
  }
  if (!command.output().result().has_key()) {
    return ::testing::AssertionFailure() << "No result value.";
  }
  const std::string &actual_value = command.output().result().value();
  const std::string &actual_key = command.output().result().key();
  if (expected_value == actual_value && expected_key == actual_key) {
    return ::testing::AssertionSuccess();
  }
  return ::testing::AssertionFailure() << "expected_value: " << expected_value
                                       << ", actual_value: " << actual_value
                                       << ", expected_key: " << expected_key
                                       << ", actual_key: " << actual_key;
}

::testing::AssertionResult TryUndoAndAssertSuccess(Session *session) {
  commands::Command command;
  session->RequestUndo(&command);
  if (!command.output().consumed()) {
    return ::testing::AssertionFailure() << "Not consumed.";
  }
  if (!command.output().has_callback()) {
    return ::testing::AssertionFailure() << "No callback.";
  }
  if (command.output().callback().session_command().type() !=
      commands::SessionCommand::UNDO) {
    return ::testing::AssertionFailure()
           << "Callback type is not Undo. Actual type: "
           << command.output().callback().session_command().type();
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult TryUndoAndAssertDoNothing(Session *session) {
  commands::Command command;
  session->RequestUndo(&command);
  if (command.output().consumed()) {
    return ::testing::AssertionFailure()
           << "Key event is consumed against expectation.";
  }
  return ::testing::AssertionSuccess();
}

#define EXPECT_PREEDIT(expected, command) \
  EXPECT_TRUE(EnsurePreedit(expected, command))
#define EXPECT_SINGLE_SEGMENT(expected, command) \
  EXPECT_TRUE(EnsureSingleSegment(expected, command))
#define EXPECT_SINGLE_SEGMENT_AND_KEY(expected_value, expected_key, command) \
  EXPECT_TRUE(EnsureSingleSegmentAndKey(expected_value, expected_key, command))
#define EXPECT_RESULT(expected, command) \
  EXPECT_TRUE(EnsureResult(expected, command))
#define EXPECT_RESULT_AND_KEY(expected_value, expected_key, command) \
  EXPECT_TRUE(EnsureResultAndKey(expected_value, expected_key, command))

void SwitchInputFieldType(commands::Context::InputFieldType type,
                          Session *session) {
  commands::Command command;
  SetSendCommandCommand(commands::SessionCommand::SWITCH_INPUT_FIELD_TYPE,
                        &command);
  command.mutable_input()->mutable_context()->set_input_field_type(type);
  EXPECT_TRUE(session->SendCommand(&command));
  EXPECT_EQ(session->context().composer().GetInputFieldType(), type);
}

bool SwitchInputModeCommand(commands::CompositionMode mode, Session *session,
                            commands::Command *command) {
  SetSendCommandCommand(commands::SessionCommand::SWITCH_INPUT_MODE, command);
  command->mutable_input()->mutable_command()->set_composition_mode(mode);
  return session->SendCommand(command);
}

void SwitchInputMode(commands::CompositionMode mode, Session *session) {
  commands::Command command;
  EXPECT_TRUE(SwitchInputModeCommand(mode, session, &command));
}

}  // namespace

class SessionTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    mobile_request_ = std::make_unique<Request>();
    request_test_util::FillMobileRequest(mobile_request_.get());

    mock_data_engine_ = MockDataEngineFactory::Create().value();

    t13n_rewriter_ = std::make_unique<TransliterationRewriter>(
        dictionary::PosMatcher(mock_data_manager_.GetPosMatcherData()));
  }

  void TearDown() override {}

  void InsertCharacterChars(const absl::string_view chars, Session *session,
                            commands::Command *command) const {
    constexpr uint32_t kNoModifiers = 0;
    for (int i = 0; i < chars.size(); ++i) {
      command->Clear();
      command->mutable_input()->set_type(commands::Input::SEND_KEY);
      commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
      key_event->set_key_code(chars[i]);
      key_event->set_modifiers(kNoModifiers);
      session->SendKey(command);
    }
  }

  void InsertCharacterCharsWithContext(const absl::string_view chars,
                                       const commands::Context &context,
                                       Session *session,
                                       commands::Command *command) const {
    constexpr uint32_t kNoModifiers = 0;
    for (size_t i = 0; i < chars.size(); ++i) {
      command->Clear();
      command->mutable_input()->set_type(commands::Input::SEND_KEY);
      *command->mutable_input()->mutable_context() = context;
      commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
      key_event->set_key_code(chars[i]);
      key_event->set_modifiers(kNoModifiers);
      session->SendKey(command);
    }
  }

  void InsertCharacterString(const absl::string_view key_strings,
                             const absl::string_view chars, Session *session,
                             commands::Command *command) const {
    constexpr uint32_t kNoModifiers = 0;
    auto chars_it = chars.begin();
    for (const absl::string_view key : Utf8AsChars(key_strings)) {
      // MSVC fails to compile if this is spelled as
      // `CHECK_NE(chars_it, chars.end())`.
      CHECK(chars_it != chars.end());
      command->Clear();
      command->mutable_input()->set_type(commands::Input::SEND_KEY);
      commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
      key_event->set_key_code(*chars_it++);
      key_event->set_modifiers(kNoModifiers);
      key_event->set_key_string(key);
      session->SendKey(command);
    }
  }

  // set result for "あいうえお"
  void SetAiueo(Segments *segments) {
    segments->Clear();
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments->add_segment();
    segment->set_key("あいうえお");
    candidate = segment->add_candidate();
    candidate->key = "あいうえお";
    candidate->content_key = "あいうえお";
    candidate->value = "あいうえお";
    candidate = segment->add_candidate();
    candidate->key = "あいうえお";
    candidate->content_key = "あいうえお";
    candidate->value = "アイウエオ";
  }

  void InitSessionToDirect(Session *session) {
    InitSessionToPrecomposition(session);
    commands::Command command;
    session->IMEOff(&command);
  }

  void InitSessionToConversionWithAiueo(Session *session,
                                        MockConverter *converter) {
    InitSessionToPrecomposition(session);

    commands::Command command;
    InsertCharacterChars("aiueo", session, &command);
    const ConversionRequest request = CreateConversionRequest(*session);
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

    command.Clear();
    EXPECT_TRUE(session->Convert(&command));
    EXPECT_EQ(session->context().state(), ImeContext::CONVERSION);
    Mock::VerifyAndClearExpectations(converter);
  }

  std::shared_ptr<MockConverter> CreateEngineConverterMock(
      MockEngine *mock_engine) {
    auto mock_converter = std::make_shared<MockConverter>();
    EXPECT_CALL(*mock_engine, CreateEngineConverter)
        .WillRepeatedly([mock_converter]() {
          return std::make_unique<engine::EngineConverter>(mock_converter);
        });
    return mock_converter;
  }

  // TODO(matsuzakit): Set the session's state to PRECOMPOSITION.
  // Though the method name asserts "ToPrecomposition",
  // this method doesn't change session's state.
  void InitSessionToPrecomposition(Session *session) {
#ifdef _WIN32
    // Session is created with direct mode on Windows
    // Direct status
    commands::Command command;
    session->IMEOn(&command);
#endif  // _WIN32
    InitSessionWithRequest(session, commands::Request::default_instance());
  }

  void InitSessionToPrecomposition(Session *session,
                                   const commands::Request &request) {
#ifdef _WIN32
    // Session is created with direct mode on Windows
    // Direct status
    commands::Command command;
    session->IMEOn(&command);
#endif  // _WIN32
    InitSessionWithRequest(session, request);
  }

  void InitSessionWithRequest(Session *session,
                              const commands::Request &request) {
    session->SetRequest(request);
    auto table = std::make_shared<composer::Table>();
    table->InitializeWithRequestAndConfig(
        request, config::ConfigHandler::DefaultConfig());
    session->SetTable(table);
  }

  // set result for "like"
  void SetLike(Segments *segments) {
    Segment *segment;
    Segment::Candidate *candidate;

    segments->Clear();
    segment = segments->add_segment();

    segment->set_key("ぃ");
    candidate = segment->add_candidate();
    candidate->value = "ぃ";

    candidate = segment->add_candidate();
    candidate->value = "ィ";

    segment = segments->add_segment();
    segment->set_key("け");
    candidate = segment->add_candidate();
    candidate->value = "家";
    candidate = segment->add_candidate();
    candidate->value = "け";
  }

  void FillT13Ns(const ConversionRequest &request, Segments *segments) {
    t13n_rewriter_->Rewrite(request, segments);
  }

  ConversionRequest CreateConversionRequest(const Session &session) {
    const ImeContext &context = session.context();
    return ConversionRequestBuilder()
        .SetComposer(context.composer())
        .SetRequestView(context.GetRequest())
        .SetContextView(context.client_context())
        .SetConfigView(context.GetConfig())
        .Build();
  }

  void SetupMockForReverseConversion(const absl::string_view kanji,
                                     const absl::string_view hiragana,
                                     MockConverter *converter) {
    // Set up Segments for reverse conversion.
    Segments reverse_segments;
    Segment *segment;
    segment = reverse_segments.add_segment();
    segment->set_key(kanji);
    Segment::Candidate *candidate;
    candidate = segment->add_candidate();
    // For reverse conversion, key is the original kanji string.
    candidate->key = kanji;
    candidate->value = hiragana;
    EXPECT_CALL(*converter, StartReverseConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(reverse_segments), Return(true)));
    // Set up Segments for forward conversion.
    Segments segments;
    segment = segments.add_segment();
    segment->set_key(hiragana);
    candidate = segment->add_candidate();
    candidate->key = hiragana;
    candidate->value = kanji;
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  void SetupCommandForReverseConversion(const absl::string_view text,
                                        commands::Input *input) {
    input->Clear();
    input->set_type(commands::Input::SEND_COMMAND);
    input->mutable_command()->set_type(
        commands::SessionCommand::CONVERT_REVERSE);
    input->mutable_command()->set_text(text);
  }

  void SetupZeroQuerySuggestionReady(bool enable, Session *session,
                                     commands::Request *request,
                                     MockConverter *mock_converter) {
    InitSessionToPrecomposition(session);

    // Enable zero query suggest.
    request->set_zero_query_suggestion(enable);
    session->SetRequest(*request);

    // Type "google".
    commands::Command command;
    InsertCharacterChars("google", session, &command);

    {
      // Set up a mock conversion result.
      Segments segments;
      Segment *segment;
      segment = segments.add_segment();
      segment->set_key("google");
      segment->add_candidate()->value = "GOOGLE";
      EXPECT_CALL(*mock_converter, StartConversion(_, _))
          .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
    }
    command.Clear();
    session->Convert(&command);

    {
      // Set up a mock suggestion result.
      Segments segments;
      Segment *segment;
      segment = segments.add_segment();
      segment->set_key("");
      AddCandidate("search", "search", segment);
      AddCandidate("input", "input", segment);
      EXPECT_CALL(*mock_converter, StartPrediction(_, _))
          .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
    }

    {
      // Set up a mock prediction result.
      Segments segments;
      Segment *segment;
      segment = segments.add_segment();
      segment->set_key("");
      AddCandidate("search", "search", segment);
      AddCandidate("input", "input", segment);
      EXPECT_CALL(*mock_converter, StartPrediction(_, _))
          .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
    }
  }

  void SetupZeroQuerySuggestion(Session *session, commands::Request *request,
                                commands::Command *command,
                                MockConverter *converter) {
    SetupZeroQuerySuggestionReady(true, session, request, converter);
    command->Clear();
    session->Commit(command);
  }

  void SetUndoContext(Session *session, MockConverter *converter) {
    commands::Command command;
    Segments segments;

    {  // Create segments
      InsertCharacterChars("aiueo", session, &command);
      SetAiueo(&segments);
      // Don't use FillT13Ns(). It makes platform dependent segments.
      // TODO(hsumita): Makes FillT13Ns() independent from platforms.
      Segment::Candidate *candidate;
      candidate = segments.mutable_segment(0)->add_candidate();
      candidate->value = "aiueo";
      candidate = segments.mutable_segment(0)->add_candidate();
      candidate->value = "AIUEO";
    }

    {  // Commit the composition to make an undo context.
      EXPECT_CALL(*converter, StartConversion(_, _))
          .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
      command.Clear();
      session->Convert(&command);
      EXPECT_FALSE(command.output().has_result());
      EXPECT_PREEDIT("あいうえお", command);

      EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
          .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
      command.Clear();

      session->Commit(&command);
      EXPECT_FALSE(command.output().has_preedit());
      EXPECT_RESULT("あいうえお", command);
      Mock::VerifyAndClearExpectations(converter);
    }
  }

  // IMPORTANT: Use std::unique_ptr and instantiate an object in SetUp() method
  //    if the target object should be initialized *AFTER* global settings
  //    such as user profile dir or global config are set up for unit test.
  //    If you directly define a variable here without std::unique_ptr, its
  //    constructor will be called *BEFORE* SetUp() is called.
  std::unique_ptr<Engine> mock_data_engine_;
  std::unique_ptr<TransliterationRewriter> t13n_rewriter_;
  std::unique_ptr<Request> mobile_request_;
  const testing::MockDataManager mock_data_manager_;
};

// This test is intentionally defined at this location so that this
// test can ensure that the first SetUp() initialized table object to
// the default state.  Please do not define another test before this.
// FYI, each TEST_F will be eventually expanded into a global variable
// and global variables in a single translation unit (source file) are
// always initialized in the order in which they are defined.
TEST_F(SessionTest, TestOfTestForSetup) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  EXPECT_FALSE(config.has_use_auto_conversion())
      << "Global config should be initialized for each test fixture.";

  // Make sure that the default roman table is initialized.
  {
    MockEngine engine;
    std::shared_ptr<MockConverter> converter =
        CreateEngineConverterMock(&engine);

    Session session(engine);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    SendKey("a", &session, &command);
    EXPECT_SINGLE_SEGMENT("あ", command)
        << "Global Romaji table should be initialized for each test fixture.";
  }
}

TEST_F(SessionTest, TestSendKey) {
  MockEngine engine;
  auto converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;

  // Precomposition status
  TestSendKey("Up", &session, &command);
  EXPECT_FALSE(command.output().consumed());

  SendKey("Up", &session, &command);
  EXPECT_FALSE(command.output().consumed());

  // InsertSpace on Precomposition status
  // TODO(komatsu): Test both cases of config.ascii_character_form() is
  // FULL_WIDTH and HALF_WIDTH.
  TestSendKey("Space", &session, &command);
  const bool consumed_on_testsendkey = command.output().consumed();
  SendKey("Space", &session, &command);
  const bool consumed_on_sendkey = command.output().consumed();
  EXPECT_EQ(consumed_on_sendkey, consumed_on_testsendkey);

  // Precomposition status
  TestSendKey("G", &session, &command);
  EXPECT_TRUE(command.output().consumed());
  SendKey("G", &session, &command);
  EXPECT_TRUE(command.output().consumed());

  // Composition status
  TestSendKey("Up", &session, &command);
  EXPECT_TRUE(command.output().consumed());
  SendKey("Up", &session, &command);
  EXPECT_TRUE(command.output().consumed());
}

TEST_F(SessionTest, UpdateComposition) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  commands::Input *input = command.mutable_input();
  input->set_type(commands::Input::SEND_COMMAND);
  input->mutable_command()->set_type(
      commands::SessionCommand::UPDATE_COMPOSITION);
  commands::SessionCommand::CompositionEvent *composition_event =
      input->mutable_command()->add_composition_events();
  composition_event->set_composition_string("かん字");
  composition_event->set_probability(1.0);

  EXPECT_TRUE(session.UpdateComposition(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().preedit().segment(0).value(), "かん字");
}

TEST_F(SessionTest, SendCommand) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  InsertCharacterChars("kanji", &session, &command);

  // REVERT
  SendCommand(commands::SessionCommand::REVERT, &session, &command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_candidate_window());

  // SUBMIT
  InsertCharacterChars("k", &session, &command);
  SendCommand(commands::SessionCommand::SUBMIT, &session, &command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_RESULT("ｋ", command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_candidate_window());

  // SWITCH_INPUT_MODE
  SendKey("a", &session, &command);
  EXPECT_SINGLE_SEGMENT("あ", command);

  SwitchInputMode(commands::FULL_ASCII, &session);

  SendKey("a", &session, &command);
  EXPECT_SINGLE_SEGMENT("あａ", command);

  // GET_STATUS
  SendCommand(commands::SessionCommand::GET_STATUS, &session, &command);
  // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
  SwitchInputMode(commands::FULL_ASCII, &session);

  // RESET_CONTEXT
  // test of reverting composition
  InsertCharacterChars("kanji", &session, &command);
  SendCommand(commands::SessionCommand::RESET_CONTEXT, &session, &command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_candidate_window());
  // test of resetting the history segements
  {
    MockEngine engine;
    std::shared_ptr<MockConverter> converter =
        CreateEngineConverterMock(&engine);
    // ResetConversion is called twice, first in IMEOff through
    // InitSessionToPrecomposition() and then EchoBack() through
    // SendCommand().
    EXPECT_CALL(*converter, ResetConversion(_)).Times(2);
    Session session(engine);
    InitSessionToPrecomposition(&session);
    SendCommand(commands::SessionCommand::RESET_CONTEXT, &session, &command);
    EXPECT_FALSE(command.output().consumed());
  }
}

TEST_F(SessionTest, SwitchInputMode) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    // SWITCH_INPUT_MODE
    SendKey("a", &session, &command);
    EXPECT_SINGLE_SEGMENT("あ", command);

    SwitchInputMode(commands::FULL_ASCII, &session);

    SendKey("a", &session, &command);
    EXPECT_SINGLE_SEGMENT("あａ", command);

    // GET_STATUS
    SendCommand(commands::SessionCommand::GET_STATUS, &session, &command);
    // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
    EXPECT_EQ(command.output().mode(), commands::FULL_ASCII);
  }

  {
    // Confirm that we can change the mode from DIRECT
    // to other modes directly (without IMEOn command).
    Session session(engine);
    InitSessionToDirect(&session);

    commands::Command command;

    // GET_STATUS
    SendCommand(commands::SessionCommand::GET_STATUS, &session, &command);
    // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
    EXPECT_EQ(command.output().mode(), commands::DIRECT);

    // SWITCH_INPUT_MODE
    SwitchInputMode(commands::HIRAGANA, &session);

    // GET_STATUS
    SendCommand(commands::SessionCommand::GET_STATUS, &session, &command);
    // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);

    SendKey("a", &session, &command);
    EXPECT_SINGLE_SEGMENT("あ", command);

    // GET_STATUS
    SendCommand(commands::SessionCommand::GET_STATUS, &session, &command);
    // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  }
}

TEST_F(SessionTest, SwitchInputModeWithCandidateList) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {
    Session session(engine);
    InitSessionToPrecomposition(&session);

    // Enable zero query suggest.
    commands::Request request;
    SetupZeroQuerySuggestionReady(true, &session, &request, converter.get());

    commands::Command command;
    session.Commit(&command);
    EXPECT_EQ(command.output().result().value(), "GOOGLE");
    EXPECT_EQ(GetComposition(command), "");

    EXPECT_TRUE(command.output().has_all_candidate_words());
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);

    // SWITCH_INPUT_MODE
    command.Clear();
    EXPECT_TRUE(
        SwitchInputModeCommand(commands::FULL_ASCII, &session, &command));

    // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
    EXPECT_EQ(command.output().mode(), commands::FULL_ASCII);
    EXPECT_TRUE(command.output().has_all_candidate_words());
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
  }

  {
    Session session(engine);
    InitSessionToPrecomposition(&session);

    {
      // Set up a mock conversion result.
      Segments segments;
      Segment *segment;
      segment = segments.add_segment();
      segment->set_key("");
      segment->add_candidate()->value = "google";
      EXPECT_CALL(*converter, StartPrediction(_, _))
          .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    }
    // Type "g".
    commands::Command command;
    InsertCharacterChars("g", &session, &command);

    EXPECT_TRUE(command.output().has_all_candidate_words());
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);

    // SWITCH_INPUT_MODE
    command.Clear();
    EXPECT_TRUE(
        SwitchInputModeCommand(commands::FULL_ASCII, &session, &command));

    // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
    EXPECT_EQ(command.output().mode(), commands::FULL_ASCII);
    EXPECT_TRUE(command.output().has_all_candidate_words());
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
  }
}

TEST_F(SessionTest, RevertComposition) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // Issue#2237323
  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  InsertCharacterChars("aiueo", &session, &command);
  const ConversionRequest request = CreateConversionRequest(session);
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);

  // REVERT
  SendCommand(commands::SessionCommand::REVERT, &session, &command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_candidate_window());

  SendKey("a", &session, &command);
  EXPECT_SINGLE_SEGMENT("あ", command);
}

TEST_F(SessionTest, InputMode) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  EXPECT_TRUE(session.InputModeHalfASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HALF_ASCII);

  SendKey("a", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).key(), "a");

  command.Clear();
  session.Commit(&command);

  // Input mode remains even after submission.
  command.Clear();
  session.GetStatus(&command);
  EXPECT_EQ(command.output().mode(), mozc::commands::HALF_ASCII);
}

TEST_F(SessionTest, SelectCandidate) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("aiueo", &session, &command);
  const ConversionRequest request = CreateConversionRequest(session);
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);

  command.Clear();
  session.ConvertNext(&command);

  SetSendCommandCommand(commands::SessionCommand::SELECT_CANDIDATE, &command);
  command.mutable_input()->mutable_command()->set_id(
      -(transliteration::HALF_KATAKANA + 1));
  session.SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("ｱｲｳｴｵ", command);
  EXPECT_FALSE(command.output().has_candidate_window());
}

TEST_F(SessionTest, HighlightCandidate) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("aiueo", &session, &command);
  const ConversionRequest request = CreateConversionRequest(session);
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);

  command.Clear();
  session.ConvertNext(&command);
  EXPECT_SINGLE_SEGMENT("アイウエオ", command);

  SetSendCommandCommand(commands::SessionCommand::HIGHLIGHT_CANDIDATE,
                        &command);
  command.mutable_input()->mutable_command()->set_id(
      -(transliteration::HALF_KATAKANA + 1));
  session.SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_SINGLE_SEGMENT("ｱｲｳｴｵ", command);
  EXPECT_TRUE(command.output().has_candidate_window());
}

TEST_F(SessionTest, Conversion) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("aiueo", &session, &command);
  const ConversionRequest request = CreateConversionRequest(session);
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  EXPECT_SINGLE_SEGMENT_AND_KEY("あいうえお", "あいうえお", command);

  command.Clear();
  session.Convert(&command);

  command.Clear();
  session.ConvertNext(&command);

  std::string key;
  for (int i = 0; i < command.output().preedit().segment_size(); ++i) {
    EXPECT_TRUE(command.output().preedit().segment(i).has_value());
    EXPECT_TRUE(command.output().preedit().segment(i).has_key());
    key += command.output().preedit().segment(i).key();
  }
  EXPECT_EQ(key, "あいうえお");
}

TEST_F(SessionTest, SegmentWidthShrink) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("aiueo", &session, &command);
  const ConversionRequest request = CreateConversionRequest(session);
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);

  command.Clear();
  session.SegmentWidthShrink(&command);

  command.Clear();
  session.SegmentWidthShrink(&command);
}

TEST_F(SessionTest, ConvertPrev) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("aiueo", &session, &command);
  const ConversionRequest request = CreateConversionRequest(session);
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);

  command.Clear();
  session.ConvertNext(&command);

  command.Clear();
  session.ConvertPrev(&command);

  command.Clear();
  session.ConvertPrev(&command);
}

TEST_F(SessionTest, ResetFocusedSegmentAfterCommit) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("watasinonamaehanakanodesu", &session, &command);
  // "わたしのなまえはなかのです[]"

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("わたしの");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "私の";
  candidate = segment->add_candidate();
  candidate->value = "わたしの";
  candidate = segment->add_candidate();
  candidate->value = "渡しの";

  segment = segments.add_segment();
  segment->set_key("なまえは");
  candidate = segment->add_candidate();
  candidate->value = "名前は";
  candidate = segment->add_candidate();
  candidate->value = "ナマエは";

  segment = segments.add_segment();
  segment->set_key("なかのです");
  candidate = segment->add_candidate();
  candidate->value = "中野です";
  candidate = segment->add_candidate();
  candidate->value = "なかのです";
  const ConversionRequest request1 = CreateConversionRequest(session);
  FillT13Ns(request1, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "[私の]名前は中野です"
  command.Clear();
  session.SegmentFocusRight(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "私の[名前は]中野です"
  command.Clear();
  session.SegmentFocusRight(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "私の名前は[中野です]"

  command.Clear();
  session.ConvertNext(&command);
  EXPECT_EQ(command.output().candidate_window().focused_index(), 1);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "私の名前は[中のです]"

  command.Clear();
  session.ConvertNext(&command);
  EXPECT_EQ(command.output().candidate_window().focused_index(), 2);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "私の名前は[なかのです]"

  command.Clear();
  session.Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_TRUE(command.output().has_result());
  // "私の名前はなかのです[]"
  Mock::VerifyAndClearExpectations(converter.get());

  InsertCharacterChars("a", &session, &command);

  segments.Clear();
  segment = segments.add_segment();
  segment->set_key("あ");
  candidate = segment->add_candidate();
  candidate->value = "阿";
  candidate = segment->add_candidate();
  candidate->value = "亜";

  const ConversionRequest request2 = CreateConversionRequest(session);
  FillT13Ns(request2, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  // "あ[]"

  command.Clear();
  session.Convert(&command);
  // "[阿]"

  command.Clear();
  // If the forcused_segment_ was not reset, this raises segmentation fault.
  session.ConvertNext(&command);
  // "[亜]"
}

TEST_F(SessionTest, ResetFocusedSegmentAfterCancel) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("ai", &session, &command);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("あい");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "愛";
  candidate = segment->add_candidate();
  candidate->value = "相";
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  // "あい[]"

  command.Clear();
  session.Convert(&command);
  // "[愛]"
  Mock::VerifyAndClearExpectations(converter.get());

  segments.Clear();
  segment = segments.add_segment();
  segment->set_key("あ");
  candidate = segment->add_candidate();
  candidate->value = "あ";
  segment = segments.add_segment();
  segment->set_key("い");
  candidate = segment->add_candidate();
  candidate->value = "い";
  candidate = segment->add_candidate();
  candidate->value = "位";
  EXPECT_CALL(*converter, ResizeSegment(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));

  command.Clear();
  session.SegmentWidthShrink(&command);
  // "[あ]い"
  Mock::VerifyAndClearExpectations(converter.get());

  segment = segments.mutable_segment(0);
  segment->set_segment_type(Segment::FIXED_VALUE);
  EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));

  command.Clear();
  session.SegmentFocusRight(&command);
  // "あ[い]"

  command.Clear();
  session.ConvertNext(&command);
  // "あ[位]"

  command.Clear();
  session.ConvertCancel(&command);
  // "あい[]"
  Mock::VerifyAndClearExpectations(converter.get());

  segments.Clear();
  segment = segments.add_segment();
  segment->set_key("あい");
  candidate = segment->add_candidate();
  candidate->value = "愛";
  candidate = segment->add_candidate();
  candidate->value = "相";
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);
  // "[愛]"

  command.Clear();
  // If the forcused_segment_ was not reset, this raises segmentation fault.
  session.Convert(&command);
  // "[相]"
}

TEST_F(SessionTest, KeepFixedCandidateAfterSegmentWidthExpand) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // Issue#1271099
  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("bariniryokouniitta", &session, &command);
  // "ばりにりょこうにいった[]"

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("ばりに");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "バリに";
  candidate = segment->add_candidate();
  candidate->value = "針に";

  segment = segments.add_segment();
  segment->set_key("りょこうに");
  candidate = segment->add_candidate();
  candidate->value = "旅行に";

  segment = segments.add_segment();
  segment->set_key("いった");
  candidate = segment->add_candidate();
  candidate->value = "行った";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);
  // ex. "[バリに]旅行に行った"
  EXPECT_EQ(GetComposition(command), "バリに旅行に行った");
  command.Clear();
  session.ConvertNext(&command);
  // ex. "[針に]旅行に行った"
  const std::string first_segment =
      command.output().preedit().segment(0).value();

  segment = segments.mutable_segment(0);
  segment->set_segment_type(Segment::FIXED_VALUE);
  segment->move_candidate(1, 0);
  EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));

  command.Clear();
  session.SegmentFocusRight(&command);
  // ex. "針に[旅行に]行った"
  // Make sure the first segment (i.e. "針に" in the above case) remains
  // after moving the focused segment right.
  EXPECT_EQ(command.output().preedit().segment(0).value(), first_segment);

  segment = segments.mutable_segment(1);
  segment->set_key("りょこうにい");
  candidate = segment->mutable_candidate(0);
  candidate->value = "旅行に行";

  segment = segments.mutable_segment(2);
  segment->set_key("った");
  candidate = segment->mutable_candidate(0);
  candidate->value = "った";

  EXPECT_CALL(*converter, ResizeSegment(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));

  command.Clear();
  session.SegmentWidthExpand(&command);
  // ex. "針に[旅行に行]った"

  // Make sure the first segment (i.e. "針に" in the above case) remains
  // after expanding the focused segment.
  EXPECT_EQ(command.output().preedit().segment(0).value(), first_segment);
}

TEST_F(SessionTest, CommitSegment) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // Issue#1560608
  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("watasinonamae", &session, &command);
  // "わたしのなまえ[]"

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("わたしの");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "私の";
  candidate = segment->add_candidate();
  candidate->value = "わたしの";
  candidate = segment->add_candidate();
  candidate->value = "渡しの";

  segment = segments.add_segment();
  segment->set_key("なまえ");
  candidate = segment->add_candidate();
  candidate->value = "名前";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);
  EXPECT_EQ(command.output().candidate_window().focused_index(), 0);
  // "[私の]名前"

  command.Clear();
  session.ConvertNext(&command);
  EXPECT_EQ(command.output().candidate_window().focused_index(), 1);
  // "[わたしの]名前"

  command.Clear();
  session.ConvertNext(&command);
  // "[渡しの]名前" showing a candidate window
  EXPECT_EQ(command.output().candidate_window().focused_index(), 2);

  segment = segments.mutable_segment(0);
  segment->set_segment_type(Segment::FIXED_VALUE);
  segment->move_candidate(2, 0);

  EXPECT_CALL(*converter, CommitSegments(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));

  command.Clear();
  session.CommitSegment(&command);
  // "渡しの" + "[名前]"
  EXPECT_EQ(command.output().candidate_window().focused_index(), 0);
}

TEST_F(SessionTest, CommitSegmentAt2ndSegment) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("watasinohaha", &session, &command);
  // "わたしのはは[]"

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("わたしの");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "私の";
  segment = segments.add_segment();
  segment->set_key("はは");
  candidate = segment->add_candidate();
  candidate->value = "母";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);
  // "[私の]母"

  command.Clear();
  session.SegmentFocusRight(&command);
  // "私の[母]"

  segment->set_segment_type(Segment::FIXED_VALUE);
  segment->move_candidate(1, 0);
  EXPECT_CALL(*converter, CommitSegments(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));

  command.Clear();
  session.CommitSegment(&command);
  // "私の" + "[母]"

  segment->set_key("は");
  candidate->value = "葉";
  segment = segments.add_segment();
  segment->set_key("は");
  candidate = segment->add_candidate();
  candidate->value = "は";
  segments.pop_front_segment();
  EXPECT_CALL(*converter, ResizeSegment(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));

  command.Clear();
  session.SegmentWidthShrink(&command);
  // "私の" + "[葉]は"
  EXPECT_EQ(command.output().preedit().segment_size(), 2);
}

TEST_F(SessionTest, Transliterations) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("jishin", &session, &command);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("じしん");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "自信";
  candidate = segment->add_candidate();
  candidate->value = "自身";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);

  command.Clear();
  session.ConvertNext(&command);

  command.Clear();
  session.TranslateHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("jishin", command);

  command.Clear();
  session.TranslateHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("JISHIN", command);

  command.Clear();
  session.TranslateHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("Jishin", command);

  command.Clear();
  session.TranslateHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("jishin", command);
}

TEST_F(SessionTest, TransliterationOfNegativeNumber) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("-255", &session, &command);
  // "－" (U+FF0D) is used as a minus sign in Windows.
  EXPECT_TRUE(EnsureSingleSegment("−２５５", command) ||  // "−" is U+2212
              EnsureSingleSegment("－２５５", command));  // "－" is U+FF0D

  command.Clear();
  session.TranslateHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("-255", command);
}

TEST_F(SessionTest, ConvertToTransliteration) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("jishin", &session, &command);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("じしん");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "自信";
  candidate = segment->add_candidate();
  candidate->value = "自身";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.ConvertToHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("jishin", command);

  command.Clear();
  session.ConvertToHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("JISHIN", command);

  command.Clear();
  session.ConvertToHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("Jishin", command);

  command.Clear();
  session.ConvertToHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("jishin", command);
}

TEST_F(SessionTest, ConvertToTransliterationOfNegativeNumber) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("-789", &session, &command);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("−７８９");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "−７８９";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.ConvertToHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("-789", command);

  command.Clear();
  session.ConvertToHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("-789", command);
}

TEST_F(SessionTest, ConvertToTransliterationWithMultipleSegments) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("like", &session, &command);

  Segments segments;
  SetLike(&segments);
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  // Convert
  command.Clear();
  session.Convert(&command);
  {  // Check the conversion #1
    const commands::Output &output = command.output();
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidate_window());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 2);
    EXPECT_EQ(conversion.segment(0).value(), "ぃ");
    EXPECT_EQ(conversion.segment(1).value(), "家");
  }

  // TranslateHalfASCII
  command.Clear();
  session.TranslateHalfASCII(&command);
  {  // Check the conversion #2
    const commands::Output &output = command.output();
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidate_window());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 2);
    EXPECT_EQ(conversion.segment(0).value(), "li");
  }
}

TEST_F(SessionTest, ConvertToHalfWidth) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("abc", &session, &command);

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    segment->set_key("あｂｃ");
    segment->add_candidate()->value = "あべし";
  }
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.ConvertToHalfWidth(&command);
  EXPECT_SINGLE_SEGMENT("ｱbc", command);

  command.Clear();
  session.ConvertToFullASCII(&command);
  // The output is "ａｂｃ".

  command.Clear();
  session.ConvertToHalfWidth(&command);
  EXPECT_SINGLE_SEGMENT("abc", command);
}

TEST_F(SessionTest, ConvertConsonantsToFullAlphanumeric) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("dvd", &session, &command);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("ｄｖｄ");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "DVD";
  candidate = segment->add_candidate();
  candidate->value = "dvd";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.ConvertToFullASCII(&command);
  EXPECT_SINGLE_SEGMENT("ｄｖｄ", command);

  command.Clear();
  session.ConvertToFullASCII(&command);
  EXPECT_SINGLE_SEGMENT("ＤＶＤ", command);

  command.Clear();
  session.ConvertToFullASCII(&command);
  EXPECT_SINGLE_SEGMENT("Ｄｖｄ", command);

  command.Clear();
  session.ConvertToFullASCII(&command);
  EXPECT_SINGLE_SEGMENT("ｄｖｄ", command);
}

TEST_F(SessionTest, ConvertConsonantsToFullAlphanumericWithoutCascadingWindow) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);

  config::Config config;
  config.set_use_cascading_window(false);
  session.SetConfig(config);

  commands::Command command;
  InitSessionToPrecomposition(&session);
  InsertCharacterChars("dvd", &session, &command);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("ｄｖｄ");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "DVD";
  candidate = segment->add_candidate();
  candidate->value = "dvd";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.ConvertToFullASCII(&command);
  EXPECT_SINGLE_SEGMENT("ｄｖｄ", command);

  command.Clear();
  session.ConvertToFullASCII(&command);
  EXPECT_SINGLE_SEGMENT("ＤＶＤ", command);

  command.Clear();
  session.ConvertToFullASCII(&command);
  EXPECT_SINGLE_SEGMENT("Ｄｖｄ", command);

  command.Clear();
  session.ConvertToFullASCII(&command);
  EXPECT_SINGLE_SEGMENT("ｄｖｄ", command);
}

// Convert input string to Hiragana, Katakana, and Half Katakana
TEST_F(SessionTest, SwitchKanaType) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {  // From composition mode.
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    InsertCharacterChars("abc", &session, &command);

    Segments segments;
    {  // Initialize segments.
      Segment *segment = segments.add_segment();
      segment->set_key("あｂｃ");
      segment->add_candidate()->value = "あべし";
    }

    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    command.Clear();
    session.SwitchKanaType(&command);
    EXPECT_SINGLE_SEGMENT("アｂｃ", command);

    command.Clear();
    session.SwitchKanaType(&command);
    EXPECT_SINGLE_SEGMENT("ｱbc", command);

    command.Clear();
    session.SwitchKanaType(&command);
    EXPECT_SINGLE_SEGMENT("あｂｃ", command);

    command.Clear();
    session.SwitchKanaType(&command);
    EXPECT_SINGLE_SEGMENT("アｂｃ", command);

    Mock::VerifyAndClearExpectations(converter.get());
  }

  {  // From conversion mode.
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    InsertCharacterChars("kanji", &session, &command);

    Segments segments;
    {  // Initialize segments.
      Segment *segment = segments.add_segment();
      segment->set_key("かんじ");
      segment->add_candidate()->value = "漢字";
    }

    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    command.Clear();
    session.Convert(&command);
    EXPECT_SINGLE_SEGMENT("漢字", command);

    command.Clear();
    session.SwitchKanaType(&command);
    EXPECT_SINGLE_SEGMENT("かんじ", command);

    command.Clear();
    session.SwitchKanaType(&command);
    EXPECT_SINGLE_SEGMENT("カンジ", command);

    command.Clear();
    session.SwitchKanaType(&command);
    EXPECT_SINGLE_SEGMENT("ｶﾝｼﾞ", command);

    command.Clear();
    session.SwitchKanaType(&command);
    EXPECT_SINGLE_SEGMENT("かんじ", command);

    Mock::VerifyAndClearExpectations(converter.get());
  }
}

// Rotate input mode among Hiragana, Katakana, and Half Katakana
TEST_F(SessionTest, InputModeSwitchKanaType) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  // HIRAGANA
  InsertCharacterChars("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ");
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);

  // HIRAGANA to FULL_KATAKANA
  command.Clear();
  session.Commit(&command);
  command.Clear();
  session.InputModeSwitchKanaType(&command);
  InsertCharacterChars("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "ア");
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(command.output().mode(), commands::FULL_KATAKANA);

  // FULL_KATRAKANA to HALF_KATAKANA
  command.Clear();
  session.Commit(&command);
  command.Clear();
  session.InputModeSwitchKanaType(&command);
  InsertCharacterChars("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "ｱ");
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(command.output().mode(), commands::HALF_KATAKANA);

  // HALF_KATAKANA to HIRAGANA
  command.Clear();
  session.Commit(&command);
  command.Clear();
  session.InputModeSwitchKanaType(&command);
  InsertCharacterChars("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ");
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);

  // To Half ASCII mode.
  command.Clear();
  session.Commit(&command);
  command.Clear();
  session.InputModeHalfASCII(&command);
  InsertCharacterChars("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "a");
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

  // HALF_ASCII to HALF_ASCII
  command.Clear();
  session.Commit(&command);
  command.Clear();
  session.InputModeSwitchKanaType(&command);
  InsertCharacterChars("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "a");
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

  // To Full ASCII mode.
  command.Clear();
  session.Commit(&command);
  command.Clear();
  session.InputModeFullASCII(&command);
  InsertCharacterChars("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "ａ");
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(command.output().mode(), commands::FULL_ASCII);

  // FULL_ASCII to FULL_ASCII
  command.Clear();
  session.Commit(&command);
  command.Clear();
  session.InputModeSwitchKanaType(&command);
  InsertCharacterChars("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "ａ");
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(command.output().mode(), commands::FULL_ASCII);
}

TEST_F(SessionTest, TranslateHalfWidth) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("abc", &session, &command);

  command.Clear();
  session.TranslateHalfWidth(&command);
  EXPECT_SINGLE_SEGMENT("ｱbc", command);

  command.Clear();
  session.TranslateFullASCII(&command);
  EXPECT_SINGLE_SEGMENT("ａｂｃ", command);

  command.Clear();
  session.TranslateHalfWidth(&command);
  EXPECT_SINGLE_SEGMENT("abc", command);
}

TEST_F(SessionTest, UpdatePreferences) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("aiueo", &session, &command);
  Segments segments;
  SetAiueo(&segments);

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

  SetSendKeyCommand("SPACE", &command);
  command.mutable_input()->mutable_config()->set_use_cascading_window(false);
  session.SendKey(&command);
  SetSendKeyCommand("SPACE", &command);
  session.SendKey(&command);

  const size_t no_cascading_cand_size =
      command.output().candidate_window().candidate_size();

  command.Clear();
  session.ConvertCancel(&command);

  SetSendKeyCommand("SPACE", &command);
  command.mutable_input()->mutable_config()->set_use_cascading_window(true);
  session.SendKey(&command);
  SetSendKeyCommand("SPACE", &command);
  session.SendKey(&command);

  const size_t cascading_cand_size =
      command.output().candidate_window().candidate_size();

#if defined(__linux__) || defined(__wasm__)
  EXPECT_EQ(cascading_cand_size, no_cascading_cand_size);
#else   // __linux__ || __wasm__
  EXPECT_GT(no_cascading_cand_size, cascading_cand_size);
#endif  // __linux__ || __wasm__

  command.Clear();
  session.ConvertCancel(&command);

  // On MS-IME keymap, EISU key does nothing.
  SetSendKeyCommand("EISU", &command);
  command.mutable_input()->mutable_config()->set_session_keymap(
      config::Config::MSIME);
  session.SendKey(&command);
  EXPECT_EQ(command.output().status().mode(), commands::HALF_ASCII);
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HALF_ASCII);

  // On KOTOERI keymap, EISU key does "ToggleAlphanumericMode".
  SetSendKeyCommand("EISU", &command);
  command.mutable_input()->mutable_config()->set_session_keymap(
      config::Config::KOTOERI);
  session.SendKey(&command);
  EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);
}

TEST_F(SessionTest, RomajiInput) {
  auto table = std::make_shared<composer::Table>();
  table->AddRule("pa", "ぱ", "");
  table->AddRule("n", "ん", "");
  table->AddRule("na", "な", "");
  // This rule makes the "n" rule ambiguous.

  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  session.get_internal_composer_only_for_unittest()->SetTable(table);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("pan", &session, &command);

  EXPECT_EQ(command.output().preedit().segment(0).value(), "ぱｎ");

  command.Clear();

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("ぱん");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "パン";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  session.ConvertToHiragana(&command);
  EXPECT_SINGLE_SEGMENT("ぱん", command);

  command.Clear();
  session.ConvertToHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("pan", command);
}

TEST_F(SessionTest, KanaInput) {
  auto table = std::make_shared<composer::Table>();
  table->AddRule("す゛", "ず", "");

  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  session.get_internal_composer_only_for_unittest()->SetTable(table);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  SetSendKeyCommand("m", &command);
  command.mutable_input()->mutable_key()->set_key_string("も");
  session.SendKey(&command);

  SetSendKeyCommand("r", &command);
  command.mutable_input()->mutable_key()->set_key_string("す");
  session.SendKey(&command);

  SetSendKeyCommand("@", &command);
  command.mutable_input()->mutable_key()->set_key_string("゛");
  session.SendKey(&command);

  SetSendKeyCommand("h", &command);
  command.mutable_input()->mutable_key()->set_key_string("く");
  session.SendKey(&command);

  SetSendKeyCommand("!", &command);
  command.mutable_input()->mutable_key()->set_key_string("!");
  session.SendKey(&command);

  EXPECT_EQ(command.output().preedit().segment(0).value(), "もずく！");

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("もずく!");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "もずく！";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.ConvertToHalfASCII(&command);
  EXPECT_SINGLE_SEGMENT("mr@h!", command);
}

TEST_F(SessionTest, ExceededComposition) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  const std::string exceeded_preedit(500, 'a');
  ASSERT_EQ(exceeded_preedit.size(), 500);
  InsertCharacterChars(exceeded_preedit, &session, &command);

  std::string long_a;
  for (int i = 0; i < 500; ++i) {
    long_a += "あ";
  }
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key(long_a);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = long_a;

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);
  EXPECT_FALSE(command.output().has_candidate_window());

  // The status should remain the preedit status, although the
  // previous command was convert.  The next command makes sure that
  // the preedit will disappear by canceling the preedit status.
  command.Clear();
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::ESCAPE);
  EXPECT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, OutputAllCandidateWords) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  Segments segments;
  SetAiueo(&segments);
  InsertCharacterChars("aiueo", &session, &command);

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  session.Convert(&command);
  {
    const commands::Output &output = command.output();
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(output.all_candidate_words().focused_index(), 0);
    EXPECT_EQ(output.all_candidate_words().category(), commands::CONVERSION);
#if defined(__linux__) || defined(__wasm__)
    // Cascading window is not supported on Linux, so the size of
    // candidate words is different from other platform.
    // TODO(komatsu): Modify the client for Linux to explicitly change
    // the preference rather than relying on the exceptional default value.
    // [ "あいうえお", "アイウエオ",
    //   "aiueo" (t13n), "AIUEO" (t13n), "Aieuo" (t13n),
    //   "ａｉｕｅｏ"  (t13n), "ＡＩＵＥＯ" (t13n), "Ａｉｅｕｏ" (t13n),
    //   "ｱｲｳｴｵ" (t13n) ]
    EXPECT_EQ(output.all_candidate_words().candidates_size(), 9);
#else   // __linux__ || __wasm__
    // [ "あいうえお", "アイウエオ", "アイウエオ" (t13n), "あいうえお" (t13n),
    //   "aiueo" (t13n), "AIUEO" (t13n), "Aieuo" (t13n),
    //   "ａｉｕｅｏ"  (t13n), "ＡＩＵＥＯ" (t13n), "Ａｉｅｕｏ" (t13n),
    //   "ｱｲｳｴｵ" (t13n) ]
    EXPECT_EQ(output.all_candidate_words().candidates_size(), 11);
#endif  // __linux__ || __wasm__
  }

  command.Clear();
  session.ConvertNext(&command);
  {
    const commands::Output &output = command.output();

    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(output.all_candidate_words().focused_index(), 1);
    EXPECT_EQ(output.all_candidate_words().category(), commands::CONVERSION);
#if defined(__linux__) || defined(__wasm__)
    // Cascading window is not supported on Linux, so the size of
    // candidate words is different from other platform.
    // TODO(komatsu): Modify the client for Linux to explicitly change
    // the preference rather than relying on the exceptional default value.
    // [ "あいうえお", "アイウエオ", "アイウエオ" (t13n), "あいうえお" (t13n),
    //   "aiueo" (t13n), "AIUEO" (t13n), "Aieuo" (t13n),
    //   "ａｉｕｅｏ"  (t13n), "ＡＩＵＥＯ" (t13n), "Ａｉｅｕｏ" (t13n),
    //   "ｱｲｳｴｵ" (t13n) ]
    EXPECT_EQ(output.all_candidate_words().candidates_size(), 9);
#else   // __linux__ || __wasm__
    // [ "あいうえお", "アイウエオ",
    //   "aiueo" (t13n), "AIUEO" (t13n), "Aieuo" (t13n),
    //   "ａｉｕｅｏ"  (t13n), "ＡＩＵＥＯ" (t13n), "Ａｉｅｕｏ" (t13n),
    //   "ｱｲｳｴｵ" (t13n) ]
    EXPECT_EQ(output.all_candidate_words().candidates_size(), 11);
#endif  // __linux__ || __wasm__
  }
}

TEST_F(SessionTest, UndoForComposition) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Enable zero query suggest.
  commands::Request request;
  SetupZeroQuerySuggestionReady(true, &session, &request, converter.get());

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  commands::Command command;
  Segments segments;
  Segments empty_segments;

  {  // Undo for CommitFirstSuggestion
    SetAiueo(&segments);
    EXPECT_CALL(*converter, StartPrediction(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
    InsertCharacterChars("ai", &session, &command);
    EXPECT_EQ(GetComposition(command), "あい");

    command.Clear();
    // EXPECT_CALL(*converter, FinishConversion(_, _))
    //    .WillOnce(SetArgPointee<1>(empty_segments));
    session.CommitFirstSuggestion(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_RESULT("あいうえお", command);
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);

    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -5);
    EXPECT_EQ(command.output().deletion_range().length(), 5);
    EXPECT_SINGLE_SEGMENT("あい", command);
    EXPECT_EQ(command.output().candidate_window().size(), 2);
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
  }
}

TEST_F(SessionTest, RequestUndo) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);

  // It is OK not to check ImeContext::DIRECT because you cannot
  // assign any key event to Undo command in DIRECT mode.
  // See "session/keymap_interface.h".

  InitSessionToPrecomposition(&session);
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session))
      << "When the UNDO context is empty and the context state is "
         "ImeContext::PRECOMPOSITION, UNDO command should be "
         "ignored. See b/5553298.";

  InitSessionToPrecomposition(&session);
  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(TryUndoAndAssertSuccess(&session));

  InitSessionToPrecomposition(&session);
  SetUndoContext(&session, converter.get());
  session.context_->set_state(ImeContext::COMPOSITION);
  EXPECT_TRUE(TryUndoAndAssertSuccess(&session));

  InitSessionToPrecomposition(&session);
  SetUndoContext(&session, converter.get());
  session.context_->set_state(ImeContext::CONVERSION);
  EXPECT_TRUE(TryUndoAndAssertSuccess(&session));
}

TEST_F(SessionTest, UndoForSingleSegment) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  commands::Command command;
  Segments segments;
  config::Config config;

  {  // Create segments
    InsertCharacterChars("aiueo", &session, &command);
    SetAiueo(&segments);
    // Don't use FillT13Ns(). It makes platform dependent segments.
    // TODO(hsumita): Makes FillT13Ns() independent from platforms.
    Segment::Candidate *candidate;
    candidate = segments.mutable_segment(0)->add_candidate();
    candidate->value = "aiueo";
    candidate = segments.mutable_segment(0)->add_candidate();
    candidate->value = "AIUEO";
  }

  {  // Undo after commitment of composition
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_PREEDIT("あいうえお", command);

    EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    session.Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_RESULT("あいうえお", command);

    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -5);
    EXPECT_EQ(command.output().deletion_range().length(), 5);
    EXPECT_PREEDIT("あいうえお", command);

    // Undo twice - do nothing and keep the previous status.
    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_deletion_range());
    EXPECT_PREEDIT("あいうえお", command);
  }

  {  // Undo after commitment of conversion
    command.Clear();
    session.ConvertNext(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_PREEDIT("アイウエオ", command);

    EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    session.Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_RESULT("アイウエオ", command);

    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -5);
    EXPECT_EQ(command.output().deletion_range().length(), 5);
    EXPECT_PREEDIT("アイウエオ", command);

    // Undo twice - do nothing and keep the previous status.
    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_deletion_range());
    EXPECT_PREEDIT("アイウエオ", command);
  }

  {  // Undo after commitment of conversion with Ctrl-Backspace.
    command.Clear();
    session.ConvertNext(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_PREEDIT("aiueo", command);

    EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    session.Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_RESULT("aiueo", command);

    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -5);
    EXPECT_EQ(command.output().deletion_range().length(), 5);
    EXPECT_PREEDIT("aiueo", command);
  }

  {
    // If capability does not support DELETE_PRECEDIGN_TEXT, Undo is not
    // performed.
    EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    session.Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_RESULT("aiueo", command);

    // Reset capability
    capability.Clear();
    session.set_client_capability(capability);

    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_deletion_range());
    EXPECT_FALSE(command.output().has_preedit());
  }
}

TEST_F(SessionTest, ClearUndoContextByKeyEventIssue5529702) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  SetUndoContext(&session, converter.get());

  commands::Command command;

  // Modifier key event does not clear undo context.
  SendKey("Shift", &session, &command);

  // Ctrl+BS should be consumed as UNDO.
  SetSendKeyCommand("Ctrl Backspace", &command);
  command.mutable_input()->mutable_config()->set_session_keymap(
      config::Config::MSIME);
  session.TestSendKey(&command);
  EXPECT_TRUE(command.output().consumed());

  // Any other (test) send key event clears undo context.
  TestSendKey("LEFT", &session, &command);
  EXPECT_FALSE(command.output().consumed());

  // Undo context is just cleared. Ctrl+BS should not be consumed b/5553298.
  SetSendKeyCommand("Ctrl Backspace", &command);
  command.mutable_input()->mutable_config()->set_session_keymap(
      config::Config::MSIME);
  session.TestSendKey(&command);
  EXPECT_FALSE(command.output().consumed());
}

TEST_F(SessionTest, UndoForMultipleSegments) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  commands::Command command;
  Segments segments;

  {  // Create segments
    InsertCharacterChars("key1key2key3", &session, &command);

    Segment::Candidate *candidate;
    Segment *segment;

    segment = segments.add_segment();
    segment->set_key("key1");
    candidate = segment->add_candidate();
    candidate->value = "cand1-1";
    candidate = segment->add_candidate();
    candidate->value = "cand1-2";

    segment = segments.add_segment();
    segment->set_key("key2");
    candidate = segment->add_candidate();
    candidate->value = "cand2-1";
    candidate = segment->add_candidate();
    candidate->value = "cand2-2";

    segment = segments.add_segment();
    segment->set_key("key3");
    candidate = segment->add_candidate();
    candidate->value = "cand3-1";
    candidate = segment->add_candidate();
    candidate->value = "cand3-2";
  }

  {  // Undo for CommitCandidate
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_PREEDIT("cand1-1cand2-1cand3-1", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

    // CommitSegments() sets the first segment SUBMITTED.
    segments.mutable_segment(0)->set_segment_type(Segment::SUBMITTED);
    segments.mutable_segment(1)->set_segment_type(Segment::FREE);
    segments.mutable_segment(2)->set_segment_type(Segment::FREE);
    EXPECT_CALL(*converter, CommitSegments(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    command.mutable_input()->mutable_command()->set_id(1);
    session.CommitCandidate(&command);
    EXPECT_PREEDIT("cand2-1cand3-1", command);
    EXPECT_RESULT("cand1-2", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -7);
    EXPECT_EQ(command.output().deletion_range().length(), 7);
    EXPECT_PREEDIT("cand1-1cand2-1cand3-1", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

    // Move to second segment and do the same thing.
    segments.mutable_segment(0)->set_segment_type(Segment::SUBMITTED);
    segments.mutable_segment(1)->set_segment_type(Segment::SUBMITTED);
    segments.mutable_segment(2)->set_segment_type(Segment::FREE);
    EXPECT_CALL(*converter, CommitSegments(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    session.SegmentFocusRight(&command);
    command.Clear();
    command.mutable_input()->mutable_command()->set_id(1);
    session.CommitCandidate(&command);
    // "cand2-2" is focused
    EXPECT_PREEDIT("cand3-1", command);
    EXPECT_RESULT("cand1-1cand2-2", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -14);
    EXPECT_EQ(command.output().deletion_range().length(), 14);
    // "cand2-1" is focused
    EXPECT_PREEDIT("cand1-1cand2-1cand3-1", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);
  }
  {  // Undo for CommitSegment
    segments.mutable_segment(0)->set_segment_type(Segment::FREE);
    segments.mutable_segment(1)->set_segment_type(Segment::FREE);
    segments.mutable_segment(2)->set_segment_type(Segment::FREE);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_PREEDIT("cand1-1cand2-1cand3-1", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

    command.Clear();
    session.ConvertNext(&command);
    EXPECT_EQ(GetComposition(command), "cand1-2cand2-1cand3-1");
    command.Clear();
    segments.mutable_segment(0)->set_segment_type(Segment::SUBMITTED);
    segments.mutable_segment(1)->set_segment_type(Segment::FREE);
    segments.mutable_segment(2)->set_segment_type(Segment::FREE);
    EXPECT_CALL(*converter, CommitSegments(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    session.CommitSegment(&command);
    EXPECT_PREEDIT("cand2-1cand3-1", command);
    EXPECT_RESULT("cand1-2", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -7);
    EXPECT_EQ(command.output().deletion_range().length(), 7);
    EXPECT_PREEDIT("cand1-2cand2-1cand3-1", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

    // Move to third segment and do the same thing.
    command.Clear();
    session.SegmentFocusRight(&command);
    command.Clear();
    session.SegmentFocusRight(&command);
    command.Clear();
    session.ConvertNext(&command);
    EXPECT_PREEDIT("cand1-1cand2-1cand3-2", command);
    command.Clear();
    segments.mutable_segment(0)->set_segment_type(Segment::SUBMITTED);
    segments.mutable_segment(1)->set_segment_type(Segment::FREE);
    segments.mutable_segment(2)->set_segment_type(Segment::FREE);
    EXPECT_CALL(*converter, CommitSegments(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    // "cand3-2" is focused, but once CommitSegment() runs, which commits
    // the first segment (Ctrl + N on MS-IME),
    // the last segment goes back to the initial candidate ("cand3-1").
    session.CommitSegment(&command);
    EXPECT_PREEDIT("cand2-1cand3-1", command);
    EXPECT_RESULT("cand1-1", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -7);
    EXPECT_EQ(command.output().deletion_range().length(), 7);
    // "cand3-2" is focused
    EXPECT_PREEDIT("cand1-1cand2-1cand3-2", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);
  }
}

TEST_F(SessionTest, UndoForCommittedBracketPairIssue284235847) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  commands::Command command;
  Segments segments;

  {  // Create segments
    InsertCharacterChars("あかっこ", &session, &command);

    Segment::Candidate *candidate;
    Segment *segment;

    segment = segments.add_segment();
    segment->set_key("あ");
    candidate = segment->add_candidate();
    candidate->value = "あ";

    segment = segments.add_segment();
    segment->set_key("かっこ");
    candidate = segment->add_candidate();
    candidate->value = "かっこ";
    candidate = segment->add_candidate();
    candidate->value = "（）";
  }

  {  // Commit 1st and 2nd segment
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.Convert(&command);
    // -> preedit: "あかっこ", result: empty, state: CONVERSION

    // CommitSegments() sets the first segment SUBMITTED.
    segments.mutable_segment(0)->set_segment_type(Segment::SUBMITTED);
    segments.mutable_segment(1)->set_segment_type(Segment::FREE);
    EXPECT_CALL(*converter, CommitSegments(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    command.mutable_input()->mutable_command()->set_id(1);
    session.CommitCandidate(&command);
    // -> preedit: "かっこ", result: "あ", state: CONVERSION

    // Move to second segment and do the same thing.
    command.Clear();
    session.SegmentFocusRight(&command);
    command.Clear();
    command.mutable_input()->mutable_command()->set_id(1);
    session.CommitCandidate(&command);
    // -> preedit: empty, result: "（）", state: PRECOMPOSITION
  }

  {  // Undo for 2nd segment (ie. bracket pair)
    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -1);
    EXPECT_EQ(command.output().deletion_range().length(), 2);
    EXPECT_PREEDIT("かっこ", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);
  }
}

TEST_F(SessionTest, MultipleUndo) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  commands::Command command;
  Segments segments;

  {  // Create segments
    InsertCharacterChars("key1key2key3", &session, &command);

    Segment::Candidate *candidate;
    Segment *segment;

    segment = segments.add_segment();
    segment->set_key("key1");
    candidate = segment->add_candidate();
    candidate->value = "cand1-1";
    candidate = segment->add_candidate();
    candidate->value = "cand1-2";

    segment = segments.add_segment();
    segment->set_key("key2");
    candidate = segment->add_candidate();
    candidate->value = "cand2-1";
    candidate = segment->add_candidate();
    candidate->value = "cand2-2";

    segment = segments.add_segment();
    segment->set_key("key3");
    candidate = segment->add_candidate();
    candidate->value = "cand3-1";
    candidate = segment->add_candidate();
    candidate->value = "cand3-2";
  }

  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  command.Clear();
  session.Convert(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("cand1-1cand2-1cand3-1", command);
  EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

  // Commit 1st and 2nd segment
  segments.mutable_segment(0)->set_segment_type(Segment::SUBMITTED);
  segments.mutable_segment(1)->set_segment_type(Segment::FREE);
  segments.mutable_segment(2)->set_segment_type(Segment::FREE);
  EXPECT_CALL(*converter, CommitSegments(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
  command.Clear();
  command.mutable_input()->mutable_command()->set_id(1);
  session.CommitCandidate(&command);
  EXPECT_PREEDIT("cand2-1cand3-1", command);
  EXPECT_RESULT("cand1-2", command);
  segments.mutable_segment(0)->set_segment_type(Segment::SUBMITTED);
  segments.mutable_segment(1)->set_segment_type(Segment::SUBMITTED);
  segments.mutable_segment(2)->set_segment_type(Segment::FREE);
  EXPECT_CALL(*converter, CommitSegments(_, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
  command.Clear();
  command.mutable_input()->mutable_command()->set_id(1);
  session.CommitCandidate(&command);
  EXPECT_PREEDIT("cand3-1", command);
  EXPECT_RESULT("cand2-2", command);
  EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

  // Undo to revive 2nd commit.
  command.Clear();
  session.Undo(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(command.output().has_deletion_range());
  EXPECT_EQ(command.output().deletion_range().offset(), -7);
  EXPECT_EQ(command.output().deletion_range().length(), 7);
  EXPECT_PREEDIT("cand2-1cand3-1", command);
  EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

  // Try undoing against the 1st commit.
  command.Clear();
  session.Undo(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(command.output().has_deletion_range());
  EXPECT_EQ(command.output().deletion_range().offset(), -7);
  EXPECT_EQ(command.output().deletion_range().length(), 7);
  EXPECT_PREEDIT("cand1-1cand2-1cand3-1", command);
  EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

  // No further undo available.
  command.Clear();
  session.Undo(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_deletion_range());
}

TEST_F(SessionTest, UndoOrRewindUndo) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  // Commit twice.
  for (size_t i = 0; i < 2; ++i) {
    commands::Command command;
    Segments segments;
    {  // Create segments
      InsertCharacterChars("aiueo", &session, &command);
      SetAiueo(&segments);
      Segment::Candidate *candidate;
      candidate = segments.mutable_segment(0)->add_candidate();
      candidate->value = "aiueo";
      candidate = segments.mutable_segment(0)->add_candidate();
      candidate->value = "AIUEO";
    }
    {
      EXPECT_CALL(*converter, StartConversion(_, _))
          .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
      command.Clear();
      session.Convert(&command);
      EXPECT_FALSE(command.output().has_result());
      EXPECT_PREEDIT("あいうえお", command);

      EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
          .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
      command.Clear();
      session.Commit(&command);
      EXPECT_FALSE(command.output().has_preedit());
      EXPECT_RESULT("あいうえお", command);
    }
  }
  // Try UndoOrRewind twice.
  // Second trial should not consume the event. Echoback is expected.
  commands::Command command;
  command.Clear();
  session.UndoOrRewind(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("あいうえお", command);
  EXPECT_TRUE(command.output().has_deletion_range());
  command.Clear();
  session.UndoOrRewind(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_deletion_range());
  EXPECT_FALSE(command.output().consumed());
}

TEST_F(SessionTest, UndoOrRewindRewind) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session, *mobile_request_);

  {  // Commit something. It's expected that Undo is not triggered later.
    commands::Command command;
    Segments segments;
    InsertCharacterChars("aiueo", &session, &command);
    SetAiueo(&segments);
    Segment::Candidate *candidate;
    candidate = segments.mutable_segment(0)->add_candidate();
    candidate->value = "aiueo";
    candidate = segments.mutable_segment(0)->add_candidate();
    candidate->value = "AIUEO";

    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_PREEDIT("あいうえお", command);

    EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    session.Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_RESULT("あいうえお", command);
  }

  Segments segments;
  {
    Segment *segment;
    segment = segments.add_segment();
    AddCandidate("e", "e", segment);
    AddCandidate("e", "E", segment);
  }
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

  commands::Command command;
  InsertCharacterChars("11111", &session, &command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("お", command);
  EXPECT_FALSE(command.output().has_deletion_range());
  EXPECT_TRUE(command.output().has_all_candidate_words());

  command.Clear();
  session.UndoOrRewind(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("え", command);
  EXPECT_FALSE(command.output().has_deletion_range());
  EXPECT_TRUE(command.output().has_all_candidate_words());
}

TEST_F(SessionTest, StopKeyToggling) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session, *mobile_request_);

  Segments segments;
  {
    Segment *segment;
    segment = segments.add_segment();
    AddCandidate("placeholder", "PLACEHOLDER", segment);
  }

  // StartPrediction() is called twice in InsertChararcterChars(),
  // but it is not called in StopKeyToggling().
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .Times(2)
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

  commands::Command command;
  InsertCharacterChars("1", &session, &command);
  EXPECT_PREEDIT("あ", command);
  EXPECT_EQ(command.output().all_candidate_words().candidates_size(), 1);

  command.Clear();
  session.StopKeyToggling(&command);
  EXPECT_EQ(command.output().all_candidate_words().candidates_size(), 1);

  command.Clear();
  InsertCharacterChars("1", &session, &command);
  EXPECT_PREEDIT("ああ", command);
  EXPECT_EQ(command.output().all_candidate_words().candidates_size(), 1);
}

TEST_F(SessionTest, CommitRawText) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {  // From composition mode.
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    InsertCharacterChars("abc", &session, &command);
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);

    Segments segments;
    {  // Initialize segments.
      Segment *segment = segments.add_segment();
      segment->set_key("あｂｃ");
      segment->add_candidate()->value = "あべし";
    }

    command.Clear();
    SetSendCommandCommand(commands::SessionCommand::COMMIT_RAW_TEXT, &command);
    session.SendCommand(&command);
    EXPECT_RESULT_AND_KEY("abc", "abc", command);
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
    Mock::VerifyAndClearExpectations(converter.get());
  }
  {  // From conversion mode.
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    InsertCharacterChars("abc", &session, &command);
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);

    Segments segments;
    {  // Initialize segments.
      Segment *segment = segments.add_segment();
      segment->set_key("あｂｃ");
      segment->add_candidate()->value = "あべし";
    }

    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.Convert(&command);
    EXPECT_PREEDIT("あべし", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

    command.Clear();
    SetSendCommandCommand(commands::SessionCommand::COMMIT_RAW_TEXT, &command);
    session.SendCommand(&command);
    EXPECT_RESULT_AND_KEY("abc", "abc", command);
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
    Mock::VerifyAndClearExpectations(converter.get());
  }
}

TEST_F(SessionTest, CommitRawTextKanaInput) {
  auto table = std::make_shared<composer::Table>();
  table->AddRule("す゛", "ず", "");

  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  session.get_internal_composer_only_for_unittest()->SetTable(table);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  SetSendKeyCommand("m", &command);
  command.mutable_input()->mutable_key()->set_key_string("も");
  session.SendKey(&command);

  SetSendKeyCommand("r", &command);
  command.mutable_input()->mutable_key()->set_key_string("す");
  session.SendKey(&command);

  SetSendKeyCommand("@", &command);
  command.mutable_input()->mutable_key()->set_key_string("゛");
  session.SendKey(&command);

  SetSendKeyCommand("h", &command);
  command.mutable_input()->mutable_key()->set_key_string("く");
  session.SendKey(&command);

  SetSendKeyCommand("!", &command);
  command.mutable_input()->mutable_key()->set_key_string("!");
  session.SendKey(&command);

  EXPECT_EQ(command.output().preedit().segment(0).value(), "もずく！");

  command.Clear();
  SetSendCommandCommand(commands::SessionCommand::COMMIT_RAW_TEXT, &command);
  session.SendCommand(&command);
  EXPECT_RESULT_AND_KEY("mr@h!", "mr@h!", command);
  EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
}

TEST_F(SessionTest, ConvertNextPagePrevPage) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  commands::Command command;

  InitSessionToPrecomposition(&session);

  // Should be ignored in precomposition state.
  {
    command.Clear();
    command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
    command.mutable_input()->mutable_command()->set_type(
        commands::SessionCommand::CONVERT_NEXT_PAGE);
    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_TRUE(command.output().consumed());

    command.Clear();
    command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
    command.mutable_input()->mutable_command()->set_type(
        commands::SessionCommand::CONVERT_PREV_PAGE);
    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_TRUE(command.output().consumed());
  }

  InsertCharacterChars("aiueo", &session, &command);
  EXPECT_PREEDIT("あいうえお", command);

  // Should be ignored in composition state.
  {
    command.Clear();
    command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
    command.mutable_input()->mutable_command()->set_type(
        commands::SessionCommand::CONVERT_NEXT_PAGE);
    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_PREEDIT("あいうえお", command) << "should do nothing";

    command.Clear();
    command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
    command.mutable_input()->mutable_command()->set_type(
        commands::SessionCommand::CONVERT_PREV_PAGE);
    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_PREEDIT("あいうえお", command) << "should do nothing";
  }

  // Generate sequential candidates as follows.
  //   "page0-cand0"
  //   "page0-cand1"
  //   ...
  //   "page0-cand8"
  //   "page1-cand0"
  //   ...
  //   "page1-cand8"
  //   "page2-cand0"
  //   ...
  //   "page2-cand8"
  {
    Segments segments;
    Segment *segment = nullptr;
    segment = segments.add_segment();
    segment->set_key("あいうえお");
    for (int page_index = 0; page_index < 3; ++page_index) {
      for (int cand_index = 0; cand_index < 9; ++cand_index) {
        segment->add_candidate()->value =
            absl::StrFormat("page%d-cand%d", page_index, cand_index);
      }
    }
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  // Make sure the selected candidate changes as follows.
  //                              -> Convert
  //  -> "page0-cand0" -> SendCommand/CONVERT_NEXT_PAGE
  //  -> "page1-cand0" -> SendCommand/CONVERT_PREV_PAGE
  //  -> "page0-cand0" -> SendCommand/CONVERT_PREV_PAGE
  //  -> "page2-cand0"

  command.Clear();
  ASSERT_TRUE(session.Convert(&command));
  EXPECT_PREEDIT("page0-cand0", command);

  command.Clear();
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::CONVERT_NEXT_PAGE);
  ASSERT_TRUE(session.SendCommand(&command));
  EXPECT_PREEDIT("page1-cand0", command);

  command.Clear();
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::CONVERT_PREV_PAGE);
  ASSERT_TRUE(session.SendCommand(&command));
  EXPECT_PREEDIT("page0-cand0", command);

  command.Clear();
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::CONVERT_PREV_PAGE);
  ASSERT_TRUE(session.SendCommand(&command));
  EXPECT_PREEDIT("page2-cand0", command);
}

TEST_F(SessionTest, NeedlessClearUndoContext) {
  // This is a unittest against http://b/3423910.
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);
  commands::Command command;

  {  // Conversion -> Send Shift -> Undo
    Segments segments;
    InsertCharacterChars("aiueo", &session, &command);
    const ConversionRequest request = CreateConversionRequest(session);
    SetAiueo(&segments);
    FillT13Ns(request, &segments);

    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_PREEDIT("あいうえお", command);

    EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    session.Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_RESULT("あいうえお", command);

    SendKey("Shift", &session, &command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());

    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -5);
    EXPECT_EQ(command.output().deletion_range().length(), 5);
    EXPECT_PREEDIT("あいうえお", command);
  }

  {  // Type "aiueo" -> Convert -> Type "a" -> Escape -> Undo
    Segments segments;
    InsertCharacterChars("aiueo", &session, &command);
    const ConversionRequest request = CreateConversionRequest(session);
    SetAiueo(&segments);
    FillT13Ns(request, &segments);

    command.Clear();
    session.Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_PREEDIT("あいうえお", command);

    SendKey("a", &session, &command);
    EXPECT_RESULT("あいうえお", command);
    EXPECT_SINGLE_SEGMENT("あ", command);

    SendKey("Escape", &session, &command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());

    command.Clear();
    session.Undo(&command);

    // Undo did nothing because the undo stack emptied by Escape event,
    // which modified the composition.
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_deletion_range());
    EXPECT_FALSE(command.output().has_result());
  }
}

TEST_F(SessionTest, ClearUndoContextAfterDirectInputAfterConversion) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Prepare Numpad
  config::Config config;
  config.set_numpad_character_form(config::Config::NUMPAD_DIRECT_INPUT);
  // Update KeyEventTransformer
  session.SetConfig(config);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);
  commands::Command command;

  // Cleate segments
  Segments segments;
  InsertCharacterChars("aiueo", &session, &command);
  const ConversionRequest request = CreateConversionRequest(session);
  SetAiueo(&segments);
  FillT13Ns(request, &segments);

  // Convert
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  command.Clear();
  session.Convert(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("あいうえお", command);
  // Direct input
  SendKey("Numpad0", &session, &command);
  EXPECT_TRUE(GetComposition(command).empty());
  EXPECT_RESULT("あいうえお0", command);

  // Undo - Do NOT nothing
  command.Clear();
  session.Undo(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_deletion_range());
  EXPECT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, TemporaryInputModeAfterUndo) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // This is a unittest against http://b/3423599.
  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);
  commands::Command command;

  // Shift + Ascii triggers temporary input mode switch.
  SendKey("A", &session, &command);
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
  SendKey("Enter", &session, &command);
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);

  // Undo and keep temporary input mode correct
  command.Clear();
  session.Undo(&command);
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("A", command);
  SendKey("Enter", &session, &command);
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);

  // Undo and input additional "A" with temporary input mode.
  command.Clear();
  session.Undo(&command);
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
  SendKey("A", &session, &command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("AA", command);
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

  // Input additional "a" with original input mode.
  SendKey("a", &session, &command);
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("AAあ", command);

  // Submit and Undo
  SendKey("Enter", &session, &command);
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  command.Clear();
  session.Undo(&command);
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("AAあ", command);

  // Input additional "Aa"
  SendKey("A", &session, &command);
  SendKey("a", &session, &command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("AAあAa", command);
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

  // Submit and Undo
  SendKey("Enter", &session, &command);
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  command.Clear();
  session.Undo(&command);
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("AAあAa", command);
}

TEST_F(SessionTest, DCHECKFailureAfterUndo) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // This is a unittest against http://b/3437358.
  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);
  commands::Command command;

  InsertCharacterChars("abe", &session, &command);
  command.Clear();
  session.Commit(&command);
  command.Clear();
  session.Undo(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("あべ", command);

  InsertCharacterChars("s", &session, &command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("あべｓ", command);

  InsertCharacterChars("h", &session, &command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("あべｓｈ", command);

  InsertCharacterChars("i", &session, &command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_PREEDIT("あべし", command);
}

TEST_F(SessionTest, ConvertToFullOrHalfAlphanumericAfterUndo) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // This is a unittest against http://b/3423592.
  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  Segments segments;
  SetAiueo(&segments);
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);

  {  // ConvertToHalfASCII
    commands::Command command;
    InsertCharacterChars("aiueo", &session, &command);

    SendKey("Enter", &session, &command);
    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    ASSERT_TRUE(command.output().has_preedit());
    EXPECT_EQ(GetComposition(command), "あいうえお");

    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.ConvertToHalfASCII(&command);
    EXPECT_FALSE(command.output().has_result());
    ASSERT_TRUE(command.output().has_preedit());
    EXPECT_EQ(GetComposition(command), "aiueo");
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {  // ConvertToFullASCII
    commands::Command command;
    InsertCharacterChars("aiueo", &session, &command);

    SendKey("Enter", &session, &command);
    command.Clear();
    session.Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    ASSERT_TRUE(command.output().has_preedit());
    EXPECT_EQ(GetComposition(command), "あいうえお");

    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.ConvertToFullASCII(&command);
    EXPECT_FALSE(command.output().has_result());
    ASSERT_TRUE(command.output().has_preedit());
    EXPECT_EQ(GetComposition(command), "ａｉｕｅｏ");
    Mock::VerifyAndClearExpectations(converter.get());
  }
}

TEST_F(SessionTest, ComposeVoicedSoundMarkAfterUndoIssue5369632) {
  // This is a unittest against http://b/5369632.
  config::Config config;
  config.set_preedit_method(config::Config::KANA);

  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  session.SetConfig(config);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  commands::Command command;

  InsertCharacterCodeAndString('a', "ち", &session, &command);
  EXPECT_EQ(GetComposition(command), "ち");

  SendKey("Enter", &session, &command);
  command.Clear();
  session.Undo(&command);

  EXPECT_FALSE(command.output().has_result());
  ASSERT_TRUE(command.output().has_preedit());
  EXPECT_EQ(GetComposition(command), "ち");

  InsertCharacterCodeAndString('@', "゛", &session, &command);
  EXPECT_FALSE(command.output().has_result());
  ASSERT_TRUE(command.output().has_preedit());
  EXPECT_EQ(GetComposition(command), "ぢ");
}

TEST_F(SessionTest, SpaceOnAlphanumeric) {
  commands::Request request;
  commands::Command command;

  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  {
    request.set_space_on_alphanumeric(commands::Request::COMMIT);

    Session session(engine);
    InitSessionToPrecomposition(&session, request);

    SendKey("A", &session, &command);
    EXPECT_EQ(GetComposition(command), "A");

    SendKey("Space", &session, &command);
    EXPECT_RESULT("A ", command);
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {
    request.set_space_on_alphanumeric(
        commands::Request::SPACE_OR_CONVERT_COMMITTING_COMPOSITION);

    Session session(engine);
    InitSessionToPrecomposition(&session, request);

    SendKey("A", &session, &command);
    EXPECT_EQ(GetComposition(command), "A");

    SendKey("Space", &session, &command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(GetComposition(command), "A ");

    SendKey("a", &session, &command);
    EXPECT_RESULT("A ", command);
    EXPECT_EQ(GetComposition(command), "あ");
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {
    request.set_space_on_alphanumeric(
        commands::Request::SPACE_OR_CONVERT_KEEPING_COMPOSITION);

    Session session(engine);
    InitSessionToPrecomposition(&session, request);

    SendKey("A", &session, &command);
    EXPECT_EQ(GetComposition(command), "A");

    SendKey("Space", &session, &command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(GetComposition(command), "A ");

    SendKey("a", &session, &command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(GetComposition(command), "A a");
    Mock::VerifyAndClearExpectations(converter.get());
  }
}

TEST_F(SessionTest, Issue1805239) {
  // This is a unittest against http://b/1805239.
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("watasinonamae", &session, &command);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("わたしの");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "私の";
  candidate = segment->add_candidate();
  candidate->value = "渡しの";
  segment = segments.add_segment();
  segment->set_key("名前");
  candidate = segment->add_candidate();
  candidate->value = "なまえ";
  candidate = segment->add_candidate();
  candidate->value = "ナマエ";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  EXPECT_TRUE(command.output().has_candidate_window());

  SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
  EXPECT_FALSE(command.output().has_candidate_window());

  SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
  EXPECT_FALSE(command.output().has_candidate_window());

  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  EXPECT_TRUE(command.output().has_candidate_window());

  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  EXPECT_TRUE(command.output().has_candidate_window());

  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  EXPECT_TRUE(command.output().has_candidate_window());

  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  EXPECT_TRUE(command.output().has_candidate_window());
}

TEST_F(SessionTest, Issue1816861) {
  // This is a unittest against http://b/1816861
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("kamabokonoinbou", &session, &command);
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("かまぼこの");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "かまぼこの";
  candidate = segment->add_candidate();
  candidate->value = "カマボコの";
  segment = segments.add_segment();
  segment->set_key("いんぼう");
  candidate = segment->add_candidate();
  candidate->value = "陰謀";
  candidate = segment->add_candidate();
  candidate->value = "印房";

  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, &session, &command);
  SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
  SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
  SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
  SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, &session, &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, &session, &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, &session, &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, &session, &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, &session, &command);

  segments.Clear();
  segment = segments.add_segment();
  segment->set_key("いんぼう");
  candidate = segment->add_candidate();
  candidate->value = "陰謀";
  candidate = segment->add_candidate();
  candidate->value = "陰謀論";
  candidate = segment->add_candidate();
  candidate->value = "陰謀説";

  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  SendSpecialKey(commands::KeyEvent::TAB, &session, &command);
}

TEST_F(SessionTest, T13NWithResegmentation) {
  // This is a unittest against http://b/3272827
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("kamabokonoinbou", &session, &command);

  {
    Segments segments;
    Segment *segment;
    segment = segments.add_segment();
    segment->set_key("かまぼこの");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "かまぼこの";
    candidate = segment->add_candidate();
    candidate->value = "カマボコの";

    segment = segments.add_segment();
    segment->set_key("いんぼう");
    candidate = segment->add_candidate();
    candidate->value = "陰謀";
    candidate = segment->add_candidate();
    candidate->value = "印房";
    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("かまぼこの");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "かまぼこの";
    candidate = segment->add_candidate();
    candidate->value = "カマボコの";

    segment = segments.add_segment();
    segment->set_key("いんぼ");
    candidate = segment->add_candidate();
    candidate->value = "いんぼ";
    candidate = segment->add_candidate();
    candidate->value = "インボ";

    segment = segments.add_segment();
    segment->set_key("う");
    candidate = segment->add_candidate();
    candidate->value = "ウ";
    candidate = segment->add_candidate();
    candidate->value = "卯";

    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, ResizeSegment(_, _, _, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
  }

  // Start conversion
  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  // Select second segment
  SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
  // Shrink segment
  SendKey("Shift left", &session, &command);
  // Convert to T13N (Half katakana)
  SendKey("F8", &session, &command);

  EXPECT_EQ(command.output().preedit().segment(1).value(), "ｲﾝﾎﾞ");
}

TEST_F(SessionTest, Shortcut) {
  const config::Config::SelectionShortcut kDataShortcut[] = {
      config::Config::NO_SHORTCUT,
      config::Config::SHORTCUT_123456789,
      config::Config::SHORTCUT_ASDFGHJKL,
  };
  const std::string kDataExpected[][2] = {
      {"", ""},
      {"1", "2"},
      {"a", "s"},
  };
  for (size_t i = 0; i < std::size(kDataShortcut); ++i) {
    config::Config::SelectionShortcut shortcut = kDataShortcut[i];
    const std::string *expected = kDataExpected[i];

    config::Config config;
    config.set_selection_shortcut(shortcut);

    ;
    MockEngine engine;
    std::shared_ptr<MockConverter> converter =
        CreateEngineConverterMock(&engine);

    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);

    Segments segments;
    SetAiueo(&segments);
    const ImeContext &context = session.context();
    const ConversionRequest request =
        ConversionRequestBuilder()
            .SetComposer(context.composer())
            .SetRequestView(context.GetRequest())
            .SetContextView(context.client_context())
            .SetConfigView(context.GetConfig())
            .Build();
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    commands::Command command;
    InsertCharacterChars("aiueo", &session, &command);

    command.Clear();
    session.Convert(&command);

    command.Clear();
    // Convert next
    SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
    ASSERT_TRUE(command.output().has_candidate_window());
    const commands::CandidateWindow &candidate_window =
        command.output().candidate_window();
    EXPECT_EQ(candidate_window.candidate(0).annotation().shortcut(),
              expected[0]);
    EXPECT_EQ(candidate_window.candidate(1).annotation().shortcut(),
              expected[1]);
  }
}

TEST_F(SessionTest, ShortcutWithCapsLockIssue5655743) {
  config::Config config;
  config.set_selection_shortcut(config::Config::SHORTCUT_ASDFGHJKL);

  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  session.SetConfig(config);
  InitSessionToPrecomposition(&session);

  Segments segments;
  SetAiueo(&segments);
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  commands::Command command;
  InsertCharacterChars("aiueo", &session, &command);

  command.Clear();
  session.Convert(&command);

  command.Clear();
  // Convert next
  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  ASSERT_TRUE(command.output().has_candidate_window());

  const commands::CandidateWindow &candidate_window =
      command.output().candidate_window();
  EXPECT_EQ(candidate_window.candidate(0).annotation().shortcut(), "a");
  EXPECT_EQ(candidate_window.candidate(1).annotation().shortcut(), "s");

  // Select the second candidate by 's' key when the CapsLock is enabled.
  // Note that "CAPS S" means that 's' key is pressed w/o shift key.
  // See the description in command.proto.
  EXPECT_TRUE(SendKey("CAPS S", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(GetComposition(command), "アイウエオ");
}

TEST_F(SessionTest, ShortcutFromVK) {
  config::Config config;
  config.set_selection_shortcut(config::Config::SHORTCUT_123456789);
  Request client_request;
  client_request.set_special_romanji_table(Request::QWERTY_MOBILE_TO_HIRAGANA);

  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  session.SetConfig(config);
  InitSessionToPrecomposition(&session, client_request);

  Segments segments;
  SetAiueo(&segments);
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  commands::Command command;
  InsertCharacterChars("aiueo", &session, &command);

  command.Clear();
  session.Convert(&command);

  command.Clear();
  // Convert next
  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  ASSERT_TRUE(command.output().has_candidate_window());

  const commands::CandidateWindow &candidate_window =
      command.output().candidate_window();
  EXPECT_EQ(candidate_window.candidate(0).annotation().shortcut(), "1");
  EXPECT_EQ(candidate_window.candidate(1).annotation().shortcut(), "2");

  // Because the request has a special romaji table (== the event is from VK),
  // "1" must be treated not as shortcut selection but as character insertion.
  EXPECT_TRUE(SendKey("1", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_RESULT("アイウエオ", command);
  EXPECT_EQ(GetComposition(command), "１");
}

TEST_F(SessionTest, NumpadKey) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  config::Config config;
  config.set_numpad_character_form(config::Config::NUMPAD_DIRECT_INPUT);
  session.SetConfig(config);

  // In the Precomposition state, numpad keys should not be consumed.
  EXPECT_TRUE(TestSendKey("Numpad1", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(SendKey("Numpad1", &session, &command));
  EXPECT_FALSE(command.output().consumed());

  EXPECT_TRUE(TestSendKey("Add", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(SendKey("Add", &session, &command));
  EXPECT_FALSE(command.output().consumed());

  EXPECT_TRUE(TestSendKey("Equals", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(SendKey("Equals", &session, &command));
  EXPECT_FALSE(command.output().consumed());

  EXPECT_TRUE(TestSendKey("Separator", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(SendKey("Separator", &session, &command));
  EXPECT_FALSE(command.output().consumed());

  EXPECT_TRUE(GetComposition(command).empty());

  config.set_numpad_character_form(config::Config::NUMPAD_HALF_WIDTH);
  session.SetConfig(config);

  // In the Precomposition state, numpad keys should not be consumed.
  EXPECT_TRUE(TestSendKey("Numpad1", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(SendKey("Numpad1", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(GetComposition(command), "1");

  EXPECT_TRUE(TestSendKey("Add", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(SendKey("Add", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(GetComposition(command), "1+");

  EXPECT_TRUE(TestSendKey("Equals", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(SendKey("Equals", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(GetComposition(command), "1+=");

  EXPECT_TRUE(TestSendKey("Separator", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(SendKey("Separator", &session, &command));
  EXPECT_TRUE(command.output().consumed());

  EXPECT_TRUE(GetComposition(command).empty());

  // "0" should be treated as full-width "０".
  EXPECT_TRUE(TestSendKey("0", &session, &command));
  EXPECT_TRUE(SendKey("0", &session, &command));

  EXPECT_SINGLE_SEGMENT_AND_KEY("０", "０", command);

  // In the Composition state, DIVIDE on the pre-edit should be treated as "/".
  EXPECT_TRUE(TestSendKey("Divide", &session, &command));
  EXPECT_TRUE(SendKey("Divide", &session, &command));

  EXPECT_SINGLE_SEGMENT_AND_KEY("０/", "０/", command);

  // In the Composition state, "Numpad0" should be treated as half-width "0".
  EXPECT_TRUE(SendKey("Numpad0", &session, &command));

  EXPECT_SINGLE_SEGMENT_AND_KEY("０/0", "０/0", command);

  // Separator should be treated as Enter.
  EXPECT_TRUE(TestSendKey("Separator", &session, &command));
  EXPECT_TRUE(SendKey("Separator", &session, &command));

  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT("０/0", command);

  // http://b/2097087
  EXPECT_TRUE(SendKey("0", &session, &command));

  EXPECT_SINGLE_SEGMENT_AND_KEY("０", "０", command);

  EXPECT_TRUE(SendKey("Divide", &session, &command));
  EXPECT_SINGLE_SEGMENT_AND_KEY("０/", "０/", command);

  EXPECT_TRUE(SendKey("Divide", &session, &command));
  EXPECT_SINGLE_SEGMENT_AND_KEY("０//", "０//", command);

  EXPECT_TRUE(SendKey("Subtract", &session, &command));
  EXPECT_TRUE(SendKey("Subtract", &session, &command));
  EXPECT_TRUE(SendKey("Decimal", &session, &command));
  EXPECT_TRUE(SendKey("Decimal", &session, &command));
  EXPECT_SINGLE_SEGMENT_AND_KEY("０//--..", "０//--..", command);
}

TEST_F(SessionTest, KanaSymbols) {
  config::Config config;
  config.set_punctuation_method(config::Config::COMMA_PERIOD);
  config.set_symbol_method(config::Config::CORNER_BRACKET_SLASH);

  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  session.SetConfig(config);
  InitSessionToPrecomposition(&session);

  {
    commands::Command command;
    SetSendKeyCommand("<", &command);
    command.mutable_input()->mutable_key()->set_key_string("、");
    EXPECT_TRUE(session.SendKey(&command));
    EXPECT_EQ(command.input().key().key_code(), static_cast<uint32_t>(','));
    EXPECT_EQ(command.input().key().key_string(), "，");
    EXPECT_EQ(command.output().preedit().segment(0).value(), "，");
  }
  {
    commands::Command command;
    session.EditCancel(&command);
  }
  {
    commands::Command command;
    SetSendKeyCommand("?", &command);
    command.mutable_input()->mutable_key()->set_key_string("・");
    EXPECT_TRUE(session.SendKey(&command));
    EXPECT_EQ(command.input().key().key_code(), static_cast<uint32_t>('/'));
    EXPECT_EQ(command.input().key().key_string(), "／");
    EXPECT_EQ(command.output().preedit().segment(0).value(), "／");
  }
}

TEST_F(SessionTest, InsertCharacterWithShiftKey) {
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {  // Basic behavior
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    EXPECT_TRUE(SendKey("a", &session, &command));
    EXPECT_TRUE(SendKey("A", &session, &command));  // "あA"
    EXPECT_TRUE(SendKey("a", &session, &command));  // "あAa"
    // Shift reverts the input mode to Hiragana.
    EXPECT_TRUE(SendKey("Shift", &session, &command));
    EXPECT_TRUE(SendKey("a", &session, &command));  // "あAaあ"
    // Shift does nothing because the input mode has already been reverted.
    EXPECT_TRUE(SendKey("Shift", &session, &command));
    EXPECT_TRUE(SendKey("a", &session, &command));  // "あAaああ"
    EXPECT_EQ(GetComposition(command), "あAaああ");
  }

  {  // Revert back to the previous input mode.
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    session.InputModeFullKatakana(&command);
    EXPECT_EQ(command.output().mode(), commands::FULL_KATAKANA);
    EXPECT_TRUE(SendKey("a", &session, &command));
    EXPECT_TRUE(SendKey("A", &session, &command));  // "アA"
    EXPECT_TRUE(SendKey("a", &session, &command));  // "アAa"
    // Shift reverts the input mode to Hiragana.
    EXPECT_TRUE(SendKey("Shift", &session, &command));
    EXPECT_TRUE(SendKey("a", &session, &command));  // "アAaア"
    // Shift does nothing because the input mode has already been reverted.
    EXPECT_TRUE(SendKey("Shift", &session, &command));
    EXPECT_TRUE(SendKey("a", &session, &command));  // "アAaアア"
    EXPECT_EQ(GetComposition(command), "アAaアア");
  }
}

TEST_F(SessionTest, ExitTemporaryAlphanumModeAfterCommittingSugesstion1) {
  // This is a unittest against http://b/2977131.
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  EXPECT_TRUE(SendKey("N", &session, &command));
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);  // obsolete
  EXPECT_EQ(command.output().status().mode(), commands::HALF_ASCII);
  // Global mode should be kept as HIRAGANA
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("NFL");
  segment->add_candidate()->value = "NFL";
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  EXPECT_TRUE(session.Convert(&command));
  EXPECT_FALSE(command.output().has_candidate_window());
  EXPECT_FALSE(command.output().candidate_window().has_focused_index());
  EXPECT_EQ(command.output().candidate_window().focused_index(), 0);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);  // obsolete
  EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

  EXPECT_TRUE(SendKey("a", &session, &command));
  EXPECT_FALSE(command.output().has_candidate_window());
  EXPECT_RESULT("NFL", command);
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);  // obsolete
  EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);
}

TEST_F(SessionTest, ExitTemporaryAlphanumModeAfterCommittingSugesstion2) {
  // This is a unittest against http://b/2977131.
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  EXPECT_TRUE(SendKey("N", &session, &command));
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);  // obsolete
  EXPECT_EQ(command.output().status().mode(), commands::HALF_ASCII);
  // Global mode should be kept as HIRAGANA
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("NFL");
  segment->add_candidate()->value = "NFL";
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  EXPECT_TRUE(session.PredictAndConvert(&command));
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_TRUE(command.output().candidate_window().has_focused_index());
  EXPECT_EQ(command.output().candidate_window().focused_index(), 0);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);  // obsolete
  EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(false)));
  EXPECT_TRUE(SendKey("a", &session, &command));
  EXPECT_FALSE(command.output().has_candidate_window());
  EXPECT_RESULT("NFL", command);

  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);  // obsolete
  EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);
}

TEST_F(SessionTest, ExitTemporaryAlphanumModeAfterCommittingSugesstion3) {
  // This is a unittest against http://b/2977131.
  ;
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  EXPECT_TRUE(SendKey("N", &session, &command));
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);  // obsolete
  EXPECT_EQ(command.output().status().mode(), commands::HALF_ASCII);
  // Global mode should be kept as HIRAGANA
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("NFL");
  segment->add_candidate()->value = "NFL";
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  EXPECT_TRUE(session.ConvertToHalfASCII(&command));
  EXPECT_FALSE(command.output().has_candidate_window());
  EXPECT_FALSE(command.output().candidate_window().has_focused_index());
  EXPECT_EQ(command.output().candidate_window().focused_index(), 0);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);  // obsolete
  EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

  EXPECT_TRUE(SendKey("a", &session, &command));
  EXPECT_FALSE(command.output().has_candidate_window());
  EXPECT_RESULT("NFL", command);
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);  // obsolete
  EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
  EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);
}

TEST_F(SessionTest, StatusOutput) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {  // Basic behavior
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    EXPECT_TRUE(SendKey("a", &session, &command));  // "あ"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    // command.output().mode() is going to be obsolete.
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
    EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
    EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

    EXPECT_TRUE(SendKey("A", &session, &command));  // "あA"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);  // obsolete
    EXPECT_EQ(command.output().status().mode(), commands::HALF_ASCII);
    // Global mode should be kept as HIRAGANA
    EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

    EXPECT_TRUE(SendKey("a", &session, &command));  // "あAa"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);  // obsolete
    EXPECT_EQ(command.output().status().mode(), commands::HALF_ASCII);
    // Global mode should be kept as HIRAGANA
    EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

    // Shift reverts the input mode to Hiragana.
    EXPECT_TRUE(SendKey("Shift", &session, &command));
    EXPECT_TRUE(SendKey("a", &session, &command));  // "あAaあ"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);  // obsolete
    EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
    EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

    EXPECT_TRUE(SendKey("A", &session, &command));  // "あAaあA"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);  // obsolete
    EXPECT_EQ(command.output().status().mode(), commands::HALF_ASCII);
    // Global mode should be kept as HIRAGANA
    EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);

    // When the IME is deactivated, the temporary composition mode is reset.
    EXPECT_TRUE(SendKey("OFF", &session, &command));  // "あAaあA"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
    // command.output().mode() always returns DIRECT when IME is
    // deactivated.  This is the reason why command.output().mode() is
    // going to be obsolete.
    EXPECT_EQ(command.output().mode(), commands::DIRECT);
    EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
    EXPECT_EQ(command.output().status().comeback_mode(), commands::HIRAGANA);
  }

  {  // Katakana mode + Shift key
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    session.InputModeFullKatakana(&command);
    EXPECT_EQ(command.output().mode(), commands::FULL_KATAKANA);  // obsolete
    EXPECT_EQ(command.output().status().mode(), commands::FULL_KATAKANA);
    EXPECT_EQ(command.output().status().comeback_mode(),
              commands::FULL_KATAKANA);

    EXPECT_TRUE(SendKey("a", &session, &command));
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(command.output().mode(), commands::FULL_KATAKANA);  // obsolete
    EXPECT_EQ(command.output().status().mode(), commands::FULL_KATAKANA);
    EXPECT_EQ(command.output().status().comeback_mode(),
              commands::FULL_KATAKANA);

    EXPECT_TRUE(SendKey("A", &session, &command));  // "アA"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);  // obsolete
    EXPECT_EQ(command.output().status().mode(), commands::HALF_ASCII);
    // Global mode should be kept as FULL_KATAKANA
    EXPECT_EQ(command.output().status().comeback_mode(),
              commands::FULL_KATAKANA);

    // When the IME is deactivated, the temporary composition mode is reset.
    EXPECT_TRUE(SendKey("OFF", &session, &command));  // "アA"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
    // command.output().mode() always returns DIRECT when IME is
    // deactivated.  This is the reason why command.output().mode() is
    // going to be obsolete.
    EXPECT_EQ(command.output().mode(), commands::DIRECT);
    EXPECT_EQ(command.output().status().mode(), commands::FULL_KATAKANA);
    EXPECT_EQ(command.output().status().comeback_mode(),
              commands::FULL_KATAKANA);
  }
}

TEST_F(SessionTest, Suggest) {
  Segments segments_m;
  {
    Segment *segment;
    segment = segments_m.add_segment();
    segment->set_key("M");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  Segments segments_mo;
  {
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  Segments segments_moz;
  {
    Segment *segment;
    segment = segments_moz.add_segment();
    segment->set_key("MOZ");
    segment->add_candidate()->value = "MOZUKU";
  }

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  SendKey("M", &session, &command);

  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
  SendKey("O", &session, &command);
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "MOCHA");

  // moz|
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_moz), Return(true)));
  SendKey("Z", &session, &command);
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 1);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "MOZUKU");

  // mo|
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
  SendKey("Backspace", &session, &command);
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "MOCHA");

  // m|o
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
  command.Clear();
  EXPECT_TRUE(session.MoveCursorLeft(&command));
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "MOCHA");

  // mo|
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
  command.Clear();
  EXPECT_TRUE(session.MoveCursorToEnd(&command));
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "MOCHA");

  // |mo
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
  command.Clear();
  EXPECT_TRUE(session.MoveCursorToBeginning(&command));
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "MOCHA");

  // m|o
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
  command.Clear();
  EXPECT_TRUE(session.MoveCursorRight(&command));
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "MOCHA");

  // m|
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_m), Return(true)));
  command.Clear();
  EXPECT_TRUE(session.Delete(&command));
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "MOCHA");

  Segments segments_m_conv;
  {
    Segment *segment;
    segment = segments_m_conv.add_segment();
    segment->set_key("M");
    segment->add_candidate()->value = "M";
    segment->add_candidate()->value = "m";
  }
  const ConversionRequest request_m_conv = CreateConversionRequest(session);
  FillT13Ns(request_m_conv, &segments_m_conv);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_m_conv), Return(true)));
  command.Clear();
  EXPECT_TRUE(session.Convert(&command));

  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_m), Return(true)));
  command.Clear();
  EXPECT_TRUE(session.ConvertCancel(&command));
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "MOCHA");
}

TEST_F(SessionTest, CommitCandidateTypingCorrection) {
  commands::Request request;
  request = *mobile_request_;
  request.set_special_romanji_table(Request::QWERTY_MOBILE_TO_HIRAGANA);

  Segments segments_jueri;
  Segment *segment = segments_jueri.add_segment();
  constexpr absl::string_view kJueri = "じゅえり";
  segment->set_key(kJueri);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->key = "くえり";
  candidate->content_key = candidate->key;
  candidate->value = "クエリ";
  candidate->attributes = Segment::Candidate::PARTIALLY_KEY_CONSUMED;
  candidate->consumed_key_size = strings::CharsLen(kJueri);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session, request);

  commands::Command command;
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments_jueri), Return(true)));
  InsertCharacterChars("jueri", &session, &command);

  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().preedit().segment_size(), 1);
  EXPECT_EQ(command.output().preedit().segment(0).key(), kJueri);
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 1);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "クエリ");

  // commit partial prediction
  EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments_jueri), Return(true)));
  Segments empty_segments;
  EXPECT_CALL(*converter, FinishConversion(_, _))
      .WillOnce(SetArgPointee<1>(empty_segments));
  SetSendCommandCommand(commands::SessionCommand::SUBMIT_CANDIDATE, &command);
  command.mutable_input()->mutable_command()->set_id(0);
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_jueri), Return(true)));
  session.SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_RESULT_AND_KEY("クエリ", "くえり", command);
  EXPECT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, MobilePartialPrediction) {
  commands::Request request;
  request = *mobile_request_;
  request.set_special_romanji_table(
      commands::Request::QWERTY_MOBILE_TO_HIRAGANA);

  Segments segments_wata;
  {
    Segment *segment;
    segment = segments_wata.add_segment();
    constexpr absl::string_view kWata = "わた";
    segment->set_key(kWata);
    Segment::Candidate *cand1 = AddCandidate(kWata, "綿", segment);
    cand1->attributes = Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    cand1->consumed_key_size = strings::CharsLen(kWata);
    Segment::Candidate *cand2 = AddCandidate(kWata, kWata, segment);
    cand2->attributes = Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    cand2->consumed_key_size = strings::CharsLen(kWata);
  }

  Segments segments_watashino;
  {
    Segment *segment;
    segment = segments_watashino.add_segment();
    constexpr absl::string_view kWatashino = "わたしの";
    segment->set_key(kWatashino);
    Segment::Candidate *cand1 = segment->add_candidate();
    cand1->value = "私の";
    cand1->attributes = Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    cand1->consumed_key_size = strings::CharsLen(kWatashino);
    Segment::Candidate *cand2 = segment->add_candidate();
    cand2->value = kWatashino;
    cand2->attributes = Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    cand2->consumed_key_size = strings::CharsLen(kWatashino);
  }

  Segments segments_shino;
  {
    Segment *segment;
    segment = segments_shino.add_segment();
    constexpr absl::string_view kShino = "しの";
    segment->set_key(kShino);
    Segment::Candidate *candidate;
    candidate = AddCandidate("しのみや", "四ノ宮", segment);
    candidate->content_key = segment->key();
    candidate->attributes = Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    candidate->consumed_key_size = strings::CharsLen(kShino);
    candidate = AddCandidate(kShino, "shino", segment);
  }

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session, request);

  commands::Command command;
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillRepeatedly(
          DoAll(SetArgPointee<1>(segments_watashino), Return(true)));
  InsertCharacterChars("watashino", &session, &command);
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "私の");

  // partial suggestion for "わた|しの"
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments_wata), Return(true)));
  command.Clear();
  EXPECT_TRUE(session.MoveCursorLeft(&command));
  command.Clear();
  EXPECT_TRUE(session.MoveCursorLeft(&command));
  // partial suggestion candidates
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "綿");

  // commit partial prediction
  EXPECT_CALL(*converter, CommitPartialSuggestionSegmentValue(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments_wata), Return(true)));
  SetSendCommandCommand(commands::SessionCommand::SUBMIT_CANDIDATE, &command);
  command.mutable_input()->mutable_command()->set_id(0);
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_shino), Return(true)));
  session.SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_RESULT_AND_KEY("綿", "わた", command);

  // remaining text in preedit
  EXPECT_EQ(command.output().preedit().cursor(), 2);
  EXPECT_SINGLE_SEGMENT("しの", command);

  // Suggestion for new text fills the candidates.
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "四ノ宮");
}

TEST_F(SessionTest, ToggleAlphanumericMode) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  {
    InsertCharacterChars("a", &session, &command);
    EXPECT_EQ(GetComposition(command), "あ");
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);

    command.Clear();
    session.ToggleAlphanumericMode(&command);
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
    InsertCharacterChars("a", &session, &command);
    EXPECT_EQ(GetComposition(command), "あa");
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

    command.Clear();
    session.ToggleAlphanumericMode(&command);
    InsertCharacterChars("a", &session, &command);
    EXPECT_EQ(GetComposition(command), "あaあ");
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  }

  {
    // ToggleAlphanumericMode on Precomposition mode should work.
    command.Clear();
    session.EditCancel(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);

    session.ToggleAlphanumericMode(&command);
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
    InsertCharacterChars("a", &session, &command);
    EXPECT_EQ(GetComposition(command), "a");
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
  }

  {
    // A single "n" on Hiragana mode should not converted to "ん" for
    // the compatibility with MS-IME.
    command.Clear();
    session.EditCancel(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

    session.ToggleAlphanumericMode(&command);
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
    InsertCharacterChars("n", &session, &command);  // on Hiragana mode
    EXPECT_EQ(GetComposition(command), "ｎ");

    command.Clear();
    session.ToggleAlphanumericMode(&command);
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
    InsertCharacterChars("a", &session, &command);  // on Half ascii mode.
    EXPECT_EQ(GetComposition(command), "ｎa");
  }

  {
    // ToggleAlphanumericMode should work even when it is called in
    // the conversion state.
    command.Clear();
    session.EditCancel(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

    session.InputModeHiragana(&command);
    InsertCharacterChars("a", &session, &command);  // on Hiragana mode
    EXPECT_EQ(GetComposition(command), "あ");

    Segments segments;
    SetAiueo(&segments);
    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    command.Clear();
    session.Convert(&command);

    EXPECT_EQ(GetComposition(command), "あいうえお");

    command.Clear();
    session.ToggleAlphanumericMode(&command);
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

    command.Clear();
    session.Commit(&command);

    InsertCharacterChars("a", &session, &command);  // on Half ascii mode.
    EXPECT_EQ(GetComposition(command), "a");
  }
}

TEST_F(SessionTest, InsertSpace) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  commands::KeyEvent space_key;
  space_key.set_special_key(commands::KeyEvent::SPACE);

  // Default should be FULL_WIDTH.
  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpace(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT("　", command);  // Full-width space

  // Change the setting to HALF_WIDTH.
  config::Config config;
  config.set_space_character_form(config::Config::FUNDAMENTAL_HALF_WIDTH);
  session.SetConfig(config);
  command.Clear();
  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpace(&command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());

  // Change the setting to FULL_WIDTH.
  config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);
  session.SetConfig(config);
  command.Clear();
  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpace(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT("　", command);  // Full-width space
}

TEST_F(SessionTest, InsertSpaceToggled) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  commands::KeyEvent space_key;
  space_key.set_special_key(commands::KeyEvent::SPACE);

  // Default should be FULL_WIDTH.  So the toggled space should be
  // half-width.
  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpaceToggled(&command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());

  // Change the setting to HALF_WIDTH.
  config::Config config;
  config.set_space_character_form(config::Config::FUNDAMENTAL_HALF_WIDTH);
  session.SetConfig(config);
  command.Clear();
  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpaceToggled(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT("　", command);  // Full-width space

  // Change the setting to FULL_WIDTH.
  config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);
  session.SetConfig(config);
  command.Clear();
  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpaceToggled(&command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, InsertSpaceHalfWidth) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  commands::KeyEvent space_key;
  space_key.set_special_key(commands::KeyEvent::SPACE);

  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpaceHalfWidth(&command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());

  EXPECT_TRUE(SendKey("a", &session, &command));
  EXPECT_EQ(GetComposition(command), "あ");

  command.Clear();
  EXPECT_TRUE(session.InsertSpaceHalfWidth(&command));
  EXPECT_EQ(GetComposition(command), "あ ");

  {  // Convert "あ " with dummy conversions.
    Segments segments;
    segments.add_segment()->add_candidate()->value = "亜 ";
    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    command.Clear();
    EXPECT_TRUE(session.Convert(&command));
  }

  command.Clear();
  EXPECT_TRUE(session.InsertSpaceHalfWidth(&command));
  EXPECT_EQ(command.output().result().value(), "亜  ");
  EXPECT_EQ(GetComposition(command), "");
}

TEST_F(SessionTest, InsertSpaceFullWidth) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  commands::KeyEvent space_key;
  space_key.set_special_key(commands::KeyEvent::SPACE);

  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpaceFullWidth(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT("　", command);  // Full-width space

  EXPECT_TRUE(SendKey("a", &session, &command));
  EXPECT_EQ(GetComposition(command), "あ");

  command.Clear();
  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpaceFullWidth(&command));
  EXPECT_EQ(GetComposition(command), "あ　");  // full-width space

  {  // Convert "あ　" (full-width space) with dummy conversions.
    Segments segments;
    segments.add_segment()->add_candidate()->value = "亜　";
    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    command.Clear();
    EXPECT_TRUE(session.Convert(&command));
  }

  command.Clear();
  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpaceFullWidth(&command));
  EXPECT_EQ(command.output().result().value(), "亜　　");
  EXPECT_EQ(GetComposition(command), "");
}

TEST_F(SessionTest, InsertSpaceWithInputMode) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // First, test against http://b/6027559
  config::Config config;
  {
    constexpr absl::string_view kCustomKeymapTable =
        "status\tkey\tcommand\n"
        "Precomposition\tSpace\tInsertSpace\n"
        "Composition\tSpace\tInsertSpace\n";
    config.set_session_keymap(config::Config::CUSTOM);
    config.set_custom_keymap_table(kCustomKeymapTable);
  }
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    EXPECT_TRUE(TestSendKeyWithMode("Space", commands::HALF_KATAKANA, &session,
                                    &command));
    EXPECT_FALSE(command.output().consumed());
    EXPECT_TRUE(
        SendKeyWithMode("Space", commands::HALF_KATAKANA, &session, &command));
    // In this case, space key event should not be consumed.
    EXPECT_FALSE(command.output().consumed());
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
  }
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    EXPECT_TRUE(TestSendKey("a", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(SendKey("a", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_PREEDIT("あ", command);
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);

    EXPECT_TRUE(TestSendKeyWithMode("Space", commands::HALF_KATAKANA, &session,
                                    &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(
        SendKeyWithMode("Space", commands::HALF_KATAKANA, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_PREEDIT("あ ", command);
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
  }

  {
    constexpr absl::string_view kCustomKeymapTable =
        "status\tkey\tcommand\n"
        "Precomposition\tSpace\tInsertAlternateSpace\n"
        "Composition\tSpace\tInsertAlternateSpace\n";
    config.set_session_keymap(config::Config::CUSTOM);
    config.set_custom_keymap_table(kCustomKeymapTable);
  }
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    EXPECT_TRUE(TestSendKeyWithMode("Space", commands::HALF_KATAKANA, &session,
                                    &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(
        SendKeyWithMode("Space", commands::HALF_KATAKANA, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_RESULT("　", command);
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
    EXPECT_EQ(command.output().mode(), commands::HALF_KATAKANA);
  }
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    EXPECT_TRUE(TestSendKey("a", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(SendKey("a", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_PREEDIT("あ", command);
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);

    EXPECT_TRUE(TestSendKeyWithMode("Space", commands::HALF_KATAKANA, &session,
                                    &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(
        SendKeyWithMode("Space", commands::HALF_KATAKANA, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_PREEDIT("あ　", command);  // Full-width space
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
  }

  // Second, the 1st case filed in http://b/2936141
  {
    constexpr absl::string_view kCustomKeymapTable =
        "status\tkey\tcommand\n"
        "Precomposition\tSpace\tInsertSpace\n"
        "Composition\tSpace\tInsertSpace\n";
    config.set_session_keymap(config::Config::CUSTOM);
    config.set_custom_keymap_table(kCustomKeymapTable);

    config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);
  }
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    EXPECT_TRUE(
        TestSendKeyWithMode("Space", commands::HALF_ASCII, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    command.Clear();
    EXPECT_TRUE(
        SendKeyWithMode("Space", commands::HALF_ASCII, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_RESULT("　", command);
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
  }
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    EXPECT_TRUE(
        TestSendKeyWithMode("a", commands::HALF_ASCII, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(SendKeyWithMode("a", commands::HALF_ASCII, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_PREEDIT("a", command);
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

    EXPECT_TRUE(
        TestSendKeyWithMode("Space", commands::HALF_ASCII, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(
        SendKeyWithMode("Space", commands::HALF_ASCII, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_PREEDIT("a　", command);  // Full-width space
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
  }

  // Finally, the 2nd case filed in http://b/2936141
  {
    constexpr absl::string_view kCustomKeymapTable =
        "status\tkey\tcommand\n"
        "Precomposition\tSpace\tInsertSpace\n"
        "Composition\tSpace\tInsertSpace\n";
    config.set_session_keymap(config::Config::CUSTOM);
    config.set_custom_keymap_table(kCustomKeymapTable);

    config.set_space_character_form(config::Config::FUNDAMENTAL_HALF_WIDTH);
  }
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    EXPECT_TRUE(
        TestSendKeyWithMode("Space", commands::FULL_ASCII, &session, &command));
    EXPECT_FALSE(command.output().consumed());
    EXPECT_TRUE(
        SendKeyWithMode("Space", commands::FULL_ASCII, &session, &command));
    EXPECT_FALSE(command.output().consumed());
  }
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    EXPECT_TRUE(
        TestSendKeyWithMode("a", commands::FULL_ASCII, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(SendKeyWithMode("a", commands::FULL_ASCII, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_PREEDIT("ａ", command);
    EXPECT_EQ(command.output().mode(), commands::FULL_ASCII);

    EXPECT_TRUE(
        TestSendKeyWithMode("Space", commands::FULL_ASCII, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(
        SendKeyWithMode("Space", commands::FULL_ASCII, &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_PREEDIT("ａ ", command);
    EXPECT_EQ(command.output().mode(), commands::FULL_ASCII);
  }
}

TEST_F(SessionTest, InsertSpaceWithCustomKeyBinding) {
  // This is a unittest against http://b/5872031
  config::Config config;
  constexpr absl::string_view kCustomKeymapTable =
      "status\tkey\tcommand\n"
      "Precomposition\tSpace\tInsertSpace\n"
      "Precomposition\tShift Space\tInsertSpace\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(kCustomKeymapTable);
  config.set_space_character_form(config::Config::FUNDAMENTAL_HALF_WIDTH);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  // A plain space key event dispatched to InsertHalfSpace should be consumed.
  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(TestSendKey("Space", &session, &command));
  EXPECT_FALSE(command.output().consumed());  // should not be consumed.
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));

  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(SendKey("Space", &session, &command));
  EXPECT_FALSE(command.output().consumed());  // should not be consumed.
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));

  // A space key event with any modifier key dispatched to InsertHalfSpace
  // should be consumed.
  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(TestSendKey("Shift Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  // It is OK not to check |TryUndoAndAssertDoNothing| here because this
  // (test) send key event is actually *consumed*.

  EXPECT_TRUE(SendKey("Shift Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT(" ", command);
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));
}

TEST_F(SessionTest, InsertAlternateSpaceWithCustomKeyBinding) {
  // This is a unittest against http://b/5872031
  config::Config config;
  constexpr absl::string_view kCustomKeymapTable =
      "status\tkey\tcommand\n"
      "Precomposition\tSpace\tInsertAlternateSpace\n"
      "Precomposition\tShift Space\tInsertAlternateSpace\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(kCustomKeymapTable);
  config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  // A plain space key event dispatched to InsertHalfSpace should be consumed.
  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(TestSendKey("Space", &session, &command));
  EXPECT_FALSE(command.output().consumed());  // should not be consumed.
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));

  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(SendKey("Space", &session, &command));
  EXPECT_FALSE(command.output().consumed());  // should not be consumed.
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));

  // A space key event with any modifier key dispatched to InsertHalfSpace
  // should be consumed.
  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(TestSendKey("Shift Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  // It is OK not to check |TryUndoAndAssertDoNothing| here because this
  // (test) send key event is actually *consumed*.

  EXPECT_TRUE(SendKey("Shift Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT(" ", command);
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));
}

TEST_F(SessionTest, InsertSpaceHalfWidthWithCustomKeyBinding) {
  // This is a unittest against http://b/5872031
  config::Config config;
  constexpr absl::string_view kCustomKeymapTable =
      "status\tkey\tcommand\n"
      "Precomposition\tSpace\tInsertHalfSpace\n"
      "Precomposition\tShift Space\tInsertHalfSpace\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(kCustomKeymapTable);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);

  session.SetKeyMapManager(key_map_manager);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  // A plain space key event assigned to InsertHalfSpace should be echoed back.
  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(TestSendKey("Space", &session, &command));
  EXPECT_FALSE(command.output().consumed());  // should not be consumed.
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));

  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(SendKey("Space", &session, &command));
  EXPECT_FALSE(command.output().consumed());  // should not be consumed.
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));

  // A space key event with any modifier key assigned to InsertHalfSpace should
  // be consumed.
  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(TestSendKey("Shift Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  // It is OK not to check |TryUndoAndAssertDoNothing| here because this
  // (test) send key event is actually *consumed*.

  EXPECT_TRUE(SendKey("Shift Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT(" ", command);
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));
}

TEST_F(SessionTest, InsertSpaceFullWidthWithCustomKeyBinding) {
  // This is a unittest against http://b/5872031
  config::Config config;
  constexpr absl::string_view kCustomKeymapTable =
      "status\tkey\tcommand\n"
      "Precomposition\tSpace\tInsertFullSpace\n"
      "Precomposition\tShift Space\tInsertFullSpace\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(kCustomKeymapTable);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);
  InitSessionToDirect(&session);

  commands::Command command;

  // A plain space key event assigned to InsertFullSpace should be consumed.
  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(TestSendKey("Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  // It is OK not to check |TryUndoAndAssertDoNothing| here because this
  // (test) send key event is actually *consumed*.

  EXPECT_TRUE(SendKey("Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT("　", command);  // Full-width space
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));

  // A space key event with any modifier key assigned to InsertFullSpace should
  // be consumed.
  SetUndoContext(&session, converter.get());
  EXPECT_TRUE(TestSendKey("Shift Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  // It is OK not to check |TryUndoAndAssertDoNothing| here because this
  // (test) send key event is actually *consumed*.

  EXPECT_TRUE(SendKey("Shift Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT("　", command);  // Full-width space
  EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));
}

TEST_F(SessionTest, InsertSpaceInDirectMode) {
  config::Config config;
  constexpr absl::string_view kCustomKeymapTable =
      "status\tkey\tcommand\n"
      "Direct\tCtrl a\tInsertSpace\n"
      "Direct\tCtrl b\tInsertAlternateSpace\n"
      "Direct\tCtrl c\tInsertHalfSpace\n"
      "Direct\tCtrl d\tInsertFullSpace\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(kCustomKeymapTable);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);
  InitSessionToDirect(&session);

  commands::Command command;

  // [InsertSpace] should be echoes back in the direct mode.
  EXPECT_TRUE(TestSendKey("Ctrl a", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(SendKey("Ctrl a", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());

  // [InsertAlternateSpace] should be echoes back in the direct mode.
  EXPECT_TRUE(TestSendKey("Ctrl b", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(SendKey("Ctrl b", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());

  // [InsertHalfSpace] should be echoes back in the direct mode.
  EXPECT_TRUE(TestSendKey("Ctrl c", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(SendKey("Ctrl c", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());

  // [InsertFullSpace] should be echoes back in the direct mode.
  EXPECT_TRUE(TestSendKey("Ctrl d", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(SendKey("Ctrl d", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, InsertSpaceInCompositionMode) {
  // This is a unittest against http://b/5872031
  config::Config config;
  constexpr absl::string_view kCustomKeymapTable =
      "status\tkey\tcommand\n"
      "Composition\tCtrl a\tInsertSpace\n"
      "Composition\tCtrl b\tInsertAlternateSpace\n"
      "Composition\tCtrl c\tInsertHalfSpace\n"
      "Composition\tCtrl d\tInsertFullSpace\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(kCustomKeymapTable);
  config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  SendKey("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ");
  EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);

  EXPECT_TRUE(TestSendKey("Ctrl a", &session, &command));
  EXPECT_TRUE(command.output().consumed());

  SendKey("Ctrl a", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ　");

  EXPECT_TRUE(TestSendKey("Ctrl b", &session, &command));
  EXPECT_TRUE(command.output().consumed());

  SendKey("Ctrl b", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ　 ");

  EXPECT_TRUE(TestSendKey("Ctrl c", &session, &command));
  EXPECT_TRUE(command.output().consumed());

  SendKey("Ctrl c", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ　  ");

  EXPECT_TRUE(TestSendKey("Ctrl d", &session, &command));
  EXPECT_TRUE(command.output().consumed());

  SendKey("Ctrl d", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ　  　");
}

TEST_F(SessionTest, InsertSpaceInConversionMode) {
  // This is a unittest against http://b/5872031
  config::Config config;
  constexpr absl::string_view kCustomKeymapTable =
      "status\tkey\tcommand\n"
      "Conversion\tCtrl a\tInsertSpace\n"
      "Conversion\tCtrl b\tInsertAlternateSpace\n"
      "Conversion\tCtrl c\tInsertHalfSpace\n"
      "Conversion\tCtrl d\tInsertFullSpace\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(kCustomKeymapTable);
  config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);

  {
    InitSessionToConversionWithAiueo(&session, converter.get());
    commands::Command command;

    EXPECT_TRUE(TestSendKey("Ctrl a", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("Ctrl a", &session, &command));
    EXPECT_TRUE(GetComposition(command).empty());
    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ(command.output().result().value(), "あいうえお　");
    EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {
    InitSessionToConversionWithAiueo(&session, converter.get());
    commands::Command command;

    EXPECT_TRUE(TestSendKey("Ctrl b", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("Ctrl b", &session, &command));
    EXPECT_TRUE(GetComposition(command).empty());
    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ(command.output().result().value(), "あいうえお ");
    EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {
    InitSessionToConversionWithAiueo(&session, converter.get());
    commands::Command command;

    EXPECT_TRUE(TestSendKey("Ctrl c", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("Ctrl c", &session, &command));
    EXPECT_TRUE(GetComposition(command).empty());
    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ(command.output().result().value(), "あいうえお ");
    EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {
    InitSessionToConversionWithAiueo(&session, converter.get());
    commands::Command command;

    EXPECT_TRUE(TestSendKey("Ctrl d", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("Ctrl d", &session, &command));
    EXPECT_TRUE(GetComposition(command).empty());
    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ(command.output().result().value(), "あいうえお　");
    EXPECT_TRUE(TryUndoAndAssertDoNothing(&session));
    Mock::VerifyAndClearExpectations(converter.get());
  }
}

TEST_F(SessionTest, InsertSpaceFullWidthOnHalfKanaInput) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  EXPECT_TRUE(session.InputModeHalfKatakana(&command));
  EXPECT_EQ(command.output().mode(), commands::HALF_KATAKANA);
  InsertCharacterChars("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "ｱ");

  command.Clear();
  commands::KeyEvent space_key;
  space_key.set_special_key(commands::KeyEvent::SPACE);
  *command.mutable_input()->mutable_key() = space_key;
  EXPECT_TRUE(session.InsertSpaceFullWidth(&command));
  EXPECT_EQ(GetComposition(command), "ｱ　");  // "ｱ　" (full-width space)
}

TEST_F(SessionTest, IsFullWidthInsertSpace) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  config::Config config;
  commands::Command command;
  const commands::Input empty_input;

  // When |empty_command| does not have |empty_command.key().input()| field,
  // the current input mode will be used.

  {
    // Default config -- follow to the current mode.
    config.set_space_character_form(config::Config::FUNDAMENTAL_INPUT_MODE);
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);

    // Hiragana
    session.InputModeHiragana(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(empty_input));
    // Full-Katakana
    command.Clear();
    session.InputModeFullKatakana(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(empty_input));
    // Half-Katakana
    command.Clear();
    session.InputModeHalfKatakana(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(empty_input));
    // Full-ASCII
    command.Clear();
    session.InputModeFullASCII(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(empty_input));
    // Half-ASCII
    command.Clear();
    session.InputModeHalfASCII(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(empty_input));
    // Direct
    command.Clear();
    session.IMEOff(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(empty_input));
  }

  {
    // Set config to 'half' -- all mode has to emit half-width space.
    config.set_space_character_form(config::Config::FUNDAMENTAL_HALF_WIDTH);
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);

    // Hiragana
    command.Clear();
    session.InputModeHiragana(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(empty_input));
    // Full-Katakana
    command.Clear();
    session.InputModeFullKatakana(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(empty_input));
    // Half-Katakana
    command.Clear();
    session.InputModeHalfKatakana(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(empty_input));
    // Full-ASCII
    command.Clear();
    session.InputModeFullASCII(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(empty_input));
    // Half-ASCII
    command.Clear();
    session.InputModeHalfASCII(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(empty_input));
    // Direct
    command.Clear();
    session.IMEOff(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(empty_input));
  }

  {
    // Set config to 'FULL' -- all mode except for DIRECT emits
    // full-width space.
    config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);

    // Hiragana
    command.Clear();
    session.InputModeHiragana(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(empty_input));
    // Full-Katakana
    command.Clear();
    session.InputModeFullKatakana(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(command.input()));
    // Half-Katakana
    command.Clear();
    session.InputModeHalfKatakana(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(empty_input));
    // Full-ASCII
    command.Clear();
    session.InputModeFullASCII(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(empty_input));
    // Half-ASCII
    command.Clear();
    session.InputModeHalfASCII(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(empty_input));
    // Direct
    command.Clear();
    session.IMEOff(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(empty_input));
  }

  // When |input| has |input.key().mode()| field,
  // the specified input mode by |input| will be used.

  {
    // Default config -- follow to the current mode.
    config.set_space_character_form(config::Config::FUNDAMENTAL_INPUT_MODE);
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);

    // Use HALF_KATAKANA for the new input mode
    commands::Input input;
    input.mutable_key()->set_mode(commands::HALF_KATAKANA);

    // Hiragana
    commands::Command command;
    session.InputModeHiragana(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(input));
    // Full-Katakana
    command.Clear();
    session.InputModeFullKatakana(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(input));
    // Half-Katakana
    command.Clear();
    session.InputModeHalfKatakana(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(input));
    // Full-ASCII
    command.Clear();
    session.InputModeFullASCII(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(input));
    // Half-ASCII
    command.Clear();
    session.InputModeHalfASCII(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(input));
    // Direct
    command.Clear();
    session.IMEOff(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(input));

    // Use FULL_ASCII for the new input mode
    input.mutable_key()->set_mode(commands::FULL_ASCII);

    // Hiragana
    command.Clear();
    session.InputModeHiragana(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(input));
    // Full-Katakana
    command.Clear();
    session.InputModeFullKatakana(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(input));
    // Half-Katakana
    command.Clear();
    session.InputModeHalfKatakana(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(input));
    // Full-ASCII
    command.Clear();
    session.InputModeFullASCII(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(input));
    // Half-ASCII
    command.Clear();
    session.InputModeHalfASCII(&command);
    EXPECT_TRUE(session.IsFullWidthInsertSpace(input));
    // Direct
    command.Clear();
    session.IMEOff(&command);
    EXPECT_FALSE(session.IsFullWidthInsertSpace(input));
  }
}

TEST_F(SessionTest, Issue1951385) {
  // This is a unittest against http://b/1951385

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  const std::string exceeded_preedit(500, 'a');
  ASSERT_EQ(exceeded_preedit.size(), 500);
  InsertCharacterChars(exceeded_preedit, &session, &command);

  Segments segments;
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(false)));

  command.Clear();
  session.ConvertToFullASCII(&command);
  EXPECT_FALSE(command.output().has_candidate_window());

  // The status should remain the preedit status, although the
  // previous command was convert.  The next command makes sure that
  // the preedit will disappear by canceling the preedit status.
  command.Clear();
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::ESCAPE);
  EXPECT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, Issue1978201) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // This is a unittest against http://b/1978201
  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("いんぼう");
  segment->add_candidate()->value = "陰謀";
  segment->add_candidate()->value = "陰謀論";
  segment->add_candidate()->value = "陰謀説";

  commands::Command command;
  EXPECT_TRUE(session.SegmentWidthShrink(&command));

  command.Clear();
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(session.Convert(&command));

  command.Clear();
  EXPECT_TRUE(session.CommitSegment(&command));
  EXPECT_RESULT("陰謀", command);
  EXPECT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, Issue1975771) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // This is a unittest against http://b/1975771
  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Trigger suggest by pressing "a".
  Segments segments;
  SetAiueo(&segments);
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  commands::Command command;
  commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
  key_event->set_key_code('a');
  key_event->set_modifiers(0);  // No modifiers.
  EXPECT_TRUE(session.InsertCharacter(&command));

  // Click the first candidate.
  SetSendCommandCommand(commands::SessionCommand::SELECT_CANDIDATE, &command);
  command.mutable_input()->mutable_command()->set_id(0);
  EXPECT_TRUE(session.SendCommand(&command));

  // After select candidate session.status_ should be
  // SessionStatus::CONVERSION.

  SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  EXPECT_TRUE(command.output().has_candidate_window());
  // The second candidate should be selected.
  EXPECT_EQ(command.output().candidate_window().focused_index(), 1);
}

TEST_F(SessionTest, Issue2029466) {
  // This is a unittest against http://b/2029466
  //
  // "a<tab><ctrl-N>a" raised an exception because CommitFirstSegment
  // did not check if the current status is in conversion or
  // precomposition.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("a", &session, &command);

  // <tab>
  Segments segments;
  SetAiueo(&segments);
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  command.Clear();
  EXPECT_TRUE(session.PredictAndConvert(&command));

  // <ctrl-N>
  segments.Clear();
  // FinishConversion is expected to return empty Segments.
  EXPECT_CALL(*converter, FinishConversion(_, _))
      .WillOnce(SetArgPointee<1>(segments));
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(false)));
  command.Clear();
  EXPECT_TRUE(session.CommitSegment(&command));

  InsertCharacterChars("a", &session, &command);
  EXPECT_SINGLE_SEGMENT("あ", command);
  EXPECT_FALSE(command.output().has_candidate_window());
}

TEST_F(SessionTest, Issue2034943) {
  // This is a unittest against http://b/2029466
  //
  // The composition should have been reset if CommitSegment submitted
  // the all segments (e.g. the size of segments is one).

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("mozu", &session, &command);

  {  // Initialize a suggest result triggered by "mozu".
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("mozu");
    Segment::Candidate *candidate;
    candidate = segment->add_candidate();
    candidate->value = "MOZU";
    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }
  // Get conversion
  command.Clear();
  EXPECT_TRUE(session.Convert(&command));

  // submit segment
  command.Clear();
  EXPECT_TRUE(session.CommitSegment(&command));

  // The composition should have been reset.
  InsertCharacterChars("ku", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "く");
}

TEST_F(SessionTest, Issue2026354) {
  // This is a unittest against http://b/2026354

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("aiueo", &session, &command);

  // Trigger suggest by pressing "a".
  Segments segments;
  SetAiueo(&segments);
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  EXPECT_TRUE(session.Convert(&command));

  //  EXPECT_TRUE(session.ConvertNext(&command));
  TestSendKey("Space", &session, &command);
  EXPECT_PREEDIT("あいうえお", command);
  command.mutable_output()->clear_candidate_window();
  EXPECT_FALSE(command.output().has_candidate_window());
}

TEST_F(SessionTest, Issue2066906) {
  // This is a unittest against http://b/2066906

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("a");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "abc";
  candidate = segment->add_candidate();
  candidate->value = "abcdef";
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  // Prediction with "a"
  commands::Command command;
  EXPECT_TRUE(session.PredictAndConvert(&command));
  EXPECT_FALSE(command.output().has_result());

  // Commit
  command.Clear();
  EXPECT_TRUE(session.Commit(&command));
  EXPECT_RESULT("abc", command);

  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  InsertCharacterChars("a", &session, &command);
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, Issue2187132) {
  // This is a unittest against http://b/2187132

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  // Shift + Ascii triggers temporary input mode switch.
  SendKey("A", &session, &command);
  SendKey("Enter", &session, &command);

  // After submission, input mode should be reverted.
  SendKey("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ");

  command.Clear();
  session.EditCancel(&command);
  EXPECT_TRUE(GetComposition(command).empty());

  // If a user intentionally switched an input mode, it should remain.
  EXPECT_TRUE(session.InputModeHalfASCII(&command));
  SendKey("A", &session, &command);
  SendKey("Enter", &session, &command);
  SendKey("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "a");
}

TEST_F(SessionTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  config::Config config;
  config.set_preedit_method(config::Config::KANA);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  session.SetConfig(config);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  session.ToggleAlphanumericMode(&command);

  InsertCharacterCodeAndString('a', "ち", &session, &command);
  EXPECT_EQ(GetComposition(command), "a");

  command.Clear();
  session.ToggleAlphanumericMode(&command);
  EXPECT_EQ(GetComposition(command), "a");

  InsertCharacterCodeAndString('i', "に", &session, &command);
  EXPECT_EQ(GetComposition(command), "aに");
}

TEST_F(SessionTest, Issue1556649) {
  // This is a unittest against http://b/1556649

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("kudoudesu", &session, &command);
  EXPECT_EQ(GetComposition(command), "くどうです");
  EXPECT_EQ(command.output().preedit().cursor(), 5);

  command.Clear();
  EXPECT_TRUE(session.DisplayAsHalfKatakana(&command));
  EXPECT_EQ(GetComposition(command), "ｸﾄﾞｳﾃﾞｽ");
  EXPECT_EQ(command.output().preedit().cursor(), 7);

  for (size_t i = 0; i < 7; ++i) {
    const size_t expected_pos = 6 - i;
    EXPECT_TRUE(SendKey("Left", &session, &command));
    EXPECT_EQ(command.output().preedit().cursor(), expected_pos);
  }
}

TEST_F(SessionTest, Issue1518994) {
  // This is a unittest against http://b/1518994.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // - Can't input space in ascii mode.
  {
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    EXPECT_TRUE(SendKey("a", &session, &command));
    command.Clear();
    EXPECT_TRUE(session.ToggleAlphanumericMode(&command));
    EXPECT_TRUE(SendKey("i", &session, &command));
    EXPECT_EQ(GetComposition(command), "あi");

    EXPECT_TRUE(SendKey("Space", &session, &command));
    EXPECT_EQ(GetComposition(command), "あi ");
  }

  {
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;
    EXPECT_TRUE(SendKey("a", &session, &command));
    EXPECT_TRUE(SendKey("I", &session, &command));
    EXPECT_EQ(GetComposition(command), "あI");

    EXPECT_TRUE(SendKey("Space", &session, &command));
    EXPECT_EQ(GetComposition(command), "あI ");
  }
}

TEST_F(SessionTest, Issue1571043) {
  // This is a unittest against http://b/1571043.
  // - Underline of composition is separated.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("aiu", &session, &command);
  EXPECT_EQ(GetComposition(command), "あいう");

  for (size_t i = 0; i < 3; ++i) {
    const size_t expected_pos = 2 - i;
    EXPECT_TRUE(SendKey("Left", &session, &command));
    EXPECT_EQ(command.output().preedit().cursor(), expected_pos);
    EXPECT_EQ(command.output().preedit().segment_size(), 1);
  }
}

TEST_F(SessionTest, Issue2217250) {
  // This is a unittest against http://b/2217250.
  // Temporary direct input mode through a special sequence such as
  // www. continues even after committing them

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  InsertCharacterChars("www.", &session, &command);
  EXPECT_EQ(GetComposition(command), "www.");
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

  SendKey("Enter", &session, &command);
  EXPECT_EQ(command.output().result().value(), "www.");
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
}

TEST_F(SessionTest, Issue2223823) {
  // This is a unittest against http://b/2223823
  // Input mode does not recover like MS-IME by single shift key down
  // and up.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  SendKey("G", &session, &command);
  EXPECT_EQ(GetComposition(command), "G");
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

  SendKey("Shift", &session, &command);
  EXPECT_EQ(GetComposition(command), "G");
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
}

TEST_F(SessionTest, Issue2223762) {
  // This is a unittest against http://b/2223762.
  // - The first space in half-width alphanumeric mode is full-width.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  EXPECT_TRUE(session.InputModeHalfASCII(&command));
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

  EXPECT_TRUE(SendKey("Space", &session, &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, Issue2223755) {
  // This is a unittest against http://b/2223755.
  // - F6 and F7 convert space to half-width.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {  // DisplayAsFullKatakana
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    EXPECT_TRUE(SendKey("a", &session, &command));
    EXPECT_TRUE(SendKey("Eisu", &session, &command));
    EXPECT_TRUE(SendKey("Space", &session, &command));
    EXPECT_TRUE(SendKey("Eisu", &session, &command));
    EXPECT_TRUE(SendKey("i", &session, &command));

    EXPECT_EQ(GetComposition(command), "あ い");

    command.Clear();
    EXPECT_TRUE(session.DisplayAsFullKatakana(&command));

    EXPECT_EQ(GetComposition(command), "ア　イ");  // fullwidth space
  }

  {  // ConvertToFullKatakana
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    EXPECT_TRUE(SendKey("a", &session, &command));
    EXPECT_TRUE(SendKey("Eisu", &session, &command));
    EXPECT_TRUE(SendKey("Space", &session, &command));
    EXPECT_TRUE(SendKey("Eisu", &session, &command));
    EXPECT_TRUE(SendKey("i", &session, &command));

    EXPECT_EQ(GetComposition(command), "あ い");

    {  // Initialize the mock converter to generate t13n candidates.
      Segments segments;
      Segment *segment;
      segment = segments.add_segment();
      segment->set_key("あ い");
      Segment::Candidate *candidate;
      candidate = segment->add_candidate();
      candidate->value = "あ い";
      const ConversionRequest request = CreateConversionRequest(session);
      FillT13Ns(request, &segments);
      EXPECT_CALL(*converter, StartConversion(_, _))
          .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    }

    command.Clear();
    EXPECT_TRUE(session.ConvertToFullKatakana(&command));

    EXPECT_EQ(GetComposition(command), "ア　イ");  // fullwidth space
  }
}

TEST_F(SessionTest, Issue2269058) {
  // This is a unittest against http://b/2269058.
  // - Temporary input mode should not be overridden by a permanent
  //   input mode change.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  EXPECT_TRUE(SendKey("G", &session, &command));
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

  command.Clear();
  EXPECT_TRUE(session.InputModeHalfASCII(&command));
  EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

  EXPECT_TRUE(SendKey("Shift", &session, &command));
  EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
}

TEST_F(SessionTest, Issue2272745) {
  // This is a unittest against http://b/2272745.
  // A temporary input mode remains when a composition is canceled.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);
  {
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    EXPECT_TRUE(SendKey("G", &session, &command));
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

    EXPECT_TRUE(SendKey("Backspace", &session, &command));
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  }

  {
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    EXPECT_TRUE(SendKey("G", &session, &command));
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

    EXPECT_TRUE(SendKey("Escape", &session, &command));
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  }
}

TEST_F(SessionTest, Issue2282319) {
  // This is a unittest against http://b/2282319.
  // InsertFullSpace is not working in half-width input mode.
  config::Config config;
  config.set_session_keymap(config::Config::MSIME);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  InitSessionToPrecomposition(&session);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);

  commands::Command command;
  EXPECT_TRUE(session.InputModeHalfASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HALF_ASCII);

  EXPECT_TRUE(TestSendKey("a", &session, &command));
  EXPECT_TRUE(command.output().consumed());

  EXPECT_TRUE(SendKey("a", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_PREEDIT("a", command);

  EXPECT_TRUE(TestSendKey("Ctrl Shift Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());

  EXPECT_TRUE(SendKey("Ctrl Shift Space", &session, &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_PREEDIT("a　", command);  // Full-width space
}

TEST_F(SessionTest, Issue2297060) {
  // This is a unittest against http://b/2297060.
  // Ctrl-Space is not working
  config::Config config;
  config.set_session_keymap(config::Config::MSIME);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  InitSessionToPrecomposition(&session);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);

  commands::Command command;
  EXPECT_TRUE(SendKey("Ctrl Space", &session, &command));
  EXPECT_FALSE(command.output().consumed());
}

TEST_F(SessionTest, Issue2379374) {
  // This is a unittest against http://b/2379374.
  // Numpad ignores Direct input style when typing after conversion.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  // Set numpad_character_form with NUMPAD_DIRECT_INPUT
  config::Config config;
  config.set_numpad_character_form(config::Config::NUMPAD_DIRECT_INPUT);
  session.SetConfig(config);

  Segments segments;
  {  // Set mock conversion.
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key("あ");
    candidate = segment->add_candidate();
    candidate->value = "亜";
    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  EXPECT_TRUE(SendKey("a", &session, &command));
  EXPECT_EQ(GetComposition(command), "あ");

  EXPECT_TRUE(SendKey("Space", &session, &command));
  EXPECT_EQ(GetComposition(command), "亜");

  EXPECT_TRUE(SendKey("Numpad0", &session, &command));
  EXPECT_TRUE(GetComposition(command).empty());
  EXPECT_RESULT_AND_KEY("亜0", "あ0", command);

  // The previous Numpad0 must not affect the current composition.
  EXPECT_TRUE(SendKey("a", &session, &command));
  EXPECT_EQ(GetComposition(command), "あ");
}

TEST_F(SessionTest, Issue2569789) {
  // This is a unittest against http://b/2379374.
  // After typing "google", the input mode does not come back to the
  // previous input mode.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    InsertCharacterChars("google", &session, &command);
    EXPECT_EQ(GetComposition(command), "google");
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);

    EXPECT_TRUE(SendKey("enter", &session, &command));
    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ(command.output().result().value(), "google");
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  }

  {
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    InsertCharacterChars("Google", &session, &command);
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

    EXPECT_TRUE(SendKey("enter", &session, &command));
    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ(command.output().result().value(), "Google");
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  }

  {
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    InsertCharacterChars("Google", &session, &command);
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

    EXPECT_TRUE(SendKey("shift", &session, &command));
    EXPECT_EQ(GetComposition(command), "Google");
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);

    InsertCharacterChars("aaa", &session, &command);
    EXPECT_EQ(GetComposition(command), "Googleあああ");
  }

  {
    Session session(engine);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    InsertCharacterChars("http", &session, &command);
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);

    EXPECT_TRUE(SendKey("enter", &session, &command));
    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ(command.output().result().value(), "http");
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
  }
}

TEST_F(SessionTest, Issue2555503) {
  // This is a unittest against http://b/2555503.
  // Mode respects the previous character too much.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  SendKey("a", &session, &command);

  command.Clear();
  session.InputModeFullKatakana(&command);

  SendKey("i", &session, &command);
  EXPECT_EQ(GetComposition(command), "あイ");

  SendKey("backspace", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ");
  EXPECT_EQ(command.output().mode(), commands::FULL_KATAKANA);
}

TEST_F(SessionTest, Issue2791640) {
  // This is a unittest against http://b/2791640.
  // Existing preedit should be committed when IME is turned off.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  SendKey("a", &session, &command);
  SendKey("hankaku/zenkaku", &session, &command);

  ASSERT_TRUE(command.output().consumed());

  ASSERT_TRUE(command.output().has_result());
  EXPECT_EQ(command.output().result().value(), "あ");
  EXPECT_EQ(command.output().mode(), commands::DIRECT);

  ASSERT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, CommitExistingPreeditWhenIMEIsTurnedOff) {
  // Existing preedit should be committed when IME is turned off.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // Check "hankaku/zenkaku"
  {
    Session session(engine);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    SendKey("a", &session, &command);
    SendKey("hankaku/zenkaku", &session, &command);

    ASSERT_TRUE(command.output().consumed());

    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ(command.output().result().value(), "あ");
    EXPECT_EQ(command.output().mode(), commands::DIRECT);

    ASSERT_FALSE(command.output().has_preedit());
  }

  // Check "kanji"
  {
    Session session(engine);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    SendKey("a", &session, &command);
    SendKey("kanji", &session, &command);

    ASSERT_TRUE(command.output().consumed());

    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ(command.output().result().value(), "あ");
    EXPECT_EQ(command.output().mode(), commands::DIRECT);

    ASSERT_FALSE(command.output().has_preedit());
  }
}

TEST_F(SessionTest, SendKeyDirectInputStateTest) {
  // InputModeChange commands from direct mode are supported only for Windows
  // for now.
#ifdef _WIN32
  config::Config config;
  constexpr absl::string_view kCustomKeymapTable =
      "status\tkey\tcommand\n"
      "DirectInput\tHiragana\tInputModeHiragana\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(kCustomKeymapTable);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);
  InitSessionToDirect(&session);
  commands::Command command;

  EXPECT_TRUE(SendKey("Hiragana", &session, &command));
  EXPECT_TRUE(SendKey("a", &session, &command));
  EXPECT_SINGLE_SEGMENT("あ", command);
#endif  // _WIN32
}

TEST_F(SessionTest, HandlingDirectInputTableAttribute) {
  auto table = std::make_shared<composer::Table>();
  table->AddRuleWithAttributes("ka", "か", "", composer::DIRECT_INPUT);
  table->AddRuleWithAttributes("tt", "っ", "t", composer::DIRECT_INPUT);
  table->AddRuleWithAttributes("ta", "た", "", composer::NO_TABLE_ATTRIBUTE);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  session.get_internal_composer_only_for_unittest()->SetTable(table);

  commands::Command command;
  SendKey("k", &session, &command);
  EXPECT_FALSE(command.output().has_result());

  SendKey("a", &session, &command);
  EXPECT_RESULT("か", command);

  SendKey("t", &session, &command);
  EXPECT_FALSE(command.output().has_result());

  SendKey("t", &session, &command);
  EXPECT_FALSE(command.output().has_result());

  SendKey("a", &session, &command);
  EXPECT_RESULT("った", command);
}

TEST_F(SessionTest, IMEOnWithModeTest) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);
  {
    Session session(engine);
    InitSessionToDirect(&session);

    commands::Command command;
    command.mutable_input()->mutable_key()->set_mode(commands::HIRAGANA);
    EXPECT_TRUE(session.IMEOn(&command));
    EXPECT_TRUE(command.output().has_consumed());
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
    SendKey("a", &session, &command);
    EXPECT_SINGLE_SEGMENT("あ", command);
  }
  {
    Session session(engine);
    InitSessionToDirect(&session);

    commands::Command command;
    command.mutable_input()->mutable_key()->set_mode(commands::FULL_KATAKANA);
    EXPECT_TRUE(session.IMEOn(&command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::FULL_KATAKANA);
    SendKey("a", &session, &command);
    EXPECT_SINGLE_SEGMENT("ア", command);
  }
  {
    Session session(engine);
    InitSessionToDirect(&session);

    commands::Command command;
    command.mutable_input()->mutable_key()->set_mode(commands::HALF_KATAKANA);
    EXPECT_TRUE(session.IMEOn(&command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::HALF_KATAKANA);
    SendKey("a", &session, &command);
    // "ｱ" (half-width Katakana)
    EXPECT_SINGLE_SEGMENT("ｱ", command);
  }
  {
    Session session(engine);
    InitSessionToDirect(&session);

    commands::Command command;
    command.mutable_input()->mutable_key()->set_mode(commands::FULL_ASCII);
    EXPECT_TRUE(session.IMEOn(&command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::FULL_ASCII);
    SendKey("a", &session, &command);
    EXPECT_SINGLE_SEGMENT("ａ", command);
  }
  {
    Session session(engine);
    InitSessionToDirect(&session);

    commands::Command command;
    command.mutable_input()->mutable_key()->set_mode(commands::HALF_ASCII);
    EXPECT_TRUE(session.IMEOn(&command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(command.output().mode(), commands::HALF_ASCII);
    SendKey("a", &session, &command);
    EXPECT_SINGLE_SEGMENT("a", command);
  }
}

TEST_F(SessionTest, InputModeConsumed) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  EXPECT_TRUE(session.InputModeHiragana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HIRAGANA);
  command.Clear();
  EXPECT_TRUE(session.InputModeFullKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::FULL_KATAKANA);
  command.Clear();
  EXPECT_TRUE(session.InputModeHalfKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HALF_KATAKANA);
  command.Clear();
  EXPECT_TRUE(session.InputModeFullASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::FULL_ASCII);
  command.Clear();
  EXPECT_TRUE(session.InputModeHalfASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HALF_ASCII);
}

TEST_F(SessionTest, InputModeConsumedForTestSendKey) {
  // This test is only for Windows, because InputModeHiragana bound
  // with Hiragana key is only supported on Windows yet.
#ifdef _WIN32
  config::Config config;
  config.set_session_keymap(config::Config::MSIME);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);
  InitSessionToPrecomposition(&session);
  // In MSIME keymap, Hiragana is assigned for
  // ImputModeHiragana in Precomposition.

  commands::Command command;
  EXPECT_TRUE(TestSendKey("Hiragana", &session, &command));
  EXPECT_TRUE(command.output().consumed());
#endif  // _WIN32
}

TEST_F(SessionTest, InputModeOutputHasComposition) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  SendKey("a", &session, &command);
  EXPECT_SINGLE_SEGMENT("あ", command);

  command.Clear();
  EXPECT_TRUE(session.InputModeHiragana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HIRAGANA);
  EXPECT_SINGLE_SEGMENT("あ", command);

  command.Clear();
  EXPECT_TRUE(session.InputModeFullKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::FULL_KATAKANA);
  EXPECT_SINGLE_SEGMENT("あ", command);

  command.Clear();
  EXPECT_TRUE(session.InputModeHalfKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HALF_KATAKANA);
  EXPECT_SINGLE_SEGMENT("あ", command);

  command.Clear();
  EXPECT_TRUE(session.InputModeFullASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::FULL_ASCII);
  EXPECT_SINGLE_SEGMENT("あ", command);

  command.Clear();
  EXPECT_TRUE(session.InputModeHalfASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HALF_ASCII);
  EXPECT_SINGLE_SEGMENT("あ", command);
}

TEST_F(SessionTest, InputModeOutputHasCandidates) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  Segments segments;
  SetAiueo(&segments);
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  commands::Command command;
  InsertCharacterChars("aiueo", &session, &command);

  command.Clear();
  session.Convert(&command);
  session.ConvertNext(&command);
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_TRUE(command.output().has_preedit());

  command.Clear();
  EXPECT_TRUE(session.InputModeHiragana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HIRAGANA);
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_TRUE(command.output().has_preedit());

  command.Clear();
  EXPECT_TRUE(session.InputModeFullKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::FULL_KATAKANA);
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_TRUE(command.output().has_preedit());

  command.Clear();
  EXPECT_TRUE(session.InputModeHalfKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HALF_KATAKANA);
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_TRUE(command.output().has_preedit());

  command.Clear();
  EXPECT_TRUE(session.InputModeFullASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::FULL_ASCII);
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_TRUE(command.output().has_preedit());

  command.Clear();
  EXPECT_TRUE(session.InputModeHalfASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HALF_ASCII);
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_TRUE(command.output().has_preedit());
}

TEST_F(SessionTest, PerformedCommand) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  {
    commands::Command command;
    // IMEOff
    SendSpecialKey(commands::KeyEvent::OFF, &session, &command);
  }
  {
    commands::Command command;
    // IMEOn
    SendSpecialKey(commands::KeyEvent::ON, &session, &command);
  }
  {
    commands::Command command;
    // 'a'
    SendKey("a", &session, &command);
  }
  {
    // SetStartConversion for changing state to Convert.
    Segments segments;
    SetAiueo(&segments);
    const ConversionRequest request = CreateConversionRequest(session);
    FillT13Ns(request, &segments);
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    commands::Command command;
    // SPACE
    SendSpecialKey(commands::KeyEvent::SPACE, &session, &command);
  }
  {
    commands::Command command;
    // ENTER
    SendSpecialKey(commands::KeyEvent::ENTER, &session, &command);
  }
}

TEST_F(SessionTest, ResetContext) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  EXPECT_CALL(*converter, ResetConversion(_)).Times(2);
  session.ResetContext(&command);
  EXPECT_FALSE(command.output().consumed());

  Segments segments;
  segments.add_segment()->add_candidate();  // Stub candidate.
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(SendKey("A", &session, &command));
  command.Clear();

  EXPECT_CALL(*converter, ResetConversion(_));
  session.ResetContext(&command);
  EXPECT_TRUE(command.output().consumed());
}

TEST_F(SessionTest, ClearUndoOnResetContext) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  commands::Command command;
  Segments segments;

  {  // Create segments
    InsertCharacterChars("aiueo", &session, &command);
    SetAiueo(&segments);
    // Don't use FillT13Ns(). It makes platform dependent segments.
    // TODO(hsumita): Makes FillT13Ns() independent from platforms.
    Segment::Candidate *candidate;
    candidate = segments.mutable_segment(0)->add_candidate();
    candidate->value = "aiueo";
    candidate = segments.mutable_segment(0)->add_candidate();
    candidate->value = "AIUEO";
  }

  {
    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_SINGLE_SEGMENT("あいうえお", command);

    EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    session.Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_RESULT("あいうえお", command);

    command.Clear();
    session.ResetContext(&command);

    command.Clear();
    session.Undo(&command);
    // After reset, undo shouldn't run.
    EXPECT_FALSE(command.output().has_preedit());
  }
}

TEST_F(SessionTest, IssueResetConversion) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  // Any meaneangless key calls ResetConversion
  EXPECT_CALL(*converter, ResetConversion(_));
  EXPECT_TRUE(SendKey("enter", &session, &command));

  EXPECT_CALL(*converter, ResetConversion(_)).Times(2);
  EXPECT_TRUE(SendKey("space", &session, &command));
}

TEST_F(SessionTest, IssueRevert) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  // Changes the state to PRECOMPOSITION
  session.IMEOn(&command);

  EXPECT_CALL(*converter, RevertConversion(_));
  EXPECT_CALL(*converter, ResetConversion(_));
  session.Revert(&command);
  EXPECT_FALSE(command.output().consumed());
}

// Undo command must call RervertConversion
TEST_F(SessionTest, Issue3428520) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  commands::Command command;
  Segments segments;
  SetAiueo(&segments);

  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  InsertCharacterChars("aiueo", &session, &command);
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);

  EXPECT_CALL(*converter, StartConversion(_, _)).WillOnce(Return(true));
  command.Clear();
  session.Convert(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_SINGLE_SEGMENT("あいうえお", command);

  EXPECT_CALL(*converter, CommitSegmentValue(_, _, _)).WillOnce(Return(true));
  EXPECT_CALL(*converter, FinishConversion(_, _));
  command.Clear();
  session.Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT("あいうえお", command);

  // RevertConversion must be called.
  EXPECT_CALL(*converter, RevertConversion(_));
  command.Clear();
  session.Undo(&command);
}

// Revert command must clear the undo context.
TEST_F(SessionTest, Issue5742293) {
  config::Config config;
  config.set_session_keymap(config::Config::MSIME);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  SetUndoContext(&session, converter.get());

  commands::Command command;

  // BackSpace key event issues Revert command, which should clear the undo
  // context.
  EXPECT_TRUE(SendKey("Backspace", &session, &command));

  // Ctrl+BS should be consumed as UNDO.
  EXPECT_TRUE(TestSendKey("Ctrl Backspace", &session, &command));

  EXPECT_FALSE(command.output().consumed());
}

TEST_F(SessionTest, AutoConversion) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Segments segments;
  SetAiueo(&segments);
  ConversionRequest default_request;
  FillT13Ns(default_request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

  // Auto Off
  config::Config config;
  config.set_use_auto_conversion(false);
  {
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterChars("tesuto.", &session, &command);

    EXPECT_SINGLE_SEGMENT_AND_KEY("てすと。", "てすと。", command);
  }
  {
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterString("てすと。", "wrs/", &session, &command);

    EXPECT_SINGLE_SEGMENT_AND_KEY("てすと。", "てすと。", command);
  }

  // Auto On
  config.set_use_auto_conversion(true);
  {
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);

    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterChars("tesuto.", &session, &command);

    EXPECT_SINGLE_SEGMENT_AND_KEY("あいうえお", "あいうえお", command);
  }
  {
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);

    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterString("てすと。", "wrs/", &session, &command);

    EXPECT_SINGLE_SEGMENT_AND_KEY("あいうえお", "あいうえお", command);
  }

  // Don't trigger auto conversion for the pattern number + "."
  {
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterChars("123.", &session, &command);

    EXPECT_SINGLE_SEGMENT_AND_KEY("１２３．", "１２３．", command);
  }

  // Don't trigger auto conversion for the ".."
  {
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterChars("..", &session, &command);

    EXPECT_SINGLE_SEGMENT_AND_KEY("。。", "。。", command);
  }

  {
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterString("１２３。", "123.", &session, &command);

    EXPECT_SINGLE_SEGMENT_AND_KEY("１２３．", "１２３．", command);
  }

  // Don't trigger auto conversion for "." only.
  {
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterChars(".", &session, &command);

    EXPECT_SINGLE_SEGMENT_AND_KEY("。", "。", command);
  }

  {
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterString("。", "/", &session, &command);

    EXPECT_SINGLE_SEGMENT_AND_KEY("。", "。", command);
  }

  // Do auto conversion even if romanji-table is modified.
  {
    Session session(engine);
    session.SetConfig(config);
    InitSessionToPrecomposition(&session);

    // Modify romanji-table to convert "zz" -> "。"
    auto zz_table = std::make_shared<composer::Table>();
    zz_table->AddRule("te", "て", "");
    zz_table->AddRule("su", "す", "");
    zz_table->AddRule("to", "と", "");
    zz_table->AddRule("zz", "。", "");
    session.get_internal_composer_only_for_unittest()->SetTable(zz_table);

    // The last "zz" is converted to "." and triggering key for auto conversion
    commands::Command command;
    InsertCharacterChars("tesutozz", &session, &command);

    EXPECT_SINGLE_SEGMENT_AND_KEY("あいうえお", "あいうえお", command);
  }

  {
    const char trigger_key[] = ".,?!";

    // try all possible patterns.
    for (int kana_mode = 0; kana_mode < 2; ++kana_mode) {
      for (int onoff = 0; onoff < 2; ++onoff) {
        for (int pattern = 0; pattern <= 16; ++pattern) {
          config.set_use_auto_conversion(onoff != 0);
          config.set_auto_conversion_key(pattern);

          int flag[4];
          flag[0] = static_cast<int>(config.auto_conversion_key() &
                                     config::Config::AUTO_CONVERSION_KUTEN);
          flag[1] = static_cast<int>(config.auto_conversion_key() &
                                     config::Config::AUTO_CONVERSION_TOUTEN);
          flag[2] =
              static_cast<int>(config.auto_conversion_key() &
                               config::Config::AUTO_CONVERSION_QUESTION_MARK);
          flag[3] = static_cast<int>(
              config.auto_conversion_key() &
              config::Config::AUTO_CONVERSION_EXCLAMATION_MARK);

          for (int i = 0; i < 4; ++i) {
            Session session(engine);
            session.SetConfig(config);
            InitSessionToPrecomposition(&session);
            commands::Command command;

            if (kana_mode) {
              std::string key = "てすと";
              key += trigger_key[i];
              InsertCharacterString(key, "wst/", &session, &command);
            } else {
              std::string key = "tesuto";
              key += trigger_key[i];
              InsertCharacterChars(key, &session, &command);
            }
            EXPECT_TRUE(command.output().has_preedit());
            EXPECT_EQ(command.output().preedit().segment_size(), 1);
            EXPECT_TRUE(command.output().preedit().segment(0).has_value());
            EXPECT_TRUE(command.output().preedit().segment(0).has_key());

            if (onoff > 0 && flag[i] > 0) {
              EXPECT_EQ(command.output().preedit().segment(0).key(),
                        "あいうえお");
            } else {
              // Not "あいうえお"
              EXPECT_NE(command.output().preedit().segment(0).key(),
                        "あいうえお");
            }
          }
        }
      }
    }
  }
}

TEST_F(SessionTest, InputSpaceWithKatakanaMode) {
  // This is a unittest against http://b/3203944.
  // Input mode should not be changed when a space key is typed.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  EXPECT_TRUE(session.InputModeHiragana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().mode(), mozc::commands::HIRAGANA);

  SetSendKeyCommand("Space", &command);
  command.mutable_input()->mutable_key()->set_mode(commands::FULL_KATAKANA);
  EXPECT_TRUE(session.SendKey(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_RESULT("　", command);
  EXPECT_EQ(command.output().mode(), mozc::commands::FULL_KATAKANA);
}

TEST_F(SessionTest, AlphanumericOfSSH) {
  // This is a unittest against http://b/3199626
  // 'ssh' (っｓｈ) + F10 should be 'ssh'.

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("ssh", &session, &command);
  EXPECT_EQ(GetComposition(command), "っｓｈ");

  Segments segments;
  // Set a dummy segments for ConvertToHalfASCII.
  {
    Segment *segment;
    segment = segments.add_segment();
    segment->set_key("っsh");

    segment->add_candidate()->value = "[SSH]";
  }
  const ConversionRequest request = CreateConversionRequest(session);
  FillT13Ns(request, &segments);
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  command.Clear();
  EXPECT_TRUE(session.ConvertToHalfASCII(&command));
  EXPECT_SINGLE_SEGMENT("ssh", command);
}

TEST_F(SessionTest, KeitaiInputToggle) {
  config::Config config;
  config.set_session_keymap(config::Config::MSIME);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
  session.SetConfig(config);
  session.SetKeyMapManager(key_map_manager);

  InitSessionToPrecomposition(&session, *mobile_request_);
  commands::Command command;

  SendKey("1", &session, &command);
  // "あ|"
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あ");
  EXPECT_EQ(command.output().preedit().cursor(), 1);

  SendKey("1", &session, &command);
  // "い|"
  EXPECT_EQ(command.output().preedit().segment(0).value(), "い");
  EXPECT_EQ(command.output().preedit().cursor(), 1);

  SendKey("1", &session, &command);
  SendKey("1", &session, &command);
  SendKey("1", &session, &command);
  SendKey("1", &session, &command);
  SendKey("1", &session, &command);
  SendKey("1", &session, &command);
  SendKey("1", &session, &command);
  SendKey("1", &session, &command);
  SendKey("1", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あ");
  EXPECT_EQ(command.output().preedit().cursor(), 1);

  SendKey("2", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あか");
  EXPECT_EQ(command.output().preedit().cursor(), 2);

  SendKey("2", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あき");
  EXPECT_EQ(command.output().preedit().cursor(), 2);

  SendKey("*", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あぎ");
  EXPECT_EQ(command.output().preedit().cursor(), 2);

  SendKey("*", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あき");
  EXPECT_EQ(command.output().preedit().cursor(), 2);

  SendKey("3", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あきさ");
  EXPECT_EQ(command.output().preedit().cursor(), 3);

  SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あきさ");
  EXPECT_EQ(command.output().preedit().cursor(), 3);

  SendKey("3", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あきささ");
  EXPECT_EQ(command.output().preedit().cursor(), 4);

  SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あきささ");
  EXPECT_EQ(command.output().preedit().cursor(), 3);

  SendKey("4", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あきさたさ");
  EXPECT_EQ(command.output().preedit().cursor(), 4);

  SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あきさたさ");
  EXPECT_EQ(command.output().preedit().cursor(), 3);

  SendKey("*", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あきざたさ");
  EXPECT_EQ(command.output().preedit().cursor(), 3);

  // Test for End key
  SendSpecialKey(commands::KeyEvent::END, &session, &command);
  SendKey("6", &session, &command);
  SendKey("6", &session, &command);
  SendSpecialKey(commands::KeyEvent::END, &session, &command);
  SendKey("6", &session, &command);
  SendKey("*", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あきざたさひば");
  EXPECT_EQ(command.output().preedit().cursor(), 7);

  // Test for Right key
  SendSpecialKey(commands::KeyEvent::END, &session, &command);
  SendKey("6", &session, &command);
  SendKey("6", &session, &command);
  SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
  SendKey("6", &session, &command);
  SendKey("*", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(),
            "あきざたさひばひば");
  EXPECT_EQ(command.output().preedit().cursor(), 9);

  // Test for Left key
  SendSpecialKey(commands::KeyEvent::END, &session, &command);
  SendKey("6", &session, &command);
  SendKey("6", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(),
            "あきざたさひばひばひ");
  SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
  SendKey("6", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(),
            "あきざたさひばひばはひ");
  SendKey("*", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(),
            "あきざたさひばひばばひ");
  EXPECT_EQ(command.output().preedit().cursor(), 10);

  // Test for Home key
  SendSpecialKey(commands::KeyEvent::HOME, &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(),
            "あきざたさひばひばばひ");
  SendKey("6", &session, &command);
  SendKey("*", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(),
            "ばあきざたさひばひばばひ");
  EXPECT_EQ(command.output().preedit().cursor(), 1);

  SendSpecialKey(commands::KeyEvent::END, &session, &command);
  SendKey("5", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(),
            "ばあきざたさひばひばばひな");
  SendKey("*", &session, &command);  // no effect
  EXPECT_EQ(command.output().preedit().segment(0).value(),
            "ばあきざたさひばひばばひな");
  EXPECT_EQ(command.output().preedit().cursor(), 13);
}

TEST_F(SessionTest, KeitaiInputFlick) {
  config::Config config;
  config.set_session_keymap(config::Config::MSIME);
  mobile_request_->set_special_romanji_table(Request::TWELVE_KEYS_TO_HIRAGANA);
  commands::Command command;

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session, *mobile_request_);
    InsertCharacterCodeAndString('6', "は", &session, &command);
    InsertCharacterCodeAndString('3', "し", &session, &command);
    SendKey("*", &session, &command);
    InsertCharacterCodeAndString('3', "ょ", &session, &command);
    InsertCharacterCodeAndString('1', "う", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "はじょう");
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session, *mobile_request_);

    SendKey("6", &session, &command);
    SendKey("3", &session, &command);
    SendKey("3", &session, &command);
    SendKey("*", &session, &command);
    InsertCharacterCodeAndString('3', "ょ", &session, &command);
    InsertCharacterCodeAndString('1', "う", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "はじょう");
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session, *mobile_request_);

    SendKey("1", &session, &command);
    SendKey("2", &session, &command);
    SendKey("3", &session, &command);
    SendKey("3", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "あかし");
    InsertCharacterCodeAndString('5', "の", &session, &command);
    InsertCharacterCodeAndString('2', "く", &session, &command);
    InsertCharacterCodeAndString('3', "し", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "あかしのくし");
    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    SendKey("9", &session, &command);
    SendKey("9", &session, &command);
    SendKey("9", &session, &command);
    SendKey("9", &session, &command);
    SendKey("9", &session, &command);
    SendKey("9", &session, &command);
    SendKey("9", &session, &command);
    SendKey("9", &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    InsertCharacterCodeAndString('0', "ん", &session, &command);
    SendSpecialKey(commands::KeyEvent::END, &session, &command);
    SendKey("1", &session, &command);
    SendKey("1", &session, &command);
    SendKey("1", &session, &command);
    SendKey("*", &session, &command);
    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    InsertCharacterCodeAndString('8', "ゆ", &session, &command);
    SendKey("*", &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendKey("*", &session, &command);
    SendKey("*", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(),
              "あるかしんのくしゅう");
    SendSpecialKey(commands::KeyEvent::HOME, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    InsertCharacterCodeAndString('6', "は", &session, &command);
    SendKey("*", &session, &command);
    SendKey("*", &session, &command);
    SendKey("*", &session, &command);
    SendKey("*", &session, &command);
    SendKey("*", &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendKey("6", &session, &command);
    SendKey("6", &session, &command);
    SendKey("6", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(),
              "あるぱかしんのふくしゅう");
    Mock::VerifyAndClearExpectations(converter.get());
  }
}

TEST_F(SessionTest, ToggleFlick) {
  config::Config config;
  config.set_session_keymap(config::Config::MSIME);
  mobile_request_->set_special_romanji_table(Request::TOGGLE_FLICK_TO_HIRAGANA);
  commands::Command command;

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session, *mobile_request_);
    InsertCharacterChars("6d*888888;", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "はじょう");
    Mock::VerifyAndClearExpectations(converter.get());
  }
  {
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session, *mobile_request_);

    InsertCharacterChars("1233", &session, &command);  // Toggle
    EXPECT_EQ(command.output().preedit().segment(0).value(), "あかし");
    EXPECT_TRUE(command.output().preedit().is_toggleable());

    InsertCharacterChars("%bd", &session, &command);  // Flick
    EXPECT_EQ(command.output().preedit().segment(0).value(), "あかしのくし");
    EXPECT_FALSE(command.output().preedit().is_toggleable());
    EXPECT_EQ(command.output().preedit().cursor(), 6);

    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    EXPECT_FALSE(command.output().preedit().is_toggleable());
    EXPECT_EQ(command.output().preedit().cursor(), 5);

    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    EXPECT_EQ(command.output().preedit().cursor(), 1);
    EXPECT_FALSE(command.output().preedit().is_toggleable());

    InsertCharacterChars("99999999", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(), "あるかしのくし");
    EXPECT_EQ(command.output().preedit().cursor(), 2);

    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    EXPECT_EQ(command.output().preedit().cursor(), 4);

    InsertCharacterChars("/", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(),
              "あるかしんのくし");

    SendSpecialKey(commands::KeyEvent::END, &session, &command);
    EXPECT_EQ(command.output().preedit().cursor(), 8);
    EXPECT_FALSE(command.output().preedit().is_toggleable());

    InsertCharacterChars("111*", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(),
              "あるかしんのくしぅ");

    SendSpecialKey(commands::KeyEvent::LEFT, &session, &command);
    InsertCharacterChars("u*", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(),
              "あるかしんのくしゅぅ");

    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    InsertCharacterChars("**", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(),
              "あるかしんのくしゅう");

    SendSpecialKey(commands::KeyEvent::HOME, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    InsertCharacterChars("6*****", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(),
              "あるぱかしんのくしゅう");

    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    SendSpecialKey(commands::KeyEvent::RIGHT, &session, &command);
    InsertCharacterChars("n", &session, &command);
    EXPECT_EQ(command.output().preedit().segment(0).value(),
              "あるぱかしんのふくしゅう");
    EXPECT_FALSE(command.output().preedit().is_toggleable());
    Mock::VerifyAndClearExpectations(converter.get());
  }
}

TEST_F(SessionTest, CommitCandidateAt2ndOf3Segments) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("nekonoshippowonuita", &session, &command);

  {  // Segments as conversion result.
    Segments segments;
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key("ねこの");
    candidate = segment->add_candidate();
    candidate->value = "猫の";

    segment = segments.add_segment();
    segment->set_key("しっぽを");
    candidate = segment->add_candidate();
    candidate->value = "しっぽを";

    segment = segments.add_segment();
    segment->set_key("ぬいた");
    candidate = segment->add_candidate();
    candidate->value = "抜いた";

    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  command.Clear();
  session.Convert(&command);
  // "[猫の]|しっぽを|抜いた"

  command.Clear();
  session.SegmentFocusRight(&command);
  // "猫の|[しっぽを]|抜いた"

  {  // Segments as result of CommitHeadToFocusedSegments
    Segments segments;
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key("ぬいた");
    candidate = segment->add_candidate();
    candidate->value = "抜いた";

    EXPECT_CALL(*converter, CommitSegments(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
  }

  command.Clear();
  command.mutable_input()->mutable_command()->set_id(0);
  ASSERT_TRUE(session.CommitCandidate(&command));
  EXPECT_PREEDIT("抜いた", command);
  EXPECT_SINGLE_SEGMENT_AND_KEY("抜いた", "ぬいた", command);
  EXPECT_RESULT("猫のしっぽを", command);
}

TEST_F(SessionTest, CommitCandidateAt3rdOf3Segments) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  InsertCharacterChars("nekonoshippowonuita", &session, &command);

  {  // Segments as conversion result.
    Segments segments;
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key("ねこの");
    candidate = segment->add_candidate();
    candidate->value = "猫の";

    segment = segments.add_segment();
    segment->set_key("しっぽを");
    candidate = segment->add_candidate();
    candidate->value = "しっぽを";

    segment = segments.add_segment();
    segment->set_key("ぬいた");
    candidate = segment->add_candidate();
    candidate->value = "抜いた";

    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  command.Clear();
  session.Convert(&command);
  // "[猫の]|しっぽを|抜いた"

  command.Clear();
  session.SegmentFocusRight(&command);
  session.SegmentFocusRight(&command);
  // "猫の|しっぽを|[抜いた]"

  command.Clear();
  command.mutable_input()->mutable_command()->set_id(0);
  ASSERT_TRUE(session.CommitCandidate(&command));
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT("猫のしっぽを抜いた", command);
}

TEST_F(SessionTest, CommitCandidateSuggestion) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session, *mobile_request_);

  Segments segments_mo;
  {
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    AddCandidate("MOCHA", "MOCHA", segment);
    AddCandidate("MOZUKU", "MOZUKU", segment);
  }

  commands::Command command;
  SendKey("M", &session, &command);
  command.Clear();
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
  SendKey("O", &session, &command);
  ASSERT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
  EXPECT_EQ(command.output().candidate_window().candidate(0).value(), "MOCHA");

  EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments_mo), Return(true)));
  EXPECT_CALL(*converter, FinishConversion(_, _))
      .WillOnce(SetArgPointee<1>(Segments()));
  SetSendCommandCommand(commands::SessionCommand::SUBMIT_CANDIDATE, &command);
  command.mutable_input()->mutable_command()->set_id(1);
  session.SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_RESULT_AND_KEY("MOZUKU", "MOZUKU", command);
  EXPECT_FALSE(command.output().has_preedit());
  // Zero query suggestion fills the candidates.
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().preedit().cursor(), 0);
}

bool FindCandidateID(const commands::CandidateWindow &candidate_window,
                     const absl::string_view value, int *id) {
  CHECK(id);
  for (size_t i = 0; i < candidate_window.candidate_size(); ++i) {
    const commands::CandidateWindow::Candidate &candidate =
        candidate_window.candidate(i);
    if (candidate.value() == value) {
      *id = candidate.id();
      return true;
    }
  }
  return false;
}

void FindCandidateIDs(const commands::CandidateWindow &candidate_window,
                      const absl::string_view value, std::vector<int> *ids) {
  CHECK(ids);
  ids->clear();
  for (size_t i = 0; i < candidate_window.candidate_size(); ++i) {
    const commands::CandidateWindow::Candidate &candidate =
        candidate_window.candidate(i);
    if (candidate.value() == value) {
      ids->push_back(candidate.id());
    }
  }
}

TEST_F(SessionTest, CommitCandidateT13N) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session, *mobile_request_);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("tok");
  AddCandidate("tok", "tok", segment);
  AddMetaCandidate("tok", "tok", segment);
  AddMetaCandidate("tok", "TOK", segment);
  AddMetaCandidate("tok", "Tok", segment);
  EXPECT_EQ(segment->candidate(-1).value, "tok");
  EXPECT_EQ(segment->candidate(-2).value, "TOK");
  EXPECT_EQ(segment->candidate(-3).value, "Tok");

  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

  commands::Command command;
  SendKey("k", &session, &command);
  ASSERT_TRUE(command.output().has_candidate_window());
  int id = 0;
#if defined(_WIN32) || defined(__APPLE__)
  // meta candidates are in cascading window
  EXPECT_FALSE(
      FindCandidateID(command.output().candidate_window(), "TOK", &id));
#else   // _WIN32, __APPLE__
  EXPECT_TRUE(FindCandidateID(command.output().candidate_window(), "TOK", &id));
  EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
  EXPECT_CALL(*converter, FinishConversion(_, _))
      .WillOnce(SetArgPointee<1>(Segments()));
  SetSendCommandCommand(commands::SessionCommand::SUBMIT_CANDIDATE, &command);
  command.mutable_input()->mutable_command()->set_id(id);
  session.SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_RESULT("TOK", command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_EQ(command.output().preedit().cursor(), 0);
#endif  // _WIN32, __APPLE__
}

TEST_F(SessionTest, RequestConvertReverse) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  EXPECT_TRUE(session.RequestConvertReverse(&command));
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_deletion_range());
  EXPECT_TRUE(command.output().has_callback());
  EXPECT_TRUE(command.output().callback().has_session_command());
  EXPECT_EQ(command.output().callback().session_command().type(),
            commands::SessionCommand::CONVERT_REVERSE);
}

TEST_F(SessionTest, ConvertReverseFails) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  constexpr absl::string_view kKanjiContainsNewline = "改行\n禁止";
  commands::Command command;
  SetupCommandForReverseConversion(kKanjiContainsNewline,
                                   command.mutable_input());

  EXPECT_TRUE(session.SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_candidate_window());
}

TEST_F(SessionTest, ConvertReverse) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  constexpr absl::string_view kKanjiAiueo = "阿伊宇江於";
  commands::Command command;
  SetupCommandForReverseConversion(kKanjiAiueo, command.mutable_input());
  SetupMockForReverseConversion(kKanjiAiueo, "あいうえお", converter.get());

  EXPECT_TRUE(session.SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().preedit().segment(0).value(), kKanjiAiueo);
  EXPECT_EQ(command.output().all_candidate_words().candidates(0).value(),
            kKanjiAiueo);
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_GT(command.output().candidate_window().candidate_size(), 0);
}

TEST_F(SessionTest, EscapeFromConvertReverse) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  constexpr absl::string_view kKanjiAiueo = "阿伊宇江於";

  commands::Command command;
  SetupCommandForReverseConversion(kKanjiAiueo, command.mutable_input());
  SetupMockForReverseConversion(kKanjiAiueo, "あいうえお", converter.get());

  EXPECT_TRUE(session.SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(GetComposition(command), kKanjiAiueo);

  SendKey("ESC", &session, &command);

  // KANJI should be converted into HIRAGANA in pre-edit state.
  EXPECT_SINGLE_SEGMENT("あいうえお", command);

  SendKey("ESC", &session, &command);

  // Fixed KANJI should be output
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_RESULT(kKanjiAiueo, command);
}

TEST_F(SessionTest, SecondEscapeFromConvertReverse) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  constexpr absl::string_view kKanjiAiueo = "阿伊宇江於";
  commands::Command command;
  SetupCommandForReverseConversion(kKanjiAiueo, command.mutable_input());
  SetupMockForReverseConversion(kKanjiAiueo, "あいうえお", converter.get());

  EXPECT_TRUE(session.SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(GetComposition(command), kKanjiAiueo);

  SendKey("ESC", &session, &command);
  SendKey("ESC", &session, &command);

  EXPECT_FALSE(command.output().has_preedit());
  // When a reverse conversion is canceled, the converter sets the
  // original text into |command.output().result().key()|.
  EXPECT_RESULT_AND_KEY(kKanjiAiueo, kKanjiAiueo, command);

  SendKey("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ");

  SendKey("ESC", &session, &command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, SecondEscapeFromConvertReverseIssue5687022) {
  // This is a unittest against http://b/5687022
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  constexpr absl::string_view kInput = "abcde";
  constexpr absl::string_view kReading = "abcde";

  commands::Command command;
  SetupCommandForReverseConversion(kInput, command.mutable_input());
  SetupMockForReverseConversion(kInput, kReading, converter.get());

  EXPECT_TRUE(session.SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(GetComposition(command), kInput);

  SendKey("ESC", &session, &command);
  SendKey("ESC", &session, &command);

  EXPECT_FALSE(command.output().has_preedit());
  // When a reverse conversion is canceled, the converter sets the
  // original text into |result().key()|.
  EXPECT_RESULT_AND_KEY(kInput, kInput, command);
}

TEST_F(SessionTest, SecondEscapeFromConvertReverseKeepsOriginalText) {
  // Second escape from ConvertReverse should restore the original text
  // without any text normalization even if the input text contains any
  // special characters which Mozc usually do normalization.
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  constexpr absl::string_view kInput = "ゔ";

  commands::Command command;
  SetupCommandForReverseConversion(kInput, command.mutable_input());
  SetupMockForReverseConversion(kInput, kInput, converter.get());

  EXPECT_TRUE(session.SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(GetComposition(command), kInput);

  SendKey("ESC", &session, &command);
  SendKey("ESC", &session, &command);

  EXPECT_FALSE(command.output().has_preedit());

  // When a reverse conversion is canceled, the converter sets the
  // original text into |result().key()|.
  EXPECT_RESULT_AND_KEY(kInput, kInput, command);
}

TEST_F(SessionTest, EscapeFromCompositionAfterConvertReverse) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  constexpr absl::string_view kKanjiAiueo = "阿伊宇江於";

  commands::Command command;
  SetupCommandForReverseConversion(kKanjiAiueo, command.mutable_input());
  SetupMockForReverseConversion(kKanjiAiueo, "あいうえお", converter.get());

  // Conversion Reverse
  EXPECT_TRUE(session.SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(GetComposition(command), kKanjiAiueo);

  session.Commit(&command);

  EXPECT_RESULT(kKanjiAiueo, command);

  // Escape in composition state
  SendKey("a", &session, &command);
  EXPECT_EQ(GetComposition(command), "あ");

  SendKey("ESC", &session, &command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, ConvertReverseFromOffState) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  const std::string kanji_aiueo = "阿伊宇江於";

  // IMEOff
  commands::Command command;
  SendSpecialKey(commands::KeyEvent::OFF, &session, &command);

  SetupCommandForReverseConversion(kanji_aiueo, command.mutable_input());
  SetupMockForReverseConversion(kanji_aiueo, "あいうえお", converter.get());
  EXPECT_TRUE(session.SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
}

TEST_F(SessionTest, DCHECKFailureAfterConvertReverse) {
  // This is a unittest against http://b/5145295.
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  SetupCommandForReverseConversion("あいうえお", command.mutable_input());
  SetupMockForReverseConversion("あいうえお", "あいうえお", converter.get());
  EXPECT_TRUE(session.SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あいうえお");
  EXPECT_EQ(command.output().all_candidate_words().candidates(0).value(),
            "あいうえお");
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_GT(command.output().candidate_window().candidate_size(), 0);

  SendKey("ESC", &session, &command);
  SendKey("a", &session, &command);
  EXPECT_EQ(command.output().preedit().segment(0).value(), "あいうえおあ");
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, LaunchTool) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);

  {
    commands::Command command;
    EXPECT_TRUE(session.LaunchConfigDialog(&command));
    EXPECT_EQ(command.output().launch_tool_mode(),
              commands::Output::CONFIG_DIALOG);
    EXPECT_TRUE(command.output().consumed());
  }

  {
    commands::Command command;
    EXPECT_TRUE(session.LaunchDictionaryTool(&command));
    EXPECT_EQ(command.output().launch_tool_mode(),
              commands::Output::DICTIONARY_TOOL);
    EXPECT_TRUE(command.output().consumed());
  }

  {
    commands::Command command;
    EXPECT_TRUE(session.LaunchWordRegisterDialog(&command));
    EXPECT_EQ(command.output().launch_tool_mode(),
              commands::Output::WORD_REGISTER_DIALOG);
    EXPECT_TRUE(command.output().consumed());
  }
}

TEST_F(SessionTest, NotZeroQuerySuggest) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Disable zero query suggest.
  commands::Request request;
  request.set_zero_query_suggestion(false);
  session.SetRequest(request);

  // Type "google".
  commands::Command command;
  InsertCharacterChars("google", &session, &command);
  EXPECT_EQ(GetComposition(command), "google");

  // Set up a mock suggestion result.
  Segments segments;
  Segment *segment;
  segment = segments.add_segment();
  segment->set_key("");
  segment->add_candidate()->value = "search";
  segment->add_candidate()->value = "input";

  // Commit composition and zero query suggest should not be invoked.
  EXPECT_CALL(*converter, StartPrediction(_, _)).Times(0);
  command.Clear();
  session.Commit(&command);
  EXPECT_EQ(command.output().result().value(), "google");
  EXPECT_EQ(GetComposition(command), "");
  EXPECT_FALSE(command.output().has_candidate_window());

  const ImeContext &context = session.context();
  EXPECT_EQ(context.state(), ImeContext::PRECOMPOSITION);
}

TEST_F(SessionTest, ZeroQuerySuggest) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);
  {  // Commit
    Session session(engine);
    commands::Request request;
    SetupZeroQuerySuggestionReady(true, &session, &request, converter.get());

    commands::Command command;
    session.Commit(&command);
    EXPECT_EQ(command.output().result().value(), "GOOGLE");
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_TRUE(command.output().has_candidate_window());
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
    EXPECT_EQ(command.output().candidate_window().candidate(0).value(),
              "search");
    EXPECT_EQ(command.output().candidate_window().candidate(1).value(),
              "input");
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {  // CommitSegment
    Session session(engine);
    commands::Request request;
    SetupZeroQuerySuggestionReady(true, &session, &request, converter.get());

    commands::Command command;
    session.CommitSegment(&command);
    EXPECT_EQ(command.output().result().value(), "GOOGLE");
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_TRUE(command.output().has_candidate_window());
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
    EXPECT_EQ(command.output().candidate_window().candidate(0).value(),
              "search");
    EXPECT_EQ(command.output().candidate_window().candidate(1).value(),
              "input");
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {  // CommitCandidate
    Session session(engine);
    commands::Request request;
    SetupZeroQuerySuggestionReady(true, &session, &request, converter.get());

    commands::Command command;
    SetSendCommandCommand(commands::SessionCommand::SUBMIT_CANDIDATE, &command);
    command.mutable_input()->mutable_command()->set_id(0);
    session.SendCommand(&command);

    EXPECT_EQ(command.output().result().value(), "GOOGLE");
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_TRUE(command.output().has_candidate_window());
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
    EXPECT_EQ(command.output().candidate_window().candidate(0).value(),
              "search");
    EXPECT_EQ(command.output().candidate_window().candidate(1).value(),
              "input");
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
    Mock::VerifyAndClearExpectations(converter.get());
  }

  {  // CommitFirstSuggestion
    Session session(engine);
    InitSessionToPrecomposition(&session);

    // Enable zero query suggest.
    commands::Request request;
    request.set_zero_query_suggestion(true);
    session.SetRequest(request);

    // Type "g".
    commands::Command command;
    InsertCharacterChars("g", &session, &command);

    {
      // Set up a mock conversion result.
      Segments segments;
      Segment *segment;
      segment = segments.add_segment();
      segment->set_key("");
      segment->add_candidate()->value = "google";
      EXPECT_CALL(*converter, StartPrediction(_, _))
          .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    }

    command.Clear();
    InsertCharacterChars("o", &session, &command);

    {
      // Set up a mock suggestion result.
      Segments segments;
      Segment *segment;
      segment = segments.add_segment();
      segment->set_key("");
      segment->add_candidate()->value = "search";
      segment->add_candidate()->value = "input";
      EXPECT_CALL(*converter, StartPrediction(_, _))
          .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    }

    command.Clear();
    session.CommitFirstSuggestion(&command);
    EXPECT_EQ(command.output().result().value(), "google");
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_TRUE(command.output().has_candidate_window());
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
    EXPECT_EQ(command.output().candidate_window().candidate(0).value(),
              "search");
    EXPECT_EQ(command.output().candidate_window().candidate(1).value(),
              "input");
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
  }
}

TEST_F(SessionTest, CommandsAfterZeroQuerySuggest) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {  // Cancel command should close the candidate window.
    Session session(engine);
    commands::Request request;
    commands::Command command;
    SetupZeroQuerySuggestion(&session, &request, &command, converter.get());

    command.Clear();
    session.EditCancel(&command);
    EXPECT_TRUE(command.output().consumed());
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
  }

  {  // PredictAndConvert should select the first candidate.
    Session session(engine);
    commands::Request request;
    commands::Command command;
    SetupZeroQuerySuggestion(&session, &request, &command, converter.get());

    command.Clear();
    session.PredictAndConvert(&command);
    EXPECT_TRUE(command.output().consumed());
    EXPECT_FALSE(command.output().has_result());
    // "search" is the first suggest candidate.
    EXPECT_PREEDIT("search", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);
  }

  {  // CommitFirstSuggestion should insert the first candidate.
    Session session(engine);
    commands::Request request;
    commands::Command command;
    SetupZeroQuerySuggestion(&session, &request, &command, converter.get());

    command.Clear();
    // FinishConversion is expected to return empty Segments.
    EXPECT_CALL(*converter, FinishConversion(_, _))
        .WillRepeatedly(SetArgPointee<1>(Segments()));
    session.CommitFirstSuggestion(&command);
    EXPECT_TRUE(command.output().consumed());
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_EQ(GetComposition(command), "");
    // "search" is the first suggest candidate.
    EXPECT_RESULT("search", command);
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
  }

  {  // Space should be inserted directly.
    Session session(engine);
    commands::Request request;
    commands::Command command;
    SetupZeroQuerySuggestion(&session, &request, &command, converter.get());

    SendKey("Space", &session, &command);
    EXPECT_TRUE(command.output().consumed());
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_RESULT("　", command);  // Full-width space
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
  }

  {  // 'a' should be inserted in the composition.
    Session session(engine);
    commands::Request request;
    commands::Command command;
    SetupZeroQuerySuggestion(&session, &request, &command, converter.get());
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);

    SendKey("a", &session, &command);
    EXPECT_TRUE(command.output().consumed());
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(command.output().mode(), commands::HIRAGANA);
    EXPECT_PREEDIT("あ", command);
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
  }

  {  // Enter should be inserted directly.
    Session session(engine);
    commands::Request request;
    commands::Command command;
    SetupZeroQuerySuggestion(&session, &request, &command, converter.get());

    SendKey("Enter", &session, &command);
    EXPECT_FALSE(command.output().consumed());
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
  }

  {  // Right should be inserted directly.
    Session session(engine);
    commands::Request request;
    commands::Command command;
    SetupZeroQuerySuggestion(&session, &request, &command, converter.get());

    SendKey("Right", &session, &command);
    EXPECT_FALSE(command.output().consumed());
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
  }

  {  // SelectCnadidate command should work with zero query suggestion.
    Session session(engine);
    commands::Request request;
    commands::Command command;
    SetupZeroQuerySuggestion(&session, &request, &command, converter.get());

    // Send SELECT_CANDIDATE command.
    const int first_id = command.output().candidate_window().candidate(0).id();
    SetSendCommandCommand(commands::SessionCommand::SELECT_CANDIDATE, &command);
    command.mutable_input()->mutable_command()->set_id(first_id);
    EXPECT_TRUE(session.SendCommand(&command));

    EXPECT_TRUE(command.output().consumed());
    EXPECT_FALSE(command.output().has_result());
    // "search" is the first suggest candidate.
    EXPECT_PREEDIT("search", command);
    EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);
  }
}

TEST_F(SessionTest, Issue4437420) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  commands::Request request;
  // Creates overriding config.
  config::Config overriding_config;
  overriding_config.set_session_keymap(config::Config::MOBILE);
  // Change to 12keys-halfascii mode.
  SwitchInputMode(commands::HALF_ASCII, &session);

  command.Clear();
  request.set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII);
  session.SetRequest(request);
  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(request,
                                        config::ConfigHandler::DefaultConfig());
  session.SetTable(table);
  // Type "2*" to produce "A".
  SetSendKeyCommand("2", &command);
  *command.mutable_input()->mutable_config() = overriding_config;
  session.SendKey(&command);
  SetSendKeyCommand("*", &command);
  *command.mutable_input()->mutable_config() = overriding_config;
  session.SendKey(&command);
  EXPECT_EQ(GetComposition(command), "A");

  // Change to 12keys-halfascii mode.
  SwitchInputMode(commands::HALF_ASCII, &session);

  command.Clear();
  request.set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII);
  session.SetRequest(request);
  table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(request,
                                        config::ConfigHandler::DefaultConfig());
  session.SetTable(table);
  // Type "2" to produce "Aa".
  SetSendKeyCommand("2", &command);
  *command.mutable_input()->mutable_config() = overriding_config;
  session.SendKey(&command);
  EXPECT_EQ(GetComposition(command), "Aa");
  command.Clear();
}

// If undo context is empty, key event for UNDO should be echoed back. b/5553298
TEST_F(SessionTest, Issue5553298) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session.set_client_capability(capability);

  commands::Command command;
  session.ResetContext(&command);

  SetSendKeyCommand("Ctrl Backspace", &command);
  command.mutable_input()->mutable_config()->set_session_keymap(
      config::Config::MSIME);
  session.TestSendKey(&command);
  EXPECT_FALSE(command.output().consumed());

  SetSendKeyCommand("Ctrl Backspace", &command);
  command.mutable_input()->mutable_config()->set_session_keymap(
      config::Config::MSIME);
  session.SendKey(&command);
  EXPECT_FALSE(command.output().consumed());
}

TEST_F(SessionTest, UndoKeyAction) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  commands::Command command;
  commands::Request request;
  // Creates overriding config.
  config::Config overriding_config;
  overriding_config.set_session_keymap(config::Config::MOBILE);
  // Test in half width ascii mode.
  {
    Session session(engine);
    InitSessionToPrecomposition(&session);

    // Change to 12keys-halfascii mode.
    SwitchInputMode(commands::HALF_ASCII, &session);

    command.Clear();
    request.set_special_romanji_table(
        commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII);
    session.SetRequest(request);
    auto table = std::make_shared<composer::Table>();
    table->InitializeWithRequestAndConfig(
        request, config::ConfigHandler::DefaultConfig());
    session.SetTable(table);

    // Type "2" to produce "a".
    SetSendKeyCommand("2", &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendKey(&command);
    EXPECT_EQ(GetComposition(command), "a");

    // Type "2" again to produce "b".
    SetSendKeyCommand("2", &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendKey(&command);
    EXPECT_EQ(GetComposition(command), "b");

    // Push UNDO key to reproduce "a".
    SetSendCommandCommand(commands::SessionCommand::UNDO_OR_REWIND, &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendCommand(&command);
    EXPECT_EQ(GetComposition(command), "a");
    EXPECT_TRUE(command.output().consumed());

    // Push UNDO key again to produce "2".
    SetSendCommandCommand(commands::SessionCommand::UNDO_OR_REWIND, &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendCommand(&command);
    EXPECT_EQ(GetComposition(command), "2");
    EXPECT_TRUE(command.output().consumed());
    command.Clear();
  }

  // Test in Hiaragana-mode.
  {
    Session session(engine);
    InitSessionToPrecomposition(&session);

    // Change to 12keys-Hiragana mode.
    SwitchInputMode(commands::HIRAGANA, &session);

    command.Clear();
    request.set_special_romanji_table(
        commands::Request::TWELVE_KEYS_TO_HIRAGANA);
    session.SetRequest(request);
    auto table = std::make_shared<composer::Table>();
    table->InitializeWithRequestAndConfig(
        request, config::ConfigHandler::DefaultConfig());
    session.SetTable(table);
    // Type "33{<}{<}" to produce "さ"->"し"->"さ"->"そ".
    SetSendKeyCommand("3", &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendKey(&command);
    EXPECT_EQ(GetComposition(command), "さ");

    SetSendKeyCommand("3", &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendKey(&command);
    EXPECT_EQ(GetComposition(command), "し");

    SetSendCommandCommand(commands::SessionCommand::UNDO_OR_REWIND, &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendCommand(&command);
    EXPECT_EQ(GetComposition(command), "さ");
    EXPECT_TRUE(command.output().consumed());
    command.Clear();

    SetSendCommandCommand(commands::SessionCommand::UNDO_OR_REWIND, &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendCommand(&command);
    EXPECT_EQ(GetComposition(command), "そ");
    EXPECT_TRUE(command.output().consumed());
    command.Clear();
  }

  // Test to do nothing for voiced sounds.
  {
    Session session(engine);
    InitSessionToPrecomposition(&session);

    // Change to 12keys-Hiragana mode.
    SwitchInputMode(commands::HIRAGANA, &session);

    command.Clear();
    request.set_special_romanji_table(
        commands::Request::TWELVE_KEYS_TO_HIRAGANA);
    session.SetRequest(request);
    auto table = std::make_shared<composer::Table>();
    table->InitializeWithRequestAndConfig(
        request, config::ConfigHandler::DefaultConfig());
    session.SetTable(table);
    // Type "3*{<}*{<}", and composition should change
    // "さ"->"ざ"->(No change)->"さ"->(No change).
    SetSendKeyCommand("3", &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendKey(&command);
    EXPECT_EQ(GetComposition(command), "さ");

    SetSendKeyCommand("*", &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendKey(&command);
    EXPECT_EQ(GetComposition(command), "ざ");

    SetSendCommandCommand(commands::SessionCommand::UNDO_OR_REWIND, &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendCommand(&command);
    EXPECT_EQ(GetComposition(command), "ざ");
    EXPECT_TRUE(command.output().consumed());

    SetSendKeyCommand("*", &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendKey(&command);
    EXPECT_EQ(GetComposition(command), "さ");
    command.Clear();

    SetSendCommandCommand(commands::SessionCommand::UNDO_OR_REWIND, &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendCommand(&command);
    EXPECT_EQ(GetComposition(command), "さ");
    EXPECT_TRUE(command.output().consumed());
    command.Clear();
  }

  // Test to make nothing newly in preedit for empty composition.
  {
    Session session(engine);
    InitSessionToPrecomposition(&session);

    // Change to 12keys-Hiragana mode.
    SwitchInputMode(commands::HIRAGANA, &session);

    command.Clear();
    request.set_special_romanji_table(
        commands::Request::TWELVE_KEYS_TO_HIRAGANA);
    session.SetRequest(request);
    auto table = std::make_shared<composer::Table>();
    table->InitializeWithRequestAndConfig(
        request, config::ConfigHandler::DefaultConfig());
    session.SetTable(table);
    // Type "{<}" and do nothing
    SetSendCommandCommand(commands::SessionCommand::UNDO_OR_REWIND, &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendCommand(&command);

    EXPECT_FALSE(command.output().has_preedit());

    command.Clear();
  }

  // Test of acting as UNDO key. Almost same as the first section in Undo test.
  {
    Session session(engine);
    InitSessionToPrecomposition(&session);

    commands::Capability capability;
    capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
    session.set_client_capability(capability);

    Segments segments;
    InsertCharacterChars("aiueo", &session, &command);
    SetAiueo(&segments);
    Segment::Candidate *candidate;
    candidate = segments.mutable_segment(0)->add_candidate();
    candidate->value = "aiueo";
    candidate = segments.mutable_segment(0)->add_candidate();
    candidate->value = "AIUEO";

    EXPECT_CALL(*converter, StartConversion(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    command.Clear();
    session.Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_PREEDIT("あいうえお", command);

    EXPECT_CALL(*converter, CommitSegmentValue(_, _, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
    command.Clear();
    session.Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_RESULT("あいうえお", command);

    command.Clear();
    SetSendCommandCommand(commands::SessionCommand::UNDO_OR_REWIND, &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendCommand(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(command.output().deletion_range().offset(), -5);
    EXPECT_EQ(command.output().deletion_range().length(), 5);
    EXPECT_PREEDIT("あいうえお", command);
    EXPECT_TRUE(command.output().consumed());

    // Undo twice - do nothing and don't cosume the input.
    command.Clear();
    SetSendCommandCommand(commands::SessionCommand::UNDO_OR_REWIND, &command);
    session.SendCommand(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_deletion_range());
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_FALSE(command.output().consumed());
  }

  // Do not UNDO even if UNDO stack is not empty if it is in COMPOSITE state.
  {
    Session session(engine);
    InitSessionToPrecomposition(&session);

    // Change to 12keys-Hiragana mode.
    SwitchInputMode(commands::HIRAGANA, &session);

    command.Clear();
    request.set_special_romanji_table(
        commands::Request::TWELVE_KEYS_TO_HIRAGANA);
    session.SetRequest(request);
    auto table = std::make_shared<composer::Table>();
    table->InitializeWithRequestAndConfig(
        request, config::ConfigHandler::DefaultConfig());
    session.SetTable(table);

    // commit "あ" to push UNDO stack
    SetSendKeyCommand("1", &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendKey(&command);
    EXPECT_EQ(GetComposition(command), "あ");
    command.Clear();

    session.Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_RESULT("あ", command);

    // Produce "か" in composition.
    SetSendKeyCommand("2", &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendKey(&command);
    EXPECT_EQ(GetComposition(command), "か");
    EXPECT_TRUE(command.output().consumed());
    command.Clear();

    // Send UNDO_OR_REWIND key, then get "こ" in composition
    SetSendCommandCommand(commands::SessionCommand::UNDO_OR_REWIND, &command);
    *command.mutable_input()->mutable_config() = overriding_config;
    session.SendCommand(&command);
    EXPECT_PREEDIT("こ", command);
    EXPECT_TRUE(command.output().consumed());
    command.Clear();
  }
}

TEST_F(SessionTest, DedupAfterUndo) {
  commands::Command command;
  {
    Session session(*mock_data_engine_);
    InitSessionToPrecomposition(&session, *mobile_request_);

    // Undo requires capability DELETE_PRECEDING_TEXT.
    commands::Capability capability;
    capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
    session.set_client_capability(capability);

    SwitchInputMode(commands::HIRAGANA, &session);

    commands::Request request(*mobile_request_);
    request.set_special_romanji_table(
        commands::Request::TWELVE_KEYS_TO_HIRAGANA);
    session.SetRequest(request);

    auto table = std::make_shared<composer::Table>();
    table->InitializeWithRequestAndConfig(
        request, config::ConfigHandler::DefaultConfig());
    session.SetTable(table);

    // Type "!" to produce "！".
    SetSendKeyCommand("!", &command);
    session.SendKey(&command);
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
    EXPECT_EQ(GetComposition(command), "！");

    ASSERT_TRUE(command.output().has_candidate_window());

    std::vector<int> ids;
    FindCandidateIDs(command.output().candidate_window(), "！", &ids);
    EXPECT_GE(1, ids.size());

    FindCandidateIDs(command.output().candidate_window(), "!", &ids);
    EXPECT_GE(1, ids.size());

    const int candidate_size_before_undo =
        command.output().candidate_window().candidate_size();

    command.Clear();
    session.CommitFirstSuggestion(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);

    command.Clear();
    session.Undo(&command);
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
    EXPECT_TRUE(command.output().has_deletion_range());
    ASSERT_TRUE(command.output().has_candidate_window());

    FindCandidateIDs(command.output().candidate_window(), "！", &ids);
    EXPECT_GE(1, ids.size());

    FindCandidateIDs(command.output().candidate_window(), "!", &ids);
    EXPECT_GE(1, ids.size());

    EXPECT_EQ(candidate_size_before_undo,
              command.output().candidate_window().candidate_size());
  }
}

TEST_F(SessionTest, MoveCursor) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  InsertCharacterChars("MOZUKU", &session, &command);
  EXPECT_EQ(command.output().preedit().cursor(), 6);
  session.MoveCursorLeft(&command);
  EXPECT_EQ(command.output().preedit().cursor(), 5);
  command.mutable_input()->mutable_command()->set_cursor_position(3);
  session.MoveCursorTo(&command);
  EXPECT_EQ(command.output().preedit().cursor(), 3);
  session.MoveCursorRight(&command);
  EXPECT_EQ(command.output().preedit().cursor(), 4);
}

TEST_F(SessionTest, MoveCursorPrecomposition) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;

  command.mutable_input()->mutable_command()->set_cursor_position(3);
  session.MoveCursorTo(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().consumed());
}

TEST_F(SessionTest, MoveCursorRightWithCommit) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  commands::Request request;
  request = *mobile_request_;
  request.set_special_romanji_table(
      commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII);
  request.set_crossing_edge_behavior(
      commands::Request::COMMIT_WITHOUT_CONSUMING);
  InitSessionToPrecomposition(&session, request);
  commands::Command command;

  InsertCharacterChars("MOZC", &session, &command);
  EXPECT_EQ(command.output().preedit().cursor(), 4);
  command.Clear();
  session.MoveCursorLeft(&command);
  EXPECT_EQ(command.output().preedit().cursor(), 3);
  command.Clear();
  session.MoveCursorRight(&command);
  EXPECT_EQ(command.output().preedit().cursor(), 4);
  command.Clear();
  session.MoveCursorRight(&command);
  EXPECT_FALSE(command.output().consumed());
  ASSERT_TRUE(command.output().has_result());
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "MOZC");
  EXPECT_EQ(command.output().result().cursor_offset(), 0);
}

TEST_F(SessionTest, MoveCursorLeftWithCommit) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  commands::Request request;
  request = *mobile_request_;
  request.set_special_romanji_table(
      commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII);
  request.set_crossing_edge_behavior(
      commands::Request::COMMIT_WITHOUT_CONSUMING);
  InitSessionToPrecomposition(&session, request);
  commands::Command command;

  InsertCharacterChars("MOZC", &session, &command);
  EXPECT_EQ(command.output().preedit().cursor(), 4);
  command.Clear();
  session.MoveCursorLeft(&command);
  EXPECT_EQ(command.output().preedit().cursor(), 3);
  command.Clear();
  session.MoveCursorLeft(&command);
  EXPECT_EQ(command.output().preedit().cursor(), 2);
  command.Clear();
  session.MoveCursorLeft(&command);
  EXPECT_EQ(command.output().preedit().cursor(), 1);
  command.Clear();
  session.MoveCursorLeft(&command);
  EXPECT_EQ(command.output().preedit().cursor(), 0);
  command.Clear();

  session.MoveCursorLeft(&command);
  EXPECT_FALSE(command.output().consumed());
  ASSERT_TRUE(command.output().has_result());
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "MOZC");
  EXPECT_EQ(command.output().result().cursor_offset(), -4);
}

TEST_F(SessionTest, MoveCursorRightWithCommitWithZeroQuerySuggestion) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  commands::Request request(*mobile_request_);
  request.set_special_romanji_table(
      commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII);
  request.set_crossing_edge_behavior(
      commands::Request::COMMIT_WITHOUT_CONSUMING);
  SetupZeroQuerySuggestionReady(true, &session, &request, converter.get());
  commands::Command command;

  InsertCharacterChars("GOOGLE", &session, &command);
  EXPECT_EQ(command.output().preedit().cursor(), 6);
  command.Clear();

  session.MoveCursorRight(&command);
  EXPECT_FALSE(command.output().consumed());
  ASSERT_TRUE(command.output().has_result());
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "GOOGLE");
  EXPECT_EQ(command.output().result().cursor_offset(), 0);
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
}

TEST_F(SessionTest, MoveCursorLeftWithCommitWithZeroQuerySuggestion) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  commands::Request request(*mobile_request_);
  request.set_special_romanji_table(
      commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII);
  request.set_crossing_edge_behavior(
      commands::Request::COMMIT_WITHOUT_CONSUMING);
  SetupZeroQuerySuggestionReady(true, &session, &request, converter.get());
  commands::Command command;

  InsertCharacterChars("GOOGLE", &session, &command);
  EXPECT_EQ(command.output().preedit().cursor(), 6);
  command.Clear();
  for (int i = 5; i >= 0; --i) {
    session.MoveCursorLeft(&command);
    EXPECT_EQ(command.output().preedit().cursor(), i);
    command.Clear();
  }

  session.MoveCursorLeft(&command);
  EXPECT_FALSE(command.output().consumed());
  ASSERT_TRUE(command.output().has_result());
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "GOOGLE");
  EXPECT_EQ(command.output().result().cursor_offset(), -6);
  EXPECT_FALSE(command.output().has_candidate_window());
}

TEST_F(SessionTest, CommitHead) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  auto table = std::make_shared<composer::Table>();
  table->AddRule("mo", "も", "");
  table->AddRule("zu", "ず", "");

  session.get_internal_composer_only_for_unittest()->SetTable(table);

  InitSessionToPrecomposition(&session);
  commands::Command command;

  InsertCharacterChars("moz", &session, &command);
  EXPECT_EQ(GetComposition(command), "もｚ");
  command.Clear();
  session.CommitHead(1, &command);
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "も");
  EXPECT_EQ(GetComposition(command), "ｚ");
  InsertCharacterChars("u", &session, &command);
  EXPECT_EQ(GetComposition(command), "ず");
}

TEST_F(SessionTest, PasswordWithToggleAlphabetInput) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);

  commands::Request request;
  request = *mobile_request_;
  request.set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII);

  InitSessionToPrecomposition(&session, request);

  // Change to 12keys-halfascii mode.
  SwitchInputFieldType(commands::Context::PASSWORD, &session);
  SwitchInputMode(commands::HALF_ASCII, &session);

  commands::Command command;
  SendKey("2", &session, &command);
  EXPECT_EQ(GetComposition(command), "a");
  EXPECT_EQ(command.output().preedit().cursor(), 1);

  SendKey("2", &session, &command);
  EXPECT_EQ(GetComposition(command), "b");
  EXPECT_EQ(command.output().preedit().cursor(), 1);

  // cursor key commits the preedit.
  SendKey("right", &session, &command);
  // "b"
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "b");
  EXPECT_EQ(GetComposition(command), "");
  EXPECT_EQ(command.output().preedit().cursor(), 0);

  SendKey("2", &session, &command);
  // "b[a]"
  EXPECT_EQ(command.output().result().type(), commands::Result::NONE);
  EXPECT_EQ(GetComposition(command), "a");
  EXPECT_EQ(command.output().preedit().cursor(), 1);

  SendKey("4", &session, &command);
  // ba[g]
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "a");
  EXPECT_EQ(GetComposition(command), "g");
  EXPECT_EQ(command.output().preedit().cursor(), 1);

  // cursor key commits the preedit.
  SendKey("left", &session, &command);
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "g");
  EXPECT_EQ(command.output().preedit().segment_size(), 0);
  EXPECT_EQ(command.output().preedit().cursor(), 0);
}

TEST_F(SessionTest, SwitchInputFieldType) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  // initial state is NORMAL
  EXPECT_EQ(session.context().composer().GetInputFieldType(),
            commands::Context::NORMAL);

  {
    SCOPED_TRACE("Switch input field type to PASSWORD");
    SwitchInputFieldType(commands::Context::PASSWORD, &session);
  }
  {
    SCOPED_TRACE("Switch input field type to NORMAL");
    SwitchInputFieldType(commands::Context::NORMAL, &session);
  }
}

TEST_F(SessionTest, CursorKeysInPasswordMode) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);

  commands::Request request;
  request = *mobile_request_;
  request.set_special_romanji_table(commands::Request::DEFAULT_TABLE);
  session.SetRequest(request);

  InitSessionToPrecomposition(&session, request);

  SwitchInputFieldType(commands::Context::PASSWORD, &session);
  SwitchInputMode(commands::HALF_ASCII, &session);

  commands::Command command;
  // cursor key commits the preedit without moving system cursor.
  SendKey("m", &session, &command);
  EXPECT_EQ(command.output().result().type(), commands::Result::NONE);
  command.Clear();
  session.MoveCursorLeft(&command);
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "m");
  EXPECT_EQ(GetComposition(command), "");
  MOZC_VLOG(0) << command;
  EXPECT_EQ(command.output().preedit().cursor(), 0);
  EXPECT_TRUE(command.output().consumed());

  SendKey("o", &session, &command);
  EXPECT_EQ(command.output().result().type(), commands::Result::NONE);
  command.Clear();
  session.MoveCursorRight(&command);
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "o");
  EXPECT_EQ(GetComposition(command), "");
  EXPECT_EQ(command.output().preedit().cursor(), 0);
  EXPECT_TRUE(command.output().consumed());

  SendKey("z", &session, &command);
  EXPECT_EQ(command.output().result().type(), commands::Result::NONE);
  SetSendCommandCommand(commands::SessionCommand::MOVE_CURSOR, &command);
  command.mutable_input()->mutable_command()->set_cursor_position(3);
  session.MoveCursorTo(&command);
  EXPECT_EQ(command.output().result().value(), "z");
  EXPECT_EQ(GetComposition(command), "");
  EXPECT_EQ(command.output().preedit().cursor(), 0);
  EXPECT_TRUE(command.output().consumed());
}

TEST_F(SessionTest, BackKeyCommitsPreeditInPasswordMode) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);
  commands::Command command;
  commands::Request request;

  request.set_zero_query_suggestion(false);
  request.set_special_romanji_table(commands::Request::DEFAULT_TABLE);
  session.SetRequest(request);

  auto table = std::make_shared<composer::Table>();
  table->InitializeWithRequestAndConfig(request,
                                        config::ConfigHandler::DefaultConfig());
  session.SetTable(table);

  SwitchInputFieldType(commands::Context::PASSWORD, &session);
  SwitchInputMode(commands::HALF_ASCII, &session);

  SendKey("m", &session, &command);
  EXPECT_EQ(command.output().result().type(), commands::Result::NONE);
  EXPECT_EQ(GetComposition(command), "m");
  SendKey("esc", &session, &command);
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "m");
  EXPECT_EQ(GetComposition(command), "");
  EXPECT_FALSE(command.output().consumed());

  SendKey("o", &session, &command);
  SendKey("z", &session, &command);
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "o");
  EXPECT_EQ(GetComposition(command), "z");
  SendKey("esc", &session, &command);
  EXPECT_EQ(command.output().result().type(), commands::Result::STRING);
  EXPECT_EQ(command.output().result().value(), "z");
  EXPECT_EQ(GetComposition(command), "");
  EXPECT_FALSE(command.output().consumed());

  // in normal mode, preedit is cleared without commit.
  SwitchInputFieldType(commands::Context::NORMAL, &session);

  SendKey("m", &session, &command);
  EXPECT_EQ(command.output().result().type(), commands::Result::NONE);
  EXPECT_EQ(GetComposition(command), "m");
  SendKey("esc", &session, &command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(command.output().result().type(), commands::Result::NONE);
  EXPECT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, EditCancel) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  Segments segments_mo;
  {
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  {  // Cancel of Suggestion
    commands::Command command;
    SendKey("M", &session, &command);

    EXPECT_CALL(*converter, StartPrediction(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
    SendKey("O", &session, &command);
    ASSERT_TRUE(command.output().has_candidate_window());
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
    EXPECT_EQ(command.output().candidate_window().candidate(0).value(),
              "MOCHA");

    command.Clear();
    session.EditCancel(&command);
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 0);
    EXPECT_FALSE(command.output().has_result());
  }

  {  // Cancel of Reverse conversion
    commands::Command command;

    // "[MO]" is a converted string like Kanji.
    // "MO" is an input string like Hiragana.
    SetupCommandForReverseConversion("[MO]", command.mutable_input());
    SetupMockForReverseConversion("[MO]", "MO", converter.get());
    EXPECT_TRUE(session.SendCommand(&command));

    command.Clear();
    EXPECT_CALL(*converter, StartPrediction(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
    session.ConvertCancel(&command);
    ASSERT_TRUE(command.output().has_candidate_window());
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
    EXPECT_EQ(command.output().candidate_window().candidate(0).value(),
              "MOCHA");

    command.Clear();
    session.EditCancel(&command);
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 0);
    // test case against b/5566728
    EXPECT_RESULT("[MO]", command);
  }
}

TEST_F(SessionTest, ImeOff) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);

  EXPECT_CALL(*converter, ResetConversion(_));
  InitSessionToPrecomposition(&session);
  commands::Command command;
  session.IMEOff(&command);
}

TEST_F(SessionTest, EditCancelAndIMEOff) {
  config::Config config;
  {
    constexpr absl::string_view kCustomKeymapTable =
        "status\tkey\tcommand\n"
        "Precomposition\thankaku/zenkaku\tCancelAndIMEOff\n"
        "Composition\thankaku/zenkaku\tCancelAndIMEOff\n"
        "Conversion\thankaku/zenkaku\tCancelAndIMEOff\n";
    config.set_session_keymap(config::Config::CUSTOM);
    config.set_custom_keymap_table(kCustomKeymapTable);
  }

  Segments segments_mo;
  {
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {  // Cancel of Precomposition and deactivate IME
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    EXPECT_TRUE(TestSendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 0);
    EXPECT_FALSE(command.output().has_result());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
  }

  {  // Cancel of Composition and deactivate IME
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    SendKey("M", &session, &command);

    EXPECT_TRUE(TestSendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 0);
    EXPECT_FALSE(command.output().has_result());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
  }

  {  // Cancel of Suggestion and deactivate IME
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;
    SendKey("M", &session, &command);

    EXPECT_CALL(*converter, StartPrediction(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
    SendKey("O", &session, &command);
    ASSERT_TRUE(command.output().has_candidate_window());
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
    EXPECT_EQ(command.output().candidate_window().candidate(0).value(),
              "MOCHA");

    EXPECT_TRUE(TestSendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 0);
    EXPECT_FALSE(command.output().has_result());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
  }

  {  // Cancel of Conversion and deactivate IME
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToConversionWithAiueo(&session, converter.get());

    commands::Command command;
    EXPECT_TRUE(TestSendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 0);
    EXPECT_FALSE(command.output().has_result());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
  }

  {  // Cancel of Reverse conversion and deactivate IME
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);

    commands::Command command;

    // "[MO]" is a converted string like Kanji.
    // "MO" is an input string like Hiragana.
    SetupCommandForReverseConversion("[MO]", command.mutable_input());
    SetupMockForReverseConversion("[MO]", "MO", converter.get());
    EXPECT_TRUE(session.SendCommand(&command));

    command.Clear();
    EXPECT_CALL(*converter, StartPrediction(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments_mo), Return(true)));
    session.ConvertCancel(&command);
    ASSERT_TRUE(command.output().has_candidate_window());
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 2);
    EXPECT_EQ(command.output().candidate_window().candidate(0).value(),
              "MOCHA");

    EXPECT_TRUE(TestSendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 0);
    EXPECT_RESULT("[MO]", command);
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
  }
}

// TODO(matsuzakit): Update the expected result when b/5955618 is fixed.
TEST_F(SessionTest, CancelInPasswordModeIssue5955618) {
  config::Config config;
  {
    constexpr absl::string_view kCustomKeymapTable =
        "status\tkey\tcommand\n"
        "Precomposition\tESC\tCancel\n"
        "Composition\tESC\tCancel\n"
        "Conversion\tESC\tCancel\n";
    config.set_session_keymap(config::Config::CUSTOM);
    config.set_custom_keymap_table(kCustomKeymapTable);
  }
  Segments segments_mo;
  {
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {  // Cancel of Precomposition in password field
     // Basically this is unusual because there is no character to be canceled
     // when Precomposition state.
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);
    SwitchInputFieldType(commands::Context::PASSWORD, &session);

    commands::Command command;
    EXPECT_TRUE(TestSendKey("ESC", &session, &command));
    EXPECT_TRUE(command.output().consumed());  // should be consumed, anyway.

    EXPECT_TRUE(SendKey("ESC", &session, &command));
    // This behavior is the bug of b/5955618.
    // The result of TestSendKey and SendKey should be the same in terms of
    // |consumed()|.
    EXPECT_FALSE(command.output().consumed())
        << "Congrats! b/5955618 seems to be fixed";
  }

  {  // Cancel of Composition in password field
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);
    SwitchInputFieldType(commands::Context::PASSWORD, &session);

    commands::Command command;
    EXPECT_TRUE(TestSendKey("ESC", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("ESC", &session, &command));
    // This behavior is the bug of b/5955618.
    // The result of TestSendKey and SendKey should be the same in terms of
    // |consumed()|.
    EXPECT_FALSE(command.output().consumed())
        << "Congrats! b/5955618 seems to be fixed";
  }

  {  // Cancel of Conversion in password field
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToConversionWithAiueo(&session, converter.get());
    SwitchInputFieldType(commands::Context::PASSWORD, &session);

    // Actually this works well because Cancel command in conversion mode
    // is mapped into ConvertCancel not EditCancel.
    commands::Command command;
    EXPECT_TRUE(TestSendKey("ESC", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(SendKey("ESC", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_FALSE(command.output().has_result());

    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
  }

  {  // Cancel of Reverse conversion in password field
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);
    SwitchInputFieldType(commands::Context::PASSWORD, &session);

    commands::Command command;

    // "[MO]" is a converted string like Kanji.
    // "MO" is an input string like Hiragana.
    SetupCommandForReverseConversion("[MO]", command.mutable_input());
    SetupMockForReverseConversion("[MO]", "MO", converter.get());
    EXPECT_TRUE(session.SendCommand(&command));

    // Actually this works well because Cancel command in conversion mode
    // is mapped into ConvertCancel not EditCancel.
    EXPECT_TRUE(TestSendKey("ESC", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(SendKey("ESC", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);

    // The second escape key will be mapped into EditCancel.
    EXPECT_TRUE(TestSendKey("ESC", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(SendKey("ESC", &session, &command));
    // This behavior is the bug of b/5955618.
    EXPECT_FALSE(command.output().consumed())
        << "Congrats! b/5955618 seems to be fixed";
    EXPECT_RESULT("[MO]", command);
  }
}

// TODO(matsuzakit): Update the expected result when b/5955618 is fixed.
TEST_F(SessionTest, CancelAndIMEOffInPasswordModeIssue5955618) {
  config::Config config;
  {
    constexpr absl::string_view kCustomKeymapTable =
        "status\tkey\tcommand\n"
        "Precomposition\thankaku/zenkaku\tCancelAndIMEOff\n"
        "Composition\thankaku/zenkaku\tCancelAndIMEOff\n"
        "Conversion\thankaku/zenkaku\tCancelAndIMEOff\n";
    config.set_session_keymap(config::Config::CUSTOM);
    config.set_custom_keymap_table(kCustomKeymapTable);
  }
  Segments segments_mo;
  {
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  {  // Cancel of Precomposition and deactivate IME in password field.
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);
    SwitchInputFieldType(commands::Context::PASSWORD, &session);

    commands::Command command;
    EXPECT_TRUE(TestSendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("hankaku/zenkaku", &session, &command));
    // This behavior is the bug of b/5955618.
    // The result of TestSendKey and SendKey should be the same in terms of
    // |consumed()|.
    EXPECT_FALSE(command.output().consumed())
        << "Congrats! b/5955618 seems to be fixed";
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 0);
    EXPECT_FALSE(command.output().has_result());
    // Current behavior seems to be a bug.
    // This command should deactivate the IME.
    ASSERT_FALSE(command.output().has_status())
        << "Congrats! b/5955618 seems to be fixed.";
    // Ideally the following condition should be satisfied.
    // EXPECT_FALSE(command.output().status().activated());
  }

  {  // Cancel of Composition and deactivate IME in password field
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);
    SwitchInputFieldType(commands::Context::PASSWORD, &session);

    commands::Command command;
    EXPECT_TRUE(TestSendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());

    EXPECT_TRUE(SendKey("hankaku/zenkaku", &session, &command));
    // This behavior is the bug of b/5955618.
    // The result of TestSendKey and SendKey should be the same in terms of
    // |consumed()|.
    EXPECT_FALSE(command.output().consumed())
        << "Congrats! b/5955618 seems to be fixed";
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 0);
    EXPECT_FALSE(command.output().has_result());
    // Following behavior seems to be a bug.
    // This command should deactivate the IME.
    ASSERT_FALSE(command.output().has_status())
        << "Congrats! b/5955618 seems to be fixed.";
    // Ideally the following condition should be satisfied.
    // EXPECT_FALSE(command.output().status().activated());
  }

  {  // Cancel of Conversion and deactivate IME in password field
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToConversionWithAiueo(&session, converter.get());
    SwitchInputFieldType(commands::Context::PASSWORD, &session);

    commands::Command command;
    EXPECT_TRUE(TestSendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    command.Clear();
    // This behavior is the bug of b/5955618.
    // The result of TestSendKey and SendKey should be the same in terms of
    // |consumed()|.
    EXPECT_FALSE(command.output().consumed())
        << "Congrats! b/5955618 seems to be fixed";
    EXPECT_EQ(GetComposition(command), "");
    EXPECT_EQ(command.output().candidate_window().candidate_size(), 0);
    EXPECT_FALSE(command.output().has_result());
    // Following behavior seems to be a bug.
    // This command should deactivate the IME.
    ASSERT_FALSE(command.output().has_status())
        << "Congrats! b/5955618 seems to be fixed.";
    // Ideally the following condition should be satisfied.
    // EXPECT_FALSE(command.output().status().activated());
  }

  {  // Cancel of Reverse conversion and deactivate IME in password field
    Session session(engine);
    auto key_map_manager = std::make_shared<keymap::KeyMapManager>(config);
    session.SetConfig(config);
    session.SetKeyMapManager(key_map_manager);
    InitSessionToPrecomposition(&session);
    SwitchInputFieldType(commands::Context::PASSWORD, &session);

    commands::Command command;

    // "[MO]" is a converted string like Kanji.
    // "MO" is an input string like Hiragana.
    SetupCommandForReverseConversion("[MO]", command.mutable_input());
    SetupMockForReverseConversion("[MO]", "MO", converter.get());
    EXPECT_TRUE(session.SendCommand(&command));

    EXPECT_TRUE(TestSendKey("hankaku/zenkaku", &session, &command));
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(SendKey("hankaku/zenkaku", &session, &command));
    // This behavior is the bug of b/5955618.
    // The result of TestSendKey and SendKey should be the same in terms of
    // |consumed()|.
    EXPECT_FALSE(command.output().consumed())
        << "Congrats! b/5955618 seems to be fixed";
    EXPECT_RESULT("[MO]", command);
    ASSERT_TRUE(command.output().has_status());
    // This behavior is the bug of b/5955618. IME should be deactivated.
    EXPECT_TRUE(command.output().status().activated())
        << "Congrats! b/5955618 seems to be fixed";
  }
}

TEST_F(SessionTest, DoNothingOnCompositionKeepingSuggestWindow) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  Segments segments_mo;
  {
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_mo), Return(true)));

  commands::Command command;
  SendKey("M", &session, &command);
  EXPECT_TRUE(command.output().has_candidate_window());

  SendKey("Ctrl", &session, &command);
  EXPECT_TRUE(command.output().has_candidate_window());
}

TEST_F(SessionTest, ModeChangeOfConvertAtPunctuations) {
  config::Config config;
  config.set_use_auto_conversion(true);

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  session.SetConfig(config);
  InitSessionToPrecomposition(&session);

  Segments segments_a_conv;
  {
    Segment *segment;
    segment = segments_a_conv.add_segment();
    segment->set_key("あ");
    segment->add_candidate()->value = "あ";
  }
  EXPECT_CALL(*converter, StartConversion(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments_a_conv), Return(true)));

  commands::Command command;
  SendKey("a", &session, &command);  // "あ|" (composition)
  EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);

  SendKey(".", &session, &command);  // "あ。|" (conversion)
  EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);

  SendKey("ESC", &session, &command);  // "あ。|" (composition)
  EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);

  SendKey("Left", &session, &command);  // "あ|。" (composition)
  EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);

  SendKey("i", &session, &command);  // "あい|。" (should be composition)
  EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
}

TEST_F(SessionTest, SuppressSuggestion) {
  Session session(*mock_data_engine_);
  InitSessionToPrecomposition(&session);

  commands::Command command;
  SendKey("a", &session, &command);
  EXPECT_TRUE(command.output().has_candidate_window());

  command.Clear();
  session.EditCancel(&command);
  EXPECT_FALSE(command.output().has_candidate_window());

  // Default behavior.
  SendKey("d", &session, &command);
  EXPECT_TRUE(command.output().has_candidate_window());

  // Suppress suggestion context
  SetSendKeyCommand("e", &command);
  command.mutable_input()->mutable_context()->set_suppress_suggestion(true);
  session.SendKey(&command);
  EXPECT_FALSE(command.output().has_candidate_window());

  // With an invalid identifier.  It should be the same with the
  // default behavior.
  SetSendKeyCommand("i", &command);
  command.mutable_input()->mutable_context()->add_experimental_features(
      "invalid_identifier");
  session.SendKey(&command);
  EXPECT_TRUE(command.output().has_candidate_window());

}

TEST_F(SessionTest, DeleteHistory) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("delete");
  segment->add_candidate()->value = "DeleteHistory";
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

  // Type "del". Preedit = "でｌ".
  commands::Command command;
  EXPECT_TRUE(SendKey("d", &session, &command));
  EXPECT_TRUE(SendKey("e", &session, &command));
  EXPECT_TRUE(SendKey("l", &session, &command));
  EXPECT_PREEDIT("でｌ", command);

  // Start prediction. Preedit = "DeleteHistory".
  command.Clear();
  EXPECT_TRUE(session.PredictAndConvert(&command));
  EXPECT_TRUE(command.output().has_candidate_window());
  EXPECT_EQ(session.context().state(), ImeContext::CONVERSION);
  EXPECT_PREEDIT("DeleteHistory", command);

  // Do DeleteHistory command. After that, the session should be back in
  // composition state and preedit gets back to "でｌ" again.
  EXPECT_CALL(*converter, DeleteCandidateFromHistory(_, 0, 0))
      .WillOnce(Return(true));
  EXPECT_TRUE(SendKey("Ctrl Delete", &session, &command));
  EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
  EXPECT_PREEDIT("でｌ", command);
}

TEST_F(SessionTest, SendKeyWithKeyStringDirect) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToDirect(&session);

  commands::Command command;
  constexpr absl::string_view kZa = "ざ";
  SetSendKeyCommandWithKeyString(kZa, &command);
  EXPECT_TRUE(session.TestSendKey(&command));
  EXPECT_FALSE(command.output().consumed());
  command.mutable_output()->Clear();
  EXPECT_TRUE(session.SendKey(&command));
  EXPECT_FALSE(command.output().consumed());
}

TEST_F(SessionTest, SendKeyWithKeyString) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  commands::Command command;

  // Test for precomposition state.
  EXPECT_EQ(session.context().state(), ImeContext::PRECOMPOSITION);
  constexpr absl::string_view kZa = "ざ";
  SetSendKeyCommandWithKeyString(kZa, &command);
  EXPECT_TRUE(session.TestSendKey(&command));
  EXPECT_TRUE(command.output().consumed());
  command.mutable_output()->Clear();
  EXPECT_TRUE(session.SendKey(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_PREEDIT(kZa, command);

  command.Clear();

  // Test for composition state.
  EXPECT_EQ(session.context().state(), ImeContext::COMPOSITION);
  constexpr absl::string_view kOnsenManju = "♨饅頭";
  SetSendKeyCommandWithKeyString(kOnsenManju, &command);
  EXPECT_TRUE(session.TestSendKey(&command));
  EXPECT_TRUE(command.output().consumed());
  command.mutable_output()->Clear();
  EXPECT_TRUE(session.SendKey(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_PREEDIT(absl::StrCat(kZa, kOnsenManju), command);
}

TEST_F(SessionTest, IndirectImeOnOff) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  {
    commands::Command command;
    // IMEOff
    SendSpecialKey(commands::KeyEvent::OFF, &session, &command);
  }
  {
    commands::Command command;
    // 'a'
    TestSendKeyWithModeAndActivated("a", true, commands::HIRAGANA, &session,
                                    &command);
    EXPECT_TRUE(command.output().consumed());
  }
  {
    commands::Command command;
    // 'a'
    SendKeyWithModeAndActivated("a", true, commands::HIRAGANA, &session,
                                &command);
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated())
        << "Should be activated.";
  }
  {
    commands::Command command;
    // 'a'
    TestSendKeyWithModeAndActivated("a", false, commands::HIRAGANA, &session,
                                    &command);
    EXPECT_FALSE(command.output().consumed());
  }
  {
    commands::Command command;
    // 'a'
    SendKeyWithModeAndActivated("a", false, commands::HIRAGANA, &session,
                                &command);
    EXPECT_FALSE(command.output().consumed());
    EXPECT_FALSE(command.output().has_result())
        << "Indirect IME off flushes ongoing composition";
    EXPECT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated())
        << "Should be inactivated.";
  }
}

TEST_F(SessionTest, MakeSureIMEOn) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToDirect(&session);

  {
    commands::Command command;
    SetSendCommandCommand(commands::SessionCommand::TURN_ON_IME, &command);

    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_TRUE(command.output().consumed());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
  }

  {
    // Make sure we can change the input mode.
    commands::Command command;
    SetSendCommandCommand(commands::SessionCommand::TURN_ON_IME, &command);
    command.mutable_input()->mutable_command()->set_composition_mode(
        commands::FULL_KATAKANA);

    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_TRUE(command.output().consumed());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(command.output().status().mode(), commands::FULL_KATAKANA);
  }

  {
    // Make sure we can change the input mode again.
    commands::Command command;
    SetSendCommandCommand(commands::SessionCommand::TURN_ON_IME, &command);
    command.mutable_input()->mutable_command()->set_composition_mode(
        commands::HIRAGANA);

    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_TRUE(command.output().consumed());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
  }

  {
    // commands::DIRECT is not supported for the composition_mode.
    commands::Command command;
    SetSendCommandCommand(commands::SessionCommand::TURN_ON_IME, &command);
    command.mutable_input()->mutable_command()->set_composition_mode(
        commands::DIRECT);
    EXPECT_FALSE(session.SendCommand(&command));
  }
}

TEST_F(SessionTest, MakeSureIMEOff) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  InitSessionToPrecomposition(&session);

  {
    commands::Command command;
    SetSendCommandCommand(commands::SessionCommand::TURN_OFF_IME, &command);

    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_TRUE(command.output().consumed());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
  }

  {
    // Make sure we can change the input mode.
    commands::Command command;
    SetSendCommandCommand(commands::SessionCommand::TURN_OFF_IME, &command);
    command.mutable_input()->mutable_command()->set_composition_mode(
        commands::FULL_KATAKANA);

    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_TRUE(command.output().consumed());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
    EXPECT_EQ(command.output().status().mode(), commands::FULL_KATAKANA);
  }

  {
    // Make sure we can change the input mode again.
    commands::Command command;
    SetSendCommandCommand(commands::SessionCommand::TURN_OFF_IME, &command);
    command.mutable_input()->mutable_command()->set_composition_mode(
        commands::HIRAGANA);

    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_TRUE(command.output().consumed());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
    EXPECT_EQ(command.output().status().mode(), commands::HIRAGANA);
  }

  {
    // commands::DIRECT is not supported for the composition_mode.
    commands::Command command;
    SetSendCommandCommand(commands::SessionCommand::TURN_OFF_IME, &command);
    command.mutable_input()->mutable_command()->set_composition_mode(
        commands::DIRECT);
    EXPECT_FALSE(session.SendCommand(&command));
  }
}

TEST_F(SessionTest, MakeSureIMEOffWithCommitComposition) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  Session session(engine);
  // Make sure SessionCommand::TURN_OFF_IME terminates the existing
  // composition.

  InitSessionToPrecomposition(&session);

  // Set up converter.
  {
    commands::Command command;
    InsertCharacterChars("aiueo", &session, &command);
  }

  // Send SessionCommand::TURN_OFF_IME to commit composition.
  {
    commands::Command command;
    SetSendCommandCommand(commands::SessionCommand::TURN_OFF_IME, &command);
    command.mutable_input()->mutable_command()->set_composition_mode(
        commands::FULL_KATAKANA);
    ASSERT_TRUE(session.SendCommand(&command));
    EXPECT_RESULT("あいうえお", command);
    EXPECT_TRUE(command.output().consumed());
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
    EXPECT_EQ(command.output().status().mode(), commands::FULL_KATAKANA);
  }
}

TEST_F(SessionTest, DeleteCandidateFromHistory) {
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);

  // InitSessionToConversionWithAiueo initializes candidates as follows:
  // 0:あいうえお, 1:アイウエオ, -3:aiueo, -4:AIUEO, ...
  {
    // A test case to delete focused candidate (i.e. without candidate ID).
    Session session(engine);
    InitSessionToConversionWithAiueo(&session, converter.get());

    EXPECT_CALL(*converter, DeleteCandidateFromHistory(_, 0, 0))
        .WillOnce(Return(true));

    commands::Command command;
    session.DeleteCandidateFromHistory(&command);

    Mock::VerifyAndClearExpectations(converter.get());
  }
  {
    // A test case to delete candidate by ID.
    Session session(engine);
    InitSessionToConversionWithAiueo(&session, converter.get());

    EXPECT_CALL(*converter, DeleteCandidateFromHistory(_, 0, 1))
        .WillOnce(Return(true));

    commands::Command command;
    SetSendCommandCommand(
        commands::SessionCommand::DELETE_CANDIDATE_FROM_HISTORY, &command);
    command.mutable_input()->mutable_command()->set_id(1);
    session.DeleteCandidateFromHistory(&command);

    Mock::VerifyAndClearExpectations(converter.get());
  }
}

TEST_F(SessionTest, SetConfig) {
  auto config =
      std::make_shared<config::Config>(config::ConfigHandler::DefaultConfig());
  config->set_session_keymap(config::Config::CUSTOM);
  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);
  Session session(engine);
  session.PushUndoContext();
  session.SetConfig(config);

  EXPECT_EQ(config.get(), &session.context_->GetConfig());
  // SetConfig() resets undo context.
  EXPECT_TRUE(session.undo_contexts_.empty());
}

TEST_F(SessionTest, ClearCompositionByBackspace) {
  // The internal candidate list should be cleared when the composition is
  // cleared by backspace.
  Segments segments;
  {
    // Set up a mock conversion result.
    Segment *segment;
    segment = segments.add_segment();
    segment->set_key("あ");
    segment->add_candidate()->value = "あ";
  }

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  Session session(engine);
  InitSessionToPrecomposition(&session, *mobile_request_);
  commands::Command command;

  EXPECT_TRUE(SendKey("1", &session, &command));
  EXPECT_SINGLE_SEGMENT("あ", command);
  EXPECT_TRUE(command.output().has_all_candidate_words());

  EXPECT_TRUE(SendKey("Backspace", &session, &command));
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_all_candidate_words());

  // Input mode switch command can output the current internal candidate list,
  // which should be cleared by the above backspace.
  session.InputModeSwitchKanaType(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_all_candidate_words());
}

TEST_F(SessionTest, ClearCompositionByEscape) {
  // The internal candidate list should be cleared when the composition is
  // cleared by escape.
  Segments segments;
  {
    // Set up a mock conversion result.
    Segment *segment;
    segment = segments.add_segment();
    segment->set_key("あ");
    segment->add_candidate()->value = "あ";
  }

  MockEngine engine;
  std::shared_ptr<MockConverter> converter = CreateEngineConverterMock(&engine);
  EXPECT_CALL(*converter, StartPrediction(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  Session session(engine);
  InitSessionToPrecomposition(&session, *mobile_request_);
  commands::Command command;

  EXPECT_TRUE(SendKey("1", &session, &command));
  EXPECT_SINGLE_SEGMENT("あ", command);
  EXPECT_TRUE(command.output().has_all_candidate_words());

  EXPECT_TRUE(SendKey("Escape", &session, &command));
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_all_candidate_words());

  // Input mode switch command can output the current internal candidate list,
  // which should be cleared by the above backspace.
  session.InputModeSwitchKanaType(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_all_candidate_words());
}

}  // namespace session
}  // namespace mozc
