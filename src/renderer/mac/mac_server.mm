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

#import "renderer/mac/mac_server.h"

#import <Cocoa/Cocoa.h>

#include <Carbon/Carbon.h>
#include <pthread.h>

#include <iterator>
#include <string>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/util.h"
#include "protocol/commands.pb.h"
#include "renderer/mac/CandidateController.h"
#include "renderer/mac/mac_server_send_command.h"

namespace mozc {
namespace renderer {
namespace mac {

namespace {
OSStatus EventHandler(EventHandlerCallRef handlerCallRef, EventRef carbonEvent, void *userData) {
  MacServer *server = reinterpret_cast<MacServer *>(userData);
  server->RunExecCommand();
  return noErr;
}
}  // namespace

MacServer::MacServer(int argc, const char **argv) : argc_(argc), argv_(argv) {
  pthread_cond_init(&event_, nullptr);
  EventHandlerUPP handler = ::NewEventHandlerUPP(EventHandler);
  EventTypeSpec spec[] = {{kEventClassApplication, 0}};
  ::InstallEventHandler(GetApplicationEventTarget(), handler, std::size(spec), spec, this, nullptr);
}

bool MacServer::AsyncExecCommand(absl::string_view proto_message) {
  {
    absl::MutexLock l(&mutex_);
    message_.assign(proto_message.data(), proto_message.size());
    ::pthread_cond_signal(&event_);
  }

  EventRef event_ref = nullptr;
  ::CreateEvent(nullptr, kEventClassApplication, 0, 0, kEventAttributeNone, &event_ref);
  ::PostEventToQueue(::GetMainEventQueue(), event_ref, kEventPriorityHigh);
  ::ReleaseEvent(event_ref);

  return true;
}

void MacServer::RunExecCommand() {
  std::string message;
  {
    absl::MutexLock l(&mutex_);
    message.swap(message_);
  }
  commands::RendererCommand command;
  if (!command.ParseFromString(message)) {
    LOG(ERROR) << "ParseFromString failed";
    return;
  }

  ExecCommandInternal(command);
}

int MacServer::StartMessageLoop() {
  NSApplicationMain(argc_, argv_);
  return 0;
}

void MacServer::Init() { NSApplicationLoad(); }
}  // namespace mac
}  // namespace renderer
}  // namespace mozc
