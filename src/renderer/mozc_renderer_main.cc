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
#endif  // OS_WINDOWS

#include "base/base.h"
#include "base/run_level.h"
#include "base/util.h"

#ifdef OS_WINDOWS
#include "base/winmain.h"
#include "base/win_util.h"
#include "renderer/win32/win32_server.h"
#elif defined(OS_MACOSX)
#include "renderer/mac/mac_server.h"
#include "renderer/mac/mac_server_send_command.h"
#include "renderer/mac/CandidateController.h"
#endif  // OS_WINDOWS, OS_MACOSX

DECLARE_bool(restricted);

int main(int argc, char *argv[]) {
  const mozc::RunLevel::RunLevelType run_level =
      mozc::RunLevel::GetRunLevel(mozc::RunLevel::RENDERER);

  if (run_level >= mozc::RunLevel::DENY) {
    return -1;
  }

#ifdef OS_WINDOWS
  mozc::ScopedCOMInitializer com_initializer;
#endif  // OS_WINDOWS

  mozc::Util::DisableIME();

  // restricted mode
  if (run_level == mozc::RunLevel::RESTRICTED) {
    FLAGS_restricted = true;
  }

  InitGoogleWithBreakPad(argv[0], &argc, &argv, false);

  int result_code = 0;

#ifdef OS_WINDOWS
  mozc::renderer::win32::Win32Server server;
  server.SetRendererInterface(&server);
  result_code = server.StartServer();
#elif defined(OS_MACOSX)
  mozc::renderer::mac::MacServer server;
  mozc::renderer::mac::CandidateController renderer;
  mozc::renderer::mac::MacServerSendCommand send_command;
  server.SetRendererInterface(&renderer);
  renderer.SetSendCommandInterface(&send_command);
  result_code = server.StartServer();
#endif  // OS_WINDOWS, OS_MACOSX

  return result_code;
}
