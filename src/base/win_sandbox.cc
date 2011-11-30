// Copyright 2010-2011, Google Inc.
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

#include "base/win_sandbox.h"

// skipp all unless OS_WINDOWS
#ifdef OS_WINDOWS
#include <Windows.h>
#include <AclAPI.h>
#include <sddl.h>
#include <strsafe.h>

#include <string>

#include "base/base.h"
#include "base/util.h"
#include "base/scoped_handle.h"
#include "base/scoped_ptr.h"

namespace mozc {
namespace {

bool OpenEffectiveToken(const DWORD dwDesiredAccess, HANDLE *phToken) {
  HANDLE hToken = NULL;

  if (!::OpenThreadToken(::GetCurrentThread(), dwDesiredAccess,
                         TRUE, &hToken)) {
    if (ERROR_NO_TOKEN != ::GetLastError()) {
      ::CloseHandle(hToken);
      return false;
    }
    if (!::OpenProcessToken(::GetCurrentProcess(), dwDesiredAccess, &hToken)) {
      ::CloseHandle(hToken);
      return false;
    }
  }

  *phToken = hToken;
  return true;
}

bool AllocGetTokenInformation(const HANDLE hToken,
                              const TOKEN_INFORMATION_CLASS TokenInfoClass,
                              PVOID* ppInfo,
                              DWORD* pdwSizeBc) {
  DWORD dwBufferSizeBc = 0;
  DWORD dwReturnSizeBc = 0;

  const BOOL bReturn = ::GetTokenInformation(hToken,
                                             TokenInfoClass,
                                             NULL,
                                             0,
                                             &dwBufferSizeBc);
  if (bReturn || (ERROR_INSUFFICIENT_BUFFER != ::GetLastError())) {
    return false;
  }

  PVOID pBuffer = ::LocalAlloc(LPTR, dwBufferSizeBc);
  if (pBuffer == NULL) {
    return false;
  }

  if (!::GetTokenInformation(hToken,
                             TokenInfoClass,
                             pBuffer,
                             dwBufferSizeBc,
                             &dwReturnSizeBc)) {
    ::LocalFree(pBuffer);
    return false;
  }

  if (ppInfo != NULL) {
    *ppInfo = pBuffer;
  }

  if (pdwSizeBc != NULL) {
    *pdwSizeBc = dwReturnSizeBc;
  }

  return true;
}

bool GetTokenUserSidStringW(const HANDLE hToken,
                            PWSTR *pwszSidString) {
  PTOKEN_USER pTokenUser = NULL;
  PWSTR wszSidString = NULL;

  if (AllocGetTokenInformation(hToken, TokenUser,
                               reinterpret_cast<PVOID *>(&pTokenUser),
                               NULL) &&
      ::ConvertSidToStringSidW(pTokenUser->User.Sid, &wszSidString)) {
    ::LocalFree(pTokenUser);
    *pwszSidString = wszSidString;
    return true;
  }

  if (wszSidString != NULL) {
    ::LocalFree(wszSidString);
  }

  if (pTokenUser != NULL) {
    ::LocalFree(pTokenUser);
  }

  return false;
}

bool GetTokenPrimaryGroupSidStringW(const HANDLE hToken,
                                    PWSTR *pwszSidString) {
  PTOKEN_PRIMARY_GROUP pTokenPrimaryGroup = NULL;
  PWSTR wszSidString = NULL;

  if (AllocGetTokenInformation(hToken, TokenPrimaryGroup,
                               reinterpret_cast<PVOID *>(&pTokenPrimaryGroup),
                               NULL) &&
      ::ConvertSidToStringSidW(pTokenPrimaryGroup->PrimaryGroup,
                               &wszSidString)) {
    ::LocalFree(pTokenPrimaryGroup);
    *pwszSidString = wszSidString;
    return true;
  }

  if (wszSidString != NULL) {
    ::LocalFree(wszSidString);
  }

  if (pTokenPrimaryGroup != NULL) {
    ::LocalFree(pTokenPrimaryGroup);
  }

  return false;
}

class ScopedLocalFreeInvoker {
 public:
  explicit ScopedLocalFreeInvoker(void *address) : address_(address) {}
  ~ScopedLocalFreeInvoker() {
    if (address_ != NULL) {
      ::LocalFree(address_);
      address_ = NULL;
    }
  }

 private:
  void *address_;

