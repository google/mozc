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

// A dll to test the loader lock detection. (Only used for internal unit test)

#if defined(_WIN32)
#include <windows.h>

#include "base/win32/win_util.h"

bool g_is_lock_check_succeeded = false;
bool g_is_lock_held = false;

extern "C" int __stdcall IsLockCheckSucceeded() {
  return g_is_lock_check_succeeded ? 1 : 0;
}

extern "C" int __stdcall IsLockHeld() { return g_is_lock_held ? 1 : 0; }

extern "C" int __stdcall ClearFlagsAndCheckAgain() {
  g_is_lock_check_succeeded = false;
  g_is_lock_held = false;

  g_is_lock_check_succeeded =
      mozc::WinUtil::IsDLLSynchronizationHeld(&g_is_lock_held);
  return 0;
}

// Represents the entry point of this module.
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    g_is_lock_check_succeeded =
        mozc::WinUtil::IsDLLSynchronizationHeld(&g_is_lock_held);
  }
  return TRUE;
}
#endif  // _WIN32
