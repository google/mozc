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

#ifndef THIRD_PARTY_MOZC_SRC_RENDERER_MAC_MAC_SERVER_H_
#define THIRD_PARTY_MOZC_SRC_RENDERER_MAC_MAC_SERVER_H_

#include <pthread.h>

#include <memory>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "renderer/renderer_interface.h"
#include "renderer/renderer_server.h"

namespace mozc {
namespace renderer {
namespace mac {

class CandidateController;

// This is an implementation class of UI-renderer server based on
// Mac Cocoa/Carbon framework.
// Actual window management is delegated to CandidateController class.
class MacServer : public RendererServer {
 public:
  MacServer(int argc, const char **argv);

  MacServer(const MacServer &) = delete;
  MacServer &operator=(const MacServer &) = delete;

  ~MacServer() override = default;

  bool AsyncExecCommand(absl::string_view proto_message) override;
  int StartMessageLoop() override;

  // This method is called when an asynchronous exec-command message
  // arrives.  This is public because it will be called from carbon
  // event handler.  You should not call this directly.
  void RunExecCommand();

  // Initialize the application status.
  static void Init();

 private:
  absl::Mutex mutex_;
  pthread_cond_t event_;
  std::string message_;
  std::unique_ptr<CandidateController> controller_;
  int argc_;
  const char **argv_;
};

}  // namespace mac
}  // namespace renderer
}  // namespace mozc

#endif  // THIRD_PARTY_MOZC_SRC_RENDERER_MAC_MAC_SERVER_H_
