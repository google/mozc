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

// Session Server

#ifndef MOZC_SESSION_SESSION_SERVER_H_
#define MOZC_SESSION_SESSION_SERVER_H_

#include <string>
#include "base/base.h"
#include "ipc/ipc.h"
#include "session/session_handler.h"

class SessionHandlerInterface;

namespace mozc {

namespace session {
class SessionUsageObserver;
}

// Session IPC Server
// Usage:
// SessionServer server;
// server.Loop();   // <- Falls into infinite Loop
//
// or
//
// server.LoopAndReturn();   // make a thread
// ..
// server.Wait();
class SessionServer: public IPCServer {
 public:
  SessionServer();
  virtual ~SessionServer();

  virtual bool Connected() const;

  virtual bool Process(const char *request,
                       size_t request_size,
                       char *response,
                       size_t *response_size);

 private:
  scoped_ptr<SessionHandlerInterface> handler_;
  scoped_ptr<session::SessionUsageObserver> usage_observer_;
  DISALLOW_COPY_AND_ASSIGN(SessionServer);
};
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_SERVER_H_
