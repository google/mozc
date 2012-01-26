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

#include "base/win_util.h"

#ifdef OS_WINDOWS
#include <Aux_ulib.h>
#include <Winternl.h>

#include <clocale>
#endif  // OS_WINDOWS

#ifdef OS_WINDOWS
#include "base/util.h"
#include "base/scoped_handle.h"
#include "base/singleton.h"
#endif  // OS_WINDOWS

namespace mozc {
#ifdef OS_WINDOWS
namespace {
class AuxLibInitializer {
 public:
  AuxLibInitializer() {
    ::AuxUlibInitialize();
  }
  bool IsDLLSynchronizationHeld(bool *lock_status) const {
    if (lock_status == NULL) {
      return false;
    }

    BOOL synchronization_held = FALSE;
    const BOOL result =
      ::AuxUlibIsDLLSynchronizationHeld(&synchronization_held);
    if (!result) {
      const int error = ::GetLastError();
      DLOG(ERROR) << "AuxUlibIsDLLSynchronizationHeld failed. error = "
                  << error;
      return false;
    }
    *lock_status = (synchronization_held != FALSE);
    return true;
  }
};

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

bool EqualLuid(const LUID &L1, const LUID &L2) {
  return (L1.LowPart == L2.LowPart && L1.HighPart == L2.HighPart);
}
}  // namespace

bool WinUtil::IsDLLSynchronizationHeld(bool *lock_status) {
  return Singleton<AuxLibInitializer>::get()->IsDLLSynchronizationHeld(
      lock_status);
}

bool WinUtil::Logoff() {
  if (!AdjustPrivilegesForShutdown()) {
    return false;
  }
  return (::ExitWindowsEx(EWX_LOGOFF, SHTDN_REASON_MINOR_INSTALLATION) != 0);
}

bool WinUtil::Win32EqualString(const wstring &lhs, const wstring &rhs,
                               bool ignore_case, bool *are_equal) {
  // http://msdn.microsoft.com/en-us/library/dd317762.aspx
  typedef int (WINAPI *FPCompareStringOrdinal)(
      __in LPCWSTR lpString1,
      __in int     cchCount1,
      __in LPCWSTR lpString2,
      __in int     cchCount2,
      __in BOOL    bIgnoreCase);

  const HMODULE kernel = Util::GetSystemModuleHandle(L"kernel32.dll");
  if (kernel == NULL) {
    LOG(ERROR) << "GetSystemModuleHandle failed";
    return false;
  }

  const FPCompareStringOrdinal compare_string_ordinal =
      reinterpret_cast<FPCompareStringOrdinal>(
          ::GetProcAddress(kernel, "CompareStringOrdinal"));
  if (compare_string_ordinal == NULL) {
    return false;
  }
  const int compare_result = compare_string_ordinal(
      lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size(),
      (ignore_case ? TRUE : FALSE));

  if (are_equal != NULL) {
    *are_equal = (compare_result == CSTR_EQUAL);
  }

  return true;
}

bool WinUtil::NativeEqualString(const wstring &lhs, const wstring &rhs,
                                bool ignore_case, bool *are_equal) {
  // http://msdn.microsoft.com/en-us/library/ff561854.aspx
  typedef BOOLEAN (NTAPI *FPRtlEqualUnicodeString)(
      __in  PCUNICODE_STRING String1,
      __in  PCUNICODE_STRING String2,
      __in  BOOLEAN CaseInSensitive);

  const HMODULE ntdll = Util::GetSystemModuleHandle(L"ntdll.dll");
  if (ntdll == NULL) {
    LOG(ERROR) << "GetSystemModuleHandle failed";
    return false;
  }

  const FPRtlEqualUnicodeString rtl_equal_unicode_string =
      reinterpret_cast<FPRtlEqualUnicodeString>(
          ::GetProcAddress(ntdll, "RtlEqualUnicodeString"));
  if (rtl_equal_unicode_string == NULL) {
    return false;
  }

  const UNICODE_STRING lhs_string = {
    lhs.size(),                     // Length
    lhs.size() + sizeof(wchar_t),   // MaximumLength
    const_cast<PWSTR>(lhs.c_str())  // Buffer
  };
  const UNICODE_STRING rhs_string = {
    rhs.size(),                     // Length
    rhs.size() + sizeof(wchar_t),   // MaximumLength
    const_cast<PWSTR>(rhs.c_str())  // Buffer
  };
  const BOOL compare_result = rtl_equal_unicode_string(
    &lhs_string, &rhs_string, (ignore_case ? TRUE : FALSE));

  if (are_equal != NULL) {
    *are_equal = (compare_result != FALSE);
  }

  return true;
}

void WinUtil::CrtEqualString(const wstring &lhs, const wstring &rhs,
                             bool ignore_case, bool *are_equal) {
  if (are_equal == NULL) {
    return;
  }

  if (!ignore_case) {
    DCHECK_NE(NULL, are_equal);
    *are_equal = (rhs == lhs);
    return;
  }

  const _locale_t locale_id = _create_locale(LC_ALL, "English");
  const int compare_result = _wcsicmp_l(lhs.c_str(), rhs.c_str(), locale_id);
  _free_locale(locale_id);

  DCHECK_NE(NULL, are_equal);
  *are_equal = (compare_result == 0);
}

bool WinUtil::SystemEqualString(
      const wstring &lhs, const wstring &rhs, bool ignore_case) {
  bool are_equal = false;

  // We assume a string instance never contains NUL character in principle.
  // So we will raise an error to notify the unexpected situation in debug
  // builds.  In production, however, we will admit such an instance and
  // silently trim it at the first NUL character.
  const wstring::size_type lhs_null_pos = lhs.find_first_of(L'\0');
  const wstring::size_type rhs_null_pos = rhs.find_first_of(L'\0');
  DCHECK_EQ(lhs.npos, lhs_null_pos)
      << "|lhs| should not contain NUL character.";
  DCHECK_EQ(rhs.npos, rhs_null_pos)
      << "|rhs| should not contain NUL character.";
  const wstring &lhs_null_trimmed = lhs.substr(0, lhs_null_pos);
  const wstring &rhs_null_trimmed = rhs.substr(0, rhs_null_pos);

  if (Win32EqualString(
          lhs_null_trimmed, rhs_null_trimmed, ignore_case, &are_equal)) {
    return are_equal;
  }

  if (NativeEqualString(
          lhs_null_trimmed, rhs_null_trimmed, ignore_case, &are_equal)) {
    return are_equal;
  }

  CrtEqualString(lhs_null_trimmed, rhs_null_trimmed, ignore_case, &are_equal);

  return are_equal;
}

bool WinUtil::IsServiceUser(HANDLE hToken, bool *is_service) {
  if (is_service == NULL) {
    return false;
  }

  TOKEN_STATISTICS ts;
  DWORD dwSize = 0;
  // Use token logon LUID instead of user SID, for brevity and safety
  if (!::GetTokenInformation(hToken, TokenStatistics,
                             (LPVOID)&ts, sizeof(ts), &dwSize)) {
    return false;
  }

  // Compare LUID
  const LUID SystemLuid = SYSTEM_LUID;
  const LUID LocalServiceLuid = LOCALSERVICE_LUID;
  const LUID NetworkServiceLuid = NETWORKSERVICE_LUID;
  if (EqualLuid(SystemLuid, ts.AuthenticationId) ||
      EqualLuid(LocalServiceLuid, ts.AuthenticationId) ||
      EqualLuid(NetworkServiceLuid, ts.AuthenticationId)) {
    *is_service = true;
    return true;
  }

  // Not a service account
  *is_service = false;
  return true;
}

bool WinUtil::IsServiceProcess(bool *is_service) {
  if (is_service == NULL) {
    return false;
  }

  if (Util::IsVistaOrLater()) {
    // Session 0 is dedicated to services
    DWORD dwSessionId = 0;
    if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &dwSessionId) ||
        (dwSessionId == 0)) {
      *is_service = true;
      return true;
    }
  }

  // Get process token
  HANDLE hProcessToken = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_QUERY | TOKEN_QUERY_SOURCE,
                          &hProcessToken)) {
    return false;
  }

  ScopedHandle process_token(hProcessToken);

  // Process token is one for a service account.
  if (!IsServiceUser(process_token.get(), is_service)) {
    return false;
  }

  return true;
}

