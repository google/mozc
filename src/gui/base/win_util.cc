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

#include "gui/base/win_util.h"

#include <cstdint>

#ifdef OS_WIN
// clang-format off
#include <windows.h>
#include <atlbase.h>
#include <atlcom.h>
#include <atlstr.h>
#include <atlwin.h>

#include <knownfolders.h>
#include <objectarray.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlobj.h>
#include <shobjidl.h>
// clang-format on
#endif  // OS_WIN

#ifdef OS_WIN
#include "base/const.h"
#endif  // OS_WIN
#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"

namespace mozc {
namespace gui {

#ifdef OS_WIN
namespace {

CComPtr<IShellLink> InitializeShellLinkItem(const char *argument,
                                            const char *item_title) {
  HRESULT hr = S_OK;
  CComPtr<IShellLink> link;
  hr = link.CoCreateInstance(CLSID_ShellLink);
  if (FAILED(hr)) {
    DLOG(INFO) << "Failed to instantiate CLSID_ShellLink. hr = " << hr;
    return nullptr;
  }

  {
    std::wstring mozc_tool_path_wide;
    Util::Utf8ToWide(SystemUtil::GetToolPath(), &mozc_tool_path_wide);
    hr = link->SetPath(mozc_tool_path_wide.c_str());
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetPath failed. hr = " << hr;
      return nullptr;
    }
  }

  {
    std::wstring argument_wide;
    Util::Utf8ToWide(argument, &argument_wide);
    hr = link->SetArguments(argument_wide.c_str());
    if (FAILED(hr)) {
      DLOG(ERROR) << "SetArguments failed. hr = " << hr;
      return nullptr;
    }
  }

  CComQIPtr<IPropertyStore> property_store(link);
  if (property_store == nullptr) {
    DLOG(ERROR) << "QueryInterface failed.";
    return nullptr;
  }

  {
    std::wstring item_title_wide;
    Util::Utf8ToWide(item_title, &item_title_wide);
    PROPVARIANT prop_variant;
    hr = ::InitPropVariantFromString(item_title_wide.c_str(), &prop_variant);
    if (FAILED(hr)) {
      DLOG(ERROR) << "QueryInterface failed. hr = " << hr;
      return nullptr;
    }
    hr = property_store->SetValue(PKEY_Title, prop_variant);
    ::PropVariantClear(&prop_variant);
  }

  if (FAILED(hr)) {
    DLOG(ERROR) << "SetValue failed. hr = " << hr;
    return nullptr;
  }

  hr = property_store->Commit();
  if (FAILED(hr)) {
    DLOG(ERROR) << "Commit failed. hr = " << hr;
    return nullptr;
  }

