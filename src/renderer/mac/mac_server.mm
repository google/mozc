// Copyright 2010-2011, Google Inc.
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

#import <Cocoa/Cocoa.h>

#include <Carbon/Carbon.h>
#include <pthread.h>

#include "renderer/mac/mac_server.h"

#include "base/base.h"
#include "base/util.h"
#include "session/commands.pb.h"
#include "renderer/mac/mac_server_send_command.h"
#include "renderer/mac/CandidateController.h"

namespace mozc {
namespace renderer {
namespace mac {

namespace {
OSStatus EventHandler(EventHandlerCallRef handlerCallRef,
                      EventRef carbonEvent,
                      void *userData) {
  MacServer *server = reinterpret_cast<MacServer *>(userData);
  server->RunExecCommand();
  return noErr;
}
}

MacServer::MacServer()
    : controller_(NULL) {
  pthread_cond_init(&event_, NULL);
  EventHandlerUPP handler = ::NewEventHandlerUPP(EventHandler);
  EventTypeSpec spec[] = { { kEventClassApplication, 0 } };
  ::InstallEventHandler(GetApplicationEventTarget(), handler,
                        arraysize(spec), spec, this, NULL);
}

bool MacServer::AsyncExecCommand(string *proto_message) {
  {
    scoped_lock l(&mutex_);
    message_.swap(*proto_message);
    ::pthread_cond_signal(&event_);
  }
  delete proto_message;

  EventRef event_ref = NULL;
  ::CreateEvent(NULL, kEventClassApplication, 0, 0,
              kEventAttributeNone, &event_ref);
  ::PostEventToQueue(::GetMainEventQueue(), event_ref, kEventPriorityHigh);
  ::ReleaseEvent(event_ref);

  return true;
}

void MacServer::RunExecCommand() {
  string message;
  {
    scoped_lock l(&mutex_);
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
  RunApplicationEventLoop();
  return 0;
}

 void MacServer::Init() {
   NSApplicationLoad();
 }
}  // namespace mozc::renderer::mac
}  // namespace mozc::renderer
}  // namespace mozc
