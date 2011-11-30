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

#include <string>
#include <vector>
#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "rewriter/transliteration_rewriter.h"
#include "session/commands.pb.h"
#include "session/internal/ime_context.h"
#include "session/internal/keymap.h"
#include "session/japanese_session_factory.h"
#include "session/key_parser.h"
#include "session/session.h"
#include "session/session_converter_interface.h"
#include "session/session_handler.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace session {

namespace {
bool SendKey(const string &key,
             Session *session,
             commands::Command *command) {
  command->Clear();
  command->mutable_input()->set_type(commands::Input::SEND_KEY);
  if (!KeyParser::ParseKey(key, command->mutable_input()->mutable_key())) {
    return false;
  }
  return session->SendKey(command);
}

bool TestSendKey(const string &key,
                 Session *session,
                 commands::Command *command) {
  command->Clear();
  command->mutable_input()->set_type(commands::Input::SEND_KEY);
  if (!KeyParser::ParseKey(key, command->mutable_input()->mutable_key())) {
    return false;
  }
  return session->TestSendKey(command);
}

void SendSpecialKey(commands::KeyEvent::SpecialKey special_key,
                    Session* session,
                    commands::Command* command) {
  command->Clear();
  command->mutable_input()->mutable_key()->set_special_key(special_key);
  session->SendKey(command);
}

bool InsertCharacterCodeAndString(const char key_code,
                                  const string &key_string,
                                  Session *session,
                                  commands::Command *command) {
  command->Clear();
  commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
  key_event->set_key_code(key_code);
  key_event->set_key_string(key_string);
  return session->InsertCharacter(command);
}

Segment::Candidate *AddCandidate(const string &key, const string &value,
                                 Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->key = key;
  candidate->content_key = key;
  candidate->value = value;
  return candidate;
}

Segment::Candidate *AddMetaCandidate(const string &key, const string &value,
                                     Segment *segment) {
  Segment::Candidate *candidate = segment->add_meta_candidate();
  candidate->key = key;
  candidate->content_key = key;
  candidate->value = value;
  return candidate;
}

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

void InitSessionToPrecomposition(Session* session) {
#ifdef OS_WINDOWS
  // Session is created with direct mode on Windows
  // Direct status
  commands::Command command;
  session->IMEOn(&command);
#endif  // OS_WINDOWS
}

void SetCaretLocation(const commands::Rectangle rectangle, Session *session) {
  commands::Command command;
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  commands::SessionCommand *session_command =
      command.mutable_input()->mutable_command();
  session_command->set_type(commands::SessionCommand::SEND_CARET_LOCATION);
  session_command->mutable_caret_rectangle()->CopyFrom(rectangle);
  session->SendCommand(&command);
}

// since History segments are almost hidden from
class ConverterMockForReset : public ConverterMock {
 public:
  bool ResetConversion(Segments *segments) const {
    reset_conversion_called_ = true;
    return true;
  }

  bool reset_conversion_called() const {
    return reset_conversion_called_;
  }

  void Reset() {
    reset_conversion_called_ = false;
  }

  ConverterMockForReset() : reset_conversion_called_(false) {}
 private:
  mutable bool reset_conversion_called_;
};

class ConverterMockForRevert : public ConverterMock {
 public:
  bool RevertConversion(Segments *segments) const {
    revert_conversion_called_ = true;
    return true;
  }

  bool revert_conversion_called() const {
    return revert_conversion_called_;
  }

  void Reset() {
    revert_conversion_called_ = false;
  }

  ConverterMockForRevert() : revert_conversion_called_(false) {}
 private:
  mutable bool revert_conversion_called_;
};
}  // namespace

class SessionTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    session::SessionFactoryManager::SetSessionFactory(&session_factory_);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
    convertermock_.reset(new ConverterMock());
    ConverterFactory::SetConverter(convertermock_.get());
    handler_.reset(new SessionHandler());
    t13n_rewriter_.reset(new TransliterationRewriter());
  }

  virtual void TearDown() {
    // just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  void InsertCharacterChars(const string &chars,
                            Session *session,
                            commands::Command *command) const {
    const uint32 kNoModifiers = 0;
    for (int i = 0; i < chars.size(); ++i) {
      command->clear_input();
      command->clear_output();
      commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
      key_event->set_key_code(chars[i]);
      key_event->set_modifiers(kNoModifiers);
      session->InsertCharacter(command);
    }
  }

  void InsertCharacterCharsWithContext(const string &chars,
                                       const commands::Context &context,
                                       Session *session,
                                       commands::Command *command) const {
    const uint32 kNoModifiers = 0;
    for (size_t i = 0; i < chars.size(); ++i) {
      command->clear_input();
      command->clear_output();
      command->mutable_input()->mutable_context()->CopyFrom(context);
      commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
      key_event->set_key_code(chars[i]);
      key_event->set_modifiers(kNoModifiers);
      session->InsertCharacter(command);
    }
  }

  void InsertCharacterString(const string &key_strings,
                             const string &chars,
                             Session *session,
                             commands::Command *command) const {
    const uint32 kNoModifiers = 0;
    vector<string> inputs;
    const char *begin = key_strings.data();
    const char *end = key_strings.data() + key_strings.size();
    while (begin < end) {
      const size_t mblen = Util::OneCharLen(begin);
      inputs.push_back(string(begin, mblen));
      begin += mblen;
    }
    CHECK_EQ(inputs.size(), chars.size());
    for (int i = 0; i < chars.size(); ++i) {
      command->clear_input();
      command->clear_output();
      commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
      key_event->set_key_code(chars[i]);
      key_event->set_modifiers(kNoModifiers);
      key_event->set_key_string(inputs[i]);
      session->InsertCharacter(command);
    }
  }

  // set result for "あいうえお"
  void SetAiueo(Segments *segments) {
    segments->Clear();
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments->add_segment();
    // "あいうえお"
    segment->set_key(
        "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a");
    candidate = segment->add_candidate();
    // "あいうえお"
    candidate->value =
        "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";
    candidate = segment->add_candidate();
    // "アイウエオ"
    candidate->value =
        "\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6\xe3\x82\xa8\xe3\x82\xaa";
  }

  // set result for "like"
  void SetLike(Segments *segments) {
    Segment *segment;
    Segment::Candidate *candidate;

    segments->Clear();
    segment = segments->add_segment();

    // "ぃ"
    segment->set_key("\xE3\x81\x83");
    candidate = segment->add_candidate();
    // "ぃ"
    candidate->value = "\xE3\x81\x83";

    candidate = segment->add_candidate();
    // "ィ"
    candidate->value = "\xE3\x82\xA3";

    segment = segments->add_segment();
    // "け"
    segment->set_key("\xE3\x81\x91");
    candidate = segment->add_candidate();
    // "家"
    candidate->value = "\xE5\xAE\xB6";
    candidate = segment->add_candidate();
    // "け"
    candidate->value = "\xE3\x81\x91";
  }

  void FillT13Ns(Segments *segments) {
    t13n_rewriter_->Rewrite(segments);
  }

  void SetComposer(Session *session, Segments *segments) {
    segments->set_composer(session->get_internal_composer_only_for_unittest());
  }

  void SetConfig(const config::Config &config) {
    config::Config base_config;
    config::ConfigHandler::GetConfig(&base_config);

    commands::Command command;
    command.mutable_input()->set_type(commands::Input::SET_CONFIG);
    command.mutable_input()->mutable_config()->CopyFrom(base_config);
    command.mutable_input()->mutable_config()->MergeFrom(config);
    handler_->EvalCommand(&command);
  }

  void SetupMockForReverseConversion(const string &kanji,
                                     const string &hiragana) {
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
    convertermock_->SetStartReverseConversion(&reverse_segments, true);
    // Set up Segments for forward conversion.
    Segments segments;
    segment = segments.add_segment();
    segment->set_key(hiragana);
    candidate = segment->add_candidate();
    candidate->key = hiragana;
    candidate->value = kanji;
    convertermock_->SetStartConversionWithComposer(&segments, true);
  }

  void SetupCommandForReverseConversion(const string &text,
                                        commands::Input *input) {
    input->set_type(commands::Input::SEND_COMMAND);
    input->mutable_command()->set_type(
        commands::SessionCommand::CONVERT_REVERSE);
    input->mutable_command()->set_text(text);
  }


  void SetUndoContext(Session *session) {
    commands::Command command;
    Segments segments;

    {  // Create segments
      InsertCharacterChars("aiueo", session, &command);
      SetComposer(session, &segments);
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
      convertermock_->SetStartConversionWithComposer(&segments, true);
      command.Clear();
      session->Convert(&command);
      EXPECT_FALSE(command.output().has_result());
      EXPECT_TRUE(command.output().has_preedit());
      // "あいうえお"
      EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
                GetComposition(command));

      convertermock_->SetCommitSegmentValue(&segments, true);
      command.Clear();
      session->Commit(&command);
      EXPECT_TRUE(command.output().has_result());
      EXPECT_FALSE(command.output().has_preedit());
      // "あいうえお"
      EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
                command.output().result().value());
    }
  }

  scoped_ptr<SessionHandler> handler_;
  scoped_ptr<ConverterMock> convertermock_;
  scoped_ptr<TransliterationRewriter> t13n_rewriter_;
  JapaneseSessionFactory session_factory_;
};

TEST_F(SessionTest, TestSendKey) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;

  // Precomposition status
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::UP);
  session->TestSendKey(&command);
  EXPECT_FALSE(command.output().consumed());

  command.clear_output();
  session->SendKey(&command);
  EXPECT_FALSE(command.output().consumed());

  // InsertSpace on Precomposition status
  // TODO(komatsu): Test both cases of GET_CONFIG(ascii_character_form) is
  // FULL_WIDTH and HALF_WIDTH after dependency injection of GET_CONFIG.
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::SPACE);
  session->TestSendKey(&command);
  const bool consumed_on_testsendkey = command.output().consumed();
  session->SendKey(&command);
  const bool consumed_on_sendkey = command.output().consumed();
  EXPECT_EQ(consumed_on_sendkey, consumed_on_testsendkey);

  // Precomposition status
  command.Clear();
  command.mutable_input()->mutable_key()->set_key_code('G');
  session->TestSendKey(&command);
  EXPECT_TRUE(command.output().consumed());

  command.clear_output();
  session->SendKey(&command);
  EXPECT_TRUE(command.output().consumed());

  // Composition status
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::UP);
  session->TestSendKey(&command);
  EXPECT_TRUE(command.output().consumed());

  command.clear_output();
  session->SendKey(&command);
  EXPECT_TRUE(command.output().consumed());
}

TEST_F(SessionTest, SendCommand) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("kanji", session.get(), &command);

  // REVERT
  command.Clear();
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::REVERT);
  session->SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_candidates());

  // SUBMIT
  InsertCharacterChars("k", session.get(), &command);
  command.Clear();
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::SUBMIT);
  session->SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  ASSERT_TRUE(command.output().has_result());
  // "ｋ"
  EXPECT_EQ("\xef\xbd\x8b", command.output().result().value());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_candidates());

  // SWITCH_INPUT_MODE
  SendKey("a", session.get(), &command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());

  command.Clear();
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::SWITCH_INPUT_MODE);
  command.mutable_input()->mutable_command()->set_composition_mode(
      commands::FULL_ASCII);
  EXPECT_TRUE(session->SendCommand(&command));

  SendKey("a", session.get(), &command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "あａ"
  EXPECT_EQ("\xE3\x81\x82\xEF\xBD\x81",
            command.output().preedit().segment(0).key());

  // GET_STATUS
  command.Clear();
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::GET_STATUS);
  EXPECT_TRUE(session->SendCommand(&command));
  // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
  EXPECT_EQ(commands::FULL_ASCII, command.output().mode());

  // RESET_CONTEXT
  // test of reverting composition
  InsertCharacterChars("kanji", session.get(), &command);
  command.Clear();
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::RESET_CONTEXT);
  session->SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_candidates());
  // test of reseting the history segements
  scoped_ptr<ConverterMockForReset> convertermock(new ConverterMockForReset);
  ConverterFactory::SetConverter(convertermock.get());
  session.reset(new Session);
  InitSessionToPrecomposition(session.get());
  command.Clear();
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::RESET_CONTEXT);
  session->SendCommand(&command);
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(convertermock->reset_conversion_called());

  // USAGE_STATS_EVENT
  command.Clear();
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::USAGE_STATS_EVENT);
  EXPECT_TRUE(session->SendCommand(&command));
  EXPECT_TRUE(command.output().has_consumed());
  EXPECT_FALSE(command.output().consumed());
}

TEST_F(SessionTest, SwitchInputMode) {
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    // SWITCH_INPUT_MODE
    SendKey("a", session.get(), &command);
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    // "あ"
    EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());

    command.Clear();
    command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::SWITCH_INPUT_MODE);
    command.mutable_input()->mutable_command()->set_composition_mode(
      commands::FULL_ASCII);
    EXPECT_TRUE(session->SendCommand(&command));

    SendKey("a", session.get(), &command);
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    // "あａ"
    EXPECT_EQ("\xE3\x81\x82\xEF\xBD\x81",
              command.output().preedit().segment(0).key());

    // GET_STATUS
    command.Clear();
    command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::GET_STATUS);
    EXPECT_TRUE(session->SendCommand(&command));
    // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
    EXPECT_EQ(commands::FULL_ASCII, command.output().mode());
  }

  {
    // Confirm that we can change the mode from DIRECT
    // to other modes directly (without IMEOn command).
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    session->IMEOff(&command);

    // GET_STATUS
    command.Clear();
    command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::GET_STATUS);
    EXPECT_TRUE(session->SendCommand(&command));
    // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
    EXPECT_EQ(commands::DIRECT, command.output().mode());

    // SWITCH_INPUT_MODE
    command.Clear();
    command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::SWITCH_INPUT_MODE);
    command.mutable_input()->mutable_command()->set_composition_mode(
      commands::HIRAGANA);
    EXPECT_TRUE(session->SendCommand(&command));

    // GET_STATUS
    command.Clear();
    command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::GET_STATUS);
    EXPECT_TRUE(session->SendCommand(&command));
    // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());

    SendKey("a", session.get(), &command);
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    // "あ"
    EXPECT_EQ("\xE3\x81\x82",
              command.output().preedit().segment(0).key());

    // GET_STATUS
    command.Clear();
    command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::GET_STATUS);
    EXPECT_TRUE(session->SendCommand(&command));
    // FULL_ASCII was set at the SWITCH_INPUT_MODE testcase.
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  }
}

TEST_F(SessionTest, RevertComposition) {
  // Issue#2237323
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  InsertCharacterChars("aiueo", session.get(), &command);
  Segments segments;
  SetComposer(session.get(), &segments);
  SetAiueo(&segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.clear_input();
  command.clear_output();
  session->Convert(&command);

  // REVERT
  command.Clear();
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::REVERT);
  session->SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_candidates());

  SendKey("a", session.get(), &command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());
}

TEST_F(SessionTest, InputMode) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  EXPECT_TRUE(session->InputModeHalfASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HALF_ASCII, command.output().mode());

  SendKey("a", session.get(), &command);
  EXPECT_EQ("a", command.output().preedit().segment(0).key());

  command.Clear();
  session->Commit(&command);

  // Input mode remains even after submission.
  command.Clear();
  session->GetStatus(&command);
  EXPECT_EQ(mozc::commands::HALF_ASCII, command.output().mode());
}

TEST_F(SessionTest, SelectCandidate) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("aiueo", session.get(), &command);
  Segments segments;
  SetComposer(session.get(), &segments);
  SetAiueo(&segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.clear_input();
  command.clear_output();
  session->Convert(&command);

  command.clear_input();
  command.clear_output();
  session->ConvertNext(&command);

  command.clear_input();
  command.clear_output();
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::SELECT_CANDIDATE);
  command.mutable_input()->mutable_command()->set_id(
      -(transliteration::HALF_KATAKANA + 1));
  session->SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_candidates());
}

TEST_F(SessionTest, HighlightCandidate) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("aiueo", session.get(), &command);
  Segments segments;
  SetComposer(session.get(), &segments);
  SetAiueo(&segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.clear_input();
  command.clear_output();
  session->Convert(&command);

  command.clear_input();
  command.clear_output();
  session->ConvertNext(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "アイウエオ"
  EXPECT_EQ("\xE3\x82\xA2\xE3\x82\xA4\xE3\x82\xA6\xE3\x82\xA8\xE3\x82\xAA",
            command.output().preedit().segment(0).value());
  EXPECT_TRUE(command.output().has_candidates());

  command.clear_input();
  command.clear_output();
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::HIGHLIGHT_CANDIDATE);
  command.mutable_input()->mutable_command()->set_id(
      -(transliteration::HALF_KATAKANA + 1));
  session->SendCommand(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "ｱｲｳｴｵ"
  EXPECT_EQ("\xEF\xBD\xB1\xEF\xBD\xB2\xEF\xBD\xB3\xEF\xBD\xB4\xEF\xBD\xB5",
            command.output().preedit().segment(0).value());
  EXPECT_TRUE(command.output().has_candidates());
}

TEST_F(SessionTest, Conversion) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("aiueo", session.get(), &command);
  Segments segments;
  SetComposer(session.get(), &segments);
  SetAiueo(&segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());

  EXPECT_TRUE(command.output().preedit().segment(0).has_value());
  EXPECT_TRUE(command.output().preedit().segment(0).has_key());
  // "あいうえお"
  EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
            command.output().preedit().segment(0).key());


  command.clear_input();
  command.clear_output();
  session->Convert(&command);

  command.clear_input();
  command.clear_output();
  session->ConvertNext(&command);

  EXPECT_TRUE(command.output().has_preedit());

  string key;
  for (int i = 0; i < command.output().preedit().segment_size(); ++i) {
    EXPECT_TRUE(command.output().preedit().segment(i).has_value());
    EXPECT_TRUE(command.output().preedit().segment(i).has_key());
    key += command.output().preedit().segment(i).key();
  }
  // "あいうえお"
  EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
            key);
}


TEST_F(SessionTest, SegmentWidthShrink) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("aiueo", session.get(), &command);
  Segments segments;
  SetComposer(session.get(), &segments);
  SetAiueo(&segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.clear_input();
  command.clear_output();
  session->Convert(&command);

  command.clear_input();
  command.clear_output();
  session->SegmentWidthShrink(&command);

  command.clear_input();
  command.clear_output();
  session->SegmentWidthShrink(&command);
}

TEST_F(SessionTest, ConvertPrev) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("aiueo", session.get(), &command);
  Segments segments;
  SetComposer(session.get(), &segments);
  SetAiueo(&segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->Convert(&command);

  command.Clear();
  session->ConvertNext(&command);

  command.Clear();
  session->ConvertPrev(&command);

  command.Clear();
  session->ConvertPrev(&command);
}

