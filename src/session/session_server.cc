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

// SessionServer is a subclass of IPCServer. It receives requests from Mozc
// clients via IPC connection. The requests and responses are serialized
// protocol buffers in string.

#include "session/session_server.h"

#include <memory>
#include <string>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/vlog.h"
#include "engine/engine_factory.h"
#include "ipc/ipc.h"
#include "ipc/named_event.h"
#include "protocol/commands.pb.h"
#include "session/session_handler.h"

namespace {

#ifdef _WIN32
// On Windows, multiple processes can create named pipe objects whose names are
// the same. To reduce the potential risk of DOS, we limit the maximum number
// of pipe instances to 1 here.
constexpr int kNumConnections = 1;
#else   // _WIN32
constexpr int kNumConnections = 10;
#endif  // _WIN32

constexpr absl::Duration kTimeOut = absl::Milliseconds(5000);
constexpr char kSessionName[] = "session";
constexpr char kEventName[] = "session";

}  // namespace

namespace mozc {

SessionServer::SessionServer()
    : IPCServer(kSessionName, kNumConnections, kTimeOut),
      session_handler_(
          std::make_unique<SessionHandler>(EngineFactory::Create().value())) {
  // start session watch dog timer
  session_handler_->StartWatchDog();

  // Send a notification event to the UI.
  NamedEventNotifier notifier(kEventName);
  if (!notifier.Notify()) {
    LOG(WARNING) << "NamedEvent " << kEventName << " is not found";
  }
}

bool SessionServer::Connected() const {
  return (session_handler_ && session_handler_->IsAvailable() &&
          IPCServer::Connected());
}

bool SessionServer::Process(absl::string_view request, std::string *response) {
  if (!session_handler_) {
    LOG(WARNING) << "handler is not available";
    return false;  // shutdown the server if handler doesn't exist
  }

  commands::Command command;
  if (!command.mutable_input()->ParseFromArray(request.data(),
                                               request.size())) {
    LOG(WARNING) << "Invalid request";
    response->clear();
    return true;
  }

  if (!session_handler_->EvalCommand(&command)) {
    LOG(WARNING) << "EvalCommand() returned false. Exiting the loop.";
    response->clear();
    return false;
  }

  if (!command.output().SerializeToString(response)) {
    LOG(WARNING) << "SerializeToString() failed";
    response->clear();
    return true;
  }

  // debug message
  MOZC_VLOG(2) << command;

  return true;
}
}  // namespace mozc
