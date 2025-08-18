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

#include "renderer/win32/win32_server.h"

#include <windows.h>

#include <memory>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/vlog.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/window_manager.h"

namespace mozc {
namespace renderer {
namespace {

bool IsTSFMessage(const commands::RendererCommand& command) {
  if (!command.has_application_info()) {
    return false;
  }
  if (!command.application_info().has_input_framework()) {
    return false;
  }
  return (command.application_info().input_framework() ==
          commands::RendererCommand::ApplicationInfo::TSF);
}

}  // namespace

namespace win32 {

Win32Server::Win32Server()
    : event_(nullptr), window_manager_(new WindowManager) {
  // Manual reset event to notify we have a renderer command
  // to be handled in the UI thread.
  // The renderer command is serialized into "message_".
  event_ = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
  DCHECK_NE(nullptr, event_)
      << "CreateEvent failed, Error = " << ::GetLastError();
}

Win32Server::~Win32Server() { ::CloseHandle(event_); }

void Win32Server::AsyncHide() {
  {
    // Cancel the remaining event
    absl::MutexLock l(&mutex_);
    ::ResetEvent(event_);
  }
  window_manager_->AsyncHideAllWindows();
}

void Win32Server::AsyncQuit() {
  {
    // Cancel the remaining event
    absl::MutexLock l(&mutex_);
    ::ResetEvent(event_);
  }
  window_manager_->AsyncQuitAllWindows();
}

bool Win32Server::Activate() {
  // TODO(yukawa): Implement this.
  return true;
}

bool Win32Server::IsAvailable() const {
  // TODO(yukawa): Implement this.
  return true;
}

bool Win32Server::ExecCommand(const commands::RendererCommand& command) {
  MOZC_VLOG(2) << command;

  switch (command.type()) {
    case commands::RendererCommand::NOOP:
      break;
    case commands::RendererCommand::SHUTDOWN:
      // Do not destroy windows here.
      window_manager_->HideAllWindows();
      break;
    case commands::RendererCommand::UPDATE:
      if (!command.visible()) {
        window_manager_->HideAllWindows();
      } else if (IsTSFMessage(command)) {
        window_manager_->UpdateLayout(command);
      } else {
        LOG(WARNING) << "output/left/bottom are not set";
      }
      break;
    default:
      LOG(WARNING) << "Unknown command: " << command.type();
      break;
  }
  return true;
}

void Win32Server::SetSendCommandInterface(
    client::SendCommandInterface* send_command_interface) {
  window_manager_->SetSendCommandInterface(send_command_interface);
}

bool Win32Server::AsyncExecCommand(absl::string_view proto_message) {
  // Take the ownership of |proto_message|.
  absl::MutexLock l(&mutex_);
  if (message_ == proto_message) {
    // This is exactly the same to the previous message. Theoretically it is
    // safe to do nothing here.
    return true;
  }
  // Since mozc rendering protocol is state-less, we can always ignore the
  // previous content of |message_|.
  message_.assign(proto_message.data(), proto_message.size());
  // Set the event signaled to mark we have a message to render.
  ::SetEvent(event_);
  return true;
}

int Win32Server::StartMessageLoop() {
  window_manager_->Initialize();

  int return_code = 0;

  while (true) {
    // WindowManager::IsAvailable() returns false at least one window does not
    // have a valid window handle.
    // - WindowManager::Initialize() somehow failed.
    // - A window is closed as a result of WM_CLOSE sent from an external
    //   process. This may happen if the shell or restart manager wants to shut
    //   down the renderer.
    if (!window_manager_->IsAvailable()) {
      // Mark this thread to quit.
      ::PostQuitMessage(0);
      break;  // exit message pump.
    }

    // Wait for the next window message or next rendering message.
    const DWORD wait_result =
        ::MsgWaitForMultipleObjects(1, &event_, FALSE, INFINITE, QS_ALLINPUT);
    if (wait_result == WAIT_OBJECT_0) {
      // "event_" is signaled so that we have to handle the renderer command
      // stored in "message_"
      std::string message;
      {
        absl::MutexLock l(&mutex_);
        message.assign(message_);
        ::ResetEvent(event_);
      }
      commands::RendererCommand command;
      if (command.ParseFromString(message)) {
        ExecCommandInternal(command);
        if (command.type() == commands::RendererCommand::SHUTDOWN) {
          break;  // exit message pump.
        }
      } else {
        LOG(ERROR) << "ParseFromString failed";
      }
    } else if (wait_result == WAIT_OBJECT_0 + 1) {
      // We have at least one window message. Let's handle them.
      while (true) {
        MSG msg = {};
        if (::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) == 0) {
          // No more message.
          break;  // exit message pump.
        }
        if (msg.message == WM_QUIT) {
          return_code = msg.wParam;
          MOZC_VLOG(0) << "Reveiced WM_QUIT.";
          break;  // exit message pump.
        }
        window_manager_->PreTranslateMessage(msg);
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
      }
    } else if (wait_result == WAIT_ABANDONED_0) {
      LOG(INFO) << "WAIT_ABANDONED_0";
    } else {
      LOG(ERROR) << "MsgWaitForMultipleObjects returned unexpected result: "
                 << wait_result;
    }
  }

  // Ensure that IPC thread is terminated.
  // TODO(yukawa): Update the IPC server so that we can set a timeout here.
  Terminate();

  // Make sure all the windows are closed.
  // WindowManager::DestroyAllWindows supports multiple calls on the UI thread.
  window_manager_->DestroyAllWindows();
  return return_code;
}
}  // namespace win32
}  // namespace renderer
}  // namespace mozc