TEST_F(SessionTest, ResetFocusedSegmentAfterCommit) {
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("watasinonamaehanakanodesu", session.get(), &command);
  // "わたしのなまえはなかのです[]"

  segment = segments.add_segment();
  // "わたしの"
  segment->set_key("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae");
  candidate = segment->add_candidate();
  // "私の"
  candidate->value = "\xe7\xa7\x81\xe3\x81\xae";
  candidate = segment->add_candidate();
  // "わたしの"
  candidate->value = "\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae";
  candidate = segment->add_candidate();
  // "渡しの"
  candidate->value = "\xe6\xb8\xa1\xe3\x81\x97\xe3\x81\xae";

  segment = segments.add_segment();
  // "なまえは"
  segment->set_key("\xe3\x81\xaa\xe3\x81\xbe\xe3\x81\x88\xe3\x81\xaf");
  candidate = segment->add_candidate();
  // "名前は"
  candidate->value = "\xe5\x90\x8d\xe5\x89\x8d\xe3\x81\xaf";
  candidate = segment->add_candidate();
  // "ナマエは"
  candidate->value = "\xe3\x83\x8a\xe3\x83\x9e\xe3\x82\xa8\xe3\x81\xaf";

  segment = segments.add_segment();
  // "なかのです"
  segment->set_key(
      "\xe3\x81\xaa\xe3\x81\x8b\xe3\x81\xae\xe3\x81\xa7\xe3\x81\x99");
  candidate = segment->add_candidate();
  // "中野です"
  candidate->value = "\xe4\xb8\xad\xe9\x87\x8e\xe3\x81\xa7\xe3\x81\x99";
  candidate = segment->add_candidate();
  // "なかのです"
  candidate->value
      = "\xe3\x81\xaa\xe3\x81\x8b\xe3\x81\xae\xe3\x81\xa7\xe3\x81\x99";
  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.clear_input();
  command.clear_output();
  session->Convert(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "[私の]名前は中野です"
  command.clear_input();
  command.clear_output();
  session->SegmentFocusRight(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "私の[名前は]中野です"
  command.clear_input();
  command.clear_output();
  session->SegmentFocusRight(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "私の名前は[中野です]"

  command.clear_input();
  command.clear_output();
  session->ConvertNext(&command);
  EXPECT_EQ(1, command.output().candidates().focused_index());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "私の名前は[中のです]"

  command.clear_input();
  command.clear_output();
  session->ConvertNext(&command);
  EXPECT_EQ(2, command.output().candidates().focused_index());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "私の名前は[なかのです]"

  command.clear_input();
  command.clear_output();
  session->Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_TRUE(command.output().has_result());
  // "私の名前はなかのです[]"

  command.clear_input();
  command.clear_output();
  InsertCharacterChars("a", session.get(), &command);

  segments.Clear();
  segment = segments.add_segment();
  // "あ"
  segment->set_key("\xe3\x81\x82");
  candidate = segment->add_candidate();
  // "阿"
  candidate->value = "\xe9\x98\xbf";
  candidate = segment->add_candidate();
  // "亜"
  candidate->value = "\xe4\xba\x9c";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  // "あ[]"

  command.clear_input();
  command.clear_output();
  session->Convert(&command);
  // "[阿]"

  command.clear_input();
  command.clear_output();
  // If the forcused_segment_ was not reset, this raises segmentation fault.
  session->ConvertNext(&command);
  // "[亜]"
}

TEST_F(SessionTest, ResetFocusedSegmentAfterCancel) {
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("ai", session.get(), &command);

  segment = segments.add_segment();
  // "あい"
  segment->set_key("\xe3\x81\x82\xe3\x81\x84");
  candidate = segment->add_candidate();
  // "愛"
  candidate->value = "\xe6\x84\x9b";
  candidate = segment->add_candidate();
  // "相"
  candidate->value = "\xe7\x9b\xb8";
  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);
  // "あい[]"

  command.clear_input();
  command.clear_output();
  session->Convert(&command);
  // "[愛]"

  segments.Clear();
  segment = segments.add_segment();
  // "あ"
  segment->set_key("\xe3\x81\x82");
  candidate = segment->add_candidate();
  // "あ"
  candidate->value = "\xe3\x81\x82";
  segment = segments.add_segment();
  // "い"
  segment->set_key("\xe3\x81\x84");
  candidate = segment->add_candidate();
  // "い"
  candidate->value = "\xe3\x81\x84";
  candidate = segment->add_candidate();
  // "位"
  candidate->value = "\xe4\xbd\x8d";
  convertermock_->SetResizeSegment1(&segments, true);

  command.clear_input();
  command.clear_output();
  session->SegmentWidthShrink(&command);
  // "[あ]い"

  segment = segments.mutable_segment(0);
  segment->set_segment_type(Segment::FIXED_VALUE);
  convertermock_->SetCommitSegmentValue(&segments, true);

  command.clear_input();
  command.clear_output();
  session->SegmentFocusRight(&command);
  // "あ[い]"

  command.clear_input();
  command.clear_output();
  session->ConvertNext(&command);
  // "あ[位]"

  command.clear_input();
  command.clear_output();
  session->ConvertCancel(&command);
  // "あい[]"

  segments.Clear();
  segment = segments.add_segment();
  // "あい"
  segment->set_key("\xe3\x81\x82\xe3\x81\x84");
  candidate = segment->add_candidate();
  // "愛"
  candidate->value = "\xe6\x84\x9b";
  candidate = segment->add_candidate();
  // "相"
  candidate->value = "\xe7\x9b\xb8";
  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.clear_input();
  command.clear_output();
  session->Convert(&command);
  // "[愛]"

  command.clear_input();
  command.clear_output();
  // If the forcused_segment_ was not reset, this raises segmentation fault.
  session->Convert(&command);
  // "[相]"
}


TEST_F(SessionTest, KeepFixedCandidateAfterSegmentWidthExpand) {
  // Issue#1271099
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("bariniryokouniitta", session.get(), &command);
  // "ばりにりょこうにいった[]"

  segment = segments.add_segment();
  // "ばりに"
  segment->set_key("\xe3\x81\xb0\xe3\x82\x8a\xe3\x81\xab");
  candidate = segment->add_candidate();
  // "バリに"
  candidate->value = "\xe3\x83\x90\xe3\x83\xaa\xe3\x81\xab";
  candidate = segment->add_candidate();
  // "針に"
  candidate->value = "\xe9\x87\x9d\xe3\x81\xab";

  segment = segments.add_segment();
  // "りょこうに"
  segment->set_key(
      "\xe3\x82\x8a\xe3\x82\x87\xe3\x81\x93\xe3\x81\x86\xe3\x81\xab");
  candidate = segment->add_candidate();
  // "旅行に"
  candidate->value = "\xe6\x97\x85\xe8\xa1\x8c\xe3\x81\xab";

  segment = segments.add_segment();
  // "いった"
  segment->set_key("\xe3\x81\x84\xe3\x81\xa3\xe3\x81\x9f");
  candidate = segment->add_candidate();
  // "行った"
  candidate->value = "\xe8\xa1\x8c\xe3\x81\xa3\xe3\x81\x9f";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.clear_input();
  command.clear_output();
  session->Convert(&command);
  // ex. "[バリに]旅行に行った"
  EXPECT_EQ("\xE3\x83\x90\xE3\x83\xAA\xE3\x81\xAB\xE6\x97\x85\xE8\xA1\x8C\xE3"
      "\x81\xAB\xE8\xA1\x8C\xE3\x81\xA3\xE3\x81\x9F", GetComposition(command));
  command.clear_input();
  command.clear_output();
  session->ConvertNext(&command);
  // ex. "[針に]旅行に行った"
  const string first_segment = command.output().preedit().segment(0).value();

  segment = segments.mutable_segment(0);
  segment->set_segment_type(Segment::FIXED_VALUE);
  segment->move_candidate(1, 0);
  convertermock_->SetCommitSegmentValue(&segments, true);

  command.clear_input();
  command.clear_output();
  session->SegmentFocusRight(&command);
  // ex. "針に[旅行に]行った"
  // Make sure the first segment (i.e. "針に" in the above case) remains
  // after moving the focused segment right.
  EXPECT_EQ(first_segment, command.output().preedit().segment(0).value());

  segment = segments.mutable_segment(1);
  // "りょこうにい"
  segment->set_key("\xe3\x82\x8a\xe3\x82\x87\xe3\x81\x93"
                   "\xe3\x81\x86\xe3\x81\xab\xe3\x81\x84");
  candidate = segment->mutable_candidate(0);
  // "旅行に行"
  candidate->value = "\xe6\x97\x85\xe8\xa1\x8c\xe3\x81\xab\xe8\xa1\x8c";

  segment = segments.mutable_segment(2);
  // "った"
  segment->set_key("\xe3\x81\xa3\xe3\x81\x9f");
  candidate = segment->mutable_candidate(0);
  // "った"
  candidate->value = "\xe3\x81\xa3\xe3\x81\x9f";

  convertermock_->SetResizeSegment1(&segments, true);

  command.clear_input();
  command.clear_output();
  session->SegmentWidthExpand(&command);
  // ex. "針に[旅行に行]った"

  // Make sure the first segment (i.e. "針に" in the above case) remains
  // after expanding the focused segment.
  EXPECT_EQ(first_segment, command.output().preedit().segment(0).value());
}

TEST_F(SessionTest, CommitSegment) {
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;

  // Issue#1560608
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("watasinonamae", session.get(), &command);
  // "わたしのなまえ[]"

  segment = segments.add_segment();
  // "わたしの"
  segment->set_key("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae");
  candidate = segment->add_candidate();
  // "私の"
  candidate->value = "\xe7\xa7\x81\xe3\x81\xae";
  candidate = segment->add_candidate();
  // "わたしの"
  candidate->value = "\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae";
  candidate = segment->add_candidate();
  // "渡しの"
  candidate->value = "\xe6\xb8\xa1\xe3\x81\x97\xe3\x81\xae";

  segment = segments.add_segment();
  // "なまえ"
  segment->set_key("\xe3\x81\xaa\xe3\x81\xbe\xe3\x81\x88");
  candidate = segment->add_candidate();
  // "名前"
  candidate->value = "\xe5\x90\x8d\xe5\x89\x8d";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->Convert(&command);
  // "[私の]名前"
  EXPECT_EQ(0, command.output().candidates().focused_index());

  command.Clear();
  session->ConvertNext(&command);
  // "[わたしの]名前"
  EXPECT_EQ(1, command.output().candidates().focused_index());

  command.Clear();
  session->ConvertNext(&command);
  // "[渡しの]名前" showing a candidate window
  EXPECT_EQ(2, command.output().candidates().focused_index());

  segment = segments.mutable_segment(0);
  segment->set_segment_type(Segment::FIXED_VALUE);
  segment->move_candidate(2, 0);

  convertermock_->SetSubmitFirstSegment(&segments, true);

  command.Clear();
  session->CommitSegment(&command);
  // "渡しの" + "[名前]"
  EXPECT_EQ(0, command.output().candidates().focused_index());
}

TEST_F(SessionTest, CommitSegmentAt2ndSegment) {
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("watasinohaha", session.get(), &command);
  // "わたしのはは[]"

  segment = segments.add_segment();
  // "わたしの"
  segment->set_key("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae");
  candidate = segment->add_candidate();
  // "私の"
  candidate->value = "\xe7\xa7\x81\xe3\x81\xae";
  segment = segments.add_segment();
  // "はは"
  segment->set_key("\xe3\x81\xaf\xe3\x81\xaf");
  candidate = segment->add_candidate();
  // "母"
  candidate->value = "\xe6\xaf\x8d";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->Convert(&command);
  // "[私の]母"

  command.Clear();
  session->SegmentFocusRight(&command);
  // "私の[母]"

  segment->set_segment_type(Segment::FIXED_VALUE);
  segment->move_candidate(1, 0);
  convertermock_->SetSubmitFirstSegment(&segments, true);

  command.Clear();
  session->CommitSegment(&command);
  // "私の" + "[母]"

  // "は"
  segment->set_key("\xe3\x81\xaf");
  // "葉"
  candidate->value = "\xe8\x91\x89";
  segment = segments.add_segment();
  // "は"
  segment->set_key("\xe3\x81\xaf");
  candidate = segment->add_candidate();
  // "は"
  candidate->value = "\xe3\x81\xaf";
  segments.pop_front_segment();
  convertermock_->SetResizeSegment1(&segments, true);

  command.Clear();
  session->SegmentWidthShrink(&command);
  // "私の" + "[葉]は"
  EXPECT_EQ(2, command.output().preedit().segment_size());
}

TEST_F(SessionTest, Transliterations) {
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("jishin", session.get(), &command);

  segment = segments.add_segment();
  // "じしん"
  segment->set_key("\xe3\x81\x98\xe3\x81\x97\xe3\x82\x93");
  candidate = segment->add_candidate();
  // "自信"
  candidate->value = "\xe8\x87\xaa\xe4\xbf\xa1";
  candidate = segment->add_candidate();
  // "自身"
  candidate->value = "\xe8\x87\xaa\xe8\xba\xab";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->Convert(&command);

  command.Clear();
  session->ConvertNext(&command);

  command.Clear();
  session->TranslateHalfASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  EXPECT_EQ("jishin", command.output().preedit().segment(0).value());

  command.Clear();
  session->TranslateHalfASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  EXPECT_EQ("JISHIN", command.output().preedit().segment(0).value());

  command.Clear();
  session->TranslateHalfASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  EXPECT_EQ("Jishin", command.output().preedit().segment(0).value());

  command.Clear();
  session->TranslateHalfASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  EXPECT_EQ("jishin", command.output().preedit().segment(0).value());
}

TEST_F(SessionTest, ConvertToTransliteration) {
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("jishin", session.get(), &command);

  segment = segments.add_segment();
  // "じしん"
  segment->set_key("\xe3\x81\x98\xe3\x81\x97\xe3\x82\x93");
  candidate = segment->add_candidate();
  // "自信"
  candidate->value = "\xe8\x87\xaa\xe4\xbf\xa1";
  candidate = segment->add_candidate();
  // "自身"
  candidate->value = "\xe8\x87\xaa\xe8\xba\xab";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->ConvertToHalfASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  EXPECT_EQ("jishin", command.output().preedit().segment(0).value());

  command.Clear();
  session->ConvertToHalfASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  EXPECT_EQ("JISHIN", command.output().preedit().segment(0).value());

  command.Clear();
  session->ConvertToHalfASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  EXPECT_EQ("Jishin", command.output().preedit().segment(0).value());

  command.Clear();
  session->ConvertToHalfASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  EXPECT_EQ("jishin", command.output().preedit().segment(0).value());
}

TEST_F(SessionTest, ConvertToTransliterationWithMultipleSegments) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("like", session.get(), &command);

  Segments segments;
  SetLike(&segments);
  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  // Convert
  command.Clear();
  session->Convert(&command);
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
  session->TranslateHalfASCII(&command);
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

TEST_F(SessionTest, ConvertToHalfWidth) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("abc", session.get(), &command);

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    // "あｂｃ"
    segment->set_key("\xE3\x81\x82\xEF\xBD\x82\xEF\xBD\x83");
    // "あべし"
    segment->add_candidate()->value = "\xE3\x81\x82\xE3\x81\xB9\xE3\x81\x97";
  }
  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->ConvertToHalfWidth(&command);
  {  // Make sure the output
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    // "ｱbc"
    EXPECT_EQ("\xEF\xBD\xB1\x62\x63",
              command.output().preedit().segment(0).value());
  }

  command.Clear();
  session->ConvertToFullASCII(&command);
  // The output is "ａｂｃ".

  command.Clear();
  session->ConvertToHalfWidth(&command);
  {  // Make sure the output
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_EQ("abc", command.output().preedit().segment(0).value());
  }
}

TEST_F(SessionTest, ConvertConsonantsToFullAlphanumeric) {
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("dvd", session.get(), &command);

  segment = segments.add_segment();
  // "ｄｖｄ"
  segment->set_key("\xEF\xBD\x84\xEF\xBD\x96\xEF\xBD\x84");
  candidate = segment->add_candidate();
  candidate->value = "DVD";
  candidate = segment->add_candidate();
  candidate->value = "dvd";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->ConvertToFullASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "ｄｖｄ"
  EXPECT_EQ("\xEF\xBD\x84\xEF\xBD\x96\xEF\xBD\x84",
            command.output().preedit().segment(0).value());

  command.Clear();
  session->ConvertToFullASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "ＤＶＤ"
  EXPECT_EQ("\xEF\xBC\xA4\xEF\xBC\xB6\xEF\xBC\xA4",
            command.output().preedit().segment(0).value());

  command.Clear();
  session->ConvertToFullASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "Ｄｖｄ"
  EXPECT_EQ("\xEF\xBC\xA4\xEF\xBD\x96\xEF\xBD\x84",
            command.output().preedit().segment(0).value());

  command.Clear();
  session->ConvertToFullASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "ｄｖｄ"
  EXPECT_EQ("\xEF\xBD\x84\xEF\xBD\x96\xEF\xBD\x84",
            command.output().preedit().segment(0).value());
}

TEST_F(SessionTest, ConvertConsonantsToFullAlphanumericWithoutCascadingWindow) {
  config::Config config;
  config.set_use_cascading_window(false);
  SetConfig(config);

  commands::Command command;
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  command.Clear();
  InsertCharacterChars("dvd", session.get(), &command);

  segment = segments.add_segment();
  // "ｄｖｄ"
  segment->set_key("\xEF\xBD\x84\xEF\xBD\x96\xEF\xBD\x84");
  candidate = segment->add_candidate();
  candidate->value = "DVD";
  candidate = segment->add_candidate();
  candidate->value = "dvd";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->ConvertToFullASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "ｄｖｄ"
  EXPECT_EQ("\xEF\xBD\x84\xEF\xBD\x96\xEF\xBD\x84",
            command.output().preedit().segment(0).value());

  command.Clear();
  session->ConvertToFullASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "ＤＶＤ"
  EXPECT_EQ("\xEF\xBC\xA4\xEF\xBC\xB6\xEF\xBC\xA4",
            command.output().preedit().segment(0).value());

  command.Clear();
  session->ConvertToFullASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "Ｄｖｄ"
  EXPECT_EQ("\xEF\xBC\xA4\xEF\xBD\x96\xEF\xBD\x84",
            command.output().preedit().segment(0).value());

  command.Clear();
  session->ConvertToFullASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "ｄｖｄ"
  EXPECT_EQ("\xEF\xBD\x84\xEF\xBD\x96\xEF\xBD\x84",
            command.output().preedit().segment(0).value());
}

// Convert input string to Hiragana, Katakana, and Half Katakana
TEST_F(SessionTest, SwitchKanaType) {
  {  // From composition mode.
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    InsertCharacterChars("abc", session.get(), &command);

    Segments segments;
    {  // Initialize segments.
      Segment *segment = segments.add_segment();
      // "あｂｃ"
      segment->set_key("\xE3\x81\x82\xEF\xBD\x82\xEF\xBD\x83");
      // "あべし"
      segment->add_candidate()->value = "\xE3\x81\x82\xE3\x81\xB9\xE3\x81\x97";
    }

    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);

    command.Clear();
    session->SwitchKanaType(&command);
    {  // Make sure the output
      EXPECT_TRUE(command.output().has_preedit());
      EXPECT_EQ(1, command.output().preedit().segment_size());
      // "アｂｃ"
      EXPECT_EQ("\xE3\x82\xA2\xEF\xBD\x82\xEF\xBD\x83",
                command.output().preedit().segment(0).value());
    }

    command.Clear();
    session->SwitchKanaType(&command);
    {  // Make sure the output
      EXPECT_TRUE(command.output().has_preedit());
      EXPECT_EQ(1, command.output().preedit().segment_size());
      // "ｱbc"
      EXPECT_EQ("\xEF\xBD\xB1\x62\x63",
                command.output().preedit().segment(0).value());
    }

    command.Clear();
    session->SwitchKanaType(&command);
    {  // Make sure the output
      EXPECT_TRUE(command.output().has_preedit());
      EXPECT_EQ(1, command.output().preedit().segment_size());
      // "あｂｃ"
      EXPECT_EQ("\xE3\x81\x82\xEF\xBD\x82\xEF\xBD\x83",
                command.output().preedit().segment(0).value());
    }

    command.Clear();
    session->SwitchKanaType(&command);
    {  // Make sure the output
      EXPECT_TRUE(command.output().has_preedit());
      EXPECT_EQ(1, command.output().preedit().segment_size());
      // "アｂｃ"
      EXPECT_EQ("\xE3\x82\xA2\xEF\xBD\x82\xEF\xBD\x83",
                command.output().preedit().segment(0).value());
    }
  }

  {  // From conversion mode.
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    InsertCharacterChars("kanji", session.get(), &command);

    Segments segments;
    {  // Initialize segments.
      Segment *segment = segments.add_segment();
      // "かんじ"
      segment->set_key("\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x98");
      // "漢字"
      segment->add_candidate()->value = "\xE6\xBC\xA2\xE5\xAD\x97";
    }

    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);

    command.Clear();
    session->Convert(&command);
    {  // Make sure the output
      EXPECT_TRUE(command.output().has_preedit());
      EXPECT_EQ(1, command.output().preedit().segment_size());
      // "漢字"
      EXPECT_EQ("\xE6\xBC\xA2\xE5\xAD\x97",
                command.output().preedit().segment(0).value());
    }

    command.Clear();
    session->SwitchKanaType(&command);
    {  // Make sure the output
      EXPECT_TRUE(command.output().has_preedit());
      EXPECT_EQ(1, command.output().preedit().segment_size());
      // "かんじ"
      EXPECT_EQ("\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x98",
                command.output().preedit().segment(0).value());
    }

    command.Clear();
    session->SwitchKanaType(&command);
    {  // Make sure the output
      EXPECT_TRUE(command.output().has_preedit());
      EXPECT_EQ(1, command.output().preedit().segment_size());
      // "カンジ"
      EXPECT_EQ("\xE3\x82\xAB\xE3\x83\xB3\xE3\x82\xB8",
                command.output().preedit().segment(0).value());
    }

    command.Clear();
    session->SwitchKanaType(&command);
    {  // Make sure the output
      EXPECT_TRUE(command.output().has_preedit());
      EXPECT_EQ(1, command.output().preedit().segment_size());
      // "ｶﾝｼﾞ"
      EXPECT_EQ("\xEF\xBD\xB6\xEF\xBE\x9D\xEF\xBD\xBC\xEF\xBE\x9E",
                command.output().preedit().segment(0).value());
    }

    command.Clear();
    session->SwitchKanaType(&command);
    {  // Make sure the output
      EXPECT_TRUE(command.output().has_preedit());
      EXPECT_EQ(1, command.output().preedit().segment_size());
      // "かんじ"
      EXPECT_EQ("\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x98",
                command.output().preedit().segment(0).value());
    }
  }
}

