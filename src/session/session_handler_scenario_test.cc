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

#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/number_util.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/protobuf/text_format.h"
#include "base/util.h"
#include "converter/converter_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "prediction/user_history_predictor.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/session_handler_test_util.h"
#include "session/session_handler_tool.h"
#include "storage/registry.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace mozc {

using ::mozc::commands::CandidateList;
using ::mozc::commands::CandidateWord;
using ::mozc::commands::Output;
using ::mozc::session::SessionHandlerInterpreter;
using ::mozc::session::testing::SessionHandlerTestBase;
using ::testing::WithParamInterface;

class SessionHandlerScenarioTest : public SessionHandlerTestBase,
                                   public WithParamInterface<const char *> {
 protected:
  void SetUp() override {
    // Note that singleton Config instance is backed up and restored
    // by SessionHandlerTestBase's SetUp and TearDown methods.
    SessionHandlerTestBase::SetUp();

    std::unique_ptr<EngineInterface> engine =
        MockDataEngineFactory::Create().value();
    handler_ = absl::make_unique<SessionHandlerInterpreter>(std::move(engine));
  }

  void TearDown() override {
    handler_.reset();
    SessionHandlerTestBase::TearDown();
  }

  std::unique_ptr<SessionHandlerInterpreter> handler_;
};

// Tests should be passed.
const char *kScenarioFileList[] = {
#define DATA_DIR "data/test/session/scenario/"
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
    DATA_DIR "twelvekeys_toggle_flick_alphabet_scenario.txt",
    DATA_DIR "twelvekeys_toggle_hiragana_preedit_scenario.txt",
    DATA_DIR "undo.txt",
    DATA_DIR "zero_query_suggestion.txt",
#undef DATA_DIR
};

