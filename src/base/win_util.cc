// Copyright 2010-2014, Google Inc.
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

// skip all unless OS_WIN
#ifdef OS_WIN

#include <Aux_ulib.h>
#include <Psapi.h>
#include <Winternl.h>

// Workaround against KB813540
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase_mozc.h>

#include <clocale>
#include <memory>

#include "base/logging.h"
#include "base/mutex.h"
#include "base/scoped_handle.h"
#include "base/system_util.h"
#include "base/util.h"

using std::unique_ptr;

namespace mozc {
namespace {

once_t g_aux_lib_initialized = MOZC_ONCE_INIT;

void CallAuxUlibInitialize() {
  ::AuxUlibInitialize();
}

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

  if (!::LookupPrivilegeValue(nullptr, SE_SHUTDOWN_NAME, &LID)) {
    // LookupPrivilegeValue failed
    return false;
  }

  att.Attributes = SE_PRIVILEGE_ENABLED;
  att.Luid = LID;
  ns.PrivilegeCount = 1;
  ns.Privileges[0] = att;

  if (!::AdjustTokenPrivileges(htoken, FALSE, &ns, 0, nullptr, nullptr)) {
    // AdjustTokenPrivileges failed
    return false;
  }

  return true;
}

bool EqualLuid(const LUID &L1, const LUID &L2) {
  return (L1.LowPart == L2.LowPart && L1.HighPart == L2.HighPart);
}


// The registry key for the CUAS setting.
// Note: We have the same values in win32/base/imm_util.cc
// TODO(yukawa): Define these constants at the same place.
const wchar_t kCUASKey[] = L"Software\\Microsoft\\CTF\\SystemShared";
const wchar_t kCUASValueName[] = L"CUAS";

// Reads CUAS value in the registry keys and returns true if the value is set
// to 1.
// The CUAS value is read from 64 bit registry keys if KEY_WOW64_64KEY is
// specified as |additional_regsam| and read from 32 bit registry keys if
// KEY_WOW64_32KEY is specified.
bool IsCuasEnabledInternal(REGSAM additional_regsam) {
  const REGSAM sam_desired = KEY_QUERY_VALUE | additional_regsam;
  ATL::CRegKey key;
  LONG result = key.Open(HKEY_LOCAL_MACHINE, kCUASKey, sam_desired);
  if (ERROR_SUCCESS != result) {
    LOG(ERROR) << "Cannot open HKEY_LOCAL_MACHINE\\Software\\Microsoft\\CTF\\"
                  "SystemShared: "
               << result;
    return false;
  }
  DWORD cuas;
  result = key.QueryDWORDValue(kCUASValueName, cuas);
  if (ERROR_SUCCESS != result) {
    LOG(ERROR) << "Failed to query CUAS value:" << result;
  }
  return (cuas == 1);
}

}  // namespace

HMODULE WinUtil::LoadSystemLibrary(const wstring &base_filename) {
  wstring fullpath = SystemUtil::GetSystemDir();
  fullpath += L"\\";
  fullpath += base_filename;

  const HMODULE module = ::LoadLibraryExW(fullpath.c_str(),
                                          nullptr,
                                          LOAD_WITH_ALTERED_SEARCH_PATH);
  if (nullptr == module) {
    const int last_error = ::GetLastError();
    DLOG(WARNING) << "LoadLibraryEx failed."
                  << " fullpath = " << fullpath.c_str()
                  << " error = " << last_error;
  }
  return module;
}

HMODULE WinUtil::LoadMozcLibrary(const wstring &base_filename) {
  wstring fullpath;
  Util::UTF8ToWide(SystemUtil::GetServerDirectory().c_str(), &fullpath);
  fullpath += L"\\";
  fullpath += base_filename;

  const HMODULE module = ::LoadLibraryExW(fullpath.c_str(),
                                          nullptr,
                                          LOAD_WITH_ALTERED_SEARCH_PATH);
  if (nullptr == module) {
    const int last_error = ::GetLastError();
    DLOG(WARNING) << "LoadLibraryEx failed."
                  << " fullpath = " << fullpath.c_str()
                  << " error = " << last_error;
  }
  return module;
}

HMODULE WinUtil::GetSystemModuleHandle(const wstring &base_filename) {
  wstring fullpath = SystemUtil::GetSystemDir();
  fullpath += L"\\";
  fullpath += base_filename;

  HMODULE module = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         fullpath.c_str(), &module) == FALSE) {
    const int last_error = ::GetLastError();
    DLOG(WARNING) << "GetModuleHandleExW failed."
                  << " fullpath = " << fullpath.c_str()
                  << " error = " << last_error;
  }
  return module;
}

