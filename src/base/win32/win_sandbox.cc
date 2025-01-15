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

#include "base/win32/win_sandbox.h"

#include <aclapi.h>
#include <atlsecurity.h>
#include <sddl.h>
#include <wil/resource.h>
#include <windows.h>
// clang-format off
#include <strsafe.h>  // needs to come after other headers
// clang-format on

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/strings/zstring_view.h"
#include "base/system_util.h"
#include "base/win32/wide_char.h"

namespace mozc {
namespace {

using ::mozc::win32::StrAppendW;
using ::mozc::win32::StrCatW;

using UniquePACL = wil::unique_any<ACL *, decltype(::LocalFree), ::LocalFree>;

wil::unique_process_handle OpenEffectiveToken(const DWORD desired_access) {
  wil::unique_process_handle token;

  if (!::OpenThreadToken(::GetCurrentThread(), desired_access, TRUE,
                         token.put())) {
    if (ERROR_NO_TOKEN != ::GetLastError()) {
      return nullptr;
    }
    if (!::OpenProcessToken(::GetCurrentProcess(), desired_access,
                            token.put())) {
      return nullptr;
    }
  }
  return token;
}

template <typename T>
wil::unique_hlocal_secure_ptr<T> AllocGetTokenInformation(
    const HANDLE hToken, const TOKEN_INFORMATION_CLASS token_info_class) {
  DWORD buffer_size = 0;
  DWORD return_size = 0;

  const BOOL bReturn =
      ::GetTokenInformation(hToken, token_info_class, nullptr, 0, &buffer_size);
  if (bReturn || (ERROR_INSUFFICIENT_BUFFER != ::GetLastError())) {
    return nullptr;
  }

  wil::unique_hlocal_secure_ptr<T> buffer(
      static_cast<T *>(::LocalAlloc(LPTR, buffer_size)));
  if (buffer == nullptr) {
    return nullptr;
  }

  if (!::GetTokenInformation(hToken, token_info_class, buffer.get(),
                             buffer_size, &return_size)) {
    return nullptr;
  }
  return buffer;
}

wil::unique_hlocal_string_secure GetTokenUserSidStringW(const HANDLE token) {
  wil::unique_hlocal_secure_ptr<TOKEN_USER> token_user =
      AllocGetTokenInformation<TOKEN_USER>(token, TokenUser);
  wil::unique_hlocal_string_secure sid_string;
  if (token_user && ::ConvertSidToStringSidW(token_user->User.Sid,
                                             wil::out_param(sid_string))) {
    return sid_string;
  }
  return nullptr;
}

wil::unique_hlocal_string_secure GetTokenPrimaryGroupSidStringW(
    const HANDLE token) {
  wil::unique_hlocal_secure_ptr<TOKEN_PRIMARY_GROUP> token_primary_group =
      AllocGetTokenInformation<TOKEN_PRIMARY_GROUP>(token, TokenPrimaryGroup);
  wil::unique_hlocal_string_secure sid_string;
  if (token_primary_group &&
      ::ConvertSidToStringSidW(token_primary_group->PrimaryGroup,
                               wil::out_param(sid_string))) {
    return sid_string;
  }
  return nullptr;
}

bool GetUserSid(std::wstring &token_user_sid,
                std::wstring &token_primary_group_sid) {
  wil::unique_process_handle token = OpenEffectiveToken(TOKEN_QUERY);
  if (!token) {
    LOG(ERROR) << "OpenEffectiveToken failed " << ::GetLastError();
    return false;
  }

  // Get token user SID
  wil::unique_hlocal_string_secure sid_string =
      GetTokenUserSidStringW(token.get());
  if (!sid_string) {
    LOG(ERROR) << "GetTokenUserSidStringW failed " << ::GetLastError();
    return false;
  }
  token_user_sid = sid_string.get();

  // Get token primary group SID
  sid_string = GetTokenPrimaryGroupSidStringW(token.get());
  if (!sid_string) {
    LOG(ERROR) << "GetTokenPrimaryGroupSidStringW failed " << ::GetLastError();
    return false;
  }
  token_primary_group_sid = sid_string.get();

  return true;
}

std::wstring Allow(std::wstring_view access_right,
                   std::wstring_view account_sid) {
  return StrCatW(L"(", SDDL_ACCESS_ALLOWED, L";;", access_right, L";;;",
                 account_sid, L")");
}

std::wstring Deny(std::wstring_view access_right,
                  std::wstring_view account_sid) {
  return StrCatW(L"(", SDDL_ACCESS_DENIED, L";;", access_right, L";;;",
                 account_sid, L")");
}

std::wstring MandatoryLevel(std::wstring_view mandatory_label,
                            std::wstring_view integrity_levels) {
  return StrCatW(L"(", SDDL_MANDATORY_LABEL, L";;", mandatory_label, L";;;",
                 integrity_levels, L")");
}

// SDDL_ALL_APP_PACKAGES is available on Windows SDK 8.0 and later.
#ifndef SDDL_ALL_APP_PACKAGES
#define SDDL_ALL_APP_PACKAGES L"AC"
#endif  // SDDL_ALL_APP_PACKAGES

// SDDL for PROCESS_QUERY_INFORMATION is not defined. So use hex digits instead.
#ifndef SDDL_PROCESS_QUERY_INFORMATION
static_assert(PROCESS_QUERY_INFORMATION == 0x0400,
              "PROCESS_QUERY_INFORMATION must be 0x0400");
#define SDDL_PROCESS_QUERY_INFORMATION L"0x0400"
#endif  // SDDL_PROCESS_QUERY_INFORMATION

// SDDL for PROCESS_QUERY_LIMITED_INFORMATION is not defined. So use hex digits
// instead.
#ifndef SDDL_PROCESS_QUERY_LIMITED_INFORMATION
static_assert(PROCESS_QUERY_LIMITED_INFORMATION == 0x1000,
              "PROCESS_QUERY_LIMITED_INFORMATION must be 0x1000");
#define SDDL_PROCESS_QUERY_LIMITED_INFORMATION L"0x1000"
#endif  // SDDL_PROCESS_QUERY_LIMITED_INFORMATION

}  // namespace

std::wstring WinSandbox::GetSDDL(ObjectSecurityType shareble_object_type,
                                 std::wstring_view token_user_sid,
                                 std::wstring_view token_primary_group_sid) {
  // See
  // http://social.msdn.microsoft.com/Forums/en-US/windowssecurity/thread/e92502b1-0b9f-4e02-9d72-e4e47e924a8f/
  // for how to access named objects from an AppContainer.

  std::wstring dacl;
  std::wstring sacl;
  switch (shareble_object_type) {
    case WinSandbox::kSharablePipe:
      // Strip implicit owner rights
      // http://technet.microsoft.com/en-us/library/dd125370.aspx
      dacl += Allow(L"", SDDL_OWNER_RIGHTS);
      // Allow general access to LocalSystem
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_LOCAL_SYSTEM);
      // Allow general access to Built-in Administorators
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_BUILTIN_ADMINISTRATORS);
      // Allow general access to ALL APPLICATION PACKAGES
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_ALL_APP_PACKAGES);
      // Allow general access to the current user
      dacl += Allow(SDDL_GENERIC_ALL, token_user_sid);
      // Allow read/write access to low integrity
      sacl += MandatoryLevel(SDDL_NO_EXECUTE_UP, SDDL_ML_LOW);
      break;
    case WinSandbox::kLooseSharablePipe:
      // Strip implicit owner rights
      // http://technet.microsoft.com/en-us/library/dd125370.aspx
      dacl += Allow(L"", SDDL_OWNER_RIGHTS);
      // Allow general access to LocalSystem
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_LOCAL_SYSTEM);
      // Allow general access to Built-in Administorators
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_BUILTIN_ADMINISTRATORS);
      // Allow general access to ALL APPLICATION PACKAGES
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_ALL_APP_PACKAGES);
      // Allow general access to the current user
      dacl += Allow(SDDL_GENERIC_ALL, token_user_sid);
      // Skip 2nd-phase ACL validation against restricted tokens.
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_RESTRICTED_CODE);
      // Allow read/write access to low integrity
      sacl += MandatoryLevel(SDDL_NO_EXECUTE_UP, SDDL_ML_LOW);
      break;
    case WinSandbox::kSharableEvent:
      // Strip implicit owner rights
      // http://technet.microsoft.com/en-us/library/dd125370.aspx
      dacl += Allow(L"", SDDL_OWNER_RIGHTS);
      // Allow general access to LocalSystem
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_LOCAL_SYSTEM);
      // Allow general access to Built-in Administorators
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_BUILTIN_ADMINISTRATORS);
      // Allow state change/synchronize to ALL APPLICATION PACKAGES
      dacl += Allow(SDDL_GENERIC_EXECUTE, SDDL_ALL_APP_PACKAGES);
      // Allow general access to the current user
      dacl += Allow(SDDL_GENERIC_ALL, token_user_sid);
      // Skip 2nd-phase ACL validation against restricted tokens regarding
      // change/synchronize.
      dacl += Allow(SDDL_GENERIC_EXECUTE, SDDL_RESTRICTED_CODE);
      // Allow read/write access to low integrity
      sacl += MandatoryLevel(SDDL_NO_EXECUTE_UP, SDDL_ML_LOW);
      break;
    case WinSandbox::kSharableMutex:
      // Strip implicit owner rights
      // http://technet.microsoft.com/en-us/library/dd125370.aspx
      dacl += Allow(L"", SDDL_OWNER_RIGHTS);
      // Allow general access to LocalSystem
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_LOCAL_SYSTEM);
      // Allow general access to Built-in Administorators
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_BUILTIN_ADMINISTRATORS);
      // Allow state change/synchronize to ALL APPLICATION PACKAGES
      dacl += Allow(SDDL_GENERIC_EXECUTE, SDDL_ALL_APP_PACKAGES);
      // Allow general access to the current user
      dacl += Allow(SDDL_GENERIC_ALL, token_user_sid);
      // Skip 2nd-phase ACL validation against restricted tokens regarding
      // change/synchronize.
      dacl += Allow(SDDL_GENERIC_EXECUTE, SDDL_RESTRICTED_CODE);
      // Allow read/write access to low integrity
      sacl += MandatoryLevel(SDDL_NO_EXECUTE_UP, SDDL_ML_LOW);
      break;
    case WinSandbox::kSharableFileForRead:
      // Strip implicit owner rights
      // http://technet.microsoft.com/en-us/library/dd125370.aspx
      dacl += Allow(L"", SDDL_OWNER_RIGHTS);
      // Allow general access to LocalSystem
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_LOCAL_SYSTEM);
      // Allow general access to Built-in Administorators
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_BUILTIN_ADMINISTRATORS);
      // Allow read access to low integrity
      // Allow general read access to ALL APPLICATION PACKAGES
      dacl += Allow(SDDL_GENERIC_READ, SDDL_ALL_APP_PACKAGES);
      // Allow general access to the current user
      dacl += Allow(SDDL_GENERIC_ALL, token_user_sid);
      // Skip 2nd-phase ACL validation against restricted tokens regarding
      // general read access.
      dacl += Allow(SDDL_GENERIC_READ, SDDL_RESTRICTED_CODE);
      // Allow read access to low integrity
      sacl += MandatoryLevel(SDDL_NO_WRITE_UP SDDL_NO_EXECUTE_UP, SDDL_ML_LOW);
      break;
    case WinSandbox::kIPCServerProcess:
      // Strip implicit owner rights
      // http://technet.microsoft.com/en-us/library/dd125370.aspx
      dacl += Allow(L"", SDDL_OWNER_RIGHTS);
      // Allow general access to LocalSystem
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_LOCAL_SYSTEM);
      // Allow general access to Built-in Administorators
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_BUILTIN_ADMINISTRATORS);
      // Allow PROCESS_QUERY_LIMITED_INFORMATION to ALL APPLICATION PACKAGES
      dacl +=
          Allow(SDDL_PROCESS_QUERY_LIMITED_INFORMATION, SDDL_ALL_APP_PACKAGES);
      // Allow general access to the current user
      dacl += Allow(SDDL_GENERIC_ALL, token_user_sid);
      // Allow PROCESS_QUERY_LIMITED_INFORMATION to restricted tokens
      dacl +=
          Allow(SDDL_PROCESS_QUERY_LIMITED_INFORMATION, SDDL_RESTRICTED_CODE);
      break;
    case WinSandbox::kPrivateObject:
    default:
      // Strip implicit owner rights
      // http://technet.microsoft.com/en-us/library/dd125370.aspx
      dacl += Allow(L"", SDDL_OWNER_RIGHTS);
      // Allow general access to LocalSystem
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_LOCAL_SYSTEM);
      // Allow general access to Built-in Administorators
      dacl += Allow(SDDL_GENERIC_ALL, SDDL_BUILTIN_ADMINISTRATORS);
      // Allow general access to the current user
      dacl += Allow(SDDL_GENERIC_ALL, token_user_sid);
      break;
  }

  std::wstring sddl;
  // Owner SID
  StrAppendW(&sddl, SDDL_OWNER SDDL_DELIMINATOR, token_user_sid);
  // Primary Group SID
  StrAppendW(&sddl, SDDL_GROUP SDDL_DELIMINATOR, token_primary_group_sid);
  // DACL
  if (!dacl.empty()) {
    StrAppendW(&sddl, SDDL_DACL SDDL_DELIMINATOR, dacl);
  }
  // SACL
  if (!sacl.empty()) {
    StrAppendW(&sddl, SDDL_SACL SDDL_DELIMINATOR, sacl);
  }

  return sddl;
}

