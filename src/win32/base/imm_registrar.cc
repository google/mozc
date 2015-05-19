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


#include "win32/base/imm_registrar.h"

#include <windows.h>
#include <WinNls32.h>
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <strsafe.h>

#include <iomanip>
#include <map>
#include <sstream>
#include <string>

#include "base/const.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/win_util.h"
#include "win32/base/display_name_resource.h"
#include "win32/base/imm_util.h"
#include "win32/base/immdev.h"
#include "win32/base/keyboard_layout_id.h"

namespace mozc {
namespace win32 {

namespace {
const wchar_t kRegKeyboardLayouts[] =
    L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts";
const wchar_t kLayoutDisplayNameKey[] = L"Layout Display Name";
const wchar_t kLayoutDisplayNamePattern[] = L"@%s,-%d";
const wchar_t kPreloadKeyName[] = L"Keyboard Layout\\Preload";
const wchar_t kPreloadTopValueName[] = L"1";

typedef map<unsigned int, DWORD> PreloadValueMap;

// Converts an unsigned integer to a wide string.
wstring utow(unsigned int i) {
  wstringstream ss;
  ss << i;
  return ss.str();
}

wstring GetSystemRegKeyName(const KeyboardLayoutID &klid) {
  return wstring(kRegKeyboardLayouts) + L"\\" + klid.ToString();
}

// Set the layout display name with the Registry String Redirection format
// to the specified keyboard layout.
// See the following documents for details.
// http://blogs.msdn.com/michkap/archive/2006/05/06/591174.aspx
// http://blogs.msdn.com/michkap/archive/2007/01/05/1387397.aspx
// http://blogs.msdn.com/michkap/archive/2007/08/25/4564548.aspx
// http://msdn.microsoft.com/en-us/library/dd374120.aspx
HRESULT SetLayoutDisplayName(const KeyboardLayoutID &klid,
                             const wstring &layout_display_name_resource_path,
                             int layout_display_name_resource_id) {
  if (!klid.has_id()) {
    return E_FAIL;
  }

  const wstring &key_name = GetSystemRegKeyName(klid);

  CRegKey keybord_layout_key;
  LRESULT result = keybord_layout_key.Open(
      HKEY_LOCAL_MACHINE, key_name.c_str(), KEY_READ | KEY_WRITE);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to open the registry key"
               << " result = " << result;
    return HRESULT_FROM_WIN32(result);
  }

  wchar_t layout_name[MAX_PATH];
  HRESULT hr = StringCchPrintf(layout_name, arraysize(layout_name),
                               kLayoutDisplayNamePattern,
                               layout_display_name_resource_path.c_str(),
                               layout_display_name_resource_id);
  if (FAILED(hr)) {
    LOG(ERROR) << "StringCchPrintf failed"
               << " hr = " << hr;
    return hr;
  }

  result = keybord_layout_key.SetStringValue(&kLayoutDisplayNameKey[0],
                                             layout_name, REG_SZ);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set a registry value"
               << " result = " << result;
    return HRESULT_FROM_WIN32(result);
  }

