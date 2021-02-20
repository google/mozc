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

// session_handler_main.cc
//
// Usage:
// session_handler_main --logtostderr --input input.txt --profile /tmp/mozc
//                      --dictionary oss --engine desktop
//
/* Example of input.txt
# Enable IME
SEND_KEY        ON

SET_MOBILE_REQUEST

RESET_CONTEXT
UPDATE_MOBILE_KEYBOARD  QWERTY_MOBILE_TO_HIRAGANA
SWITCH_INPUT_MODE       HIRAGANA

SEND_KEYS       arigatou
SEND_KEY        Enter
# EXPAND_SUGGESTION
# SHOW_OUTPUT
SHOW
SHOW_LOG_BY_VALUE       ございます
SHOW_LOG_BY_VALUE       ございました
*/

#include <iostream>
#include <string>

#include "base/file_stream.h"
#include "base/init_mozc.h"
#include "base/status.h"
#include "base/system_util.h"
#include "data_manager/android/android_data_manager.h"
#include "data_manager/google/google_data_manager.h"
#include "data_manager/oss/oss_data_manager.h"
#include "engine/engine.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "session/session_handler_tool.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"

ABSL_FLAG(std::string, input, "", "Input file");
ABSL_FLAG(std::string, profile, "", "User profile directory");
ABSL_FLAG(std::string, engine, "", "Conversion engine: 'mobile' or 'desktop'");
ABSL_FLAG(std::string, dictionary, "",
          "Dictionary: 'google', 'android' or 'oss'");

namespace mozc {
void Show(const commands::Output &output) {
  for (const auto &segment : output.preedit().segment()) {
    std::cout << segment.value() << " ";
  }
  std::cout << "(" << output.preedit().cursor() << ")" << std::endl;
  for (const auto &candidate : output.candidates().candidate()) {
    std::cout << candidate.id() << ": " << candidate.value() << std::endl;
  }
}

void ShowLog(const commands::Output &output, const int cand_id) {
  for (const auto &candidate : output.all_candidate_words().candidates()) {
    if (candidate.id() == cand_id) {
      std::cout << candidate.value() << std::endl;
      std::cout << candidate.log() << std::endl;
      return;
    }
  }

  for (const auto &candidate :
       output.removed_candidate_words_for_debug().candidates()) {
    if (candidate.id() == cand_id) {
      std::cout << candidate.value() << std::endl;
      std::cout << candidate.log() << std::endl;
      return;
    }
  }
}

void ParseLine(session::SessionHandlerInterpreter &handler, std::string line) {
  std::vector<std::string> args = handler.Parse(line);
  if (args.empty()) {
    return;
  }
  const std::string &command = args[0];

  if (command == "SHOW_OUTPUT") {
    std::cout << handler.LastOutput().Utf8DebugString() << std::endl;
    return;
  }
  if (command == "SHOW") {
    Show(handler.LastOutput());
    return;
  }
  if (command == "SHOW_LOG") {
    uint32 id;
    if (args.size() == 2 && absl::SimpleAtoi(args[1], &id)) {
      ShowLog(handler.LastOutput(), id);
    } else {
      std::cout << "ERROR: " << line << std::endl;
    }
    return;
  }
  if (command == "SHOW_LOG_BY_VALUE") {
    uint32 id;
    if (args.size() == 2 && handler.GetCandidateIdByValue(args[1], &id)) {
      ShowLog(handler.LastOutput(), id);
    } else {
      std::cout << "ERROR: " << line << std::endl;
    }
    return;
  }

  const mozc::Status status = handler.Eval(args);
  if (!status.ok()) {
    std::cout << "ERROR: " << status.message() << std::endl;
  }
}

std::unique_ptr<const DataManagerInterface> CreateDataManager(
    const std::string &dictionary) {
  if (dictionary == "android") {
    return absl::make_unique<const android::AndroidDataManager>();
  }
  if (dictionary == "google") {
    return absl::make_unique<const google::GoogleDataManager>();
  }
  if (dictionary == "oss") {
    return absl::make_unique<const oss::OssDataManager>();
  }
  if (!dictionary.empty()) {
    std::cout << "ERROR: Unknown dictionary name: " << dictionary << std::endl;
  }
  return absl::make_unique<const google::GoogleDataManager>();
}

mozc::StatusOr<std::unique_ptr<Engine>> CreateEngine(
    const std::string &engine, const std::string &dictionary) {
  if (engine == "desktop") {
    return Engine::CreateDesktopEngine(CreateDataManager(dictionary));
  }
  if (engine == "mobile") {
    return Engine::CreateMobileEngine(CreateDataManager(dictionary));
  }
  if (!engine.empty()) {
    std::cout << "ERROR: Unknown engine name: " << engine << std::endl;
  }
  return Engine::CreateMobileEngine(CreateDataManager(dictionary));
}

}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  if (!absl::GetFlag(FLAGS_profile).empty()) {
    mozc::SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_profile));
  }
  auto engine = mozc::CreateEngine(absl::GetFlag(FLAGS_engine),
                                   absl::GetFlag(FLAGS_dictionary));
  if (!engine.ok()) {
    std::cout << "engine init error" << std::endl;
    return 1;
  }
  mozc::session::SessionHandlerInterpreter handler(*std::move(engine));

  std::string line;
  if (!absl::GetFlag(FLAGS_input).empty()) {
    mozc::InputFileStream input(absl::GetFlag(FLAGS_input).c_str());
    while (std::getline(input, line)) {
      mozc::ParseLine(handler, line);
    }
  }

  while (std::getline(std::cin, line)) {
    mozc::ParseLine(handler, line);
  }
  return 0;
}
