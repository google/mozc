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

#include "win32/base/tsf_registrar.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <msctf.h>
#include <objbase.h>
#include <windows.h>
#include <wrl/client.h>

#include <cstddef>
#include <memory>
#include <string>

#include "base/const.h"
#include "base/logging.h"
#include "base/win32/wide_char.h"
#include "win32/base/display_name_resource.h"
#include "win32/base/input_dll.h"
#include "win32/base/tsf_profile.h"

namespace mozc {
namespace win32 {
namespace {

using Microsoft::WRL::ComPtr;

// Defines the constant strings used in the TsfRegistrar::RegisterCOMServer()
// function and the TsfRegistrar::UnregisterCOMServer() function.
constexpr wchar_t kTipInfoKeyPrefix[] = L"CLSID\\";
constexpr wchar_t kTipInProcServer32[] = L"InProcServer32";
constexpr wchar_t kTipThreadingModel[] = L"ThreadingModel";
constexpr wchar_t kTipTextServiceModel[] = L"Apartment";

// The categories this text service is registered under.
// This needs to be const as the included constants are defined as const.
const GUID kCategories[] = {
    GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,     // It supports inline input.
    GUID_TFCAT_TIPCAP_COMLESS,               // It's a COM-Less module.
    GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,  // It supports input mode.
    GUID_TFCAT_TIPCAP_UIELEMENTENABLED,      // It supports UI less mode.
    GUID_TFCAT_TIP_KEYBOARD,                 // It's a keyboard input method.
    GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,      // It supports Metro mode.
    GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,        // It supports Win8 systray.
};

}  // namespace

// Register this module as a COM server.
// This function creates two registry entries for this module:
//   * "HKCR\CLSID\{xxxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxx}", and;
//   * "HKCR\CLSID\{xxxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxx}\InProcServer32".
// Also, this function Writes:
//   * The description of this module;
//   * The path to this module, and;
//   * The type of this module (threading moddel).
HRESULT TsfRegistrar::RegisterCOMServer(const wchar_t *path, DWORD length) {
  if (length == 0) {
    return E_OUTOFMEMORY;
  }
  // Open the registry key "HKEY_CLASSES_ROOT\CLSID", which stores information
  // of all COM modules installed in this PC.
  // To register a COM module to Windows, we should create a key of its GUID
  // and fill the information below:
  //  * The description of this module (optional);
  //  * The ProgID of this module (optional);
  //  * The absolute path to this module;
  //  * The threading model of this module.
  ATL::CRegKey parent;
  LONG result = parent.Open(HKEY_CLASSES_ROOT, &kTipInfoKeyPrefix[0],
                            KEY_READ | KEY_WRITE);
  if (result != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(result);
  }

  // Create a sub-key for this COM module and set its description.
  // These operations are allowed only for administrators.
  wchar_t ime_key[64] = {};
  if (!::StringFromGUID2(TsfProfile::GetTextServiceGuid(), &ime_key[0],
                         std::size(ime_key))) {
    return E_OUTOFMEMORY;
  }

  ATL::CRegKey key;
  result = key.Create(parent, &ime_key[0], REG_NONE, REG_OPTION_NON_VOLATILE,
                      KEY_READ | KEY_WRITE, nullptr, 0);
  if (result != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(result);
  }

  std::wstring description = Utf8ToWide(mozc::kProductNameInEnglish);

  result = key.SetStringValue(nullptr, description.c_str(), REG_SZ);
  if (result != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(result);
  }

  // Write the absolute path to this module and its threading model.
  // Windows use these values to load this module and set it up.
  result = key.SetKeyValue(&kTipInProcServer32[0], &path[0], nullptr);
  if (result != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(result);
  }

  result = key.SetKeyValue(&kTipInProcServer32[0], &kTipTextServiceModel[0],
                           &kTipThreadingModel[0]);
  if (result != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(result);
  }

  return HRESULT_FROM_WIN32(key.Close());
}

// Unregisters this module from Windows.
// This function just deletes the all registry keys under
// "HKCR\CLSID\{xxxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxx}".
void TsfRegistrar::UnregisterCOMServer() {
  // Create the registry key for this COM server.
  // Only Administrators can create keys under HKEY_CLASSES_ROOT.
  wchar_t ime_key[64] = {};
  if (!::StringFromGUID2(TsfProfile::GetTextServiceGuid(), &ime_key[0],
                         std::size(ime_key))) {
    return;
  }

  ATL::CRegKey key;
  HRESULT result =
      key.Open(HKEY_CLASSES_ROOT, &kTipInfoKeyPrefix[0], KEY_READ | KEY_WRITE);
  if (result != ERROR_SUCCESS) {
    return;
  }

  key.RecurseDeleteKey(&ime_key[0]);
  key.Close();
}

// Register this COM server to the profile store for input processors.
// After completing this operation, Windows can treat this module as a
// text-input service.
// To see the list of registers input processors:
//  1. Open the "Control Panel";
//  2. Select "Date, Time, Language and Regional Options";
//  3. Select "Language and Regional Options";
//  4. Click the "Languages" tab;
//  5. Click "Details" in the "Text services and input languages" frame, and;
//  6. All installed prossors are enumerated in the "Installed services"
//     frame.
HRESULT TsfRegistrar::RegisterProfiles(const wchar_t *path, DWORD path_length) {
  // Retrieve the profile store for input processors.
  // If you might want to create the manager object w/o calling the pair of
  // CoInitialize/CoUninitialize, there is a helper function to retrieve the
  // object.
  // http://msdn.microsoft.com/en-us/library/ms629059.aspx
  ComPtr<ITfInputProcessorProfiles> profiles;
  HRESULT result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                    CLSCTX_ALL, IID_PPV_ARGS(&profiles));
  if (FAILED(result)) {
    return result;
  }