  return S_OK;
}

// ImmInstallIME has a bug in 64-bit Windows. It can't recoganize SysWOW64
// folder as a system folder, so it will refuse to install our IME. The
// solution here is to combine the 64-bit System32 folder and our filename
// to make ImmInstallIME happy.
wstring GetFullPathForSystem(const string& basename) {
  string system_dir;
  if (Util::WideToUTF8(SystemUtil::GetSystemDir(), &system_dir) <= 0) {
    return L"";
  }

  const string fullpath = FileUtil::JoinPath(system_dir, basename);

  wstring wfullpath;
  if (Util::UTF8ToWide(fullpath.c_str(), &wfullpath) <= 0) {
    return L"";
  }

  return wfullpath;
}

// Retrieves values under the preload key and stores the result to |keys|.
// Returns ERROR_SUCCESS if the operation completes successfully.
LONG RetrievePreloadValues(HKEY preload_key,
                           PreloadValueMap *keys) {
  if (nullptr == keys) {
    return ERROR_INVALID_PARAMETER;
  }

  // Registry element size limits are described in the link below.
  // http://msdn.microsoft.com/en-us/library/ms724872(VS.85).aspx
  const DWORD kMaxValueNameLength = 16383;
  wchar_t value_name[kMaxValueNameLength];
  const DWORD kMaxValueLength = 256;
  BYTE value[kMaxValueLength];
  for (DWORD i = 0;; ++i) {
    DWORD value_name_length = kMaxValueNameLength;
    DWORD value_length = kMaxValueLength;
    LONG result = RegEnumValue(preload_key,
                               i,
                               value_name,
                               &value_name_length,
                               nullptr,  // reserved (must be NULL)
                               nullptr,  // type (optional)
                               value,
                               &value_length);

    if (ERROR_NO_MORE_ITEMS == result) {
      break;
    } else if (ERROR_SUCCESS != result) {
      return result;
    }

    const int ivalue_name = _wtoi(value_name);
    const wstring wvalue(reinterpret_cast<wchar_t*>(value),
                         (value_length / sizeof(wchar_t)) - 1);
    KeyboardLayoutID klid(wvalue);
    if (!klid.has_id()) {
      continue;
    }
    (*keys)[ivalue_name] = klid.id();
  }
  return ERROR_SUCCESS;
}

// Returns the index of |klid| in |preload_values|.
// This function works well on 64-bit Windows.
unsigned int GetPreloadIndex(const KeyboardLayoutID &klid,
                             const PreloadValueMap &preload_values) {
  unsigned int index = 0;
  for (PreloadValueMap::const_iterator i = preload_values.begin();
       i != preload_values.end(); ++i) {
    if ((*i).second == klid.id()) {
      index = (*i).first;
    }
  }
  return index;
}

wstring ToWideString(const string &str) {
  wstring wide;
  if (Util::UTF8ToWide(str.c_str(), &wide) <= 0) {
    return L"";
  }
  return wide;
}

bool RemoveHotKey(HKL hkl) {
  if (hkl == nullptr) {
    return false;
  }

  bool succeeded = true;
  for (DWORD id = IME_HOTKEY_DSWITCH_FIRST; id <= IME_HOTKEY_DSWITCH_LAST;
       ++id) {
    UINT modifiers = 0;
    UINT virtual_key = 0;
    HKL assigned_hkl = nullptr;
    BOOL result = ::ImmGetHotKey(id, &modifiers, &virtual_key, &assigned_hkl);
    if (result == FALSE) {
      continue;
    }
    if (assigned_hkl != hkl) {
      continue;
    }
    // ImmSetHotKey fails when both 2nd and 3rd arguments are valid while 4th
    // argument is NULL.  To remove the HotKey, pass 0 to them.
    result = ::ImmSetHotKey(id, 0, 0, nullptr);
    if (result == FALSE) {
      succeeded = false;
    }
  }
  return succeeded;
}
}  // anonymous namespace

HRESULT ImmRegistrar::Register(const wstring &ime_filename,
                               const wstring &layout_name,
                               const wstring &layout_display_name_resource_path,
                               int layout_display_name_resource_id,
                               HKL* hkl) {
  HKL dummy_hkl = nullptr;
  if (hkl == nullptr) {
    hkl = &dummy_hkl;
  }

  // If the IME is already registered, return directly. If we install 32-bit
  // and 64-bit IME side-by-side in a 64-bit Windows, the ImmInstallIME
  // function should be called only once, either for the 32-bit or 64-bit
  // version of the IME DLL.
  {
    const KeyboardLayoutID klid = GetKLIDFromFileName(ime_filename);
    if (klid.has_id()) {
      // already registered.
      *hkl = ::LoadKeyboardLayoutW(klid.ToString().c_str(), KLF_ACTIVATE);
      return S_OK;
    }
  }  // |klid| is no longer needed.

  IMEPROW dummy_ime_property = { 0 };

  const wstring &fullpath(
      wstring(SystemUtil::GetSystemDir()) + L"\\" + ime_filename);

  // The path name of IME has hard limit. (http://b/2072809)
  if (fullpath.size() + 1 > arraysize(dummy_ime_property.szName)) {
    // Path name is too long. It will be truncated.
    return E_FAIL;
  }

  // The description of IME has hard limit. (http://b/2072809)
  if (layout_name.size() + 1 > arraysize(dummy_ime_property.szDescription)) {
    // Description is too long. It will be truncated.
    return E_FAIL;
  }

  // On 64-bit Windows, it would better to use native (64-bit) version of
  // ImmInstallIME instead of WOW (32-bit) version.  See b/2931871 for details.
  const HKL installed_hkl =
      ::ImmInstallIME(fullpath.c_str(), layout_name.c_str());

  if (!installed_hkl) {
    return E_FAIL;
  }

  // Remove HotKey (if any), which is likely to be an unregistered HotKey
  // used for the previous IME.
  if (!RemoveHotKey(installed_hkl)) {
    DLOG(ERROR) << "RemoveUnregisteredHotKey failed.";
    // Removing the hotkey is an optional, nice-to-have task so its failure
    // is not critical.  Go to the next step.
  }

  *hkl = installed_hkl;

  const KeyboardLayoutID installed_klid = GetKLIDFromFileName(ime_filename);
  if (!installed_klid.has_id()) {
    // ImmInstallIME returned a HKL but KLID is not found.  Something wrong.
    return E_FAIL;
  }

  // SetLayoutDisplayName is not mandatory so that we do nothing if it fails.
  if (FAILED(SetLayoutDisplayName(installed_klid,
                                  layout_display_name_resource_path,
                                  layout_display_name_resource_id))) {
    DLOG(ERROR) << "SetLayoutDisplayName failed.";
  }

  return S_OK;
}

