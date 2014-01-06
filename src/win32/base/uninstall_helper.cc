// Copyright 2010-2014, Google Inc.
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

#include "win32/base/uninstall_helper.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <msctf.h>
#include <strsafe.h>
#include <objbase.h>

#include <iomanip>
#include <map>
#include <memory>
#include <sstream>

#include "base/logging.h"
#include "base/scoped_handle.h"
#include "base/system_util.h"
#include "base/win_util.h"
#include "win32/base/imm_registrar.h"
#include "win32/base/imm_util.h"
#include "win32/base/immdev.h"
#include "win32/base/tsf_profile.h"

namespace mozc {
namespace win32 {
using ATL::CComPtr;
using ATL::CRegKey;
using std::unique_ptr;

namespace {

typedef map<int, DWORD> PreloadOrderToKLIDMap;

// Windows NT 5.1
const DWORD kDefaultKLIDForMSIMEJa = 0xE0010411;
const wchar_t kDefaultMSIMEJaFileName[] = L"imjp81.ime";

// Windows NT 6.0, 6.1 and 6.2
const CLSID CLSID_IMJPTIP = {
    0x03b5835f, 0xf03c, 0x411b, {0x9c, 0xe2, 0xaa, 0x23, 0xe1, 0x17, 0x1e, 0x36}
};
const GUID GUID_IMJPTIP = {
    0xa76c93d9, 0x5523, 0x4e90, {0xaa, 0xfa, 0x4d, 0xb1, 0x12, 0xf9, 0xac, 0x76}
};

const LANGID kLANGJaJP = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);

const wchar_t kRegKeyboardLayouts[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts";
const wchar_t kPreloadKeyName[] = L"Keyboard Layout\\Preload";

// Registry element size limits are described in the link below.
// http://msdn.microsoft.com/en-us/library/ms724872(VS.85).aspx
const DWORD kMaxValueNameLength = 16383;


// Converts an unsigned integer to a wide string.
wstring utow(unsigned int i) {
  wstringstream ss;
  ss << i;
  return ss.str();
}

wstring GetIMEFileNameFromKeyboardLayout(
    const CRegKey &key, const KeyboardLayoutID &klid) {
  CRegKey subkey;
  LONG result = subkey.Open(key, klid.ToString().c_str(), KEY_READ);
  if (ERROR_SUCCESS != result) {
    return L"";
  }

  wchar_t filename_buffer[kMaxValueNameLength];
  ULONG filename_length_including_null = kMaxValueNameLength;
  result = subkey.QueryStringValue(
      L"Ime File", filename_buffer, &filename_length_including_null);

  // Note that |filename_length_including_null| contains NUL terminator.
  if ((ERROR_SUCCESS != result) || (filename_length_including_null <= 1)) {
    return L"";
  }

  const ULONG filename_length = (filename_length_including_null - 1);
  const wstring filename(filename_buffer);

  // Note that |filename_length| does not contain NUL character.
  DCHECK_EQ(filename_length, filename.size());
  return filename;
}

bool GenerateKeyboardLayoutList(vector<KeyboardLayoutInfo> *keyboard_layouts) {
  if (keyboard_layouts == nullptr) {
    return false;
  }
  keyboard_layouts->clear();

  CRegKey key;
  LONG result = key.Open(
      HKEY_LOCAL_MACHINE, kRegKeyboardLayouts, KEY_READ);
  if (ERROR_SUCCESS != result) {
    return false;
  }

  wchar_t value_name[kMaxValueNameLength];
  for (DWORD enum_reg_index = 0;; ++enum_reg_index) {
    DWORD value_name_length = kMaxValueNameLength;
    result = key.EnumKey(
        enum_reg_index,
        value_name,
        &value_name_length);
    if (ERROR_NO_MORE_ITEMS == result) {
      return true;
    }
    if (ERROR_SUCCESS != result) {
      return true;
    }

    // Note that |value_name_length| does not contain NUL character.
    const KeyboardLayoutID klid(
        wstring(value_name, value_name + value_name_length));

    if (!klid.has_id()) {
      continue;
    }

    KeyboardLayoutInfo info;
    info.klid = klid.id();
    info.ime_filename = GetIMEFileNameFromKeyboardLayout(key, klid);
    keyboard_layouts->push_back(info);
  }
  return true;
}

bool GenerateKeyboardLayoutMap(map<DWORD, wstring> *keyboard_layouts) {
  if (keyboard_layouts == nullptr) {
    return false;
  }
  vector<KeyboardLayoutInfo> keyboard_layout_list;
  if (!GenerateKeyboardLayoutList(&keyboard_layout_list)) {
    return false;
  }

  for (size_t i = 0; i < keyboard_layout_list.size(); ++i) {
    const KeyboardLayoutInfo &layout = keyboard_layout_list[i];
    (*keyboard_layouts)[layout.klid] = layout.ime_filename;
  }

  return true;
}

wstring GetIMEFileName(HKL hkl) {
  const UINT num_chars_without_null = ::ImmGetIMEFileName(hkl, nullptr, 0);
  const size_t num_chars_with_null = num_chars_without_null + 1;
  unique_ptr<wchar_t[]> buffer(new wchar_t[num_chars_with_null]);
  const UINT num_copied =
      ::ImmGetIMEFileName(hkl, buffer.get(), num_chars_with_null);

  // |num_copied| does not include terminating null character.
  return wstring(buffer.get(), buffer.get() + num_copied);
}

bool GetInstalledProfilesByLanguageForTSF(
    LANGID langid,
    vector<LayoutProfileInfo> *installed_profiles) {
  ScopedCOMInitializer com_initializer;
  if (FAILED(com_initializer.error_code())) {
    return false;
  }

  HRESULT hr = S_OK;

  CComPtr<ITfInputProcessorProfiles> profiles;
  hr = profiles.CoCreateInstance(CLSID_TF_InputProcessorProfiles);
  if (FAILED(hr)) {
    return false;
  }

  CComPtr<IEnumTfLanguageProfiles> enum_profiles;
  hr = profiles->EnumLanguageProfiles(langid, &enum_profiles);
  if (FAILED(hr)) {
    return false;
  }

  while (true) {
    ULONG num_fetched = 0;
    TF_LANGUAGEPROFILE src = {0};
    hr = enum_profiles->Next(1, &src, &num_fetched);
    if (FAILED(hr)) {
      return false;
    }
    if ((hr == S_FALSE) || (num_fetched != 1)) {
      break;
    }

    if (src.catid != GUID_TFCAT_TIP_KEYBOARD) {
      continue;
    }
    LayoutProfileInfo profile;
    profile.langid = src.langid;
    profile.is_default = (src.fActive != FALSE);
    BOOL enabled = FALSE;
    hr = profiles->IsEnabledLanguageProfile(
        src.clsid, langid, src.guidProfile, &enabled);
    if (SUCCEEDED(hr)) {
      profile.is_enabled = (enabled != FALSE);
    }
    profile.clsid = src.clsid;
    profile.profile_guid = src.guidProfile;
    profile.is_tip = true;
    installed_profiles->push_back(profile);
  }

  return true;
}

bool GetInstalledProfilesByLanguageForIMM32(
    LANGID langid,
    vector<LayoutProfileInfo> *installed_profiles) {
  vector<KeyboardLayoutInfo> keyboard_layouts;
  if (!GenerateKeyboardLayoutList(&keyboard_layouts)) {
    DLOG(ERROR) << "GenerateKeyboardLayoutList failed.";
    return false;
  }

  for (size_t i = 0; i < keyboard_layouts.size(); ++i) {
    const KeyboardLayoutInfo &info = keyboard_layouts[i];
    const LANGID info_langid = static_cast<LANGID>(info.klid & 0xffff);
    if (info_langid == langid) {
      LayoutProfileInfo profile;
      profile.langid = langid;
      profile.is_tip = false;
      profile.klid = info.klid;
      // TODO(yukawa): determine |profile.is_default|
      // TODO(yukawa): determine |profile.is_enabled|
      profile.ime_filename = info.ime_filename;
      installed_profiles->push_back(profile);
    }
  }

  return true;
}

bool GetPreloadLayoutsMain(PreloadOrderToKLIDMap *preload_map) {
  if (preload_map == nullptr) {
    return false;
  }

  // Retrieve keys under kPreloadKeyName.
  CRegKey preload_key;
  LONG result = preload_key.Open(HKEY_CURRENT_USER, kPreloadKeyName);
  if (ERROR_SUCCESS != result) {
    return false;
  }

  wchar_t value_name[kMaxValueNameLength];
  const DWORD kMaxValueLength = 256;
  BYTE value[kMaxValueLength];
  for (DWORD i = 0;; ++i) {
    DWORD value_name_length = kMaxValueNameLength;
    DWORD value_length = kMaxValueLength;
    result = RegEnumValue(preload_key,
                          i,
                          value_name,
                          &value_name_length,
                          nullptr,  // reserved (must be nullptr)
                          nullptr,  // type (optional)
                          value,
                          &value_length);
    if (ERROR_NO_MORE_ITEMS == result) {
      break;
    }
    if (ERROR_SUCCESS != result) {
      return false;
    }

    const int ivalue_name = _wtoi(value_name);
    const wstring wvalue(reinterpret_cast<wchar_t*>(value),
                         (value_length / sizeof(wchar_t)) - 1);
    KeyboardLayoutID klid(wvalue);
    if (!klid.has_id()) {
      continue;
    }
    (*preload_map)[ivalue_name] = klid.id();
  }
  return true;
}

wstring GUIDToString(const GUID &guid) {
  wchar_t buffer[256];
  const int character_length_with_null =
      ::StringFromGUID2(guid, buffer, arraysize(buffer));
  if (character_length_with_null <= 0) {
    return L"";
  }

  const size_t character_length_without_null =
      character_length_with_null - 1;
  return wstring(buffer, buffer + character_length_without_null);
}

wstring LANGIDToString(LANGID langid) {
  wchar_t buffer[5];
  HRESULT hr = ::StringCchPrintf(buffer, arraysize(buffer), L"%04x", langid);
  if (FAILED(hr)) {
    return L"";
  }
  return buffer;
}

bool BroadcastNewIME(const KeyboardLayoutInfo &layout) {
  KeyboardLayoutID klid(layout.klid);

  // We cannot use const HKL because |&mozc_hkl| will be cast into PVOID.
  HKL hkl = ::LoadKeyboardLayout(klid.ToString().c_str(), KLF_ACTIVATE);

  // SPI_SETDEFAULTINPUTLANG ensures that new process in this session will
  // use |hkl| by default but this setting is volatile even if you specified
  // SPIF_UPDATEINIFILE flag.
  // SPI_SETDEFAULTINPUTLANG does not work perfectly for a HKL substituted
  // by a TIP on Windows XP.  It works for notepad but wordpad still uses
  // the previous layout.  Consider to use
  // ITfInputProcessorProfiles::SetDefaultLanguageProfile for TIP backed
  // layout.
  if (::SystemParametersInfo(
          SPI_SETDEFAULTINPUTLANG, 0, &hkl, SPIF_SENDCHANGE) == FALSE) {
    LOG(ERROR) << "SystemParameterInfo failed: " << GetLastError();
    return false;
  }


  // Broadcasting WM_INPUTLANGCHANGEREQUEST so that existing process in the
  // current session will change their input method to |hkl|. This mechanism
  // also works against a HKL which is substituted by a TIP on Windows XP.
  // Note: we have virtually the same code in imm_util.cc too.
  // TODO(yukawa): Make a common function around WM_INPUTLANGCHANGEREQUEST.
  DWORD recipients = BSM_APPLICATIONS;
  const LONG result = ::BroadcastSystemMessage(
      BSF_POSTMESSAGE,
      &recipients,
      WM_INPUTLANGCHANGEREQUEST,
      INPUTLANGCHANGE_SYSCHARSET,
      reinterpret_cast<LPARAM>(hkl));
  if (result == 0) {
    const int error = ::GetLastError();
    LOG(ERROR) << "BroadcastSystemMessage failed. error = " << error;
    return false;
  }

  return true;
}

bool BroadcastNewTIPOnVista(const LayoutProfileInfo &profile) {
  ScopedCOMInitializer com_initializer;
  if (FAILED(com_initializer.error_code())) {
    return false;
  }

  CComPtr<ITfInputProcessorProfileMgr> profile_manager;

  HRESULT hr = S_OK;
  hr = profile_manager.CoCreateInstance(CLSID_TF_InputProcessorProfiles);
  if (FAILED(hr)) {
    return false;
  }

  const DWORD activate_flags =
      TF_IPPMF_FORSESSION | TF_IPPMF_ENABLEPROFILE |
      TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE;

  hr = profile_manager->ActivateProfile(
      TF_PROFILETYPE_INPUTPROCESSOR, profile.langid, profile.clsid,
      profile.profile_guid, nullptr, activate_flags);
  if (FAILED(hr)) {
    return false;
  }

  return true;
}

bool EnableAndBroadcastNewLayout(
    const LayoutProfileInfo &profile, bool broadcast_change) {
  if (profile.is_tip) {
    ScopedCOMInitializer com_initializer;
    if (FAILED(com_initializer.error_code())) {
      return false;
    }

    CComPtr<ITfInputProcessorProfileMgr> profile_manager;

    HRESULT hr = S_OK;
    hr = profile_manager.CoCreateInstance(CLSID_TF_InputProcessorProfiles);
    if (FAILED(hr)) {
      return false;
    }

    DWORD activate_flags = TF_IPPMF_ENABLEPROFILE |
                           TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE;

    if (broadcast_change) {
      activate_flags |= TF_IPPMF_FORSESSION;
    }
    hr = profile_manager->ActivateProfile(
        TF_PROFILETYPE_INPUTPROCESSOR, profile.langid, profile.clsid,
        profile.profile_guid, nullptr, activate_flags);
    if (FAILED(hr)) {
      DLOG(ERROR) << "ActivateProfile failed";
      return false;
    }

    return true;
  }

  // |profile| is IME.
  if (!broadcast_change) {
    return true;
  }

  KeyboardLayoutInfo layout;
  layout.klid = profile.klid;
  layout.ime_filename = profile.ime_filename;
  if (!BroadcastNewIME(layout)) {
    DLOG(ERROR) << "BroadcastNewIME failed";
    return false;
  }
  return true;
}

bool GetActiveKeyboardLayouts(vector<HKL> *keyboard_layouts) {
  if (keyboard_layouts == nullptr) {
    return false;
  }
  keyboard_layouts->clear();

  const int num_keyboard_layout = ::GetKeyboardLayoutList(0, nullptr);
  unique_ptr<HKL[]> buffer(new HKL[num_keyboard_layout]);
  const int num_copied = ::GetKeyboardLayoutList(num_keyboard_layout,
                                                 buffer.get());
  keyboard_layouts->assign(buffer.get(), buffer.get() + num_copied);

  return true;
}

void EnableAndSetDefaultIfLayoutIsTIP(const KeyboardLayoutInfo &layout) {
  ScopedCOMInitializer com_initializer;
  if (FAILED(com_initializer.error_code())) {
    return;
  }

  HRESULT hr = S_OK;

  CComPtr<ITfInputProcessorProfiles> profiles;
  hr = profiles.CoCreateInstance(CLSID_TF_InputProcessorProfiles);
  if (FAILED(hr)) {
    return;
  }
  CComPtr<ITfInputProcessorProfileSubstituteLayout> substitute_layout;
  hr = profiles.QueryInterface(&substitute_layout);
  if (FAILED(hr)) {
    return;
  }

  CComPtr<IEnumTfLanguageProfiles> enum_profiles;
  const LANGID langid = static_cast<LANGID>(layout.klid);
  hr = profiles->EnumLanguageProfiles(langid, &enum_profiles);
  if (FAILED(hr)) {
    return;
  }

  while (true) {
    ULONG num_fetched = 0;
    TF_LANGUAGEPROFILE profile = {0};
    hr = enum_profiles->Next(1, &profile, &num_fetched);
    if (FAILED(hr)) {
      return;
    }
    if ((hr == S_FALSE) || (num_fetched != 1)) {
      return;
    }

    if (profile.catid != GUID_TFCAT_TIP_KEYBOARD) {
      continue;
    }

    HKL hkl = nullptr;
    hr = substitute_layout->GetSubstituteKeyboardLayout(profile.clsid,
                                                        profile.langid,
                                                        profile.guidProfile,
                                                        &hkl);
    if (FAILED(hr)) {
      DLOG(ERROR) << "GetSubstituteKeyboardLayout failed";
      continue;
    }
    if (!WinUtil::SystemEqualString(
             GetIMEFileName(hkl), layout.ime_filename, true)) {
      continue;
    }

    BOOL enabled = TRUE;
    hr = profiles->EnableLanguageProfile(profile.clsid,
                                         profile.langid,
                                         profile.guidProfile,
                                         TRUE);
    if (FAILED(hr)) {
      DLOG(ERROR) << "EnableLanguageProfile failed";
      continue;
    }

    hr = profiles->SetDefaultLanguageProfile(profile.langid,
                                             profile.clsid,
                                             profile.guidProfile);
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetDefaultLanguageProfile failed";
      continue;
    }
    return;
  }
  return;
}

// This function lists all the active keyboard layouts up and unloads each
// layout based on the specified condition.  If |exclude| is true, this
// function unloads any active IME if it is included in |ime_filenames|.
// If |exclude| is false, this function unloads any active IME unless it is
// included in |ime_filenames|.
void UnloadActivatedKeyboardMain(const vector<wstring> &ime_filenames,
                                 bool exclude) {
  vector<HKL> loaded_layouts;
  if (!GetActiveKeyboardLayouts(&loaded_layouts)) {
    return;
  }
  for (size_t i = 0; i < loaded_layouts.size(); ++i) {
    const HKL hkl = loaded_layouts[i];
    const wstring ime_filename = GetIMEFileName(hkl);
    if (ime_filename.empty()) {
      continue;
    }
    bool can_unload = !exclude;
    for (size_t j = 0; j < ime_filenames.size(); ++j) {
      if (WinUtil::SystemEqualString(ime_filename, ime_filenames[j], true)) {
        can_unload = exclude;
        break;
      }
    }
    if (can_unload) {
      ::UnloadKeyboardLayout(hkl);
    }
  }
}

void UnloadLayoutsForXP(
    const vector<KeyboardLayoutInfo> &new_preload_layouts) {
  vector <wstring> ime_filenames;
  for (size_t i = 0; i < new_preload_layouts.size(); ++i) {
    ime_filenames.push_back(new_preload_layouts[i].ime_filename);
  }
  UnloadActivatedKeyboardMain(ime_filenames, false);
}

void UnloadProfilesForVista(
    const vector<LayoutProfileInfo> &profiles_to_be_removed) {
  vector <wstring> ime_filenames;
  for (size_t i = 0; i < profiles_to_be_removed.size(); ++i) {
    ime_filenames.push_back(profiles_to_be_removed[i].ime_filename);
  }
  UnloadActivatedKeyboardMain(ime_filenames, true);
}

bool IsEqualProfile(const LayoutProfileInfo &lhs,
                    const LayoutProfileInfo &rhs) {
  // Check if the profile type (TIP or IME) is the same.
  if (lhs.is_tip != rhs.is_tip) {
    return false;
  }
  // Check if the target language is the same.
  if (lhs.langid != rhs.langid) {
    return false;
  }

  if (lhs.is_tip) {
    // If both of them are TIP, check if they have the same CLSID and Profile
    // GUID.  Otherwise, they are different from each other.
    DCHECK(rhs.is_tip);
    if (::IsEqualCLSID(lhs.clsid, rhs.clsid) == 0) {
      return false;
    }
    if (::IsEqualGUID(lhs.profile_guid, rhs.profile_guid) == 0) {
      return false;
    }
    return true;
  }

  // If both of them are IME, check if they have the same KLID and IME file
  // name (if any).  Otherwise, they are different from each other.
  DCHECK(!lhs.is_tip);
  DCHECK(!rhs.is_tip);
  if (lhs.klid != rhs.klid) {
    return false;
  }
  if (!WinUtil::SystemEqualString(
           lhs.ime_filename.c_str(), rhs.ime_filename.c_str(), true)) {
    return false;
  }
  return true;
}

bool IsEqualPreload(const PreloadOrderToKLIDMap &current_preload_map,
                    const vector<KeyboardLayoutInfo> &new_preload_layouts) {
  if (current_preload_map.size() != new_preload_layouts.size()) {
    return false;
  }

  int index = 0;
  for (PreloadOrderToKLIDMap::const_iterator it = current_preload_map.begin();
       it != current_preload_map.end(); ++it) {
    const DWORD klid = it->second;
    DCHECK_GE(index, 0);
    DCHECK_LT(index, new_preload_layouts.size());
    if (klid != new_preload_layouts[index].klid) {
      return false;
    }
    ++index;
  }

  return true;
}

// Currently only keyboard layouts which have IME filename are supported.
bool RemoveHotKeyForIME(
    const vector<KeyboardLayoutInfo> &layouts_to_be_removed) {
  bool succeeded = true;
  for (DWORD id = IME_HOTKEY_DSWITCH_FIRST; id <= IME_HOTKEY_DSWITCH_LAST;
       ++id) {
    UINT modifiers = 0;
    UINT virtual_key = 0;
    HKL hkl = nullptr;
    BOOL result = ::ImmGetHotKey(id, &modifiers, &virtual_key, &hkl);
    if (result == FALSE) {
      continue;
    }
    if (hkl == nullptr) {
      continue;
    }
    const wstring ime_name = GetIMEFileName(hkl);
    for (size_t i = 0; i < layouts_to_be_removed.size(); ++i) {
      const KeyboardLayoutInfo &layout = layouts_to_be_removed[i];
      if (layout.ime_filename.empty()) {
        continue;
      }
      if (!WinUtil::SystemEqualString(layout.ime_filename, ime_name, true)) {
        continue;
      }
      // ImmSetHotKey fails when both 2nd and 3rd arguments are valid while 4th
      // argument is nullptr.  To remove the HotKey, pass 0 to them.
      result = ::ImmSetHotKey(id, 0, 0, nullptr);
      if (result == FALSE) {
        succeeded = false;
      }
      break;
    }
  }
  return succeeded;
}

// Currently this function is Mozc-specific.
// TODO(yukawa): Generalize this function for any IME.
void RemoveHotKeyForXP(const vector<KeyboardLayoutInfo> &installed_layouts) {
  vector<KeyboardLayoutInfo> hotkey_remove_targets;
  for (size_t i = 0; i < installed_layouts.size(); ++i) {
    const KeyboardLayoutInfo &layout = installed_layouts[i];
    if (WinUtil::SystemEqualString(
            layout.ime_filename, ImmRegistrar::GetFileNameForIME(), true)) {
      // This is the full IMM32 version of Google Japanese Input.
      hotkey_remove_targets.push_back(layout);
      continue;
    }
  }

  if (!RemoveHotKeyForIME(hotkey_remove_targets)) {
    DLOG(ERROR) << "RemoveHotKeyForIME failed.";
  }
}

// Currently this function is Mozc-specific.
// TODO(yukawa): Generalize this function for any IME.
void RemoveHotKeyForVista(const vector<LayoutProfileInfo> &installed_profiles) {
  vector<KeyboardLayoutInfo> hotkey_remove_targets;
  for (size_t i = 0; i < installed_profiles.size(); ++i) {
    const LayoutProfileInfo &profile = installed_profiles[i];
    if (!profile.is_tip && WinUtil::SystemEqualString(
              profile.ime_filename, ImmRegistrar::GetFileNameForIME(), true)) {
      // This is the full IMM32 version of Google Japanese Input.
      KeyboardLayoutInfo info;
      info.klid = profile.klid;
      info.ime_filename = profile.ime_filename;
      hotkey_remove_targets.push_back(info);
      continue;
    }
  }

  if (!RemoveHotKeyForIME(hotkey_remove_targets)) {
    DLOG(ERROR) << "RemoveHotKeyForIME failed.";
  }
}

}  // namespace

KeyboardLayoutInfo::KeyboardLayoutInfo()
    : klid(0) {}

LayoutProfileInfo::LayoutProfileInfo()
    : langid(0),
      clsid(CLSID_NULL),
      profile_guid(GUID_NULL),
      klid(0),
      is_default(false),
      is_tip(false),
      is_enabled(false) {}

// Currently this function is Mozc-specific.
// TODO(yukawa): Generalize this function for any IME.
bool UninstallHelper::GetNewPreloadLayoutsForXP(
    const vector<KeyboardLayoutInfo> &preload_layouts,
    const vector<KeyboardLayoutInfo> &installed_layouts,
    vector<KeyboardLayoutInfo> *new_preloads) {
  if (new_preloads == nullptr) {
    return false;
  }

  new_preloads->clear();
  for (size_t i = 0; i < preload_layouts.size(); ++i) {
    const KeyboardLayoutInfo &layout = preload_layouts[i];
    if (WinUtil::SystemEqualString(
            layout.ime_filename, ImmRegistrar::GetFileNameForIME(), true)) {
      // This is the full IMM32 version of Google Japanese Input.
      continue;
    }
    new_preloads->push_back(layout);
  }

  if (new_preloads->size() == 0) {
    // TODO(yukawa): Consider this case.
    // Use MS-IME as a fallback.
    KeyboardLayoutInfo msime_info;
    msime_info.klid = kDefaultKLIDForMSIMEJa;
    msime_info.ime_filename = kDefaultMSIMEJaFileName;
    new_preloads->push_back(msime_info);
  }

  return true;
}

// Currently this function is Mozc-specific.
// TODO(yukawa): Generalize this function for any IME and/or TIP.
bool UninstallHelper::GetNewEnabledProfileForVista(
    const vector<LayoutProfileInfo> &current_profiles,
    const vector<LayoutProfileInfo> &installed_profiles,
    LayoutProfileInfo *current_default,
    LayoutProfileInfo *new_default,
    vector<LayoutProfileInfo> *removed_profiles) {
  if (current_default == nullptr) {
    return false;
  }
  // Initialize in case no entry is marked as default.
  *current_default = LayoutProfileInfo();

  if (new_default == nullptr) {
    return false;
  }
  if (removed_profiles == nullptr) {
    return false;
  }
  removed_profiles->clear();

  bool default_found = false;
  bool default_set = false;
  for (size_t i = 0; i < current_profiles.size(); ++i) {
    const LayoutProfileInfo &profile = current_profiles[i];
    if (profile.is_default) {
      *current_default = profile;
    }

    if (!profile.is_tip &&
        WinUtil::SystemEqualString(
            profile.ime_filename, ImmRegistrar::GetFileNameForIME(), true)) {
      // This is the full IMM32 version of Google Japanese Input.
      removed_profiles->push_back(profile);
      continue;
    }
    if (profile.is_tip &&
        ::IsEqualCLSID(TsfProfile::GetTextServiceGuid(), profile.clsid) &&
        ::IsEqualGUID(TsfProfile::GetProfileGuid(), profile.profile_guid)) {
      // This is the full TSF version of Google Japanese Input.
      removed_profiles->push_back(profile);
      continue;
    }

    if (!default_found && profile.is_enabled && profile.is_default) {
      default_found = true;
      default_set = true;
      *new_default = profile;
    }

    if (!default_found && !default_set && profile.is_enabled) {
      default_set = true;
      *new_default = profile;
    }
  }

  if (!default_set) {
    // TODO(yukawa): Consider this case.
    // Use MS-IME as a fallback.
    new_default->langid = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
    new_default->clsid = CLSID_IMJPTIP;
    new_default->profile_guid = GUID_IMJPTIP;
    new_default->klid = 0;
    new_default->ime_filename.clear();
    new_default->is_default = true;
    new_default->is_tip = true;
    new_default->is_enabled = false;
  }

  return true;
}

bool UninstallHelper::GetInstalledProfilesByLanguage(
    LANGID langid,
    vector<LayoutProfileInfo> *installed_profiles) {
  if (installed_profiles == nullptr) {
    return false;
  }
  installed_profiles->clear();

  if (!GetInstalledProfilesByLanguageForTSF(langid, installed_profiles)) {
    // Actually this can fail if user have explicitly unregistered TSF modules
    // like b/2636769.  We do not return false because it would be better to
    // continue with the result of GetInstalledProfilesByLanguageForIMM32.
    LOG(ERROR) << "GetInstalledProfilesByLanguageForTSF failed.";
  }

  if (!GetInstalledProfilesByLanguageForIMM32(langid, installed_profiles)) {
    LOG(ERROR) << "GetInstalledProfilesByLanguageForIMM32 failed.";
    return false;
  }

  return true;
}

bool UninstallHelper::GetKeyboardLayoutsForXP(
    vector<KeyboardLayoutInfo> *preload_layouts,
    vector<KeyboardLayoutInfo> *installed_layouts) {
  if (preload_layouts == nullptr) {
    return false;
  }
  preload_layouts->clear();
  if (installed_layouts == nullptr) {
    return false;
  }
  installed_layouts->clear();
  if (!GenerateKeyboardLayoutList(installed_layouts)) {
    return false;
  }

  map<DWORD, wstring> keyboard_layouts;
  for (size_t i = 0; i < installed_layouts->size(); ++i) {
    const KeyboardLayoutInfo &layout = (*installed_layouts)[i];
    keyboard_layouts[layout.klid] = layout.ime_filename;
  }

  PreloadOrderToKLIDMap preload_map;
  if (!GetPreloadLayoutsMain(&preload_map)) {
    return false;
  }

  for (PreloadOrderToKLIDMap::const_iterator it = preload_map.begin();
       it != preload_map.end(); ++it) {
    KeyboardLayoutInfo info;
    info.klid = it->second;
    if (keyboard_layouts.find(info.klid) != keyboard_layouts.end()) {
      info.ime_filename = keyboard_layouts[info.klid];
    }
    preload_layouts->push_back(info);
  }

  return true;
}

bool UninstallHelper::GetCurrentProfilesForVista(
    vector<LayoutProfileInfo> *current_profiles) {
  if (current_profiles == nullptr) {
    return false;
  }
  current_profiles->clear();

  if (!InputDll::EnsureInitialized()) {
    return false;
  }
  if (InputDll::enum_enabled_layout_or_tip() == nullptr) {
    return false;
  }

  map<DWORD, wstring> keyboard_layouts;
  if (!GenerateKeyboardLayoutMap(&keyboard_layouts)) {
    return false;
  }

  {
    const UINT num_element = InputDll::enum_enabled_layout_or_tip()(
        nullptr, nullptr, nullptr, nullptr, 0);
    unique_ptr<LAYOUTORTIPPROFILE[]> buffer(
        new LAYOUTORTIPPROFILE[num_element]);
    const UINT num_copied = InputDll::enum_enabled_layout_or_tip()(
        nullptr, nullptr, nullptr, buffer.get(), num_element);

    for (size_t i = 0; i < num_copied; ++i) {
      const LAYOUTORTIPPROFILE &src = buffer[i];
      if (src.catid != GUID_TFCAT_TIP_KEYBOARD) {
        continue;
      }

      LayoutProfileInfo profile;
      profile.langid = src.langid;
      profile.is_default = ((src.dwFlags & LOT_DEFAULT) == LOT_DEFAULT);
      profile.is_enabled = !((src.dwFlags & LOT_DISABLED) == LOT_DISABLED);
      profile.clsid = src.clsid;
      profile.profile_guid = src.guidProfile;

      if ((src.dwProfileType & LOTP_INPUTPROCESSOR) == LOTP_INPUTPROCESSOR) {
        profile.is_tip = true;
        current_profiles->push_back(profile);
        continue;
      }

      if ((src.dwProfileType & LOTP_KEYBOARDLAYOUT) == LOTP_KEYBOARDLAYOUT) {
        const wstring id(src.szId);
        // A valid |profile.szId| should consists of language ID (LANGID) and
        // keyboard layout ID (KILD) as follows.
        //  <LangID 1>:<KLID 1>
        //       "0411:E0200411"
        // Check if |id.size()| is expected.
        if (id.size() != 13) {
          continue;
        }
        // Extract KLID.  It should be 8-letter hexadecimal code begins at 6th
        // character in the |id|, that is, |id.substr(5, 8)|.
        const KeyboardLayoutID klid(id.substr(5, 8));
        if (!klid.has_id()) {
          continue;
        }
        profile.klid = klid.id();
        if (keyboard_layouts.find(profile.klid) != keyboard_layouts.end()) {
          profile.ime_filename = keyboard_layouts[profile.klid];
        }
        profile.is_tip = false;
        current_profiles->push_back(profile);
        continue;
      }
    }
  }  // release |buffer|

  return true;
}

bool UninstallHelper::UpdatePreloadLayoutsForXP(
    const vector<KeyboardLayoutInfo> &new_preload_layouts) {
  // First, retrieve existing preload entries.  |current_preload_map|
  // represents the relationship between the value name and KLID as following
  // example.
  //   1: 0xE0200411
  //   2: 0x00000411
  //   3: 0xE0210411
  //   4: 0xE0220411
  PreloadOrderToKLIDMap current_preload_map;
  if (!GetPreloadLayoutsMain(&current_preload_map)) {
    return false;
  }

  if (IsEqualPreload(current_preload_map, new_preload_layouts)) {
    // Already the same.
    return true;
  }

  // Open the preload key for update.
  CRegKey preload_key;
  LONG result = preload_key.Open(
      HKEY_CURRENT_USER, kPreloadKeyName, KEY_READ | KEY_WRITE);
  if (ERROR_SUCCESS != result) {
    return false;
  }

  // Second, delete unnecessary entries from bottom to top.  For example,
  // if |new_preload_layouts| consists of [0xE0210411, 0xE0220411], the
  // following code removes |current_preload_map[4]| and
  // |current_preload_map[3]| in this order.
  bool failed = false;
  for (PreloadOrderToKLIDMap::const_reverse_iterator it =
           current_preload_map.rbegin();
       it != current_preload_map.rend(); ++it) {
    if (it->first > new_preload_layouts.size()) {
      result = preload_key.DeleteValue(utow(it->first).c_str());
      if (result != ERROR_SUCCESS) {
        failed = true;
      }
    }
  }

  // Third, (over)write the new entry from top to down.  Note that
  // the preload value name, which seems to be a sort of index, is
  // 1-origin.
  for (size_t i = 0; i < new_preload_layouts.size(); ++i) {
    const KeyboardLayoutID klid(new_preload_layouts[i].klid);
    const int preload_index = i + 1;  // 1-origin.
    result = preload_key.SetStringValue(
        utow(preload_index).c_str(), klid.ToString().c_str());
    if (result != ERROR_SUCCESS) {
      failed = true;
    }
  }

  return !failed;
}

bool UninstallHelper::RemoveProfilesForVista(
    const vector<LayoutProfileInfo> &profiles_to_be_removed) {
  if (profiles_to_be_removed.size() == 0) {
    // Nothing to do.
    return true;
  }

  if (!InputDll::EnsureInitialized()) {
    return false;
  }
  if (InputDll::install_layout_or_tip_user_reg() == nullptr) {
    return false;
  }

  const wstring &profile_string = ComposeProfileStringForVista(
      profiles_to_be_removed);

  const BOOL result = InputDll::install_layout_or_tip_user_reg()(
      nullptr, nullptr, nullptr, profile_string.c_str(), ILOT_UNINSTALL);

  return result != FALSE;
}

wstring UninstallHelper::ComposeProfileStringForVista(
    const vector<LayoutProfileInfo> &profiles) {
  wstringstream ss;
  for (size_t i = 0; i < profiles.size(); ++i) {
    const LayoutProfileInfo &info = profiles[i];
    if (i != 0) {
      ss << L";";
    }

    const wstring &langid_string = LANGIDToString(info.langid);
    if (langid_string.empty()) {
      continue;
    }

    if (info.is_tip) {
      const wstring &clsid_string = GUIDToString(info.clsid);
      const wstring &guid_string = GUIDToString(info.profile_guid);
      if (clsid_string.empty() || guid_string.empty()) {
        continue;
      }
      ss << langid_string << L":" << clsid_string << guid_string;
    } else {
      const KeyboardLayoutID klid(info.klid);
      ss << langid_string << L":" << klid.ToString();
    }
  }
  return ss.str();
}

bool UninstallHelper::SetDefaultForXP(
    const KeyboardLayoutInfo &layout, bool broadcast_change) {
  EnableAndSetDefaultIfLayoutIsTIP(layout);

  if (broadcast_change) {
    if (!BroadcastNewIME(layout)) {
      DLOG(ERROR) << "BroadcastNewIME failed";
      return false;
    }
  }

  return true;
}

bool UninstallHelper::SetDefaultForVista(
    const LayoutProfileInfo &current_default,
    const LayoutProfileInfo &new_default, bool broadcast_change) {
  if (current_default.is_default && current_default.is_enabled &&
      IsEqualProfile(current_default, new_default)) {
    // |new_default| is alreasy default and enabled.
    return true;
  }

  if (!InputDll::EnsureInitialized()) {
    return false;
  }

  if (InputDll::set_default_layout_or_tip() == nullptr) {
    return false;
  }

  if (!EnableAndBroadcastNewLayout(new_default, broadcast_change)) {
    // We do not return false here because the main task of this function is
    // setting the specified profile to default.
    DLOG(ERROR) << "EnableAndBroadcastNewLayout failed.";
  }

  vector<LayoutProfileInfo> profile_list;
  profile_list.push_back(new_default);
  const wstring profile_string = ComposeProfileStringForVista(profile_list);
  if (profile_string.empty()) {
    return false;
  }

  DWORD flag = 0;
  if (!broadcast_change) {
    // If |broadcast_change| prevent the SetDefaultLayoutOrTip API from
    // disturbing the current session in case the thread is impersonated.
    flag = SDLOT_NOAPPLYTOCURRENTSESSION;
  }

  if (InputDll::set_default_layout_or_tip()(profile_string.c_str(), flag) ==
      FALSE) {
    DLOG(ERROR) << "SetDefaultLayoutOrTip failed";
    return false;
  }

  return true;
}

bool UninstallHelper::RestoreUserIMEEnvironmentForXP(bool broadcast_change) {
  vector<KeyboardLayoutInfo> preload_layouts;
  vector<KeyboardLayoutInfo> installed_layouts;
  if (!GetKeyboardLayoutsForXP(&preload_layouts, &installed_layouts)) {
    return false;
  }

  RemoveHotKeyForXP(installed_layouts);

  vector<KeyboardLayoutInfo> new_preloads;
  if (!GetNewPreloadLayoutsForXP(
           preload_layouts, installed_layouts, &new_preloads)) {
    return false;
  }
  if (new_preloads.size() > 0) {
    // The entry named '1' under the 'Preload' key corresponds to the user's
    // default layout.  This was documented at least Windows 2000 Server and
    // seems to be applicable on later version such as Windows XP.
    //   http://technet.microsoft.com/en-us/library/cc978687.aspx
    // Starting with Vista, there are some documented functions to tweak
    // default keyboard layout or TIP.  See input_dll.h for details.
    const KeyboardLayoutInfo &new_default = new_preloads[0];
    if (!SetDefaultForXP(new_default, broadcast_change)) {
      DLOG(ERROR) << "SetDefaultForXP failed.";
    }
    if (!UpdatePreloadLayoutsForXP(new_preloads)) {
      DLOG(ERROR) << "UpdatePreloadLayoutsForXP failed.";
    }
    if (broadcast_change) {
      // Finally unload unnecessary keyboard layouts.
      UnloadLayoutsForXP(new_preloads);
    }
  }
  return true;
}

bool UninstallHelper::RestoreUserIMEEnvironmentForVista(bool broadcast_change) {
  vector<LayoutProfileInfo> installed_profiles;
  if (!GetInstalledProfilesByLanguage(kLANGJaJP, &installed_profiles)) {
    return false;
  }

  RemoveHotKeyForVista(installed_profiles);

  vector<LayoutProfileInfo> current_profiles;
  if (!GetCurrentProfilesForVista(&current_profiles)) {
    return false;
  }
  LayoutProfileInfo current_default;
  LayoutProfileInfo new_default;
  vector<LayoutProfileInfo> removed_profiles;
  if (!GetNewEnabledProfileForVista(current_profiles, installed_profiles,
                                    &current_default, &new_default,
                                    &removed_profiles)) {
    return false;
  }
  if (!SetDefaultForVista(current_default, new_default, broadcast_change)) {
    DLOG(ERROR) << "SetDefaultForVista failed.";
  }
  if (!RemoveProfilesForVista(removed_profiles)) {
    DLOG(ERROR) << "RemoveProfilesForVista failed.";
  }
  if (broadcast_change) {
    // Unload unnecessary keyboard layouts.
    UnloadProfilesForVista(removed_profiles);
  }
  return true;
}

bool UninstallHelper::RestoreUserIMEEnvironmentMain() {
  // Basically this function is called from the "non-deferred" custom action
  // with "non-elevated" user privileges, which is enough and prefarable to
  // update entries under HKCU.  Assuming the desktop is available,
  // broadcast messages into existing processes so that these processes
  // start to use the new default IME.
  const bool kBroadcastNewIME = true;

  if (SystemUtil::IsVistaOrLater()) {
    return RestoreUserIMEEnvironmentForVista(kBroadcastNewIME);
  } else {
    return RestoreUserIMEEnvironmentForXP(kBroadcastNewIME);
  }
}

bool UninstallHelper::EnsureIMEIsRemovedForCurrentUser(
    bool disable_hkcu_cache) {

  if (disable_hkcu_cache) {
    // For some reason, HKCU in a deferred, non-impersonated custom action is
    // occasionally mapped not to the HKU/.Default but to the active user's
    // profile as if the current thread is impersonated to the active user.
    // To fix up this, we should disable per-process registry cache by calling
    // RegDisablePredefinedCache API.
    const LRESULT result = ::RegDisablePredefinedCache();
    if (result != ERROR_SUCCESS) {
      DLOG(ERROR) << "IsServiceThread failed.  result = " << result;
      return false;
    }
  }

  // Since this function is targeting for the service account, IME change
  // notification will not be sent in case it causes unwilling side-effects
  // against other important processes running in the service session.
  const bool kBroadcastNewIME = false;
  if (SystemUtil::IsVistaOrLater()) {
    return RestoreUserIMEEnvironmentForVista(kBroadcastNewIME);
  } else {
    return RestoreUserIMEEnvironmentForXP(kBroadcastNewIME);
  }
}
}  // namespace win32
}  // namespace mozc
