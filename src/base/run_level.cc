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

#include "base/run_level.h"

#ifdef __linux__
#include <sys/types.h>
#endif  // __linux__

#ifdef _WIN32
#include <aclapi.h>
#include <wil/resource.h>
#include <windows.h>

#include "absl/strings/string_view.h"
#include "base/const.h"
#include "base/system_util.h"
#include "base/win32/wide_char.h"
#include "base/win32/win_sandbox.h"
#include "base/win32/win_util.h"
#else  // _WIN32
#include <unistd.h>
#endif  // _WIN32

namespace mozc {
namespace {

#ifdef _WIN32
constexpr wchar_t kElevatedProcessDisabledName[] = L"elevated_process_disabled";

// returns true if the token was created by Secondary Logon
// (typically via RunAs command) or UAC (w/ alternative credential provided)
//  or if failed to determine.
bool IsDifferentUser(const HANDLE hToken) {
  TOKEN_SOURCE src;
  DWORD dwReturnedBc;

  // Get TOKEN_SOURCE
  if (!::GetTokenInformation(hToken, TokenSource, &src, sizeof(src),
                             &dwReturnedBc)) {
    // Most likely there was an error.
    return true;
  }

  // SourceName is not always null-terminated.
  //  We're looking for the cases marked '->'.
  //  Vista SP1 (Normal)                     "User32 \0"
  //  ->  Vista SP1 (RunAs):                 "seclogo\0"
  //  ->  Vista SP1 (Over-the-shoulder UAC): "CredPro\0"
  constexpr absl::string_view kSeclogo("seclogo", TOKEN_SOURCE_LENGTH);
  constexpr absl::string_view kCredPro("CredPro", TOKEN_SOURCE_LENGTH);

  absl::string_view source_name(src.SourceName, TOKEN_SOURCE_LENGTH);

  return source_name == kSeclogo || source_name == kCredPro;
}

// Returns true if UAC gave the high integrity level to the token
// or if failed to determine.
// This code is written by thatanaka
bool IsElevatedByUAC(const HANDLE hToken) {
  // Get TokenElevationType.
  DWORD dwSize;
  TOKEN_ELEVATION_TYPE ElevationType;
  if (!::GetTokenInformation(hToken, TokenElevationType, &ElevationType,
                             sizeof(ElevationType), &dwSize)) {
    return true;
  }

  // Only TokenElevationTypeFull means the process token was elevated by UAC.
  if (TokenElevationTypeFull != ElevationType) {
    return false;
  }

  // Although rare, it is still possible for an elevated token to have a lower
  // integrity level.
  // Checking to see if it is actually higher than medium.

  // Get TokenIntegrityLevel.
  BYTE buffer[sizeof(TOKEN_MANDATORY_LABEL) + SECURITY_MAX_SID_SIZE];
  if (!::GetTokenInformation(hToken, TokenIntegrityLevel, buffer,
                             sizeof(buffer), &dwSize)) {
    return true;
  }

  PTOKEN_MANDATORY_LABEL pMandatoryLabel =
      reinterpret_cast<PTOKEN_MANDATORY_LABEL>(buffer);

  // Since the SID was acquired from token, it should be always valid.
  DCHECK(::IsValidSid(pMandatoryLabel->Label.Sid));

  // Find the integrity level RID.
  PDWORD pdwIntegrityLevelRID;
  pdwIntegrityLevelRID = ::GetSidSubAuthority(pMandatoryLabel->Label.Sid,
                                              0 /* index 0 always exists */);
  if (!pdwIntegrityLevelRID) {
    return true;
  }

  return (SECURITY_MANDATORY_MEDIUM_RID < *pdwIntegrityLevelRID);
}
#endif  // _WIN32
}  // namespace

RunLevel::RunLevelType RunLevel::GetRunLevel(RunLevel::RequestType type) {
#ifdef _WIN32
  bool is_service_process = false;
  if (!WinUtil::IsServiceProcess(&is_service_process)) {
    // Returns DENY conservatively.
    return RunLevel::DENY;
  }
  if (is_service_process) {
    return RunLevel::DENY;
  }

  // Get process token
  wil::unique_process_handle process_token;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_QUERY | TOKEN_QUERY_SOURCE,
                          process_token.put())) {
    return RunLevel::DENY;
  }

  // Opt out of elevated process.
  if (CLIENT == type && GetElevatedProcessDisabled() &&
      mozc::IsElevatedByUAC(process_token.get())) {
    return RunLevel::DENY;
  }

  // Get thread token (if any)
  wil::unique_process_handle thread_token;
  if (!::OpenThreadToken(::GetCurrentThread(), TOKEN_QUERY, TRUE,
                         thread_token.put()) &&
      ERROR_NO_TOKEN != ::GetLastError()) {
    return RunLevel::DENY;
  }

  // Thread token (if any) must not a service account.
  if (thread_token) {
    bool is_service_thread = false;
    if (!WinUtil::IsServiceUser(thread_token.get(), &is_service_thread)) {
      // Returns DENY conservatively.
      return RunLevel::DENY;
    }
    if (is_service_thread) {
      return RunLevel::DENY;
    }
  }

  // Check whether the server/renderer is running inside sandbox.
  if (type == SERVER || type == RENDERER) {
    // Restricted token must be created by sandbox.
    // Server is launched with NON_ADMIN so that it can use SSL access.
    // This is why it doesn't have restricted token. b/5502343
    if (type != SERVER && !::IsTokenRestricted(process_token.get())) {
      return RunLevel::DENY;
    }

    // Thread token must be created by sandbox.
    if (!thread_token) {
      return RunLevel::DENY;
    }

    // Get the server path before the process is sandboxed.
    // SHGetFolderPath may fail in a sandboxed process.
    // See http://b/2301066 for details.
    // 'volatile' is used here to make sure calling GetServerDirectory.
    // It internally initializes caches of file paths.
    const volatile std::string sys_dir = SystemUtil::GetServerDirectory();

    // Get the user profile path here because of the same reason.
    // See http://b/2301066 for details.
    const std::string user_dir = SystemUtil::GetUserProfileDirectory();

    wil::unique_hfile dir_handle(::CreateFile(
        win32::Utf8ToWide(user_dir).c_str(), READ_CONTROL | WRITE_DAC, 0,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0));
    if (dir_handle) {
      BYTE buffer[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
      DWORD size = 0;
      if (::GetTokenInformation(thread_token.get(), TokenUser, buffer,
                                sizeof(buffer), &size)) {
        PTOKEN_USER ptoken_user = reinterpret_cast<PTOKEN_USER>(buffer);
        // Looks like in some environment, the profile foler's permission
        // includes Administrators but does not include the user himself.
        // In such a case, Mozc wouldn't work because Administrators identity
        // is removed by sandboxing so we should recover the permission here.
        // See http://b/2317718 for details.
        WinSandbox::AddKnownSidToKernelObject(
            dir_handle.get(), static_cast<SID*>(ptoken_user->User.Sid),
            SUB_CONTAINERS_AND_OBJECTS_INHERIT, GENERIC_ALL);
      }
    }

    // Revert from the impersonation token supplied by sandbox.
    // Note: This succeeds even when the thread is not impersonating.
    if (!::RevertToSelf()) {
      return RunLevel::DENY;
    }
  }

  // All DENY checks are passed.

  // Check whether the server/renderer is running as RunAs.
  if (type == SERVER || type == RENDERER) {
    // It's ok to do this check after RevertToSelf, as it's a process token
    // and also its handle was opened before.
    if (IsDifferentUser(process_token.get())) {
      // Run in RESTRICTED level in order to prevent the process from running
      // too long in another user's desktop.
      return RunLevel::RESTRICTED;
    }
  }

  return RunLevel::NORMAL;

#elif defined(__wasm__)
  // WASM doesn't have runlevels. Always return normal.
  return RunLevel::NORMAL;
#else  // _WIN32
  if (type == SERVER || type == RENDERER) {
    if (::geteuid() == 0) {
      // This process is started by root, or the executable is setuid to root.

      // TODO(yusukes): It would be better to add 'SAFE' run-level which
      //  prohibits all mutable operations to local resources and return the
      //  level after calling chroot("/somewhere/safe"), setgid("nogroup"),
      //  and setuid("nobody") here. This is because many novice Linux users
      //  tend to login to their desktop as root.

      return RunLevel::DENY;
    }
    if (::getuid() == 0) {
      // The executable is setuided to non-root and is started by root user?
      // This is unexpected. Returns DENY.
      return RunLevel::DENY;
    }
    return RunLevel::NORMAL;
  }

  // type is 'CLIENT'
  if (::geteuid() == 0 || ::getuid() == 0) {
    // When mozc.so is loaded into a privileged process, deny clients to use
    // dictionary_tool and config_dialog.
    return RunLevel::DENY;
  }

  return RunLevel::NORMAL;

#endif  // _WIN32
}