// Rotate input mode among Hiragana, Katakana, and Half Katakana
TEST_F(SessionTest, InputModeSwitchKanaType) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  // HIRAGANA
  InsertCharacterChars("a", session.get(), &command);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", GetComposition(command));
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());

  // HIRAGANA to FULL_KATAKANA
  command.Clear();
  session->Commit(&command);
  command.Clear();
  session->InputModeSwitchKanaType(&command);
  InsertCharacterChars("a", session.get(), &command);
  // "ア"
  EXPECT_EQ("\xE3\x82\xA2", GetComposition(command));
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(commands::FULL_KATAKANA, command.output().mode());

  // FULL_KATRAKANA to HALF_KATAKANA
  command.Clear();
  session->Commit(&command);
  command.Clear();
  session->InputModeSwitchKanaType(&command);
  InsertCharacterChars("a", session.get(), &command);
  // "ｱ"
  EXPECT_EQ("\xEF\xBD\xB1",
            GetComposition(command));
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(commands::HALF_KATAKANA, command.output().mode());

  // HALF_KATAKANA to HIRAGANA
  command.Clear();
  session->Commit(&command);
  command.Clear();
  session->InputModeSwitchKanaType(&command);
  InsertCharacterChars("a", session.get(), &command);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", GetComposition(command));
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());

  // To Half ASCII mode.
  command.Clear();
  session->Commit(&command);
  command.Clear();
  session->InputModeHalfASCII(&command);
  InsertCharacterChars("a", session.get(), &command);
  // "a"
  EXPECT_EQ("a", GetComposition(command));
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

  // HALF_ASCII to HALF_ASCII
  command.Clear();
  session->Commit(&command);
  command.Clear();
  session->InputModeSwitchKanaType(&command);
  InsertCharacterChars("a", session.get(), &command);
  // "a"
  EXPECT_EQ("a", GetComposition(command));
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

  // To Full ASCII mode.
  command.Clear();
  session->Commit(&command);
  command.Clear();
  session->InputModeFullASCII(&command);
  InsertCharacterChars("a", session.get(), &command);
  // "ａ"
  EXPECT_EQ("\xEF\xBD\x81", GetComposition(command));
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(commands::FULL_ASCII, command.output().mode());

  // FULL_ASCII to FULL_ASCII
  command.Clear();
  session->Commit(&command);
  command.Clear();
  session->InputModeSwitchKanaType(&command);
  InsertCharacterChars("a", session.get(), &command);
  // "ａ"
  EXPECT_EQ("\xEF\xBD\x81", GetComposition(command));
  EXPECT_TRUE(command.output().has_mode());
  EXPECT_EQ(commands::FULL_ASCII, command.output().mode());
}

TEST_F(SessionTest, TranslateHalfWidth) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("abc", session.get(), &command);

  command.Clear();
  session->TranslateHalfWidth(&command);
  {  // Make sure the output
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    // "ｱbc"
    EXPECT_EQ("\xEF\xBD\xB1\x62\x63",
              command.output().preedit().segment(0).value());
  }

  command.Clear();
  session->TranslateFullASCII(&command);
  // The output is "ａｂｃ".

  command.Clear();
  session->TranslateHalfWidth(&command);
  {  // Make sure the output
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_EQ("abc", command.output().preedit().segment(0).value());
  }
}

TEST_F(SessionTest, UpdatePreferences) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("aiueo", session.get(), &command);
  Segments segments;
  SetAiueo(&segments);

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->Convert(&command);

  command.Clear();
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::SPACE);
  command.mutable_input()->mutable_config()->set_use_cascading_window(false);
  session->SendKey(&command);
  const size_t no_cascading_cand_size =
      command.output().candidates().candidate_size();

  command.Clear();
  session->ConvertCancel(&command);

  command.Clear();
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::SPACE);
  command.mutable_input()->mutable_config()->set_use_cascading_window(true);
  session->SendKey(&command);
  const size_t cascading_cand_size =
      command.output().candidates().candidate_size();

  EXPECT_GT(no_cascading_cand_size, cascading_cand_size);

  command.Clear();
  session->ConvertCancel(&command);

  // On MS-IME keymap, EISU key does nothing.
  command.Clear();
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::EISU);
  command.mutable_input()->mutable_config()->set_session_keymap(
      config::Config::MSIME);
  session->SendKey(&command);
  EXPECT_EQ(commands::HALF_ASCII, command.output().status().mode());

  // On KOTOERI keymap, EISU key does "ToggleAlphanumericMode".
  command.Clear();
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::EISU);
  command.mutable_input()->mutable_config()->set_session_keymap(
      config::Config::KOTOERI);
  session->SendKey(&command);
  EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
}

TEST_F(SessionTest, RomajiInput) {
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  composer::Table table;
  // "ぱ"
  table.AddRule("pa", "\xe3\x81\xb1", "");
  // "ん"
  table.AddRule("n", "\xe3\x82\x93", "");
  // "な"
  table.AddRule("na", "\xe3\x81\xaa", "");
  // This rule makes the "n" rule ambiguous.

  scoped_ptr<Session> session(new Session);
  session->get_internal_composer_only_for_unittest()->SetTable(&table);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("pan", session.get(), &command);

  // "ぱｎ"
  EXPECT_EQ("\xe3\x81\xb1\xef\xbd\x8e",
            command.output().preedit().segment(0).value());

  command.Clear();

  segment = segments.add_segment();
  // "ぱん"
  segment->set_key("\xe3\x81\xb1\xe3\x82\x93");
  candidate = segment->add_candidate();
  // "パン"
  candidate->value = "\xe3\x83\x91\xe3\x83\xb3";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  session->ConvertToHiragana(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "ぱん"
  EXPECT_EQ("\xe3\x81\xb1\xe3\x82\x93",
            command.output().preedit().segment(0).value());

  command.Clear();
  session->ConvertToHalfASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  EXPECT_EQ("pan", command.output().preedit().segment(0).value());
}


TEST_F(SessionTest, KanaInput) {
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  composer::Table table;
  // "す゛", "ず"
  table.AddRule("\xe3\x81\x99\xe3\x82\x9b", "\xe3\x81\x9a", "");

  scoped_ptr<Session> session(new Session);
  session->get_internal_composer_only_for_unittest()->SetTable(&table);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  command.mutable_input()->mutable_key()->set_key_code('m');
  // "も"
  command.mutable_input()->mutable_key()->set_key_string("\xe3\x82\x82");
  session->SendKey(&command);

  command.Clear();
  command.mutable_input()->mutable_key()->set_key_code('r');
  // "す"
  command.mutable_input()->mutable_key()->set_key_string("\xe3\x81\x99");
  session->SendKey(&command);

  command.Clear();
  command.mutable_input()->mutable_key()->set_key_code('@');
  // "゛"
  command.mutable_input()->mutable_key()->set_key_string("\xe3\x82\x9b");
  session->SendKey(&command);

  command.Clear();
  command.mutable_input()->mutable_key()->set_key_code('h');
  // "く"
  command.mutable_input()->mutable_key()->set_key_string("\xe3\x81\x8f");
  session->SendKey(&command);

  command.Clear();
  command.mutable_input()->mutable_key()->set_key_code('!');
  command.mutable_input()->mutable_key()->set_key_string("!");
  session->SendKey(&command);

  // "もずく！"
  EXPECT_EQ("\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f\xef\xbc\x81",
            command.output().preedit().segment(0).value());

  segment = segments.add_segment();
  // "もずく!"
  segment->set_key("\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f!");
  candidate = segment->add_candidate();
  // "もずく！"
  candidate->value = "\xe3\x82\x82\xe3\x81\x9a\xe3\x81\x8f\xef\xbc\x81";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->ConvertToHalfASCII(&command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  EXPECT_EQ("mr@h!", command.output().preedit().segment(0).value());
}

TEST_F(SessionTest, ExceededComposition) {
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  const string exceeded_preedit(500, 'a');
  ASSERT_EQ(500, exceeded_preedit.size());
  InsertCharacterChars(exceeded_preedit, session.get(), &command);

  string long_a;
  for (int i = 0; i < 500; ++i) {
    // "あ"
    long_a += "\xe3\x81\x82";
  }
  segment = segments.add_segment();
  segment->set_key(long_a);
  candidate = segment->add_candidate();
  candidate->value = long_a;

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->Convert(&command);
  EXPECT_FALSE(command.output().has_candidates());

  // The status should remain the preedit status, although the
  // previous command was convert.  The next command makes sure that
  // the preedit will disappear by canceling the preedit status.
  command.Clear();
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::ESCAPE);
  EXPECT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, OutputAllCandidateWords) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  Segments segments;
  SetAiueo(&segments);
  InsertCharacterChars("aiueo", session.get(), &command);

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->Convert(&command);
  {
    const commands::Output &output = command.output();
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(0, output.all_candidate_words().focused_index());
    EXPECT_EQ(commands::CONVERSION, output.all_candidate_words().category());
#ifdef OS_LINUX
    // Cascading window is not supported on Linux, so the size of
    // candidate words is different from other platform.
    // TODO(komatsu): Modify the client for Linux to explicitly change
    // the preference rather than relying on the exceptional default value.
    // [ "あいうえお", "アイウエオ",
    //   "aiueo" (t13n), "AIUEO" (t13n), "Aieuo" (t13n),
    //   "ａｉｕｅｏ"  (t13n), "ＡＩＵＥＯ" (t13n), "Ａｉｅｕｏ" (t13n),
    //   "ｱｲｳｴｵ" (t13n) ]
    EXPECT_EQ(9, output.all_candidate_words().candidates_size());
#else
    // [ "あいうえお", "アイウエオ", "アイウエオ" (t13n), "あいうえお" (t13n),
    //   "aiueo" (t13n), "AIUEO" (t13n), "Aieuo" (t13n),
    //   "ａｉｕｅｏ"  (t13n), "ＡＩＵＥＯ" (t13n), "Ａｉｅｕｏ" (t13n),
    //   "ｱｲｳｴｵ" (t13n) ]
    EXPECT_EQ(11, output.all_candidate_words().candidates_size());
#endif  // OS_LINUX
  }

  command.Clear();
  session->ConvertNext(&command);
  {
    const commands::Output &output = command.output();

    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(1, output.all_candidate_words().focused_index());
    EXPECT_EQ(commands::CONVERSION, output.all_candidate_words().category());
#ifdef OS_LINUX
    // Cascading window is not supported on Linux, so the size of
    // candidate words is different from other platform.
    // TODO(komatsu): Modify the client for Linux to explicitly change
    // the preference rather than relying on the exceptional default value.
    // [ "あいうえお", "アイウエオ", "アイウエオ" (t13n), "あいうえお" (t13n),
    //   "aiueo" (t13n), "AIUEO" (t13n), "Aieuo" (t13n),
    //   "ａｉｕｅｏ"  (t13n), "ＡＩＵＥＯ" (t13n), "Ａｉｅｕｏ" (t13n),
    //   "ｱｲｳｴｵ" (t13n) ]
    EXPECT_EQ(9, output.all_candidate_words().candidates_size());
#else
    // [ "あいうえお", "アイウエオ",
    //   "aiueo" (t13n), "AIUEO" (t13n), "Aieuo" (t13n),
    //   "ａｉｕｅｏ"  (t13n), "ＡＩＵＥＯ" (t13n), "Ａｉｅｕｏ" (t13n),
    //   "ｱｲｳｴｵ" (t13n) ]
    EXPECT_EQ(11, output.all_candidate_words().candidates_size());
#endif  // OS_LINUX
  }
}

TEST_F(SessionTest, UndoForComposition) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session->set_client_capability(capability);

  commands::Command command;
  Segments segments;

  {  // Undo for CommitFirstSuggestion
    SetAiueo(&segments);
    convertermock_->SetStartSuggestionWithComposer(&segments, true);
    InsertCharacterChars("ai", session.get(), &command);
    SetComposer(session.get(), &segments);
    // "あい"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84", GetComposition(command));

    command.Clear();
    session->CommitFirstSuggestion(&command);
    EXPECT_TRUE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              command.output().result().value());
    EXPECT_EQ(ImeContext::PRECOMPOSITION, session->context().state());

    command.Clear();
    session->Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(-5, command.output().deletion_range().offset());
    EXPECT_EQ(5, command.output().deletion_range().length());
    EXPECT_TRUE(command.output().has_preedit());
    // "あい"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84", GetComposition(command));
    EXPECT_EQ(2, command.output().candidates().size());
    EXPECT_EQ(ImeContext::COMPOSITION, session->context().state());
  }
}

TEST_F(SessionTest, RequestUndo) {
  scoped_ptr<Session> session(new Session);
  commands::Command command;

  class TestData {
   public:
    ImeContext::State state_;
    bool expected_processed_;
    TestData(ImeContext::State state, bool expected_consumed) :
        state_(state), expected_processed_(expected_consumed) {}
  };
  const TestData test_data_list[] = {
    TestData(ImeContext::DIRECT, false),
    TestData(ImeContext::PRECOMPOSITION, true),
    TestData(ImeContext::COMPOSITION, true),
    TestData(ImeContext::CONVERSION, true),
  };

  for (size_t i = 0; i < ARRAYSIZE(test_data_list); ++i) {
    const TestData &test_data = test_data_list[i];
    command.Clear();

    // Make sure that the undo context is not empty because RequestUndo is
    // expected to ignore the key event when the undo context is empty and.
    // the context state is ImeContext::PRECOMPOSITION.  See b/5553298.
    SetUndoContext(session.get());

    session->context_->set_state(test_data.state_);
    session->RequestUndo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_deletion_range());
    if (test_data.expected_processed_) {
      EXPECT_TRUE(command.output().consumed());
      EXPECT_TRUE(command.output().has_callback());
      EXPECT_TRUE(command.output().callback().has_session_command());
      EXPECT_EQ(commands::SessionCommand::UNDO,
                command.output().callback().session_command().type());
    } else {
      EXPECT_FALSE(command.output().has_callback());
    }
  }
}

TEST_F(SessionTest, UndoForSingleSegment) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session->set_client_capability(capability);

  commands::Command command;
  Segments segments;

  {  // Create segments
    InsertCharacterChars("aiueo", session.get(), &command);
    SetComposer(session.get(), &segments);
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
    convertermock_->SetStartConversionWithComposer(&segments, true);
    command.Clear();
    session->Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              GetComposition(command));

    convertermock_->SetCommitSegmentValue(&segments, true);
    command.Clear();
    session->Commit(&command);
    EXPECT_TRUE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              command.output().result().value());

    command.Clear();
    session->Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(-5, command.output().deletion_range().offset());
    EXPECT_EQ(5, command.output().deletion_range().length());
    EXPECT_TRUE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              GetComposition(command));

    // Undo twice - do nothing and keep the previous status.
    command.Clear();
    session->Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_deletion_range());
    EXPECT_TRUE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              GetComposition(command));
  }

  {  // Undo after commitment of conversion
    command.Clear();
    session->ConvertNext(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_preedit());
    // "アイウエオ"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6\xe3\x82\xa8\xe3\x82\xaa",
              GetComposition(command));

    convertermock_->SetCommitSegmentValue(&segments, true);
    command.Clear();
    session->Commit(&command);
    EXPECT_TRUE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());
    // "アイウエオ"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6\xe3\x82\xa8\xe3\x82\xaa",
              command.output().result().value());

    command.Clear();
    session->Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(-5, command.output().deletion_range().offset());
    EXPECT_EQ(5, command.output().deletion_range().length());
    EXPECT_TRUE(command.output().has_preedit());
    // "アイウエオ"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6\xe3\x82\xa8\xe3\x82\xaa",
              GetComposition(command));

    // Undo twice - do nothing and keep the previous status.
    command.Clear();
    session->Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_deletion_range());
    EXPECT_TRUE(command.output().has_preedit());
    // "アイウエオ"
    EXPECT_EQ("\xe3\x82\xa2\xe3\x82\xa4\xe3\x82\xa6\xe3\x82\xa8\xe3\x82\xaa",
              GetComposition(command));
  }

  {  // Undo after commitment of conversion with Ctrl-Backspace.
    command.Clear();
    session->ConvertNext(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ("aiueo", GetComposition(command));

    convertermock_->SetCommitSegmentValue(&segments, true);
    command.Clear();
    session->Commit(&command);
    EXPECT_TRUE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_EQ("aiueo", command.output().result().value());

    config::Config config;
    config.set_session_keymap(config::Config::MSIME);
    SetConfig(config);

    command.Clear();
    session->Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(-5, command.output().deletion_range().offset());
    EXPECT_EQ(5, command.output().deletion_range().length());
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ("aiueo", GetComposition(command));
  }

  {
    // If capability does not support DELETE_PRECEDIGN_TEXT, Undo is not
    // performed.
    convertermock_->SetCommitSegmentValue(&segments, true);
    command.Clear();
    session->Commit(&command);
    EXPECT_TRUE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_EQ("aiueo", command.output().result().value());

    // Reset capability
    capability.Clear();
    session->set_client_capability(capability);

    command.Clear();
    session->Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_deletion_range());
    EXPECT_FALSE(command.output().has_preedit());
  }
}

TEST_F(SessionTest, ClearUndoContextByKeyEvent_Issue5529702) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session->set_client_capability(capability);

  SetUndoContext(session.get());

  commands::Command command;

  // Modifier key event does not clear undo context.
  command.Clear();
  command.mutable_input()->mutable_key()->add_modifier_keys(
      commands::KeyEvent::SHIFT);
  session->SendKey(&command);

  // Ctrl+BS should be consumed as UNDO.
  command.Clear();
  command.mutable_input()->mutable_key()->add_modifier_keys(
      commands::KeyEvent::CTRL);
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::BACKSPACE);
  command.mutable_input()->mutable_config()->set_session_keymap(
      config::Config::MSIME);
  session->TestSendKey(&command);
  EXPECT_TRUE(command.output().consumed());

  // Any other (test) send key event clears undo context.
  command.Clear();
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::LEFT);
  session->TestSendKey(&command);
  EXPECT_FALSE(command.output().consumed());

  // Undo context is just cleared. Ctrl+BS should not be consumed b/5553298.
  command.Clear();
  command.mutable_input()->mutable_key()->add_modifier_keys(
      commands::KeyEvent::CTRL);
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::BACKSPACE);
  command.mutable_input()->mutable_config()->set_session_keymap(
      config::Config::MSIME);
  session->TestSendKey(&command);
  EXPECT_FALSE(command.output().consumed());
}


TEST_F(SessionTest, NeedlessClearUndoContext) {
  // This is a unittest against http://b/3423910.

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session->set_client_capability(capability);
  commands::Command command;

  {  // Conversion -> Send Shift -> Undo
    Segments segments;
    InsertCharacterChars("aiueo", session.get(), &command);
    SetComposer(session.get(), &segments);
    SetAiueo(&segments);
    FillT13Ns(&segments);

    convertermock_->SetStartConversionWithComposer(&segments, true);
    command.Clear();
    session->Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              GetComposition(command));

    convertermock_->SetCommitSegmentValue(&segments, true);
    command.Clear();
    session->Commit(&command);
    EXPECT_TRUE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              command.output().result().value());

    command.Clear();
    SendKey("Shift", session.get(), &command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());

    command.Clear();
    session->Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(-5, command.output().deletion_range().offset());
    EXPECT_EQ(5, command.output().deletion_range().length());
    EXPECT_TRUE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              GetComposition(command));
  }

  {  // Type "aiueo" -> Convert -> Type "a" -> Escape -> Undo
    Segments segments;
    InsertCharacterChars("aiueo", session.get(), &command);
    SetComposer(session.get(), &segments);
    SetAiueo(&segments);
    FillT13Ns(&segments);

    command.Clear();
    session->Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              GetComposition(command));

    command.Clear();
    SendKey("a", session.get(), &command);
    EXPECT_TRUE(command.output().has_result());
    EXPECT_TRUE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              command.output().result().value());
    // "あ"
    EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).value());

    command.Clear();
    SendKey("Escape", session.get(), &command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());

    command.Clear();
    session->Undo(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_deletion_range());
    EXPECT_EQ(-5, command.output().deletion_range().offset());
    EXPECT_EQ(5, command.output().deletion_range().length());
    EXPECT_TRUE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              GetComposition(command));
  }
}

