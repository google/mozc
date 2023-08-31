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

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/number_util.h"
#include "base/protobuf/message.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "session/request_test_util.h"
#include "session/session_handler_test_util.h"
#include "session/session_handler_tool.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace mozc {

using ::mozc::commands::CandidateList;
using ::mozc::commands::CandidateWord;
using ::mozc::commands::Output;
using ::mozc::session::SessionHandlerInterpreter;
using ::mozc::session::testing::SessionHandlerTestBase;
using ::testing::WithParamInterface;

class SessionHandlerScenarioTestBase : public SessionHandlerTestBase {
 protected:
  void SetUp() override {
    // Note that singleton Config instance is backed up and restored
    // by SessionHandlerTestBase's SetUp and TearDown methods.
    SessionHandlerTestBase::SetUp();

    std::unique_ptr<EngineInterface> engine =
        MockDataEngineFactory::Create().value();
    handler_ = std::make_unique<SessionHandlerInterpreter>(std::move(engine));
  }

  void TearDown() override {
    handler_.reset();
    SessionHandlerTestBase::TearDown();
  }

  std::unique_ptr<SessionHandlerInterpreter> handler_;
};

class SessionHandlerScenarioTest : public SessionHandlerScenarioTestBase,
                                   public WithParamInterface<const char *> {};

// Tests should be passed.
const char *kScenarioFileList[] = {
#define DATA_DIR "test/session/scenario/"
    DATA_DIR "auto_partial_suggestion.txt",
    DATA_DIR "b12751061_scenario.txt",
    DATA_DIR "b16123009_scenario.txt",
    DATA_DIR "b18112966_scenario.txt",
    DATA_DIR "b7132535_scenario.txt",
    DATA_DIR "b7321313_scenario.txt",
    DATA_DIR "b7548679_scenario.txt",
    DATA_DIR "b8690065_scenario.txt",
    DATA_DIR "b8703702_scenario.txt",
    DATA_DIR "change_request.txt",
    DATA_DIR "clear_user_prediction.txt",
    DATA_DIR "commit.txt",
    DATA_DIR "commit_by_space.txt",
    DATA_DIR "composing_alphanumeric.txt",
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
    DATA_DIR "description.txt",
    DATA_DIR "desktop_t13n_candidates.txt",
    DATA_DIR "domain_suggestion.txt",
#if !defined(__APPLE__)
    // "InputModeX" commands are not supported on Mac.
    // Mac: We do not have the way to change the mode indicator from IME.
    DATA_DIR "input_mode.txt",
#endif  // !__APPLE__
    DATA_DIR "insert_characters.txt",
    DATA_DIR "kana_modifier_insensitive_conversion.txt",
    DATA_DIR "mobile_partial_variant_candidates.txt",
    DATA_DIR "mobile_qwerty_transliteration_scenario.txt",
    DATA_DIR "mobile_revert_user_history_learning.txt",
    DATA_DIR "mobile_t13n_candidates.txt",
    DATA_DIR "on_off_cancel.txt",
    DATA_DIR "partial_suggestion.txt",
    DATA_DIR "pending_character.txt",
    DATA_DIR "predict_and_convert.txt",
    DATA_DIR "reconvert.txt",
    DATA_DIR "revert.txt",
    DATA_DIR "segment_focus.txt",
    DATA_DIR "segment_width.txt",
    DATA_DIR "suggest_after_zero_query.txt",
    DATA_DIR "twelvekeys_switch_inputmode_scenario.txt",
    DATA_DIR "twelvekeys_toggle_flick_alphabet_scenario.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_a.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_ka.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_sa.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_ta.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_na.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_ha.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_ma.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_ya.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_ra.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_wa.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario_symbol.txt",
    DATA_DIR "undo.txt",
    DATA_DIR "undo_partial_commit.txt",
    DATA_DIR "zero_query_suggestion.txt",
#undef DATA_DIR
};

INSTANTIATE_TEST_SUITE_P(SessionHandlerScenarioParameters,
                         SessionHandlerScenarioTest,
                         ::testing::ValuesIn(kScenarioFileList));

