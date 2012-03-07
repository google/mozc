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

#include "renderer/renderer_server.h"

#ifdef OS_WINDOWS
#include <windows.h>
#endif  // OS_WINDOWS

#include <algorithm>
#include "base/base.h"
#include "base/const.h"
#include "base/logging.h"
#include "base/util.h"
#include "client/client_interface.h"
#include "config/config_handler.h"
#include "ipc/ipc.h"
#include "ipc/named_event.h"
#include "ipc/process_watch_dog.h"
#include "session/commands.pb.h"

// By default, mozc_renderer quits when user-input continues to be
// idle for 10min.
DEFINE_int32(timeout, 10 * 60, "timeout of candidate server (sec)");
DEFINE_bool(restricted, false,
            "launch candidates server with restricted mode");

namespace mozc {
namespace renderer {

namespace {
const int kNumConnections   = 10;
const int kIPCServerTimeOut = 1000;
const char kServiceName[]   = "renderer";

string GetServiceName() {
  string name = kServiceName;
  const string desktop_name = Util::GetDesktopNameAsString();
  if (!desktop_name.empty()) {
    name += ".";
    name += desktop_name;
  }
  return name;
}
}  // namespace

class ParentApplicationWatchDog : public ProcessWatchDog {
 public:
  explicit ParentApplicationWatchDog(RendererServer *renderer_server)
      : renderer_server_(renderer_server) {}
  virtual ~ParentApplicationWatchDog() {}

  void Signaled(ProcessWatchDog::SignalType type) {
    if (renderer_server_ == NULL) {
      LOG(ERROR) << "renderer_server is NULL";
      return;
    }
    if (type == ProcessWatchDog::PROCESS_SIGNALED ||
        type == ProcessWatchDog::THREAD_SIGNALED) {
      VLOG(1) << "Parent process is terminated: call Hide event";
      mozc::commands::RendererCommand command;
      command.set_type(mozc::commands::RendererCommand::UPDATE);
      command.set_visible(false);
      string *buf = new string;
      if (command.SerializeToString(buf)) {
        renderer_server_->AsyncExecCommand(buf);
      } else {
        LOG(ERROR) << "SerializeToString failed";
        delete buf;
      }
    }
  }

 private:
  RendererServer *renderer_server_;
};

class RendererServerSendCommand : public client::SendCommandInterface {
 public:
  RendererServerSendCommand() : receiver_handle_(0) {}
  virtual ~RendererServerSendCommand() {}

  bool SendCommand(const mozc::commands::SessionCommand &command,
                   mozc::commands::Output* output) {
#ifdef OS_WINDOWS
    if ((command.type() != commands::SessionCommand::SELECT_CANDIDATE) &&
        (command.type() != commands::SessionCommand::HIGHLIGHT_CANDIDATE) &&
        (command.type() != commands::SessionCommand::USAGE_STATS_EVENT)) {
      // Unsupported command.
      return false;
    }

    HWND target = reinterpret_cast<HWND>(receiver_handle_);
    if (target == NULL) {
      LOG(ERROR) << "target window is NULL";
      return false;
    }
    UINT mozc_msg =
        ::RegisterWindowMessageW(kMessageReceiverMessageName);
    if (mozc_msg == 0) {
      LOG(ERROR) << "RegisterWindowMessage failed: " << ::GetLastError();
      return false;
    }
    if (command.type() == mozc::commands::SessionCommand::USAGE_STATS_EVENT) {
      WPARAM type = static_cast<WPARAM>(command.type());
      LPARAM event = static_cast<LPARAM>(command.usage_stats_event());
      ::PostMessage(target, mozc_msg, type, event);
    } else {  // SELECT_CANDIDATE or HIGHLIGHT_CANDIDATE
      WPARAM type = static_cast<WPARAM>(command.type());
      LPARAM id = static_cast<LPARAM>(command.id());
      ::PostMessage(target, mozc_msg, type, id);
    }
#endif

    // TODO(all): implementation for Mac/Linux
    return true;
  }

