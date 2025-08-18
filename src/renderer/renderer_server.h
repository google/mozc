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

#ifndef MOZC_RENDERER_RENDERER_SERVER_H_
#define MOZC_RENDERER_RENDERER_SERVER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "ipc/ipc.h"
#include "ipc/process_watch_dog.h"
#include "renderer/renderer_interface.h"

namespace mozc {
namespace renderer {

class RendererServerSendCommand;

// RendererServer base class. Implement Async* method.
class RendererServer : public IPCServer {
 public:
  RendererServer();
  RendererServer(const RendererServer&) = delete;
  RendererServer& operator=(const RendererServer&) = delete;
  ~RendererServer() override;

  void SetRendererInterface(RendererInterface* renderer_interface);

  // Enters the main event loop and waits UI events.
  // This method is basically initialize IPC server and
  // call StartMessageLoop() defined below.
  // The return value is suppose to be used for the arg of exit().
  int StartServer();

  bool Process(absl::string_view request, std::string* response) override;

  // DEPRECATED: this functions is never called
  virtual void AsyncHide() {}

  // DEPRECATED: this functions is never called
  virtual void AsyncQuit() {}

  // Exec command.
  // We don't pass protocol buffer but pass a serialized (raw)
  // protocol buffer received in Process method so that
  // IPC listener thread can reply to the client request as
  // early as possible.
  virtual bool AsyncExecCommand(absl::string_view proto_message) = 0;

 protected:
  // implement Message Loop function.
  // This function should be blocking.
  // The return value is supposed to be used for the arg of exit().
  virtual int StartMessageLoop() = 0;

  // Call ExecCommandInternal() from the implementation
  // of AsyncExecCommand()
  bool ExecCommandInternal(const commands::RendererCommand& command);

  // return timeout (msec) passed by FLAGS_timeout
  uint32_t timeout() const;

  RendererInterface* renderer_interface_ = nullptr;

 private:
  uint32_t timeout_;
  std::unique_ptr<ProcessWatchDog> watch_dog_;
  std::unique_ptr<RendererServerSendCommand> send_command_;
};

}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_RENDERER_SERVER_H_