Sid::Sid(const SID *sid) {
  ::CopySid(sizeof(sid_), sid_, const_cast<SID *>(sid));
}

Sid::Sid(WELL_KNOWN_SID_TYPE type) {
  DWORD size_sid = sizeof(sid_);
  ::CreateWellKnownSid(type, nullptr, sid_, &size_sid);
}

const SID *Sid::GetPSID() const { return reinterpret_cast<const SID *>(sid_); }

SID *Sid::GetPSID() { return reinterpret_cast<SID *>(sid_); }

std::wstring Sid::GetName() const {
  Sid temp_sid(GetPSID());
  wil::unique_hlocal_string temp_str;
  ConvertSidToStringSidW(temp_sid.GetPSID(), wil::out_param(temp_str));
  return std::wstring(temp_str.get());
}

std::wstring Sid::GetAccountName() const {
  DWORD name_size = 0;
  DWORD domain_name_size = 0;
  SID_NAME_USE name_use;
  Sid temp_sid(GetPSID());
  ::LookupAccountSid(nullptr, temp_sid.GetPSID(), nullptr, &name_size, nullptr,
                     &domain_name_size, &name_use);
  if (domain_name_size == 0) {
    if (name_size == 0) {
      // Use string SID instead.
      return GetName();
    }
    auto name_buffer = std::make_unique<wchar_t[]>(name_size);
    ::LookupAccountSid(nullptr, temp_sid.GetPSID(), name_buffer.get(),
                       &name_size, nullptr, &domain_name_size, &name_use);
    return StrCatW(L"/", name_buffer.get());
  }
  auto name_buffer = std::make_unique<wchar_t[]>(name_size);
  auto domain_name_buffer = std::make_unique<wchar_t[]>(domain_name_size);
  ::LookupAccountSid(nullptr, temp_sid.GetPSID(), name_buffer.get(), &name_size,
                     domain_name_buffer.get(), &domain_name_size, &name_use);
  return StrCatW(domain_name_buffer.get(), L"/", name_buffer.get());
}

