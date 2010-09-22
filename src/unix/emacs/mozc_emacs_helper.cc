// Copyright 2010, Google Inc.
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

#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/version.h"
#include "client/session.h"
#include "session/commands.pb.h"
#include "unix/emacs/session_pool.h"

namespace {

// Prints a greeting message when a process starts.
void PrintGreetingMessage() {
  cout << "((mozc-emacs-helper . t)"
       << "(version . "
       << mozc::emacs::QuoteString(mozc::Version::GetMozcVersion())
       << "))"
       << endl;
}

// Main loop, which take an input line as a command and print a corresponding
// result returned by Mozc server in S-expression.
void ProcessLoop() {
  using mozc::emacs::ErrorExit;

  mozc::emacs::SessionPool session_pool;
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
        session_id = session_pool.CreateSession();
        break;
      case mozc::commands::Input::DELETE_SESSION:
        session_pool.DeleteSession(session_id);
        break;
      case mozc::commands::Input::SEND_KEY: {
        linked_ptr<mozc::client::Session> session =
            session_pool.GetSession(session_id);
        CHECK(session.get());
        if (!session->SendKey(command.input().key(),
                              command.mutable_output())) {
          ErrorExit(mozc::emacs::kErrSessionError, "Session failed");
        }
        break;
      }
      default:
        ErrorExit(mozc::emacs::kErrVoidFunction, "Unknown function");
    }

    // Output results.
    cout << "((emacs-event-id . " << event_id << ")";
    cout << "(emacs-session-id . " << session_id << ")";
    cout << "(output . ";
    mozc::emacs::PrintMessage(command.output(), cout);
    cout << "))" << endl;
  }
}

}  // namespace


int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, true);

  PrintGreetingMessage();

  ProcessLoop();

  return 0;
}