  DISALLOW_COPY_AND_ASSIGN(ScopedLocalFreeInvoker);
};

}  // namespace

Sid::Sid(const SID *sid) {
  ::CopySid(sizeof(sid_), sid_, const_cast<SID*>(sid));
};

Sid::Sid(WELL_KNOWN_SID_TYPE type) {
  DWORD size_sid = sizeof(sid_);
  ::CreateWellKnownSid(type, NULL, sid_, &size_sid);
}

const SID *Sid::GetPSID() const {
  return reinterpret_cast<SID*>(const_cast<BYTE*>(sid_));
}

SID *Sid::GetPSID() {
  return reinterpret_cast<SID*>(const_cast<BYTE*>(sid_));
}

wstring Sid::GetName() const {
  wchar_t *ptr = NULL;
  Sid temp_sid(GetPSID());
  ConvertSidToStringSidW(temp_sid.GetPSID(), &ptr);
  wstring name = ptr;
  ::LocalFree(ptr);
  return name;
}

wstring Sid::GetAccountName() const {
  wchar_t *ptr = NULL;
  DWORD name_size = 0;
  DWORD domain_name_size = 0;
  SID_NAME_USE name_use;
  Sid temp_sid(GetPSID());
  ::LookupAccountSid(NULL, temp_sid.GetPSID(), NULL, &name_size,
                     NULL, &domain_name_size, &name_use);
  if (domain_name_size == 0) {
    if (name_size == 0) {
      // Use string SID instead.
      return GetName();
    }
    scoped_array<wchar_t> name_buffer(new wchar_t[name_size]);
    ::LookupAccountSid(NULL, temp_sid.GetPSID(), name_buffer.get(),
                       &name_size, NULL, &domain_name_size, &name_use);
    return wstring(L"/") + name_buffer.get();
  }
  scoped_array<wchar_t> name_buffer(new wchar_t[name_size]);
  scoped_array<wchar_t> domain_name_buffer(new wchar_t[domain_name_size]);
  ::LookupAccountSid(NULL, temp_sid.GetPSID(), name_buffer.get(), &name_size,
                     domain_name_buffer.get(), &domain_name_size, &name_use);
  const wstring domain_name = wstring(domain_name_buffer.get());
  const wstring user_name = wstring(name_buffer.get());
  return domain_name + L"/" + user_name;
}

// make SecurityAttributes for the named pipe.
bool WinSandbox::MakeSecurityAttributes(
    SECURITY_ATTRIBUTES *security_attributes) {
  PWSTR wszTokenUserSid = NULL;
  PWSTR wszTokenPrimaryGroupSid = NULL;
  {
    // Open effective token
    ScopedHandle token;
    {
      HANDLE hToken = NULL;
      if (!OpenEffectiveToken(TOKEN_QUERY, &hToken)) {
        LOG(ERROR) << "OpenEffectiveToken failed "
                   << ::GetLastError();
        return false;
      }
      token.reset(hToken);
    }

    // Get token user SID
    if (!GetTokenUserSidStringW(token.get(), &wszTokenUserSid)) {
      LOG(ERROR) << "GetTokenUserSidStringW failed "
                 << ::GetLastError();
      return false;
    }

    // Get token primary group SID
    if (!GetTokenPrimaryGroupSidStringW(token.get(),
                                        &wszTokenPrimaryGroupSid)) {
      LOG(ERROR) << "GetTokenPrimaryGroupSidStringW failed "
                 << ::GetLastError();
      return false;
    }
  }  // close effective token.

  // Create SDDL
  WCHAR wszSddl[2048];
  const HRESULT hr = ::StringCbPrintfW(
      wszSddl,
      sizeof(wszSddl),
      L"O:%s"  // SDDL_OWNER
      L"G:%s"  // SDDL_GROUP
      L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;%s)",
      // SDDL_DACL:(SDDL_ACCESS_ALLOWED;;SDDL_GENERIC_ALL;SDDL_LOCAL_SYSTEM)
      // (...SDDL_BUILTIN_ADMINISTRATORS)(...)
      wszTokenUserSid,
      wszTokenPrimaryGroupSid,
      wszTokenUserSid);

  ::LocalFree(wszTokenUserSid);
  ::LocalFree(wszTokenPrimaryGroupSid);

  if (S_OK != hr) {
    return false;
  }

  // Create self-relative SD
  PSECURITY_DESCRIPTOR pSelfRelativeSd = NULL;
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(wszSddl,
                                                              SDDL_REVISION_1,
                                                              &pSelfRelativeSd,
                                                              NULL)) {
    if (pSelfRelativeSd != NULL) {
      ::LocalFree(pSelfRelativeSd);
    }
    LOG(ERROR)
        << "ConvertStringSecurityDescriptorToSecurityDescriptorW failed: "
        << ::GetLastError();
    return false;
  }

  // Set up security attributes
  security_attributes->nLength= sizeof(SECURITY_ATTRIBUTES);
  security_attributes->lpSecurityDescriptor= pSelfRelativeSd;
  security_attributes->bInheritHandle= FALSE;

  return true;
}

