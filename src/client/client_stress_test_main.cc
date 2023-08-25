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
#include <iostream>

#include "client/client.h"

#ifdef _WIN32
#include <windows.h>
#else  // _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif  // _WIN32

#include <memory>
#include <ostream>
#include <vector>

#include "base/init_mozc.h"
#include "base/logging.h"
#include "protocol/renderer_command.pb.h"
#include "session/random_keyevents_generator.h"
#include "absl/flags/flag.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "renderer/renderer_client.h"

// TODO(taku)
// 1. multi-thread testing
// 2. change/config the senario

ABSL_FLAG(int32_t, max_keyevents, 100000,
          "test at most |max_keyevents| key sequences");
ABSL_FLAG(std::string, server_path, "", "specify server path");
ABSL_FLAG(int32_t, key_duration, 10, "key duration (msec)");
ABSL_FLAG(bool, test_renderer, false, "test renderer");
ABSL_FLAG(bool, test_testsendkey, true, "test TestSendKey");

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  absl::SetFlag(&FLAGS_logtostderr, true);

  mozc::client::Client client;
  if (!absl::GetFlag(FLAGS_server_path).empty()) {
    client.set_server_program(absl::GetFlag(FLAGS_server_path));
  }

  CHECK(client.IsValidRunLevel()) << "IsValidRunLevel failed";
  CHECK(client.EnsureSession()) << "EnsureSession failed";
  CHECK(client.NoOperation()) << "Server is not respoinding";

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
    renderer_client = std::make_unique<mozc::renderer::RendererClient>();
    CHECK(renderer_client->Activate());
#else   // _WIN32 || __APPLE__
    LOG(FATAL) << "test_renderer is only supported on Windows and Mac";
#endif  // _WIN32 || __APPLE__
  }

  std::vector<mozc::commands::KeyEvent> keys;
  mozc::commands::Output output;
  int32_t keyevents_size = 0;

  // TODO(taku):
  // Stop the test if server is crashed.
  // Currently, we cannot detect the server crash out of
  // client library, as client automatically re-lahches the server.

  mozc::session::RandomKeyEventsGenerator key_events_generator;
  while (true) {
    key_events_generator.GenerateSequence(&keys);
    CHECK(client.NoOperation()) << "Server is not responding";
    for (size_t i = 0; i < keys.size(); ++i) {
      absl::SleepFor(absl::Milliseconds(absl::GetFlag(FLAGS_key_duration)));
      keyevents_size++;
      if (keyevents_size % 100 == 0) {
        std::cout << keyevents_size << " key events finished" << std::endl;
      }
      if (absl::GetFlag(FLAGS_max_keyevents) < keyevents_size) {
        std::cout << "key events reached to "
                  << absl::GetFlag(FLAGS_max_keyevents) << std::endl;
        return 0;
      }
      if (absl::GetFlag(FLAGS_test_testsendkey)) {
        VLOG(2) << "Sending to Server: " << keys[i].DebugString();
        client.TestSendKey(keys[i], &output);
        VLOG(2) << "Output of TestSendKey: " << MOZC_LOG_PROTOBUF(output);
        absl::SleepFor(absl::Milliseconds(10));
      }

      VLOG(2) << "Sending to Server: " << keys[i].DebugString();
      client.SendKey(keys[i], &output);
      VLOG(2) << "Output of SendKey: " << MOZC_LOG_PROTOBUF(output);

      if (renderer_client != nullptr) {
        renderer_command.set_type(mozc::commands::RendererCommand::UPDATE);
        renderer_command.set_visible(output.has_candidates());
        *renderer_command.mutable_output() = output;
        VLOG(2) << "Sending to Renderer: "
                << MOZC_LOG_PROTOBUF(renderer_command);
        renderer_client->ExecCommand(renderer_command);
      }
    }
  }

  return 0;
}
