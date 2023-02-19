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
#include <iepmapi.h>
#include <atlbase.h>
#if !defined(MOZC_NO_LOGGING)
#include <atlstr.h>
#endif  // !MOZC_NO_LOGGING
#include <msiquery.h>
// clang-format on

#include <memory>
#include <string>
#include <vector>

#include "base/const.h"
#include "base/process.h"
#include "base/system_util.h"
#include "base/url.h"
#include "base/util.h"
#include "base/version.h"
#include "base/win32/win_sandbox.h"
#include "base/win32/win_util.h"
#include "client/client_interface.h"
#include "config/stats_config_util.h"
#include "protocol/commands.pb.h"
#include "renderer/renderer_client.h"
#include "server/cache_service_manager.h"
#include "win32/base/imm_registrar.h"
#include "win32/base/imm_util.h"
#include "win32/base/keyboard_layout_id.h"
#include "win32/base/omaha_util.h"
#include "win32/base/tsf_registrar.h"
#include "win32/base/uninstall_helper.h"
#include "win32/custom_action/resource.h"

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

constexpr char kIEFrameDll[] = "ieframe.dll";
const wchar_t kSystemSharedKey[] = L"Software\\Microsoft\\CTF\\SystemShared";

HMODULE g_module = nullptr;

std::wstring GetMozcComponentPath(const std::string &filename) {
  const std::string path =
      mozc::SystemUtil::GetServerDirectory() + "\\" + filename;
  std::wstring wpath;
  mozc::Util::Utf8ToWide(path, &wpath);
  return wpath;
}

// Retrieves the value for an installer property.
// Returns an empty string if a property corresponding to |name| is not found or
// error occurs.
std::wstring GetProperty(MSIHANDLE msi, const std::wstring &name) {
  DWORD num_buf = 0;
  // Obtains the size of the property's string, without null termination.
  // Note: |MsiGetProperty()| requires non-null writable buffer.
  wchar_t dummy_buffer[1] = {L'\0'};
  UINT result = MsiGetProperty(msi, name.c_str(), dummy_buffer, &num_buf);
  if (ERROR_MORE_DATA != result) {
    return L"";
  }

  // add 1 for null termination
  ++num_buf;
  std::unique_ptr<wchar_t[]> buf(new wchar_t[num_buf]);
  result = MsiGetProperty(msi, name.c_str(), buf.get(), &num_buf);
  if (ERROR_SUCCESS != result) {
    return L"";
  }

  return std::wstring(buf.get());
}

bool SetProperty(MSIHANDLE msi, const std::wstring &name,
                 const std::wstring &value) {
  if (MsiSetProperty(msi, name.c_str(), value.c_str()) != ERROR_SUCCESS) {
    return false;
  }
  return true;
}

#ifndef MOZC_NO_LOGGING
bool DisableErrorReportingInternal(const wchar_t *key_name,
                                   const wchar_t *value_name, DWORD value,
                                   REGSAM additional_sam_desired) {
  CRegKey key;
  LONG result =
      key.Create(HKEY_LOCAL_MACHINE, key_name, REG_NONE,
                 REG_OPTION_NON_VOLATILE, KEY_WRITE | additional_sam_desired);
  if (ERROR_SUCCESS != result) {
    return false;
  }
  result = key.SetDWORDValue(value_name, value);
  if (ERROR_SUCCESS != result) {
    return false;
  }
  return true;
}
#endif  // MOZC_NO_LOGGING

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
#if !defined(MOZC_NO_LOGGING)
  CStringW log;
  log.Format(L"%s: %s; %s(%d)", L"WriteOmahaError: ",
             mozc::Version::GetMozcVersionW().c_str(), function, line);
  ::OutputDebugStringW(log);
#endif  // !defined(MOZC_NO_LOGGING)
  const std::wstring &message =
      FormatMessageByResourceId(IDS_FORMAT_FUNCTION_AND_LINE, function, line);
  return OmahaUtil::WriteOmahaError(message, GetVersionHeader());
}

// Compose an error message based on the function name and line number.
// This message will be displayed by Omaha meta installer on the error
// dialog.
#define LOG_ERROR_FOR_OMAHA() WriteOmahaError(_T(__FUNCTION__), __LINE__)