  return link;
}

struct LinkInfo {
  const char *argument;
  const char *title_english;
  const char *title_japanese;
};

bool AddTasksToList(CComPtr<ICustomDestinationList> destination_list) {
  HRESULT hr = S_OK;
  CComPtr<IObjectCollection> object_collection;

  hr = object_collection.CoCreateInstance(CLSID_EnumerableObjectCollection);
  if (FAILED(hr)) {
    DLOG(INFO) << "Failed to instantiate CLSID_EnumerableObjectCollection."
                  " hr = "
               << hr;
    return false;
  }

  // TODO(yukawa): Investigate better way to localize strings.
  const LinkInfo kLinks[] = {
      {"--mode=dictionary_tool", "Dictionary Tool", "辞書ツール"},
      {"--mode=word_register_dialog", "Add Word", "単語登録"},
      {"--mode=config_dialog", "Properties", "プロパティ"},
  };

  const LANGID kJapaneseLangId =
      MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
  const bool use_japanese_ui =
      (kJapaneseLangId == ::GetUserDefaultUILanguage());

  for (size_t i = 0; i < std::size(kLinks); ++i) {
    CComPtr<IShellLink> link;
    if (use_japanese_ui) {
      link =
          InitializeShellLinkItem(kLinks[i].argument, kLinks[i].title_japanese);
    } else {
      link =
          InitializeShellLinkItem(kLinks[i].argument, kLinks[i].title_english);
    }
    if (link != nullptr) {
      object_collection->AddObject(link);
    }
  }

  CComQIPtr<IObjectArray> object_array(object_collection);
  if (object_array == nullptr) {
    DLOG(ERROR) << "QueryInterface failed.";
    return false;
  }

  hr = destination_list->AddUserTasks(object_array);
  if (FAILED(hr)) {
    DLOG(ERROR) << "AddUserTasks failed. hr = " << hr;
    return false;
  }

  return true;
}

void InitializeJumpList() {
  HRESULT hr = S_OK;

  CComPtr<ICustomDestinationList> destination_list;
  hr = destination_list.CoCreateInstance(CLSID_DestinationList);
  if (FAILED(hr)) {
    DLOG(INFO) << "Failed to instantiate CLSID_DestinationList. hr = " << hr;
    return;
  }

  UINT min_slots = 0;
  CComPtr<IObjectArray> removed_objects;
  hr = destination_list->BeginList(&min_slots, IID_IObjectArray,
                                   reinterpret_cast<void **>(&removed_objects));
  if (FAILED(hr)) {
    DLOG(INFO) << "BeginList failed. hr = " << hr;
    return;
  }

  if (!AddTasksToList(destination_list)) {
    return;
  }

  hr = destination_list->CommitList();
  if (FAILED(hr)) {
    DLOG(INFO) << "Commit failed. hr = " << hr;
    return;
  }
}
}  // namespace
#endif  // OS_WIN

#ifdef OS_WIN
namespace {

struct FindVisibleWindowInfo {
  HWND found_window_handle;
  DWORD target_process_id;
};

BOOL CALLBACK FindVisibleWindowProc(HWND hwnd, LPARAM lp) {
  DWORD process_id = 0;
  ::GetWindowThreadProcessId(hwnd, &process_id);
  FindVisibleWindowInfo *info = reinterpret_cast<FindVisibleWindowInfo *>(lp);
  if (process_id != info->target_process_id) {
    // continue enum
    return TRUE;
  }
  if (::IsWindowVisible(hwnd) == FALSE) {
    // continue enum
    return TRUE;
  }
  info->found_window_handle = hwnd;
  return FALSE;
}

}  // namespace
#endif  // OS_WIN

void WinUtil::ActivateWindow(uint32_t process_id) {
#ifdef OS_WIN
  FindVisibleWindowInfo info = {};
  info.target_process_id = process_id;

  // The target process may contain several top-level windows.
  // We do not care about the invisible windows.
  if (::EnumWindows(FindVisibleWindowProc, reinterpret_cast<LPARAM>(&info)) !=
      0) {
    LOG(ERROR) << "Could not find the exsisting window.";
  }
  const CWindow window(info.found_window_handle);
  std::wstring window_title_wide;
  {
    CString buf;
    window.GetWindowTextW(buf);
    window_title_wide.assign(buf.GetString(), buf.GetLength());
  }
  std::string window_title_utf8;
  Util::WideToUtf8(window_title_wide, &window_title_utf8);
  LOG(INFO) << "A visible window found. hwnd: " << window.m_hWnd
            << ", title: " << window_title_utf8;

  // SetForegroundWindow API does not automatically restore the minimized
  // window. Use explicitly OpenIcon API in this case.
  if (window.IsIconic()) {
    if (::OpenIcon(window.m_hWnd) == FALSE) {
      LOG(ERROR) << "::OpenIcon() failed.";
    }
  }

  // SetForegroundWindow API works wll iff the caller process satisfies the
  // condition described in the following document.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms633539.aspx
  // Never use AttachThreadInput API to work around this restriction.
  // http://blogs.msdn.com/b/oldnewthing/archive/2008/08/01/8795860.aspx
  if (::SetForegroundWindow(window.m_hWnd) == FALSE) {
    LOG(ERROR) << "::SetForegroundWindow() failed.";
  }
#endif  // OS_WIN
}

#ifdef OS_WIN
namespace {
const wchar_t kIMEHotKeyEntryKey[] = L"Keyboard Layout\\Toggle";
const wchar_t kIMEHotKeyEntryValue[] = L"Layout Hotkey";
const wchar_t kIMEHotKeyEntryData[] = L"3";
}  // namespace
#endif  // OS_WIN

// static
bool WinUtil::GetIMEHotKeyDisabled() {
#ifdef OS_WIN
  CRegKey key;
  LONG result = key.Open(HKEY_CURRENT_USER, kIMEHotKeyEntryKey, KEY_READ);

  // When the key doesn't exist, can return |false| as well.
  if (ERROR_SUCCESS != result) {
    return false;
  }

  wchar_t data[4] = {};
  ULONG num_chars = std::size(data);
  result = key.QueryStringValue(kIMEHotKeyEntryValue, data, &num_chars);
  // Returned |num_char| includes nullptr character.

  // This is only the condition when this function
  // can return |true|
  if (ERROR_SUCCESS == result && num_chars < std::size(data) &&
      std::wstring(data) == kIMEHotKeyEntryData) {
    return true;
  }

  return false;
#else   // OS_WIN
  return false;
#endif  // OS_WIN
}

// static
bool WinUtil::SetIMEHotKeyDisabled(bool disabled) {
#ifdef OS_WIN
  if (WinUtil::GetIMEHotKeyDisabled() == disabled) {
    // Do not need to update this entry.
    return true;
  }

  if (disabled) {
    CRegKey key;
    LONG result = key.Create(HKEY_CURRENT_USER, kIMEHotKeyEntryKey);
    if (ERROR_SUCCESS != result) {
      return false;
    }

    // set "3"
    result = key.SetStringValue(kIMEHotKeyEntryValue, kIMEHotKeyEntryData);

    return ERROR_SUCCESS == result;
  } else {
    CRegKey key;
    LONG result =
        key.Open(HKEY_CURRENT_USER, kIMEHotKeyEntryKey, KEY_SET_VALUE | DELETE);
    if (result == ERROR_FILE_NOT_FOUND) {
      return true;  // default value will be used.
    }

    if (ERROR_SUCCESS != result) {
      return false;
    }

    result = key.DeleteValue(kIMEHotKeyEntryValue);

    return (ERROR_SUCCESS == result || ERROR_FILE_NOT_FOUND == result);
  }
#endif  // OS_WIN

  return false;
}

void WinUtil::KeepJumpListUpToDate() {
#ifdef OS_WIN
  HRESULT hr = S_OK;

  hr = ::CoInitializeEx(nullptr,
                        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
  if (FAILED(hr)) {
    DLOG(INFO) << "CoInitializeEx failed. hr = " << hr;
    return;
  }
  InitializeJumpList();
  ::CoUninitialize();
#endif  // OS_WIN
}
}  // namespace gui
}  // namespace mozc
