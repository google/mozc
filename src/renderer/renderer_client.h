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

#ifndef MOZC_RENDERER_RENDERER_CLIENT_H_
#define MOZC_RENDERER_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "client/client_interface.h"
#include "ipc/ipc.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/renderer_interface.h"

namespace mozc {

namespace renderer {

class RendererLauncherInterface {
 public:
  enum RendererErrorType { RENDERER_VERSION_MISMATCH, RENDERER_FATAL };

  virtual ~RendererLauncherInterface() = default;

  // implement StartRenderer
  virtual void StartRenderer(
      const std::string& name, const std::string& renderer_path,
      bool disable_renderer_path_check,
      IPCClientFactoryInterface* client_factory_intarface) = 0;

  // force to terminate the renderer
  // do not use this method unless protocol version mismatch
  virtual bool ForceTerminateRenderer(const std::string& name) = 0;

  // called when fatal error occurred
  virtual void OnFatal(RendererErrorType type) = 0;

  // return true if the renderer is running
  virtual bool IsAvailable() const = 0;

  // return true if client can make a IPC connection.
  virtual bool CanConnect() const = 0;

  // |command| is sent to the server just after
  // renderer is launched.
  virtual void SetPendingCommand(const commands::RendererCommand& command) = 0;

  // Sets the flag of error dialog suppression.
  virtual void set_suppress_error_dialog(bool suppress) = 0;
};

// IPC-based client for out-proc renderer.
class RendererClient final : public RendererInterface {
 public:
  enum class RendererPathCheckMode {
    ENABLED,
    DISABLED,
  };

  static std::unique_ptr<RendererClient> Create();

  static std::unique_ptr<RendererClient> CreateForTesting(
      IPCClientFactoryInterface* absl_nullable ipc_client_factory_for_testing,
      RendererLauncherInterface* absl_nullable renderer_launcher_for_testing,
      RendererPathCheckMode renderer_path_check_mode);

  ~RendererClient() override;

  // activate renderer server
  bool Activate() override;

  // return true if the renderer is available
  bool IsAvailable() const override;

  // shutdown the renderer if it is running
  // return true if the function finishes without error.
  // If force is true ForceTerminateRenderer is used to shutdown the renderer.
  // Otherwise command::RendererCommand::SHUDDOWN is used.
  bool Shutdown(bool force);

  bool ExecCommand(const commands::RendererCommand& command) override;

  // Sets the flag of error dialog suppression.
  void set_suppress_error_dialog(bool suppress);

 private:
  RendererClient(
      std::string name,
      IPCClientFactoryInterface* absl_nullable ipc_client_factory_for_testing,
      RendererLauncherInterface* absl_nullable renderer_launcher_for_testing,
      bool disable_renderer_path_check_for_testing);

  IPCClientFactoryInterface* absl_nonnull GetIPCClientFactory() const;
  RendererLauncherInterface* absl_nonnull GetRendererLauncher() const;

  std::unique_ptr<IPCClientInterface> CreateIPCClient() const;

  bool is_window_visible_;
  int version_mismatch_nums_;
  const std::string name_;
  std::unique_ptr<RendererLauncherInterface> default_renderer_launcher_;

  // Behavior overrides for testing
  IPCClientFactoryInterface* const absl_nullable
      ipc_client_factory_for_testing_;
  RendererLauncherInterface* const absl_nullable renderer_launcher_for_testing_;
  const bool disable_renderer_path_check_for_testing_;
};

}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_RENDERER_CLIENT_H_
