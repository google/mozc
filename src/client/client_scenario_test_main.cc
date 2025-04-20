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

// This is a client and session test with fixed sequence of key events.  It is
// similar test with session_stress_test_main, but senario test uses fixed key
// events specified by FLAGS_input file or interactive standard input.  Input
// file format is same as one of session/session_client_main.

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <istream>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/init_mozc.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "client/client.h"
#include "composer/key_parser.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/renderer_client.h"

ABSL_FLAG(std::string, input, "", "Input file");
ABSL_FLAG(int32_t, key_duration, 10, "Key duration (msec)");
ABSL_FLAG(std::string, profile_dir, "", "Profile dir");
ABSL_FLAG(bool, sentence_mode, false, "Use input as sentences");
ABSL_FLAG(std::string, server_path, "", "Specify server path");
ABSL_FLAG(bool, test_renderer, false, "Test renderer");
ABSL_FLAG(bool, test_testsendkey, true, "Test TestSendKey");

namespace mozc {
namespace {

// Parses key events.  If |input| gets EOF, returns false.
bool ReadKeys(std::istream *input, std::vector<commands::KeyEvent> *keys,
              std::string *answer) {
  keys->clear();
  answer->clear();

  std::string line;
  while (std::getline(*input, line)) {
    Util::ChopReturns(&line);
    if (line.size() > 1 && line[0] == '#' && line[1] == '#') {
      continue;
    }
    if (line.starts_with(">> ")) {
      // Answer line
      answer->assign(line, 3, line.size() - 3);
      continue;
    }
    if (line.empty()) {
      return true;
    }
    commands::KeyEvent key;
    if (!KeyParser::ParseKey(line, &key)) {
      LOG(ERROR) << "cannot parse: " << line;
      continue;
    }
    keys->push_back(key);
  }
  return false;
}

int Loop(std::istream *input) {
  mozc::client::Client client;
  if (!absl::GetFlag(FLAGS_server_path).empty()) {
    client.set_server_program(absl::GetFlag(FLAGS_server_path));
  }

  CHECK(client.IsValidRunLevel()) << "IsValidRunLevel failed";
  CHECK(client.EnsureSession()) << "EnsureSession failed";
  CHECK(client.NoOperation()) << "Server is not responding";

  std::unique_ptr<mozc::renderer::RendererClient> renderer_client;
  mozc::commands::RendererCommand renderer_command;

  if (absl::GetFlag(FLAGS_test_renderer)) {
#ifdef _WIN32
    renderer_command.mutable_application_info()->set_process_id(
        ::GetCurrentProcessId());
    renderer_command.mutable_application_info()->set_thread_id(
        ::GetCurrentThreadId());
#endif  // _WIN32
#if defined(_WIN32) || defined(__APPLE__)
    renderer_command.mutable_preedit_rectangle()->set_left(10);
    renderer_command.mutable_preedit_rectangle()->set_top(10);
    renderer_command.mutable_preedit_rectangle()->set_right(200);
    renderer_command.mutable_preedit_rectangle()->set_bottom(30);
    renderer_client = std::make_unique<renderer::RendererClient>();
    CHECK(renderer_client->Activate());
#else   // defined(_WIN32) || defined(__APPLE__)
    LOG(FATAL) << "test_renderer is only supported on Windows and Mac";
#endif  // defined(_WIN32) || defined(__APPLE__)
  }

  commands::Command command;
  commands::Output output;
  std::vector<commands::KeyEvent> keys;
  std::string answer;

  // TODO(tok): Stop the test if server is crashed.  Currently, we cannot
  // detect the server crash out of client library, as client automatically
  // re-launches the server.  See also session_stress_test_main.cc.

  while (ReadKeys(input, &keys, &answer)) {
    CHECK(client.NoOperation()) << "Server is not responding";
    for (size_t i = 0; i < keys.size(); ++i) {
      absl::SleepFor(absl::Milliseconds(absl::GetFlag(FLAGS_key_duration)));

      if (absl::GetFlag(FLAGS_test_testsendkey)) {
        MOZC_VLOG(2) << "Sending to Server: " << keys[i];
        client.TestSendKey(keys[i], &output);
        MOZC_VLOG(2) << "Output of TestSendKey: " << output;
        absl::SleepFor(absl::Milliseconds(10));
      }

      MOZC_VLOG(2) << "Sending to Server: " << keys[i];
      client.SendKey(keys[i], &output);
      MOZC_VLOG(2) << "Output of SendKey: " << output;

      if (renderer_client != nullptr) {
        renderer_command.set_type(commands::RendererCommand::UPDATE);
        renderer_command.set_visible(output.has_candidate_window());
        *renderer_command.mutable_output() = output;
        MOZC_VLOG(2) << "Sending to Renderer: " << renderer_command;
        renderer_client->ExecCommand(renderer_command);
      }
    }
    if (!answer.empty() && (output.result().value() != answer)) {
      LOG(ERROR) << "wrong value: " << output.result().value()
                 << " (expected: " << answer << ")";
    }
  }

  return 0;
}

}  // namespace
}  // namespace mozc

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (!absl::GetFlag(FLAGS_profile_dir).empty()) {
    if (absl::Status s =
            mozc::FileUtil::CreateDirectory(absl::GetFlag(FLAGS_profile_dir));
        !s.ok() && !absl::IsAlreadyExists(s)) {
      LOG(ERROR) << s;
      return -1;
    }
    mozc::SystemUtil::SetUserProfileDirectory(absl::GetFlag(FLAGS_profile_dir));
  }

  std::unique_ptr<mozc::InputFileStream> input_file;
  std::istream *input = nullptr;

  if (!absl::GetFlag(FLAGS_input).empty()) {
    // Batch mode loading the input file.
    input_file =
        std::make_unique<mozc::InputFileStream>(absl::GetFlag(FLAGS_input));
    if (input_file->fail()) {
      LOG(ERROR) << "File not opened: " << absl::GetFlag(FLAGS_input);
      return 1;
    }
    input = input_file.get();
  } else {
    // Interaction mode.
    input = &std::cin;
  }

  return mozc::Loop(input);
}