TEST_F(SessionTest, ClearUndoContextAfterDirectInputAfterConversion) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session->set_client_capability(capability);
  commands::Command command;

  // Prepare Numpad
  config::Config config;
  config.set_numpad_character_form(config::Config::NUMPAD_DIRECT_INPUT);
  SetConfig(config);
  ASSERT_EQ(config::Config::NUMPAD_DIRECT_INPUT,
            GET_CONFIG(numpad_character_form));

  // Cleate segments
  Segments segments;
  InsertCharacterChars("aiueo", session.get(), &command);
  SetComposer(session.get(), &segments);
  SetAiueo(&segments);
  FillT13Ns(&segments);

  // Convert
  convertermock_->SetStartConversionWithComposer(&segments, true);
  command.Clear();
  session->Convert(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(command.output().has_preedit());
  // "あいうえお"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
            GetComposition(command));

  // Direct input
  command.Clear();
  SendKey("Numpad0", session.get(), &command);
  EXPECT_TRUE(GetComposition(command).empty());
  EXPECT_TRUE(command.output().has_result());
  // "あいうえお0"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A" "0",
            command.output().result().value());

  // Undo - Do NOT nothing
  command.Clear();
  session->Undo(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_deletion_range());
  EXPECT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, TemporaryInputModeAfterUndo) {
  // This is a unittest against http://b/3423599.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session->set_client_capability(capability);
  commands::Command command;

  // Shift + Ascii triggers temporary input mode switch.
  command.Clear();
  SendKey("A", session.get(), &command);
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());
  command.Clear();
  SendKey("Enter", session.get(), &command);
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());

  // Undo and keep temporary input mode correct
  command.Clear();
  session->Undo(&command);
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_EQ("A", GetComposition(command));
  command.Clear();
  SendKey("Enter", session.get(), &command);
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());

  // Undo and input additional "A" with temporary input mode.
  command.Clear();
  session->Undo(&command);
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());
  command.Clear();
  SendKey("A", session.get(), &command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_EQ("AA", GetComposition(command));
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

  // Input additional "a" with original input mode.
  command.Clear();
  SendKey("a", session.get(), &command);
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "AAあ"
  EXPECT_EQ("AA\xE3\x81\x82", GetComposition(command));

  // Submit and Undo
  command.Clear();
  SendKey("Enter", session.get(), &command);
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  command.Clear();
  session->Undo(&command);
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "AAあ"
  EXPECT_EQ("AA\xE3\x81\x82", GetComposition(command));

  // Input additional "Aa"
  command.Clear();
  SendKey("A", session.get(), &command);
  command.Clear();
  SendKey("a", session.get(), &command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  EXPECT_EQ("AA\xE3\x81\x82" "Aa", GetComposition(command));
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

  // Submit and Undo
  command.Clear();
  SendKey("Enter", session.get(), &command);
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  command.Clear();
  session->Undo(&command);
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
  // "AAあAa"
  EXPECT_EQ("AA\xE3\x81\x82" "Aa", GetComposition(command));
}

TEST_F(SessionTest, DCHECKFailureAfterUndo) {
  // This is a unittest against http://b/3437358.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session->set_client_capability(capability);
  commands::Command command;

  command.Clear();
  InsertCharacterChars("abe", session.get(), &command);
  command.Clear();
  session->Commit(&command);
  command.Clear();
  session->Undo(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(command.output().has_preedit());
  // "あべ"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\xB9", GetComposition(command));

  command.Clear();
  InsertCharacterChars("s", session.get(), &command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(command.output().has_preedit());
  // "あべｓ"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\xB9\xEF\xBD\x93", GetComposition(command));

  command.Clear();
  InsertCharacterChars("h", session.get(), &command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(command.output().has_preedit());
  // "あべｓｈ"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\xB9\xEF\xBD\x93\xEF\xBD\x88",
            GetComposition(command));

  command.Clear();
  InsertCharacterChars("i", session.get(), &command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(command.output().has_preedit());
  // "あべし"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\xB9\xE3\x81\x97", GetComposition(command));
}



TEST_F(SessionTest, Issue1805239) {
  // This is a unittest against http://b/1805239.
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("watasinonamae", session.get(), &command);

  segment = segments.add_segment();
  // "わたしの"
  segment->set_key("\xe3\x82\x8f\xe3\x81\x9f\xe3\x81\x97\xe3\x81\xae");
  candidate = segment->add_candidate();
  // "私の"
  candidate->value = "\xe7\xa7\x81\xe3\x81\xae";
  candidate = segment->add_candidate();
  // "渡しの"
  candidate->value = "\xe6\xb8\xa1\xe3\x81\x97\xe3\x81\xae";
  segment = segments.add_segment();
  // "名前"
  segment->set_key("\xe5\x90\x8d\xe5\x89\x8d");
  candidate = segment->add_candidate();
  // "なまえ"
  candidate->value = "\xe3\x81\xaa\xe3\x81\xbe\xe3\x81\x88";
  candidate = segment->add_candidate();
  // "ナマエ"
  candidate->value = "\xe3\x83\x8a\xe3\x83\x9e\xe3\x82\xa8";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::RIGHT, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
  EXPECT_TRUE(command.output().has_candidates());

  SendSpecialKey(commands::KeyEvent::LEFT, session.get(), &command);
  EXPECT_FALSE(command.output().has_candidates());

  SendSpecialKey(commands::KeyEvent::RIGHT, session.get(), &command);
  EXPECT_FALSE(command.output().has_candidates());

  SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
  EXPECT_TRUE(command.output().has_candidates());

  SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
  EXPECT_TRUE(command.output().has_candidates());

  SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
  EXPECT_TRUE(command.output().has_candidates());

  SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
  EXPECT_TRUE(command.output().has_candidates());
}

TEST_F(SessionTest, Issue1816861) {
  // This is a unittest against http://b/1816861
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("kamabokonoinbou", session.get(), &command);
  segment = segments.add_segment();
  // "かまぼこの"
  segment->set_key(
      "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae");
  candidate = segment->add_candidate();
  // "かまぼこの"
  candidate->value
      = "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
  candidate = segment->add_candidate();
  // "カマボコの"
  candidate->value
      = "\xe3\x82\xab\xe3\x83\x9e\xe3\x83\x9c\xe3\x82\xb3\xe3\x81\xae";
  segment = segments.add_segment();
  // "いんぼう"
  segment->set_key("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86");
  candidate = segment->add_candidate();
  // "陰謀"
  candidate->value = "\xe9\x99\xb0\xe8\xac\x80";
  candidate = segment->add_candidate();
  // "印房"
  candidate->value = "\xe5\x8d\xb0\xe6\x88\xbf";

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::RIGHT, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::LEFT, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::LEFT, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::LEFT, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::LEFT, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, session.get(), &command);
  SendSpecialKey(commands::KeyEvent::BACKSPACE, session.get(), &command);

  segments.Clear();
  segment = segments.add_segment();
  // "いんぼう"
  segment->set_key("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86");
  candidate = segment->add_candidate();
  // "陰謀"
  candidate->value = "\xe9\x99\xb0\xe8\xac\x80";
  candidate = segment->add_candidate();
  // "陰謀論"
  candidate->value = "\xe9\x99\xb0\xe8\xac\x80\xe8\xab\x96";
  candidate = segment->add_candidate();
  // "陰謀説"
  candidate->value = "\xe9\x99\xb0\xe8\xac\x80\xe8\xaa\xac";

  convertermock_->SetStartPredictionWithComposer(&segments, true);

  SendSpecialKey(commands::KeyEvent::TAB, session.get(), &command);
}

TEST_F(SessionTest, T13NWithResegmentation) {
  // This is a unittest against http://b/3272827
  Segment::Candidate *candidate;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("kamabokonoinbou", session.get(), &command);

  {
    Segments segments;
    Segment *segment;
    segment = segments.add_segment();
    // "かまぼこの"
    segment->set_key(
        "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae");
    candidate = segment->add_candidate();
    // "かまぼこの"
    candidate->value
        = "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
    candidate = segment->add_candidate();
    // "カマボコの"
    candidate->value
        = "\xe3\x82\xab\xe3\x83\x9e\xe3\x83\x9c\xe3\x82\xb3\xe3\x81\xae";

    segment = segments.add_segment();
    // "いんぼう"
    segment->set_key("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86");
    candidate = segment->add_candidate();
    // "陰謀"
    candidate->value = "\xe9\x99\xb0\xe8\xac\x80";
    candidate = segment->add_candidate();
    // "印房"
    candidate->value = "\xe5\x8d\xb0\xe6\x88\xbf";
    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);
  }
  {
    Segments segments;
    Segment *segment;
    segment = segments.add_segment();
    // "かまぼこの"
    segment->set_key(
        "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae");
    candidate = segment->add_candidate();
    // "かまぼこの"
    candidate->value
        = "\xe3\x81\x8b\xe3\x81\xbe\xe3\x81\xbc\xe3\x81\x93\xe3\x81\xae";
    candidate = segment->add_candidate();
    // "カマボコの"
    candidate->value
        = "\xe3\x82\xab\xe3\x83\x9e\xe3\x83\x9c\xe3\x82\xb3\xe3\x81\xae";

    segment = segments.add_segment();
    // "いんぼ"
    segment->set_key("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc");
    candidate = segment->add_candidate();
    // "いんぼ"
    candidate->value = "\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc";
    candidate = segment->add_candidate();
    // "インボ"
    candidate->value = "\xe3\x82\xa4\xe3\x83\xb3\xe3\x83\x9c";

    segment = segments.add_segment();
    // "う"
    segment->set_key("\xe3\x81\x86");
    candidate = segment->add_candidate();
    // "ウ"
    candidate->value = "\xe3\x82\xa6";
    candidate = segment->add_candidate();
    // "卯"
    candidate->value = "\xe5\x8d\xaf";

    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetResizeSegment1(&segments, true);
  }

  // Start conversion
  SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
  // Select second segment
  SendSpecialKey(commands::KeyEvent::RIGHT, session.get(), &command);
  // Shrink segment
  SendKey("Shift left", session.get(), &command);
  // Convert to T13N (Half katakana)
  SendKey("F8", session.get(), &command);

  // "ｲﾝﾎﾞ"
  EXPECT_EQ("\xef\xbd\xb2\xef\xbe\x9d\xef\xbe\x8e\xef\xbe\x9e",
            command.output().preedit().segment(1).value());
}

TEST_F(SessionTest, Shortcut) {
  const config::Config::SelectionShortcut kDataShortcut[] = {
    config::Config::NO_SHORTCUT,
    config::Config::SHORTCUT_123456789,
    config::Config::SHORTCUT_ASDFGHJKL,
  };
  const string kDataExpected[][2] = {
    {"", ""},
    {"1", "2"},
    {"a", "s"},
  };
  for (size_t i = 0; i < arraysize(kDataShortcut); ++i) {
    config::Config::SelectionShortcut shortcut = kDataShortcut[i];
    const string *expected = kDataExpected[i];

    config::Config config;
    config.set_selection_shortcut(shortcut);
    SetConfig(config);
    ASSERT_EQ(shortcut, GET_CONFIG(selection_shortcut));

    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());

    Segments segments;
    SetAiueo(&segments);
    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);

    commands::Command command;
    InsertCharacterChars("aiueo", session.get(), &command);

    command.Clear();
    session->Convert(&command);

    command.Clear();
    // Convert next
    SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
    ASSERT_TRUE(command.output().has_candidates());
    const commands::Candidates &candidates = command.output().candidates();
    EXPECT_EQ(expected[0], candidates.candidate(0).annotation().shortcut());
    EXPECT_EQ(expected[1], candidates.candidate(1).annotation().shortcut());
  }
}

TEST_F(SessionTest, ShortcutWithCapsLock_Issue5655743) {
  config::Config config;
  config.set_selection_shortcut(config::Config::SHORTCUT_ASDFGHJKL);
  SetConfig(config);
  ASSERT_EQ(config::Config::SHORTCUT_ASDFGHJKL,
            GET_CONFIG(selection_shortcut));

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  Segments segments;
  SetAiueo(&segments);
  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  commands::Command command;
  InsertCharacterChars("aiueo", session.get(), &command);

  command.Clear();
  session->Convert(&command);

  command.Clear();
  // Convert next
  SendSpecialKey(commands::KeyEvent::SPACE, session.get(), &command);
  ASSERT_TRUE(command.output().has_candidates());

  const commands::Candidates &candidates = command.output().candidates();
  EXPECT_EQ("a", candidates.candidate(0).annotation().shortcut());
  EXPECT_EQ("s", candidates.candidate(1).annotation().shortcut());

  // Select the second candidate by 's' key when the CapsLock is enabled.
  // Note that "CAPS S" means that 's' key is pressed w/o shift key.
  // See the description in command.proto.
  EXPECT_TRUE(SendKey("CAPS S", session.get(), &command));
  EXPECT_TRUE(command.output().consumed());
  // "アイウエオ"
  EXPECT_EQ("\xE3\x82\xA2\xE3\x82\xA4\xE3\x82\xA6\xE3\x82\xA8\xE3\x82\xAA",
            GetComposition(command));
}

TEST_F(SessionTest, NumpadKey) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  config::Config config;
  config.set_numpad_character_form(config::Config::NUMPAD_DIRECT_INPUT);
  SetConfig(config);
  ASSERT_EQ(config::Config::NUMPAD_DIRECT_INPUT,
            GET_CONFIG(numpad_character_form));

  // In the Precomposition state, numpad keys should not be consumed.
  EXPECT_TRUE(TestSendKey("Numpad1", session.get(), &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(SendKey("Numpad1", session.get(), &command));
  EXPECT_FALSE(command.output().consumed());

  EXPECT_TRUE(TestSendKey("Add", session.get(), &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(SendKey("Add", session.get(), &command));
  EXPECT_FALSE(command.output().consumed());

  EXPECT_TRUE(TestSendKey("Equals", session.get(), &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(SendKey("Equals", session.get(), &command));
  EXPECT_FALSE(command.output().consumed());

  EXPECT_TRUE(TestSendKey("Separator", session.get(), &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(SendKey("Separator", session.get(), &command));
  EXPECT_FALSE(command.output().consumed());

  EXPECT_TRUE(GetComposition(command).empty());

  config.set_numpad_character_form(config::Config::NUMPAD_HALF_WIDTH);
  SetConfig(config);
  ASSERT_EQ(config::Config::NUMPAD_HALF_WIDTH,
            GET_CONFIG(numpad_character_form));

  // In the Precomposition state, numpad keys should not be consumed.
  EXPECT_TRUE(TestSendKey("Numpad1", session.get(), &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(SendKey("Numpad1", session.get(), &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ("1", GetComposition(command));

  EXPECT_TRUE(TestSendKey("Add", session.get(), &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(SendKey("Add", session.get(), &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ("1+", GetComposition(command));

  EXPECT_TRUE(TestSendKey("Equals", session.get(), &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(SendKey("Equals", session.get(), &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ("1+=", GetComposition(command));

  EXPECT_TRUE(TestSendKey("Separator", session.get(), &command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(SendKey("Separator", session.get(), &command));
  EXPECT_TRUE(command.output().consumed());

  EXPECT_TRUE(GetComposition(command).empty());

  // "0" should be treated as full-width "０".
  EXPECT_TRUE(TestSendKey("0", session.get(), &command));
  EXPECT_TRUE(SendKey("0", session.get(), &command));

  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());

  EXPECT_TRUE(command.output().preedit().segment(0).has_value());
  EXPECT_TRUE(command.output().preedit().segment(0).has_key());
  // "０"
  EXPECT_EQ("\xEF\xBC\x90", command.output().preedit().segment(0).key());
  EXPECT_EQ("\xEF\xBC\x90", command.output().preedit().segment(0).value());

  // In the Composition state, DIVIDE on the pre-edit should be treated as "/".
  EXPECT_TRUE(TestSendKey("Divide", session.get(), &command));
  EXPECT_TRUE(SendKey("Divide", session.get(), &command));

  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());

  EXPECT_TRUE(command.output().preedit().segment(0).has_value());
  EXPECT_TRUE(command.output().preedit().segment(0).has_key());
  // "０/"
  EXPECT_EQ("\xEF\xBC\x90\x2F", command.output().preedit().segment(0).key());
  EXPECT_EQ("\xEF\xBC\x90\x2F", command.output().preedit().segment(0).value());

  // In the Composition state, "Numpad0" should be treated as half-width "0".
  EXPECT_TRUE(SendKey("Numpad0", session.get(), &command));

  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());

  EXPECT_TRUE(command.output().preedit().segment(0).has_value());
  EXPECT_TRUE(command.output().preedit().segment(0).has_key());
  // "０/0"
  EXPECT_EQ("\xEF\xBC\x90\x2F\x30",
            command.output().preedit().segment(0).key());
  EXPECT_EQ("\xEF\xBC\x90\x2F\x30",
            command.output().preedit().segment(0).value());

  // Separator should be treated as Enter.
  EXPECT_TRUE(TestSendKey("Separator", session.get(), &command));
  EXPECT_TRUE(SendKey("Separator", session.get(), &command));

  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_TRUE(command.output().has_result());
  // "０/0"
  EXPECT_EQ("\xEF\xBC\x90\x2F\x30", command.output().result().value());

  // http://b/2097087
  EXPECT_TRUE(SendKey("0", session.get(), &command));
  // "０"
  EXPECT_EQ("\xEF\xBC\x90", command.output().preedit().segment(0).key());
  EXPECT_EQ("\xEF\xBC\x90", command.output().preedit().segment(0).value());

  EXPECT_TRUE(SendKey("Divide", session.get(), &command));
  EXPECT_TRUE(command.output().has_preedit());
  // "０/"
  EXPECT_EQ("\xEF\xBC\x90\x2F", command.output().preedit().segment(0).key());
  EXPECT_EQ("\xEF\xBC\x90\x2F", command.output().preedit().segment(0).value());

  EXPECT_TRUE(SendKey("Divide", session.get(), &command));
  // "０//"
  EXPECT_EQ("\xEF\xBC\x90\x2F\x2F",
            command.output().preedit().segment(0).key());
  EXPECT_EQ("\xEF\xBC\x90\x2F\x2F",
            command.output().preedit().segment(0).value());

  EXPECT_TRUE(SendKey("Subtract", session.get(), &command));
  EXPECT_TRUE(SendKey("Subtract", session.get(), &command));
  EXPECT_TRUE(SendKey("Decimal", session.get(), &command));
  EXPECT_TRUE(SendKey("Decimal", session.get(), &command));
  EXPECT_TRUE(command.output().has_preedit());
  // "０//--.."
  EXPECT_EQ("\xEF\xBC\x90\x2F\x2F\x2D\x2D\x2E\x2E",
            command.output().preedit().segment(0).key());
  EXPECT_EQ("\xEF\xBC\x90\x2F\x2F\x2D\x2D\x2E\x2E",
            command.output().preedit().segment(0).value());
}

TEST_F(SessionTest, KanaSymbols) {
  config::Config config;
  config.set_punctuation_method(config::Config::COMMA_PERIOD);
  config.set_symbol_method(config::Config::CORNER_BRACKET_SLASH);
  SetConfig(config);
  ASSERT_EQ(config::Config::COMMA_PERIOD, GET_CONFIG(punctuation_method));
  ASSERT_EQ(config::Config::CORNER_BRACKET_SLASH, GET_CONFIG(symbol_method));

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  {
    commands::Command command;
    commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
    key_event->set_key_code('<');
    // "、"
    key_event->set_key_string("\xe3\x80\x81");
    EXPECT_TRUE(session->SendKey(&command));
    // "，"
    EXPECT_EQ(static_cast<uint32>(','), command.input().key().key_code());
    EXPECT_EQ("\xef\xbc\x8c", command.input().key().key_string());
    EXPECT_EQ("\xef\xbc\x8c", command.output().preedit().segment(0).value());
  }
  {
    commands::Command command;
    session->EditCancel(&command);
  }
  {
    commands::Command command;
    commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
    key_event->set_key_code('?');
    // "・"
    key_event->set_key_string("\xe3\x83\xbb");
    EXPECT_TRUE(session->SendKey(&command));
    // "／"
    EXPECT_EQ(static_cast<uint32>('/'), command.input().key().key_code());
    EXPECT_EQ("\xef\xbc\x8f", command.input().key().key_string());
    EXPECT_EQ("\xef\xbc\x8f", command.output().preedit().segment(0).value());
  }
}

TEST_F(SessionTest, InsertCharacterWithShiftKey) {
  {  // Basic behavior
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    EXPECT_TRUE(SendKey("a", session.get(), &command));
    EXPECT_TRUE(SendKey("A", session.get(), &command));  // "あA"
    EXPECT_TRUE(SendKey("a", session.get(), &command));  // "あAa"
    // Shift reverts the input mode to Hiragana.
    EXPECT_TRUE(SendKey("Shift", session.get(), &command));
    EXPECT_TRUE(SendKey("a", session.get(), &command));  // "あAaあ"
    // Shift does nothing because the input mode has already been reverted.
    EXPECT_TRUE(SendKey("Shift", session.get(), &command));
    EXPECT_TRUE(SendKey("a", session.get(), &command));  // "あAaああ"
    // "あAaああ"
    EXPECT_EQ("\xE3\x81\x82\x41\x61\xE3\x81\x82\xE3\x81\x82",
              GetComposition(command));
  }

  {  // Revert back to the previous input mode.
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    session->InputModeFullKatakana(&command);
    EXPECT_EQ(commands::FULL_KATAKANA, command.output().mode());
    EXPECT_TRUE(SendKey("a", session.get(), &command));
    EXPECT_TRUE(SendKey("A", session.get(), &command));  // "アA"
    EXPECT_TRUE(SendKey("a", session.get(), &command));  // "アAa"
    // Shift reverts the input mode to Hiragana.
    EXPECT_TRUE(SendKey("Shift", session.get(), &command));
    EXPECT_TRUE(SendKey("a", session.get(), &command));  // "アAaア"
    // Shift does nothing because the input mode has already been reverted.
    EXPECT_TRUE(SendKey("Shift", session.get(), &command));
    EXPECT_TRUE(SendKey("a", session.get(), &command));  // "アAaアア"
    // "アAaアア"
    EXPECT_EQ("\xE3\x82\xA2\x41\x61\xE3\x82\xA2\xE3\x82\xA2",
              GetComposition(command));
  }
}

TEST_F(SessionTest, ExitTemporaryAlphanumModeAfterCommitingSugesstion) {
  // This is a unittest against http://b/2977131.
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    EXPECT_TRUE(SendKey("N", session.get(), &command));
    EXPECT_EQ(commands::HALF_ASCII, command.output().status().mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());  // obsolete

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("NFL");
    segment->add_candidate()->value = "NFL";
    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);

    EXPECT_TRUE(session->Convert(&command));
    EXPECT_FALSE(command.output().has_candidates());
    EXPECT_FALSE(command.output().candidates().has_focused_index());
    EXPECT_EQ(0, command.output().candidates().focused_index());
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());  // obsolete

    EXPECT_TRUE(SendKey("a", session.get(), &command));
    EXPECT_FALSE(command.output().has_candidates());
    EXPECT_TRUE(command.output().has_result());
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());  // obsolete
  }

  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    EXPECT_TRUE(SendKey("N", session.get(), &command));
    EXPECT_EQ(commands::HALF_ASCII, command.output().status().mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());  // obsolete

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("NFL");
    segment->add_candidate()->value = "NFL";
    convertermock_->SetStartPredictionWithComposer(&segments, true);

    EXPECT_TRUE(session->PredictAndConvert(&command));
    ASSERT_TRUE(command.output().has_candidates());
    EXPECT_TRUE(command.output().candidates().has_focused_index());
    EXPECT_EQ(0, command.output().candidates().focused_index());
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());  // obsolete

    EXPECT_TRUE(SendKey("a", session.get(), &command));
    EXPECT_FALSE(command.output().has_candidates());
    EXPECT_TRUE(command.output().has_result());
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());  // obsolete
  }

  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    EXPECT_TRUE(SendKey("N", session.get(), &command));
    EXPECT_EQ(commands::HALF_ASCII, command.output().status().mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());  // obsolete

    EXPECT_TRUE(session->ConvertToHalfASCII(&command));
    EXPECT_FALSE(command.output().has_candidates());
    EXPECT_FALSE(command.output().candidates().has_focused_index());
    EXPECT_EQ(0, command.output().candidates().focused_index());
    EXPECT_FALSE(command.output().has_result());
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());  // obsolete

    EXPECT_TRUE(SendKey("a", session.get(), &command));
    EXPECT_FALSE(command.output().has_candidates());
    EXPECT_TRUE(command.output().has_result());
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());  // obsolete
  }
}

TEST_F(SessionTest, StatusOutput) {
  {  // Basic behavior
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    EXPECT_TRUE(SendKey("a", session.get(), &command));  // "あ"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    // command.output().mode() is going to be obsolete.
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());

    EXPECT_TRUE(SendKey("A", session.get(), &command));  // "あA"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(commands::HALF_ASCII, command.output().status().mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());  // obsolete

    EXPECT_TRUE(SendKey("a", session.get(), &command));  // "あAa"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(commands::HALF_ASCII, command.output().status().mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());  // obsolete

    // Shift reverts the input mode to Hiragana.
    EXPECT_TRUE(SendKey("Shift", session.get(), &command));
    EXPECT_TRUE(SendKey("a", session.get(), &command));  // "あAaあ"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());  // obsolete

    EXPECT_TRUE(SendKey("A", session.get(), &command));  // "あAaあA"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(commands::HALF_ASCII, command.output().status().mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

    // When the IME is deactivated, the temporary composition mode is reset.
    EXPECT_TRUE(SendKey("OFF", session.get(), &command));  // "あAaあA"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    // command.output().mode() always returns DIRECT when IME is
    // deactivated.  This is the reason why command.output().mode() is
    // going to be obsolete.
    EXPECT_EQ(commands::DIRECT, command.output().mode());
  }

  {  // Katakana mode + Shift key
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    session->InputModeFullKatakana(&command);
    EXPECT_EQ(commands::FULL_KATAKANA, command.output().status().mode());
    EXPECT_EQ(commands::FULL_KATAKANA, command.output().mode());  // obsolete

    EXPECT_TRUE(SendKey("a", session.get(), &command));
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(commands::FULL_KATAKANA, command.output().status().mode());
    EXPECT_EQ(commands::FULL_KATAKANA, command.output().mode());  // obsolete

    EXPECT_TRUE(SendKey("A", session.get(), &command));  // "アA"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_TRUE(command.output().status().activated());
    EXPECT_EQ(commands::HALF_ASCII, command.output().status().mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());  // obsolete

    // When the IME is deactivated, the temporary composition mode is reset.
    EXPECT_TRUE(SendKey("OFF", session.get(), &command));  // "アA"
    ASSERT_TRUE(command.output().has_status());
    EXPECT_FALSE(command.output().status().activated());
    EXPECT_EQ(commands::FULL_KATAKANA, command.output().status().mode());
    // command.output().mode() always returns DIRECT when IME is
    // deactivated.  This is the reason why command.output().mode() is
    // going to be obsolete.
    EXPECT_EQ(commands::DIRECT, command.output().mode());
  }
}

TEST_F(SessionTest, Suggest) {
  Segments segments_m;
  {
    segments_m.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_m.add_segment();
    segment->set_key("M");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  Segments segments_mo;
  {
    segments_mo.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  Segments segments_moz;
  {
    segments_moz.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_moz.add_segment();
    segment->set_key("MOZ");
    segment->add_candidate()->value = "MOZUKU";
  }

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  SendKey("M", session.get(), &command);

  command.Clear();
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  SendKey("O", session.get(), &command);
  ASSERT_TRUE(command.output().has_candidates());
  EXPECT_EQ(2, command.output().candidates().candidate_size());
  EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());

  // moz|
  convertermock_->SetStartSuggestionWithComposer(&segments_moz, true);
  command.Clear();
  SendKey("Z", session.get(), &command);
  ASSERT_TRUE(command.output().has_candidates());
  EXPECT_EQ(1, command.output().candidates().candidate_size());
  EXPECT_EQ("MOZUKU", command.output().candidates().candidate(0).value());

  // mo|
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  command.Clear();
  SendKey("Backspace", session.get(), &command);
  ASSERT_TRUE(command.output().has_candidates());
  EXPECT_EQ(2, command.output().candidates().candidate_size());
  EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());

  // m|o
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  command.Clear();
  EXPECT_TRUE(session->MoveCursorLeft(&command));
  ASSERT_TRUE(command.output().has_candidates());
  EXPECT_EQ(2, command.output().candidates().candidate_size());
  EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());

  // mo|
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  command.Clear();
  EXPECT_TRUE(session->MoveCursorToEnd(&command));
  ASSERT_TRUE(command.output().has_candidates());
  EXPECT_EQ(2, command.output().candidates().candidate_size());
  EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());

  // |mo
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  command.Clear();
  EXPECT_TRUE(session->MoveCursorToBeginning(&command));
  ASSERT_TRUE(command.output().has_candidates());
  EXPECT_EQ(2, command.output().candidates().candidate_size());
  EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());

  // m|o
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  command.Clear();
  EXPECT_TRUE(session->MoveCursorRight(&command));
  ASSERT_TRUE(command.output().has_candidates());
  EXPECT_EQ(2, command.output().candidates().candidate_size());
  EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());

  // m|
  convertermock_->SetStartSuggestionWithComposer(&segments_m, true);
  command.Clear();
  EXPECT_TRUE(session->Delete(&command));
  ASSERT_TRUE(command.output().has_candidates());
  EXPECT_EQ(2, command.output().candidates().candidate_size());
  EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());

  Segments segments_m_conv;
  {
    segments_m_conv.set_request_type(Segments::CONVERSION);
    Segment *segment;
    segment = segments_m_conv.add_segment();
    segment->set_key("M");
    segment->add_candidate()->value = "M";
    segment->add_candidate()->value = "m";
  }
  SetComposer(session.get(), &segments_m_conv);
  FillT13Ns(&segments_m_conv);
  convertermock_->SetStartConversionWithComposer(&segments_m_conv, true);
  command.Clear();
  EXPECT_TRUE(session->Convert(&command));

  convertermock_->SetStartSuggestionWithComposer(&segments_m, true);
  command.Clear();
  EXPECT_TRUE(session->ConvertCancel(&command));
  ASSERT_TRUE(command.output().has_candidates());
  EXPECT_EQ(2, command.output().candidates().candidate_size());
  EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());
}

TEST_F(SessionTest, ExpandSuggestion) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  // Prepare suggestion candidates.
  Segments segments_m;
  {
    segments_m.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_m.add_segment();
    segment->set_key("M");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }
  convertermock_->SetStartSuggestionWithComposer(&segments_m, true);

  SendKey("M", session.get(), &command);
  ASSERT_TRUE(command.output().has_candidates());
  EXPECT_EQ(2, command.output().candidates().candidate_size());

  // Prepare expanded suggestion candidates.
  Segments segments_mo;
  {
    segments_mo.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOZUKU";
    segment->add_candidate()->value = "MOZUKUSU";
  }
  convertermock_->SetStartPredictionWithComposer(&segments_mo, true);

  command.Clear();
  EXPECT_TRUE(session->ExpandSuggestion(&command));
  ASSERT_TRUE(command.output().has_candidates());
  // 3 == MOCHA, MOZUKU and MOZUKUSU (duplicate MOZUKU is not counted).
  EXPECT_EQ(3, command.output().candidates().candidate_size());
  EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());
}

