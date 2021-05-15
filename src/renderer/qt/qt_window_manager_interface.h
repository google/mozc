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

#ifndef MOZC_RENDERER_QT_QT_WINDOW_MANAGER_INTERFACE_H_
#define MOZC_RENDERER_QT_QT_WINDOW_MANAGER_INTERFACE_H_

namespace mozc {

namespace client {
class SendCommandInterface;
}  // namespace client

namespace commands {
class RendererCommand;
}  // namespace commands

namespace renderer {

class QtWindowManagerInterface {
 public:
  QtWindowManagerInterface() = default;
  virtual ~QtWindowManagerInterface() = default;

  virtual int StartRendererLoop(int argc, char **argv) = 0;

  virtual void Initialize() = 0;
  virtual void HideAllWindows() = 0;
  virtual void ShowAllWindows() = 0;
  virtual void UpdateLayout(const commands::RendererCommand &command) = 0;
  virtual bool Activate() = 0;
  virtual bool IsAvailable() const = 0;
  // WindowManager does NOT takes ownership of send_command_interface.
  virtual bool SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface) = 0;
  virtual void SetWindowPos(int x, int y) = 0;
};

}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_QT_QT_WINDOW_MANAGER_INTERFACE_H_
