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

#include "win32/custom_action/custom_action.h"

// clang-format off
#include <windows.h>
#include <atlbase.h>
#include <msiquery.h>
// clang-format on

#undef StrCat  // NOLINT: TODO: triggers clang-tidy, defined by windows.h.

#include <cstdarg>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/const.h"
#include "base/process.h"
#include "base/strings/zstring_view.h"
#include "base/system_util.h"
#include "base/url.h"
#include "base/version.h"
#include "base/win32/scoped_com.h"
#include "base/win32/wide_char.h"
#include "base/win32/win_sandbox.h"
#include "base/win32/win_util.h"
#include "client/client.h"
#include "client/client_interface.h"
#include "config/config_handler.h"
#include "renderer/renderer_client.h"
#include "win32/base/input_dll.h"
#include "win32/base/omaha_util.h"
#include "win32/base/tsf_profile.h"
#include "win32/base/tsf_registrar.h"
#include "win32/base/uninstall_helper.h"
#include "win32/cache_service/cache_service_manager.h"
#include "win32/custom_action/resource.h"

#if !defined(NDEBUG)
#include <atlstr.h>
#endif  // !NDEBUG

#ifdef _DEBUG
#define DEBUG_BREAK_FOR_DEBUGGER()                                      \
  ::OutputDebugStringA(                                                 \
      (mozc::Version::GetMozcVersion() + ": " + __FUNCTION__).c_str()); \
  if (::IsDebuggerPresent()) {                                          \
    __debugbreak();                                                     \
  }
#else  // _DEBUG
#define DEBUG_BREAK_FOR_DEBUGGER()
#endif  // _DEBUG

namespace {

using mozc::win32::OmahaUtil;

HMODULE g_module = nullptr;

std::wstring GetMozcComponentPath(const absl::string_view filename) {
  return mozc::win32::Utf8ToWide(
      absl::StrCat(mozc::SystemUtil::GetServerDirectory(), "\\", filename));
}

// Retrieves the value for an installer property.
// Returns an empty string if a property corresponding to |name| is not found or
// error occurs.
std::wstring GetProperty(MSIHANDLE msi, const mozc::zwstring_view name) {
  DWORD num_buf = 0;
  // Obtains the size of the property's string, without null termination.
  // Note: |MsiGetProperty()| requires non-null writable buffer.
  std::wstring buf;
  UINT result = MsiGetProperty(msi, name.c_str(), buf.data(), &num_buf);
  if (result != ERROR_MORE_DATA) {
    return L"";
  }

  buf.resize(num_buf);
  // add 1 for null termination
  num_buf += 1;
  result = MsiGetProperty(msi, name.c_str(), buf.data(), &num_buf);
  if (result != ERROR_SUCCESS) {
    return L"";
  }

  return buf;
}

bool SetProperty(MSIHANDLE msi, const mozc::zwstring_view name,
                 const mozc::zwstring_view value) {
  if (MsiSetProperty(msi, name.c_str(), value.c_str()) != ERROR_SUCCESS) {
    return false;
  }
  return true;
}

std::wstring FormatMessageByResourceId(int resourceID, ...) {
  wchar_t format_message[4096];
  {
    const int length = ::LoadString(g_module, resourceID, format_message,
                                    std::size(format_message));
    if (length <= 0 || std::size(format_message) <= length) {
      return L"";
    }
  }
  va_list va_args = nullptr;
  va_start(va_args, resourceID);

  wchar_t buffer[4096];  // should be less than 64KB.
  // TODO(yukawa): Use message table instead of string table.
  {
    const DWORD num_chars =
        ::FormatMessage(FORMAT_MESSAGE_FROM_STRING, format_message, 0, 0,
                        &buffer[0], std::size(buffer), &va_args);
    va_end(va_args);

    if (num_chars == 0 || num_chars >= std::size(buffer)) {
      return L"";
    }
  }

  return buffer;
}

std::wstring GetVersionHeader() {
  return FormatMessageByResourceId(IDS_FORMAT_VERSION_INFO,
                                   mozc::Version::GetMozcVersionW().c_str());
}

bool WriteOmahaErrorById(int resource_id) {
  wchar_t buffer[4096];
  const int length =
      ::LoadString(g_module, resource_id, buffer, std::size(buffer));
  if (length <= 0 || std::size(buffer) <= length) {
    return false;
  }

  return OmahaUtil::WriteOmahaError(buffer, GetVersionHeader());
}

template <size_t num_elements>
bool WriteOmahaError(const wchar_t (&function)[num_elements], int line) {
#if !defined(NDEBUG)
  ATL::CStringW log;
  log.Format(L"%s: %s; %s(%d)", L"WriteOmahaError: ",
             mozc::Version::GetMozcVersionW().c_str(), function, line);
  ::OutputDebugStringW(log);
#endif  // !defined(NDEBUG)
  const std::wstring &message =
      FormatMessageByResourceId(IDS_FORMAT_FUNCTION_AND_LINE, function, line);
  return OmahaUtil::WriteOmahaError(message, GetVersionHeader());
}

// Compose an error message based on the function name and line number.
// This message will be displayed by Omaha meta installer on the error
// dialog.
#define LOG_ERROR_FOR_OMAHA() WriteOmahaError(_T(__FUNCTION__), __LINE__)

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD ul_reason_for_call,
                      LPVOID reserved) {
  switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
      g_module = module;
      break;
    case DLL_PROCESS_DETACH:
      g_module = nullptr;
      break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
      break;
  }

  return TRUE;
}

