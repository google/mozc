// Copyright 2010-2014, Google Inc.
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

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/number_util.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/protobuf/text_format.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "engine/user_data_manager_interface.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"
#include "session/request_test_util.h"
#include "session/session_handler_test_util.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

DECLARE_string(test_srcdir);
DECLARE_string(test_tmpdir);

namespace {

using mozc::FileUtil;
using mozc::InputFileStream;
using mozc::KeyParser;
using mozc::NumberUtil;
using mozc::StringPiece;
using mozc::Util;
using mozc::commands::CandidateList;
using mozc::commands::CandidateWord;
using mozc::commands::CompositionMode;
using mozc::commands::CompositionMode_Parse;
using mozc::commands::Input;
using mozc::commands::KeyEvent;
using mozc::commands::Output;
using mozc::commands::Request;
using mozc::commands::RequestForUnitTest;
using mozc::config::Config;
using mozc::config::ConfigHandler;
using mozc::MockDataEngineFactory;
using mozc::EngineInterface;
using mozc::protobuf::FieldDescriptor;
using mozc::protobuf::Message;
using mozc::protobuf::TextFormat;
using mozc::session::testing::SessionHandlerTestBase;
using mozc::session::testing::TestSessionClient;
using testing::WithParamInterface;

class SessionHandlerScenarioTest : public SessionHandlerTestBase,
                                   public WithParamInterface<const char *> {
 protected:
  virtual void SetUp() {
    // Note that singleton Config instance is backed up and restored
    // by SessionHandlerTestBase's SetUp and TearDown methods.
    SessionHandlerTestBase::SetUp();

    engine_.reset(MockDataEngineFactory::Create());
    client_.reset(new TestSessionClient(engine_.get()));
    config_.reset(new Config);
    last_output_.reset(new Output);
    request_.reset(new Request);

    ConfigHandler::GetConfig(config_.get());
  }

  void ClearAll() {
    ResetContext();
    ClearUserPrediction();
    ClearUsageStats();
  }

  void ResetContext() {
    ASSERT_TRUE(client_->ResetContext());
    last_output_->Clear();
  }

  void ClearUserPrediction() {
    EXPECT_TRUE(client_->ClearUserPrediction());
    EXPECT_TRUE(engine_->GetUserDataManager()->WaitForSyncerForTest());
  }

  void ClearUsageStats() {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  scoped_ptr<EngineInterface> engine_;
  scoped_ptr<TestSessionClient> client_;
  scoped_ptr<Config> config_;
  scoped_ptr<Output> last_output_;
  scoped_ptr<Request> request_;
};

// Tests should be passed.
const char *kScenarioFileList[] = {
#define DATA_DIR "data/test/session/scenario/"
  DATA_DIR "auto_partial_suggestion.txt",
  DATA_DIR "b7132535_scenario.txt",
  DATA_DIR "b7321313_scenario.txt",
  DATA_DIR "b7548679_scenario.txt",
  DATA_DIR "b8703702_scenario.txt",
  DATA_DIR "change_request.txt",
  DATA_DIR "clear_user_prediction.txt",
  DATA_DIR "commit.txt",
  DATA_DIR "composition_display_as.txt",
  DATA_DIR "conversion.txt",
  DATA_DIR "conversion_display_as.txt",
  DATA_DIR "conversion_with_history_segment.txt",
  DATA_DIR "conversion_with_long_history_segments.txt",
  DATA_DIR "convert_from_full_ascii_to_t13n.txt",
  DATA_DIR "convert_from_full_katakana_to_t13n.txt",
  DATA_DIR "convert_from_half_ascii_to_t13n.txt",
  DATA_DIR "convert_from_half_katakana_to_t13n.txt",
  DATA_DIR "convert_from_hiragana_to_t13n.txt",
  DATA_DIR "delete_history.txt",
  DATA_DIR "desktop_t13n_candidates.txt",
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // "InputModeX" commands are not supported on Mac and Android.
  // Mac: We do not have the way to change the mode indicator from IME.
  // Android: Input mode change is performed by changing software keyboard.
  DATA_DIR "input_mode.txt",
#endif
  DATA_DIR "insert_characters.txt",
  DATA_DIR "mobile_qwerty_transliteration_scenario.txt",
  DATA_DIR "mobile_t13n_candidates.txt",
  DATA_DIR "on_off_cancel.txt",
  DATA_DIR "partial_suggestion.txt",
  DATA_DIR "pending_character.txt",
  DATA_DIR "predict_and_convert.txt",
  DATA_DIR "reconvert.txt",
  DATA_DIR "revert.txt",
  DATA_DIR "segment_focus.txt",
  DATA_DIR "segment_width.txt",
  DATA_DIR "twelvekeys_switch_inputmode_scenario.txt",
  DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario.txt",
  DATA_DIR "undo.txt",
#undef DATA_DIR
};

INSTANTIATE_TEST_CASE_P(SessionHandlerScenarioParameters,
                        SessionHandlerScenarioTest,
                        ::testing::ValuesIn(kScenarioFileList));

const char *kUsageStatsScenarioFileList[] = {
#define DATA_DIR "data/test/session/scenario/usage_stats/"
  DATA_DIR "auto_partial_suggestion.txt",
  DATA_DIR "backspace_after_commit.txt",
  DATA_DIR "backspace_after_commit_after_backspace.txt",
  DATA_DIR "composition.txt",
  DATA_DIR "continue_input.txt",
  DATA_DIR "continuous_input.txt",
  DATA_DIR "conversion.txt",
  DATA_DIR "insert_space.txt",
  DATA_DIR "language_aware_input.txt",
  DATA_DIR "mouse_select_from_suggestion.txt",
  DATA_DIR "multiple_backspace_after_commit.txt",
  DATA_DIR "multiple_segments.txt",
  DATA_DIR "numpad_in_direct_input_mode.txt",
  DATA_DIR "prediction.txt",
  DATA_DIR "select_candidates_in_multiple_segments.txt",
  DATA_DIR "select_candidates_in_multiple_segments_and_expand_segment.txt",
  DATA_DIR "select_minor_conversion.txt",
  DATA_DIR "select_minor_prediction.txt",
  DATA_DIR "select_prediction.txt",
  DATA_DIR "select_t13n_by_key.txt",
#ifndef OS_LINUX
  // This test requires cascading window.
  // TODO(hsumita): Removes this ifndef block.
  DATA_DIR "select_t13n_on_cascading_window.txt",
#endif  // OS_LINUX
  DATA_DIR "suggestion.txt",
  DATA_DIR "switch_kana_type.txt",
  DATA_DIR "zero_query_suggestion.txt",
#undef DATA_DIR
};
INSTANTIATE_TEST_CASE_P(SessionHandlerUsageStatsScenarioParameters,
                        SessionHandlerScenarioTest,
                        ::testing::ValuesIn(kUsageStatsScenarioFileList));

// Temporarily disabled test scenario.
//
// NOTE: If you want to have test scenario which does not pass at this
// moment but for the recording, you can describe it as follows.
const char *kFailedScenarioFileList[] = {
  // Requires multiple session handling.
  "data/test/session/scenario/usage_stats/multiple_sessions.txt",
};
INSTANTIATE_TEST_CASE_P(DISABLED_SessionHandlerScenarioParameters,
                        SessionHandlerScenarioTest,
                        ::testing::ValuesIn(kFailedScenarioFileList));

bool GetCandidateIdByValue(const StringPiece value,
                           const Output &output, uint32 *id) {
  if (!output.has_all_candidate_words()) {
    return false;
  }

  const CandidateList &all_candidate_words =  output.all_candidate_words();
  for (int i = 0; i < all_candidate_words.candidates_size(); ++i) {
    const CandidateWord &candidate_word = all_candidate_words.candidates(i);
    if (candidate_word.has_value() &&
        candidate_word.value() == value) {
      *id = i;
      return true;
    }
  }
  return false;
}

bool SetOrAddFieldValueFromString(const string &name, const string &value,
                                  Message *message) {
  const FieldDescriptor *field =
      message->GetDescriptor()->FindFieldByName(name);
  if (!field) {
    LOG(ERROR) << "Unknown field name: " << name;
    return false;
  }
  // String type value should be quoted for ParseFieldValueFromString().
  if (field->type() == FieldDescriptor::TYPE_STRING &&
      (value[0] != '"' || value[value.size() - 1] != '"')) {
    LOG(ERROR) << "String type value should be quoted: " << value;
    return false;
  }
  return TextFormat::ParseFieldValueFromString(value, field, message);
}

// Parses protobuf from string without validation.
// input sample: context.experimental_features="chrome_omnibox"
// We cannot use TextFormat::ParseFromString since it doesn't allow invalid
// protobuf. (e.g. lack of required field)
bool ParseProtobufFromString(const string &text, Message *message) {
  const size_t separator_pos = text.find('=');
  const string full_name = text.substr(0, separator_pos);
  const string value = text.substr(separator_pos + 1);
  vector<string> names;
  Util::SplitStringUsing(full_name, ".", &names);

  Message *msg = message;
  for (size_t i = 0; i < names.size() - 1; ++i) {
    const FieldDescriptor *field =
        msg->GetDescriptor()->FindFieldByName(names[i]);
    if (!field) {
      LOG(ERROR) << "Unknown field name: " << names[i];
      return false;
    }
    msg = msg->GetReflection()->MutableMessage(msg, field);
  }

  return SetOrAddFieldValueFromString(names[names.size() - 1], value, msg);
}

bool IsInAllCandidateWords(
    const StringPiece expected_candidate,
    const Output &output) {
  uint32 tmp;
  return GetCandidateIdByValue(expected_candidate, output, &tmp);
}

::testing::AssertionResult IsInAllCandidateWordsWithFormat(
    const char *expected_candidate_string,
    const char *output_string,
    const string &expected_candidate,
    const Output &output) {
  if (!IsInAllCandidateWords(expected_candidate, output)) {
    return ::testing::AssertionFailure()
        << expected_candidate_string << "(" << expected_candidate << ")"
        << " is not found in " << output_string << "\n"
        << output.Utf8DebugString();
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult IsNotInAllCandidateWordsWithFormat(
    const char *expected_candidate_string,
    const char *output_string,
    const StringPiece expected_candidate,
    const Output &output) {
  if (IsInAllCandidateWords(expected_candidate, output)) {
    return ::testing::AssertionFailure()
        << expected_candidate_string << "(" << expected_candidate << ")"
        << " is found in " << output_string << "\n"
        << output.Utf8DebugString();
  }
  return ::testing::AssertionSuccess();
}

#define EXPECT_IN_ALL_CANDIDATE_WORDS(expected_candidate, output) \
  EXPECT_PRED_FORMAT2(IsInAllCandidateWordsWithFormat, \
                      expected_candidate, output)
#define EXPECT_NOT_IN_ALL_CANDIDATE_WORDS(expected_candidate, output) \
  EXPECT_PRED_FORMAT2(IsNotInAllCandidateWordsWithFormat, \
                      expected_candidate, output)

TEST_P(SessionHandlerScenarioTest, TestImpl) {
  // Open the scenario file.
  string scenario_file = GetParam();
  const string &scenario_path =
      FileUtil::JoinPath(FLAGS_test_srcdir, scenario_file);
  ASSERT_TRUE(FileUtil::FileExists(scenario_path))
      << "Scenario file is not found: " << scenario_path;
  InputFileStream input_stream(scenario_path.c_str());

  // Set up session.
  ASSERT_TRUE(client_->CreateSession()) << "Client initialization is failed.";

  string line_text;
  int line_number = 0;
  vector<string> columns;
  while (getline(input_stream, line_text)) {
    ++line_number;
    SCOPED_TRACE(Util::StringPrintf("Scenario: %s [%s:%d]",
                                    line_text.c_str(),
                                    scenario_file.c_str(),
                                    line_number));

    if (line_text.empty() || line_text[0] == '#') {
      // Skip an empty or comment line.
      continue;
    }

    columns.clear();
    Util::SplitStringUsing(line_text, "\t", &columns);
    CHECK_GE(columns.size(), 1);
    const string &command = columns[0];
    // TODO(hidehiko): Refactor out about each command when the number of
    //   supported commands is increased.
    if (command == "RESET_CONTEXT") {
      ASSERT_EQ(1, columns.size());
      ResetContext();
    } else if (command == "SEND_KEYS") {
      ASSERT_EQ(2, columns.size());
      const string &keys = columns[1];
      KeyEvent key_event;
      for (size_t i = 0; i < keys.size(); ++i) {
        key_event.Clear();
        key_event.set_key_code(keys[i]);
        ASSERT_TRUE(client_->SendKey(key_event, last_output_.get()))
            << "Failed at " << i << "th key";
      }
    } else if (command == "SEND_KANA_KEYS") {
      ASSERT_LE(3, columns.size())
          << "SEND_KEY requires more than or equal to two columns "
          << line_text;
      const string &keys = columns[1];
      const string &kanas = columns[2];
      ASSERT_EQ(keys.size(), Util::CharsLen(kanas))
          << "1st and 2nd column must have the same number of characters.";
      KeyEvent key_event;
      for (size_t i = 0; i < keys.size(); ++i) {
        key_event.Clear();
        key_event.set_key_code(keys[i]);
        key_event.set_key_string(Util::SubString(kanas, i, 1));
        ASSERT_TRUE(client_->SendKey(key_event, last_output_.get()))
            << "Failed at " << i << "th " << line_text;
      }
    } else if (command == "SEND_KEY") {
      ASSERT_EQ(2, columns.size());
      KeyEvent key_event;
      ASSERT_TRUE(KeyParser::ParseKey(columns[1], &key_event));
      ASSERT_TRUE(client_->SendKey(key_event, last_output_.get()));
    } else if (command == "SEND_KEY_WITH_OPTION") {
      ASSERT_LE(3, columns.size());
      KeyEvent key_event;
      ASSERT_TRUE(KeyParser::ParseKey(columns[1], &key_event));
      Input option;
      for (size_t i = 2; i < columns.size(); ++i) {
        ASSERT_TRUE(ParseProtobufFromString(columns[i], &option));
      }
      ASSERT_TRUE(client_->SendKeyWithOption(key_event, option,
                                             last_output_.get()));
    } else if (command == "TEST_SEND_KEY") {
      ASSERT_EQ(2, columns.size());
      KeyEvent key_event;
      ASSERT_TRUE(KeyParser::ParseKey(columns[1], &key_event));
      ASSERT_TRUE(client_->TestSendKey(key_event, last_output_.get()));
    } else if (command == "TEST_SEND_KEY_WITH_OPTION") {
      ASSERT_LE(3, columns.size());
      KeyEvent key_event;
      ASSERT_TRUE(KeyParser::ParseKey(columns[1], &key_event));
      Input option;
      for (size_t i = 2; i < columns.size(); ++i) {
        ASSERT_TRUE(ParseProtobufFromString(columns[i], &option));
      }
      ASSERT_TRUE(client_->TestSendKeyWithOption(key_event, option,
        last_output_.get()));
    } else if (command == "SELECT_CANDIDATE") {
      ASSERT_EQ(2, columns.size());
      ASSERT_TRUE(client_->SelectCandidate(
          NumberUtil::SimpleAtoi(columns[1]), last_output_.get()));
    } else if (command == "SELECT_CANDIDATE_BY_VALUE") {
      ASSERT_EQ(2, columns.size());
      uint32 id;
      ASSERT_TRUE(GetCandidateIdByValue(columns[1], *last_output_, &id));
      ASSERT_TRUE(client_->SelectCandidate(id, last_output_.get()));
    } else if (command == "SUBMIT_CANDIDATE") {
      ASSERT_EQ(2, columns.size());
      ASSERT_TRUE(client_->SubmitCandidate(
          NumberUtil::SimpleAtoi(columns[1]), last_output_.get()));
    } else if (command == "SUBMIT_CANDIDATE_BY_VALUE") {
      ASSERT_EQ(2, columns.size());
      uint32 id;
      ASSERT_TRUE(GetCandidateIdByValue(columns[1], *last_output_, &id));
      ASSERT_TRUE(client_->SubmitCandidate(id, last_output_.get()));
    } else if (command == "UNDO_OR_REWIND") {
      ASSERT_TRUE(client_->UndoOrRewind(last_output_.get()));
    } else if (command == "SWITCH_INPUT_MODE") {
      ASSERT_EQ(2, columns.size());
      CompositionMode composition_mode;
      ASSERT_TRUE(CompositionMode_Parse(columns[1], &composition_mode))
          << "Unknown CompositionMode";
      ASSERT_TRUE(client_->SwitchInputMode(composition_mode));
    } else if (command == "SET_DEFAULT_REQUEST") {
      request_->CopyFrom(Request::default_instance());
      ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
    } else if (command == "SET_MOBILE_REQUEST") {
      RequestForUnitTest::FillMobileRequest(request_.get());
      ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
    } else if (command == "SET_REQUEST") {
      ASSERT_EQ(3, columns.size());
      ASSERT_TRUE(SetOrAddFieldValueFromString(columns[1], columns[2],
                                               request_.get()));
      ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
    } else if (command == "SET_CONFIG") {
      ASSERT_EQ(3, columns.size());
      ASSERT_TRUE(SetOrAddFieldValueFromString(columns[1], columns[2],
                                               config_.get()));
      ASSERT_TRUE(ConfigHandler::SetConfig(*config_));
      ASSERT_TRUE(client_->Reload());
    } else if (command == "SET_SELECTION_TEXT") {
      ASSERT_EQ(2, columns.size());
      client_->SetCallbackText(columns[1]);
    } else if (command == "UPDATE_MOBILE_KEYBOARD") {
      ASSERT_EQ(3, columns.size());
      Request::SpecialRomanjiTable special_romanji_table;
      ASSERT_TRUE(Request::SpecialRomanjiTable_Parse(
          columns[1], &special_romanji_table))
          << "Unknown SpecialRomanjiTable";
      Request::SpaceOnAlphanumeric space_on_alphanumeric;
      ASSERT_TRUE(Request::SpaceOnAlphanumeric_Parse(
          columns[2], &space_on_alphanumeric))
          << "Unknown SpaceOnAlphanumeric";
      request_->set_special_romanji_table(special_romanji_table);
      request_->set_space_on_alphanumeric(space_on_alphanumeric);
      ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
    } else if (command == "CLEAR_ALL") {
      ASSERT_EQ(1, columns.size());
      ClearAll();
    } else if (command == "CLEAR_USER_PREDICTION") {
      ASSERT_EQ(1, columns.size());
      ClearUserPrediction();
    } else if (command == "CLEAR_USAGE_STATS") {
      ASSERT_EQ(1, columns.size());
      ClearUsageStats();
    } else if (command == "EXPECT_CONSUMED") {
      ASSERT_EQ(2, columns.size());
      ASSERT_TRUE(last_output_->has_consumed());
      EXPECT_EQ(columns[1] == "true", last_output_->consumed());
    } else if (command == "EXPECT_PREEDIT") {
      // Concat preedit segments and assert.
      const string &expected_preedit = columns.size() == 1 ? "" : columns[1];
      string preedit_string;
      const mozc::commands::Preedit &preedit = last_output_->preedit();
      for (int i = 0; i < preedit.segment_size(); ++i) {
        preedit_string += preedit.segment(i).value();
      }
      EXPECT_EQ(expected_preedit, preedit_string)
          << "Expected preedit: " << expected_preedit << "\n"
          << "Actual preedit: " <<preedit.Utf8DebugString();
    } else if (command == "EXPECT_PREEDIT_IN_DETAIL") {
      ASSERT_LE(1, columns.size());
      const mozc::commands::Preedit &preedit = last_output_->preedit();
      ASSERT_EQ(columns.size() - 1, preedit.segment_size());
      for (int i = 0; i < preedit.segment_size(); ++i) {
        EXPECT_EQ(columns[i + 1], preedit.segment(i).value())
            << "Segment index = " << i;
      }
    } else if (command == "EXPECT_PREEDIT_CURSOR_POS") {
      // Concat preedit segments and assert.
      ASSERT_EQ(2, columns.size());
      const size_t &expected_pos = NumberUtil::SimpleAtoi(columns[1]);
      const mozc::commands::Preedit &preedit = last_output_->preedit();
      EXPECT_EQ(expected_pos, preedit.cursor()) << preedit.Utf8DebugString();
    } else if (command == "EXPECT_CANDIDATE") {
      ASSERT_EQ(3, columns.size());
      uint32 candidate_id = 0;
      const bool has_result = GetCandidateIdByValue(columns[2], *last_output_,
                                                    &candidate_id);
      EXPECT_TRUE(has_result) << columns[2] + " is not found\n"
                              << last_output_->candidates().Utf8DebugString();
      if (has_result) {
        EXPECT_EQ(NumberUtil::SimpleAtoi(columns[1]), candidate_id);
      }
    } else if (command == "EXPECT_RESULT") {
      if (columns.size() == 2 && !columns[1].empty()) {
        ASSERT_TRUE(last_output_->has_result());
        const mozc::commands::Result &result = last_output_->result();
        EXPECT_EQ(columns[1], result.value()) << result.Utf8DebugString();
      } else {
        EXPECT_FALSE(last_output_->has_result())
            << last_output_->result().Utf8DebugString();
      }
    } else if (command == "EXPECT_IN_ALL_CANDIDATE_WORDS") {
      ASSERT_EQ(2, columns.size());
      EXPECT_IN_ALL_CANDIDATE_WORDS(columns[1], *last_output_)
          << columns[1] << " is not found.\n"
          << last_output_->Utf8DebugString();
    } else if (command == "EXPECT_NOT_IN_ALL_CANDIDATE_WORDS") {
      ASSERT_EQ(2, columns.size());
      EXPECT_NOT_IN_ALL_CANDIDATE_WORDS(columns[1], *last_output_);
    } else if (command == "EXPECT_HAS_CANDIDATES") {
      ASSERT_TRUE(last_output_->has_candidates());
    } else if (command == "EXPECT_NO_CANDIDATES") {
      ASSERT_FALSE(last_output_->has_candidates());
    } else if (command == "EXPECT_SEGMENTS_SIZE") {
      ASSERT_EQ(2, columns.size());
      ASSERT_EQ(NumberUtil::SimpleAtoi(columns[1]),
                last_output_->preedit().segment_size());
    } else if (command == "EXPECT_HIGHLIGHTED_SEGMENT_INDEX") {
      ASSERT_EQ(2, columns.size());
      ASSERT_TRUE(last_output_->has_preedit());
      const mozc::commands::Preedit &preedit = last_output_->preedit();
      int index = -1;
      for (int i = 0; i < preedit.segment_size(); ++i) {
        if (preedit.segment(i).annotation() ==
            mozc::commands::Preedit::Segment::HIGHLIGHT) {
          index = i;
          break;
        }
      }
      ASSERT_EQ(NumberUtil::SimpleAtoi(columns[1]), index);
    } else if (command == "EXPECT_USAGE_STATS_COUNT") {
      ASSERT_EQ(3, columns.size());
      const uint32 expected_value = NumberUtil::SimpleAtoi(columns[2]);
      if (expected_value == 0) {
        EXPECT_STATS_NOT_EXIST(columns[1]);
      } else {
        EXPECT_COUNT_STATS(columns[1], expected_value);
      }
    } else if (command == "EXPECT_USAGE_STATS_INTEGER") {
      ASSERT_EQ(3, columns.size());
      EXPECT_INTEGER_STATS(columns[1], NumberUtil::SimpleAtoi(columns[2]));
    } else if (command == "EXPECT_USAGE_STATS_BOOLEAN") {
      ASSERT_EQ(3, columns.size());
      EXPECT_BOOLEAN_STATS(columns[1], columns[2] == "true");
    } else if (command == "EXPECT_USAGE_STATS_TIMING") {
      ASSERT_EQ(6, columns.size());
      const uint64 expected_total = NumberUtil::SimpleAtoi(columns[2]);
      const uint32 expected_num = NumberUtil::SimpleAtoi(columns[3]);
      const uint32 expected_min = NumberUtil::SimpleAtoi(columns[4]);
      const uint32 expected_max = NumberUtil::SimpleAtoi(columns[5]);
      if (expected_num == 0) {
        EXPECT_STATS_NOT_EXIST(columns[1]);
      } else {
        EXPECT_TIMING_STATS(columns[1], expected_total, expected_num,
                            expected_min, expected_max);
      }
    } else {
      FAIL() << "Unknown command";
    }
  }

  // Shut down.
  ASSERT_TRUE(client_->DeleteSession());
}

#undef EXPECT_IN_ALL_CANDIDATE_WORDS
#undef EXPECT_NOT_IN_ALL_CANDIDATE_WORDS

}  // namespace
