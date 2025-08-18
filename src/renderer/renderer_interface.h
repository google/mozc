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

#ifndef MOZC_RENDERER_RENDERER_INTERFACE_H_
#define MOZC_RENDERER_RENDERER_INTERFACE_H_

#include "client/client_interface.h"
#include "protocol/renderer_command.pb.h"

namespace mozc {
namespace renderer {

// An abstract interface class for renderer
class RendererInterface {
 public:
  RendererInterface() = default;
  virtual ~RendererInterface() = default;

  // Activate candidate window.
  // For instance, if the renderer is out-proc renderer,
  // Activate can launch renderer process.
  // Activate must not have any visible change.
  // If the renderer is already activated, this method does nothing
  // and return false.
  virtual bool Activate() = 0;

  // return true if the renderer is available
  virtual bool IsAvailable() const = 0;

  // exec stateless rendering command
  // TODO(taku): RendererCommand should be stateless.
  virtual bool ExecCommand(const commands::RendererCommand& command) = 0;

  // set mouse callback handler.
  // default implementation is empty
  virtual void SetSendCommandInterface(
      client::SendCommandInterface* send_command_interface) {}
};
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_RENDERER_INTERFACE_H_
