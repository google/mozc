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

#include <iostream>

#include "client/client.h"

#ifdef OS_WIN
#include <windows.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif  // OS_WIN

#include <memory>

#include "base/file_stream.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/renderer_client.h"
#include "session/random_keyevents_generator.h"
#include "absl/flags/flag.h"

// TODO(taku)
// 1. multi-thread testing
// 2. change/config the senario

MOZC_FLAG(int32, max_keyevents, 100000,
          "test at most |max_keyevents| key sequences");
MOZC_FLAG(string, server_path, "", "specify server path");
MOZC_FLAG(int32, key_duration, 10, "key duration (msec)");
MOZC_FLAG(bool, test_renderer, false, "test renderer");
MOZC_FLAG(bool, test_testsendkey, true, "test TestSendKey");

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);

  mozc::SetFlag(&FLAGS_logtostderr, true);

  mozc::client::Client client;
  if (!mozc::GetFlag(FLAGS_server_path).empty()) {
    client.set_server_program(mozc::GetFlag(FLAGS_server_path));
  }

  CHECK(client.IsValidRunLevel()) << "IsValidRunLevel failed";
  CHECK(client.EnsureSession()) << "EnsureSession failed";
  CHECK(client.NoOperation()) << "Server is not respoinding";

  std::unique_ptr<mozc::renderer::RendererClient> renderer_client;
  mozc::commands::RendererCommand renderer_command;

  if (mozc::GetFlag(FLAGS_test_renderer)) {
#ifdef OS_WIN
    renderer_command.mutable_application_info()->set_process_id(
        ::GetCurrentProcessId());
    renderer_command.mutable_application_info()->set_thread_id(
        ::GetCurrentThreadId());
#endif  // OS_WIN
#if defined(OS_WIN) || defined(__APPLE__)
    renderer_command.mutable_preedit_rectangle()->set_left(10);
    renderer_command.mutable_preedit_rectangle()->set_top(10);
    renderer_command.mutable_preedit_rectangle()->set_right(200);
    renderer_command.mutable_preedit_rectangle()->set_bottom(30);
    renderer_client = absl::make_unique<mozc::renderer::RendererClient>();
    CHECK(renderer_client->Activate());
#else
    LOG(FATAL) << "test_renderer is only supported on Windows and Mac";
#endif  // OS_WIN || __APPLE__
  }

  std::vector<mozc::commands::KeyEvent> keys;
  mozc::commands::Output output;
  int32 keyevents_size = 0;

  // TODO(taku):
  // Stop the test if server is crashed.
  // Currently, we cannot detect the server crash out of
  // client library, as client automatically re-lahches the server.

  while (true) {
    mozc::session::RandomKeyEventsGenerator::GenerateSequence(&keys);
    CHECK(client.NoOperation()) << "Server is not responding";
    for (size_t i = 0; i < keys.size(); ++i) {
      mozc::Util::Sleep(mozc::GetFlag(FLAGS_key_duration));
      keyevents_size++;
      if (keyevents_size % 100 == 0) {
        std::cout << keyevents_size << " key events finished" << std::endl;
      }
      if (mozc::GetFlag(FLAGS_max_keyevents) < keyevents_size) {
        std::cout << "key events reached to "
                  << mozc::GetFlag(FLAGS_max_keyevents) << std::endl;
        return 0;
      }
      if (mozc::GetFlag(FLAGS_test_testsendkey)) {
        VLOG(2) << "Sending to Server: " << keys[i].DebugString();
        client.TestSendKey(keys[i], &output);
        VLOG(2) << "Output of TestSendKey: " << output.DebugString();
        mozc::Util::Sleep(10);
      }

      VLOG(2) << "Sending to Server: " << keys[i].DebugString();
      client.SendKey(keys[i], &output);
      VLOG(2) << "Output of SendKey: " << output.DebugString();

      if (renderer_client != nullptr) {
        renderer_command.set_type(mozc::commands::RendererCommand::UPDATE);
        renderer_command.set_visible(output.has_candidates());
        renderer_command.mutable_output()->CopyFrom(output);
        VLOG(2) << "Sending to Renderer: " << renderer_command.DebugString();
        renderer_client->ExecCommand(renderer_command);
      }
    }
  }

  return 0;
}
