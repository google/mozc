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

#include "base/win32/win_util.h"

// skip all unless _WIN32
#ifdef _WIN32

#include <atlbase.h>
#include <aux_ulib.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stringapiset.h>
#include <wil/resource.h>
#include <winternl.h>

#include <clocale>
#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "absl/base/call_once.h"
#include "absl/base/no_destructor.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "base/vlog.h"
#include "base/win32/wide_char.h"

namespace mozc {
namespace {

absl::once_flag g_aux_lib_initialized;

void CallAuxUlibInitialize() { ::AuxUlibInitialize(); }

bool EqualLuid(const LUID& L1, const LUID& L2) {
  return (L1.LowPart == L2.LowPart && L1.HighPart == L2.HighPart);
}

bool IsProcessSandboxedImpl() {
  bool is_restricted = false;
  if (!WinUtil::IsProcessRestricted(::GetCurrentProcess(), &is_restricted)) {
    return true;
  }
  if (is_restricted) {
    return true;
  }

  bool in_appcontainer = false;
  if (!WinUtil::IsProcessInAppContainer(::GetCurrentProcess(),
                                        &in_appcontainer)) {
    return true;
  }

  return in_appcontainer;
}

// A workaround until we improve the method signature of
// ShellExecuteInSystemDir.
// TODO(b/290998032): Remove this.
constexpr std::wstring_view to_wstring_view(const wchar_t* ptr) {
  return ptr == nullptr ? std::wstring_view() : std::wstring_view(ptr);
}

HRESULT TryGetLocalAppDataForAppContainer(std::string* dir) {
  // User profiles for processes running under AppContainer seem to be as
  // follows, while the scheme is not officially documented.
  //   "%LOCALAPPDATA%\Packages\<package sid>\..."
  // Note: You can also obtain this path by GetAppContainerFolderPath API.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/hh448543.aspx
  // Anyway, here we use heuristics to obtain "LocalLow" folder path.
  wchar_t config[MAX_PATH] = {};
  const HRESULT result = ::SHGetFolderPathW(
      nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, &config[0]);
  if (FAILED(result)) {
    return result;
  }
  std::wstring path = config;
  const std::wstring::size_type local_pos = path.find(L"\\Packages\\");
  if (local_pos == std::wstring::npos) {
    return E_FAIL;
  }
  path.erase(local_pos);
  *dir = win32::WideToUtf8(path);
  if (dir->empty()) {
    return E_FAIL;
  }
  absl::StrAppend(dir, "Low");
  return S_OK;
}

HRESULT TryGetLocalAppDataLow(std::string* dir) {
  if (dir == nullptr) {
    return E_FAIL;
  }
  dir->clear();

  wchar_t* task_mem_buffer = nullptr;
  const HRESULT result = ::SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0,
                                                nullptr, &task_mem_buffer);
  if (FAILED(result)) {
    if (task_mem_buffer != nullptr) {
      ::CoTaskMemFree(task_mem_buffer);
    }
    return result;
  }

  if (task_mem_buffer == nullptr) {
    return E_UNEXPECTED;
  }

  std::wstring wpath = task_mem_buffer;
  ::CoTaskMemFree(task_mem_buffer);

  const std::string path = win32::WideToUtf8(wpath);
  if (path.empty()) {
    return E_UNEXPECTED;
  }

