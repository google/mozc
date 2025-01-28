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

#include "session/session_usage_stats_util.h"

#include "absl/strings/string_view.h"
#include "composer/key_parser.h"
#include "protocol/commands.pb.h"
#include "testing/gunit.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

using mozc::commands::Context;
using mozc::commands::Input;
using mozc::commands::KeyEvent;
using mozc::commands::Output;
using mozc::commands::SessionCommand;

namespace mozc {
namespace session {
namespace {

class SessionUsageStatsUtilTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  void TearDown() override {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(SessionUsageStatsUtilTest, AddSendKeyInputStats) {
  Input input;
  input.set_type(Input::SEND_KEY);

  KeyParser::ParseKey("a", input.mutable_key());
  SessionUsageStatsUtil::AddSendKeyInputStats(input);
  EXPECT_COUNT_STATS("ASCIITyping", 1);
  EXPECT_COUNT_STATS("NonASCIITyping", 0);

  KeyParser::ParseKey("Space", input.mutable_key());
  SessionUsageStatsUtil::AddSendKeyInputStats(input);
  EXPECT_COUNT_STATS("ASCIITyping", 1);
  EXPECT_COUNT_STATS("NonASCIITyping", 1);
  EXPECT_COUNT_STATS("SPACE", 1);

  // Smoke test to make sure it works without crash.
  for (int i = 0; i < KeyEvent::NUM_SPECIALKEYS; ++i) {
    input.Clear();
    input.set_type(Input::SEND_KEY);
    input.mutable_key()->set_special_key(static_cast<KeyEvent::SpecialKey>(i));
    SessionUsageStatsUtil::AddSendKeyInputStats(input);
  }
  EXPECT_COUNT_STATS("ASCIITyping", 1);
  EXPECT_COUNT_STATS("NonASCIITyping", 1 + KeyEvent::NUM_SPECIALKEYS);
  EXPECT_COUNT_STATS("SPACE", 2);
  EXPECT_COUNT_STATS("ENTER", 1);
}

TEST_F(SessionUsageStatsUtilTest, AddSendKeyOutputStats) {
  Output output;

  output.set_consumed(false);
  SessionUsageStatsUtil::AddSendKeyOutputStats(output);
  EXPECT_COUNT_STATS("ConsumedSendKey", 0);
  EXPECT_COUNT_STATS("UnconsumedSendKey", 1);

  output.set_consumed(true);
  SessionUsageStatsUtil::AddSendKeyOutputStats(output);
  EXPECT_COUNT_STATS("ConsumedSendKey", 1);
  EXPECT_COUNT_STATS("UnconsumedSendKey", 1);
}

TEST_F(SessionUsageStatsUtilTest, AddSendCommandInputStats) {
  Input input;
  input.set_type(Input::SEND_COMMAND);
  input.mutable_command()->set_type(SessionCommand::SUBMIT);
  SessionUsageStatsUtil::AddSendCommandInputStats(input);
  EXPECT_COUNT_STATS("SendCommand_Submit", 1);

  input.Clear();
  input.set_type(Input::SEND_COMMAND);

  input.mutable_command()->set_type(SessionCommand::REVERT);
  SessionUsageStatsUtil::AddSendCommandInputStats(input);
  EXPECT_COUNT_STATS("SendCommand_Revert", 1);
  EXPECT_COUNT_STATS("SendCommand_RevertInChromeOmnibox", 0);
  EXPECT_COUNT_STATS("SendCommand_RevertInGoogleSearchBox", 0);

  input.mutable_context()->add_experimental_features("chrome_omnibox");
  SessionUsageStatsUtil::AddSendCommandInputStats(input);
  EXPECT_COUNT_STATS("SendCommand_Revert", 2);
  EXPECT_COUNT_STATS("SendCommand_RevertInChromeOmnibox", 1);
  EXPECT_COUNT_STATS("SendCommand_RevertInGoogleSearchBox", 0);

  input.mutable_context()->add_experimental_features("google_search_box");
  SessionUsageStatsUtil::AddSendCommandInputStats(input);
  EXPECT_COUNT_STATS("SendCommand_Revert", 3);
  EXPECT_COUNT_STATS("SendCommand_RevertInChromeOmnibox", 2);
  EXPECT_COUNT_STATS("SendCommand_RevertInGoogleSearchBox", 1);

  input.Clear();
  input.set_type(Input::SEND_COMMAND);
  input.mutable_command()->set_type(SessionCommand::SELECT_CANDIDATE);
  input.mutable_command()->set_id(0);
  SessionUsageStatsUtil::AddSendCommandInputStats(input);
  EXPECT_COUNT_STATS("SendCommand_SelectCandidate", 1);
  EXPECT_COUNT_STATS("MouseSelect", 1);
  input.mutable_command()->set_type(SessionCommand::SUBMIT_CANDIDATE);
  SessionUsageStatsUtil::AddSendCommandInputStats(input);
  EXPECT_COUNT_STATS("SendCommand_SubmitCandidate", 1);
  EXPECT_COUNT_STATS("MouseSelect", 2);

  // Smoke test to make sure it works without crash.
  for (int i = 0; i < SessionCommand::CommandType_ARRAYSIZE; ++i) {
    if (!SessionCommand::CommandType_IsValid(i)) {
      continue;
    }
    input.Clear();
    input.set_type(Input::SEND_COMMAND);
    input.mutable_command()->set_type(
        static_cast<SessionCommand::CommandType>(i));
    SessionUsageStatsUtil::AddSendCommandInputStats(input);
  }
  EXPECT_COUNT_STATS("SendCommand_Submit", 2);
  EXPECT_COUNT_STATS("SendCommand_Revert", 4);
  EXPECT_COUNT_STATS("SendCommand_SelectCandidate", 2);
  EXPECT_COUNT_STATS("SendCommand_SubmitCandidate", 2);
  EXPECT_COUNT_STATS("SendCommand_Undo", 1);
  EXPECT_COUNT_STATS("SendCommand_StopKeyToggling", 1);
  EXPECT_COUNT_STATS("MouseSelect", 4);
}

}  // namespace
}  // namespace session
}  // namespace mozc