// Uninstall module by deleting a registry key under kRegKeyboardLayouts.
HRESULT ImmRegistrar::Unregister(const wstring &ime_filename) {
  const KeyboardLayoutID &klid = GetKLIDFromFileName(ime_filename);
  if (!klid.has_id()) {
    // already unregistered?
    return S_OK;
  }

  // Ensure the target IME is unloaded.
  {
    const int num_keyboard_layout = ::GetKeyboardLayoutList(0, nullptr);
    scoped_array<HKL> keyboard_layouts(new HKL[num_keyboard_layout]);
    const size_t num_copied = ::GetKeyboardLayoutList(num_keyboard_layout,
                                                      keyboard_layouts.get());
    for (size_t i = 0; i < num_copied; ++i) {
      const HKL hkl = keyboard_layouts[i];
      if (!ImmRegistrar::IsIME(hkl, ime_filename)) {
        continue;
      }
      ::UnloadKeyboardLayout(hkl);
      break;
    }
  }

  // Remove IME registry key.
  {
    CRegKey keyboard_layouts;
    LONG result = keyboard_layouts.Open(
        HKEY_LOCAL_MACHINE, kRegKeyboardLayouts, KEY_READ | KEY_WRITE);
    if (ERROR_SUCCESS != result) {
      return HRESULT_FROM_WIN32(result);
    }
    result = keyboard_layouts.RecurseDeleteKey(klid.ToString().c_str());
    if (ERROR_SUCCESS != result) {
      return HRESULT_FROM_WIN32(result);
    }
  }

  return S_OK;
}

bool ImmRegistrar::IsIME(HKL hkl, const wstring &ime_filename) {
  if (hkl == nullptr) {
    return false;
  }

  wchar_t buf[MAX_PATH];
  if (!::ImmGetIMEFileNameW(hkl, buf, MAX_PATH)) {
    LOG(WARNING) << "Failed to get ime file name";
    return false;
  }

  // TODO(yukawa): Support short filename.  See b/2977730
  return WinUtil::SystemEqualString(buf, ime_filename, true);
}

wstring ImmRegistrar::GetFileNameForIME() {
  return ToWideString(mozc::kIMEFile);
}

KeyboardLayoutID ImmRegistrar::GetKLIDForIME() {
  return GetKLIDFromFileName(GetFileNameForIME());
}