// [Return='ignore']
UINT __stdcall EnsureAllApplicationPackagesPermisssions(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  if (!mozc::WinSandbox::EnsureAllApplicationPackagesPermisssion(
          GetMozcComponentPath(mozc::kMozcServerName),
          mozc::WinSandbox::AppContainerVisibilityType::kProgramFiles)) {
    return ERROR_INSTALL_FAILURE;
  }
  if (!mozc::WinSandbox::EnsureAllApplicationPackagesPermisssion(
          GetMozcComponentPath(mozc::kMozcRenderer),
          mozc::WinSandbox::AppContainerVisibilityType::kProgramFiles)) {
    return ERROR_INSTALL_FAILURE;
  }
  if (!mozc::WinSandbox::EnsureAllApplicationPackagesPermisssion(
          GetMozcComponentPath(mozc::kMozcTIP32),
          mozc::WinSandbox::AppContainerVisibilityType::kProgramFiles)) {
    return ERROR_INSTALL_FAILURE;
  }
  if (!mozc::WinSandbox::EnsureAllApplicationPackagesPermisssion(
          GetMozcComponentPath(mozc::kMozcTIP64),
          mozc::WinSandbox::AppContainerVisibilityType::kProgramFiles)) {
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}

// [Return='ignore']
UINT __stdcall OpenUninstallSurveyPage(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  mozc::Process::OpenBrowser(
      mozc::url::GetUninstallationSurveyUrl(mozc::Version::GetMozcVersion()));
  return ERROR_SUCCESS;
}

UINT __stdcall ShutdownServer(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  std::unique_ptr<mozc::client::ClientInterface> server_client(
      mozc::client::ClientFactory::NewClient());
  if (server_client->PingServer()) {
    if (!server_client->Shutdown()) {
      // This is not fatal as Windows Installer can replace executables even
      // when they still are running. Just log error then go ahead.
      LOG_ERROR_FOR_OMAHA();
    }
  }
  std::unique_ptr<mozc::renderer::RendererClient> renderer_client =
      mozc::renderer::RendererClient::Create();
  if (!renderer_client->Shutdown(true)) {
    // This is not fatal as Windows Installer can replace executables even when
    // they are still running. Just log error then go ahead.
    LOG_ERROR_FOR_OMAHA();
  }
  return ERROR_SUCCESS;
}

// [Return='ignore']
UINT __stdcall RestoreUserIMEEnvironment(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  const bool result =
      mozc::win32::UninstallHelper::RestoreUserIMEEnvironmentMain();
  return result ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
}

// [Return='ignore']
// Hides the cancel button on a progress dialog shown by the installer shows.
// Please see the following page for details.
// http://msdn.microsoft.com/en-us/library/aa368791(VS.85).aspx
UINT __stdcall HideCancelButton(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  PMSIHANDLE record = MsiCreateRecord(2);
  if (!record) {
    return ERROR_INSTALL_FAILURE;
  }
  if ((ERROR_SUCCESS != MsiRecordSetInteger(record, 1, 2) ||
       ERROR_SUCCESS != MsiRecordSetInteger(record, 2, 0))) {
    return ERROR_INSTALL_FAILURE;
  }
  MsiProcessMessage(msi_handle, INSTALLMESSAGE_COMMONDATA, record);
  return ERROR_SUCCESS;
}

UINT __stdcall InitialInstallation(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();

  // Write a general error message in case any unexpected error occurs.
  WriteOmahaErrorById(IDS_UNEXPECTED_ERROR);

  return ERROR_SUCCESS;
}

UINT __stdcall InitialInstallationCommit(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();

  // Set error code 0, which means success.
  OmahaUtil::ClearOmahaError();
  return ERROR_SUCCESS;
}

UINT __stdcall EnableTipProfile(MSIHANDLE msi_handle) {
  bool is_service = false;
  if (::mozc::WinUtil::IsServiceAccount(&is_service) && is_service) {
    // Do nothing if this is a service account.
    return ERROR_SUCCESS;
  }

  wchar_t clsid[64] = {};
  if (!::StringFromGUID2(::mozc::win32::TsfProfile::GetTextServiceGuid(), clsid,
                         std::size(clsid))) {
    // Do not care about errors.
    return ERROR_SUCCESS;
  }
  wchar_t profile_id[64] = {};
  if (!::StringFromGUID2(::mozc::win32::TsfProfile::GetProfileGuid(),
                         profile_id, std::size(profile_id))) {
    // Do not care about errors.
    return ERROR_SUCCESS;
  }

  // 0x0411 == MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN)
  const auto desc = ::mozc::win32::StrCatW(L"0x0411:", clsid, profile_id);

  // Do not care about errors.
  ::InstallLayoutOrTip(desc.c_str(), 0);
  return ERROR_SUCCESS;
}

UINT __stdcall FixupConfigFilePermission(MSIHANDLE msi_handle) {
  bool is_service = false;
  if (::mozc::WinUtil::IsServiceAccount(&is_service) && is_service) {
    // Do nothing if this is a service account.
    return ERROR_SUCCESS;
  }

  // Check the file permission of "config1.db" if exists to ensure that
  // "ALL APPLICATION PACKAGES" have read access to it.
  // See https://github.com/google/mozc/issues/1076 for details.
  ::mozc::config::ConfigHandler::FixupFilePermission();

  // Return always ERROR_SUCCESS regardless of the result, as not being able
  // to fixup the permission is not problematic enough to block installation
  // and upgrading.
  return ERROR_SUCCESS;
}

UINT __stdcall SaveCustomActionData(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  // store the CHANNEL value specified in the command line argument for
  // WriteApValue.
  const std::wstring channel = GetProperty(msi_handle, L"CHANNEL");
  if (!channel.empty()) {
    if (!SetProperty(msi_handle, L"WriteApValue", channel)) {
      LOG_ERROR_FOR_OMAHA();
      return ERROR_INSTALL_FAILURE;
    }
  }

  // store the original ap value for WriteApValueRollback.
  const std::wstring ap_value = OmahaUtil::ReadChannel();
  if (!SetProperty(msi_handle, L"WriteApValueRollback", ap_value.c_str())) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
  }

  // store the current settings of the cache service.
  std::wstring backup;
  if (!mozc::CacheServiceManager::BackupStateAsString(&backup)) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
  }
  if (!SetProperty(msi_handle, L"RestoreServiceState", backup.c_str())) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
  }
  if (!SetProperty(msi_handle, L"RestoreServiceStateRollback",
                   backup.c_str())) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}