// SetMandatoryLabelW() function:
bool WinSandbox::SetMandatoryLabelW(
    const HANDLE handle,
    const SE_OBJECT_TYPE object_type,
    const wchar_t *desired_access_type,
    const wchar_t *integrity_level) {
  // SDDL_SACL(SDDL_MANDATORY_LABEL;;desired_access;;;integrity_level)
  const wchar_t kSACLFormat[] = L"S:(ML;;%s;;;%s)";
  // SDDL
  WCHAR wszSddl[1024];  // assuming this is enough
  if (S_OK != ::StringCbPrintfW(wszSddl,
                                sizeof(wszSddl),
                                kSACLFormat,
                                desired_access_type,
                                integrity_level)) {
    LOG(ERROR) << "StringCbPrintfW() failed";
    return false;
  }

  // SD
  PSECURITY_DESCRIPTOR pSD = NULL;
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(
          wszSddl,
          SDDL_REVISION_1,
          &pSD,
          NULL)) {
    LOG(ERROR)
        << "ConvertStringSecurityDescriptorToSecurityDescriptorW() faild: "
        << ::GetLastError();
    return false;
  }

  // SACL
  PACL pSacl = NULL;
  BOOL fSaclPresent, fSaclDefaulted;
  if (!::GetSecurityDescriptorSacl(pSD,
                                   &fSaclPresent,
                                   &pSacl,
                                   &fSaclDefaulted)) {
    LOG(ERROR) << "GetSecurityDescriptorSacl() faield: "
               << ::GetLastError();
    return false;
  }

  // Set
  if (ERROR_SUCCESS != ::SetSecurityInfo(handle,
                                         object_type,
                                         LABEL_SECURITY_INFORMATION,
                                         NULL,
                                         NULL,
                                         NULL,
                                         pSacl)) {
    LOG(ERROR) << "SetSecurityInfo() failed: " << ::GetLastError();
    return false;
  }

  if (pSD != NULL) {
    ::LocalFree(pSD);
  }

  return true;
}

bool WinSandbox::AddKnownSidToKernelObject(HANDLE object, const SID *known_sid,
                                           DWORD inhericance_flag,
                                           ACCESS_MASK access_mask) {
  // We must pass |&descriptor| because 6th argument (|&old_dacl|) is
  // non-null.  Actually, returned |old_dacl| points the memory block
  // of |descriptor|, which must be freed by ::LocalFree API.
  // http://msdn.microsoft.com/en-us/library/aa446654.aspx
  PSECURITY_DESCRIPTOR descriptor = NULL;
  PACL old_dacl = NULL;
  DWORD error = ::GetSecurityInfo(
      object, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL,
      &old_dacl, NULL, &descriptor);
  // You need not to free |old_dacl| because |old_dacl| points inside of
  // |descriptor|.
  ScopedLocalFreeInvoker descripter_deleter(descriptor);

  if (error != ERROR_SUCCESS) {
    DLOG(ERROR) << "GetSecurityInfo failed" << error;
    return false;
  }

  EXPLICIT_ACCESS new_access = {};
  new_access.grfAccessMode = GRANT_ACCESS;
  new_access.grfAccessPermissions = access_mask;
  new_access.grfInheritance = inhericance_flag;
  new_access.Trustee.pMultipleTrustee = NULL;
  new_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
  new_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  // When |TrusteeForm| is TRUSTEE_IS_SID, |ptstrName| is a pointer to the SID
  // of the trustee.
  // http://msdn.microsoft.com/en-us/library/aa379636.aspx
  new_access.Trustee.ptstrName =
      reinterpret_cast<wchar_t *>(const_cast<SID *>(known_sid));

  PACL new_dacl = NULL;
  error = ::SetEntriesInAcl(1, &new_access, old_dacl, &new_dacl);
  ScopedLocalFreeInvoker new_decl_deleter(new_dacl);
  if (error != ERROR_SUCCESS) {
    DLOG(ERROR) << "SetEntriesInAcl failed" << error;
    return false;
  }

  error = ::SetSecurityInfo(
      object, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL,
      new_dacl, NULL);
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
  LockedDownJob() : job_handle_(NULL) {}

  ~LockedDownJob() {
    if (job_handle_ != NULL) {
      ::CloseHandle(job_handle_);
      job_handle_ = NULL;
    };
  }

  bool IsValid() const {
    return (job_handle_ != NULL);
  }

  DWORD Init(const wchar_t *job_name, bool allow_ui_operation) {
    if (job_handle_ != NULL) {
      return ERROR_ALREADY_INITIALIZED;
    }
    job_handle_ = ::CreateJobObject(NULL, job_name);
    if (job_handle_ == NULL) {
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
                                    &limit_info,
                                    sizeof(limit_info))) {
        return ::GetLastError();
      }
    }

    if (!allow_ui_operation) {
      JOBOBJECT_BASIC_UI_RESTRICTIONS ui_restrictions = {};
      ui_restrictions.UIRestrictionsClass =
          JOB_OBJECT_UILIMIT_WRITECLIPBOARD |
          JOB_OBJECT_UILIMIT_READCLIPBOARD |
          JOB_OBJECT_UILIMIT_HANDLES |
          JOB_OBJECT_UILIMIT_GLOBALATOMS |
          JOB_OBJECT_UILIMIT_DISPLAYSETTINGS |
          JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
          JOB_OBJECT_UILIMIT_DESKTOP |
          JOB_OBJECT_UILIMIT_EXITWINDOWS;
      if (!::SetInformationJobObject(job_handle_,
                                     JobObjectBasicUIRestrictions,
                                     &ui_restrictions,
                                     sizeof(ui_restrictions))) {
        return ::GetLastError();
      }
    }
    return ERROR_SUCCESS;
  }

  DWORD AssignProcessToJob(HANDLE process_handle) {
    if (job_handle_ == NULL) {
      return ERROR_NO_DATA;
    }
    if (!::AssignProcessToJobObject(job_handle_, process_handle)) {
      return ::GetLastError();
    }
    return ERROR_SUCCESS;
  }

 private:
  HANDLE job_handle_;

  DISALLOW_COPY_AND_ASSIGN(LockedDownJob);
};