// make SecurityAttributes for the named pipe.
bool WinSandbox::MakeSecurityAttributes(
    ObjectSecurityType shareble_object_type,
    SECURITY_ATTRIBUTES *security_attributes) {
  std::wstring token_user_sid;
  std::wstring token_primary_group_sid;
  if (!GetUserSid(token_user_sid, token_primary_group_sid)) {
    return false;
  }

  const std::wstring &sddl =
      GetSDDL(shareble_object_type, token_user_sid, token_primary_group_sid);

  // Create self-relative SD
  wil::unique_hlocal_security_descriptor self_relative_desc;
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(
          sddl.c_str(), SDDL_REVISION_1, self_relative_desc.put(), nullptr)) {
    LOG(ERROR)
        << "ConvertStringSecurityDescriptorToSecurityDescriptorW failed: "
        << ::GetLastError();
    return false;
  }

  // Set up security attributes
  security_attributes->nLength = sizeof(SECURITY_ATTRIBUTES);
  security_attributes->lpSecurityDescriptor = self_relative_desc.release();
  security_attributes->bInheritHandle = FALSE;

  return true;
}

bool WinSandbox::AddKnownSidToKernelObject(HANDLE object, const SID *known_sid,
                                           DWORD inheritance_flag,
                                           ACCESS_MASK access_mask) {
  // We must pass |&descriptor| because 6th argument (|&old_dacl|) is
  // non-null.  Actually, returned |old_dacl| points the memory block
  // of |descriptor|, which must be freed by ::LocalFree API.
  // http://msdn.microsoft.com/en-us/library/aa446654.aspx
  wil::unique_hlocal_security_descriptor descriptor;
  PACL old_dacl = nullptr;
  DWORD error =
      ::GetSecurityInfo(object, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                        nullptr, nullptr, &old_dacl, nullptr, descriptor.put());
  // You need not to free |old_dacl| because |old_dacl| points inside of
  // |descriptor|.

  if (error != ERROR_SUCCESS) {
    DLOG(ERROR) << "GetSecurityInfo failed" << error;
    return false;
  }

  EXPLICIT_ACCESS new_access = {};
  new_access.grfAccessMode = GRANT_ACCESS;
  new_access.grfAccessPermissions = access_mask;
  new_access.grfInheritance = inheritance_flag;
  new_access.Trustee.pMultipleTrustee = nullptr;
  new_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
  new_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  // When |TrusteeForm| is TRUSTEE_IS_SID, |ptstrName| is a pointer to the SID
  // of the trustee.
  // http://msdn.microsoft.com/en-us/library/aa379636.aspx
  new_access.Trustee.ptstrName =
      reinterpret_cast<wchar_t *>(const_cast<SID *>(known_sid));

  UniquePACL new_dacl;
  error = ::SetEntriesInAcl(1, &new_access, old_dacl, new_dacl.put());
  if (error != ERROR_SUCCESS) {
    DLOG(ERROR) << "SetEntriesInAcl failed" << error;
    return false;
  }

  error = ::SetSecurityInfo(object, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION,
                            nullptr, nullptr, new_dacl.get(), nullptr);
  if (error != ERROR_SUCCESS) {
    DLOG(ERROR) << "SetSecurityInfo failed" << error;
    return false;
  }

  return true;
}

