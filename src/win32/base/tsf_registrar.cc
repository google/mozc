// Copyright 2010-2013, Google Inc.
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

#include <windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <msctf.h>
#include <objbase.h>

#include "base/const.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "win32/base/display_name_resource.h"
#include "win32/base/input_dll.h"
#include "win32/base/tsf_profile.h"

namespace mozc {
namespace win32 {

namespace {

// Defines the constant strings used in the TsfRegistrar::RegisterCOMServer()
// function and the TsfRegistrar::UnregisterCOMServer() function.
// We define these strings as WCHAR arrays to retrieve the size of this
// string with the ARRAYSIZE() macro.
const wchar_t kTipInfoKeyPrefix[] = L"CLSID\\";
const wchar_t kTipInProcServer32[] = L"InProcServer32";
const wchar_t kTipThreadingModel[] = L"ThreadingModel";
const wchar_t kTipTextServiceModel[] = L"Apartment";

// GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT
const GUID KGuidTfcatTipcapImmersiveSupport = {
  0x13a016df, 0x560b, 0x46cd, {0x94, 0x7a, 0x4c, 0x3a, 0xf1, 0xe0, 0xe3, 0x5d}
};
// GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT
const GUID KGuidTfcatTipcapSystraySupport = {
  0x25504fb4, 0x7bab, 0x4bc1, {0x9c, 0x69, 0xcf, 0x81, 0x89, 0x0f, 0x0e, 0xf5}
};

// The categories this text service is registered under.
const GUID kCategories[] = {
  GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,     // It supports inline input.
  GUID_TFCAT_TIPCAP_COMLESS,               // It's a COM-Less module.
  GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,  // It supports input mode.
  GUID_TFCAT_TIPCAP_UIELEMENTENABLED,      // It supports UI less mode.
  GUID_TFCAT_TIP_KEYBOARD,                 // It's a keyboard input method.
  KGuidTfcatTipcapImmersiveSupport,        // It supports Metro mode.
  KGuidTfcatTipcapSystraySupport,          // It supports Win8 systray.
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
                         arraysize(ime_key))) {
    return E_OUTOFMEMORY;
  }

  ATL::CRegKey key;
  result = key.Create(parent, &ime_key[0], REG_NONE, REG_OPTION_NON_VOLATILE,
                      KEY_READ | KEY_WRITE, nullptr, 0);
  if (result != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(result);
  }

  wstring description;
  mozc::Util::UTF8ToWide(mozc::kProductNameInEnglish, &description);

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
  if (!::StringFromGUID2(TsfProfile::GetTextServiceGuid(),
                         &ime_key[0], arraysize(ime_key))) {
    return;
  }

  ATL::CRegKey key;
  HRESULT result = key.Open(HKEY_CLASSES_ROOT, &kTipInfoKeyPrefix[0],
                            KEY_READ | KEY_WRITE);
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
HRESULT TsfRegistrar::RegisterProfiles(const wchar_t *path,
                                       DWORD path_length) {
  // Retrieve the profile store for input processors.
  // If you might want to create the manager object w/o calling the pair of
  // CoInitialize/CoUninitialize, there is a helper function to retrieve the
  // object.
  // http://msdn.microsoft.com/en-us/library/ms629059.aspx
  ATL::CComPtr<ITfInputProcessorProfiles> profiles;
  HRESULT result = profiles.CoCreateInstance(CLSID_TF_InputProcessorProfiles);
  if (result != S_OK) {
    return result;
  }

  // Register this COM server as an input processor, and add this module as an
  // input processor for the language |kTextServiceLanguage|.
  result = profiles->Register(TsfProfile::GetTextServiceGuid());
  if (result == S_OK) {
    // We use English name here as culture-invariant description.
    // Localized name is specified later by SetLanguageProfileDisplayName.
    wstring description;
    mozc::Util::UTF8ToWide(mozc::kProductNameInEnglish, &description);

    result = profiles->AddLanguageProfile(TsfProfile::GetTextServiceGuid(),
                                          TsfProfile::GetLangId(),
                                          TsfProfile::GetProfileGuid(),
                                          description.c_str(),
                                          description.size(),
                                          path,
                                          path_length,
                                          TsfProfile::GetIconIndex());

    HRESULT set_display_name_result = S_OK;
    ATL::CComPtr<ITfInputProcessorProfilesEx> profiles_ex;
    set_display_name_result = profiles.QueryInterface(&profiles_ex);
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
          TsfProfile::GetTextServiceGuid(),
          TsfProfile::GetLangId(),
          TsfProfile::GetProfileGuid(), path,
          path_length,
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
  ATL::CComPtr<ITfInputProcessorProfiles> profiles;
  HRESULT result = profiles.CoCreateInstance(CLSID_TF_InputProcessorProfiles);
  if (result == S_OK) {
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
  ATL::CComPtr<ITfCategoryMgr> category;
  HRESULT result = category.CoCreateInstance(CLSID_TF_CategoryMgr);
  if (result == S_OK) {
    for (int i = 0; i < arraysize(kCategories); ++i) {
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
  ATL::CComPtr<ITfCategoryMgr> category;
  HRESULT result = category.CoCreateInstance(CLSID_TF_CategoryMgr);
  if (result == S_OK) {
    for (int i = 0; i < arraysize(kCategories); ++i) {
      result = category->UnregisterCategory(TsfProfile::GetTextServiceGuid(),
                                            kCategories[i],
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

  // Check if input.dll is exporting EnumEnabledLayoutOrTIP API, which is the
  // best way to enumerate enabled profiles for the current user.
  if (InputDll::EnsureInitialized() ||
      (InputDll::enum_enabled_layout_or_tip() == nullptr)) {
    return E_FAIL;
  }

  const int num_profiles = InputDll::enum_enabled_layout_or_tip()(
      nullptr, nullptr, nullptr, nullptr, 0);
  scoped_array<LAYOUTORTIPPROFILE> profiles(
      new LAYOUTORTIPPROFILE[num_profiles]);
  const int num_copied = InputDll::enum_enabled_layout_or_tip()(
      nullptr, nullptr, nullptr, profiles.get(), num_profiles);

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
    ATL::CComPtr<ITfInputProcessorProfileMgr> profile_mgr;
    result = profile_mgr.CoCreateInstance(CLSID_TF_InputProcessorProfiles);
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
  ATL::CComPtr<ITfInputProcessorProfiles> profiles;
  result = profiles.CoCreateInstance(CLSID_TF_InputProcessorProfiles);
  if (FAILED(result)) {
    return result;
  }

  return profiles->EnableLanguageProfile(TsfProfile::GetTextServiceGuid(),
                                         TsfProfile::GetLangId(),
                                         TsfProfile::GetProfileGuid(),
                                         enable);
}

}  // namespace win32
}  // namespace mozc
