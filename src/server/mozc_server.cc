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

#include "server/mozc_server.h"

#include <bit>
#include <memory>
#include <string>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "base/init_mozc.h"
#include "base/process_mutex.h"
#include "base/run_level.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/vlog.h"
#include "session/session_server.h"

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

ABSL_DECLARE_FLAG(bool, restricted);  // in SessionHandler

namespace {
mozc::SessionServer *g_session_server = nullptr;
}

namespace mozc {
namespace server {

void InitMozcAndMozcServer(const char *arg0, int *argc, char ***argv,
                           bool remove_flags) {
  mozc::SystemUtil::DisableIME();

  // Big endian is not supported. The storage for user history is endian
  // dependent. If we want to sync the data via network sync feature, we
  // will see some problems.
  static_assert(std::endian::native == std::endian::little,
                "Big endian is not supported.");
#ifdef _WIN32
  // http://msdn.microsoft.com/en-us/library/ms686227.aspx
  // Make sure that mozc_server exits all after other processes.
  ::SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY);
#endif  // _WIN32

  // call GetRunLevel before mozc::InitMozc().
  // mozc::InitMozc() will do all static initialization and may access
  // local resources.
  const mozc::RunLevel::RunLevelType run_level =
      mozc::RunLevel::GetRunLevel(mozc::RunLevel::SERVER);

  if (run_level >= mozc::RunLevel::DENY) {
    LOG(FATAL) << "Do not execute Mozc server as high authority";
    return;
  }

  mozc::InitMozc(arg0, argc, argv);

  if (run_level == mozc::RunLevel::RESTRICTED) {
    MOZC_VLOG(1) << "Mozc server starts with timeout mode";
    absl::SetFlag(&FLAGS_restricted, true);
  }
}

int MozcServer::Run() {
  std::string mutex_name = "server";
  mozc::ProcessMutex mutex(mutex_name);
  if (!mutex.Lock()) {
    LOG(INFO) << "Mozc Server is already running";
    return -1;
  }

  {
    std::unique_ptr<mozc::SessionServer> session_server(
        new mozc::SessionServer);
    g_session_server = session_server.get();
    CHECK(g_session_server);
    if (!g_session_server->Connected()) {
      LOG(ERROR) << "SessionServer initialization failed";
      return -1;
    }

#if defined(_WIN32)
    // On Windows, ShutdownSessionCallback is not called intentionally in order
    // to avoid crashes oritinates from it. See b/2696087.
    g_session_server->Loop();
#else   // defined(_WIN32)
    // Create a new thread.
    // We can't call Loop() as Loop() doesn't make a thread.
    // We have to make a thread here so that ShutdownSessionCallback()
    // is called properly.
    g_session_server->LoopAndReturn();

    // Wait until the session server thread finishes.
    g_session_server->Wait();
#endif  // defined(_WIN32)
  }

  return 0;
}

int MozcServer::Finalize() {
  mozc::FinalizeSingletons();
  return 0;
}

}  // namespace server
}  // namespace mozc