// Local functions for SpawnSandboxedProcess.
namespace {
// This Windows job object wrapper class corresponds to the Job class in
// Chromium sandbox library with JOB_LOCKDOWN except for LockedDownJob does not
// set JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, which is not required by Mozc.
// http://src.chromium.org/viewvc/chrome/trunk/src/sandbox/src/security_level.h?view=markup
class LockedDownJob {
 public:
  LockedDownJob() : job_handle_(nullptr) {}
  LockedDownJob(const LockedDownJob &) = delete;
  LockedDownJob &operator=(const LockedDownJob &) = delete;

  ~LockedDownJob() {
    if (job_handle_ != nullptr) {
      ::CloseHandle(job_handle_);
      job_handle_ = nullptr;
    }
  }

  bool IsValid() const { return (job_handle_ != nullptr); }

  DWORD Init(const wchar_t *job_name, bool allow_ui_operation) {
    if (job_handle_ != nullptr) {
      return ERROR_ALREADY_INITIALIZED;
    }
    job_handle_ = ::CreateJobObject(nullptr, job_name);
    if (job_handle_ == nullptr) {
      return ::GetLastError();
    }
    {
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION limit_info = {};
      limit_info.BasicLimitInformation.ActiveProcessLimit = 1;
      limit_info.BasicLimitInformation.LimitFlags =
          // Mozc does not use JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE so that the
          // child process can continue running even after the parent is
          // terminated.
          // JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
          JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION |
          JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
      if (::SetInformationJobObject(job_handle_,
                                    JobObjectExtendedLimitInformation,
                                    &limit_info, sizeof(limit_info))) {
        return ::GetLastError();
      }
    }

    if (!allow_ui_operation) {
      JOBOBJECT_BASIC_UI_RESTRICTIONS ui_restrictions = {};
      ui_restrictions.UIRestrictionsClass =
          JOB_OBJECT_UILIMIT_WRITECLIPBOARD | JOB_OBJECT_UILIMIT_READCLIPBOARD |
          JOB_OBJECT_UILIMIT_HANDLES | JOB_OBJECT_UILIMIT_GLOBALATOMS |
          JOB_OBJECT_UILIMIT_DISPLAYSETTINGS |
          JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS | JOB_OBJECT_UILIMIT_DESKTOP |
          JOB_OBJECT_UILIMIT_EXITWINDOWS;
      if (!::SetInformationJobObject(job_handle_, JobObjectBasicUIRestrictions,
                                     &ui_restrictions,
                                     sizeof(ui_restrictions))) {
        return ::GetLastError();
      }
    }
    return ERROR_SUCCESS;
  }

  DWORD AssignProcessToJob(HANDLE process_handle) {
    if (job_handle_ == nullptr) {
      return ERROR_NO_DATA;
    }
    if (!::AssignProcessToJobObject(job_handle_, process_handle)) {
      return ::GetLastError();
    }
    return ERROR_SUCCESS;
  }

 private:
  HANDLE job_handle_;
};

std::optional<wil::unique_process_information> CreateSuspendedRestrictedProcess(
    std::wstring command_line, const WinSandbox::SecurityInfo &info) {
  wil::unique_process_handle process_token;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                          process_token.put())) {
    return std::nullopt;
  }

  wil::unique_process_handle primary_token =
      WinSandbox::GetRestrictedTokenHandle(
          process_token.get(), info.primary_level, info.integrity_level);
  if (!primary_token) {
    return std::nullopt;
  }

  wil::unique_process_handle impersonation_token =
      WinSandbox::GetRestrictedTokenHandleForImpersonation(
          process_token.get(), info.impersonation_level, info.integrity_level);
  if (!impersonation_token) {
    return std::nullopt;
  }

  PSECURITY_ATTRIBUTES security_attributes_ptr = nullptr;
  SECURITY_ATTRIBUTES security_attributes = {};
  if (WinSandbox::MakeSecurityAttributes(WinSandbox::kIPCServerProcess,
                                         &security_attributes)) {
    security_attributes_ptr = &security_attributes;
    // Override the impersonation thread token's DACL to avoid http://b/1728895
    // On Windows Server, the objects created by a member of
    // the built-in administrators group do not always explicitly
    // allow the current user to access the objects.
    // Instead, such objects implicitly allow the user by allowing
    // the built-in administratros group.
    // However, Mozc asks Sandbox to remove the built-in administrators
    // group from the current user's groups. Thus the impersonation thread
    // cannot even look at its own thread token.
    // That prevents GetRunLevel() from verifying its own thread identity.
    // Note: Overriding the thread token's
    // DACL will not elevate the thread's running context.
    if (!::SetKernelObjectSecurity(
            impersonation_token.get(), DACL_SECURITY_INFORMATION,
            security_attributes_ptr->lpSecurityDescriptor)) {
      const DWORD last_error = ::GetLastError();
      DLOG(ERROR) << "SetKernelObjectSecurity failed. Error: " << last_error;
      return std::nullopt;
    }
  }

  DWORD creation_flags = info.creation_flags | CREATE_SUSPENDED;
  // Note: If the current process is already in a job, you cannot use
  // CREATE_BREAKAWAY_FROM_JOB.  See b/1571395
  if (info.use_locked_down_job) {
    creation_flags |= CREATE_BREAKAWAY_FROM_JOB;
  }

  const wchar_t *startup_directory =
      (info.in_system_dir ? SystemUtil::GetSystemDir() : nullptr);

  STARTUPINFO startup_info = {sizeof(STARTUPINFO)};
  wil::unique_process_information process_info;
  // 3rd parameter of CreateProcessAsUser must be a writable buffer.
  if (!::CreateProcessAsUser(primary_token.get(),
                             nullptr,              // No application name.
                             command_line.data(),  // must be writable.
                             security_attributes_ptr, nullptr,
                             FALSE,  // Do not inherit handles.
                             creation_flags,
                             nullptr,  // Use the environment of the caller.
                             startup_directory, &startup_info,
                             process_info.reset_and_addressof())) {
    const DWORD last_error = ::GetLastError();
    DLOG(ERROR) << "CreateProcessAsUser failed. Error: " << last_error;
    return std::nullopt;
  }

  if (security_attributes_ptr != nullptr) {
    ::LocalFree(security_attributes_ptr->lpSecurityDescriptor);
  }

  // Change the token of the main thread of the new process for the
  // impersonation token with more rights.
  if (!::SetThreadToken(&process_info.hThread, impersonation_token.get())) {
    const DWORD last_error = ::GetLastError();
    DLOG(ERROR) << "SetThreadToken failed. Error: " << last_error;
    ::TerminateProcess(process_info.hProcess, 0);
    return std::nullopt;
  }

  return process_info;
}

