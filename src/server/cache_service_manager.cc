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

#include "server/cache_service_manager.h"

#include <windows.h>
#include <shlwapi.h>  // for SHLoadIndirectString
#include <strsafe.h>
#include <wincrypt.h>
#include <string>

#include "base/base.h"
#include "base/scoped_handle.h"
#include "base/util.h"
#include "server/mozc_cache_service_resource.h"
#include "server/win32_service_state.pb.h"

namespace mozc {
namespace {
const char    kProgramName[] = "GoogleIMEJaCacheService.exe";
const wchar_t kServiceName[] = L"GoogleIMEJaCacheService";

const uint64 kMinimumRequiredMemorySizeForInstall = 384 * 1024 * 1024;

class ScopedSCHandle {
 public:
  ScopedSCHandle()
      : handle_(NULL) {}
  explicit ScopedSCHandle(SC_HANDLE handle)
      : handle_(handle) {}

  ~ScopedSCHandle() {
    if (NULL != handle_) {
      ::CloseServiceHandle(handle_);
    }
  }

  SC_HANDLE get() const { return handle_; }

  void swap(ScopedSCHandle & other) {
    SC_HANDLE tmp = handle_;
    handle_ = other.handle_;
    other.handle_ = tmp;
  }

 private:
  SC_HANDLE handle_;
};

// Returns a redirector for the specified string resource in Vista or later.
// Returns a redirected string for the specified string resource in XP.
// Return an empty string if any error occurs.
// See http://msdn.microsoft.com/en-us/library/dd374120.aspx for details.
wstring GetRegistryStringRedirectorOrRedirectedString(int resource_id) {
  const wchar_t kRegistryStringRedirectionPattern[] = L"@%s,-%d";
  const wstring &service_path = CacheServiceManager::GetUnquotedServicePath();
  wchar_t buffer[MAX_PATH] = { L'\0' };

  HRESULT hr = ::StringCchPrintf(
      buffer, ARRAYSIZE(buffer), kRegistryStringRedirectionPattern,
      CacheServiceManager::GetUnquotedServicePath().c_str(), resource_id);
  if (hr != S_OK) {
    return L"";
  }
  const wstring redirector(buffer);
  if (Util::IsVistaOrLater()) {
    // If this program is running on Windows Vista or later,
    // just returns the redirector.
    return redirector;
  }

  // If this program is running on Windows XP,
  // explicitly calls SHLoadIndirectString and returns the retrieved string
  // resource.
  wchar_t redirected_string[4096] = { L'\0' };
  hr = ::SHLoadIndirectString(redirector.c_str(), redirected_string,
      sizeof(redirected_string), NULL);
  if (hr != S_OK) {
    return L"";
  }
  return redirected_string;
}

wstring GetDisplayName() {
  return GetRegistryStringRedirectorOrRedirectedString(IDS_DISPLAYNAME);
}

wstring GetDescription() {
  return GetRegistryStringRedirectorOrRedirectedString(IDS_DESCRIPTION);
}

// This function serializes a given message (any type of protobuf message)
// as a wstring encoded by base64.
// Returns true if succeeds.
bool SerializeToBase64WString(const ::google::protobuf::Message &message,
                              wstring *dest) {
  if (dest == NULL) {
    return false;
  }

  const int serialized_len = message.ByteSize();
  scoped_array<BYTE> serialized(new BYTE[serialized_len]);
  if (!message.SerializeToArray(serialized.get(), serialized_len)) {
    LOG(ERROR) << "SerializeAsArray failed";
    return false;
  }

  DWORD base64_string_len = 0;
  BOOL result = ::CryptBinaryToString(serialized.get(), serialized_len,
                                      CRYPT_STRING_BASE64, NULL,
                                      &base64_string_len);
  if (result == FALSE) {
    return false;
  }
  scoped_array<wchar_t> base64_string(new wchar_t[base64_string_len]);
  result = ::CryptBinaryToString(serialized.get(), serialized_len,
                                 CRYPT_STRING_BASE64, base64_string.get(),
                                 &base64_string_len);
  if (result == FALSE) {
    return false;
  }
  dest->assign(base64_string.get(), base64_string_len);
  return true;
}

// This function deserializes a message (any type of protobuf message)
// from the given wstring.  This wstring should be generated by
// SerializeToBase64WString.
// Returns true if succeeds
bool DeserializeFromBase64WString(const wstring &src,
                                  ::google::protobuf::Message *message) {
  if (message == NULL) {
    return false;
  }

  DWORD buffer_len = 0;
  BOOL result = ::CryptStringToBinary(src.c_str(),
                                      src.size(),
                                      CRYPT_STRING_BASE64,
                                      NULL,
                                      &buffer_len,
                                      NULL,
                                      NULL);
  if (result == FALSE) {
    return false;
  }
  scoped_array<BYTE> buffer(new BYTE[buffer_len]);
  result = ::CryptStringToBinary(src.c_str(),
                                 src.size(),
                                 CRYPT_STRING_BASE64,
                                 buffer.get(),
                                 &buffer_len,
                                 NULL,
                                 NULL);
  if (result == FALSE) {
    return false;
  }
  if (!message->ParseFromArray(buffer.get(), buffer_len)) {
    LOG(ERROR) << "ParseFromArray failed";
    return false;
  }
  return true;
}

// A helper function to retrieve a service handle of the cache service
// with specified access rights.
// Returns true if one of the following conditions is satisfied.
//  - A valid service handle is retrieved.  In this case, |handle| owns the
//    retrieved handle.
//  - It turns out w/o any error that the cache service is not installed.
//    In this case, |handle| owns a NULL handle.
// Otherwise, returns false.
bool GetCacheService(DWORD service_controler_rights, DWORD service_rights,
                     ScopedSCHandle *handle) {
  if (NULL == handle) {
    return false;
  }

  ScopedSCHandle sc_handle(::OpenSCManager(NULL, NULL,
                                           service_controler_rights));
  if (NULL == sc_handle.get()) {
    LOG(ERROR) << "OpenSCManager failed: " << ::GetLastError();
    return false;
  }

  ScopedSCHandle service_handle(::OpenService(
      sc_handle.get(), CacheServiceManager::GetServiceName(), service_rights));
  const int error = ::GetLastError();

  if (NULL == service_handle.get() && ERROR_SERVICE_DOES_NOT_EXIST != error) {
    LOG(ERROR) << "OpenService failed: " << error;
    return false;
  }

  // |service_handle| is null if and only if the cache service is not
  // installed.
  service_handle.swap(*handle);
  return true;
}

bool IsServiceRunning(const ScopedSCHandle &service_handle) {
  if (NULL == service_handle.get()) {
    return false;
  }

  SERVICE_STATUS service_status = { 0 };
  if (!::QueryServiceStatus(service_handle.get(), &service_status)) {
    LOG(ERROR) << "QueryServiceStatus failed: " << ::GetLastError();
    return false;
  }

  return service_status.dwCurrentState == SERVICE_RUNNING;
}

bool StartServiceInternal(const ScopedSCHandle &service_handle,
                          const vector<wstring> &arguments) {
  if (arguments.size() <= 0) {
    if (!::StartService(service_handle.get(), 0, NULL)) {
      LOG(ERROR) << "StartService failed: " << ::GetLastError();
      return false;
    }
    return true;
  }

  scoped_array<const wchar_t*> args(new const wchar_t*[arguments.size()]);
  for (size_t i = 0; i < arguments.size(); ++i) {
    args[i] = arguments[i].c_str();
  }
  if (!::StartService(service_handle.get(), arguments.size(), args.get())) {
    LOG(ERROR) << "StartService failed: " << ::GetLastError();
    return false;
  }

  return true;
}

bool StopService(const ScopedSCHandle &service_handle) {
  SERVICE_STATUS status;
  if (!::ControlService(service_handle.get(), SERVICE_CONTROL_STOP, &status)) {
    LOG(ERROR) << "ControlService failed: " << ::GetLastError();
    return false;
  }
  return true;
}

bool SetServiceDescription(const ScopedSCHandle &service_handle,
                           const wstring &description) {
  // +1 for '\0'
  const size_t buffer_length = description.size() + 1;
  scoped_array<wchar_t> buffer(new wchar_t[buffer_length]);
  description._Copy_s(buffer.get(), buffer_length, description.size());
  buffer[buffer_length - 1] = L'\0';

  SERVICE_DESCRIPTION desc = { 0 };
  desc.lpDescription = buffer.get();
  if (!::ChangeServiceConfig2(service_handle.get(),
                              SERVICE_CONFIG_DESCRIPTION, &desc)) {
    LOG(ERROR) << "ChangeServiceConfig2 failed: " << ::GetLastError();
    return false;
  }

  // Windows Vista or later allows the SCM to run a service in a restricted
  // context as described in following documents.
  // http://msdn.microsoft.com/en-us/magazine/cc164252.aspx
  // http://blogs.technet.com/richard_macdonald/archive/2007/06/27/1375523.aspx
  // See also http://b/2470180
  if (Util::IsVistaOrLater()) {
    // Only SE_INC_BASE_PRIORITY_NAME and SE_CHANGE_NOTIFY are needed.
    // According to MSDN Library, we need not explicitly specify the later.
    // See http://msdn.microsoft.com/en-us/library/ms685976.aspx for details.
    SERVICE_REQUIRED_PRIVILEGES_INFO privileges_info = { 0 };
    privileges_info.pmszRequiredPrivileges = SE_INC_BASE_PRIORITY_NAME
                                             TEXT("\0");
    if (!::ChangeServiceConfig2(service_handle.get(),
                                SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO,
                                &privileges_info)) {
      LOG(ERROR) << "ChangeServiceConfig2 failed: " << ::GetLastError();
      return false;
    }

    // Remove write privileges from the cache service.
    // See http://msdn.microsoft.com/en-us/library/ms685987.aspx for details.
    // This may restrict glog functions such as LOG(ERROR).
    // TODO(yukawa): Output logging messages as debug strings, or output them
    // to the Win32 event log.
    SERVICE_SID_INFO sid_info = { 0 };
    sid_info.dwServiceSidType = SERVICE_SID_TYPE_RESTRICTED;
    if (!::ChangeServiceConfig2(service_handle.get(),
                                SERVICE_CONFIG_SERVICE_SID_INFO,
                                &sid_info)) {
      LOG(ERROR) << "ChangeServiceConfig2 failed: " << ::GetLastError();
      return false;
    }
  }
  return true;
}

// This function updates the following settings of the cache service as
// specified.
//  - Display name
//  - Desctiption
//  - Load type (regardless of the amount of the system memory)
// This function also starts or stops the cache service.
// Win32ServiceState::installed() will be ignored.  You can not use this
// function to install nor uninstall the cache service.
bool RestoreStateInternal(const cache_service::Win32ServiceState &state) {
  ScopedSCHandle service_handle;
  const DWORD kSCRights = SC_MANAGER_CONNECT;
  const DWORD kServiceRights =
      GENERIC_READ | GENERIC_WRITE | SERVICE_START | SERVICE_STOP;
  if (!GetCacheService(kSCRights, kServiceRights, &service_handle)
      || (NULL == service_handle.get())) {
    return false;
  }

  if (!::ChangeServiceConfig(service_handle.get(), SERVICE_NO_CHANGE,
                             static_cast<DWORD>(state.load_type()),
                             SERVICE_NO_CHANGE, NULL, NULL, NULL, NULL, NULL,
                             NULL, GetDisplayName().c_str())) {
    LOG(ERROR) << "ChangeServiceConfig failed: " << ::GetLastError();
    return false;
  }

  if (!SetServiceDescription(service_handle, GetDescription())) {
    return false;
  }

  const bool now_running = IsServiceRunning(service_handle);

  // If the service was runnning and is not running now, start it unless the
  // load type is not DISABLED.
  if (state.running() && !now_running &&
      (state.load_type() != cache_service::Win32ServiceState::DISABLED)) {
    vector<wstring> arguments(state.arguments_size());
    arguments.resize(state.arguments_size());
    for (size_t i = 0; i < state.arguments_size(); ++i) {
      if (Util::UTF8ToWide(state.arguments(i).c_str(), &arguments[i]) <= 0) {
        return false;
      }
    }
    return StartServiceInternal(service_handle, arguments);
  // If the service is now runnning and was not running, stop it.
  } else if (!state.running() && now_running) {
    return StopService(service_handle);
  // Nothing to do.
  } else {
    return true;
  }

  assert(0);
  return false;
}

// Returns true if a byte array which contains valid QUERY_SERVICE_CONFIG
// is successfully retrieved.  Some members of QUERY_SERVICE_CONFIG points
// memory addresses inside the retrieved byte array.
bool GetServiceConfig(const ScopedSCHandle &service_handle,
                      scoped_array<char> *result) {
  if (NULL == result) {
    return false;
  }

  if (NULL == service_handle.get()) {
    return false;
  }

  DWORD size = 0;
  if (!::QueryServiceConfig(service_handle.get(), NULL, 0, &size) &&
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    LOG(ERROR) << "QueryServiceConfig failed: " << ::GetLastError();
    return false;
  }

  if (size == 0) {
    LOG(ERROR) << "buffer size is 0";
    return false;
  }

  scoped_array<char> buf(new char[size]);
  LPQUERY_SERVICE_CONFIG service_config =
      reinterpret_cast<LPQUERY_SERVICE_CONFIG>(buf.get());

  if (!::QueryServiceConfig(service_handle.get(),
                            service_config, size, &size)) {
    LOG(ERROR) << "QueryServiceConfig failed: " << ::GetLastError();
    return false;
  }

  buf.swap(*result);
  return true;
}
}  // anonymous namespace

bool CacheServiceManager::IsInstalled() {
  ScopedSCHandle service_handle;
  if (!GetCacheService(SC_MANAGER_CONNECT | GENERIC_READ, SERVICE_QUERY_STATUS,
                       &service_handle)
      || (NULL == service_handle.get())) {
    return false;
  }
  return true;
}

bool CacheServiceManager::IsRunning() {
  ScopedSCHandle service_handle;
  if (!GetCacheService(SC_MANAGER_CONNECT | GENERIC_READ, SERVICE_QUERY_STATUS,
                       &service_handle)
      || (NULL == service_handle.get())) {
    return false;
  }
  return IsServiceRunning(service_handle);
}

bool CacheServiceManager::IsEnabled() {
  ScopedSCHandle service_handle;
  if (!GetCacheService(SC_MANAGER_CONNECT | GENERIC_READ, SERVICE_QUERY_CONFIG,
                       &service_handle)
      || (NULL == service_handle.get())) {
    return false;
  }

  // This byte array will contain both QUERY_SERVICE_CONFIG and some
  // string buffers which are reffered by corresponding members of
  // QUERY_SERVICE_CONFIG.  We have to keep the array until we complete
  // all tasks which use the contents of QUERY_SERVICE_CONFIG.
  scoped_array<char> buffer;
  if (!GetServiceConfig(service_handle, &buffer)) {
    return false;
  }
  const LPQUERY_SERVICE_CONFIG service_config =
      reinterpret_cast<const LPQUERY_SERVICE_CONFIG>(buffer.get());
  return service_config->dwStartType == SERVICE_AUTO_START;
}

const wchar_t *CacheServiceManager::GetServiceName() {
  return kServiceName;
}

wstring CacheServiceManager::GetUnquotedServicePath() {
  const string lock_service_path =
      Util::JoinPath(Util::GetServerDirectory(), kProgramName);
  wstring wlock_service_path;
  if (Util::UTF8ToWide(lock_service_path.c_str(), &wlock_service_path) <= 0) {
    return L"";
  }
  return wlock_service_path;
}

wstring CacheServiceManager::GetQuotedServicePath() {
  return L"\"" + GetUnquotedServicePath() + L"\"";
}

bool CacheServiceManager::EnableAutostart() {
  if (!IsInstalled()) {
    return false;
  }

  const bool enough = CacheServiceManager::HasEnoughMemory();

  // Fortunately, the expected service settings can be descried by
  // Win32ServiceState so we can use RestoreStateInternal to change the
  // settings and start the service (if needed).
  // If the system does not have enough memory, disable the service
  // to reduce the performance impact on the boot-time.
  cache_service::Win32ServiceState state;
  state.set_version(1);
  state.set_installed(true);
  state.set_load_type(enough ? cache_service::Win32ServiceState::AUTO_START
                             : cache_service::Win32ServiceState::DISABLED);
  state.set_running(enough);
  const bool result = RestoreStateInternal(state);
  return enough ? result : false;
}

bool CacheServiceManager::DisableService() {
  if (!IsInstalled()) {
    return false;
  }

  // Fortunately, the expected service settings can be descried by
  // Win32ServiceState so we can use RestoreStateInternal to change the
  // settings and stop the service (if needed).
  cache_service::Win32ServiceState state;
  state.set_version(1);
  state.set_installed(true);
  state.set_load_type(cache_service::Win32ServiceState::DISABLED);
  state.set_running(false);
  return RestoreStateInternal(state);
}

bool CacheServiceManager::RestartService() {
  ScopedSCHandle service_handle;
  if (!GetCacheService(SC_MANAGER_CONNECT,
                       SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS,
                       &service_handle)
      || (NULL == service_handle.get())) {
    return false;
  }

  SERVICE_STATUS status;
  if (!::ControlService(service_handle.get(), SERVICE_CONTROL_STOP, &status)) {
    LOG(ERROR) << "ControlService failed: " << ::GetLastError();
  }

  const int kNumTrial = 10;
  for (int i = 0; i < kNumTrial; ++i) {
    SERVICE_STATUS service_status = { 0 };
    if (!::QueryServiceStatus(service_handle.get(), &service_status)) {
      LOG(ERROR) << "QueryServiceStatus failed: " << ::GetLastError();
      return false;
    }
    if (service_status.dwCurrentState != SERVICE_RUNNING) {
      break;
    }
    ::Sleep(200);  // wait for 0.2sec
  }

  return StartServiceInternal(service_handle, vector<wstring>());
}

bool CacheServiceManager::HasEnoughMemory() {
  return Util::GetTotalPhysicalMemory()
      >= kMinimumRequiredMemorySizeForInstall;
}

bool CacheServiceManager::BackupStateAsString(wstring *result) {
  if (result == NULL) {
    return false;
  }
  result->clear();

  cache_service::Win32ServiceState state;
  state.set_version(1);

  ScopedSCHandle service_handle;
  if (!GetCacheService(SC_MANAGER_CONNECT | GENERIC_READ,
                       SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG,
                       &service_handle)) {
    // If it turns out w/o any error that the cache service is not installed,
    // |GetCacheService| returns true.  We can conclude any other error
    // occurred.
    return false;
  }

  // If the cachse service is actually installed, |service_handle| should have
  // non-null value.
  state.set_installed(NULL != service_handle.get());
  if (!state.installed()) {
    // The cache service is not installed.
    // Use default settings with setting |installed| flag to false.
    state.set_load_type(cache_service::Win32ServiceState::AUTO_START);
    state.set_running(true);
  } else {
    // This byte array will contain both QUERY_SERVICE_CONFIG and some
    // string buffers which are reffered by corresponding members of
    // QUERY_SERVICE_CONFIG.  We have to keep the array until we complete
    // all tasks which use the contents of QUERY_SERVICE_CONFIG.
    scoped_array<char> buffer;
    if (!GetServiceConfig(service_handle, &buffer)) {
      return false;
    }
    const LPQUERY_SERVICE_CONFIG service_config =
        reinterpret_cast<const LPQUERY_SERVICE_CONFIG>(buffer.get());
    // retrieves the server load type.
    state.set_load_type(
        static_cast<cache_service::Win32ServiceState::LoadType>(
            service_config->dwStartType));
    // retrieves the server running status.
    state.set_running(IsServiceRunning(service_handle));
  }

  if (!SerializeToBase64WString(state, result)) {
    return false;
  }

  return true;
}

bool CacheServiceManager::RestoreStateFromString(const wstring &serialized) {
  cache_service::Win32ServiceState state;
  if (!DeserializeFromBase64WString(serialized, &state)) {
    return false;
  }

  return RestoreStateInternal(state);
}

bool CacheServiceManager::EnsureServiceStopped() {
  ScopedSCHandle service_handle;
  const DWORD kSCRights = SC_MANAGER_CONNECT;
  const DWORD kServiceRights = GENERIC_READ | SERVICE_STOP;
  if (!GetCacheService(kSCRights, kServiceRights, &service_handle)) {
    return false;
  }

  if (NULL == service_handle.get()) {
    return true;
  }

  if (!IsServiceRunning(service_handle)) {
    return true;
  }

  if (!StopService(service_handle)) {
    return false;
  }

  return IsServiceRunning(service_handle);
}
}  // mozc
#endif  // OS_WINDOWS
