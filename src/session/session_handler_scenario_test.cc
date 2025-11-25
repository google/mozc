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

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/flags/reflection.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_replace.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/commands.pb.h"
#include "request/request_test_util.h"
#include "session/session_handler_test_util.h"
#include "session/session_handler_tool.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"

ABSL_DECLARE_FLAG(bool, use_history_rewriter);

namespace mozc {

using ::mozc::session::SessionHandlerInterpreter;
using ::mozc::session::testing::SessionHandlerTestBase;
using ::testing::TestParamInfo;
using ::testing::WithParamInterface;

class SessionHandlerScenarioTestBase : public SessionHandlerTestBase {
 protected:
  void SetUp() override {
    flagsaver_ = std::make_unique<absl::FlagSaver>();
    // Make sure to include history rewriter for testing.
    absl::SetFlag(&FLAGS_use_history_rewriter, true);
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
    flagsaver_.reset(nullptr);
  }

  std::unique_ptr<SessionHandlerInterpreter> handler_;
  std::unique_ptr<absl::FlagSaver> flagsaver_;
};

class SessionHandlerScenarioTest : public SessionHandlerScenarioTestBase,
                                   public WithParamInterface<const char*> {
 public:
  static std::string GetTestName(
      const ::testing::TestParamInfo<ParamType>& info) {
    return absl::StrReplaceAll(
        FileUtil::Basename(FileUtil::NormalizeDirectorySeparator(info.param)),
        {{".", "_"}});
  }
};

// Tests should be passed.
const char* kScenarioFileList[] = {
#define DATA_DIR "test/session/scenario/"
    DATA_DIR "auto_partial_suggestion.txt",
    DATA_DIR "b12751061_scenario.txt",
    DATA_DIR "b16123009_scenario.txt",
    DATA_DIR "b18112966_scenario.txt",
    DATA_DIR "b7132535_scenario.txt",
    DATA_DIR "b7548679_scenario.txt",
    DATA_DIR "b8690065_scenario.txt",
    DATA_DIR "b8703702_scenario.txt",
    DATA_DIR "change_request.txt",
    DATA_DIR "clear_user_prediction.txt",
    DATA_DIR "commit.txt",
    DATA_DIR "commit_by_space.txt",
    DATA_DIR "composing_alphanumeric.txt",
    DATA_DIR "composition_display_as.txt",
    DATA_DIR "composition_itchome.txt",
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
    DATA_DIR "handwriting.txt",
    DATA_DIR "insert_characters.txt",
    DATA_DIR "mobile_partial_variant_candidates.txt",
    DATA_DIR "on_off_cancel.txt",
    DATA_DIR "partial_suggestion.txt",
    DATA_DIR "pending_character.txt",
    DATA_DIR "predict_and_convert.txt",
    DATA_DIR "reconvert.txt",
    DATA_DIR "revert.txt",
    DATA_DIR "segment_focus.txt",
    DATA_DIR "segment_width.txt",
    DATA_DIR "suggest_after_zero_query.txt",
    DATA_DIR "switch_kana_type.txt",
    DATA_DIR "t13n_negative_number.txt",
    DATA_DIR "t13n_rewriter_twice.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_a.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_ka.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_sa.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_ta.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_na.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_ha.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_ma.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_ya.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_ra.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_wa.txt",
    DATA_DIR "toggle_flick_hiragana_preedit_symbol.txt",
    DATA_DIR "transliterations_f10.txt",
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
                         ::testing::ValuesIn(kScenarioFileList),
                         SessionHandlerScenarioTest::GetTestName);

void ParseLine(SessionHandlerInterpreter& handler, const std::string& line) {
  std::vector<std::string> args = handler.Parse(line);
  if (args.empty()) {
    return;
  }

  const absl::Status status = handler.Eval(args);
  if (status.ok()) {
    return;
  }
  FAIL() << line << "\n" << status.message();
}

TEST_P(SessionHandlerScenarioTest, TestImplBase) {
  // Open the scenario file.
  const absl::StatusOr<std::string> scenario_path =
      mozc::testing::GetSourceFile({"data", GetParam()});
  ASSERT_TRUE(scenario_path.ok()) << scenario_path.status();
  handler_->ClearAll();
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
      public WithParamInterface<std::tuple<const char*, commands::Request>> {
 public:
  static std::string GetTestName(
      const ::testing::TestParamInfo<ParamType>& info) {
    return absl::StrCat(
        info.index, "_",
        absl::StrReplaceAll(
            FileUtil::Basename(
                FileUtil::NormalizeDirectorySeparator(std::get<0>(info.param))),
            {{".", "_"}}));
  }
};

const char* kScenariosForExperimentParams[] = {
#define DATA_DIR "test/session/scenario/"
    DATA_DIR "mobile_apply_user_segment_history_rewriter.txt",
    DATA_DIR "mobile_delete_history.txt",
    DATA_DIR "mobile_preedit.txt",
    DATA_DIR "mobile_qwerty_transliteration_scenario.txt",
    DATA_DIR "mobile_revert_user_history_learning.txt",
    DATA_DIR "mobile_switch_composition_mode.txt",
    DATA_DIR "mobile_t13n_candidates.txt",
    DATA_DIR "mobile_zero_query.txt",
#undef DATA_DIR
};

commands::Request GetMobileRequest() {
  commands::Request request = commands::Request::default_instance();
  request_test_util::FillMobileRequest(&request);
  return request;
}

// Makes sure that the results are not changed by experiment params.
INSTANTIATE_TEST_SUITE_P(
    TestForExperimentParams, SessionHandlerScenarioTestForRequest,
    ::testing::Combine(::testing::ValuesIn(kScenariosForExperimentParams),
                       ::testing::Values(GetMobileRequest())),
    SessionHandlerScenarioTestForRequest::GetTestName);

TEST_P(SessionHandlerScenarioTestForRequest, TestImplBase) {
  // Open the scenario file.
  const absl::StatusOr<std::string> scenario_path =
      mozc::testing::GetSourceFile({"data", std::get<0>(GetParam())});
  ASSERT_TRUE(scenario_path.ok()) << scenario_path.status();
  handler_->ClearAll();
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
}  // namespace mozc
