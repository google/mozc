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

#include "renderer/unix/unix_renderer.h"

#include "renderer/renderer_command.pb.h"

namespace mozc {
namespace renderer {
namespace gtk {

UnixRenderer::UnixRenderer(WindowManagerInterface *window_manager)
    : window_manager_(window_manager) {
}

UnixRenderer::~UnixRenderer() {
}

bool UnixRenderer::Activate() {
  return window_manager_->Activate();
}

bool UnixRenderer::IsAvailable() const {
  return window_manager_->IsAvailable();
}

bool UnixRenderer::ExecCommand(const commands::RendererCommand &command) {
  switch (command.type()) {
    case commands::RendererCommand::NOOP:
      break;
    case commands::RendererCommand::SHUTDOWN:
      // TODO(nona): Implement shutdown command.
      DLOG(ERROR) << "Shutdown command is not implemented.";
      return false;
      break;
    case commands::RendererCommand::UPDATE:
      if (!command.visible()) {
        window_manager_->HideAllWindows();
      } else {
        window_manager_->UpdateLayout(command);
      }
      return true;
      break;
    default:
      LOG(WARNING) << "Unknown command: " << command.type();
      break;
  }
  return true;
}

void UnixRenderer::Initialize() {
  window_manager_->Initialize();
}

void UnixRenderer::SetSendCommandInterface(
    client::SendCommandInterface *send_command_interface) {
  window_manager_->SetSendCommandInterface(send_command_interface);
}

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
