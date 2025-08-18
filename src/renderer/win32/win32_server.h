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

#ifndef MOZC_RENDERER_WIN32_WIN32_SERVER_H_
#define MOZC_RENDERER_WIN32_WIN32_SERVER_H_

#include <windows.h>

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "renderer/renderer_interface.h"
#include "renderer/renderer_server.h"
#include "renderer/win32/window_manager.h"

namespace mozc {
namespace renderer {
namespace win32 {

// This is an implementation class of UI-renderer server based on
// Win32 Event object and Win32 window messages.
// Primary role of this class is to safely marshal async renderer
// events into UI thread.
// This class also implements RendererInterface to receive a handler
// to callback mouse events.
// Actual window management is delegated to WindowManager class.
class Win32Server : public RendererServer, public RendererInterface {
 public:
  Win32Server();

  Win32Server(const Win32Server&) = delete;
  Win32Server& operator=(const Win32Server&) = delete;

  ~Win32Server() override;

  void AsyncHide() override;
  void AsyncQuit() override;
  bool Activate() override;
  bool IsAvailable() const override;
  bool ExecCommand(const commands::RendererCommand& command) override;
  void SetSendCommandInterface(
      client::SendCommandInterface* send_command_interface) override;
  bool AsyncExecCommand(absl::string_view proto_message) override;
  int StartMessageLoop() override;

 private:
  std::string message_;
  absl::Mutex mutex_;
  HANDLE event_;
  std::unique_ptr<WindowManager> window_manager_;
};

}  // namespace win32
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_WIN32_WIN32_SERVER_H_