KeyboardLayoutID ImmRegistrar::GetKLIDFromFileName(
    const wstring &ime_filename) {
  if (ime_filename.empty()) {
    return KeyboardLayoutID();
  }

  CRegKey keyboard_layouts;
  LONG result = keyboard_layouts.Open(
      HKEY_LOCAL_MACHINE, kRegKeyboardLayouts, KEY_READ);
  if (ERROR_SUCCESS != result) {
    return KeyboardLayoutID();
  }

  // Registry element size limits are described in the link below.
  // http://msdn.microsoft.com/en-us/library/ms724872(VS.85).aspx
  const DWORD kMaxValueNameLength = 16383;
  wchar_t value_name[kMaxValueNameLength];
  for (DWORD enum_reg_index = 0;; ++enum_reg_index) {
    DWORD value_name_length = kMaxValueNameLength;
    result = keyboard_layouts.EnumKey(
        enum_reg_index,
        value_name,
        &value_name_length);
    if (ERROR_NO_MORE_ITEMS == result) {
      break;
    } else if (ERROR_SUCCESS != result) {
      break;
    }

    // Note that |value_name_length| does not contain NUL character.
    const KeyboardLayoutID klid(
        wstring(value_name, value_name + value_name_length));

    if (!klid.has_id()) {
      continue;
    }

    CRegKey subkey;
    result = subkey.Open(keyboard_layouts, klid.ToString().c_str(), KEY_READ);
    if (ERROR_SUCCESS != result) {
      continue;
    }

    wchar_t filename_buffer[kMaxValueNameLength];
    ULONG filename_length_including_null = kMaxValueNameLength;
    result = subkey.QueryStringValue(
        L"Ime File", filename_buffer, &filename_length_including_null);

    // Note that |filename_length_including_null| contains NUL terminator.
    if (ERROR_SUCCESS != result || (filename_length_including_null == 0)) {
      continue;
    }

    const ULONG filename_length = (filename_length_including_null - 1);
    // Note that |filename_length| does not contain NUL character.
    const wstring target_basename(
        filename_buffer, filename_buffer + filename_length);

    // TODO(yukawa): Support short filename.  See b/2977730
    if (WinUtil::SystemEqualString(target_basename, ime_filename, true)) {
      return klid;
    }
  }
  return KeyboardLayoutID();
}

wstring ImmRegistrar::GetFullPathForIME() {
  return GetFullPathForSystem(mozc::kIMEFile);
}

wstring ImmRegistrar::GetLayoutName() {
  wstring layout_name;
  // We use English name here as culture-invariant layout name.
  if (Util::UTF8ToWide(kProductNameInEnglish, &layout_name) <= 0) {
    return L"";
  }
  return layout_name;
}

int ImmRegistrar::GetLayoutDisplayNameResourceId() {
  return IDS_IME_DISPLAYNAME;
}

// Removes a value equal to hkl from HKCU\Keyboard Layout\\Preload and decrement
// value names which are more than hkl.
HRESULT ImmRegistrar::RemoveKeyFromPreload(
    const KeyboardLayoutID &klid, const KeyboardLayoutID &defaultKLID) {
  // Retrieve keys under kPreloadKeyName.
  CRegKey preload_key;
  LONG result = preload_key.Open(HKEY_CURRENT_USER, kPreloadKeyName);
  if (ERROR_SUCCESS != result) {
    return E_FAIL;
  }

  PreloadValueMap preload_values;
  result = RetrievePreloadValues(preload_key, &preload_values);
  if (ERROR_SUCCESS != result) {
    return E_FAIL;
  }

  const unsigned int preload_index = GetPreloadIndex(klid, preload_values);
  if (0 == preload_index) {
    // Not found.  Already removed?
    return S_OK;
  }

  // Write the default HKL if the deleted value was the last one.
  if (preload_values.size() == 1) {
    _ASSERT((*preload_values.begin()).first == preload_index);
    result = preload_key.SetStringValue(kPreloadTopValueName,
                                        defaultKLID.ToString().c_str());
    if (ERROR_SUCCESS != result) {
      return E_FAIL;
    }
  } else {
    // Remove values whose names are less than |preload_index|.
    PreloadValueMap::iterator target_iter =
        preload_values.find(preload_index);
    preload_values.erase(preload_values.begin(), target_iter);
    for (PreloadValueMap::iterator i = preload_values.begin();
         i != preload_values.end();
         ++i) {
      const wstring& value_name = utow((*i).first);
      const KeyboardLayoutID target_klid((*i).second);
      result = preload_key.DeleteValue(value_name.c_str());
      if ((*i).first == preload_index) {
        continue;
      }
      const wstring& new_value_name = utow((*i).first - 1);
      preload_key.SetStringValue(new_value_name.c_str(),
                                 target_klid.ToString().c_str());
    }
  }
  return S_OK;
}

