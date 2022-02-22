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

#ifndef MOZC_RENDERER_QT_QT_SERVER_H_
#define MOZC_RENDERER_QT_QT_SERVER_H_

#include <memory>
#include <string>

#include "base/port.h"
#include "renderer/qt/qt_ipc_server.h"
#include "renderer/qt/qt_window_manager.h"
#include "absl/synchronization/mutex.h"

namespace mozc {
namespace renderer {

class QtServer {
 public:
  QtServer(int argc, char **argv);
  ~QtServer();

  void SetRenderer(QtWindowManager *renderer);

  // Enters the main event loop and waits UI events.
  // This method is basically initialize IPC server and
  // call StartMessageLoop() defined below.
  // The return value is suppose to be used for the arg of exit().
  int StartServer();

  bool AsyncExecCommand(std::string proto_message);

  void StartReceiverLoop();

 protected:
  int StartMessageLoop();

  // Call ExecCommandInternal() from the implementation
  // of AsyncExecCommand()
  bool ExecCommandInternal(const commands::RendererCommand &command);

  // return timeout (msec) passed by FLAGS_timeout
  uint32_t timeout() const;

  QtWindowManager *renderer_ = nullptr;

 private:
  int argc_;
  char **argv_;

  absl::Mutex mutex_;
  bool updated_;
  std::string message_;

  QtIpcServer ipc_;

  // From RendererServer
  uint32_t timeout_;

  DISALLOW_COPY_AND_ASSIGN(QtServer);
};

}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_QT_QT_SERVER_H_