const char *kUsageStatsScenarioFileList[] = {
#define DATA_DIR "test/session/scenario/usage_stats/"
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
#ifndef __linux__
    // This test requires cascading window.
    // TODO(hsumita): Removes this ifndef block.
    DATA_DIR "select_t13n_on_cascading_window.txt",
#endif  // !__linux__
    DATA_DIR "suggestion.txt",
    DATA_DIR "switch_kana_type.txt",
    DATA_DIR "zero_query_suggestion.txt",
#undef DATA_DIR
};
INSTANTIATE_TEST_SUITE_P(SessionHandlerUsageStatsScenarioParameters,
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
INSTANTIATE_TEST_SUITE_P(DISABLED_SessionHandlerScenarioParameters,
                         SessionHandlerScenarioTest,
                         ::testing::ValuesIn(kFailedScenarioFileList));

bool GetCandidateIdByValue(const absl::string_view value, const Output &output,
                           uint32_t *id) {
  if (!output.has_all_candidate_words()) {
    return false;
  }

  const CandidateList &all_candidate_words = output.all_candidate_words();
  for (int i = 0; i < all_candidate_words.candidates_size(); ++i) {
    const CandidateWord &candidate_word = all_candidate_words.candidates(i);
    if (candidate_word.has_value() && candidate_word.value() == value) {
      *id = candidate_word.id();
      return true;
    }
  }
  return false;
}

bool IsInAllCandidateWords(const absl::string_view expected_candidate,
                           const Output &output) {
  uint32_t tmp;
  return GetCandidateIdByValue(expected_candidate, output, &tmp);
}

::testing::AssertionResult IsInAllCandidateWordsWithFormat(
    const char *expected_candidate_string, const char *output_string,
    const std::string &expected_candidate, const Output &output) {
  if (!IsInAllCandidateWords(expected_candidate, output)) {
    return ::testing::AssertionFailure()
           << expected_candidate_string << "(" << expected_candidate << ")"
           << " is not found in " << output_string << "\n"
           << protobuf::Utf8Format(output);
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult IsNotInAllCandidateWordsWithFormat(
    const char *expected_candidate_string, const char *output_string,
    const absl::string_view expected_candidate, const Output &output) {
  if (IsInAllCandidateWords(expected_candidate, output)) {
    return ::testing::AssertionFailure()
           << expected_candidate_string << "(" << expected_candidate << ")"
           << " is found in " << output_string << "\n"
           << protobuf::Utf8Format(output);
  }
  return ::testing::AssertionSuccess();
}

#define EXPECT_IN_ALL_CANDIDATE_WORDS(expected_candidate, output)          \
  EXPECT_PRED_FORMAT2(IsInAllCandidateWordsWithFormat, expected_candidate, \
                      output)
#define EXPECT_NOT_IN_ALL_CANDIDATE_WORDS(expected_candidate, output)         \
  EXPECT_PRED_FORMAT2(IsNotInAllCandidateWordsWithFormat, expected_candidate, \
                      output)

void ParseLine(SessionHandlerInterpreter &handler, const std::string &line) {
  std::vector<std::string> args = handler.Parse(line);
  if (args.empty()) {
    return;
  }

  const absl::Status status = handler.Eval(args);
  if (status.ok()) {
    return;
  }
  if (status.code() != absl::StatusCode::kUnimplemented) {
    FAIL() << status.message();
  }

  const std::string &command = args[0];
  const Output &output = handler.LastOutput();

  if (command == "EXPECT_CONSUMED") {
    ASSERT_EQ(args.size(), 2);
    ASSERT_TRUE(output.has_consumed());
    EXPECT_EQ(output.consumed(), args[1] == "true");
  } else if (command == "EXPECT_PREEDIT") {
    // Concat preedit segments and assert.
    const std::string &expected_preedit = args.size() == 1 ? "" : args[1];
    std::string preedit_string;
    const mozc::commands::Preedit &preedit = output.preedit();
    for (int i = 0; i < preedit.segment_size(); ++i) {
      preedit_string += preedit.segment(i).value();
    }
    EXPECT_EQ(preedit_string, expected_preedit)
        << "Expected preedit: " << expected_preedit << "\n"
        << "Actual preedit: " << protobuf::Utf8Format(preedit);
  } else if (command == "EXPECT_PREEDIT_IN_DETAIL") {
    ASSERT_LE(1, args.size());
    const mozc::commands::Preedit &preedit = output.preedit();
    ASSERT_EQ(preedit.segment_size(), args.size() - 1);
    for (int i = 0; i < preedit.segment_size(); ++i) {
      EXPECT_EQ(preedit.segment(i).value(), args[i + 1])
          << "Segment index = " << i;
    }
  } else if (command == "EXPECT_PREEDIT_CURSOR_POS") {
    // Concat preedit segments and assert.
    ASSERT_EQ(args.size(), 2);
    const size_t expected_pos = NumberUtil::SimpleAtoi(args[1]);
    const mozc::commands::Preedit &preedit = output.preedit();
    EXPECT_EQ(preedit.cursor(), expected_pos) << protobuf::Utf8Format(preedit);
  } else if (command == "EXPECT_CANDIDATE") {
    ASSERT_EQ(args.size(), 3);
    uint32_t candidate_id = 0;
    const bool has_result =
        GetCandidateIdByValue(args[2], output, &candidate_id);
    EXPECT_TRUE(has_result) << args[2] + " is not found\n"
                            << protobuf::Utf8Format(output.candidates());
    if (has_result) {
      EXPECT_EQ(candidate_id, NumberUtil::SimpleAtoi(args[1]));
    }
  } else if (command == "EXPECT_CANDIDATE_DESCRIPTION") {
    ASSERT_EQ(args.size(), 3);
    const CandidateWord &cand = handler.GetCandidateByValue(args[1]);
    const bool has_cand = !cand.value().empty();
    EXPECT_TRUE(has_cand) << args[1] + " is not found\n"
                          << protobuf::Utf8Format(output.candidates());
    if (has_cand) {
      EXPECT_EQ(cand.annotation().description(), args[2])
          << protobuf::Utf8Format(cand);
    }
  } else if (command == "EXPECT_RESULT") {
    if (args.size() == 2 && !args[1].empty()) {
      ASSERT_TRUE(output.has_result());
      const mozc::commands::Result &result = output.result();
      EXPECT_EQ(result.value(), args[1]) << protobuf::Utf8Format(result);
    } else {
      EXPECT_FALSE(output.has_result()) << protobuf::Utf8Format(output.result());
    }
  } else if (command == "EXPECT_IN_ALL_CANDIDATE_WORDS") {
    ASSERT_EQ(args.size(), 2);
    EXPECT_IN_ALL_CANDIDATE_WORDS(args[1], output)
        << args[1] << " is not found.\n"
        << protobuf::Utf8Format(output);
  } else if (command == "EXPECT_NOT_IN_ALL_CANDIDATE_WORDS") {
    ASSERT_EQ(args.size(), 2);
    EXPECT_NOT_IN_ALL_CANDIDATE_WORDS(args[1], output);
  } else if (command == "EXPECT_HAS_CANDIDATES") {
    if (args.size() == 2 && !args[1].empty()) {
      ASSERT_TRUE(output.has_candidates());
      ASSERT_GT(output.candidates().size(), NumberUtil::SimpleAtoi(args[1]))
          << protobuf::Utf8Format(output);
    } else {
      ASSERT_TRUE(output.has_candidates());
    }
  } else if (command == "EXPECT_NO_CANDIDATES") {
    ASSERT_FALSE(output.has_candidates());
  } else if (command == "EXPECT_SEGMENTS_SIZE") {
    ASSERT_EQ(args.size(), 2);
    ASSERT_EQ(output.preedit().segment_size(), NumberUtil::SimpleAtoi(args[1]));
  } else if (command == "EXPECT_HIGHLIGHTED_SEGMENT_INDEX") {
    ASSERT_EQ(args.size(), 2);
    ASSERT_TRUE(output.has_preedit());
    const mozc::commands::Preedit &preedit = output.preedit();
    int index = -1;
    for (int i = 0; i < preedit.segment_size(); ++i) {
      if (preedit.segment(i).annotation() ==
          mozc::commands::Preedit::Segment::HIGHLIGHT) {
        index = i;
        break;
      }
    }
    ASSERT_EQ(index, NumberUtil::SimpleAtoi(args[1]));
  } else if (command == "EXPECT_USAGE_STATS_COUNT") {
    ASSERT_EQ(args.size(), 3);
    const uint32_t expected_value = NumberUtil::SimpleAtoi(args[2]);
    if (expected_value == 0) {
      EXPECT_STATS_NOT_EXIST(args[1]);
    } else {
      EXPECT_COUNT_STATS(args[1], expected_value);
    }
  } else if (command == "EXPECT_USAGE_STATS_INTEGER") {
    ASSERT_EQ(args.size(), 3);
    EXPECT_INTEGER_STATS(args[1], NumberUtil::SimpleAtoi(args[2]));
  } else if (command == "EXPECT_USAGE_STATS_BOOLEAN") {
    ASSERT_EQ(args.size(), 3);
    EXPECT_BOOLEAN_STATS(args[1], args[2] == "true");
  } else if (command == "EXPECT_USAGE_STATS_TIMING") {
    ASSERT_EQ(args.size(), 6);
    const uint64_t expected_total = NumberUtil::SimpleAtoi(args[2]);
    const uint32_t expected_num = NumberUtil::SimpleAtoi(args[3]);
    const uint32_t expected_min = NumberUtil::SimpleAtoi(args[4]);
    const uint32_t expected_max = NumberUtil::SimpleAtoi(args[5]);
    if (expected_num == 0) {
      EXPECT_STATS_NOT_EXIST(args[1]);
    } else {
      EXPECT_TIMING_STATS(args[1], expected_total, expected_num, expected_min,
                          expected_max);
    }

  } else {
    FAIL() << "Unknown command";
  }
}

TEST_P(SessionHandlerScenarioTest, TestImplBase) {
  // Open the scenario file.
  const absl::StatusOr<std::string> scenario_path =
      mozc::testing::GetSourceFile({MOZC_DICT_DIR_COMPONENTS, GetParam()});
  ASSERT_TRUE(scenario_path.ok()) << scenario_path.status();
  LOG(INFO) << "Testing " << FileUtil::Basename(*scenario_path);
  InputFileStream input_stream(*scenario_path);

  std::string line_text;
  int line_number = 0;
  while (std::getline(input_stream, line_text)) {
    ++line_number;
    SCOPED_TRACE(absl::StrFormat("Scenario: %s [%s:%d]", line_text,
                                 *scenario_path, line_number));
    ParseLine(*handler_, line_text);
  }
}

class SessionHandlerScenarioTestForRequest
    : public SessionHandlerScenarioTestBase,
      public WithParamInterface<std::tuple<const char *, commands::Request>> {};

const char *kScenariosForExperimentParams[] = {
#define DATA_DIR "test/session/scenario/"
    DATA_DIR "mobile_zero_query.txt",
    DATA_DIR "mobile_preedit.txt",
#undef DATA_DIR
};

commands::Request GetMobileRequest() {
  commands::Request request = commands::Request::default_instance();
  commands::RequestForUnitTest::FillMobileRequest(&request);
  return request;
}

// Makes sure that the results are not changed by experiment params.
INSTANTIATE_TEST_SUITE_P(
    TestForExperimentParams, SessionHandlerScenarioTestForRequest,
    ::testing::Combine(::testing::ValuesIn(kScenariosForExperimentParams),
                       ::testing::Values(
                           GetMobileRequest(),
                           []() {
                             auto request = GetMobileRequest();
                             request.mutable_decoder_experiment_params()
                                 ->set_enable_new_spatial_scoring(true);
                             return request;
                           }(),
                           []() {
                             auto request = GetMobileRequest();
                             request.mutable_decoder_experiment_params()
                                 ->set_enable_single_kanji_prediction(true);
                             return request;
                           }(),
                           []() {
                             auto request = GetMobileRequest();
                             request.mutable_decoder_experiment_params()
                                 ->set_cancel_content_word_suffix_penalty(true);
                             return request;
                           }(),
                           []() {
                             auto request = GetMobileRequest();
                             request.mutable_decoder_experiment_params()
                                 ->set_enable_number_style_learning(true);
                             return request;
                           }())));

TEST_P(SessionHandlerScenarioTestForRequest, TestImplBase) {
  // Open the scenario file.
  const absl::StatusOr<std::string> scenario_path =
      mozc::testing::GetSourceFile(
          {MOZC_DICT_DIR_COMPONENTS, std::get<0>(GetParam())});
  ASSERT_TRUE(scenario_path.ok()) << scenario_path.status();

  handler_->SetRequest(std::get<1>(GetParam()));

  LOG(INFO) << "Testing " << FileUtil::Basename(*scenario_path);
  InputFileStream input_stream(*scenario_path);
  std::string line_text;
  int line_number = 0;
  while (std::getline(input_stream, line_text)) {
    ++line_number;
    SCOPED_TRACE(absl::StrFormat("Scenario: %s [%s:%d]", line_text,
                                 *scenario_path, line_number));
    ParseLine(*handler_, line_text);
  }
}

#undef EXPECT_IN_ALL_CANDIDATE_WORDS
#undef EXPECT_NOT_IN_ALL_CANDIDATE_WORDS

}  // namespace mozc
