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

// Session Server

#include "session/session_server.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/port.h"
#include "base/scheduler.h"
#include "engine/engine_factory.h"
#include "ipc/ipc.h"
#include "ipc/named_event.h"
#include "protocol/commands.pb.h"
#include "session/session_handler.h"
#include "session/session_usage_observer.h"
#include "usage_stats/usage_stats_uploader.h"

namespace {

#ifdef OS_WIN
// On Windows, multiple processes can create named pipe objects whose names are
// the same. To reduce the potential risk of DOS, we limit the maximum number
// of pipe instances to 1 here.
constexpr int kNumConnections = 1;
#else   // OS_WIN
constexpr int kNumConnections = 10;
#endif  // OS_WIN

constexpr int kTimeOut = 5000;  // 5000msec
constexpr char kSessionName[] = "session";
constexpr char kEventName[] = "session";

}  // namespace

namespace mozc {

SessionServer::SessionServer()
    : IPCServer(kSessionName, kNumConnections, kTimeOut),
      usage_observer_(std::make_unique<session::SessionUsageObserver>()),
      session_handler_(
          std::make_unique<SessionHandler>(EngineFactory::Create().value())) {
  using usage_stats::UsageStatsUploader;
  // start session watch dog timer
  session_handler_->StartWatchDog();
  session_handler_->AddObserver(usage_observer_.get());

  // Send a notification event to the UI.
  NamedEventNotifier notifier(kEventName);
  if (!notifier.Notify()) {
    LOG(WARNING) << "NamedEvent " << kEventName << " is not found";
  }
}

SessionServer::~SessionServer() = default;

bool SessionServer::Connected() const {
  return (session_handler_ && session_handler_->IsAvailable() &&
          IPCServer::Connected());
}

bool SessionServer::Process(absl::string_view request, std::string *response) {
  if (!session_handler_) {
    LOG(WARNING) << "handler is not available";
    return false;  // shutdown the server if handler doesn't exist
  }

  commands::Command command;  // can define as a private member?
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
  VLOG(2) << command.DebugString();

  return true;
}
}  // namespace mozc