  *dir = path;
  return S_OK;
}

HRESULT TryGetLocalAppData(std::string* dir) {
  if (dir == nullptr) {
    return E_FAIL;
  }
  dir->clear();

  bool in_app_container = false;
  if (!WinUtil::IsProcessInAppContainer(::GetCurrentProcess(),
                                        &in_app_container)) {
    return E_FAIL;
  }
  if (in_app_container) {
    return TryGetLocalAppDataForAppContainer(dir);
  }
  return TryGetLocalAppDataLow(dir);
}

// b/5707813 implies that TryGetLocalAppData causes an exception in some cases.
// To prevent that from putting the cached value into an invalid state (which
// historically resulted in an infinite spin loop in call_once), the value must
// be computed by an exception-free function.
// Note that __try and __except does not guarantees that any destruction
// of internal C++ objects when a non-C++ exception occurs except that
// /EHa compiler option is specified.
// Do not use /EHs option, otherwise we must admit potential
// memory leakes when any non-C++ exception occues in TryGetLocalAppData.
// See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
HRESULT __declspec(nothrow) SafeTryGetLocalAppData(std::string* dir) {
  __try {
    return TryGetLocalAppData(dir);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return E_UNEXPECTED;
  }
}

HRESULT TryProgramFilesPath(std::string* path) {
  if (path == nullptr) {
    return E_FAIL;
  }
  path->clear();

  wchar_t program_files_path_buffer[MAX_PATH] = {};
  // For historical reasons Mozc executables have been installed under
  // %ProgramFiles(x86)%.
  // TODO(https://github.com/google/mozc/issues/1086): Stop using "(x86)".
  const HRESULT result =
      ::SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr,
                         SHGFP_TYPE_CURRENT, program_files_path_buffer);
  if (FAILED(result)) {
    return result;
  }

  const std::string program_files =
      win32::WideToUtf8(program_files_path_buffer);
  if (program_files.empty()) {
    return E_FAIL;
  }
  *path = program_files;
  return S_OK;
}

// b/5707813 implies that the Shell API causes an exception in some cases.
// In order to avoid potential infinite loops in call_once, the cached value
// must be computed by an exception-free function.
// Note that __try and __except does not guarantees that any destruction
// of internal C++ objects when a non-C++ exception occurs except that
// /EHa compiler option is specified.
// Do not use /EHs option, otherwise we must admit potential
// memory leaks when any non-C++ exception occurs in TryProgramFilesPath.
// See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
HRESULT __declspec(nothrow) SafeTryProgramFilesPath(std::string* path) {
  __try {
    return TryProgramFilesPath(path);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return E_UNEXPECTED;
  }
}

}  // namespace

bool WinUtil::IsDLLSynchronizationHeld(bool* lock_status) {
  absl::call_once(g_aux_lib_initialized, &CallAuxUlibInitialize);

  if (lock_status == nullptr) {
    return false;
  }

  BOOL synchronization_held = FALSE;
  const BOOL result = ::AuxUlibIsDLLSynchronizationHeld(&synchronization_held);
  if (!result) {
    const int error = ::GetLastError();
    DLOG(ERROR) << "AuxUlibIsDLLSynchronizationHeld failed. error = " << error;
    return false;
  }
  *lock_status = (synchronization_held != FALSE);
  return true;
}

uint32_t WinUtil::EncodeWindowHandle(HWND window_handle) {
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(window_handle));
}

HWND WinUtil::DecodeWindowHandle(uint32_t window_handle_value) {
  return reinterpret_cast<HWND>(static_cast<uintptr_t>(window_handle_value));
}

bool WinUtil::SystemEqualString(const std::wstring_view lhs,
                                const std::wstring_view rhs, bool ignore_case) {
  const int compare_result =
      ::CompareStringOrdinal(lhs.data(), lhs.size(), rhs.data(), rhs.size(),
                             (ignore_case ? TRUE : FALSE));

  return compare_result == CSTR_EQUAL;
}

bool WinUtil::IsServiceUser(HANDLE hToken, bool* is_service) {
  if (is_service == nullptr) {
    return false;
  }

  TOKEN_STATISTICS ts;
  DWORD dwSize = 0;
  // Use token logon LUID instead of user SID, for brevity and safety
  if (!::GetTokenInformation(hToken, TokenStatistics, (LPVOID)&ts, sizeof(ts),
                             &dwSize)) {
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

bool WinUtil::IsServiceProcess(bool* is_service) {
  if (is_service == nullptr) {
    return false;
  }

  // Session 0 is dedicated to services
  DWORD dwSessionId = 0;
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &dwSessionId) ||
      (dwSessionId == 0)) {
    *is_service = true;
    return true;
  }

  // Get process token
  wil::unique_process_handle process_token;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_QUERY | TOKEN_QUERY_SOURCE,
                          process_token.put())) {
    return false;
  }

  // Process token is one for a service account.
  if (!IsServiceUser(process_token.get(), is_service)) {
    return false;
  }

  return true;
}

