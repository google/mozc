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
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "base/init_mozc.h"
#include "base/version.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "unix/emacs/client_pool.h"
#include "unix/emacs/mozc_emacs_helper_lib.h"

ABSL_FLAG(bool, suppress_stderr, false, "Discards all the output to stderr.");

namespace mozc::emacs {
namespace {

// Prints a greeting message when a process starts.
void PrintGreetingMessage() {
  std::shared_ptr<const config::Config> config =
      config::ConfigHandler::GetSharedConfig();
  absl::string_view preedit_method = "unknown";
  switch (config->preedit_method()) {
    case config::Config::ROMAN:
      preedit_method = "roman";
      break;
    case config::Config::KANA:
      preedit_method = "kana";
      break;
  }

  absl::FPrintF(stdout,
                "((mozc-emacs-helper . t)(version . %s)"
                "(config . ((preedit-method . %s))))\n",
                QuoteString(Version::GetMozcVersion()), preedit_method);
  fflush(stdout);
}

// Main loop, which takes an input line as a command and print a corresponding
// result returned by Mozc server in S-expression.
void ProcessLoop() {
  ClientPool client_pool;
  commands::Command command;
  std::string line;

  while (std::getline(std::cin, line)) {
    command.clear_input();
    command.clear_output();
    uint32_t event_id = 0;
    uint32_t session_id = 0;

    // Parse an input line.
    ParseInputLine(line, &event_id, &session_id, command.mutable_input());

    switch (command.input().type()) {
      case commands::Input::CREATE_SESSION:
        session_id = client_pool.CreateClient();
        break;
      case commands::Input::DELETE_SESSION:
        client_pool.DeleteClient(session_id);
        break;
      case commands::Input::SEND_KEY: {
        std::shared_ptr<client::Client> client =
            client_pool.GetClient(session_id);
        CHECK(client.get());
        if (!client->SendKey(command.input().key(), command.mutable_output())) {
          ErrorExit(kErrSessionError, "Session failed");
        }
        break;
      }
      default:
        ErrorExit(kErrVoidFunction, "Unknown function");
    }

    RemoveUsageData(command.mutable_output());

    // Output results.
    std::vector<std::string> buffer;
    PrintMessage(command.output(), &buffer);
    const std::string output = absl::StrJoin(buffer, "");
    absl::FPrintF(
        stdout, "((emacs-event-id . %u)(emacs-session-id . %u)(output . %s))\n",
        event_id, session_id, output);
    fflush(stdout);
  }
}

}  // namespace
}  // namespace mozc::emacs

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv);
  if (absl::GetFlag(FLAGS_suppress_stderr)) {
#ifdef _WIN32
    constexpr char kPath[] = "NUL";
#else   // _WIN32
    constexpr char kPath[] = "/dev/null";
#endif  // _WIN32
    if (!freopen(kPath, "a", stderr)) {
      mozc::emacs::ErrorExit(mozc::emacs::kErrFileError,
                             "freopen for stderr failed");
    }
  }

  mozc::emacs::PrintGreetingMessage();

  mozc::emacs::ProcessLoop();

  return 0;
}