bool SpawnSandboxedProcessImpl(std::wstring_view command_line,
                               const WinSandbox::SecurityInfo &info,
                               DWORD &pid) {
  LockedDownJob job;

  if (info.use_locked_down_job) {
    const DWORD error_code = job.Init(nullptr, info.allow_ui_operation);
    if (error_code != ERROR_SUCCESS) {
      return false;
    }
  }

  std::optional<wil::unique_process_information> process_info =
      CreateSuspendedRestrictedProcess(std::wstring(command_line), info);
  if (!process_info.has_value()) {
    return false;
  }
  pid = process_info->dwProcessId;

  if (job.IsValid()) {
    const DWORD error_code = job.AssignProcessToJob(process_info->hProcess);
    if (error_code != ERROR_SUCCESS) {
      ::TerminateProcess(process_info->hProcess, 0);
      return false;
    }
  }

  ::ResumeThread(process_info->hThread);

  return true;
}

}  // namespace

WinSandbox::SecurityInfo::SecurityInfo()
    : primary_level(WinSandbox::USER_LOCKDOWN),
      impersonation_level(WinSandbox::USER_LOCKDOWN),
      integrity_level(WinSandbox::INTEGRITY_LEVEL_SYSTEM),
      creation_flags(0),
      use_locked_down_job(false),
      allow_ui_operation(false),
      in_system_dir(false) {}

bool WinSandbox::SpawnSandboxedProcess(absl::string_view path,
                                       absl::string_view arg,
                                       const SecurityInfo &info, DWORD *pid) {
  std::wstring wpath = StrCatW(L"\"", win32::Utf8ToWide(path), L"\"");
  if (!arg.empty()) {
    StrAppendW(&wpath, L" ", win32::Utf8ToWide(arg));
  }

  if (!SpawnSandboxedProcessImpl(wpath, info, *pid)) {
    return false;
  }

  return true;
}

// Utility functions and classes for GetRestrictionInfo
namespace {
// A utility class for GetTokenInformation API.
// This class manages data buffer into which |TokenDataType| type data
// is filled.
template <TOKEN_INFORMATION_CLASS TokenClass, typename TokenDataType>
class ScopedTokenInfo {
 public:
  explicit ScopedTokenInfo(HANDLE token) : initialized_(false) {
    DWORD num_bytes = 0;
    ::GetTokenInformation(token, TokenClass, nullptr, 0, &num_bytes);
    if (num_bytes == 0) {
      return;
    }
    buffer_.reset(new BYTE[num_bytes]);
    TokenDataType *all_token_groups =
        reinterpret_cast<TokenDataType *>(buffer_.get());
    if (!::GetTokenInformation(token, TokenClass, all_token_groups, num_bytes,
                               &num_bytes)) {
      const DWORD last_error = ::GetLastError();
      DLOG(ERROR) << "GetTokenInformation failed. Last error: " << last_error;
      buffer_.reset();
      return;
    }
    initialized_ = true;
  }
  TokenDataType *get() const {
    return reinterpret_cast<TokenDataType *>(buffer_.get());
  }
  TokenDataType *operator->() const { return get(); }

 private:
  std::unique_ptr<BYTE[]> buffer_;
  bool initialized_;
};

// Wrapper class for the SID_AND_ATTRIBUTES structure.
class SidAndAttributes {
 public:
  SidAndAttributes() : sid_(static_cast<SID *>(nullptr)), attributes_(0) {}
  SidAndAttributes(Sid sid, DWORD attributes)
      : sid_(sid), attributes_(attributes) {}
  const Sid &sid() const { return sid_; }
  const DWORD &attributes() const { return attributes_; }
  bool HasAttribute(DWORD attribute) const {
    return (attributes_ & attribute) == attribute;
  }

