// Copyright 2010-2013, Google Inc.
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

#include <iostream>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "client/client.h"
#include "renderer/renderer_command.pb.h"
#include "renderer/renderer_client.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"

DEFINE_bool(display_preedit, false, "display predit to tty");
DEFINE_string(input, "", "Input file");
DEFINE_int32(key_duration, 10, "Key duration (msec)");
DEFINE_string(profile_dir, "", "Profile dir");
DEFINE_bool(sentence_mode, false, "Use input as sentences");
DEFINE_string(server_path, "", "Specify server path");
DEFINE_bool(test_renderer, false, "Test renderer");
DEFINE_bool(test_testsendkey, true, "Test TestSendKey");

namespace mozc {
namespace {

string UTF8ToTtyString(const string &text) {
#ifdef OS_WIN
  string tmp;
  Util::UTF8ToSJIS(text, &tmp);
  return tmp;
#else
  return text;
#endif
}

void DisplayPreedit(const commands::Output &output) {
  if (output.has_preedit()) {
    string value;
    for (size_t i = 0; i < output.preedit().segment_size(); ++i) {
      value += output.preedit().segment(i).value();
    }
    cout << UTF8ToTtyString(value) << '\r';
  } else if (output.has_result()) {
    cout << UTF8ToTtyString(output.result().value()) << endl;
  }
}

// Parses key events.  If |input| gets EOF, returns false.
bool ReadKeys(istream *input,
              vector<commands::KeyEvent> *keys,
              string *answer) {
  keys->clear();
  answer->clear();

  string line;
  while (getline(*input, line)) {
    Util::ChopReturns(&line);
    if (line.size() > 1 && line[0] == '#' && line[1] == '#') {
      continue;
    }
    if (line.find(">> ") == 0) {
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

int Loop(istream *input) {
  mozc::client::Client client;
  if (!FLAGS_server_path.empty()) {
    client.set_server_program(FLAGS_server_path);
  }

  CHECK(client.IsValidRunLevel()) << "IsValidRunLevel failed";
  CHECK(client.EnsureSession()) << "EnsureSession failed";
  CHECK(client.NoOperation()) << "Server is not responding";

  scoped_ptr<mozc::renderer::RendererClient> renderer_client;
  mozc::commands::RendererCommand renderer_command;

  if (FLAGS_test_renderer) {
#if defined(OS_WIN) || defined(OS_MACOSX)
#ifdef OS_WIN
    renderer_command.mutable_application_info()->set_process_id
        (::GetCurrentProcessId());
    renderer_command.mutable_application_info()->set_thread_id
        (::GetCurrentThreadId());
#endif
    renderer_command.mutable_preedit_rectangle()->set_left(10);
    renderer_command.mutable_preedit_rectangle()->set_top(10);
    renderer_command.mutable_preedit_rectangle()->set_right(200);
    renderer_command.mutable_preedit_rectangle()->set_bottom(30);
#else
    LOG(FATAL) << "test_renderer is only supported on Windows and Mac";
#endif
    renderer_client.reset(new renderer::RendererClient);
    CHECK(renderer_client->Activate());
  }

  commands::Command command;
  commands::Output output;
  vector<commands::KeyEvent> keys;
  string answer;

  // TODO(tok): Stop the test if server is crashed.  Currently, we cannot
  // detect the server crash out of client library, as client automatically
  // re-launches the server.  See also session_stress_test_main.cc.

  while (ReadKeys(input, &keys, &answer)) {
    CHECK(client.NoOperation()) << "Server is not responding";
    for (size_t i = 0; i < keys.size(); ++i) {
      Util::Sleep(FLAGS_key_duration);

      if (FLAGS_test_testsendkey) {
        VLOG(2) << "Sending to Server: " << keys[i].DebugString();
        client.TestSendKey(keys[i], &output);
        VLOG(2) << "Output of TestSendKey: " << output.DebugString();
        Util::Sleep(10);
      }

      VLOG(2) << "Sending to Server: " << keys[i].DebugString();
      client.SendKey(keys[i], &output);
      VLOG(2) << "Output of SendKey: " << output.DebugString();

      if (renderer_client.get() != NULL) {
        renderer_command.set_type(commands::RendererCommand::UPDATE);
        renderer_command.set_visible(output.has_candidates());
        renderer_command.mutable_output()->CopyFrom(output);
        VLOG(2) << "Sending to Renderer: " << renderer_command.DebugString();
        renderer_client->ExecCommand(renderer_command);
      }

      if (FLAGS_display_preedit) {
        mozc::DisplayPreedit(output);
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
  InitGoogle(argv[0], &argc, &argv, false);

  if (!FLAGS_profile_dir.empty()) {
    mozc::FileUtil::CreateDirectory(FLAGS_profile_dir);
    mozc::SystemUtil::SetUserProfileDirectory(FLAGS_profile_dir);
  }

  scoped_ptr<mozc::InputFileStream> input_file(NULL);
  istream *input = NULL;

  if (!FLAGS_input.empty()) {
    // Batch mode loading the input file.
    input_file.reset(new mozc::InputFileStream(FLAGS_input.c_str()));
    if (input_file->fail()) {
      LOG(ERROR) << "File not opened: " << FLAGS_input;
      return 1;
    }
    input = input_file.get();
  } else {
    // Interaction mode.
    input = &cin;
  }

  return mozc::Loop(input);
}
