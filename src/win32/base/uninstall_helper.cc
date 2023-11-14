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

#include "win32/base/uninstall_helper.h"

#include <atlbase.h>
#include <msctf.h>
#include <objbase.h>
#include <wil/com.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/win32/win_util.h"
#include "win32/base/imm_util.h"
#include "win32/base/input_dll.h"
#include "win32/base/keyboard_layout_id.h"
#include "win32/base/tsf_profile.h"

// clang-format off
#include <strsafe.h>  // NOLINT: strsafe.h needs to be the last include.
// clang-format on

namespace mozc {
namespace win32 {
namespace {

using ATL::CRegKey;

typedef std::map<int, DWORD> PreloadOrderToKLIDMap;

// Windows NT 6.0, 6.1 and 6.2
constexpr CLSID CLSID_IMJPTIP = {
    0x03b5835f,
    0xf03c,
    0x411b,
    {0x9c, 0xe2, 0xaa, 0x23, 0xe1, 0x17, 0x1e, 0x36}};
constexpr GUID GUID_IMJPTIP = {
    0xa76c93d9,
    0x5523,
    0x4e90,
    {0xaa, 0xfa, 0x4d, 0xb1, 0x12, 0xf9, 0xac, 0x76}};

constexpr LANGID kLANGJaJP = MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);

constexpr wchar_t kRegKeyboardLayouts[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts";

// Registry element size limits are described in the link below.
// http://msdn.microsoft.com/en-us/library/ms724872(VS.85).aspx
constexpr DWORD kMaxValueNameLength = 16383;

// Timeout value used by a work around against b/5765783.
// Note that the following timeout threshold is not well tested.
// TODO(yukawa): Investigate the best timeout threshold. b/6165722
constexpr uint32_t kWaitForAsmCacheReadyEventTimeout = 10000;  // 10 sec.

std::wstring GetIMEFileNameFromKeyboardLayout(const CRegKey &key,
                                              const KeyboardLayoutID &klid) {
  CRegKey subkey;
  LONG result = subkey.Open(key, klid.ToString().c_str(), KEY_READ);
  if (ERROR_SUCCESS != result) {
    return L"";
  }

  wchar_t filename_buffer[kMaxValueNameLength];
  ULONG filename_length_including_null = kMaxValueNameLength;
  result = subkey.QueryStringValue(L"Ime File", filename_buffer,
                                   &filename_length_including_null);

  // Note that |filename_length_including_null| contains NUL terminator.
  if ((ERROR_SUCCESS != result) || (filename_length_including_null <= 1)) {
    return L"";
  }

  const ULONG filename_length = (filename_length_including_null - 1);
  const std::wstring filename(filename_buffer);

  // Note that |filename_length| does not contain NUL character.
  DCHECK_EQ(filename_length, filename.size());
  return filename;
}

bool GenerateKeyboardLayoutList(
    std::vector<KeyboardLayoutInfo> *keyboard_layouts) {
  if (keyboard_layouts == nullptr) {
    return false;
  }
  keyboard_layouts->clear();

  CRegKey key;
  LONG result = key.Open(HKEY_LOCAL_MACHINE, kRegKeyboardLayouts, KEY_READ);
  if (ERROR_SUCCESS != result) {
    return false;
  }

  wchar_t value_name[kMaxValueNameLength];
  for (DWORD enum_reg_index = 0;; ++enum_reg_index) {
    DWORD value_name_length = kMaxValueNameLength;
    result = key.EnumKey(enum_reg_index, value_name, &value_name_length);
    if (ERROR_NO_MORE_ITEMS == result) {
      return true;
    }
    if (ERROR_SUCCESS != result) {
      return true;
    }

    // Note that |value_name_length| does not contain NUL character.
    const KeyboardLayoutID klid(
        std::wstring(value_name, value_name + value_name_length));

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

bool GenerateKeyboardLayoutMap(
    std::map<DWORD, std::wstring> *keyboard_layouts) {
  if (keyboard_layouts == nullptr) {
    return false;
  }
  std::vector<KeyboardLayoutInfo> keyboard_layout_list;
  if (!GenerateKeyboardLayoutList(&keyboard_layout_list)) {
    return false;
  }

  for (size_t i = 0; i < keyboard_layout_list.size(); ++i) {
    const KeyboardLayoutInfo &layout = keyboard_layout_list[i];
    (*keyboard_layouts)[layout.klid] = layout.ime_filename;
  }

  return true;
}

std::wstring GetIMEFileName(HKL hkl) {
  const UINT num_chars_without_null = ::ImmGetIMEFileName(hkl, nullptr, 0);
  const size_t num_chars_with_null = num_chars_without_null + 1;
  std::unique_ptr<wchar_t[]> buffer(new wchar_t[num_chars_with_null]);
  const UINT num_copied =
      ::ImmGetIMEFileName(hkl, buffer.get(), num_chars_with_null);

  // |num_copied| does not include terminating null character.
  return std::wstring(buffer.get(), buffer.get() + num_copied);
}

bool GetInstalledProfilesByLanguageForTSF(
    LANGID langid, std::vector<LayoutProfileInfo> *installed_profiles) {
  HRESULT hr = S_OK;

  wil::com_ptr_nothrow<ITfInputProcessorProfiles> profiles;
  hr = TF_CreateInputProcessorProfiles(profiles.put());
  if (!profiles) {
    return false;
  }

  wil::com_ptr_nothrow<IEnumTfLanguageProfiles> enum_profiles;
  hr = profiles->EnumLanguageProfiles(langid, enum_profiles.put());
  if (FAILED(hr)) {
    return false;
  }

  while (true) {
    ULONG num_fetched = 0;
    TF_LANGUAGEPROFILE src;
    ZeroMemory(&src, sizeof(src));
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
    hr = profiles->IsEnabledLanguageProfile(src.clsid, langid, src.guidProfile,
                                            &enabled);
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
    LANGID langid, std::vector<LayoutProfileInfo> *installed_profiles) {
  std::vector<KeyboardLayoutInfo> keyboard_layouts;
  if (!GenerateKeyboardLayoutList(&keyboard_layouts)) {
    DLOG(ERROR) << "GenerateKeyboardLayoutList failed.";
    return false;
  }

  for (const auto &info : keyboard_layouts) {
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

std::wstring GUIDToString(const GUID &guid) {
  wchar_t buffer[256];
  const int character_length_with_null =
      ::StringFromGUID2(guid, buffer, std::size(buffer));
  if (character_length_with_null <= 0) {
    return L"";
  }

  const size_t character_length_without_null = character_length_with_null - 1;
  return std::wstring(buffer, buffer + character_length_without_null);
}

std::wstring LANGIDToString(LANGID langid) {
  wchar_t buffer[5];
  HRESULT hr = ::StringCchPrintf(buffer, std::size(buffer), L"%04x", langid);
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
  if (::SystemParametersInfo(SPI_SETDEFAULTINPUTLANG, 0, &hkl,
                             SPIF_SENDCHANGE) == FALSE) {
    LOG(ERROR) << "SystemParameterInfo failed: " << GetLastError();
    return false;
  }

  // A work around against b/5765783.
  if (!ImeUtil::WaitForAsmCacheReady(kWaitForAsmCacheReadyEventTimeout)) {
    DLOG(ERROR) << "ImeUtil::WaitForAsmCacheReady failed.";
  }

  // Broadcasting WM_INPUTLANGCHANGEREQUEST so that existing process in the
  // current session will change their input method to |hkl|. This mechanism
  // also works against a HKL which is substituted by a TIP on Windows XP.
  // Note: we have virtually the same code in imm_util.cc too.
  // TODO(yukawa): Make a common function around WM_INPUTLANGCHANGEREQUEST.
  DWORD recipients = BSM_APPLICATIONS;
  const LONG result = ::BroadcastSystemMessage(
      BSF_POSTMESSAGE, &recipients, WM_INPUTLANGCHANGEREQUEST,
      INPUTLANGCHANGE_SYSCHARSET, reinterpret_cast<LPARAM>(hkl));
  if (result == 0) {
    const int error = ::GetLastError();
    LOG(ERROR) << "BroadcastSystemMessage failed. error = " << error;
    return false;
  }

  return true;
}

bool EnableAndBroadcastNewLayout(const LayoutProfileInfo &profile,
                                 bool broadcast_change) {
  if (profile.is_tip) {
    wil::com_ptr_nothrow<ITfInputProcessorProfileMgr> profile_manager;
    {
      wil::com_ptr_nothrow<ITfInputProcessorProfiles> profiles;
      TF_CreateInputProcessorProfiles(profiles.put());
      if (!profiles) {
        return false;
      }
      profiles.query_to(&profile_manager);
    }
    if (!profile_manager) {
      return false;
    }

    DWORD activate_flags =
        TF_IPPMF_ENABLEPROFILE | TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE;

    if (broadcast_change) {
      activate_flags |= TF_IPPMF_FORSESSION;
    }
    HRESULT hr = profile_manager->ActivateProfile(
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

bool GetActiveKeyboardLayouts(std::vector<HKL> *keyboard_layouts) {
  if (keyboard_layouts == nullptr) {
    return false;
  }
  keyboard_layouts->clear();

  const int num_keyboard_layout = ::GetKeyboardLayoutList(0, nullptr);
  std::unique_ptr<HKL[]> buffer(new HKL[num_keyboard_layout]);
  const int num_copied =
      ::GetKeyboardLayoutList(num_keyboard_layout, buffer.get());
  keyboard_layouts->assign(buffer.get(), buffer.get() + num_copied);

  return true;
}

// This function lists all the active keyboard layouts up and unloads each
// layout based on the specified condition.  If |exclude| is true, this
// function unloads any active IME if it is included in |ime_filenames|.
// If |exclude| is false, this function unloads any active IME unless it is
// included in |ime_filenames|.
void UnloadActivatedKeyboardMain(const std::vector<std::wstring> &ime_filenames,
                                 bool exclude) {
  std::vector<HKL> loaded_layouts;
  if (!GetActiveKeyboardLayouts(&loaded_layouts)) {
    return;
  }
  for (size_t i = 0; i < loaded_layouts.size(); ++i) {
    const HKL hkl = loaded_layouts[i];
    const std::wstring ime_filename = GetIMEFileName(hkl);
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

void UnloadProfilesForVista(
    const std::vector<LayoutProfileInfo> &profiles_to_be_removed) {
  std::vector<std::wstring> ime_filenames;
  ime_filenames.reserve(profiles_to_be_removed.size());
  for (const auto &profile : profiles_to_be_removed) {
    ime_filenames.push_back(profile.ime_filename);
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
  if (!WinUtil::SystemEqualString(lhs.ime_filename, rhs.ime_filename, true)) {
    return false;
  }
  return true;
}

bool IsEqualPreload(
    const PreloadOrderToKLIDMap &current_preload_map,
    const std::vector<KeyboardLayoutInfo> &new_preload_layouts) {
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

}  // namespace

KeyboardLayoutInfo::KeyboardLayoutInfo() : klid(0) {}

LayoutProfileInfo::LayoutProfileInfo()
    : langid(0),
      clsid(CLSID_NULL),
      profile_guid(GUID_NULL),
      klid(0),
      is_default(false),
      is_tip(false),
      is_enabled(false) {}

// Currently this function is Mozc-specific.
// TODO(yukawa): Generalize this function for any IME and/or TIP.
bool UninstallHelper::GetNewEnabledProfileForVista(
    const std::vector<LayoutProfileInfo> &current_profiles,
    const std::vector<LayoutProfileInfo> &installed_profiles,
    LayoutProfileInfo *current_default, LayoutProfileInfo *new_default,
    std::vector<LayoutProfileInfo> *removed_profiles) {
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
  for (const auto &profile : current_profiles) {
    if (profile.is_default) {
      *current_default = profile;
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
    LANGID langid, std::vector<LayoutProfileInfo> *installed_profiles) {
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

bool UninstallHelper::GetCurrentProfilesForVista(
    std::vector<LayoutProfileInfo> *current_profiles) {
  if (current_profiles == nullptr) {
    return false;
  }
  current_profiles->clear();

  std::map<DWORD, std::wstring> keyboard_layouts;
  if (!GenerateKeyboardLayoutMap(&keyboard_layouts)) {
    return false;
  }

  {
    const UINT num_element =
        ::EnumEnabledLayoutOrTip(nullptr, nullptr, nullptr, nullptr, 0);
    std::unique_ptr<LAYOUTORTIPPROFILE[]> buffer(
        new LAYOUTORTIPPROFILE[num_element]);
    const UINT num_copied = ::EnumEnabledLayoutOrTip(nullptr, nullptr, nullptr,
                                                     buffer.get(), num_element);

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
        const std::wstring id(src.szId);
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

bool UninstallHelper::RemoveProfilesForVista(
    const std::vector<LayoutProfileInfo> &profiles_to_be_removed) {
  if (profiles_to_be_removed.empty()) {
    // Nothing to do.
    return true;
  }

  const std::wstring &profile_string =
      ComposeProfileStringForVista(profiles_to_be_removed);

  const BOOL result = ::InstallLayoutOrTipUserReg(
      nullptr, nullptr, nullptr, profile_string.c_str(), ILOT_UNINSTALL);

  return result != FALSE;
}

std::wstring UninstallHelper::ComposeProfileStringForVista(
    const std::vector<LayoutProfileInfo> &profiles) {
  std::wstringstream ss;
  for (size_t i = 0; i < profiles.size(); ++i) {
    const LayoutProfileInfo &info = profiles[i];
    if (i != 0) {
      ss << L";";
    }

    const std::wstring &langid_string = LANGIDToString(info.langid);
    if (langid_string.empty()) {
      continue;
    }

    if (info.is_tip) {
      const std::wstring &clsid_string = GUIDToString(info.clsid);
      const std::wstring &guid_string = GUIDToString(info.profile_guid);
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

bool UninstallHelper::SetDefaultForVista(
    const LayoutProfileInfo &current_default,
    const LayoutProfileInfo &new_default, bool broadcast_change) {
  if (current_default.is_default && current_default.is_enabled &&
      IsEqualProfile(current_default, new_default)) {
    // |new_default| is alreasy default and enabled.
    return true;
  }

  if (!EnableAndBroadcastNewLayout(new_default, broadcast_change)) {
    // We do not return false here because the main task of this function is
    // setting the specified profile to default.
    DLOG(ERROR) << "EnableAndBroadcastNewLayout failed.";
  }

  std::vector<LayoutProfileInfo> profile_list;
  profile_list.push_back(new_default);
  const std::wstring profile_string =
      ComposeProfileStringForVista(profile_list);
  if (profile_string.empty()) {
    return false;
  }

  DWORD flag = 0;
  if (!broadcast_change) {
    // If |broadcast_change| prevent the SetDefaultLayoutOrTip API from
    // disturbing the current session in case the thread is impersonated.
    flag = SDLOT_NOAPPLYTOCURRENTSESSION;
  }

  if (!::SetDefaultLayoutOrTip(profile_string.c_str(), flag)) {
    DLOG(ERROR) << "SetDefaultLayoutOrTip failed";
    return false;
  }

  return true;
}

bool UninstallHelper::RestoreUserIMEEnvironmentForVista(bool broadcast_change) {
  std::vector<LayoutProfileInfo> installed_profiles;
  if (!GetInstalledProfilesByLanguage(kLANGJaJP, &installed_profiles)) {
    return false;
  }

  std::vector<LayoutProfileInfo> current_profiles;
  if (!GetCurrentProfilesForVista(&current_profiles)) {
    return false;
  }
  LayoutProfileInfo current_default;
  LayoutProfileInfo new_default;
  std::vector<LayoutProfileInfo> removed_profiles;
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
  constexpr bool kBroadcastNewIME = true;

  return RestoreUserIMEEnvironmentForVista(kBroadcastNewIME);
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
  constexpr bool kBroadcastNewIME = false;
  return RestoreUserIMEEnvironmentForVista(kBroadcastNewIME);
}

}  // namespace win32
}  // namespace mozc
