// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_RENDERER_RENDERER_CLIENT_H_
#define MOZC_RENDERER_RENDERER_CLIENT_H_

#include <string>
#include "base/base.h"
#include "renderer/renderer_interface.h"

namespace mozc {

class IPCClientFactoryInterface;

namespace renderer {

class RendererLauncherInterface {
 public:
  enum RendererErrorType {
    RENDERER_VERSION_MISMATCH,
    RENDERER_FATAL
  };

  // implement StartRenderer
  virtual void StartRenderer(
      const string &name,
      const string &renderer_path,
      bool disable_renderer_path_check,
      IPCClientFactoryInterface *client_factory_intarface) = 0;

  // force to terminate the renderer
  // do not use this method unless protocol version mismatch
  virtual bool ForceTerminateRenderer(const string &name) = 0;

  // called when fatal error occured
  virtual void OnFatal(RendererErrorType type) = 0;

  // return true if the renderer is running
  virtual bool IsAvailable() const = 0;

  // return true if client can make a IPC connection.
  virtual bool CanConnect() const = 0;

  // |command| is sent to the server just after
  // renderer is launched.
  virtual void SetPendingCommand(
      const commands::RendererCommand &command) = 0;

  // Sets the flag of error dialog suppression.
  virtual void set_suppress_error_dialog(bool suppress) = 0;

  RendererLauncherInterface() {}
  virtual ~RendererLauncherInterface() {}
};

// IPC-based client for out-proc renderer.
class RendererClient : public RendererInterface {
 public:
  RendererClient();
  virtual ~RendererClient();

  // set IPC factory
  void SetIPCClientFactory(IPCClientFactoryInterface
                           *ipc_client_factory_interface);

  // set StartRendererInterface
  void SetRendererLauncherInterface(RendererLauncherInterface
                                    *renderer_launcher_interface);

  // send_command_interface is not used in the client.
  // Currently, mouse handling must be implemented in each
  // platform separately.
  // TODO(taku): move win32 code into RendererClient
  void SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface) {}

  // activate renderer server
  bool Activate();

  // return true if the renderer is available
  bool IsAvailable() const;

  // shutdown the renderer if it is running
  // return true if the function finishes without error.
  // If force is true ForceTerminateRenderer is used to shutdown the renderer.
  // Otherwise command::RendererCommand::SHUDDOWN is used.
  bool Shutdown(bool force);

  bool ExecCommand(const commands::RendererCommand &command);

  // Don't check the renderer server path.
  // DO NOT call it except for testing
  void DisableRendererServerCheck();

  // Sets the flag of error dialog suppression.
  void set_suppress_error_dialog(bool suppress);

 private:
  bool is_window_visible_;
  bool disable_renderer_path_check_;
  int  version_mismatch_nums_;
  string name_;
  string renderer_path_;

  IPCClientFactoryInterface *ipc_client_factory_interface_;

  scoped_ptr<RendererLauncherInterface> renderer_launcher_;
  RendererLauncherInterface *renderer_launcher_interface_;
};
}  // renderer
}  // mozc
#endif  // MOZC_RENDERER_RENDERER_INTERFACE_H_
