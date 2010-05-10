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

#include "base/win_util.h"
#ifdef OS_WINDOWS
#include <winternl.h>
#endif  // OS_WINDOWS

namespace mozc {
#ifdef OS_WINDOWS

namespace {
// Adjusts privileges in the process token to be able to shutdown the machine.
// Returns true if the operation finishes without error.
// We do not use LOG functions in this function to avoid dependency to CRT.
bool AdjustPrivilegesForShutdown() {
  TOKEN_PRIVILEGES ns;
  HANDLE htoken;
  LUID LID;
  LUID_AND_ATTRIBUTES att;

  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES,
                          &htoken)) {
    // Cannot open process token
    return false;
  }

  if (!::LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &LID)) {
    // LookupPrivilegeValue failed
    return false;
  }

  att.Attributes = SE_PRIVILEGE_ENABLED;
  att.Luid = LID;
  ns.PrivilegeCount = 1;
  ns.Privileges[0] = att;

  if (!::AdjustTokenPrivileges(htoken, FALSE, &ns, NULL, NULL, NULL)) {
    // AdjustTokenPrivileges failed
    return false;
  }

  return true;
}
}  // namespace

bool WinUtil::Logoff() {
  if (!AdjustPrivilegesForShutdown()) {
    return false;
  }
  return (::ExitWindowsEx(EWX_LOGOFF, SHTDN_REASON_MINOR_INSTALLATION) != 0);
}

ScopedCOMInitializer::ScopedCOMInitializer()
 : hr_(::CoInitialize(NULL)) {
}

ScopedCOMInitializer::~ScopedCOMInitializer() {
  if (SUCCEEDED(hr_)) {
    ::CoUninitialize();
  }
}
#endif  // OS_WINDOWS
}  // namespace mozc