bool RunLevel::IsProcessInJob() {
#ifdef _WIN32
  // Check to see if we're in a job where
  // we can't create a child in our sandbox

  JOBOBJECT_EXTENDED_LIMIT_INFORMATION JobExtLimitInfo;
  // Get the job information of the current process
  if (!::QueryInformationJobObject(nullptr, JobObjectExtendedLimitInformation,
                                   &JobExtLimitInfo, sizeof(JobExtLimitInfo),
                                   nullptr)) {
    return false;
  }

  // Check to see if we can break away from the current job
  if (JobExtLimitInfo.BasicLimitInformation.LimitFlags &
      (JOB_OBJECT_LIMIT_BREAKAWAY_OK | JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK)) {
    // We're in a job, but it allows to break away.
    return false;
  }

  return true;
#else   // _WIN32
  return false;
#endif  // _WIN32
}

bool RunLevel::IsElevatedByUAC() {
#ifdef _WIN32
  // Get process token
  wil::unique_process_handle process_token;
  if (!::OpenProcessToken(::GetCurrentProcess(),
                          TOKEN_QUERY | TOKEN_QUERY_SOURCE,
                          process_token.put())) {
    return false;
  }

  return mozc::IsElevatedByUAC(process_token.get());
#else   // _WIN32
  return false;
#endif  // _WIN32
}