 private:
  Sid sid_;
  DWORD attributes_;
};

// Returns all the 'TokenGroups' information of the specified |token_handle|.
std::vector<SidAndAttributes> GetAllTokenGroups(HANDLE token_handle) {
  std::vector<SidAndAttributes> result;
  ScopedTokenInfo<TokenGroups, TOKEN_GROUPS> all_token_groups(token_handle);
  if (all_token_groups.get() == nullptr) {
    return result;
  }
  for (size_t i = 0; i < all_token_groups->GroupCount; ++i) {
    Sid sid(static_cast<SID *>(all_token_groups->Groups[i].Sid));
    const DWORD attributes = all_token_groups->Groups[i].Attributes;
    result.push_back(SidAndAttributes(sid, attributes));
  }
  return result;
}

std::vector<SidAndAttributes> FilterByHavingAttribute(
    absl::Span<const SidAndAttributes> source, DWORD attribute) {
  std::vector<SidAndAttributes> result;
  for (size_t i = 0; i < source.size(); ++i) {
    if (source[i].HasAttribute(attribute)) {
      result.push_back(source[i]);
    }
  }
  return result;
}

std::vector<SidAndAttributes> FilterByNotHavingAttribute(
    absl::Span<const SidAndAttributes> source, DWORD attribute) {
  std::vector<SidAndAttributes> result;
  for (size_t i = 0; i < source.size(); ++i) {
    if (!source[i].HasAttribute(attribute)) {
      result.push_back(source[i]);
    }
  }
  return result;
}

template <size_t NumExceptions>
std::vector<Sid> FilterSidExceptFor(
    absl::Span<const SidAndAttributes> source_sids,
    const WELL_KNOWN_SID_TYPE (&exception_sids)[NumExceptions]) {
  std::vector<Sid> result;
  // find logon_sid.
  for (size_t i = 0; i < source_sids.size(); ++i) {
    bool in_the_exception_list = false;
    for (size_t j = 0; j < NumExceptions; ++j) {
      // These variables must be non-const because EqualSid API requires
      // non-const pointer.
      Sid source = source_sids[i].sid();
      Sid except(exception_sids[j]);
      if (::EqualSid(source.GetPSID(), except.GetPSID())) {
        in_the_exception_list = true;
        break;
      }
    }
    if (!in_the_exception_list) {
      result.push_back(source_sids[i].sid());
    }
  }
  return result;
}

template <size_t NumExceptions>
std::vector<LUID> FilterPrivilegesExceptFor(
    const absl::Span<const LUID_AND_ATTRIBUTES> source_privileges,
    const wchar_t *(&exception_privileges)[NumExceptions]) {
  std::vector<LUID> result;
  for (size_t i = 0; i < source_privileges.size(); ++i) {
    bool in_the_exception_list = false;
    for (size_t j = 0; j < NumExceptions; ++j) {
      const LUID source = source_privileges[i].Luid;
      LUID except = {};
      ::LookupPrivilegeValue(nullptr, exception_privileges[j], &except);
      if ((source.HighPart == except.HighPart) &&
          (source.LowPart == except.LowPart)) {
        in_the_exception_list = true;
        break;
      }
    }
    if (!in_the_exception_list) {
      result.push_back(source_privileges[i].Luid);
    }
  }
  return result;
}

std::optional<SidAndAttributes> GetUserSid(HANDLE token) {
  ScopedTokenInfo<TokenUser, TOKEN_USER> token_user(token);
  if (token_user.get() == nullptr) {
    return std::nullopt;
  }

  Sid sid(static_cast<SID *>(token_user->User.Sid));
  const DWORD attributes = token_user->User.Attributes;
  return SidAndAttributes(sid, attributes);
}

std::vector<LUID_AND_ATTRIBUTES> GetPrivileges(HANDLE token) {
  std::vector<LUID_AND_ATTRIBUTES> result;
  ScopedTokenInfo<TokenPrivileges, TOKEN_PRIVILEGES> token_privileges(token);
  if (token_privileges.get() == nullptr) {
    return result;
  }

  for (size_t i = 0; i < token_privileges->PrivilegeCount; ++i) {
    result.push_back(token_privileges->Privileges[i]);
  }

  return result;
}

wil::unique_process_handle CreateRestrictedTokenImpl(
    HANDLE effective_token, WinSandbox::TokenLevel security_level) {
  const std::vector<Sid> sids_to_disable =
      WinSandbox::GetSidsToDisable(effective_token, security_level);
  const std::vector<LUID> privileges_to_disable =
      WinSandbox::GetPrivilegesToDisable(effective_token, security_level);
  const std::vector<Sid> sids_to_restrict =
      WinSandbox::GetSidsToRestrict(effective_token, security_level);

  if ((sids_to_disable.empty()) && (privileges_to_disable.empty()) &&
      (sids_to_restrict.empty())) {
    // Duplicate the token even if it's not modified at this point
    // because any subsequent changes to this token would also affect the
    // current process.
    wil::unique_process_handle new_token;
    const BOOL result = ::DuplicateTokenEx(effective_token, TOKEN_ALL_ACCESS,
                                           nullptr, SecurityIdentification,
                                           TokenPrimary, new_token.put());
    if (result == FALSE) {
      return nullptr;
    }
    return new_token;
  }

  std::unique_ptr<SID_AND_ATTRIBUTES[]> sids_to_disable_array;
  std::vector<Sid> sids_to_disable_array_buffer = sids_to_disable;
  {
    const size_t size = sids_to_disable.size();
    if (size > 0) {
      sids_to_disable_array = std::make_unique<SID_AND_ATTRIBUTES[]>(size);
      for (size_t i = 0; i < size; ++i) {
        sids_to_disable_array[i].Attributes = SE_GROUP_USE_FOR_DENY_ONLY;
        sids_to_disable_array[i].Sid =
            sids_to_disable_array_buffer[i].GetPSID();
      }
    }
  }

  std::unique_ptr<LUID_AND_ATTRIBUTES[]> privileges_to_disable_array;
  {
    const size_t size = privileges_to_disable.size();
    if (size > 0) {
      privileges_to_disable_array =
          std::make_unique<LUID_AND_ATTRIBUTES[]>(size);
      for (unsigned int i = 0; i < size; ++i) {
        privileges_to_disable_array[i].Attributes = 0;
        privileges_to_disable_array[i].Luid = privileges_to_disable[i];
      }
    }
  }

  std::unique_ptr<SID_AND_ATTRIBUTES[]> sids_to_restrict_array;
  std::vector<Sid> sids_to_restrict_array_buffer = sids_to_restrict;
  {
    const size_t size = sids_to_restrict.size();
    if (size > 0) {
      sids_to_restrict_array = std::make_unique<SID_AND_ATTRIBUTES[]>(size);
      for (size_t i = 0; i < size; ++i) {
        sids_to_restrict_array[i].Attributes = 0;
        sids_to_restrict_array[i].Sid =
            sids_to_restrict_array_buffer[i].GetPSID();
      }
    }
  }

  wil::unique_process_handle new_token;
  const BOOL result = ::CreateRestrictedToken(
      effective_token,
      SANDBOX_INERT,  // This flag is used on Windows 7
      static_cast<DWORD>(sids_to_disable.size()), sids_to_disable_array.get(),
      static_cast<DWORD>(privileges_to_disable.size()),
      privileges_to_disable_array.get(),
      static_cast<DWORD>(sids_to_restrict.size()), sids_to_restrict_array.get(),
      new_token.put());
  if (result == FALSE) {
    return nullptr;
  }
  return new_token;
}

bool AddSidToDefaultDacl(HANDLE token, const Sid &sid, ACCESS_MASK access) {
  if (token == nullptr) {
    return false;
  }

  ScopedTokenInfo<TokenDefaultDacl, TOKEN_DEFAULT_DACL> default_dacl(token);
  if (default_dacl.get() == nullptr) {
    return false;
  }

  UniquePACL new_dacl;
  {
    EXPLICIT_ACCESS new_access = {};
    new_access.grfAccessMode = GRANT_ACCESS;
    new_access.grfAccessPermissions = access;
    new_access.grfInheritance = NO_INHERITANCE;

    new_access.Trustee.pMultipleTrustee = nullptr;
    new_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    new_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    Sid temp_sid(sid);
    new_access.Trustee.ptstrName = reinterpret_cast<LPWSTR>(temp_sid.GetPSID());
    const DWORD result = ::SetEntriesInAcl(
        1, &new_access, default_dacl->DefaultDacl, new_dacl.put());
    if (result != ERROR_SUCCESS) {
      return false;
    }
  }

  TOKEN_DEFAULT_DACL new_token_dacl = {new_dacl.get()};
  const BOOL result = ::SetTokenInformation(
      token, TokenDefaultDacl, &new_token_dacl, sizeof(new_token_dacl));
  return (result != FALSE);
}

const wchar_t *GetPredefinedSidString(
    WinSandbox::IntegrityLevel integrity_level) {
  // Defined in the following documents.
  // http://msdn.microsoft.com/en-us/library/cc980032.aspx
  // http://support.microsoft.com/kb/243330
  switch (integrity_level) {
    case WinSandbox::INTEGRITY_LEVEL_SYSTEM:
      return L"S-1-16-16384";
    case WinSandbox::INTEGRITY_LEVEL_HIGH:
      return L"S-1-16-12288";
    case WinSandbox::INTEGRITY_LEVEL_MEDIUM_PLUS:
      return L"S-1-16-8448";
    case WinSandbox::INTEGRITY_LEVEL_MEDIUM:
      return L"S-1-16-8192";
    case WinSandbox::INTEGRITY_LEVEL_LOW:
      return L"S-1-16-4096";
    case WinSandbox::INTEGRITY_LEVEL_UNTRUSTED:
      return L"S-1-16-0";
    case WinSandbox::INTEGRITY_LEVEL_LAST:
      return nullptr;
  }

  return nullptr;
}

bool SetTokenIntegrityLevel(HANDLE token,
                            WinSandbox::IntegrityLevel integrity_level) {
  const wchar_t *sid_string = GetPredefinedSidString(integrity_level);
  if (sid_string == nullptr) {
    // do not change the integrity level.
    return true;
  }

  wil::unique_any_psid integrity_sid;
  if (!::ConvertStringSidToSid(sid_string, integrity_sid.put())) {
    return false;
  }
  TOKEN_MANDATORY_LABEL label = {{integrity_sid.get(), SE_GROUP_INTEGRITY}};
  const DWORD size =
      sizeof(TOKEN_MANDATORY_LABEL) + ::GetLengthSid(integrity_sid.get());
  const BOOL result =
      ::SetTokenInformation(token, TokenIntegrityLevel, &label, size);

  return (result != FALSE);
}

constexpr ACCESS_MASK GetAccessMask(
    WinSandbox::AppContainerVisibilityType type) {
  const ACCESS_MASK kBaseType =
      FILE_READ_DATA | FILE_READ_EA | READ_CONTROL | SYNCHRONIZE;

  switch (type) {
    case WinSandbox::AppContainerVisibilityType::kProgramFiles:
      // As of Windows 10 Anniversary Update, following access masks (==0x1200a9)
      // are specified to files under Program Files by default.
      return FILE_EXECUTE | kBaseType;
    case WinSandbox::AppContainerVisibilityType::kConfigFile:
      return FILE_GENERIC_READ | FILE_READ_ATTRIBUTES | kBaseType;
    default:
      return kBaseType;
  }
}

}  // namespace

