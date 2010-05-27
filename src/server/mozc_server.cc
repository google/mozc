// Copyright 2010, Google Inc.
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

#ifdef OS_WINDOWS
#include <windows.h>
#endif

#include "base/base.h"
#include "base/process.h"
#include "base/process_mutex.h"
#include "base/run_level.h"
#include "base/singleton.h"
#include "base/util.h"
#include "ipc/ipc.h"
#include "session/session_server.h"

DECLARE_bool(restricted);   // in SessionHandler

namespace mozc {
namespace {

// return singleton object of SessionServer
SessionServer *GetSessionServer() {
  return Singleton<SessionServer>::get();
}

// When OS is about to shutdown/logoff,
// ShutdownSessionCallback is kicked.
void ShutdownSessionCallback() {
  VLOG(1) << "ShutdownSessionFunc is called";
  GetSessionServer()->Terminate();
}

REGISTER_MODULE_SHUTDOWN_HANDLER(shudown_session,
                                 ShutdownSessionCallback());
}  // namespace
}  // mozc

int main(int argc, char *argv[]) {
  mozc::Util::DisableIME();

  // Big endian is not supported. The storage for user history is endian
  // dependent. If we want to sync the data via network sync feature, we
  // will see some problems.
  CHECK(mozc::Util::IsLittleEndian()) << "Big endian is not supported.";

#ifdef OS_WINDOWS
  // http://msdn.microsoft.com/en-us/library/ms686227.aspx
  // Make sure that mozc_server exits all after other processes.
  ::SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY);
#endif

  // call GetRunLevel before InitGoogle().
  // InitGoogle() will do all static initialization and may access
  // local resources.
  const mozc::RunLevel::RunLevelType run_level =
      mozc::RunLevel::GetRunLevel(mozc::RunLevel::SERVER);

  if (run_level >= mozc::RunLevel::DENY) {
    return -1;
  }

  InitGoogleWithBreakPad(argv[0], &argc, &argv, false);

  mozc::ProcessMutex mutex("server");
  if (!mutex.Lock()) {
    LOG(INFO) << "Mozc Server is already running";
    return -1;
  }

  if (run_level == mozc::RunLevel::RESTRICTED) {
    VLOG(1) << "Mozc server starts with timeout mode";
    FLAGS_restricted = true;
  }

  mozc::SessionServer *session_server = mozc::GetSessionServer();
  CHECK(session_server);

  if (!session_server->Connected()) {
    LOG(ERROR) << "SessionServer initialization failed";
    return -1;
  }

  session_server->LoopAndReturn();
  session_server->Wait();

  // cleanup all static instances (like singleton)
  mozc::RunFinalizers();

  return 0;
}