// [Return='ignore']
// This function is used for the following CustomActions:
// "RestoreServiceState" and "RestoreServiceStateRollback"
UINT __stdcall RestoreServiceState(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  const std::wstring &backup = GetProperty(msi_handle, L"CustomActionData");
  if (!mozc::CacheServiceManager::RestoreStateFromString(backup)) {
    return ERROR_INSTALL_FAILURE;
  }

  return ERROR_SUCCESS;
}

// [Return='ignore']
UINT __stdcall StopCacheService(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  if (!mozc::CacheServiceManager::EnsureServiceStopped()) {
    return ERROR_INSTALL_FAILURE;
  }

  return ERROR_SUCCESS;
}

UINT __stdcall WriteApValue(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  const std::wstring channel = GetProperty(msi_handle, L"CustomActionData");
  if (channel.empty()) {
    // OK. Does not change ap value when CustomActionData is not found.
    return ERROR_SUCCESS;
  }

  const bool result = OmahaUtil::WriteChannel(channel);
  if (!result) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}

UINT __stdcall WriteApValueRollback(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  const std::wstring ap_value = GetProperty(msi_handle, L"CustomActionData");
  if (ap_value.empty()) {
    // The ap value did not originally exist so attempt to delete the value.
    if (!OmahaUtil::ClearChannel()) {
      LOG_ERROR_FOR_OMAHA();
      return ERROR_INSTALL_FAILURE;
    }
    return ERROR_SUCCESS;
  }

  // Restore the original ap value.
  if (!OmahaUtil::WriteChannel(ap_value)) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
  }
  return ERROR_SUCCESS;
}

UINT __stdcall RegisterTIP(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  mozc::ScopedCOMInitializer com_initializer;
  HRESULT result = S_OK;

  // The path here is to retrieve Win32 resources such as icon and product name,
  // which does not need to match the native CPU architecture. Here we use
  // 32-bit TIP DLL as it is always installed even on an ARM64 target.
  const std::wstring resource_dll_path = GetMozcComponentPath(mozc::kMozcTIP32);
  result = mozc::win32::TsfRegistrar::RegisterProfiles(resource_dll_path);
  if (FAILED(result)) {
    LOG_ERROR_FOR_OMAHA();
    UnregisterTIP(msi_handle);
    return ERROR_INSTALL_FAILURE;
  }

  result = mozc::win32::TsfRegistrar::RegisterCategories();
  if (FAILED(result)) {
    LOG_ERROR_FOR_OMAHA();
    UnregisterTIP(msi_handle);
    return ERROR_INSTALL_FAILURE;
  }

  return ERROR_SUCCESS;
}

UINT __stdcall RegisterTIPRollback(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  return UnregisterTIP(msi_handle);
}

// [Return='ignore']
UINT __stdcall UnregisterTIP(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  mozc::ScopedCOMInitializer com_initializer;

  mozc::win32::TsfRegistrar::UnregisterCategories();
  mozc::win32::TsfRegistrar::UnregisterProfiles();

  return ERROR_SUCCESS;
}

// [Return='ignore']
UINT __stdcall UnregisterTIPRollback(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  return RegisterTIP(msi_handle);
}
