// Copyright 2010-2011, Google Inc.
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

#include "base/base.h"
#include "base/run_level.h"
#include "base/util.h"
#include "renderer/win32/window_manager.h"
#include "session/commands.pb.h"

namespace mozc {
namespace renderer {
namespace {
bool IsIMM32Message(const commands::RendererCommand &command) {
  if (!command.has_application_info()) {
    return false;
  }
  if (!command.application_info().has_input_framework()) {
    return false;
  }
  if (command.application_info().input_framework() !=
      commands::RendererCommand::ApplicationInfo::IMM32) {
    return false;
  }
  return true;
}
}  // anonymous namespace

namespace win32 {

Win32Server::Win32Server()
    : event_(NULL),
      window_manager_(new WindowManager) {
  // Manual reset event to notify we have a renderer command
  // to be handled in the UI thread.
  // The renderer command is serialized into "message_".
  event_ = ::CreateEvent(NULL, FALSE, FALSE, NULL);
  DCHECK(event_ != NULL) << "CreateEvent failed, Error = " << ::GetLastError();
}

Win32Server::~Win32Server() {
  ::CloseHandle(event_);
}

void Win32Server::AsyncHide() {
  {
    // Cancel the remaining event
    scoped_lock l(&mutex_);
    ::ResetEvent(event_);
  }
  window_manager_->AsyncHideAllWindows();
}

void Win32Server::AsyncQuit() {
  {
    // Cancel the remaining event
    scoped_lock l(&mutex_);
    ::ResetEvent(event_);
  }
  window_manager_->AsyncQuitAllWindows();
}

bool Win32Server::Activate() {
  return window_manager_->Activate();
}

bool Win32Server::IsAvailable() const {
  return window_manager_->IsAvailable();
}

bool Win32Server::ExecCommand(const commands::RendererCommand &command) {
  VLOG(2) << command.DebugString();

  switch (command.type()) {
    case commands::RendererCommand::NOOP:
      break;
    case commands::RendererCommand::SHUTDOWN:
      window_manager_->DestroyAllWindows();
      break;
    case commands::RendererCommand::UPDATE:
      if (!command.visible()) {
        window_manager_->HideAllWindows();
      } else if (IsIMM32Message(command)) {
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
    client::SendCommandInterface *send_command_interface) {
  window_manager_->SetSendCommandInterface(send_command_interface);
}

bool Win32Server::AsyncExecCommand(string *proto_message) {
  {
    scoped_lock l(&mutex_);

    // Since mozc rendering protocol is state-less,
    // we can always discard the old message.
    message_.assign(*proto_message);

    // Set the event signaled to mark we have a message to render.
    ::SetEvent(event_);
  }
  delete proto_message;

  return true;
}

int Win32Server::StartMessageLoop() {
  window_manager_->Initialize();

  MSG msg;
  ZeroMemory(&msg, sizeof(msg));

  while (msg.message != WM_QUIT) {
    // Wait for the next window message or next rendering message.
    const DWORD ret =
      ::MsgWaitForMultipleObjects(1, &event_, FALSE, INFINITE, QS_ALLINPUT);

    if (ret == WAIT_OBJECT_0) {
      // "event_" is signaled so that we have to handle the renderer command
      // stored in "message_"
      string message;
      {
        scoped_lock l(&mutex_);
        message.assign(message_);
        ::ResetEvent(event_);
      }
      commands::RendererCommand command;
      if (command.ParseFromString(message)) {
        ExecCommandInternal(command);
      } else {
        LOG(WARNING) << "ParseFromString failed";
      }
    } else if (ret == WAIT_OBJECT_0 + 1) {
      // We have one or many window messages. Let's handle them.
      ZeroMemory(&msg, sizeof(msg));
      while ((::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0) &&
             (msg.message != WM_QUIT)) {
        window_manager_->PreTranslateMessage(msg);
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
      }
    } else if (ret == WAIT_ABANDONED_0) {
      LOG(INFO) << "WAIT_ABANDONED_0";
    } else {
      LOG(ERROR) << "Unexpected result";
    }
  }

  // Make sure to close all windows.
  // WindowManager::DestroyAllWindows supports multiple calls on the UI thread.
  window_manager_->DestroyAllWindows();
  return msg.wParam;
}
}  // win32
}  // renderer
}  // mozc