TEST_F(SessionTest, ExpandSuggestionDirectMode) {
  // On direct mode, ExpandSuggestion() should do nothing.
  scoped_ptr<Session> session(new Session);
  commands::Command command;

  session->IMEOff(&command);
  EXPECT_TRUE(session->ExpandSuggestion(&command));
  ASSERT_FALSE(command.output().has_candidates());

  // This test expects that ConverterInterface.StartPrediction() is not called
  // so SetStartPredictionWithComposer() is not called.
}

TEST_F(SessionTest, ExpandSuggestionConversionMode) {
  // On conversion mode, ExpandSuggestion() should do nothing.
  scoped_ptr<Session> session(new Session);
  commands::Command command;

  InsertCharacterChars("aiueo", session.get(), &command);
  Segments segments;
  SetComposer(session.get(), &segments);
  SetAiueo(&segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->Convert(&command);
  command.Clear();
  session->ConvertNext(&command);

  EXPECT_TRUE(session->ExpandSuggestion(&command));

  // This test expects that ConverterInterface.StartPrediction() is not called
  // so SetStartPredictionWithComposer() is not called.
}


TEST_F(SessionTest, ToggleAlphanumericMode) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  {
    InsertCharacterChars("a", session.get(), &command);
    // "あ"
    EXPECT_EQ("\xE3\x81\x82", GetComposition(command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());

    command.Clear();
    session->ToggleAlphanumericMode(&command);
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());
    InsertCharacterChars("a", session.get(), &command);
    // "あa"
    EXPECT_EQ("\xE3\x81\x82\x61", GetComposition(command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

    command.Clear();
    session->ToggleAlphanumericMode(&command);
    InsertCharacterChars("a", session.get(), &command);
    // "あaあ"
    EXPECT_EQ("\xE3\x81\x82\x61\xE3\x81\x82", GetComposition(command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  }

  {
    // ToggleAlphanumericMode on Precomposition mode should work.
    command.Clear();
    session->EditCancel(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());

    session->ToggleAlphanumericMode(&command);
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());
    InsertCharacterChars("a", session.get(), &command);
    EXPECT_EQ("a", GetComposition(command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());
  }

  {
    // A single "n" on Hiragana mode should not converted to "ん" for
    // the compatibility with MS-IME.
    command.Clear();
    session->EditCancel(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

    session->ToggleAlphanumericMode(&command);
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());
    InsertCharacterChars("n", session.get(), &command);  // on Hiragana mode
    // "ｎ"
    EXPECT_EQ("\xEF\xBD\x8E", GetComposition(command));

    command.Clear();
    session->ToggleAlphanumericMode(&command);
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());
    InsertCharacterChars("a", session.get(), &command);  // on Half ascii mode.
    // "ｎa"
    EXPECT_EQ("\xEF\xBD\x8E\x61", GetComposition(command));
  }

  {
    // ToggleAlphanumericMode should work even when it is called in
    // the conversion state.
    command.Clear();
    session->EditCancel(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

    session->InputModeHiragana(&command);
    InsertCharacterChars("a", session.get(), &command);  // on Hiragana mode
    // "あ"
    EXPECT_EQ("\xE3\x81\x82", GetComposition(command));

    Segments segments;
    SetAiueo(&segments);
    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);

    command.Clear();
    session->Convert(&command);

    // "あいうえお"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
              GetComposition(command));

    command.Clear();
    session->ToggleAlphanumericMode(&command);
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

    command.Clear();
    session->Commit(&command);

    InsertCharacterChars("a", session.get(), &command);  // on Half ascii mode.
    EXPECT_EQ("a", GetComposition(command));
  }
}

TEST_F(SessionTest, InsertSpace) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  // Default should be FULL_WIDTH.
  EXPECT_TRUE(session->InsertSpace(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_TRUE(command.output().has_result());
  // "　" (full-width space)
  EXPECT_EQ("\xE3\x80\x80", command.output().result().value());

  // Change the setting to HALF_WIDTH.
  config::Config config;
  config.set_space_character_form(config::Config::FUNDAMENTAL_HALF_WIDTH);
  SetConfig(config);
  command.Clear();
  EXPECT_TRUE(session->InsertSpace(&command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());

  // Change the setting to FULL_WIDTH.
  config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);
  SetConfig(config);
  command.Clear();
  EXPECT_TRUE(session->InsertSpace(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_TRUE(command.output().has_result());
  // "　" (full-width space)
  EXPECT_EQ("\xE3\x80\x80", command.output().result().value());
}

TEST_F(SessionTest, InsertSpaceToggled) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  // Default should be FULL_WIDTH.  So the toggled space should be
  // half-width.
  EXPECT_TRUE(session->InsertSpaceToggled(&command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());

  // Change the setting to HALF_WIDTH.
  config::Config config;
  config.set_space_character_form(config::Config::FUNDAMENTAL_HALF_WIDTH);
  SetConfig(config);
  command.Clear();
  EXPECT_TRUE(session->InsertSpaceToggled(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_TRUE(command.output().has_result());
  // "　" (full-width space)
  EXPECT_EQ("\xE3\x80\x80", command.output().result().value());

  // Change the setting to FULL_WIDTH.
  config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);
  SetConfig(config);
  command.Clear();
  EXPECT_TRUE(session->InsertSpaceToggled(&command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, InsertSpaceHalfWidth) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  EXPECT_TRUE(session->InsertSpaceHalfWidth(&command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());

  EXPECT_TRUE(SendKey("a", session.get(), &command));
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", GetComposition(command));

  command.Clear();
  EXPECT_TRUE(session->InsertSpaceHalfWidth(&command));
  // "あ "
  EXPECT_EQ("\xE3\x81\x82\x20", GetComposition(command));

  {  // Convert "あ " with dummy conversions.
    Segments segments;
    // "亜 "
    segments.add_segment()->add_candidate()->value = "\xE4\xBA\x9C\x20";
    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);

    command.Clear();
    EXPECT_TRUE(session->Convert(&command));
  }

  command.Clear();
  EXPECT_TRUE(session->InsertSpaceHalfWidth(&command));
  // "亜  "
  EXPECT_EQ("\xE4\xBA\x9C  ", command.output().result().value());
  EXPECT_EQ("", GetComposition(command));
}

TEST_F(SessionTest, InsertSpaceFullWidth) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  EXPECT_TRUE(session->InsertSpaceFullWidth(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_TRUE(command.output().has_result());
  // "　" (full-width space)
  EXPECT_EQ("\xE3\x80\x80", command.output().result().value());

  EXPECT_TRUE(SendKey("a", session.get(), &command));
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", GetComposition(command));

  command.Clear();
  EXPECT_TRUE(session->InsertSpaceFullWidth(&command));
  // "あ　" (full-width space)
  EXPECT_EQ("\xE3\x81\x82\xE3\x80\x80", GetComposition(command));

  {  // Convert "あ　" with dummy conversions.
    Segments segments;
    // "亜　"
    segments.add_segment()->add_candidate()->value = "\xE4\xBA\x9C\xE3\x80\x80";
    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);

    command.Clear();
    EXPECT_TRUE(session->Convert(&command));
  }

  command.Clear();
  EXPECT_TRUE(session->InsertSpaceFullWidth(&command));
  // "亜　　"
  EXPECT_EQ("\xE4\xBA\x9C\xE3\x80\x80\xE3\x80\x80",
            command.output().result().value());
  EXPECT_EQ("", GetComposition(command));
}

TEST_F(SessionTest, InsertSpaceFullWidthOnHalfKanaInput) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  EXPECT_TRUE(session->InputModeHalfKatakana(&command));
  EXPECT_EQ(commands::HALF_KATAKANA, command.output().mode());
  InsertCharacterChars("a", session.get(), &command);
  // "ｱ"
  EXPECT_EQ("\xEF\xBD\xB1", GetComposition(command));

  command.Clear();
  EXPECT_TRUE(session->InsertSpaceFullWidth(&command));
  // "ｱ　" (full-width space)
  EXPECT_EQ("\xEF\xBD\xB1\xE3\x80\x80", GetComposition(command));
}

TEST_F(SessionTest, IsFullWidthInsertSpace) {
  scoped_ptr<Session> session(new Session);
  config::Config config;

  // Default config -- follow to the current mode.
  InitSessionToPrecomposition(session.get());
  config.set_space_character_form(config::Config::FUNDAMENTAL_INPUT_MODE);
  SetConfig(config);
  // Hiragana
  commands::Command command;
  session->InputModeHiragana(&command);
  EXPECT_TRUE(session->IsFullWidthInsertSpace());
  // Full-Katakana
  command.Clear();
  session->InputModeFullKatakana(&command);
  EXPECT_TRUE(session->IsFullWidthInsertSpace());
  // Half-Katakana
  command.Clear();
  session->InputModeHalfKatakana(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());
  // Full-ASCII
  command.Clear();
  session->InputModeFullASCII(&command);
  EXPECT_TRUE(session->IsFullWidthInsertSpace());
  // Half-ASCII
  command.Clear();
  session->InputModeHalfASCII(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());
  // Direct
  command.Clear();
  session->IMEOff(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());

  // Set config to 'half' -- all mode has to emit half-width space.
  session.reset(new Session);
  InitSessionToPrecomposition(session.get());
  config.set_space_character_form(config::Config::FUNDAMENTAL_HALF_WIDTH);
  SetConfig(config);
  InitSessionToPrecomposition(session.get());
  // Hiragana
  command.Clear();
  session->InputModeHiragana(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());
  // Full-Katakana
  command.Clear();
  session->InputModeFullKatakana(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());
  // Half-Katakana
  command.Clear();
  session->InputModeHalfKatakana(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());
  // Full-ASCII
  command.Clear();
  session->InputModeFullASCII(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());
  // Half-ASCII
  command.Clear();
  session->InputModeHalfASCII(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());
  // Direct
  command.Clear();
  session->IMEOff(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());

  // Set config to 'FULL' -- all mode except for DIRECT emits
  // full-width space.
  session.reset(new Session);
  InitSessionToPrecomposition(session.get());
  config.set_space_character_form(config::Config::FUNDAMENTAL_FULL_WIDTH);
  SetConfig(config);
  InitSessionToPrecomposition(session.get());
  // Hiragana
  command.Clear();
  session->InputModeHiragana(&command);
  EXPECT_TRUE(session->IsFullWidthInsertSpace());
  // Full-Katakana
  command.Clear();
  session->InputModeFullKatakana(&command);
  EXPECT_TRUE(session->IsFullWidthInsertSpace());
  // Half-Katakana
  command.Clear();
  session->InputModeHalfKatakana(&command);
  EXPECT_TRUE(session->IsFullWidthInsertSpace());
  // Full-ASCII
  command.Clear();
  session->InputModeFullASCII(&command);
  EXPECT_TRUE(session->IsFullWidthInsertSpace());
  // Half-ASCII
  command.Clear();
  session->InputModeHalfASCII(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());
  // Direct
  command.Clear();
  session->IMEOff(&command);
  EXPECT_FALSE(session->IsFullWidthInsertSpace());
}

TEST_F(SessionTest, Issue1951385) {
  // This is a unittest against http://b/1951385
  Segments segments;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  const string exceeded_preedit(500, 'a');
  ASSERT_EQ(500, exceeded_preedit.size());
  InsertCharacterChars(exceeded_preedit, session.get(), &command);

  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, false);

  command.Clear();
  session->ConvertToFullASCII(&command);
  EXPECT_FALSE(command.output().has_candidates());

  // The status should remain the preedit status, although the
  // previous command was convert.  The next command makes sure that
  // the preedit will disappear by canceling the preedit status.
  command.Clear();
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::ESCAPE);
  EXPECT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, Issue1978201) {
  // This is a unittest against http://b/1978201
  Segments segments;
  Segment *segment;
  segment = segments.add_segment();
  // "いんぼう"
  segment->set_key("\xe3\x81\x84\xe3\x82\x93\xe3\x81\xbc\xe3\x81\x86");
  // "陰謀"
  segment->add_candidate()->value = "\xe9\x99\xb0\xe8\xac\x80";
  // "陰謀論"
  segment->add_candidate()->value = "\xe9\x99\xb0\xe8\xac\x80\xe8\xab\x96";
  // "陰謀説"
  segment->add_candidate()->value = "\xe9\x99\xb0\xe8\xac\x80\xe8\xaa\xac";
  convertermock_->SetStartPredictionWithComposer(&segments, true);

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  EXPECT_TRUE(session->SegmentWidthShrink(&command));

  command.Clear();
  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);
  EXPECT_TRUE(session->Convert(&command));

  command.Clear();
  EXPECT_TRUE(session->CommitSegment(&command));
  EXPECT_TRUE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, Issue1975771) {
  // This is a unittest against http://b/1975771
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // Trigger suggest by pressing "a".
  Segments segments;
  SetAiueo(&segments);
  convertermock_->SetStartSuggestionWithComposer(&segments, true);

  commands::Command command;
  commands::KeyEvent* key_event = command.mutable_input()->mutable_key();
  key_event->set_key_code('a');
  key_event->set_modifiers(0);  // No modifiers.
  EXPECT_TRUE(session->InsertCharacter(&command));

  // Click the first candidate.
  command.Clear();
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  commands::SessionCommand *session_command =
      command.mutable_input()->mutable_command();
  session_command->set_type(commands::SessionCommand::SELECT_CANDIDATE);
  session_command->set_id(0);
  EXPECT_TRUE(session->SendCommand(&command));

  // After select candidate session->status_ should be
  // SessionStatus::CONVERSION.

  command.Clear();
  command.mutable_input()->mutable_command()->set_id(0);
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::SPACE);
  EXPECT_TRUE(session->SendKey(&command));
  EXPECT_TRUE(command.output().has_candidates());
  // The second candidate should be selected.
  EXPECT_EQ(1, command.output().candidates().focused_index());
}

TEST_F(SessionTest, Issue2029466) {
  // This is a unittest against http://b/2029466
  //
  // "a<tab><ctrl-N>a" raised an exception because CommitFirstSegment
  // did not check if the current status is in conversion or
  // precomposition.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // "a"
  commands::Command command;
  InsertCharacterChars("a", session.get(), &command);

  // <tab>
  Segments segments;
  SetAiueo(&segments);
  convertermock_->SetStartPredictionWithComposer(&segments, true);
  command.Clear();
  EXPECT_TRUE(session->PredictAndConvert(&command));

  // <ctrl-N>
  segments.Clear();
  // FinishConversion is expected to return empty Segments.
  convertermock_->SetFinishConversion(&segments, true);
  command.Clear();
  EXPECT_TRUE(session->CommitSegment(&command));

  // "a"
  command.Clear();
  InsertCharacterChars("a", session.get(), &command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_candidates());
}

TEST_F(SessionTest, Issue2034943) {
  // This is a unittest against http://b/2029466
  //
  // The composition should have been reset if CommitSegment submitted
  // the all segments (e.g. the size of segments is one).
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("mozu", session.get(), &command);

  {  // Initialize a suggest result triggered by "mozu".
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("mozu");
    Segment::Candidate *candidate;
    candidate = segment->add_candidate();
    candidate->value = "MOZU";
    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);
  }
  // Get conversion
  command.Clear();
  EXPECT_TRUE(session->Convert(&command));

  // submit segment
  command.Clear();
  EXPECT_TRUE(session->CommitSegment(&command));

  // The composition should have been reset.
  InsertCharacterChars("ku", session.get(), &command);
  // "く"
  EXPECT_EQ("\343\201\217", command.output().preedit().segment(0).value());
}

TEST_F(SessionTest, Issue2026354) {
  // This is a unittest against http://b/2026354
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("aiueo", session.get(), &command);

  // Trigger suggest by pressing "a".
  Segments segments;
  SetAiueo(&segments);
  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  EXPECT_TRUE(session->Convert(&command));

  command.Clear();
  //  EXPECT_TRUE(session->ConvertNext(&command));
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::SPACE);
  EXPECT_TRUE(session->TestSendKey(&command));
  EXPECT_TRUE(command.output().has_preedit());
  command.mutable_output()->clear_candidates();
  EXPECT_FALSE(command.output().has_candidates());
}