  // Register this COM server as an input processor, and add this module as an
  // input processor for the language |kTextServiceLanguage|.
  result = profiles->Register(TsfProfile::GetTextServiceGuid());
  if (SUCCEEDED(result)) {
    // We use English name here as culture-invariant description.
    // Localized name is specified later by SetLanguageProfileDisplayName.
    std::wstring description = Utf8ToWide(mozc::kProductNameInEnglish);

    result = profiles->AddLanguageProfile(
        TsfProfile::GetTextServiceGuid(), TsfProfile::GetLangId(),
        TsfProfile::GetProfileGuid(), description.c_str(), description.size(),
        path, path_length, TsfProfile::GetIconIndex());

    HRESULT set_display_name_result = S_OK;
    ComPtr<ITfInputProcessorProfilesEx> profiles_ex;
    set_display_name_result = profiles.As(&profiles_ex);
    if (SUCCEEDED(set_display_name_result) && profiles_ex != nullptr) {
      // Unfortunately, the document of SetLanguageProfileDisplayName is very
      // poor but we can guess that the mechanism of MUI is similar to that of
      // IMM32. IMM32 uses registry value "Layout Text" and
      // "Layout Display Name", where the content of "Layout Display Name" is
      // used by SHLoadIndirectString to display appropriate string based on
      // the current UI language.
      // This mechanism is called "Registry String Redirection".
      //   http://msdn.microsoft.com/en-us/library/dd374120.aspx
      // You can find similar string based on "Registry String Redirection"
      // used by the TSF:
      //   HKLM\SOFTWARE\Microsoft\CTF\TIP\<TextService CLSID>\LanguageProfile
      //       \<LangID>\<Profile GUID>\Display Description
      // Therefore, it is considered that arguments "pchFile" and "pchFile" of
      // the SetLanguageProfileDisplayName is resource file name and string
      // resource id, respectively.

      // You should use a new resource ID when you need to update the MUI text
      // because SetLanguageProfileDisplayName does not support version
      // modifiers.  See b/2994558 and the following article for details.
      // http://msdn.microsoft.com/en-us/library/bb759919.aspx
      set_display_name_result = profiles_ex->SetLanguageProfileDisplayName(
          TsfProfile::GetTextServiceGuid(), TsfProfile::GetLangId(),
          TsfProfile::GetProfileGuid(), path, path_length,
          TsfProfile::GetDescriptionTextIndex());
      if (FAILED(set_display_name_result)) {
        LOG(ERROR) << "SetLanguageProfileDisplayName failed."
                   << " hr = " << set_display_name_result;
      }
    }
  }

  return result;
}

// Unregister this COM server from the text-service framework.
void TsfRegistrar::UnregisterProfiles() {
  // If you might want to create the manager object w/o calling the pair of
  // CoInitialize/CoUninitialize, there is a helper function to retrieve the
  // object.
  // http://msdn.microsoft.com/en-us/library/ms629059.aspx
  ComPtr<ITfInputProcessorProfiles> profiles;
  HRESULT result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                                    CLSCTX_ALL, IID_PPV_ARGS(&profiles));
  if (SUCCEEDED(result)) {
    result = profiles->Unregister(TsfProfile::GetTextServiceGuid());
  }
}

