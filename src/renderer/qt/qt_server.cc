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

#include "renderer/qt/qt_server.h"

#include <memory>

#include "base/logging.h"
#include "protocol/renderer_command.pb.h"
#include "absl/synchronization/mutex.h"

using std::unique_ptr;

namespace mozc {
namespace renderer {

QtServer::QtServer(int argc, char **argv)
    : argc_(argc), argv_(argv), updated_(false) {}

bool QtServer::AsyncExecCommand(std::string *proto_message) {
  // Take the ownership of |proto_message|.
  unique_ptr<std::string> proto_message_owner(proto_message);
  {
    absl::MutexLock l(&mutex_);
    if (message_ == *proto_message_owner.get()) {
      // This is exactly the same to the previous message. Theoretically it is
      // safe to do nothing here.
      return true;
    }

    // Since mozc rendering protocol is state-less, we can always ignore the
    // previous content of |message_|.
    message_.swap(*proto_message_owner.get());
    updated_ = true;
  }

  return true;
}

namespace {
bool cond_func(bool *cond_var) {
  return *cond_var;
}
}  // namespace

void QtServer::StartReceiverLoop() {
  while (true) {
    mutex_.LockWhen(absl::Condition(cond_func, &updated_));
    std::string message(message_);
    updated_ = false;
    mutex_.Unlock();

    commands::RendererCommand command;
    if (command.ParseFromString(message)) {
      ExecCommandInternal(command);
    } else {
      LOG(WARNING) << "Parse From String Failed";
    }
  }
}

int QtServer::StartMessageLoop() {
  ReceiverLoopFunc receiver_loop_func = [&](){ StartReceiverLoop(); };
  renderer_interface_->SetReceiverLoopFunction(receiver_loop_func);
  renderer_interface_->StartRendererLoop(argc_, argv_);
  return 0;
}

}  // namespace renderer
}  // namespace mozc