TEST_F(SessionTest, Issue2066906) {
  // This is a unittest against http://b/2066906
  Segments segments;
  Segment *segment;
  Segment::Candidate *candidate;
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  segment = segments.add_segment();
  segment->set_key("a");
  candidate = segment->add_candidate();
  candidate->value = "abc";
  candidate = segment->add_candidate();
  candidate->value = "abcdef";
  convertermock_->SetStartPredictionWithComposer(&segments, true);

  // Prediction with "a"
  commands::Command command;
  EXPECT_TRUE(session->PredictAndConvert(&command));
  EXPECT_FALSE(command.output().has_result());

  // Commit
  command.Clear();
  EXPECT_TRUE(session->Commit(&command));
  EXPECT_TRUE(command.output().has_result());

  convertermock_->SetStartSuggestionWithComposer(&segments, true);
  command.Clear();
  InsertCharacterChars("a", session.get(), &command);
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, Issue2187132) {
  // This is a unittest against http://b/2187132
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  // Shift + Ascii triggers temporary input mode switch.
  SendKey("A", session.get(), &command);
  SendKey("Enter", session.get(), &command);

  // After submission, input mode should be reverted.
  SendKey("a", session.get(), &command);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", GetComposition(command));

  command.Clear();
  session->EditCancel(&command);
  EXPECT_TRUE(GetComposition(command).empty());

  // If a user intentionally switched an input mode, it should remain.
  EXPECT_TRUE(session->InputModeHalfASCII(&command));
  SendKey("A", session.get(), &command);
  SendKey("Enter", session.get(), &command);
  SendKey("a", session.get(), &command);
  EXPECT_EQ("a", GetComposition(command));
}

TEST_F(SessionTest, Issue2190364) {
  // This is a unittest against http://b/2190364
  config::Config config;
  config.set_preedit_method(config::Config::KANA);
  SetConfig(config);
  ASSERT_EQ(config::Config::KANA, GET_CONFIG(preedit_method));

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  session->ToggleAlphanumericMode(&command);

  // "ち"
  InsertCharacterCodeAndString('a', "\xE3\x81\xA1", session.get(), &command);
  EXPECT_EQ("a", GetComposition(command));

  command.Clear();
  session->ToggleAlphanumericMode(&command);
  EXPECT_EQ("a", GetComposition(command));

  // "に"
  InsertCharacterCodeAndString('i', "\xE3\x81\xAB", session.get(), &command);
  // "aに"
  EXPECT_EQ("a\xE3\x81\xAB", GetComposition(command));
}

TEST_F(SessionTest, Issue1556649) {
  // This is a unittest against http://b/1556649
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("kudoudesu", session.get(), &command);
  // "くどうです"
  EXPECT_EQ("\xE3\x81\x8F\xE3\x81\xA9\xE3\x81\x86\xE3\x81\xA7\xE3\x81\x99",
            GetComposition(command));
  EXPECT_EQ(5, command.output().preedit().cursor());

  command.Clear();
  EXPECT_TRUE(session->DisplayAsHalfKatakana(&command));
  // "ｸﾄﾞｳﾃﾞｽ"
  EXPECT_EQ("\xEF\xBD\xB8\xEF\xBE\x84\xEF\xBE\x9E\xEF\xBD\xB3\xEF\xBE\x83"
            "\xEF\xBE\x9E\xEF\xBD\xBD",
            GetComposition(command));
  EXPECT_EQ(7, command.output().preedit().cursor());

  for (size_t i = 0; i < 7; ++i) {
    const size_t expected_pos = 6 - i;
    EXPECT_TRUE(SendKey("Left", session.get(), &command));
    EXPECT_EQ(expected_pos, command.output().preedit().cursor());
  }
}

TEST_F(SessionTest, Issue1518994) {
  // This is a unittest against http://b/1518994.
  // - Can't input space in ascii mode.
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    EXPECT_TRUE(SendKey("a", session.get(), &command));
    command.Clear();
    EXPECT_TRUE(session->ToggleAlphanumericMode(&command));
    EXPECT_TRUE(SendKey("i", session.get(), &command));
    // "あi"
    EXPECT_EQ("\xE3\x81\x82\x69", GetComposition(command));

    EXPECT_TRUE(SendKey("Space", session.get(), &command));
    // "あi "
    EXPECT_EQ("\xE3\x81\x82\x69\x20", GetComposition(command));
  }

  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    EXPECT_TRUE(SendKey("a", session.get(), &command));
    EXPECT_TRUE(SendKey("I", session.get(), &command));
    // "あI"
    EXPECT_EQ("\xE3\x81\x82\x49", GetComposition(command));

    EXPECT_TRUE(SendKey("Space", session.get(), &command));
    // "あI "
    EXPECT_EQ("\xE3\x81\x82\x49\x20", GetComposition(command));
  }
}

TEST_F(SessionTest, Issue1571043) {
  // This is a unittest against http://b/1571043.
  // - Underline of composition is separated.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("aiu", session.get(), &command);
  // "あいう"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86", GetComposition(command));

  for (size_t i = 0; i < 3; ++i) {
    const size_t expected_pos = 2 - i;
    EXPECT_TRUE(SendKey("Left", session.get(), &command));
    EXPECT_EQ(expected_pos, command.output().preedit().cursor());
    EXPECT_EQ(1, command.output().preedit().segment_size());
  }
}

TEST_F(SessionTest, Issue1799384) {
  // This is a unittest against http://b/1571043.
  // - ConvertToHiragana converts Vu to U+3094 "ヴ"
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("ravu", session.get(), &command);
  // TODO(komatsu) "ヴ" might be preferred on Mac.
  // "らヴ"
  EXPECT_EQ("\xE3\x82\x89\xE3\x83\xB4", GetComposition(command));

  {  // Initialize convertermock_ to generate t13n candidates.
    Segments segments;
    Segment *segment;
    segments.set_request_type(Segments::CONVERSION);
    segment = segments.add_segment();
    // "らヴ"
    segment->set_key("\xE3\x82\x89\xE3\x83\xB4");
    Segment::Candidate *candidate;
    candidate = segment->add_candidate();
    // "らぶ"
    candidate->value = "\xE3\x82\x89\xE3\x81\xB6";
    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);
  }

  command.Clear();
  EXPECT_TRUE(session->ConvertToHiragana(&command));

  // "らヴ"
  EXPECT_EQ("\xE3\x82\x89\xE3\x83\xB4", GetComposition(command));
}

TEST_F(SessionTest, Issue2217250) {
  // This is a unittest against http://b/2217250.
  // Temporary direct input mode through a special sequence such as
  // www. continues even after committing them
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  InsertCharacterChars("www.", session.get(), &command);
  EXPECT_EQ("www.", GetComposition(command));
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

  SendKey("Enter", session.get(), &command);
  EXPECT_EQ("www.", command.output().result().value());
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());
}

TEST_F(SessionTest, Issue2223823) {
  // This is a unittest against http://b/2223823
  // Input mode does not recover like MS-IME by single shift key down
  // and up.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  SendKey("G", session.get(), &command);
  EXPECT_EQ("G", GetComposition(command));
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

  SendKey("Shift", session.get(), &command);
  EXPECT_EQ("G", GetComposition(command));
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());
}


TEST_F(SessionTest, Issue2223762) {
  // This is a unittest against http://b/2223762.
  // - The first space in half-width alphanumeric mode is full-width.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  EXPECT_TRUE(session->InputModeHalfASCII(&command));
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

  EXPECT_TRUE(SendKey("Space", session.get(), &command));
  EXPECT_FALSE(command.output().consumed());
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, Issue2223755) {
  // This is a unittest against http://b/2223755.
  // - F6 and F7 convert space to half-width.

  {  // DisplayAsFullKatakana
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    EXPECT_TRUE(SendKey("a", session.get(), &command));
    EXPECT_TRUE(SendKey("Eisu", session.get(), &command));
    EXPECT_TRUE(SendKey("Space", session.get(), &command));
    EXPECT_TRUE(SendKey("Eisu", session.get(), &command));
    EXPECT_TRUE(SendKey("i", session.get(), &command));

    // "あ い"
    EXPECT_EQ("\xE3\x81\x82\x20\xE3\x81\x84", GetComposition(command));

    command.Clear();
    EXPECT_TRUE(session->DisplayAsFullKatakana(&command));

    // "ア　イ"(fullwidth space)
    EXPECT_EQ("\xE3\x82\xA2\xE3\x80\x80\xE3\x82\xA4", GetComposition(command));
  }

  {  // ConvertToFullKatakana
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    EXPECT_TRUE(SendKey("a", session.get(), &command));
    EXPECT_TRUE(SendKey("Eisu", session.get(), &command));
    EXPECT_TRUE(SendKey("Space", session.get(), &command));
    EXPECT_TRUE(SendKey("Eisu", session.get(), &command));
    EXPECT_TRUE(SendKey("i", session.get(), &command));

    // "あ い"
    EXPECT_EQ("\xE3\x81\x82\x20\xE3\x81\x84", GetComposition(command));

    {  // Initialize convertermock_ to generate t13n candidates.
      Segments segments;
      Segment *segment;
      segments.set_request_type(Segments::CONVERSION);
      segment = segments.add_segment();
      // "あ い"
      segment->set_key("\xE3\x81\x82\x20\xE3\x81\x84");
      Segment::Candidate *candidate;
      candidate = segment->add_candidate();
      // "あ い"
      candidate->value = "\xE3\x81\x82\x20\xE3\x81\x84";
      SetComposer(session.get(), &segments);
      FillT13Ns(&segments);
      convertermock_->SetStartConversionWithComposer(&segments, true);
    }

    command.Clear();
    EXPECT_TRUE(session->ConvertToFullKatakana(&command));

    // "ア　イ"(fullwidth space)
    EXPECT_EQ("\xE3\x82\xA2\xE3\x80\x80\xE3\x82\xA4", GetComposition(command));
  }
}

TEST_F(SessionTest, Issue2269058) {
  // This is a unittest against http://b/2269058.
  // - Temporary input mode should not be overridden by a permanent
  //   input mode change.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  EXPECT_TRUE(SendKey("G", session.get(), &command));
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

  command.Clear();
  EXPECT_TRUE(session->InputModeHalfASCII(&command));
  EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

  EXPECT_TRUE(SendKey("Shift", session.get(), &command));
  EXPECT_EQ(commands::HIRAGANA, command.output().mode());
}

TEST_F(SessionTest, Issue2272745) {
  // This is a unittest against http://b/2272745.
  // A temporary input mode remains when a composition is canceled.
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    EXPECT_TRUE(SendKey("G", session.get(), &command));
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

    EXPECT_TRUE(SendKey("Backspace", session.get(), &command));
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  }

  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    EXPECT_TRUE(SendKey("G", session.get(), &command));
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

    EXPECT_TRUE(SendKey("Escape", session.get(), &command));
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  }
}

TEST_F(SessionTest, Issue2297060) {
  // This is a unittest against http://b/2297060.
  // Ctrl-Space is not working
  config::Config config;
  config.set_session_keymap(config::Config::MSIME);
  SetConfig(config);

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  ASSERT_EQ(config::Config::MSIME, GET_CONFIG(session_keymap));

  commands::Command command;
  EXPECT_TRUE(SendKey("Ctrl Space", session.get(), &command));
  EXPECT_FALSE(command.output().consumed());
}

TEST_F(SessionTest, Issue2379374) {
  // This is a unittest against http://b/2379374.
  // Numpad ignores Direct input style when typing after conversion.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  {  // Set numpad_character_form with NUMPAD_DIRECT_INPUT
    config::Config config;
    config.set_numpad_character_form(config::Config::NUMPAD_DIRECT_INPUT);
    SetConfig(config);
    ASSERT_EQ(config::Config::NUMPAD_DIRECT_INPUT,
              GET_CONFIG(numpad_character_form));
  }

  Segments segments;
  {  // Set mock conversion.
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    // "あ"
    segment->set_key("\xe3\x81\x82");
    candidate = segment->add_candidate();
    // "亜"
    candidate->value = "\xE4\xBA\x9C";
    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);
  }

  EXPECT_TRUE(SendKey("a", session.get(), &command));
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", GetComposition(command));

  EXPECT_TRUE(SendKey("Space", session.get(), &command));
  // "亜"
  EXPECT_EQ("\xE4\xBA\x9C", GetComposition(command));

  EXPECT_TRUE(SendKey("Numpad0", session.get(), &command));
  EXPECT_TRUE(GetComposition(command).empty());
  ASSERT_TRUE(command.output().has_result());
  // "亜0"
  EXPECT_EQ("\xE4\xBA\x9C\x30", command.output().result().value());
  // "あ0"
  EXPECT_EQ("\xE3\x81\x82\x30", command.output().result().key());

  // The previous Numpad0 must not affect the current composition.
  EXPECT_TRUE(SendKey("a", session.get(), &command));
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", GetComposition(command));
}

TEST_F(SessionTest, Issue2569789) {
  // This is a unittest against http://b/2379374.
  // After typing "google", the input mode does not come back to the
  // previous input mode.
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    InsertCharacterChars("google", session.get(), &command);
    EXPECT_EQ("google", GetComposition(command));
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());

    EXPECT_TRUE(SendKey("enter", session.get(), &command));
    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ("google", command.output().result().value());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  }

  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    InsertCharacterChars("Google", session.get(), &command);
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

    EXPECT_TRUE(SendKey("enter", session.get(), &command));
    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ("Google", command.output().result().value());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  }

  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    InsertCharacterChars("Google", session.get(), &command);
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

    EXPECT_TRUE(SendKey("shift", session.get(), &command));
    EXPECT_EQ("Google", GetComposition(command));
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());

    InsertCharacterChars("aaa", session.get(), &command);
    // "Googleあああ"
    EXPECT_EQ("Google\xE3\x81\x82\xE3\x81\x82\xE3\x81\x82",
              GetComposition(command));
  }

  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    InsertCharacterChars("http", session.get(), &command);
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());

    EXPECT_TRUE(SendKey("enter", session.get(), &command));
    ASSERT_TRUE(command.output().has_result());
    EXPECT_EQ("http", command.output().result().value());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());
  }
}

TEST_F(SessionTest, Issue2555503) {
  // This is a unittest against http://b/2555503.
  // Mode respects the previous character too much.

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  SendKey("a", session.get(), &command);

  command.Clear();
  session->InputModeFullKatakana(&command);

  SendKey("i", session.get(), &command);
  // "あイ"
  EXPECT_EQ("\xE3\x81\x82\xE3\x82\xA4", GetComposition(command));

  SendKey("backspace", session.get(), &command);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", GetComposition(command));
  EXPECT_EQ(commands::FULL_KATAKANA, command.output().mode());
}

TEST_F(SessionTest, Issue2791640) {
  // This is a unittest against http://b/2791640.
  // Existing preedit should be committed when IME is turned off.

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  SendKey("a", session.get(), &command);

  command.Clear();

  SendKey("hankaku/zenkaku", session.get(), &command);

  ASSERT_TRUE(command.output().consumed());

  ASSERT_TRUE(command.output().has_result());
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", command.output().result().value());
  EXPECT_EQ(commands::DIRECT, command.output().mode());

  ASSERT_FALSE(command.output().has_preedit());
}

TEST_F(SessionTest, CommitExistingPreeditWhenIMEIsTurnedOff) {
  // Existing preedit should be committed when IME is turned off.

  // Check "hankaku/zenkaku"
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());

    commands::Command command;
    SendKey("a", session.get(), &command);

    command.Clear();

    SendKey("hankaku/zenkaku", session.get(), &command);

    ASSERT_TRUE(command.output().consumed());

    ASSERT_TRUE(command.output().has_result());
    // "あ"
    EXPECT_EQ("\xE3\x81\x82", command.output().result().value());
    EXPECT_EQ(commands::DIRECT, command.output().mode());

    ASSERT_FALSE(command.output().has_preedit());
  }

  // Check "kanji"
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());

    commands::Command command;
    SendKey("a", session.get(), &command);

    command.Clear();

    SendKey("kanji", session.get(), &command);

    ASSERT_TRUE(command.output().consumed());

    ASSERT_TRUE(command.output().has_result());
    // "あ"
    EXPECT_EQ("\xE3\x81\x82", command.output().result().value());
    EXPECT_EQ(commands::DIRECT, command.output().mode());

    ASSERT_FALSE(command.output().has_preedit());
  }
}


TEST_F(SessionTest, SendKeyDirectInputStateTest) {
  // InputModeChange commands from direct mode are supported only for Windows
  // for now.
#ifdef OS_WINDOWS
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  session->IMEOff(&command);

  config::Config config;
  const string custom_keymap_table =
      "status\tkey\tcommand\n"
      "DirectInput\tHiragana\tInputModeHiragana\n";
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(custom_keymap_table);
  SetConfig(config);

  command.Clear();
  EXPECT_TRUE(SendKey("Hiragana", session.get(), &command));
  SendKey("a", session.get(), &command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());
#endif  // OS_WINDOWS
}

TEST_F(SessionTest, HandlingDirectInputTableAttribute) {
  composer::Table table;
  // "か"
  table.AddRuleWithAttributes("ka", "\xE3\x81\x8B", "",
                              composer::DIRECT_INPUT);
  // "っ"
  table.AddRuleWithAttributes("tt", "\xE3\x81\xA3", "t",
                              composer::DIRECT_INPUT);
  // "た"
  table.AddRuleWithAttributes("ta", "\xE3\x81\x9F", "",
                              composer::NO_TABLE_ATTRIBUTE);

  Session session;
  InitSessionToPrecomposition(&session);
  session.get_internal_composer_only_for_unittest()->SetTable(&table);

  commands::Command command;
  SendKey("k", &session, &command);
  EXPECT_FALSE(command.output().has_result());

  command.Clear();
  SendKey("a", &session, &command);
  EXPECT_TRUE(command.output().has_result());
  // "か"
  EXPECT_EQ("\xE3\x81\x8B", command.output().result().value());

  command.Clear();
  SendKey("t", &session, &command);
  EXPECT_FALSE(command.output().has_result());

  command.Clear();
  SendKey("t", &session, &command);
  EXPECT_FALSE(command.output().has_result());

  command.Clear();
  SendKey("a", &session, &command);
  EXPECT_TRUE(command.output().has_result());
  // "った"
  EXPECT_EQ("\xE3\x81\xA3\xE3\x81\x9F", command.output().result().value());
}