bool CreateSuspendedRestrictedProcess(scoped_array<wchar_t> *command_line,
                                      const WinSandbox::SecurityInfo &info,
                                      ScopedHandle *process_handle,
                                      ScopedHandle *thread_handle,
                                      DWORD *pid) {
  HANDLE process_token_ret = NULL;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                          &process_token_ret)) {
    return false;
  }
  ScopedHandle process_token(process_token_ret);

  ScopedHandle primary_token;
  if (!WinSandbox::GetRestrictedTokenHandle(process_token.get(),
                                            info.primary_level,
                                            info.integrity_level,
                                            &primary_token)) {
    return false;
  }

  ScopedHandle impersonation_token;
  if (!WinSandbox::GetRestrictedTokenHandleForImpersonation(
          process_token.get(),
          info.impersonation_level,
          info.integrity_level,
          &impersonation_token)) {
    return false;
  }

  PSECURITY_ATTRIBUTES security_attributes_ptr = NULL;
  SECURITY_ATTRIBUTES security_attributes = {};
  if (WinSandbox::MakeSecurityAttributes(&security_attributes)) {
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
            impersonation_token.get(),
            DACL_SECURITY_INFORMATION,
            security_attributes_ptr->lpSecurityDescriptor)) {
      const DWORD last_error = ::GetLastError();
      DLOG(ERROR) << "SetKernelObjectSecurity failed. Error: " << last_error;
      return false;
    }
  }

  DWORD creation_flags = info.creation_flags | CREATE_SUSPENDED;
  // Note: If the current process is already in a job, you cannot use
  // CREATE_BREAKAWAY_FROM_JOB.  See b/1571395
  if (info.use_locked_down_job) {
    creation_flags |= CREATE_BREAKAWAY_FROM_JOB;
  }

  const wchar_t *startup_directory =
      (info.in_system_dir ? Util::GetSystemDir() : NULL);

  STARTUPINFO startup_info = {sizeof(STARTUPINFO)};
  PROCESS_INFORMATION process_info = {};
  // 3rd parameter of CreateProcessAsUser must be a writable buffer.
  if (!::CreateProcessAsUser(primary_token.get(),
                             NULL,   // No application name.
                             command_line->get(),  // must be writable.
                             security_attributes_ptr,
                             NULL,
                             FALSE,  // Do not inherit handles.
                             creation_flags,
                             NULL,   // Use the environment of the caller.
                             startup_directory,
                             &startup_info,
                             &process_info)) {
    const DWORD last_error = ::GetLastError();
    DLOG(ERROR) << "CreateProcessAsUser failed. Error: " << last_error;
    return false;
  }

  if (security_attributes_ptr != NULL) {
    ::LocalFree(security_attributes_ptr->lpSecurityDescriptor);
  }

  // Change the token of the main thread of the new process for the
  // impersonation token with more rights.
  if (!::SetThreadToken(&process_info.hThread, impersonation_token.get())) {
    const DWORD last_error = ::GetLastError();
    DLOG(ERROR) << "SetThreadToken failed. Error: " << last_error;
    ::TerminateProcess(process_info.hProcess, 0);
    ::CloseHandle(process_info.hProcess);
    ::CloseHandle(process_info.hThread);
    return false;
  }
  if (thread_handle != NULL) {
    thread_handle->reset(process_info.hThread);
  } else {
    ::CloseHandle(process_info.hThread);
  }
  if (process_handle != NULL) {
    process_handle->reset(process_info.hProcess);
  } else {
    ::CloseHandle(process_info.hProcess);
  }
  if (pid != NULL) {
    *pid = process_info.dwProcessId;
  }

  return true;
}