HMODULE WinUtil::GetSystemModuleHandleAndIncrementRefCount(
    const wstring &base_filename) {
  wstring fullpath = SystemUtil::GetSystemDir();
  fullpath += L"\\";
  fullpath += base_filename;

  HMODULE module = nullptr;
  if (GetModuleHandleExW(0, fullpath.c_str(), &module) == FALSE) {
    const int last_error = ::GetLastError();
    DLOG(WARNING) << "GetModuleHandleExW failed."
                  << " fullpath = " << fullpath.c_str()
                  << " error = " << last_error;
  }
  return module;
}

bool WinUtil::IsDLLSynchronizationHeld(bool *lock_status) {
  mozc::CallOnce(&g_aux_lib_initialized, &CallAuxUlibInitialize);

  if (lock_status == nullptr) {
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

  const HMODULE kernel = WinUtil::GetSystemModuleHandle(L"kernel32.dll");
  if (kernel == nullptr) {
    LOG(ERROR) << "GetSystemModuleHandle failed";
    return false;
  }

  const FPCompareStringOrdinal compare_string_ordinal =
      reinterpret_cast<FPCompareStringOrdinal>(
          ::GetProcAddress(kernel, "CompareStringOrdinal"));
  if (compare_string_ordinal == nullptr) {
    return false;
  }
  const int compare_result = compare_string_ordinal(
      lhs.c_str(), lhs.size(), rhs.c_str(), rhs.size(),
      (ignore_case ? TRUE : FALSE));

  if (are_equal != nullptr) {
    *are_equal = (compare_result == CSTR_EQUAL);
  }

  return true;
}

bool WinUtil::NativeEqualString(const wstring &lhs, const wstring &rhs,
                                bool ignore_case, bool *are_equal) {
  // http://msdn.microsoft.com/en-us/library/ff561854.aspx
  typedef BOOLEAN (NTAPI *FPRtlEqualUnicodeString)(
      __in PCUNICODE_STRING String1,
      __in PCUNICODE_STRING String2,
      __in BOOLEAN CaseInSensitive);

  const HMODULE ntdll = GetSystemModuleHandle(L"ntdll.dll");
  if (ntdll == nullptr) {
    LOG(ERROR) << "GetSystemModuleHandle failed";
    return false;
  }

  const FPRtlEqualUnicodeString rtl_equal_unicode_string =
      reinterpret_cast<FPRtlEqualUnicodeString>(
          ::GetProcAddress(ntdll, "RtlEqualUnicodeString"));
  if (rtl_equal_unicode_string == nullptr) {
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

  if (are_equal != nullptr) {
    *are_equal = (compare_result != FALSE);
  }

  return true;
}

void WinUtil::CrtEqualString(const wstring &lhs, const wstring &rhs,
                             bool ignore_case, bool *are_equal) {
  if (are_equal == nullptr) {
    return;
  }

  if (!ignore_case) {
    DCHECK_NE(nullptr, are_equal);
    *are_equal = (rhs == lhs);
    return;
  }

  const _locale_t locale_id = _create_locale(LC_ALL, "English");
  const int compare_result = _wcsicmp_l(lhs.c_str(), rhs.c_str(), locale_id);
  _free_locale(locale_id);

  DCHECK_NE(nullptr, are_equal);
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
  if (is_service == nullptr) {
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
  if (is_service == nullptr) {
    return false;
  }

  if (SystemUtil::IsVistaOrLater()) {
    // Session 0 is dedicated to services
    DWORD dwSessionId = 0;
    if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &dwSessionId) ||
        (dwSessionId == 0)) {
      *is_service = true;
      return true;
    }
  }

  // Get process token
  HANDLE hProcessToken = nullptr;
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
  if (is_service == nullptr) {
    return false;
  }

  // Get thread token (if any)
  HANDLE hThreadToken = nullptr;
  if (!::OpenThreadToken(::GetCurrentThread(),
                        TOKEN_QUERY, TRUE, &hThreadToken) &&
      ERROR_NO_TOKEN != ::GetLastError()) {
    return false;
  }

  if (hThreadToken == nullptr) {
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
  if (is_service == nullptr) {
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

bool WinUtil::IsProcessImmersive(HANDLE process_handle,
                                 bool *is_immersive) {
  if (is_immersive == nullptr) {
    return false;
  }
  *is_immersive = false;
  // ImmersiveMode is supported only in Windows8 and later.
  if (!SystemUtil::IsWindows8OrLater()) {
    return true;
  }

  const HMODULE module = WinUtil::GetSystemModuleHandle(L"user32.dll");
  if (module == nullptr) {
    return false;
  }

  typedef BOOL (WINAPI* IsImmersiveProcessFunc)(HANDLE process);
  IsImmersiveProcessFunc is_immersive_process =
      reinterpret_cast<IsImmersiveProcessFunc>(
          ::GetProcAddress(module, "IsImmersiveProcess"));
  if (is_immersive_process == nullptr) {
    return false;
  }

  *is_immersive = !!is_immersive_process(process_handle);
  return true;
}

bool WinUtil::IsProcessRestricted(HANDLE process_handle, bool *is_restricted) {
  if (is_restricted == nullptr) {
    return false;
  }
  *is_restricted = false;

  HANDLE token = nullptr;
  if (!::OpenProcessToken(process_handle, TOKEN_QUERY, &token)) {
    return false;
  }

  ScopedHandle process_token(token);
  ::SetLastError(NOERROR);
  if (::IsTokenRestricted(process_token.get()) == FALSE) {
    const DWORD error = ::GetLastError();
    if (error != NOERROR) {
      return false;
    }
  } else {
    *is_restricted = true;
  }
  return true;
}

bool WinUtil::IsProcessInAppContainer(HANDLE process_handle,
                                      bool *in_appcontainer) {
  if (in_appcontainer == nullptr) {
    return false;
  }
  *in_appcontainer = false;

  // AppContainer is supported only in Windows8 and later.
  if (!SystemUtil::IsWindows8OrLater()) {
    return true;
  }

  HANDLE token = nullptr;
  if (!::OpenProcessToken(process_handle, TOKEN_QUERY | TOKEN_QUERY_SOURCE,
                          &token)) {
    return false;
  }

  // TokenIsAppContainer is defined only in Windows SDK 8.0 and later.
  ScopedHandle process_token(token);
  const TOKEN_INFORMATION_CLASS kTokenIsAppContainer =
      static_cast<TOKEN_INFORMATION_CLASS>(29);  // TokenIsAppContainer
#if defined(_WIN32_WINNT_WIN8)
  static_assert(kTokenIsAppContainer == TokenIsAppContainer,
                "Checking |kTokenIsAppContainer| has correct value.");
#endif  // _WIN32_WINNT_WIN8
  DWORD returned_size = 0;
  DWORD retval = 0;
  if (!GetTokenInformation(process_token.get(), kTokenIsAppContainer,
                           &retval, sizeof(retval), &returned_size)) {
    return false;
  }
  if (returned_size != sizeof(retval)) {
    return false;
  }

  *in_appcontainer = (retval != 0);
  return true;
}

bool WinUtil::IsCuasEnabled() {
  if (SystemUtil::IsVistaOrLater()) {
    // CUAS is always enabled on Vista or later.
    return true;
  }

  if (SystemUtil::IsWindowsX64()) {
    // see both 64 bit and 32 bit registry keys
    return IsCuasEnabledInternal(KEY_WOW64_64KEY) &&
           IsCuasEnabledInternal(KEY_WOW64_32KEY);
  } else {
    return IsCuasEnabledInternal(0);
  }
}

bool WinUtil::GetFileSystemInfoFromPath(
    const wstring &path, BY_HANDLE_FILE_INFORMATION *info) {
  // no read access is required.
  ScopedHandle handle(::CreateFileW(
      path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL, nullptr));

  // Caveats: handle.get() returns nullptr when it is initialized with
  //     INVALID_HANDLE_VALUE.
  if (handle.get() == nullptr) {
    return false;
  }
  return !!::GetFileInformationByHandle(handle.get(), info);
}

bool WinUtil::AreEqualFileSystemObject(const wstring &left_path,
                                       const wstring &right_path) {
  BY_HANDLE_FILE_INFORMATION left_info = {};
  if (!GetFileSystemInfoFromPath(left_path, &left_info)) {
    return false;
  }
  BY_HANDLE_FILE_INFORMATION right_info = {};
  if (!GetFileSystemInfoFromPath(right_path, &right_info)) {
    return false;
  }
  return (left_info.nFileIndexLow == right_info.nFileIndexLow) &&
         (left_info.nFileIndexHigh == right_info.nFileIndexHigh);
}

bool WinUtil::GetNtPath(const wstring &dos_path, wstring *nt_path) {
  if (nt_path == nullptr) {
    return false;
  }

  nt_path->clear();

  typedef DWORD (WINAPI *GetFinalPathNameByHandleWFunc)(
      __in HANDLE file,
      __out wchar_t *buffer,
      __in DWORD buffer_num_chars,
      __in DWORD flags);
  GetFinalPathNameByHandleWFunc get_final_path_name_by_handle =
      reinterpret_cast<GetFinalPathNameByHandleWFunc>(
          ::GetProcAddress(WinUtil::GetSystemModuleHandle(L"kernel32.dll"),
                           "GetFinalPathNameByHandleW"));
  if (get_final_path_name_by_handle == nullptr) {
    return false;
  }

  ScopedHandle file_handle(::CreateFileW(
      dos_path.c_str(), 0,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL, nullptr));
  if (file_handle.get() == nullptr) {
    // Caveats: |file_handle.get()| becomes nullptr instead of
    // INVALID_HANDLE_VALUE when failure.
    return false;
  }

  const size_t kMaxPath = 4096;
  unique_ptr<wchar_t[]> ntpath_buffer(
      new wchar_t[kMaxPath]);
  const DWORD copied_len_without_null = get_final_path_name_by_handle(
      file_handle.get(),
      ntpath_buffer.get(),
      kMaxPath,
      FILE_NAME_NORMALIZED | VOLUME_NAME_NT);
  if (copied_len_without_null == 0 ||
      copied_len_without_null > kMaxPath) {
    const DWORD error = ::GetLastError();
    VLOG(1) << "GetFinalPathNameByHandleW() failed: " << error;
    return false;
  }

  nt_path->assign(ntpath_buffer.get(), copied_len_without_null);
  return true;
}

bool WinUtil::GetProcessInitialNtPath(DWORD pid, wstring *nt_path) {
  if (nt_path == nullptr) {
    return false;
  }
  nt_path->clear();

  const DWORD required_access =
      SystemUtil::IsVistaOrLater() ? PROCESS_QUERY_LIMITED_INFORMATION
                                   : PROCESS_QUERY_INFORMATION;
  ScopedHandle process_handle(::OpenProcess(required_access, FALSE, pid));

  if (process_handle.get() == nullptr) {
    VLOG(1) << "OpenProcess() failed: " << ::GetLastError();
    return false;
  }

  const size_t kMaxPath = 4096;
  unique_ptr<wchar_t[]> ntpath_buffer(new wchar_t[kMaxPath]);
  const DWORD copied_len_without_null =
      ::GetProcessImageFileNameW(process_handle.get(),
                                 ntpath_buffer.get(),
                                 kMaxPath);
  if (copied_len_without_null == 0 || copied_len_without_null > kMaxPath) {
    const DWORD error = ::GetLastError();
    VLOG(1) << "GetProcessImageFileNameW() failed: " << error;
    return false;
  }

  nt_path->assign(ntpath_buffer.get(), copied_len_without_null);
  return true;
}

// SPI_GETTHREADLOCALINPUTSETTINGS is available on Windows 8 SDK and later.
#ifndef SPI_GETTHREADLOCALINPUTSETTINGS
#define SPI_GETTHREADLOCALINPUTSETTINGS 0x104E
#endif  // SPI_GETTHREADLOCALINPUTSETTINGS

bool WinUtil::IsPerUserInputSettingsEnabled() {
  if (!SystemUtil::IsWindows8OrLater()) {
    // Windows 7 and below does not support per-user input mode.
    return false;
  }
  BOOL is_thread_local = FALSE;
  if (::SystemParametersInfo(SPI_GETTHREADLOCALINPUTSETTINGS,
                             0,
                             reinterpret_cast<void *>(&is_thread_local),
                             0) == FALSE) {
    return false;
  }
  return !is_thread_local;
}

ScopedCOMInitializer::ScopedCOMInitializer()
    : hr_(::CoInitialize(nullptr)) {
}

ScopedCOMInitializer::~ScopedCOMInitializer() {
  if (SUCCEEDED(hr_)) {
    ::CoUninitialize();
  }
}

}  // namespace mozc

#endif  // OS_WIN