TEST_F(SessionTest, IMEOnWithModeTest) {
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    session->IMEOff(&command);
    command.Clear();
    command.mutable_input()->mutable_key()->set_mode(
        commands::HIRAGANA);
    EXPECT_TRUE(session->IMEOn(&command));
    EXPECT_TRUE(command.output().has_consumed());
    EXPECT_TRUE(command.output().consumed());
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::HIRAGANA,
              command.output().mode());
    SendKey("a", session.get(), &command);
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    // "あ"
    EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());
  }
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    session->IMEOff(&command);
    command.Clear();
    command.mutable_input()->mutable_key()->set_mode(
        commands::FULL_KATAKANA);
    EXPECT_TRUE(session->IMEOn(&command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::FULL_KATAKANA,
              command.output().mode());
    SendKey("a", session.get(), &command);
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    // "ア"
    EXPECT_EQ("\xE3\x82\xA2", command.output().preedit().segment(0).key());
  }
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    session->IMEOff(&command);
    command.Clear();
    command.mutable_input()->mutable_key()->set_mode(
        commands::HALF_KATAKANA);
    EXPECT_TRUE(session->IMEOn(&command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::HALF_KATAKANA,
              command.output().mode());
    SendKey("a", session.get(), &command);
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    // "ｱ" (half-width Katakana)
    EXPECT_EQ("\xEF\xBD\xB1", command.output().preedit().segment(0).key());
  }
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    session->IMEOff(&command);
    command.Clear();
    command.mutable_input()->mutable_key()->set_mode(
        commands::FULL_ASCII);
    EXPECT_TRUE(session->IMEOn(&command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::FULL_ASCII,
              command.output().mode());
    SendKey("a", session.get(), &command);
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    // "ａ"
    EXPECT_EQ("\xEF\xBD\x81", command.output().preedit().segment(0).key());
  }
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;
    session->IMEOff(&command);
    command.Clear();
    command.mutable_input()->mutable_key()->set_mode(
        commands::HALF_ASCII);
    EXPECT_TRUE(session->IMEOn(&command));
    EXPECT_TRUE(command.output().has_mode());
    EXPECT_EQ(commands::HALF_ASCII,
              command.output().mode());
    SendKey("a", session.get(), &command);
    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    // "a"
    EXPECT_EQ("a", command.output().preedit().segment(0).key());
  }
}

TEST_F(SessionTest, InputModeConsumed) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  EXPECT_TRUE(session->InputModeHiragana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HIRAGANA, command.output().mode());
  command.Clear();
  EXPECT_TRUE(session->InputModeFullKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::FULL_KATAKANA, command.output().mode());
  command.Clear();
  EXPECT_TRUE(session->InputModeHalfKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HALF_KATAKANA, command.output().mode());
  command.Clear();
  EXPECT_TRUE(session->InputModeFullASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::FULL_ASCII, command.output().mode());
  command.Clear();
  EXPECT_TRUE(session->InputModeHalfASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HALF_ASCII, command.output().mode());
}

TEST_F(SessionTest, InputModeConsumedForTestSendKey) {
  // This test is only for Windows, because InputModeHiragana bound
  // with Hiragana key is only supported on Windows yet.
#ifdef OS_WINDOWS
  config::Config config;
  config.set_session_keymap(config::Config::MSIME);
  SetConfig(config);

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  ASSERT_EQ(config::Config::MSIME, GET_CONFIG(session_keymap));
  // In MSIME keymap, Hiragana is assigned for
  // ImputModeHiragana in Precomposition.

  commands::Command command;
  EXPECT_TRUE(TestSendKey("Hiragana", session.get(), &command));
  EXPECT_TRUE(command.output().consumed());
#endif  // OS_WINDOWS
}

TEST_F(SessionTest, InputModeOutputHasComposition) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  SendKey("a", session.get(), &command);
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());

  command.Clear();
  EXPECT_TRUE(session->InputModeHiragana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HIRAGANA, command.output().mode());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());

  command.Clear();
  EXPECT_TRUE(session->InputModeFullKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::FULL_KATAKANA, command.output().mode());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());

  command.Clear();
  EXPECT_TRUE(session->InputModeHalfKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HALF_KATAKANA, command.output().mode());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());

  command.Clear();
  EXPECT_TRUE(session->InputModeFullASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::FULL_ASCII, command.output().mode());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());

  command.Clear();
  EXPECT_TRUE(session->InputModeHalfASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HALF_ASCII, command.output().mode());
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(1, command.output().preedit().segment_size());
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", command.output().preedit().segment(0).key());
}

TEST_F(SessionTest, InputModeOutputHasCandidates) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  Segments segments;
  SetAiueo(&segments);
  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  commands::Command command;
  InsertCharacterChars("aiueo", session.get(), &command);

  command.Clear();
  session->Convert(&command);
  session->ConvertNext(&command);
  EXPECT_TRUE(command.output().has_candidates());
  EXPECT_TRUE(command.output().has_preedit());

  command.Clear();
  EXPECT_TRUE(session->InputModeHiragana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HIRAGANA, command.output().mode());
  EXPECT_TRUE(command.output().has_candidates());
  EXPECT_TRUE(command.output().has_preedit());

  command.Clear();
  EXPECT_TRUE(session->InputModeFullKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::FULL_KATAKANA, command.output().mode());
  EXPECT_TRUE(command.output().has_candidates());
  EXPECT_TRUE(command.output().has_preedit());

  command.Clear();
  EXPECT_TRUE(session->InputModeHalfKatakana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HALF_KATAKANA, command.output().mode());
  EXPECT_TRUE(command.output().has_candidates());
  EXPECT_TRUE(command.output().has_preedit());

  command.Clear();
  EXPECT_TRUE(session->InputModeFullASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::FULL_ASCII, command.output().mode());
  EXPECT_TRUE(command.output().has_candidates());
  EXPECT_TRUE(command.output().has_preedit());

  command.Clear();
  EXPECT_TRUE(session->InputModeHalfASCII(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HALF_ASCII, command.output().mode());
  EXPECT_TRUE(command.output().has_candidates());
  EXPECT_TRUE(command.output().has_preedit());
}

TEST_F(SessionTest, PerformedCommand) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  {
    commands::Command command;
    // IMEOff
    command.mutable_input()->mutable_key()->set_special_key(
        commands::KeyEvent::OFF);
    session->SendKey(&command);
    EXPECT_EQ("Precomposition_IMEOff", command.output().performed_command());
  }
  {
    commands::Command command;
    // IMEOn
    command.mutable_input()->mutable_key()->set_special_key(
        commands::KeyEvent::ON);
    session->SendKey(&command);
    EXPECT_EQ("Direct_IMEOn", command.output().performed_command());
  }
  {
    commands::Command command;
    // 'a'
    command.mutable_input()->mutable_key()->set_key_code('a');
    session->SendKey(&command);
    EXPECT_EQ("Precomposition_InsertCharacter",
              command.output().performed_command());
  }
  {
    // SetStartConversion for changing state to Convert.
    Segments segments;
    SetAiueo(&segments);
    SetComposer(session.get(), &segments);
    FillT13Ns(&segments);
    convertermock_->SetStartConversionWithComposer(&segments, true);
    commands::Command command;
    // SPACE
    command.mutable_input()->mutable_key()->set_special_key(
        commands::KeyEvent::SPACE);
    session->SendKey(&command);
    EXPECT_EQ("Composition_Convert",
              command.output().performed_command());
  }
  {
    commands::Command command;
    // ENTER
    command.mutable_input()->mutable_key()->set_special_key(
        commands::KeyEvent::ENTER);
    session->SendKey(&command);
    EXPECT_EQ("Conversion_Commit",
              command.output().performed_command());
  }
}

// Independent test
TEST(SessionResetTest, ResetContext) {
  scoped_ptr<ConverterMockForReset> convertermock(new ConverterMockForReset);
  ConverterFactory::SetConverter(convertermock.get());
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  session->ResetContext(&command);
  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(convertermock->reset_conversion_called());

  convertermock->Reset();
  command.Clear();
  EXPECT_TRUE(SendKey("A", session.get(), &command));
  command.Clear();
  session->ResetContext(&command);
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(convertermock->reset_conversion_called());
}

TEST_F(SessionTest, ClearUndoOnResetContext) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session->set_client_capability(capability);

  commands::Command command;
  Segments segments;

  {  // Create segments
    InsertCharacterChars("aiueo", session.get(), &command);
    SetComposer(session.get(), &segments);
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
    convertermock_->SetStartConversionWithComposer(&segments, true);
    command.Clear();
    session->Convert(&command);
    EXPECT_FALSE(command.output().has_result());
    EXPECT_TRUE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              GetComposition(command));

    convertermock_->SetCommitSegmentValue(&segments, true);
    command.Clear();
    session->Commit(&command);
    EXPECT_TRUE(command.output().has_result());
    EXPECT_FALSE(command.output().has_preedit());
    // "あいうえお"
    EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
              command.output().result().value());

    command.Clear();
    session->ResetContext(&command);

    command.Clear();
    session->Undo(&command);
    // After reset, undo shouldn't run.
    EXPECT_FALSE(command.output().has_preedit());
  }
}

TEST(SessionResetTest, IssueResetConversion) {
  scoped_ptr<ConverterMockForReset> convertermock(new ConverterMockForReset);
  ConverterFactory::SetConverter(convertermock.get());
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  // any meaneangless key calls ResetConversion
  EXPECT_FALSE(convertermock->reset_conversion_called());
  EXPECT_TRUE(SendKey("enter", session.get(), &command));
  EXPECT_TRUE(convertermock->reset_conversion_called());

  convertermock->Reset();
  EXPECT_FALSE(convertermock->reset_conversion_called());
  EXPECT_TRUE(SendKey("space", session.get(), &command));
  EXPECT_TRUE(convertermock->reset_conversion_called());
}

TEST(SessionRevertTest, IssueRevert) {
  scoped_ptr<ConverterMockForRevert> convertermock(new ConverterMockForRevert);
  ConverterFactory::SetConverter(convertermock.get());
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  // changes the state to PRECOMPOSITION
  session->IMEOn(&command);

  session->Revert(&command);

  EXPECT_FALSE(command.output().consumed());
  EXPECT_TRUE(convertermock->revert_conversion_called());
}

// Undo command must call RervertConversion
TEST_F(SessionTest, Issue3428520) {
  scoped_ptr<ConverterMockForRevert> convertermock(new ConverterMockForRevert);
  ConverterFactory::SetConverter(convertermock.get());
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // Undo requires capability DELETE_PRECEDING_TEXT.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session->set_client_capability(capability);

  commands::Command command;
  Segments segments;

  InsertCharacterChars("aiueo", session.get(), &command);
  SetComposer(session.get(), &segments);
  SetAiueo(&segments);
  FillT13Ns(&segments);
  convertermock->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  session->Convert(&command);
  EXPECT_FALSE(command.output().has_result());
  EXPECT_TRUE(command.output().has_preedit());
  // "あいうえお"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
            GetComposition(command));

  convertermock->SetCommitSegmentValue(&segments, true);
  command.Clear();
  session->Commit(&command);
  EXPECT_TRUE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());
  // "あいうえお"
  EXPECT_EQ("\xE3\x81\x82\xE3\x81\x84\xE3\x81\x86\xE3\x81\x88\xE3\x81\x8A",
            command.output().result().value());

  command.Clear();
  session->Undo(&command);

  // After check the status of revert_conversion_called.
  EXPECT_TRUE(convertermock->revert_conversion_called());
}

TEST_F(SessionTest, AutoConversion) {
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  // Auto Off
  config::Config config;
  config.set_use_auto_conversion(false);
  SetConfig(config);
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterChars("tesuto.", session.get(), &command);

    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_TRUE(command.output().preedit().segment(0).has_value());
    EXPECT_TRUE(command.output().preedit().segment(0).has_key());
    // "てすと。",
    EXPECT_EQ("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8\xE3\x80\x82",
              command.output().preedit().segment(0).key());
  }
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    // "てすと。"
    InsertCharacterString("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8\xE3\x80\x82",
                          "wrs/", session.get(), &command);

    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_TRUE(command.output().preedit().segment(0).has_value());
    EXPECT_TRUE(command.output().preedit().segment(0).has_key());
    // "てすと。",
    EXPECT_EQ("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8\xE3\x80\x82",
              command.output().preedit().segment(0).key());
  }

  // Auto On
  config.set_use_auto_conversion(true);
  SetConfig(config);
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());

    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterChars("tesuto.", session.get(), &command);

    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_TRUE(command.output().preedit().segment(0).has_value());
    EXPECT_TRUE(command.output().preedit().segment(0).has_key());

    // "あいうえお"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
              command.output().preedit().segment(0).key());

    EXPECT_TRUE(command.output().has_preedit());

    string key;
    for (int i = 0; i < command.output().preedit().segment_size(); ++i) {
      EXPECT_TRUE(command.output().preedit().segment(i).has_value());
      EXPECT_TRUE(command.output().preedit().segment(i).has_key());
      key += command.output().preedit().segment(i).key();
    }
    // "あいうえお"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
              key);
  }
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());

    commands::Command command;

    // The last "." is a triggering key for auto conversion
    // "てすと。",
    InsertCharacterString("\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8\xE3\x80\x82",
                          "wrs/", session.get(), &command);

    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_TRUE(command.output().preedit().segment(0).has_value());
    EXPECT_TRUE(command.output().preedit().segment(0).has_key());

    // "あいうえお"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
              command.output().preedit().segment(0).key());

    EXPECT_TRUE(command.output().has_preedit());

    string key;
    for (int i = 0; i < command.output().preedit().segment_size(); ++i) {
      EXPECT_TRUE(command.output().preedit().segment(i).has_value());
      EXPECT_TRUE(command.output().preedit().segment(i).has_key());
      key += command.output().preedit().segment(i).key();
    }
    // "あいうえお"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
              key);
  }

  // Don't trigger auto conversion for the pattern number + "."
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterChars("123.", session.get(), &command);

    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_TRUE(command.output().preedit().segment(0).has_value());
    EXPECT_TRUE(command.output().preedit().segment(0).has_key());

    // "１２３．"
    EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x93\xEF\xBC\x8E",
              command.output().preedit().segment(0).key());
    EXPECT_TRUE(command.output().has_preedit());
  }

  // Don't trigger auto conversion for the ".."
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterChars("..", session.get(), &command);

    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_TRUE(command.output().preedit().segment(0).has_value());
    EXPECT_TRUE(command.output().preedit().segment(0).has_key());

    // "。。"
    EXPECT_EQ("\xE3\x80\x82\xE3\x80\x82",
              command.output().preedit().segment(0).key());
    EXPECT_TRUE(command.output().has_preedit());
  }

  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    // "１２３。"
    InsertCharacterString("\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x93\xE3\x80\x82",
                          "123.", session.get(), &command);

    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_TRUE(command.output().preedit().segment(0).has_value());
    EXPECT_TRUE(command.output().preedit().segment(0).has_key());

    // "１２３．"
    EXPECT_EQ("\xEF\xBC\x91\xEF\xBC\x92\xEF\xBC\x93\xEF\xBC\x8E",
              command.output().preedit().segment(0).key());
    EXPECT_TRUE(command.output().has_preedit());
  }

  // Don't trigger auto conversion for "." only.
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    InsertCharacterChars(".", session.get(), &command);

    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_TRUE(command.output().preedit().segment(0).has_value());
    EXPECT_TRUE(command.output().preedit().segment(0).has_key());

    // "。"
    EXPECT_EQ("\xE3\x80\x82",
              command.output().preedit().segment(0).key());
    EXPECT_TRUE(command.output().has_preedit());
  }

  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());
    commands::Command command;

    // The last "." is a triggering key for auto conversion
    // "。",
    InsertCharacterString("\xE3\x80\x82", "/", session.get(), &command);

    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_TRUE(command.output().preedit().segment(0).has_value());
    EXPECT_TRUE(command.output().preedit().segment(0).has_key());

    // "。"
    EXPECT_EQ("\xE3\x80\x82",
              command.output().preedit().segment(0).key());
    EXPECT_TRUE(command.output().has_preedit());
  }

  // Do auto conversion even if romanji-table is modified.
  {
    scoped_ptr<Session> session(new Session);
    InitSessionToPrecomposition(session.get());

    // Modify romanji-table to convert "zz" -> "。"
    composer::Table zz_table;
    // "てすと。"
    zz_table.AddRule("te", "\xE3\x81\xA6", "");
    zz_table.AddRule("su", "\xE3\x81\x99", "");
    zz_table.AddRule("to", "\xE3\x81\xA8", "");
    zz_table.AddRule("zz", "\xE3\x80\x82", "");
    session->get_internal_composer_only_for_unittest()->SetTable(&zz_table);

    // The last "zz" is converted to "." and triggering key for auto conversion
    commands::Command command;
    InsertCharacterChars("tesutozz", session.get(), &command);

    EXPECT_TRUE(command.output().has_preedit());
    EXPECT_EQ(1, command.output().preedit().segment_size());
    EXPECT_TRUE(command.output().preedit().segment(0).has_value());
    EXPECT_TRUE(command.output().preedit().segment(0).has_key());

    // "あいうえお"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
              command.output().preedit().segment(0).key());
    EXPECT_TRUE(command.output().has_preedit());

    string key;
    for (int i = 0; i < command.output().preedit().segment_size(); ++i) {
      EXPECT_TRUE(command.output().preedit().segment(i).has_value());
      EXPECT_TRUE(command.output().preedit().segment(i).has_key());
      key += command.output().preedit().segment(i).key();
    }
    // "あいうえお"
    EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a",
              key);
  }

  {
    const char trigger_key[] = ".,?!";

    // try all possible patterns.
    for (int kana_mode = 0; kana_mode < 2; ++kana_mode) {
      for (int onoff = 0; onoff < 2; ++onoff) {
        for (int pattern = 0; pattern <= 16; ++pattern) {
          config.set_use_auto_conversion(static_cast<bool>(onoff));
          config.set_auto_conversion_key(pattern);
          config::ConfigHandler::SetConfig(config);

          int flag[4];
          flag[0] = static_cast<int>(
              config.auto_conversion_key() &
              config::Config::AUTO_CONVERSION_KUTEN);
          flag[1] = static_cast<int>(
              config.auto_conversion_key() &
              config::Config::AUTO_CONVERSION_TOUTEN);
          flag[2] = static_cast<int>(
              config.auto_conversion_key() &
              config::Config::AUTO_CONVERSION_QUESTION_MARK);
          flag[3] = static_cast<int>(
              config.auto_conversion_key() &
              config::Config::AUTO_CONVERSION_EXCLAMATION_MARK);

          for (int i = 0; i < 4; ++i) {
            scoped_ptr<Session> session(new Session);
            InitSessionToPrecomposition(session.get());
            commands::Command command;

            if (kana_mode) {
              // "てすと"
              string key = "\xE3\x81\xA6\xE3\x81\x99\xE3\x81\xA8";
              key += trigger_key[i];
              InsertCharacterString(key, "wst/", session.get(), &command);
            } else {
              string key = "tesuto";
              key += trigger_key[i];
              InsertCharacterChars(key, session.get(), &command);
            }
            EXPECT_TRUE(command.output().has_preedit());
            EXPECT_EQ(1, command.output().preedit().segment_size());
            EXPECT_TRUE(command.output().preedit().segment(0).has_value());
            EXPECT_TRUE(command.output().preedit().segment(0).has_key());

            if (onoff > 0 && flag[i] > 0) {
              // "あいうえお"
              EXPECT_EQ("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86"
                        "\xe3\x81\x88\xe3\x81\x8a",
                        command.output().preedit().segment(0).key());
            } else {
              // Not "あいうえお"
              EXPECT_NE("\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86"
                        "\xe3\x81\x88\xe3\x81\x8a",
                        command.output().preedit().segment(0).key());
            }
          }
        }
      }
    }
  }
}

TEST_F(SessionTest, FillHistoryContext) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  const string kHistory[] = {"abc", "def"};

  Segments segments;
  {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = kHistory[0];
  }
  // FinishConversion is expected to return empty Segments.
  convertermock_->SetFinishConversion(&segments, true);

  // dummy code to set segments above
  EXPECT_TRUE(SendKey("a", session.get(), &command));
  EXPECT_TRUE(SendKey("Enter", session.get(), &command));

  // Fill context in TestSendKey
  EXPECT_TRUE(TestSendKey("x", session.get(), &command));
  EXPECT_TRUE(command.input().context().has_preceding_text());
  EXPECT_EQ("abc", command.input().context().preceding_text());

  // Fill context in SendKey
  EXPECT_TRUE(SendKey("x", session.get(), &command));
  EXPECT_TRUE(command.input().context().has_preceding_text());
  EXPECT_EQ("abc", command.input().context().preceding_text());

  // Fill context in SendCommand
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::REVERT);
  EXPECT_TRUE(session->SendCommand(&command));
  EXPECT_EQ("abc", command.input().context().preceding_text());

  // Multiple history segments are concatenated and put into context.
  {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = kHistory[1];
  }
  // FinishConversion is expected to return empty Segments.
  convertermock_->SetFinishConversion(&segments, true);

  // dummy code to set segments above
  EXPECT_TRUE(SendKey("a", session.get(), &command));
  EXPECT_TRUE(SendKey("Enter", session.get(), &command));

  EXPECT_TRUE(TestSendKey("x", session.get(), &command));
  EXPECT_TRUE(command.input().context().has_preceding_text());
  EXPECT_EQ("abcdef", command.input().context().preceding_text());
}

