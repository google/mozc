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

#ifndef MOZC_RENDERER_QT_QT_IPC_SERVER_H_
#define MOZC_RENDERER_QT_QT_IPC_SERVER_H_

#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "base/port.h"
#include "absl/strings/string_view.h"
#include "ipc/ipc.h"

namespace mozc {
namespace renderer {

class QtIpcServer : public IPCServer {
 public:
  using Callback = std::function<void(std::string)>;

  QtIpcServer();
  QtIpcServer(const QtIpcServer&) = delete;
  QtIpcServer& operator=(const QtIpcServer&) = delete;
  ~QtIpcServer() override;

  bool Process(absl::string_view request, std::string *response) override;

  void SetCallback(Callback callback) { callback_ = std::move(callback); }

 private:
  Callback callback_;
};

}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_QT_QT_IPC_SERVER_H_
