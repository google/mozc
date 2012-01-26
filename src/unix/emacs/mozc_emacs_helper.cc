// Copyright 2010-2012, Google Inc.
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

#include "unix/emacs/mozc_emacs_helper_lib.h"

#include <cstdio>
#include <iostream>

#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/util.h"
#include "base/version.h"
#include "client/client.h"
#include "config/config_handler.h"
#include "languages/global_language_spec.h"
#include "languages/japanese/lang_dep_spec.h"
#include "session/commands.pb.h"
#include "unix/emacs/client_pool.h"

DEFINE_bool(suppress_stderr, false,
            "Discards all the output to stderr.");

namespace {

// Prints a greeting message when a process starts.
void PrintGreetingMessage() {
  const mozc::config::Config &config = mozc::config::ConfigHandler::GetConfig();
  const char *preedit_method = "unknown";
  switch (config.preedit_method()) {
    case mozc::config::Config::ROMAN:
      preedit_method = "roman";
      break;
    case mozc::config::Config::KANA:
      preedit_method = "kana";
      break;
  }

  fprintf(stdout,
          "((mozc-emacs-helper . t)(version . %s)"
          "(config . ((preedit-method . %s))))\n",
          mozc::emacs::QuoteString(mozc::Version::GetMozcVersion()).c_str(),
          preedit_method);
  fflush(stdout);
}

// Main loop, which takes an input line as a command and print a corresponding
// result returned by Mozc server in S-expression.
void ProcessLoop() {
  using mozc::emacs::ErrorExit;

  mozc::emacs::ClientPool client_pool;
  mozc::commands::Command command;
  string line;

  while (getline(cin, line)) {
    command.clear_input();
    command.clear_output();
    uint32 event_id = 0;
    uint32 session_id = 0;

    // Parse an input line.
    mozc::emacs::ParseInputLine(line, &event_id, &session_id,
                                command.mutable_input());

    switch (command.input().type()) {
      case mozc::commands::Input::CREATE_SESSION:
        session_id = client_pool.CreateClient();
        break;
      case mozc::commands::Input::DELETE_SESSION:
        client_pool.DeleteClient(session_id);
        break;
      case mozc::commands::Input::SEND_KEY: {
        linked_ptr<mozc::client::Client> client =
            client_pool.GetClient(session_id);
        CHECK(client.get());
        if (!client->SendKey(command.input().key(),
                              command.mutable_output())) {
          ErrorExit(mozc::emacs::kErrSessionError, "Session failed");
        }
        break;
      }
      default:
        ErrorExit(mozc::emacs::kErrVoidFunction, "Unknown function");
    }

    // Output results.
    vector<string> buffer;
    mozc::emacs::PrintMessage(command.output(), &buffer);
    string output;
    mozc::Util::JoinStrings(buffer, "", &output);
    fprintf(stdout,
            "((emacs-event-id . %u)(emacs-session-id . %u)(output . %s))\n",
            event_id, session_id, output.c_str());
    fflush(stdout);
  }
}

}  // namespace


int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  mozc::japanese::LangDepSpecJapanese spec;
  mozc::language::GlobalLanguageSpec::SetLanguageDependentSpec(&spec);

  if (FLAGS_suppress_stderr) {
#ifdef OS_WINDOWS
    const char path[] = "NUL";
#else
    const char path[] = "/dev/null";
#endif
    if (!freopen(path, "a", stderr)) {
      mozc::emacs::ErrorExit(mozc::emacs::kErrFileError,
                             "freopen for stderr failed");
    }
  }

  PrintGreetingMessage();

  ProcessLoop();

  return 0;
}