HRESULT ImmRegistrar::RestorePreload(const KeyboardLayoutID &klid) {
  if (!klid.has_id()) {
    return E_FAIL;
  }

  CRegKey preload_key;
  LONG result = preload_key.Open(HKEY_CURRENT_USER, kPreloadKeyName);
  if (ERROR_SUCCESS != result) {
    return E_FAIL;
  }

  PreloadValueMap preload_values;
  result = RetrievePreloadValues(preload_key, &preload_values);
  if (ERROR_SUCCESS != result ||
      preload_values.find(1) == preload_values.end()) {
    // The value corresponding to |hkl| or the value on the top does not exist.
    return E_FAIL;
  }

  const unsigned int preload_index = GetPreloadIndex(klid, preload_values);
  if (0 != preload_index) {
    // |klid| already exists in the preload list.
    // nothing to do.
    return S_OK;
  }

  if (preload_values.size() == 0) {
    result = preload_key.SetStringValue(
        kPreloadTopValueName, klid.ToString().c_str());
    if (ERROR_SUCCESS != result) {
      return E_FAIL;
    }
    return S_OK;
  }

  const unsigned int new_preload_index = (preload_values.rbegin()->first + 1);
  result = preload_key.SetStringValue(utow(new_preload_index).c_str(),
                                      klid.ToString().c_str());
  if (ERROR_SUCCESS != result) {
    return E_FAIL;
  }
  return S_OK;
}

// NOTE: There are several ways to reorder the values other than |klid| after
// |klid| is moved to the top.
// The current implementation just swaps the order of |klid| for the top.
HRESULT ImmRegistrar::MovePreloadValueToTop(const KeyboardLayoutID &klid) {
  CRegKey preload_key;
  LONG result = preload_key.Open(HKEY_CURRENT_USER, kPreloadKeyName);
  if (ERROR_SUCCESS != result) {
    return E_FAIL;
  }

  PreloadValueMap preload_values;
  result = RetrievePreloadValues(preload_key, &preload_values);
  if (ERROR_SUCCESS != result ||
      preload_values.find(1) == preload_values.end()) {
    // The value corresponding to |klid| or the value on the top does not exist.
    return E_FAIL;
  }

  const unsigned int preload_index = GetPreloadIndex(klid, preload_values);
  if (0 == preload_index) {
    if (preload_values.size() == 0) {
      // It is not necessary to move values since there is no preload key.
      result = preload_key.SetStringValue(
          kPreloadTopValueName, klid.ToString().c_str());
      if (ERROR_SUCCESS != result) {
        return E_FAIL;
      }
      return S_OK;
    }
    // Duplicate the first entry into the end of the list.
    unsigned int new_preload_index = (preload_values.rbegin()->first + 1);
    result = preload_key.SetStringValue(
        utow(new_preload_index).c_str(),
        KeyboardLayoutID(preload_values[1]).ToString().c_str());
    if (ERROR_SUCCESS != result) {
      return E_FAIL;
    }
    // Overwrite the first entry with the target keyboard layout ID.
    result = preload_key.SetStringValue(kPreloadTopValueName,
                                        klid.ToString().c_str());
    if (ERROR_SUCCESS != result) {
      // Attempt rollback when the second call fails.
      preload_key.DeleteValue(utow(new_preload_index).c_str());
      return E_FAIL;
    }
    return S_OK;
  }

  // It is not necessary to move values since the target hkl is already listed
  // on the top.
  if (1 == preload_index) {
    return S_OK;
  }
  result = preload_key.SetStringValue(
      kPreloadTopValueName,
      KeyboardLayoutID(preload_values[preload_index]).ToString().c_str());
  if (ERROR_SUCCESS != result) {
    return E_FAIL;
  }
  const KeyboardLayoutID first_klid(preload_values[1]);
  result = preload_key.SetStringValue(utow(preload_index).c_str(),
                                      first_klid.ToString().c_str());
  if (ERROR_SUCCESS != result) {
    // Attempt rollback when the second call fails.
    preload_key.SetStringValue(
        kPreloadTopValueName, first_klid.ToString().c_str());
    return E_FAIL;
  }

  return S_OK;
}

}  // namespace win32
}  // namespace mozc
