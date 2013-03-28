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
#include "engine/engine_interface.h"
#include "engine/user_data_manager_interface.h"
#include "session/commands.pb.h"
#include "session/japanese_session_factory.h"
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
using mozc::protobuf::FieldDescriptor;
using mozc::protobuf::Message;
using mozc::protobuf::TextFormat;
using mozc::session::testing::JapaneseSessionHandlerTestBase;
using mozc::session::testing::TestSessionClient;
using testing::WithParamInterface;

class SessionHandlerScenarioTest : public JapaneseSessionHandlerTestBase,
                                   public WithParamInterface<const char *> {
 protected:
  virtual void SetUp() {
    // Note that singleton Config instance is backed up and restored
    // by JapaneseSessionHandlerTestBase's SetUp and TearDown methods.
    JapaneseSessionHandlerTestBase::SetUp();

    client_.reset(new TestSessionClient);
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
    EXPECT_TRUE(engine()->GetUserDataManager()->WaitForSyncerForTest());
  }

  void ClearUsageStats() {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  scoped_ptr<TestSessionClient> client_;
  scoped_ptr<Config> config_;
  scoped_ptr<Output> last_output_;
  scoped_ptr<Request> request_;
};

// Tests should be passed.
const char *kScenarioFileList[] = {
  "data/test/session/scenario/b7132535_scenario.txt",
  "data/test/session/scenario/b7321313_scenario.txt",
  "data/test/session/scenario/change_request.txt",
  "data/test/session/scenario/clear_user_prediction.txt",
  "data/test/session/scenario/conversion.txt",
  "data/test/session/scenario/conversion_with_history_segment.txt",
  "data/test/session/scenario/conversion_with_long_history_segments.txt",
#ifdef MOZC_ENABLE_HISTORY_DELETION
  "data/test/session/scenario/delete_history.txt",
#endif  // MOZC_ENABLE_HISTORY_DELETION
  "data/test/session/scenario/desktop_t13n_candidates.txt",
  "data/test/session/scenario/mobile_qwerty_transliteration_scenario.txt",
  "data/test/session/scenario/twelvekeys_switch_inputmode_scenario.txt",
  "data/test/session/scenario/twelvekeys_toggle_hiragana_preedit_scenario.txt",
  "data/test/session/scenario/mobile_t13n_candidates.txt",
  "data/test/session/scenario/partial_suggestion.txt",
  "data/test/session/scenario/auto_partial_suggestion.txt",
  "data/test/session/scenario/pending_character.txt",
  "data/test/session/scenario/composition_display_as.txt",
  "data/test/session/scenario/conversion_display_as.txt",
  "data/test/session/scenario/insert_characters.txt",
  "data/test/session/scenario/on_off_cancel.txt",
  "data/test/session/scenario/segment_focus.txt",
  "data/test/session/scenario/segment_width.txt",
};

INSTANTIATE_TEST_CASE_P(SessionHandlerScenarioParameters,
                        SessionHandlerScenarioTest,
                        ::testing::ValuesIn(kScenarioFileList));

const char *kUsageStatsScenarioFileList[] = {
  "data/test/session/scenario/usage_stats/composition.txt",
  "data/test/session/scenario/usage_stats/conversion.txt",
  "data/test/session/scenario/usage_stats/prediction.txt",
  "data/test/session/scenario/usage_stats/suggestion.txt",
  "data/test/session/scenario/usage_stats/select_prediction.txt",
  "data/test/session/scenario/usage_stats/select_minor_conversion.txt",
  "data/test/session/scenario/usage_stats/select_minor_prediction.txt",
  "data/test/session/scenario/usage_stats/mouse_select_from_suggestion.txt",
  "data/test/session/scenario/usage_stats/continue_input.txt",
  "data/test/session/scenario/usage_stats/continuous_input.txt",
  "data/test/session/scenario/usage_stats/select_t13n_by_key.txt",
#ifndef OS_LINUX
  // This test requires cascading window.
  // TODO(hsumita): Removes this ifndef block.
  "data/test/session/scenario/usage_stats/select_t13n_on_cascading_window.txt",
#endif  // OS_LINUX
  "data/test/session/scenario/usage_stats/switch_kana_type.txt",
  "data/test/session/scenario/usage_stats/multiple_segments.txt",
  "data/test/session/scenario/usage_stats/"
      "select_candidates_in_multiple_segments.txt",
  "data/test/session/scenario/usage_stats/"
      "select_candidates_in_multiple_segments_and_expand_segment.txt",
  "data/test/session/scenario/usage_stats/backspace_after_commit.txt",
  "data/test/session/scenario/usage_stats/"
      "backspace_after_commit_after_backspace.txt",
  "data/test/session/scenario/usage_stats/multiple_backspace_after_commit.txt",
  "data/test/session/scenario/usage_stats/zero_query_suggestion.txt",
  "data/test/session/scenario/usage_stats/auto_partial_suggestion.txt",
  "data/test/session/scenario/usage_stats/insert_space.txt",
  "data/test/session/scenario/usage_stats/numpad_in_direct_input_mode.txt",
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
      ASSERT_EQ(columns.size(), 2);
      const string &keys = columns[1];
      KeyEvent key_event;
      for (size_t i = 0; i < keys.size(); ++i) {
        key_event.Clear();
        key_event.set_key_code(keys[i]);
        ASSERT_TRUE(client_->SendKey(key_event, last_output_.get()))
            << "Failed at " << i << "th key";
      }
    } else if (command == "SEND_KANA_KEYS") {
      ASSERT_GE(columns.size(), 3)
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
      ASSERT_EQ(columns.size(), 2);
      KeyEvent key_event;
      ASSERT_TRUE(KeyParser::ParseKey(columns[1], &key_event));
      ASSERT_TRUE(client_->SendKey(key_event, last_output_.get()));
    } else if (command == "SEND_KEY_WITH_OPTION") {
      ASSERT_GE(columns.size(), 3);
      KeyEvent key_event;
      ASSERT_TRUE(KeyParser::ParseKey(columns[1], &key_event));
      Input option;
      for (size_t i = 2; i < columns.size(); ++i) {
        ASSERT_TRUE(ParseProtobufFromString(columns[i], &option));
      }
      ASSERT_TRUE(client_->SendKeyWithOption(key_event, option,
                                             last_output_.get()));
    } else if (command == "SELECT_CANDIDATE") {
      ASSERT_EQ(columns.size(), 2);
      ASSERT_TRUE(client_->SelectCandidate(
          NumberUtil::SimpleAtoi(columns[1]), last_output_.get()));
    } else if (command == "SELECT_CANDIDATE_BY_VALUE") {
      ASSERT_EQ(columns.size(), 2);
      uint32 id;
      ASSERT_TRUE(GetCandidateIdByValue(columns[1], *last_output_, &id));
      ASSERT_TRUE(client_->SelectCandidate(id, last_output_.get()));
    } else if (command == "SUBMIT_CANDIDATE") {
      ASSERT_EQ(columns.size(), 2);
      ASSERT_TRUE(client_->SubmitCandidate(
          NumberUtil::SimpleAtoi(columns[1]), last_output_.get()));
    } else if (command == "SUBMIT_CANDIDATE_BY_VALUE") {
      ASSERT_EQ(columns.size(), 2);
      uint32 id;
      ASSERT_TRUE(GetCandidateIdByValue(columns[1], *last_output_, &id));
      ASSERT_TRUE(client_->SubmitCandidate(id, last_output_.get()));
    } else if (command == "UNDO_OR_REWIND") {
      ASSERT_TRUE(client_->UndoOrRewind(last_output_.get()));
    } else if (command == "SWITCH_INPUT_MODE") {
      ASSERT_EQ(columns.size(), 2);
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
    } else if (command == "SET_CONFIG") {
      ASSERT_EQ(columns.size(), 3);
      ASSERT_TRUE(SetOrAddFieldValueFromString(columns[1], columns[2],
                                               config_.get()));
      ASSERT_TRUE(ConfigHandler::SetConfig(*config_));
      ASSERT_TRUE(client_->Reload());
    } else if (command == "UPDATE_MOBILE_KEYBOARD") {
      ASSERT_EQ(columns.size(), 3);
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
      ASSERT_EQ(columns.size(), 2);
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
      EXPECT_EQ(expected_preedit, preedit_string) << preedit.Utf8DebugString();
    } else if (command == "EXPECT_PREEDIT_IN_DETAIL") {
      ASSERT_GE(columns.size(), 1);
      const mozc::commands::Preedit &preedit = last_output_->preedit();
      ASSERT_EQ(columns.size() - 1, preedit.segment_size());
      for (int i = 0; i < preedit.segment_size(); ++i) {
        EXPECT_EQ(columns[i + 1], preedit.segment(i).value())
            << "Segment index = " << i;
      }
    } else if (command == "EXPECT_PREEDIT_CURSOR_POS") {
      // Concat preedit segments and assert.
      ASSERT_EQ(columns.size(), 2);
      const size_t &expected_pos = NumberUtil::SimpleAtoi(columns[1]);
      const mozc::commands::Preedit &preedit = last_output_->preedit();
      EXPECT_EQ(expected_pos, preedit.cursor()) << preedit.Utf8DebugString();
    } else if (command == "EXPECT_CANDIDATE") {
      ASSERT_EQ(columns.size(), 3);
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
      ASSERT_EQ(columns.size(), 2);
      EXPECT_IN_ALL_CANDIDATE_WORDS(columns[1], *last_output_)
          << columns[1] << " is not found.\n"
          << last_output_->Utf8DebugString();
    } else if (command == "EXPECT_NOT_IN_ALL_CANDIDATE_WORDS") {
      ASSERT_EQ(columns.size(), 2);
      EXPECT_NOT_IN_ALL_CANDIDATE_WORDS(columns[1], *last_output_);
    } else if (command == "EXPECT_HAS_CANDIDATES") {
      ASSERT_TRUE(last_output_->has_candidates());
    } else if (command == "EXPECT_NO_CANDIDATES") {
      ASSERT_FALSE(last_output_->has_candidates());
    } else if (command == "EXPECT_SEGMENTS_SIZE") {
      ASSERT_EQ(columns.size(), 2);
      ASSERT_EQ(NumberUtil::SimpleAtoi(columns[1]),
                last_output_->preedit().segment_size());
    } else if (command == "EXPECT_HIGHLIGHTED_SEGMENT_INDEX") {
      ASSERT_EQ(columns.size(), 2);
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
      ASSERT_EQ(columns.size(), 3);
      const uint32 expected_value = NumberUtil::SimpleAtoi(columns[2]);
      if (expected_value == 0) {
        EXPECT_STATS_NOT_EXIST(columns[1]);
      } else {
        EXPECT_COUNT_STATS(columns[1], expected_value);
      }
    } else if (command == "EXPECT_USAGE_STATS_INTEGER") {
      ASSERT_EQ(columns.size(), 3);
      EXPECT_INTEGER_STATS(columns[1], NumberUtil::SimpleAtoi(columns[2]));
    } else if (command == "EXPECT_USAGE_STATS_BOOLEAN") {
      ASSERT_EQ(columns.size(), 3);
      EXPECT_BOOLEAN_STATS(columns[1], columns[2] == "true");
    } else if (command == "EXPECT_USAGE_STATS_TIMING") {
      ASSERT_EQ(columns.size(), 6);
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