TEST_F(SessionTest, ExpandCompositionForNestedCalculation) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Context context;
  // "あい 1１"
  context.set_preceding_text("\xE3\x81\x82\xE3\x81\x84 1\xEF\xBC\x91");

  // Incapable case
  commands::Capability capability;
  session->set_client_capability(capability);

  commands::Command command;
  InsertCharacterCharsWithContext("+1=", context, session.get(), &command);

  EXPECT_EQ(0, command.output().deletion_range().offset());
  EXPECT_EQ(0, command.output().deletion_range().length());

  // "＋１＝"
  EXPECT_EQ("\xEF\xBC\x8B\xEF\xBC\x91\xEF\xBC\x9D",
            command.output().preedit().segment(0).key());

  command.Clear();
  session->Revert(&command);
  command.Clear();

  // Capable case
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session->set_client_capability(capability);

  InsertCharacterCharsWithContext("+1=", context, session.get(), &command);

  EXPECT_TRUE(command.output().has_deletion_range());
  EXPECT_EQ(-2, command.output().deletion_range().offset());
  EXPECT_EQ(2, command.output().deletion_range().length());
  // "1１＋１＝"
  EXPECT_EQ("1\xEF\xBC\x91\xEF\xBC\x8B\xEF\xBC\x91\xEF\xBC\x9D",
            command.output().preedit().segment(0).key());
}

TEST_F(SessionTest, InputSpaceWithKatakanaMode) {
  // This is a unittest against http://b/3203944.
  // Input mode should not be changed when a space key is typed.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  EXPECT_TRUE(session->InputModeHiragana(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(mozc::commands::HIRAGANA, command.output().mode());

  command.Clear();
  command.mutable_input()->set_type(commands::Input::SEND_KEY);
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::SPACE);
  command.mutable_input()->mutable_key()->set_mode(
      commands::FULL_KATAKANA);
  EXPECT_TRUE(session->SendKey(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_TRUE(command.output().has_result());
  EXPECT_EQ(mozc::commands::FULL_KATAKANA, command.output().mode());
}

TEST_F(SessionTest, AlphanumericOfSSH) {
  // This is a unittest against http://b/3199626
  // 'ssh' (っｓｈ) + F10 should be 'ssh'.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  InsertCharacterChars("ssh", session.get(), &command);
  // "っｓｈ"
  EXPECT_EQ("\xE3\x81\xA3\xEF\xBD\x93\xEF\xBD\x88", GetComposition(command));

  Segments segments;
  // Set a dummy segments for ConvertToHalfASCII.
  {
    Segment *segment;
    segment = segments.add_segment();
    //    // "っｓｈ"
    //    segment->set_key("\xE3\x81\xA3\xEF\xBD\x93\xEF\xBD\x88");
    // "っsh"
    segment->set_key("\xE3\x81\xA3sh");

    segment->add_candidate()->value = "[SSH]";
  }
  SetComposer(session.get(), &segments);
  FillT13Ns(&segments);
  convertermock_->SetStartConversionWithComposer(&segments, true);

  command.Clear();
  EXPECT_TRUE(session->ConvertToHalfASCII(&command));
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ("ssh", GetComposition(command));
}





TEST_F(SessionTest, RequestConvertReverse) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  EXPECT_TRUE(session->RequestConvertReverse(&command));
  EXPECT_FALSE(command.output().has_result());
  EXPECT_FALSE(command.output().has_deletion_range());
  EXPECT_TRUE(command.output().has_callback());
  EXPECT_TRUE(command.output().callback().has_session_command());
  EXPECT_EQ(commands::SessionCommand::CONVERT_REVERSE,
            command.output().callback().session_command().type());
}

TEST_F(SessionTest, ConvertReverse) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  // "阿伊宇江於"
  const string kanji_aiueo =
      "\xe9\x98\xbf\xe4\xbc\x8a\xe5\xae\x87\xe6\xb1\x9f\xe6\x96\xbc";
  // "あいうえお"
  const string hiragana_aiueo =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";

  commands::Command command;
  SetupCommandForReverseConversion(kanji_aiueo, command.mutable_input());
  SetupMockForReverseConversion(kanji_aiueo, hiragana_aiueo);

  EXPECT_TRUE(session->SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(kanji_aiueo,
            command.output().preedit().segment(0).value());
  EXPECT_EQ(kanji_aiueo,
            command.output().all_candidate_words().candidates(0).value());
  EXPECT_TRUE(command.output().has_candidates());
  EXPECT_GT(command.output().candidates().candidate_size(), 0);
}

TEST_F(SessionTest, EscapeFromConvertReverse) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  // "阿伊宇江於"
  const string kanji_aiueo =
      "\xe9\x98\xbf\xe4\xbc\x8a\xe5\xae\x87\xe6\xb1\x9f\xe6\x96\xbc";
  // "あいうえお"
  const string hiragana_aiueo =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";

  commands::Command command;
  SetupCommandForReverseConversion(kanji_aiueo, command.mutable_input());
  SetupMockForReverseConversion(kanji_aiueo, hiragana_aiueo);

  EXPECT_TRUE(session->SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(kanji_aiueo, GetComposition(command));

  SendKey("ESC", session.get(), &command);

  // KANJI should be converted into HIRAGANA in pre-edit state.
  EXPECT_TRUE(command.output().has_preedit());
  EXPECT_EQ(hiragana_aiueo, GetComposition(command));

  SendKey("ESC", session.get(), &command);

  // Fixed KANJI should be output
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_TRUE(command.output().has_result());
  EXPECT_EQ(kanji_aiueo, command.output().result().value());
}

TEST_F(SessionTest, SecondEscapeFromConvertReverse) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  // "阿伊宇江於"
  const string kanji_aiueo =
      "\xe9\x98\xbf\xe4\xbc\x8a\xe5\xae\x87\xe6\xb1\x9f\xe6\x96\xbc";
  // "あいうえお"
  const string hiragana_aiueo =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";

  commands::Command command;
  SetupCommandForReverseConversion(kanji_aiueo, command.mutable_input());
  SetupMockForReverseConversion(kanji_aiueo, hiragana_aiueo);

  EXPECT_TRUE(session->SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(kanji_aiueo, GetComposition(command));

  SendKey("ESC", session.get(), &command);
  SendKey("ESC", session.get(), &command);

  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_TRUE(command.output().has_result());
  EXPECT_EQ(kanji_aiueo, command.output().result().value());

  command.Clear();
  SendKey("a", session.get(), &command);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", GetComposition(command));
  SendKey("ESC", session.get(), &command);

  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, EscapeFromCompositionAfterConvertReverse) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  // "阿伊宇江於"
  const string kanji_aiueo =
      "\xe9\x98\xbf\xe4\xbc\x8a\xe5\xae\x87\xe6\xb1\x9f\xe6\x96\xbc";
  // "あいうえお"
  const string hiragana_aiueo =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";

  commands::Command command;
  SetupCommandForReverseConversion(kanji_aiueo, command.mutable_input());
  SetupMockForReverseConversion(kanji_aiueo, hiragana_aiueo);

  // Conversion Reverse
  EXPECT_TRUE(session->SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(kanji_aiueo, GetComposition(command));

  session->Commit(&command);

  EXPECT_TRUE(command.output().has_result());
  EXPECT_EQ(kanji_aiueo, command.output().result().value());

  // Escape in composition state
  command.Clear();
  SendKey("a", session.get(), &command);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", GetComposition(command));
  SendKey("ESC", session.get(), &command);

  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, ConvertReverseFromOffState) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  // "阿伊宇江於"
  const string kanji_aiueo =
      "\xe9\x98\xbf\xe4\xbc\x8a\xe5\xae\x87\xe6\xb1\x9f\xe6\x96\xbc";
  // "あいうえお"
  const string hiragana_aiueo =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";

  // IMEOff
  commands::Command off_command;
  off_command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::OFF);
  session->SendKey(&off_command);

  commands::Command command;
  SetupCommandForReverseConversion(kanji_aiueo, command.mutable_input());
  SetupMockForReverseConversion(kanji_aiueo, hiragana_aiueo);
  EXPECT_TRUE(session->SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
}

TEST_F(SessionTest, DCHECKFailureAfterConvertReverse) {
  // This is a unittest against http://b/5145295.
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  // "あいうえお"
  const string kAiueo =
      "\xe3\x81\x82\xe3\x81\x84\xe3\x81\x86\xe3\x81\x88\xe3\x81\x8a";

  commands::Command command;
  SetupCommandForReverseConversion(kAiueo, command.mutable_input());
  SetupMockForReverseConversion(kAiueo, kAiueo);
  EXPECT_TRUE(session->SendCommand(&command));
  EXPECT_TRUE(command.output().consumed());
  EXPECT_EQ(kAiueo, command.output().preedit().segment(0).value());
  EXPECT_EQ(kAiueo,
            command.output().all_candidate_words().candidates(0).value());
  EXPECT_TRUE(command.output().has_candidates());
  EXPECT_GT(command.output().candidates().candidate_size(), 0);

  command.Clear();
  SendKey("ESC", session.get(), &command);

  command.Clear();
  SendKey("a", session.get(), &command);
  // "あいうえおあ"
  EXPECT_EQ(kAiueo + "\xe3\x81\x82",
            command.output().preedit().segment(0).value());
  EXPECT_FALSE(command.output().has_result());
}

TEST_F(SessionTest, LaunchTool) {
  scoped_ptr<Session> session(new Session);

  {
    commands::Command command;
    EXPECT_TRUE(session->LaunchConfigDialog(&command));
    EXPECT_EQ(commands::Output::CONFIG_DIALOG,
              command.output().launch_tool_mode());
    EXPECT_TRUE(command.output().consumed());
  }

  {
    commands::Command command;
    EXPECT_TRUE(session->LaunchDictionaryTool(&command));
    EXPECT_EQ(commands::Output::DICTIONARY_TOOL,
              command.output().launch_tool_mode());
    EXPECT_TRUE(command.output().consumed());
  }

  {
    commands::Command command;
    EXPECT_TRUE(session->LaunchWordRegisterDialog(&command));
    EXPECT_EQ(commands::Output::WORD_REGISTER_DIALOG,
              command.output().launch_tool_mode());
    EXPECT_TRUE(command.output().consumed());
  }
}


TEST_F(SessionTest, MoveCursor) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;

  InsertCharacterChars("MOZUKU", session.get(), &command);
  EXPECT_EQ(6, command.output().preedit().cursor());
  session->MoveCursorLeft(&command);
  EXPECT_EQ(5, command.output().preedit().cursor());
  command.mutable_input()->mutable_command()->set_cursor_position(3);
  session->MoveCursorTo(&command);
  EXPECT_EQ(3, command.output().preedit().cursor());
  session->MoveCursorRight(&command);
  EXPECT_EQ(4, command.output().preedit().cursor());
}

TEST_F(SessionTest, CommitHead) {
  scoped_ptr<Session> session(new Session);
  composer::Table table;
  // "も"
  table.AddRule("mo", "\xe3\x82\x82", "");
  // "ず"
  table.AddRule("zu", "\xe3\x81\x9a", "");

  session->get_internal_composer_only_for_unittest()->SetTable(&table);

  InitSessionToPrecomposition(session.get());
  commands::Command command;

  InsertCharacterChars("moz", session.get(), &command);
  // 'もｚ'
  EXPECT_EQ("\xe3\x82\x82\xef\xbd\x9a", GetComposition(command));
  command.Clear();
  session->CommitHead(1, &command);
  EXPECT_EQ(commands::Result_ResultType_STRING,
            command.output().result().type());
  EXPECT_EQ("\xe3\x82\x82", command.output().result().value());  // 'も'
  EXPECT_EQ("\xef\xbd\x9a", GetComposition(command));            // 'ｚ'
  InsertCharacterChars("u", session.get(), &command);
  // 'ず'
  EXPECT_EQ("\xe3\x81\x9a", GetComposition(command));
}


TEST_F(SessionTest, EditCancel) {
  Session session;
  InitSessionToPrecomposition(&session);

  Segments segments_mo;
  {
    segments_mo.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  {  // Cancel of Suggestion
    commands::Command command;
    SendKey("M", &session, &command);

    command.Clear();
    convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
    SendKey("O", &session, &command);
    ASSERT_TRUE(command.output().has_candidates());
    EXPECT_EQ(2, command.output().candidates().candidate_size());
    EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());

    command.Clear();
    session.EditCancel(&command);
    EXPECT_EQ("", GetComposition(command));
    EXPECT_EQ(0, command.output().candidates().candidate_size());
    EXPECT_FALSE(command.output().has_result());
  }

  {  // Cancel of Reverse convertion
    commands::Command command;

    // "[MO]" is a converted string like Kanji.
    // "MO" is an input string like Hiragana.
    SetupCommandForReverseConversion("[MO]", command.mutable_input());
    SetupMockForReverseConversion("[MO]", "MO");
    EXPECT_TRUE(session.SendCommand(&command));

    command.Clear();
    convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
    session.ConvertCancel(&command);
    ASSERT_TRUE(command.output().has_candidates());
    EXPECT_EQ(2, command.output().candidates().candidate_size());
    EXPECT_EQ("MOCHA", command.output().candidates().candidate(0).value());

    command.Clear();
    session.EditCancel(&command);
    EXPECT_EQ("", GetComposition(command));
    EXPECT_EQ(0, command.output().candidates().candidate_size());
    EXPECT_TRUE(command.output().has_result());
  }
}

TEST_F(SessionTest, ImeOff) {
  scoped_ptr<ConverterMockForReset> convertermock(new ConverterMockForReset);
  ConverterFactory::SetConverter(convertermock.get());

  convertermock->Reset();
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  session->IMEOff(&command);

  EXPECT_TRUE(convertermock->reset_conversion_called());
}

// We use following represenetaion for indicating all state-change pass.
// State:
//   [PRECOMP] : Precomposition state
//   [COMP-L]  : Composition state with cursor at left most.
//   [COMP-M]  : Composition state with cursor at middle of composition.
//   [COMP-R]  : Composition state with cursor at right most.
//   [CONV-L]  : Conversion state with cursor at left most.
//   [CONV-M]  : Conversion state with cursor at middle of composition.
// State Change:
//  "abcdef" means composition characters.
//  "^" means suggestion/conversion window left-top position
//  "|" means caret position.
// NOTE:
//  It is not necessary to test in case as follows because they never occur.
//   - [PRECOMP] -> [PRECOMP]
//   - [PRECOMP] -> [COMP-M] or [COMP-L]
//   - [PRECOMP] -> [CONV-L] or [CONV-R]
//  Also it is not necessary to test in case of changing to CONVERSION state,
//  because conversion window is always shown under current cursor.
TEST_F(SessionTest, CaretManagePrecompositionToCompositionTest) {
  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  Segments segments;
  const int kCaretInitialXpos = 10;
  commands::Rectangle rectangle;
  rectangle.set_x(kCaretInitialXpos);
  rectangle.set_y(0);
  rectangle.set_width(0);
  rectangle.set_height(0);

  Segments segments_mo;
  {
    segments_mo.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  // [PRECOMP] -> [COMP-R]:
  //  Expectation: -> ^a|
  SetCaretLocation(rectangle, session.get());

  SendKey("M", session.get(), &command);

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  command.Clear();
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  SendKey("O", session.get(), &command);
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());
}

TEST_F(SessionTest, CaretManageCompositionToCompositionTest) {
  Segments segments_m;
  {
    segments_m.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_m.add_segment();
    segment->set_key("M");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  Segments segments_mo;
  {
    segments_mo.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  Segments segments_moz;
  {
    segments_moz.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_moz.add_segment();
    segment->set_key("MOZ");
    segment->add_candidate()->value = "MOZUKU";
  }

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());
  commands::Command command;
  const int kCaretInitialXpos = 10;
  commands::Rectangle rectangle;
  rectangle.set_x(kCaretInitialXpos);
  rectangle.set_y(0);
  rectangle.set_width(0);
  rectangle.set_height(0);

  SetCaretLocation(rectangle, session.get());

  SendKey("M", session.get(), &command);

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  command.Clear();
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  SendKey("O", session.get(), &command);
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  // [COMP-R] -> [COMP-R]:
  //  Expectation: ^mo| -> ^moz|
  convertermock_->SetStartSuggestionWithComposer(&segments_moz, true);
  command.Clear();
  SendKey("Z", session.get(), &command);
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  // [COMP-R] -> [COMP-R]:
  //  Expectation: ^moz| -> ^mo|
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  command.Clear();
  SendKey("Backspace", session.get(), &command);
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  // [COMP-R] -> [COMP-M]:
  //  Expectation: ^mo| -> ^m|o
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  command.Clear();
  EXPECT_TRUE(session->MoveCursorLeft(&command));
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  // [COMP-M] -> [COMP-R]:
  //  Expectation: ^m|o -> ^mo|
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  command.Clear();
  EXPECT_TRUE(session->MoveCursorToEnd(&command));
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  // [COMP-R] -> [COMP-L]:
  //  Expectation: ^mo| -> ^|mo
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  command.Clear();
  EXPECT_TRUE(session->MoveCursorToBeginning(&command));
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  // [COMP-L] -> [COMP-M]:
  //  Expectation: ^|mo -> ^m|o
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  command.Clear();
  EXPECT_TRUE(session->MoveCursorRight(&command));
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  // [COMP-M] -> [COMP-L]:
  //  Expectation: ^m|o -> ^m|
  convertermock_->SetStartSuggestionWithComposer(&segments_m, true);
  command.Clear();
  EXPECT_TRUE(session->Delete(&command));
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());
}

TEST_F(SessionTest, CaretManageConversionToCompositionTest) {
  // There are two ways to change state from CONVERSION to COMPOSITION,
  // One is canceling conversion with BS key. In this case cursor location
  // becomes right most and suggest position is left most.
  // The second is continuing typing under conversion. If user types key under
  // conversion, the IME commits selected candidate and creates new composition
  // at once.
  // For example:
  //    KeySequence: 'a' -> SP -> SP -> 'i'
  //    Expectation: a^|i (a and i are corresponding japanese characters)
  //    Actual: ^a|i
  // In the session side, we can only support the former case.

  scoped_ptr<Session> session(new Session);
  InitSessionToPrecomposition(session.get());

  commands::Command command;
  Segments segments;
  const int kCaretInitialXpos = 10;
  commands::Rectangle rectangle;
  rectangle.set_x(kCaretInitialXpos);
  rectangle.set_y(0);
  rectangle.set_width(0);
  rectangle.set_height(0);

  Segments segments_m;
  {
    segments_m.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_m.add_segment();
    segment->set_key("M");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  Segments segments_mo;
  {
    segments_mo.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_mo.add_segment();
    segment->set_key("MO");
    segment->add_candidate()->value = "MOCHA";
    segment->add_candidate()->value = "MOZUKU";
  }

  Segments segments_moz;
  {
    segments_moz.set_request_type(Segments::SUGGESTION);
    Segment *segment;
    segment = segments_moz.add_segment();
    segment->set_key("MOZ");
    segment->add_candidate()->value = "MOZUKU";
  }

  Segments segments_m_conv;
  {
    segments_m_conv.set_request_type(Segments::CONVERSION);
    Segment *segment;
    segment = segments_m_conv.add_segment();
    segment->set_key("M");
    segment->add_candidate()->value = "M";
    segment->add_candidate()->value = "m";
  }

  // [CONV-L] -> [COMP-R]
  //  Expectation: ^|a -> ^a|
  SetCaretLocation(rectangle, session.get());

  SendKey("M", session.get(), &command);

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  command.Clear();
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  SendKey("O", session.get(), &command);
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  command.Clear();
  SetComposer(session.get(), &segments_m_conv);
  FillT13Ns(&segments_m_conv);
  convertermock_->SetStartConversionWithComposer(&segments_m_conv, true);
  EXPECT_TRUE(session->Convert(&command));

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  command.Clear();
  convertermock_->SetStartSuggestionWithComposer(&segments_m, true);
  EXPECT_TRUE(session->ConvertCancel(&command));
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());

  // [CONV-M] -> [COMP-R]
  //  Expectation: ^a|b -> ^ab|
  session.reset(new Session());
  InitSessionToPrecomposition(session.get());
  command.Clear();
  rectangle.set_x(kCaretInitialXpos);

  SetCaretLocation(rectangle, session.get());

  SendKey("M", session.get(), &command);

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  command.Clear();
  convertermock_->SetStartSuggestionWithComposer(&segments_mo, true);
  SendKey("O", session.get(), &command);

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  command.Clear();
  SetComposer(session.get(), &segments_m_conv);
  FillT13Ns(&segments_m_conv);
  convertermock_->SetStartConversionWithComposer(&segments_m_conv, true);
  EXPECT_TRUE(session->Convert(&command));

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  command.Clear();
  SendSpecialKey(commands::KeyEvent::LEFT, session.get(), &command);

  rectangle.set_x(rectangle.x() + 5);
  SetCaretLocation(rectangle, session.get());

  command.Clear();
  convertermock_->SetStartSuggestionWithComposer(&segments_m, true);
  EXPECT_TRUE(session->ConvertCancel(&command));
  EXPECT_EQ(kCaretInitialXpos,
            command.output().candidates().composition_rectangle().x());
}

}  // namespace session
}  // namespace mozc
