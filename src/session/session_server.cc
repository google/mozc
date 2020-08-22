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
const int kNumConnections = 1;
#else
const int kNumConnections = 10;
#endif  // OS_WIN

const int kTimeOut = 5000;  // 5000msec
const char kSessionName[] = "session";
const char kEventName[] = "session";

}  // namespace

namespace mozc {

SessionServer::SessionServer()
    : IPCServer(kSessionName, kNumConnections, kTimeOut),
      usage_observer_(new session::SessionUsageObserver()),
      session_handler_(new SessionHandler(
          std::unique_ptr<Engine>(EngineFactory::Create()))) {
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

bool SessionServer::Process(const char *request, size_t request_size,
                            char *response, size_t *response_size) {
  if (!session_handler_) {
    LOG(WARNING) << "handler is not available";
    return false;  // shutdown the server if handler doesn't exist
  }

  commands::Command command;  // can define as a private member?
  if (!command.mutable_input()->ParseFromArray(request, request_size)) {
    LOG(WARNING) << "Invalid request";
    *response_size = 0;
    return true;
  }

  if (!session_handler_->EvalCommand(&command)) {
    LOG(WARNING) << "EvalCommand() returned false. Exiting the loop.";
    *response_size = 0;
    return false;
  }

  std::string output;
  if (!command.output().SerializeToString(&output)) {
    LOG(WARNING) << "SerializeToString() failed";
    *response_size = 0;
    return true;
  }

  // TODO(taku) automatically increase the buffer.
  // Needs to fix IPCServer as well
  if (*response_size < output.size()) {
    LOG(WARNING) << "response size < output.size";
    *response_size = 0;
    return true;
  }

  ::memcpy(response, output.data(), output.size());
  *response_size = output.size();

  // debug message
  VLOG(2) << command.DebugString();

  return true;
}
}  // namespace mozc