bool SpawnSandboxedProcessImpl(scoped_array<wchar_t> *command_line,
                                const WinSandbox::SecurityInfo &info,
                                DWORD *pid) {
  LockedDownJob job;

  if (info.use_locked_down_job) {
    const DWORD error_code = job.Init(NULL, info.allow_ui_operation);
    if (error_code != ERROR_SUCCESS) {
      return false;
    }
  }

  ScopedHandle thread_handle;
  ScopedHandle process_handle;
  if (!CreateSuspendedRestrictedProcess(
          command_line, info, &process_handle, &thread_handle, pid)) {
      return false;
  }

  if (job.IsValid()) {
    const DWORD error_code = job.AssignProcessToJob(process_handle.get());
    if (error_code != ERROR_SUCCESS) {
      ::TerminateProcess(process_handle.get(), 0);
      return false;
    }
  }

  ::ResumeThread(thread_handle.get());

  return true;
}
}  // anonymous namespace

WinSandbox::SecurityInfo::SecurityInfo()
  : primary_level(WinSandbox::USER_LOCKDOWN),
    impersonation_level(WinSandbox::USER_LOCKDOWN),
    integrity_level(WinSandbox::INTEGRITY_LEVEL_SYSTEM),
    creation_flags(0),
    use_locked_down_job(false),
    allow_ui_operation(false),
    in_system_dir(false) {}

bool WinSandbox::SpawnSandboxedProcess(const string &path,
                                       const string &arg,
                                       const SecurityInfo &info,
                                       DWORD *pid) {
  wstring wpath;
  Util::UTF8ToWide(path.c_str(), &wpath);
  wpath = L"\"" + wpath + L"\"";
  if (!arg.empty()) {
    wstring warg;
    Util::UTF8ToWide(arg.c_str(), &warg);
    wpath += L" ";
    wpath += warg;
  }

  scoped_array<wchar_t> wpath2(new wchar_t[wpath.size() + 1]);
  if (0 != wcscpy_s(wpath2.get(), wpath.size() + 1, wpath.c_str())) {
    return false;
  }

  if (!SpawnSandboxedProcessImpl(&wpath2, info, pid)) {
    return false;
  }

  return true;
}