INSTANTIATE_TEST_SUITE_P(SessionHandlerScenarioParameters,
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
#if !defined(OS_LINUX) && !defined(OS_ANDROID)
    // This test requires cascading window.
    // TODO(hsumita): Removes this ifndef block.
    DATA_DIR "select_t13n_on_cascading_window.txt",
#endif  // !OS_LINUX && !OS_ANDROID
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
           << output.Utf8DebugString();
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
           << output.Utf8DebugString();
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
    EXPECT_TRUE(false) << status.message();
  }

  const std::string &command = args[0];
  const Output &output = handler.LastOutput();

  if (command == "EXPECT_CONSUMED") {
    ASSERT_EQ(2, args.size());
    ASSERT_TRUE(output.has_consumed());
    EXPECT_EQ(args[1] == "true", output.consumed());
  } else if (command == "EXPECT_PREEDIT") {
    // Concat preedit segments and assert.
    const std::string &expected_preedit = args.size() == 1 ? "" : args[1];
    std::string preedit_string;
    const mozc::commands::Preedit &preedit = output.preedit();
    for (int i = 0; i < preedit.segment_size(); ++i) {
      preedit_string += preedit.segment(i).value();
    }
    EXPECT_EQ(expected_preedit, preedit_string)
        << "Expected preedit: " << expected_preedit << "\n"
        << "Actual preedit: " << preedit.Utf8DebugString();
  } else if (command == "EXPECT_PREEDIT_IN_DETAIL") {
    ASSERT_LE(1, args.size());
    const mozc::commands::Preedit &preedit = output.preedit();
    ASSERT_EQ(args.size() - 1, preedit.segment_size());
    for (int i = 0; i < preedit.segment_size(); ++i) {
      EXPECT_EQ(args[i + 1], preedit.segment(i).value())
          << "Segment index = " << i;
    }
  } else if (command == "EXPECT_PREEDIT_CURSOR_POS") {
    // Concat preedit segments and assert.
    ASSERT_EQ(2, args.size());
    const size_t expected_pos = NumberUtil::SimpleAtoi(args[1]);
    const mozc::commands::Preedit &preedit = output.preedit();
    EXPECT_EQ(expected_pos, preedit.cursor()) << preedit.Utf8DebugString();
  } else if (command == "EXPECT_CANDIDATE") {
    ASSERT_EQ(3, args.size());
    uint32_t candidate_id = 0;
    const bool has_result =
        GetCandidateIdByValue(args[2], output, &candidate_id);
    EXPECT_TRUE(has_result) << args[2] + " is not found\n"
                            << output.candidates().Utf8DebugString();
    if (has_result) {
      EXPECT_EQ(NumberUtil::SimpleAtoi(args[1]), candidate_id);
    }
  } else if (command == "EXPECT_RESULT") {
    if (args.size() == 2 && !args[1].empty()) {
      ASSERT_TRUE(output.has_result());
      const mozc::commands::Result &result = output.result();
      EXPECT_EQ(args[1], result.value()) << result.Utf8DebugString();
    } else {
      EXPECT_FALSE(output.has_result()) << output.result().Utf8DebugString();
    }
  } else if (command == "EXPECT_IN_ALL_CANDIDATE_WORDS") {
    ASSERT_EQ(2, args.size());
    EXPECT_IN_ALL_CANDIDATE_WORDS(args[1], output)
        << args[1] << " is not found.\n"
        << output.Utf8DebugString();
  } else if (command == "EXPECT_NOT_IN_ALL_CANDIDATE_WORDS") {
    ASSERT_EQ(2, args.size());
    EXPECT_NOT_IN_ALL_CANDIDATE_WORDS(args[1], output);
  } else if (command == "EXPECT_HAS_CANDIDATES") {
    ASSERT_TRUE(output.has_candidates());
  } else if (command == "EXPECT_NO_CANDIDATES") {
    ASSERT_FALSE(output.has_candidates());
  } else if (command == "EXPECT_SEGMENTS_SIZE") {
    ASSERT_EQ(2, args.size());
    ASSERT_EQ(NumberUtil::SimpleAtoi(args[1]), output.preedit().segment_size());
  } else if (command == "EXPECT_HIGHLIGHTED_SEGMENT_INDEX") {
    ASSERT_EQ(2, args.size());
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
    ASSERT_EQ(NumberUtil::SimpleAtoi(args[1]), index);
  } else if (command == "EXPECT_USAGE_STATS_COUNT") {
    ASSERT_EQ(3, args.size());
    const uint32_t expected_value = NumberUtil::SimpleAtoi(args[2]);
    if (expected_value == 0) {
      EXPECT_STATS_NOT_EXIST(args[1]);
    } else {
      EXPECT_COUNT_STATS(args[1], expected_value);
    }
  } else if (command == "EXPECT_USAGE_STATS_INTEGER") {
    ASSERT_EQ(3, args.size());
    EXPECT_INTEGER_STATS(args[1], NumberUtil::SimpleAtoi(args[2]));
  } else if (command == "EXPECT_USAGE_STATS_BOOLEAN") {
    ASSERT_EQ(3, args.size());
    EXPECT_BOOLEAN_STATS(args[1], args[2] == "true");
  } else if (command == "EXPECT_USAGE_STATS_TIMING") {
    ASSERT_EQ(6, args.size());
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
      mozc::testing::GetSourceFile({GetParam()});
  ASSERT_TRUE(scenario_path.ok()) << scenario_path.status();
  LOG(INFO) << "Testing " << FileUtil::Basename(*scenario_path);
  InputFileStream input_stream(scenario_path->c_str());

  std::string line_text;
  int line_number = 0;
  while (std::getline(input_stream, line_text)) {
    ++line_number;
    SCOPED_TRACE(absl::StrFormat("Scenario: %s [%s:%d]", line_text.c_str(),
                                 scenario_path->c_str(), line_number));
    ParseLine(*handler_, line_text);
  }
}

#undef EXPECT_IN_ALL_CANDIDATE_WORDS
#undef EXPECT_NOT_IN_ALL_CANDIDATE_WORDS

}  // namespace mozc
