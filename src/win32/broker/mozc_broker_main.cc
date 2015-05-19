// Copyright 2010-2012, Google Inc.
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
#include "base/util.h"

#ifdef OS_WINDOWS
#include "base/winmain.h"
#endif  // OS_WINDOWS

DEFINE_string(mode, "", "mozc_broker mode");

#ifdef OS_WINDOWS
namespace mozc {
namespace win32 {
int RunPrelaunchProcesses(int argc, char *argv[]);
int RunRegisterIME(int argc, char *argv[]);
int RunUnregisterIME(int argc, char *argv[]);
int RunSetDefault(int argc, char *argv[]);
}  // namespace win32
}  // namespace mozc
#endif  // OS_WINDOWS

int main(int argc, char *argv[]) {
  // Currently, mozc_broker does not care about runlevel because this process
  // might run on system account to support silent upgrading.

  mozc::Util::DisableIME();

  InitGoogleWithBreakPad(argv[0], &argc, &argv, false);

  int result = 0;
#ifdef OS_WINDOWS
  if (FLAGS_mode == "register_ime") {
    result = mozc::win32::RunRegisterIME(argc, argv);
  } else if (FLAGS_mode == "set_default") {
    result = mozc::win32::RunSetDefault(argc, argv);
  } else if (FLAGS_mode == "unregister_ime") {
    result = mozc::win32::RunUnregisterIME(argc, argv);
  } else if (FLAGS_mode == "prelaunch_processes") {
    result = mozc::win32::RunPrelaunchProcesses(argc, argv);
  }
#endif  // OS_WINDOWS

  return result;
}