bool WinUtil::IsServiceThread(bool *is_service) {
  if (is_service == NULL) {
    return false;
  }

  // Get thread token (if any)
  HANDLE hThreadToken = NULL;
  if (!::OpenThreadToken(::GetCurrentThread(),
                        TOKEN_QUERY, TRUE, &hThreadToken) &&
      ERROR_NO_TOKEN != ::GetLastError()) {
    return false;
  }

  if (hThreadToken == NULL) {
    // No thread token.
    *is_service = false;
    return true;
  }

  ScopedHandle thread_token(hThreadToken);

  // Check if the thread token (if any) is one for a service account.
  if (!IsServiceUser(thread_token.get(), is_service)) {
    return false;
  }
  return true;
}

bool WinUtil::IsServiceAccount(bool *is_service) {
  if (is_service == NULL) {
    return false;
  }

  bool is_service_process = false;
  if (!WinUtil::IsServiceProcess(&is_service_process)) {
    DLOG(ERROR) << "WinUtil::IsServiceProcess failed.";
    return false;
  }

  if (is_service_process) {
    *is_service = true;
    return true;
  }

  // Process token is not one for service.
  // Check thread token just in case.
  bool is_service_thread = false;
  if (!WinUtil::IsServiceThread(&is_service_thread)) {
    DLOG(ERROR) << "WinUtil::IsServiceThread failed.";
    return false;
  }

  if (is_service_thread) {
    *is_service = true;
    return true;
  }

  *is_service = false;
  return true;
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
