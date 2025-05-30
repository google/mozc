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

#include <atlbase.h>
#include <msctf.h>
#include <objbase.h>
#include <wil/com.h>
#include <windows.h>

#include <string>
#include <string_view>

#include "absl/log/log.h"
#include "base/const.h"
#include "base/win32/com.h"
#include "base/win32/wide_char.h"
#include "win32/base/display_name_resource.h"
#include "win32/base/tsf_profile.h"

namespace mozc {
namespace win32 {
namespace {

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
HRESULT TsfRegistrar::RegisterProfiles(std::wstring_view resource_dll_path) {
  // Retrieve the profile store for input processors.
  // If you might want to create the manager object w/o calling the pair of
  // CoInitialize/CoUninitialize, there is a helper function to retrieve the
  // object.
  // http://msdn.microsoft.com/en-us/library/ms629059.aspx
  auto profiles = ComCreateInstance<ITfInputProcessorProfiles>(
      CLSID_TF_InputProcessorProfiles);
  if (!profiles) {
    return E_FAIL;
  }

  // Register this COM server as an input processor, and add this module as an
  // input processor for the language |kTextServiceLanguage|.
  HRESULT result = profiles->Register(TsfProfile::GetTextServiceGuid());
  if (SUCCEEDED(result)) {
    // We use English name here as culture-invariant description.
    // Localized name is specified later by SetLanguageProfileDisplayName.
    std::wstring description = Utf8ToWide(mozc::kProductNameInEnglish);

    result = profiles->AddLanguageProfile(
        TsfProfile::GetTextServiceGuid(), TsfProfile::GetLangId(),
        TsfProfile::GetProfileGuid(), description.c_str(), description.size(),
        resource_dll_path.data(), resource_dll_path.length(),
        TsfProfile::GetIconIndex());

    auto profiles_ex = ComQuery<ITfInputProcessorProfilesEx>(profiles);
    if (profiles_ex) {
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
      HRESULT set_display_name_result =
          profiles_ex->SetLanguageProfileDisplayName(
              TsfProfile::GetTextServiceGuid(), TsfProfile::GetLangId(),
              TsfProfile::GetProfileGuid(), resource_dll_path.data(),
              resource_dll_path.length(),
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
  auto profiles = ComCreateInstance<ITfInputProcessorProfiles>(
      CLSID_TF_InputProcessorProfiles);
  if (profiles) {
    (void)profiles->Unregister(TsfProfile::GetTextServiceGuid());
  }
}

// Retrieves the category manager for text input processors, and
// registers this module as a keyboard and a display attribute provider.
HRESULT TsfRegistrar::RegisterCategories() {
  // If you might want to create the manager object w/o calling the pair of
  // CoInitialize/CoUninitialize, there is a helper function to retrieve the
  // object.
  // http://msdn.microsoft.com/en-us/library/aa383439.aspx
  auto category = ComCreateInstance<ITfCategoryMgr>(CLSID_TF_CategoryMgr);
  if (!category) {
    return E_FAIL;
  }
  HRESULT result = S_OK;
  if (category) {
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
  auto category_mgr = ComCreateInstance<ITfCategoryMgr>(CLSID_TF_CategoryMgr);
  if (category_mgr) {
    for (const auto &category : kCategories) {
      category_mgr->UnregisterCategory(TsfProfile::GetTextServiceGuid(),
                                       category,
                                       TsfProfile::GetTextServiceGuid());
    }
  }
}

}  // namespace win32
}  // namespace mozc