UINT RemovePreloadKeyByKLID(const mozc::win32::KeyboardLayoutID &klid) {
  if (!klid.has_id()) {
    // already removed.
    return ERROR_SUCCESS;
  }
  const mozc::win32::KeyboardLayoutID default_klid(
      mozc::kDefaultKeyboardLayout);
  const HRESULT result =
      mozc::win32::ImmRegistrar::RemoveKeyFromPreload(klid, default_klid);
  return SUCCEEDED(result) ? ERROR_SUCCESS : ERROR_INSTALL_FAILURE;
}

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
          GetMozcComponentPath(mozc::kMozcServerName))) {
    return ERROR_INSTALL_FAILURE;
  }
  if (!mozc::WinSandbox::EnsureAllApplicationPackagesPermisssion(
          GetMozcComponentPath(mozc::kMozcRenderer))) {
    return ERROR_INSTALL_FAILURE;
  }
  if (!mozc::WinSandbox::EnsureAllApplicationPackagesPermisssion(
          GetMozcComponentPath(mozc::kMozcTIP32))) {
    return ERROR_INSTALL_FAILURE;
  }
  if (mozc::SystemUtil::IsWindowsX64()) {
    if (!mozc::WinSandbox::EnsureAllApplicationPackagesPermisssion(
            GetMozcComponentPath(mozc::kMozcTIP64))) {
      return ERROR_INSTALL_FAILURE;
    }
  }
  return ERROR_SUCCESS;
}

UINT __stdcall CallIERefreshElevationPolicy(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  HRESULT result = ::IERefreshElevationPolicy();
  if (FAILED(result)) {
    LOG_ERROR_FOR_OMAHA();
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
  bool server_result = true;
  if (server_client->PingServer()) {
    server_result = server_client->Shutdown();
  }
  mozc::renderer::RendererClient renderer_client;
  const bool renderer_result = renderer_client.Shutdown(true);
  if (!server_result) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
  }
  if (!renderer_result) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
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
UINT __stdcall EnsureIMEIsDisabledForServiceAccount(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  bool is_service = false;
  if (!mozc::WinUtil::IsServiceAccount(&is_service)) {
    return ERROR_INSTALL_FAILURE;
  }
  if (!is_service) {
    // Do nothing if this is not a service account.
    return ERROR_SUCCESS;
  }

  const bool result =
      mozc::win32::UninstallHelper::EnsureIMEIsRemovedForCurrentUser(true);
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

UINT __stdcall InstallIME(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();

  const std::wstring ime_path = mozc::win32::ImmRegistrar::GetFullPathForIME();
  if (ime_path.empty()) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
  }
  const std::wstring ime_filename =
      mozc::win32::ImmRegistrar::GetFileNameForIME();

  const std::wstring layout_name = mozc::win32::ImmRegistrar::GetLayoutName();
  if (layout_name.empty()) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
  }

  // Install IME and obtain the corresponding HKL value.
  HKL hkl = nullptr;
  HRESULT result = mozc::win32::ImmRegistrar::Register(
      ime_filename, layout_name, ime_path,
      mozc::win32::ImmRegistrar::GetLayoutDisplayNameResourceId(), &hkl);
  if (result != S_OK) {
    LOG_ERROR_FOR_OMAHA();
    return ERROR_INSTALL_FAILURE;
  }

  return ERROR_SUCCESS;
}

UINT __stdcall InstallIMERollback(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  return RemovePreloadKeyByKLID(mozc::win32::ImmRegistrar::GetKLIDForIME());
}

UINT __stdcall UninstallIME(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  const std::wstring ime_filename =
      mozc::win32::ImmRegistrar::GetFileNameForIME();
  if (ime_filename.empty()) {
    return ERROR_INSTALL_FAILURE;
  }
  mozc::win32::ImmRegistrar::Unregister(ime_filename);
  return ERROR_SUCCESS;
}

UINT __stdcall UninstallIMERollback(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();

  return ERROR_SUCCESS;
}

UINT __stdcall RegisterTIP(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  mozc::ScopedCOMInitializer com_initializer;

#if defined(_M_X64)
  const std::wstring &path = GetMozcComponentPath(mozc::kMozcTIP64);
#elif defined(_M_IX86)
  const std::wstring &path = GetMozcComponentPath(mozc::kMozcTIP32);
#else  // _M_X64, _M_IX86
#error "Unsupported CPU architecture"
#endif  // _M_X64, _M_IX86, and others
  HRESULT result =
      mozc::win32::TsfRegistrar::RegisterCOMServer(path.c_str(), path.length());
  if (FAILED(result)) {
    LOG_ERROR_FOR_OMAHA();
    UnregisterTIP(msi_handle);
    return ERROR_INSTALL_FAILURE;
  }

  result =
      mozc::win32::TsfRegistrar::RegisterProfiles(path.c_str(), path.length());
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
  mozc::win32::TsfRegistrar::UnregisterCOMServer();

  return ERROR_SUCCESS;
}

// [Return='ignore']
UINT __stdcall UnregisterTIPRollback(MSIHANDLE msi_handle) {
  DEBUG_BREAK_FOR_DEBUGGER();
  return RegisterTIP(msi_handle);
}
