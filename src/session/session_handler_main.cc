// Copyright 2010-2020, Google Inc.
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

#include <iostream>
#include <string>

#include "base/file_stream.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/status.h"
#include "base/system_util.h"
#include "data_manager/android/android_data_manager.h"
#include "data_manager/google/google_data_manager.h"
#include "data_manager/oss/oss_data_manager.h"
#include "engine/engine.h"
#include "protocol/commands.pb.h"
#include "session/session_handler_tool.h"

DEFINE_string(input, "", "Input file");
DEFINE_string(profile, "", "User profile directory");
DEFINE_string(engine, "", "Conversion engine: 'mobile' or 'desktop'");
DEFINE_string(dictionary, "", "Dictionary: 'google', 'android' or 'oss'");

namespace mozc {
void Show(const commands::Output &output) {
  for (const auto &segment : output.preedit().segment()) {
    std::cout << segment.value() << " " << std::endl;
  }
  for (const auto &candidate : output.candidates().candidate()) {
    std::cout << candidate.id() << ": " << candidate.value() << std::endl;
  }
}

void ParseLine(session::SessionHandlerInterpreter &handler, std::string line) {
  if (line == "SHOW_OUTPUT") {
    std::cout << handler.LastOutput().Utf8DebugString() << std::endl;
    return;
  }
  if (line == "SHOW") {
    Show(handler.LastOutput());
    return;
  }

  const mozc::Status status = handler.ParseLine(line);
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
  if (!mozc::GetFlag(FLAGS_profile).empty()) {
    mozc::SystemUtil::SetUserProfileDirectory(mozc::GetFlag(FLAGS_profile));
  }
  auto engine = mozc::CreateEngine(mozc::GetFlag(FLAGS_engine),
                                   mozc::GetFlag(FLAGS_dictionary));
  if (!engine.ok()) {
    std::cout << "engine init error" << std::endl;
    return 1;
  }
  mozc::session::SessionHandlerInterpreter handler(*std::move(engine));

  std::string line;
  if (!mozc::GetFlag(FLAGS_input).empty()) {
    mozc::InputFileStream input(mozc::GetFlag(FLAGS_input).c_str());
    while (std::getline(input, line)) {
      mozc::ParseLine(handler, line);
    }
  }

  while (std::getline(std::cin, line)) {
    mozc::ParseLine(handler, line);
  }
  return 0;
}