// Retrieves the category manager for text input processors, and
// registers this module as a keyboard and a display attribute provider.
HRESULT TsfRegistrar::RegisterCategories() {
  // If you might want to create the manager object w/o calling the pair of
  // CoInitialize/CoUninitialize, there is a helper function to retrieve the
  // object.
  // http://msdn.microsoft.com/en-us/library/aa383439.aspx
  ComPtr<ITfCategoryMgr> category;
  HRESULT result = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_ALL,
                                    IID_PPV_ARGS(&category));
  if (SUCCEEDED(result)) {
    for (int i = 0; i < std::size(kCategories); ++i) {
      result = category->RegisterCategory(TsfProfile::GetTextServiceGuid(),
                                          kCategories[i],
                                          TsfProfile::GetTextServiceGuid());
    }
  }

  return result;
}

// Retrieves the category manager for text input processors, and
// unregisters this keyboard module.
void TsfRegistrar::UnregisterCategories() {
  // If you might want to create the manager object w/o calling the pair of
  // CoInitialize/CoUninitialize, there is a helper function to retrieve the
  // object.
  // http://msdn.microsoft.com/en-us/library/aa383439.aspx
  ComPtr<ITfCategoryMgr> category_mgr;
  HRESULT result = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_ALL,
                                    IID_PPV_ARGS(&category_mgr));
  if (SUCCEEDED(result)) {
    for (const auto &category : kCategories) {
      result = category_mgr->UnregisterCategory(
          TsfProfile::GetTextServiceGuid(), category,
          TsfProfile::GetTextServiceGuid());
    }
  }
}

HRESULT TsfRegistrar::GetProfileEnabled(BOOL *enabled) {
  BOOL dummy_bool = FALSE;
  if (enabled == nullptr) {
    enabled = &dummy_bool;
  }
  *enabled = FALSE;

  const int num_profiles =
      ::EnumEnabledLayoutOrTip(nullptr, nullptr, nullptr, nullptr, 0);
  std::unique_ptr<LAYOUTORTIPPROFILE[]> profiles(
      new LAYOUTORTIPPROFILE[num_profiles]);
  const int num_copied = ::EnumEnabledLayoutOrTip(nullptr, nullptr, nullptr,
                                                  profiles.get(), num_profiles);

  for (size_t i = 0; i < num_copied; ++i) {
    if ((profiles[i].dwProfileType == LOTP_INPUTPROCESSOR) &&
        ::IsEqualGUID(profiles[i].clsid, TsfProfile::GetTextServiceGuid()) &&
        (profiles[i].langid == TsfProfile::GetLangId()) &&
        ::IsEqualGUID(profiles[i].guidProfile, TsfProfile::GetProfileGuid())) {
      // Found.
      *enabled = TRUE;
      return S_OK;
    }
  }

  // Not Found.
  *enabled = FALSE;
  return S_OK;
}

HRESULT TsfRegistrar::SetProfileEnabled(BOOL enable) {
  BOOL is_enabled = FALSE;
  HRESULT result = GetProfileEnabled(&is_enabled);
  if (FAILED(result)) {
    return result;
  }

  if ((is_enabled == FALSE) && (enable == FALSE)) {
    // Already disabled.  Nothing to do here.
    return S_OK;
  }

  // On Windows Vista or Windows 7 x64 editions,
  // ITfInputProcessorProfileMgr::DeactivateProfile seems to be more reliable
  // as opposed to ITfInputProcessorProfiles::EnableLanguageProfile, which
  // sometimes fails on those environment for some reason.  See b/3002349.
  if (enable == FALSE) {
    ComPtr<ITfInputProcessorProfileMgr> profile_mgr;
    result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                              CLSCTX_ALL, IID_PPV_ARGS(&profile_mgr));
    if (SUCCEEDED(result)) {
      result = profile_mgr->DeactivateProfile(
          TF_PROFILETYPE_INPUTPROCESSOR, TsfProfile::GetLangId(),
          TsfProfile::GetTextServiceGuid(), TsfProfile::GetProfileGuid(),
          nullptr, TF_IPPMF_FORSESSION | TF_IPPMF_DISABLEPROFILE);
      if (SUCCEEDED(result)) {
        return result;
      }
    }
  }

  // Retrieve the profile store for input processors.
  // If you might want to create the manager object w/o calling the pair of
  // CoInitialize/CoUninitialize, there is a helper function to retrieve the
  // object.
  // http://msdn.microsoft.com/en-us/library/ms629059.aspx
  ComPtr<ITfInputProcessorProfiles> profiles;
  result = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr,
                            CLSCTX_ALL, IID_PPV_ARGS(&profiles));
  if (FAILED(result)) {
    return result;
  }

  return profiles->EnableLanguageProfile(TsfProfile::GetTextServiceGuid(),
                                         TsfProfile::GetLangId(),
                                         TsfProfile::GetProfileGuid(), enable);
}

}  // namespace win32
}  // namespace mozc
