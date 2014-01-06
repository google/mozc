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

#include "session/session_usage_stats_util.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/util.h"
#include "session/commands.pb.h"
#include "usage_stats/usage_stats.h"

using mozc::commands::Command;
using mozc::commands::Input;
using mozc::commands::Output;
using mozc::protobuf::EnumValueDescriptor;
using mozc::protobuf::FieldDescriptor;
using mozc::protobuf::Message;
using mozc::usage_stats::UsageStats;

namespace mozc {
namespace session {

namespace {
bool GetEnumValueName(const Message &message, const string &enum_name,
                      string *output) {
  const FieldDescriptor *field =
      message.GetDescriptor()->FindFieldByName(enum_name);
  if (field == NULL || field->cpp_type() != FieldDescriptor::CPPTYPE_ENUM) {
    LOG(DFATAL) << "Invalid enum field name: " << enum_name;
    return false;
  }
  const EnumValueDescriptor *enum_value =
      message.GetReflection()->GetEnum(message, field);
  *output = enum_value->name();
  return true;
}

// Splits a text by delimitor, capitalizes each piece and joins them.
// ex. "AbCd_efgH" => "AbcdEfgh" (delimitor = "_")
void CamelCaseString(string *str, const char *delm) {
  vector<string> pieces;
  Util::SplitStringUsing(*str, delm, &pieces);
  for (size_t i = 0; i < pieces.size(); ++i) {
    Util::CapitalizeString(&pieces[i]);
  }
  Util::JoinStrings(pieces, "", str);
}
}  // namespace

bool SessionUsageStatsUtil::HasExperimentalFeature(
    const commands::Context &context, const char *key) {
  for (size_t i = 0; i < context.experimental_features_size(); ++i) {
    if (context.experimental_features(i) == key) {
      return true;
    }
  }
  return false;
}

void SessionUsageStatsUtil::AddSendKeyInputStats(const Input &input) {
  CHECK(input.has_key() && input.type() == commands::Input::SEND_KEY);

  if (input.key().has_key_code()) {
    UsageStats::IncrementCount("ASCIITyping");
  } else if (input.key().has_special_key()) {
    UsageStats::IncrementCount("NonASCIITyping");
    const char kEnumTypeName[] = "special_key";
    string name;
    if (GetEnumValueName(input.key(), kEnumTypeName, &name)) {
      UsageStats::IncrementCount(name);
    }
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

  const char kEnumTypeName[] = "type";
  string name;
  if (GetEnumValueName(input.command(), kEnumTypeName, &name)) {
    CamelCaseString(&name, "_");
    UsageStats::IncrementCount("SendCommand_" + name);

    if (input.command().type() == commands::SessionCommand::REVERT) {
      if (HasExperimentalFeature(input.context(), "chrome_omnibox")) {
        UsageStats::IncrementCount("SendCommand_RevertInChromeOmnibox");
      }
      if (HasExperimentalFeature(input.context(), "google_search_box")) {
        UsageStats::IncrementCount("SendCommand_RevertInGoogleSearchBox");
      }
    }
  }

  if (input.command().type() == commands::SessionCommand::SELECT_CANDIDATE ||
      input.command().type() == commands::SessionCommand::SUBMIT_CANDIDATE) {
    UsageStats::IncrementCount("MouseSelect");
  }
}

}  // namespace session
}  // namespace mozc
