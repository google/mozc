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

#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "protocol/commands.pb.h"
#include "usage_stats/usage_stats.h"

namespace mozc {
namespace session {
namespace {

using ::mozc::commands::Input;
using ::mozc::commands::Output;
using ::mozc::usage_stats::UsageStats;

bool HasExperimentalFeature(const commands::Context &context,
                            absl::string_view key) {
  return absl::c_any_of(context.experimental_features(),
                        [=](absl::string_view f) { return f == key; });
}

// Splits a text by delimiter, capitalizes each piece and joins them.
// ex. "AbCd_efgH" => "AbcdEfgh" (delimiter = '_')
void CamelCaseString(std::string *str, char delm) {
  std::vector<std::string> pieces =
      absl::StrSplit(*str, delm, absl::SkipEmpty());
  for (std::string &piece : pieces) {
    Util::CapitalizeString(&piece);
  }
  *str = absl::StrJoin(pieces, "");
}
}  // namespace

void SessionUsageStatsUtil::AddSendKeyInputStats(const Input &input) {
  CHECK(input.has_key() && input.type() == commands::Input::SEND_KEY);

  if (input.key().has_key_code()) {
    UsageStats::IncrementCount("ASCIITyping");
  } else if (input.key().has_special_key()) {
    UsageStats::IncrementCount("NonASCIITyping");
    UsageStats::IncrementCount(
        commands::KeyEvent::SpecialKey_Name(input.key().special_key()));
  }
}

void SessionUsageStatsUtil::AddSendKeyOutputStats(const Output &output) {
  if (output.has_consumed() && output.consumed()) {
    UsageStats::IncrementCount("ConsumedSendKey");
  } else {
    UsageStats::IncrementCount("UnconsumedSendKey");
  }
}

void SessionUsageStatsUtil::AddSendCommandInputStats(const Input &input) {
  CHECK(input.has_command() && input.type() == commands::Input::SEND_COMMAND);

  std::string name =
      commands::SessionCommand::CommandType_Name(input.command().type());
  CamelCaseString(&name, '_');
  UsageStats::IncrementCount("SendCommand_" + name);

  if (input.command().type() == commands::SessionCommand::REVERT) {
    if (HasExperimentalFeature(input.context(), "chrome_omnibox")) {
      UsageStats::IncrementCount("SendCommand_RevertInChromeOmnibox");
    }
    if (HasExperimentalFeature(input.context(), "google_search_box")) {
      UsageStats::IncrementCount("SendCommand_RevertInGoogleSearchBox");
    }
  }

  if (input.command().type() == commands::SessionCommand::SELECT_CANDIDATE ||
      input.command().type() == commands::SessionCommand::SUBMIT_CANDIDATE) {
    UsageStats::IncrementCount("MouseSelect");
  }
}

}  // namespace session
}  // namespace mozc