std::vector<Sid> WinSandbox::GetSidsToDisable(HANDLE effective_token,
                                              TokenLevel security_level) {
  const std::vector<SidAndAttributes> all_token_groups =
      GetAllTokenGroups(effective_token);
  const std::optional<SidAndAttributes> current_user_sid =
      GetUserSid(effective_token);
  const std::vector<SidAndAttributes> normal_tokens =
      FilterByNotHavingAttribute(
          FilterByNotHavingAttribute(all_token_groups, SE_GROUP_LOGON_ID),
          SE_GROUP_INTEGRITY);

  std::vector<Sid> sids_to_disable;
  switch (security_level) {
    case USER_UNPROTECTED:
    case USER_RESTRICTED_SAME_ACCESS:
      sids_to_disable.clear();
      break;
    case USER_NON_ADMIN:
    case USER_INTERACTIVE: {
      const WELL_KNOWN_SID_TYPE kSidExceptions[] = {
          WinBuiltinUsersSid,
          WinWorldSid,
          WinInteractiveSid,
          WinAuthenticatedUserSid,
      };
      sids_to_disable = FilterSidExceptFor(normal_tokens, kSidExceptions);
      break;
    }
    case USER_LIMITED: {
      const WELL_KNOWN_SID_TYPE kSidExceptions[] = {
          WinBuiltinUsersSid,
          WinWorldSid,
          WinInteractiveSid,
      };
      sids_to_disable = FilterSidExceptFor(normal_tokens, kSidExceptions);
      break;
    }
    case USER_RESTRICTED:
    case USER_LOCKDOWN:
      if (current_user_sid.has_value()) {
        sids_to_disable.push_back(current_user_sid.value().sid());
      }
      for (size_t i = 0; i < normal_tokens.size(); ++i) {
        sids_to_disable.push_back(normal_tokens[i].sid());
      }
      break;
    default:
      DLOG(FATAL) << "unexpeced TokenLevel";
      break;
  }
  return sids_to_disable;
}

std::vector<LUID> WinSandbox::GetPrivilegesToDisable(
    HANDLE effective_token, TokenLevel security_level) {
  const std::vector<LUID_AND_ATTRIBUTES> all_privileges =
      GetPrivileges(effective_token);

  std::vector<LUID> privileges_to_disable;
  switch (security_level) {
    case USER_UNPROTECTED:
    case USER_RESTRICTED_SAME_ACCESS:
      privileges_to_disable.clear();
      break;
    case USER_NON_ADMIN:
    case USER_INTERACTIVE:
    case USER_LIMITED:
    case USER_RESTRICTED: {
      const wchar_t *kPrivilegeExceptions[] = {
          SE_CHANGE_NOTIFY_NAME,
      };
      privileges_to_disable =
          FilterPrivilegesExceptFor(all_privileges, kPrivilegeExceptions);
      break;
    }
    case USER_LOCKDOWN:
      for (size_t i = 0; i < all_privileges.size(); ++i) {
        privileges_to_disable.push_back(all_privileges[i].Luid);
      }
      break;
    default:
      DLOG(FATAL) << "unexpeced TokenLevel";
      break;
  }
  return privileges_to_disable;
}

