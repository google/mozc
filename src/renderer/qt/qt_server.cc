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

#include "renderer/qt/qt_server.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/system_util.h"
#include "client/client_interface.h"
#include "config/config_handler.h"
#include "ipc/ipc.h"
#include "ipc/named_event.h"
#include "protocol/renderer_command.pb.h"
#include "absl/synchronization/mutex.h"

// By default, mozc_renderer quits when user-input continues to be
// idle for 10min.
ABSL_FLAG(int32_t, timeout, 10 * 60, "timeout of candidate server (sec)");
ABSL_FLAG(bool, restricted, false,
          "launch candidates server with restricted mode");

namespace mozc {
namespace renderer {

namespace {
constexpr int kNumConnections = 10;
constexpr int kIPCServerTimeOut = 1000;
constexpr char kServiceName[] = "renderer";

std::string GetServiceName() {
  std::string name = kServiceName;
  const std::string desktop_name = SystemUtil::GetDesktopNameAsString();
  if (!desktop_name.empty()) {
    name += ".";
    name += desktop_name;
  }
  return name;
}
}  // namespace

QtServer::QtServer(int argc, char **argv)
    : IPCServer(GetServiceName(), kNumConnections, kIPCServerTimeOut),
      renderer_interface_(nullptr),
      argc_(argc), argv_(argv), updated_(false),
      timeout_(0) {
  if (absl::GetFlag(FLAGS_restricted)) {
    absl::SetFlag(&FLAGS_timeout,
                  // set 60sec with restricted mode
                  std::min(absl::GetFlag(FLAGS_timeout), 60));
  }

  timeout_ = 1000 * std::max(3, std::min(24 * 60 * 60,
                                         absl::GetFlag(FLAGS_timeout)));
  VLOG(2) << "timeout is set to be : " << timeout_;

#ifndef MOZC_NO_LOGGING
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  Logging::SetConfigVerboseLevel(config.verbose_level());
#endif  // MOZC_NO_LOGGING
}

QtServer::~QtServer() {}

bool QtServer::AsyncExecCommand(std::string *proto_message) {
  // Take the ownership of |proto_message|.
  std::unique_ptr<std::string> proto_message_owner(proto_message);
  {
    absl::MutexLock l(&mutex_);
    if (message_ == *proto_message_owner) {
      // This is exactly the same to the previous message. Theoretically it is
      // safe to do nothing here.
      return true;
    }

    // Since mozc rendering protocol is state-less, we can always ignore the
    // previous content of |message_|.
    message_.swap(*proto_message_owner);
    updated_ = true;
  }

  return true;
}

namespace {
bool cond_func(bool *cond_var) {
  return *cond_var;
}
}  // namespace

void QtServer::StartReceiverLoop() {
  while (true) {
    mutex_.LockWhen(absl::Condition(cond_func, &updated_));
    std::string message(message_);
    updated_ = false;
    mutex_.Unlock();

    commands::RendererCommand command;
    if (command.ParseFromString(message)) {
      ExecCommandInternal(command);
    } else {
      LOG(WARNING) << "Parse From String Failed";
    }
  }
}

int QtServer::StartMessageLoop() {
  ReceiverLoopFunc receiver_loop_func = [&](){ StartReceiverLoop(); };
  renderer_interface_->SetReceiverLoopFunction(receiver_loop_func);
  renderer_interface_->StartRendererLoop(argc_, argv_);
  return 0;
}

void QtServer::SetRendererInterface(
    RendererInterface *renderer_interface) {
  renderer_interface_ = renderer_interface;
}

int QtServer::StartServer() {
  if (!Connected()) {
    LOG(ERROR) << "cannot start server";
    return -1;
  }

  LoopAndReturn();

  // send "ready" event to the client
  const std::string name = GetServiceName();
  NamedEventNotifier notifier(name.c_str());
  notifier.Notify();

  // start main event loop
  return StartMessageLoop();
}

bool QtServer::Process(const char *request, size_t request_size,
                             char *response, size_t *response_size) {
  // here we just copy the serialized message in order
  // to reply to the client ui as soon as possible.
  // ParseFromString is executed in the main(another) thread.
  //
  // Since Process() and ExecCommand() are executed in
  // different threads, we have to use heap to share the serialized message.
  // If we use stack, this program will be crashed.
  //
  // The receiver of command_str takes the ownership of this string.
  std::string *command_str = new std::string(request, request_size);

  // no need to set the result code.
  *response_size = 1;
  response[0] = '\0';

  // Cannot call the method directly like renderer_interface_->ExecCommand()
  // as it's not thread-safe.
  return AsyncExecCommand(command_str);
}

bool QtServer::ExecCommandInternal(
    const commands::RendererCommand &command) {
  if (renderer_interface_ == nullptr) {
    LOG(ERROR) << "renderer_interface is nullptr";
    return false;
  }

  VLOG(2) << command.DebugString();

  return renderer_interface_->ExecCommand(command);
}

uint32_t QtServer::timeout() const { return timeout_; }

}  // namespace renderer
}  // namespace mozc