// Utility functions and classes for GetRestrictionInfo
namespace {
template <typename T>
class Optional {
 public:
  Optional()
    : value_(T()),
    has_value_(false) {}
  explicit Optional(const T &src)
    : value_(src),
    has_value_(true) {}
  const T &value() const {
    return value_;
  }
  static Optional<T> None() {
    return Optional<T>();
  }
  T *mutable_value() {
    has_value_ = true;
    return &value_;
  }
  bool has_value() const {
    return has_value_;
  }
  void Clear() {
    has_value_ = false;
  }
 private:
  T value_;
  bool has_value_;
};

// A utility class for GetTokenInformation API.
// This class manages data buffer into which |TokenDataType| type data
// is filled.
template<TOKEN_INFORMATION_CLASS TokenClass, typename TokenDataType>
class ScopedTokenInfo {
 public:
  explicit ScopedTokenInfo(HANDLE token) : initialized_(false) {
    DWORD num_bytes = 0;
    ::GetTokenInformation(token, TokenClass, NULL, 0, &num_bytes);
    if (num_bytes == 0) {
      return;
    }
    buffer_.reset(new BYTE[num_bytes]);
    TokenDataType *all_token_groups =
        reinterpret_cast<TokenDataType *>(buffer_.get());
    if (!::GetTokenInformation(token, TokenClass, all_token_groups,
                               num_bytes, &num_bytes)) {
      const DWORD last_error = ::GetLastError();
      DLOG(ERROR) << "GetTokenInformation failed. Last error: " << last_error;
      buffer_.reset(NULL);
      return;
    }
    initialized_ = true;
  }
  TokenDataType *get() const {
    return reinterpret_cast<TokenDataType *>(buffer_.get());
  }
  TokenDataType *operator->() const  {
    return get();
  }
 private:
  scoped_array<BYTE> buffer_;
  bool initialized_;
};

// Wrapper class for the SID_AND_ATTRIBUTES structure.
class SidAndAttributes {
 public:
  SidAndAttributes()
    : sid_(static_cast<SID *>(NULL)),
      attributes_(0) {}
  SidAndAttributes(Sid sid, DWORD attributes)
    : sid_(sid),
      attributes_(attributes) {}
  const Sid &sid() const {
    return sid_;
  }
  const DWORD &attributes() const {
    return attributes_;
  }
  const bool HasAttribute(DWORD attribute) const {
    return (attributes_ & attribute) == attribute;
  }
 private:
  Sid sid_;
  DWORD attributes_;
};

// Returns all the 'TokenGroups' information of the specified |token_handle|.
vector<SidAndAttributes> GetAllTokenGroups(HANDLE token_handle) {
  vector<SidAndAttributes> result;
  ScopedTokenInfo<TokenGroups, TOKEN_GROUPS> all_token_groups(token_handle);
  if (all_token_groups.get() == NULL) {
    return result;
  }
  for (size_t i = 0; i < all_token_groups->GroupCount; ++i) {
    Sid sid(static_cast<SID *>(all_token_groups->Groups[i].Sid));
    const DWORD attributes = all_token_groups->Groups[i].Attributes;
    result.push_back(SidAndAttributes(sid, attributes));
  }
  return result;
}

vector<SidAndAttributes> FilterByHavingAttribute(
    const vector<SidAndAttributes> &source, DWORD attribute) {
  vector<SidAndAttributes> result;
  for (size_t i = 0; i < source.size(); ++i) {
    if (source[i].HasAttribute(attribute)) {
      result.push_back(source[i]);
    }
  }
  return result;
}

vector<SidAndAttributes> FilterByNotHavingAttribute(
    const vector<SidAndAttributes> &source, DWORD attribute) {
  vector<SidAndAttributes> result;
  for (size_t i = 0; i < source.size(); ++i) {
    if (!source[i].HasAttribute(attribute)) {
      result.push_back(source[i]);
    }
  }
  return result;
}

template <size_t NumExceptions>
vector<Sid> FilterSidExceptFor(
    const vector<SidAndAttributes> &source_sids,
    const WELL_KNOWN_SID_TYPE (&exception_sids)[NumExceptions]) {
  vector<Sid> result;
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
vector<LUID> FilterPrivilegesExceptFor(
    const vector<LUID_AND_ATTRIBUTES> &source_privileges,
    const wchar_t *(&exception_privileges)[NumExceptions]) {
  vector<LUID> result;
  for (size_t i = 0; i < source_privileges.size(); ++i) {
    bool in_the_exception_list = false;
    for (size_t j = 0; j < NumExceptions; ++j) {
      const LUID source = source_privileges[i].Luid;
      LUID except = {};
      ::LookupPrivilegeValue(NULL, exception_privileges[j], &except);
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

Optional<SidAndAttributes> GetUserSid(HANDLE token) {
  ScopedTokenInfo<TokenUser, TOKEN_USER> token_user(token);
  if (token_user.get() == NULL) {
    return Optional<SidAndAttributes>::None();
  }

  Sid sid(static_cast<SID *>(token_user->User.Sid));
  const DWORD attributes = token_user->User.Attributes;
  return Optional<SidAndAttributes>(SidAndAttributes(sid, attributes));
}

vector<LUID_AND_ATTRIBUTES> GetPrivileges(HANDLE token) {
  vector<LUID_AND_ATTRIBUTES> result;
  ScopedTokenInfo<TokenPrivileges, TOKEN_PRIVILEGES> token_privileges(token);
  if (token_privileges.get() == NULL) {
    return result;
  }

  for (size_t i = 0; i < token_privileges->PrivilegeCount; ++i) {
    result.push_back(token_privileges->Privileges[i]);
  }

  return result;
}

bool CreateRestrictedTokenImpl(HANDLE effective_token,
                               WinSandbox::TokenLevel security_level,
                               ScopedHandle *restricted_token) {
  const vector<Sid> sids_to_disable =
      WinSandbox::GetSidsToDisable(effective_token, security_level);
  const vector<LUID> privileges_to_disable =
      WinSandbox::GetPrivilegesToDisable(effective_token, security_level);
  const vector<Sid> sids_to_restrict =
      WinSandbox::GetSidsToRestrict(effective_token, security_level);

  if ((sids_to_disable.size() == 0) &&
      (privileges_to_disable.size() == 0) &&
      (sids_to_restrict.size() == 0)) {
    // Duplicate the token even if it's not modified at this point
    // because any subsequent changes to this token would also affect the
    // current process.
    HANDLE new_token = NULL;
    const BOOL result = ::DuplicateTokenEx(
        effective_token, TOKEN_ALL_ACCESS, NULL,
        SecurityIdentification, TokenPrimary, &new_token);
    if (result == FALSE) {
      return false;
    }
    restricted_token->reset(new_token);
    return true;
  }

  scoped_array<SID_AND_ATTRIBUTES> sids_to_disable_array;
  vector<Sid> sids_to_disable_array_buffer = sids_to_disable;
  {
    const size_t size = sids_to_disable.size();
    if (size > 0) {
      sids_to_disable_array.reset(new SID_AND_ATTRIBUTES[size]);
      for (size_t i = 0; i < size; ++i) {
        sids_to_disable_array[i].Attributes = SE_GROUP_USE_FOR_DENY_ONLY;
        sids_to_disable_array[i].Sid =
            sids_to_disable_array_buffer[i].GetPSID();
      }
    }
  }

  scoped_array<LUID_AND_ATTRIBUTES> privileges_to_disable_array;
  {
    const size_t size = privileges_to_disable.size();
    if (size > 0) {
      privileges_to_disable_array.reset(new LUID_AND_ATTRIBUTES[size]);
      for (unsigned int i = 0; i < size; ++i) {
        privileges_to_disable_array[i].Attributes = 0;
        privileges_to_disable_array[i].Luid = privileges_to_disable[i];
      }
    }
  }

  scoped_array<SID_AND_ATTRIBUTES> sids_to_restrict_array;
  vector<Sid> sids_to_restrict_array_buffer = sids_to_restrict;
  {
    const size_t size = sids_to_restrict.size();
    if (size > 0) {
      sids_to_restrict_array.reset(new SID_AND_ATTRIBUTES[size]);
      for (size_t i = 0; i < size; ++i) {
        sids_to_restrict_array[i].Attributes = 0;
        sids_to_restrict_array[i].Sid =
            sids_to_restrict_array_buffer[i].GetPSID();
      }
    }
  }

  HANDLE new_token = NULL;
  const BOOL result = ::CreateRestrictedToken(effective_token,
      SANDBOX_INERT,  // This flag is used on Windows 7
      static_cast<DWORD>(sids_to_disable.size()),
      sids_to_disable_array.get(),
      static_cast<DWORD>(privileges_to_disable.size()),
      privileges_to_disable_array.get(),
      static_cast<DWORD>(sids_to_restrict.size()),
      sids_to_restrict_array.get(),
      &new_token);
    if (result == FALSE) {
      return false;
    }
    restricted_token->reset(new_token);
    return true;
}

bool AddSidToDefaultDacl(HANDLE token, const Sid& sid, ACCESS_MASK access) {
  if (token == NULL) {
    return false;
  }

  ScopedTokenInfo<TokenDefaultDacl, TOKEN_DEFAULT_DACL> default_dacl(token);
  if (default_dacl.get() == NULL) {
    return false;
  }

  ACL* new_dacl = NULL;
  {
    EXPLICIT_ACCESS new_access = {};
    new_access.grfAccessMode = GRANT_ACCESS;
    new_access.grfAccessPermissions = access;
    new_access.grfInheritance = NO_INHERITANCE;

    new_access.Trustee.pMultipleTrustee = NULL;
    new_access.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    new_access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    Sid temp_sid(sid);
    new_access.Trustee.ptstrName = reinterpret_cast<LPWSTR>(temp_sid.GetPSID());
    const DWORD result = ::SetEntriesInAcl(
        1, &new_access, default_dacl->DefaultDacl, &new_dacl);
    if (result != ERROR_SUCCESS) {
      return false;
    }
  }

  TOKEN_DEFAULT_DACL new_token_dacl = {new_dacl};
  const BOOL result = ::SetTokenInformation(
      token, TokenDefaultDacl, &new_token_dacl, sizeof(new_token_dacl));
  ::LocalFree(new_dacl);
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
      return NULL;
  }

  return NULL;
}

bool SetTokenIntegrityLevel(HANDLE token,
                            WinSandbox::IntegrityLevel integrity_level) {
  if (!Util::IsVistaOrLater()) {
    return true;
  }

  const wchar_t* sid_string = GetPredefinedSidString(integrity_level);
  if (sid_string == NULL) {
    // do not change the integrity level.
    return true;
  }

  PSID integrity_sid = NULL;
  if (!::ConvertStringSidToSid(sid_string, &integrity_sid)) {
    return false;
  }
  TOKEN_MANDATORY_LABEL label = { {integrity_sid, SE_GROUP_INTEGRITY} };
  const DWORD size = sizeof(TOKEN_MANDATORY_LABEL) +
                     ::GetLengthSid(integrity_sid);
  const BOOL result = ::SetTokenInformation(
      token, TokenIntegrityLevel, &label, size);
  ::LocalFree(integrity_sid);

  return (result != FALSE);
}
}  // anonymous namespace

vector<Sid> WinSandbox::GetSidsToDisable(HANDLE effective_token,
                                         TokenLevel security_level) {
  const vector<SidAndAttributes> all_token_groups =
      GetAllTokenGroups(effective_token);
  const Optional<SidAndAttributes> current_user_sid =
      GetUserSid(effective_token);
  const vector<SidAndAttributes> normal_tokens =
      FilterByNotHavingAttribute(
          FilterByNotHavingAttribute(all_token_groups, SE_GROUP_LOGON_ID),
          SE_GROUP_INTEGRITY);

  vector<Sid> sids_to_disable;
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

vector<LUID> WinSandbox::GetPrivilegesToDisable(HANDLE effective_token,
                                                TokenLevel security_level ) {
  const vector<LUID_AND_ATTRIBUTES> all_privileges =
      GetPrivileges(effective_token);

  vector<LUID> privileges_to_disable;
  switch (security_level) {
    case USER_UNPROTECTED:
    case USER_RESTRICTED_SAME_ACCESS:
      privileges_to_disable.clear();
      break;
    case USER_NON_ADMIN:
    case USER_INTERACTIVE:
    case USER_LIMITED:
    case USER_RESTRICTED: {
      const wchar_t* kPrivilegeExceptions[] = {
        SE_CHANGE_NOTIFY_NAME,
      };
      privileges_to_disable = FilterPrivilegesExceptFor(all_privileges,
                                                        kPrivilegeExceptions);
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

vector<Sid> WinSandbox::GetSidsToRestrict(HANDLE effective_token,
                                          TokenLevel security_level) {
  const vector<SidAndAttributes> all_token_groups =
      GetAllTokenGroups(effective_token);
  const Optional<SidAndAttributes> current_user_sid =
      GetUserSid(effective_token);
  const vector<SidAndAttributes> token_logon_session =
      FilterByHavingAttribute(all_token_groups, SE_GROUP_LOGON_ID);

  vector<Sid> sids_to_restrict;
  switch (security_level) {
    case USER_UNPROTECTED:
      sids_to_restrict.clear();
      break;
    case USER_RESTRICTED_SAME_ACCESS: {
      if (current_user_sid.has_value()) {
        sids_to_restrict.push_back(current_user_sid.value().sid());
      }
      const vector<SidAndAttributes> tokens =
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
      if (Util::IsVistaOrLater()) {
        for (size_t i = 0; i < token_logon_session.size(); ++i) {
          sids_to_restrict.push_back(token_logon_session[i].sid());
        }
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

bool WinSandbox::GetRestrictedTokenHandle(
    HANDLE effective_token,
    TokenLevel security_level,
    IntegrityLevel integrity_level,
    ScopedHandle* restricted_token) {
  ScopedHandle new_token;
  if (!CreateRestrictedTokenImpl(effective_token, security_level,
                                 &new_token)) {
    return false;
  }

  // Modify the default dacl on the token to contain Restricted and the user.
  if (!AddSidToDefaultDacl(new_token.get(), Sid(WinRestrictedCodeSid),
                           GENERIC_ALL)) {
    return false;
  }

  {
    ScopedTokenInfo<TokenUser, TOKEN_USER> token_user(new_token.get());
    if (token_user.get() == NULL) {
      return false;
    }
    Sid user_sid(static_cast<SID *>(token_user->User.Sid));
    if (!AddSidToDefaultDacl(new_token.get(), user_sid, GENERIC_ALL)) {
      return false;
    }
  }

  if (!SetTokenIntegrityLevel(new_token.get(), integrity_level)) {
    return false;
  }

  HANDLE token_handle = NULL;
  const BOOL result = ::DuplicateHandle(
      ::GetCurrentProcess(),
      new_token.get(),
      ::GetCurrentProcess(),
      &token_handle,
      TOKEN_ALL_ACCESS,
      FALSE,
      0);
  if (result == FALSE) {
    return false;
  }
  restricted_token->reset(token_handle);

  return true;
}

bool WinSandbox::GetRestrictedTokenHandleForImpersonation(
    HANDLE effective_token,
    TokenLevel security_level,
    IntegrityLevel integrity_level,
    ScopedHandle* restricted_token) {
  ScopedHandle new_token;
  if (!GetRestrictedTokenHandle(effective_token, security_level,
                                integrity_level, &new_token)) {
    return false;
  }

  HANDLE impersonation_token_ret = NULL;
  if (!::DuplicateToken(new_token.get(), SecurityImpersonation,
                        &impersonation_token_ret)) {
    return false;
  }
  ScopedHandle impersonation_token(impersonation_token_ret);

  HANDLE restricted_token_ret = NULL;
  if (!::DuplicateHandle(::GetCurrentProcess(), impersonation_token.get(),
                         ::GetCurrentProcess(), &restricted_token_ret,
                         TOKEN_ALL_ACCESS, FALSE, 0)) {
    return false;
  }
  restricted_token->reset(restricted_token_ret);
  return true;
}
}   // namespace mozc
#endif  // OS_WINDOWS