  void set_receiver_handle(uint32 receiver_handle) {
    receiver_handle_ = receiver_handle;
  }

 private:
  uint32 receiver_handle_;
};

RendererServer::RendererServer()
    : IPCServer(GetServiceName(), kNumConnections, kIPCServerTimeOut),
      timeout_(0),
      renderer_interface_(NULL),
      watch_dog_(new ParentApplicationWatchDog(this)),
      send_command_(new RendererServerSendCommand) {
  if (FLAGS_restricted) {
    FLAGS_timeout = min(FLAGS_timeout, 60);   // set 60sec with restricted mode
  }

  timeout_ = 1000 * max(3, min(24 * 60 * 60, FLAGS_timeout));
  VLOG(2) << "timeout is set to be : " << timeout_;

#ifndef NO_LOGGING
  const config::Config &config = config::ConfigHandler::GetConfig();
  Logging::SetConfigVerboseLevel(config.verbose_level());
#endif  // NO_LOGGING

}

RendererServer::~RendererServer() {}

void RendererServer::SetRendererInterface(
    RendererInterface *renderer_interface) {
  renderer_interface_ = renderer_interface;
  if (renderer_interface_ != NULL) {
    renderer_interface_->SetSendCommandInterface(send_command_.get());
  }
}

int RendererServer::StartServer() {
  if (!Connected()) {
    LOG(ERROR) << "cannot start server";
    return -1;
  }

  LoopAndReturn();

  // send "ready" event to the client
  const string name = GetServiceName();
  NamedEventNotifier notifier(name.c_str());
  notifier.Notify();

  // start main event loop
  return StartMessageLoop();
}

bool RendererServer::Process(const char *request,
                             size_t request_size,
                             char *response,
                             size_t *response_size) {
  // here we just copy the serialized message in order
  // to reply to the client ui as soon as possible.
  // ParseFromString is executed in the main(another) thread.
  //
  // Since Process() and ExecCommand() are executed in
  // different threads, we have to use heap to share the serialized message.
  // If we use stack, this program will be crashed.
  //
  // The reciver of command_str takes the ownership of this string.
  string *command_str = new string(request, request_size);

  // no need to set the result code.
  *response_size = 1;
  response[0] = '\0';

  // Cannot call the method directly like renderer_interface_->ExecCommand()
  // as it's not thread-safe.
  return AsyncExecCommand(command_str);
}

bool RendererServer::ExecCommandInternal(
    const commands::RendererCommand &command) {
  if (renderer_interface_ == NULL) {
    LOG(ERROR) << "renderer_interface is NULL";
    return false;
  }

  VLOG(2) << command.DebugString();

  // Check process info if update mode
  if (command.type() == commands::RendererCommand::UPDATE){
    // set HWND of message-only window
    if (command.has_application_info() &&
        command.application_info().has_receiver_handle()) {
      send_command_->set_receiver_handle
          (command.application_info().receiver_handle());
    } else {
      LOG(WARNING) << "receiver_handle is not set";
    }

    // watch the parent application.
    if (command.has_application_info() &&
        command.application_info().has_process_id() &&
        command.application_info().has_thread_id()) {
      if (!watch_dog_->SetID
          (static_cast<ProcessWatchDog::ProcessID>
           (command.application_info().process_id()),
           static_cast<ProcessWatchDog::ThreadID>
           (command.application_info().thread_id()), -1)) {
        LOG(ERROR) << "Cannot set new ids for watch dog";
      }
    } else {
      LOG(WARNING) << "process id and thread id are not set";
    }
  }

  if (renderer_interface_->ExecCommand(command)) {
    return true;
  }

  return false;
}

uint32 RendererServer::timeout() const {
  return timeout_;
}
}  // renderer
}  // mozc