bool RunLevel::SetElevatedProcessDisabled(bool disable) {
#ifdef _WIN32
  HKEY key = 0;
  LONG result =
      ::RegCreateKeyExW(HKEY_CURRENT_USER, kElevatedProcessDisabledKey, 0,
                        nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);

  if (ERROR_SUCCESS != result) {
    return false;
  }

  const DWORD value = disable ? 1 : 0;
  result =
      ::RegSetValueExW(key, kElevatedProcessDisabledName, 0, REG_DWORD,
                       reinterpret_cast<const BYTE*>(&value), sizeof(value));
  ::RegCloseKey(key);

  return ERROR_SUCCESS == result;
#else   // _WIN32
  return false;
#endif  // _WIN32
}

bool RunLevel::GetElevatedProcessDisabled() {
#ifdef _WIN32
  HKEY key = 0;
  LONG result = ::RegOpenKeyExW(HKEY_CURRENT_USER, kElevatedProcessDisabledKey,
                                0, KEY_READ, &key);
  if (ERROR_SUCCESS != result) {
    return false;
  }

  DWORD value = 0;
  DWORD value_size = sizeof(value);
  DWORD value_type = 0;
  result =
      ::RegQueryValueEx(key, kElevatedProcessDisabledName, nullptr, &value_type,
                        reinterpret_cast<BYTE*>(&value), &value_size);
  ::RegCloseKey(key);

  if (ERROR_SUCCESS != result || value_type != REG_DWORD ||
      value_size != sizeof(value)) {
    return false;
  }

  return value > 0;
#else   // _WIN32
  return false;
#endif  // _WIN32
}
}  // namespace mozc