std::vector<Sid> WinSandbox::GetSidsToRestrict(HANDLE effective_token,
                                               TokenLevel security_level) {
  const std::vector<SidAndAttributes> all_token_groups =
      GetAllTokenGroups(effective_token);
  const std::optional<SidAndAttributes> current_user_sid =
      GetUserSid(effective_token);
  const std::vector<SidAndAttributes> token_logon_session =
      FilterByHavingAttribute(all_token_groups, SE_GROUP_LOGON_ID);

  std::vector<Sid> sids_to_restrict;
  switch (security_level) {
    case USER_UNPROTECTED:
      sids_to_restrict.clear();
      break;
    case USER_RESTRICTED_SAME_ACCESS: {
      if (current_user_sid.has_value()) {
        sids_to_restrict.push_back(current_user_sid.value().sid());
      }
      const std::vector<SidAndAttributes> tokens =
          FilterByNotHavingAttribute(all_token_groups, SE_GROUP_INTEGRITY);
      for (size_t i = 0; i < tokens.size(); ++i) {
        sids_to_restrict.push_back(tokens[i].sid());
      }
      break;
    }
    case USER_NON_ADMIN:
      sids_to_restrict.clear();
      break;
    case USER_INTERACTIVE:
      sids_to_restrict.push_back(Sid(WinBuiltinUsersSid));
      sids_to_restrict.push_back(Sid(WinWorldSid));
      sids_to_restrict.push_back(Sid(WinRestrictedCodeSid));
      if (current_user_sid.has_value()) {
        sids_to_restrict.push_back(current_user_sid.value().sid());
      }
      for (size_t i = 0; i < token_logon_session.size(); ++i) {
        sids_to_restrict.push_back(token_logon_session[i].sid());
      }
      break;
    case USER_LIMITED:
      sids_to_restrict.push_back(Sid(WinBuiltinUsersSid));
      sids_to_restrict.push_back(Sid(WinWorldSid));
      sids_to_restrict.push_back(Sid(WinRestrictedCodeSid));
      // On Windows Vista, the following token (current logon sid) is required
      // to create objects in BNO.  Consider to use low integrity level
      // so that it cannot access object created by other processes.
      for (size_t i = 0; i < token_logon_session.size(); ++i) {
        sids_to_restrict.push_back(token_logon_session[i].sid());
      }
      break;
    case USER_RESTRICTED:
      sids_to_restrict.push_back(Sid(WinRestrictedCodeSid));
      break;
    case USER_LOCKDOWN:
      sids_to_restrict.push_back(Sid(WinNullSid));
      break;
    default:
      DLOG(FATAL) << "unexpeced TokenLevel";
      break;
  }
  return sids_to_restrict;
}

wil::unique_process_handle WinSandbox::GetRestrictedTokenHandle(
    HANDLE effective_token, TokenLevel security_level,
    IntegrityLevel integrity_level) {
  wil::unique_process_handle new_token =
      CreateRestrictedTokenImpl(effective_token, security_level);
  if (!new_token) {
    return nullptr;
  }

  // Modify the default dacl on the token to contain Restricted and the user.
  if (!AddSidToDefaultDacl(new_token.get(), Sid(WinRestrictedCodeSid),
                           GENERIC_ALL)) {
    return nullptr;
  }

  {
    ScopedTokenInfo<TokenUser, TOKEN_USER> token_user(new_token.get());
    if (token_user.get() == nullptr) {
      return nullptr;
    }
    Sid user_sid(static_cast<SID *>(token_user->User.Sid));
    if (!AddSidToDefaultDacl(new_token.get(), user_sid, GENERIC_ALL)) {
      return nullptr;
    }
  }

  if (!SetTokenIntegrityLevel(new_token.get(), integrity_level)) {
    return nullptr;
  }

  wil::unique_process_handle token_handle;
  const BOOL result = ::DuplicateHandle(
      ::GetCurrentProcess(), new_token.get(), ::GetCurrentProcess(),
      token_handle.put(), TOKEN_ALL_ACCESS, FALSE, 0);
  if (result == FALSE) {
    return nullptr;
  }
  return token_handle;
}

wil::unique_process_handle WinSandbox::GetRestrictedTokenHandleForImpersonation(
    HANDLE effective_token, TokenLevel security_level,
    IntegrityLevel integrity_level) {
  wil::unique_process_handle new_token = GetRestrictedTokenHandle(
      effective_token, security_level, integrity_level);
  if (!new_token) {
    return nullptr;
  }

  wil::unique_process_handle impersonation_token;
  if (!::DuplicateToken(new_token.get(), SecurityImpersonation,
                        impersonation_token.put())) {
    return nullptr;
  }

  wil::unique_process_handle restricted_token;
  if (!::DuplicateHandle(::GetCurrentProcess(), impersonation_token.get(),
                         ::GetCurrentProcess(), restricted_token.put(),
                         TOKEN_ALL_ACCESS, FALSE, 0)) {
    return nullptr;
  }
  return restricted_token;
}

bool WinSandbox::EnsureAllApplicationPackagesPermisssion(
    zwstring_view file_name, AppContainerVisibilityType type) {
  // Get "All Application Packages" group SID.
  const ATL::CSid all_application_packages(
      Sid(WinBuiltinAnyPackageSid).GetPSID());

  // Get current DACL (Discretionary Access Control List) of |file_name|.
  ATL::CDacl dacl;
  if (!ATL::AtlGetDacl(file_name.c_str(), SE_FILE_OBJECT, &dacl)) {
    return false;
  }

  const ACCESS_MASK desired_mask = GetAccessMask(type);

  // Check if the desired ACE is already specified or not.
  for (UINT i = 0; i < dacl.GetAceCount(); ++i) {
    ATL::CSid ace_sid;
    ACCESS_MASK access_mask = 0;
    BYTE ace_type = 0;
    dacl.GetAclEntry(i, &ace_sid, &access_mask, &ace_type);
    if (ace_sid == all_application_packages &&
        ace_type == ACCESS_ALLOWED_ACE_TYPE &&
        (access_mask & desired_mask) == desired_mask) {
      // This is the desired ACE.  There is nothing to do.
      return true;
    }
  }

  // We are here because there is no desired ACE.  Hence we do add it.
  if (!dacl.AddAllowedAce(all_application_packages, desired_mask,
                          ACCESS_ALLOWED_ACE_TYPE)) {
    return false;
  }
  return ATL::AtlSetDacl(file_name.c_str(), SE_FILE_OBJECT, dacl);
}

}  // namespace mozc