bool WinUtil::IsServiceThread(bool* is_service) {
  if (is_service == nullptr) {
    return false;
  }

  // Get thread token (if any)
  wil::unique_process_handle thread_token;
  if (!::OpenThreadToken(::GetCurrentThread(), TOKEN_QUERY, TRUE,
                         thread_token.put()) &&
      ::GetLastError() != ERROR_NO_TOKEN) {
    return false;
  }

  if (!thread_token) {
    // No thread token.
    *is_service = false;
    return true;
  }

  // Check if the thread token (if any) is one for a service account.
  if (!IsServiceUser(thread_token.get(), is_service)) {
    return false;
  }
  return true;
}

bool WinUtil::IsServiceAccount(bool* is_service) {
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

bool WinUtil::IsProcessImmersive(HANDLE process_handle, bool* is_immersive) {
  if (is_immersive == nullptr) {
    return false;
  }
  *is_immersive = IsImmersiveProcess(process_handle) != FALSE;
  return true;
}

bool WinUtil::IsProcessRestricted(HANDLE process_handle, bool* is_restricted) {
  if (is_restricted == nullptr) {
    return false;
  }
  *is_restricted = false;

  wil::unique_process_handle process_token;
  if (!::OpenProcessToken(process_handle, TOKEN_QUERY, process_token.put())) {
    return false;
  }

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
                                      bool* in_appcontainer) {
  if (in_appcontainer == nullptr) {
    return false;
  }
  *in_appcontainer = false;

  wil::unique_process_handle process_token;
  if (!::OpenProcessToken(process_handle, TOKEN_QUERY | TOKEN_QUERY_SOURCE,
                          process_token.put())) {
    return false;
  }

  DWORD returned_size = 0;
  DWORD retval = 0;
  if (!GetTokenInformation(process_token.get(), TokenIsAppContainer, &retval,
                           sizeof(retval), &returned_size)) {
    return false;
  }
  if (returned_size != sizeof(retval)) {
    return false;
  }

  *in_appcontainer = (retval != 0);
  return true;
}

bool WinUtil::GetFileSystemInfoFromPath(std::wstring_view path,
                                        BY_HANDLE_FILE_INFORMATION* info) {
  const std::wstring w_path(path);
  // no read access is required.
  wil::unique_hfile handle(::CreateFileW(
      w_path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
      nullptr, OPEN_EXISTING,
      FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL, nullptr));

  if (!handle) {
    return false;
  }
  return !!::GetFileInformationByHandle(handle.get(), info);
}

bool WinUtil::AreEqualFileSystemObject(std::wstring_view left_path,
                                       std::wstring_view right_path) {
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

bool WinUtil::GetNtPath(std::wstring_view dos_path, std::wstring* nt_path) {
  if (nt_path == nullptr) {
    return false;
  }

  nt_path->clear();

  const std::wstring w_dos_path(dos_path);

  wil::unique_hfile file_handle(::CreateFileW(
      w_dos_path.c_str(), 0,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL,
      nullptr));
  if (!file_handle) {
    return false;
  }

  constexpr size_t kMaxPath = 4096;
  auto ntpath_buffer = std::make_unique<wchar_t[]>(kMaxPath);
  const DWORD copied_len_without_null = ::GetFinalPathNameByHandleW(
      file_handle.get(), ntpath_buffer.get(), kMaxPath,
      FILE_NAME_NORMALIZED | VOLUME_NAME_NT);
  if (copied_len_without_null == 0 || copied_len_without_null > kMaxPath) {
    const DWORD error = ::GetLastError();
    MOZC_VLOG(1) << "GetFinalPathNameByHandleW() failed: " << error;
    return false;
  }

  nt_path->assign(ntpath_buffer.get(), copied_len_without_null);
  return true;
}

bool WinUtil::GetProcessInitialNtPath(DWORD pid, std::wstring* nt_path) {
  if (nt_path == nullptr) {
    return false;
  }
  nt_path->clear();

  wil::unique_process_handle process_handle(
      ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));

  if (!process_handle) {
    MOZC_VLOG(1) << "OpenProcess() failed: " << ::GetLastError();
    return false;
  }

  constexpr size_t kMaxPath = 4096;
  auto ntpath_buffer = std::make_unique<wchar_t[]>(kMaxPath);
  const DWORD copied_len_without_null = ::GetProcessImageFileNameW(
      process_handle.get(), ntpath_buffer.get(), kMaxPath);
  if (copied_len_without_null == 0 || copied_len_without_null > kMaxPath) {
    const DWORD error = ::GetLastError();
    MOZC_VLOG(1) << "GetProcessImageFileNameW() failed: " << error;
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
  BOOL is_thread_local = FALSE;
  if (::SystemParametersInfo(SPI_GETTHREADLOCALINPUTSETTINGS, 0,
                             static_cast<void*>(&is_thread_local),
                             0) == FALSE) {
    return false;
  }
  return !is_thread_local;
}

bool WinUtil::IsProcessSandboxed() {
  // Thread safety is not required.
  static bool sandboxed = IsProcessSandboxedImpl();
  return sandboxed;
}

const std::optional<std::wstring>& WinUtil::GetSystem32Path() {
  static const absl::NoDestructor<std::optional<std::wstring>> dir(
      []() -> std::optional<std::wstring> {
        wchar_t path_buffer[MAX_PATH] = {};
        const UINT copied_len_wo_null_if_success =
            ::GetSystemDirectory(path_buffer, std::size(path_buffer));
        if (copied_len_wo_null_if_success >= std::size(path_buffer)) {
          // Function failed.
          return std::nullopt;
        }
        DCHECK_EQ(L'\0', path_buffer[copied_len_wo_null_if_success]);
        return std::wstring(path_buffer);
      }());
  return *dir;
}

const std::optional<std::string>& WinUtil::GetProgramFilesX86Path() {
  static const absl::NoDestructor<std::optional<std::string>> path(
      []() -> std::optional<std::string> {
        std::string program_files;
        if (SUCCEEDED(SafeTryProgramFilesPath(&program_files))) {
          return program_files;
        }
        return std::nullopt;
      }());
  return *path;
}

const std::optional<std::string>& WinUtil::GetLocalAppDataPath() {
  static const absl::NoDestructor<std::optional<std::string>> dir(
      []() -> std::optional<std::string> {
        std::string path;
        if (SUCCEEDED(SafeTryGetLocalAppData(&path))) {
          return path;
        }
        return std::nullopt;
      }());
  return *dir;
}

bool WinUtil::ShellExecuteInSystemDir(const wchar_t* verb, const wchar_t* file,
                                      const wchar_t* parameters) {
  const std::optional<std::wstring>& system_dir = GetSystem32Path();
  const auto result =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(::ShellExecuteW(
          nullptr, verb, file, parameters,
          system_dir.has_value() ? system_dir->c_str() : nullptr, SW_SHOW)));
  LOG_IF(ERROR, result <= 32)
      << "ShellExecute failed." << ", error:" << result
      << ", verb: " << win32::WideToUtf8(to_wstring_view(verb))
      << ", file: " << win32::WideToUtf8(to_wstring_view(file))
      << ", parameters: " << win32::WideToUtf8(to_wstring_view(parameters));
  return result > 32;
}

}  // namespace mozc

#endif  // _WIN32
