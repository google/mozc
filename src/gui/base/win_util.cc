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

// clang-format off
#include <windows.h>
#include <atlbase.h>
#include <atlstr.h>
#include <atlwin.h>

#include <knownfolders.h>
#include <objectarray.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shlobj.h>
#include <shobjidl.h>
// clang-format on
#include <wil/com.h>
#include <wil/resource.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "absl/log/log.h"
#include "base/strings/zstring_view.h"
#include "base/system_util.h"
#include "base/win32/com.h"
#include "base/win32/hresult.h"
#include "base/win32/wide_char.h"

namespace mozc {
namespace gui {

namespace {

using unique_prop_variant_default_init =
    ::wil::unique_struct<PROPVARIANT, decltype(&::PropVariantClear),
                         ::PropVariantClear>;
using ::mozc::win32::ComCreateInstance;
using ::mozc::win32::HResult;
using ::mozc::win32::Utf8ToWide;

wil::com_ptr_nothrow<IShellLink> InitializeShellLinkItem(
    const zwstring_view argument, const zwstring_view item_title) {
  auto link = ComCreateInstance<IShellLink>(CLSID_ShellLink);
  if (!link) {
    DLOG(INFO) << "Failed to instantiate CLSID_ShellLink.";
    return nullptr;
  }

  HResult hr(link->SetPath(Utf8ToWide(SystemUtil::GetToolPath()).c_str()));
  if (hr.Failed()) {
    DLOG(ERROR) << "SetPath failed. hr = " << hr;
    return nullptr;
  }

  hr = link->SetArguments(argument.c_str());
  if (hr.Failed()) {
    DLOG(ERROR) << "SetArguments failed. hr = " << hr;
    return nullptr;
  }

  auto property_store = win32::ComQuery<IPropertyStore>(link);
  if (property_store == nullptr) {
    DLOG(ERROR) << "QueryInterface failed.";
    return nullptr;
  }

  unique_prop_variant_default_init prop_variant;
  hr = ::InitPropVariantFromString(item_title.c_str(),
                                   prop_variant.reset_and_addressof());
  if (hr.Failed()) {
    DLOG(ERROR) << "InitPropVariantFromString failed. hr = " << hr;
    return nullptr;
  }

  hr = property_store->SetValue(PKEY_Title, prop_variant);
  if (hr.Failed()) {
    DLOG(ERROR) << "SetValue failed. hr = " << hr;
    return nullptr;
  }

  hr = property_store->Commit();
  if (hr.Failed()) {
    DLOG(ERROR) << "Commit failed. hr = " << hr;
    return nullptr;
  }

  return link;
}

struct LinkInfo {
  zwstring_view argument;
  zwstring_view title_english;
  zwstring_view title_japanese;
};

bool AddTasksToList(ICustomDestinationList *destination_list) {
  auto object_collection =
      ComCreateInstance<IObjectCollection>(CLSID_EnumerableObjectCollection);
  if (!object_collection) {
    DLOG(INFO) << "Failed to instantiate CLSID_EnumerableObjectCollection.";
    return false;
  }

  // TODO(yukawa): Investigate better way to localize strings.
  constexpr LinkInfo kLinks[] = {
      {L"--mode=dictionary_tool", L"Dictionary Tool", L"辞書ツール"},
      {L"--mode=word_register_dialog", L"Add Word", L"単語登録"},
      {L"--mode=config_dialog", L"Properties", L"プロパティ"},
  };

  const LANGID kJapaneseLangId =
      MAKELANGID(LANG_JAPANESE, SUBLANG_JAPANESE_JAPAN);
  const bool use_japanese_ui =
      (kJapaneseLangId == ::GetUserDefaultUILanguage());

  for (size_t i = 0; i < std::size(kLinks); ++i) {
    wil::com_ptr_nothrow<IShellLink> link = InitializeShellLinkItem(
        kLinks[i].argument,
        use_japanese_ui ? kLinks[i].title_japanese : kLinks[i].title_english);
    if (link) {
      object_collection->AddObject(link.get());
    }
  }

  auto object_array = win32::ComQuery<IObjectArray>(object_collection);
  if (!object_array) {
    DLOG(ERROR) << "QueryInterface failed.";
    return false;
  }

  HResult hr(destination_list->AddUserTasks(object_array.get()));
  if (hr.Failed()) {
    DLOG(ERROR) << "AddUserTasks failed. hr = " << hr;
    return false;
  }

  return true;
}

void InitializeJumpList() {
  auto destination_list =
      ComCreateInstance<ICustomDestinationList>(CLSID_DestinationList);
  if (!destination_list) {
    DLOG(INFO) << "Failed to instantiate CLSID_DestinationList.";
    return;
  }

  UINT min_slots = 0;
  wil::com_ptr_nothrow<IObjectArray> removed_objects;
  HResult hr(destination_list->BeginList(&min_slots, IID_IObjectArray,
                                         removed_objects.put_void()));
  if (hr.Failed()) {
    DLOG(INFO) << "BeginList failed. hr = " << hr;
    return;
  }

  if (!AddTasksToList(destination_list.get())) {
    return;
  }

  hr = destination_list->CommitList();
  if (hr.Failed()) {
    DLOG(INFO) << "Commit failed. hr = " << hr;
    return;
  }
}
}  // namespace

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

void WinUtil::ActivateWindow(uint32_t process_id) {
  FindVisibleWindowInfo info = {};
  info.target_process_id = process_id;

  // The target process may contain several top-level windows.
  // We do not care about the invisible windows.
  if (::EnumWindows(FindVisibleWindowProc, reinterpret_cast<LPARAM>(&info)) !=
      0) {
    LOG(ERROR) << "Could not find the exsisting window.";
  }
  const ATL::CWindow window(info.found_window_handle);
  ATL::CStringW window_title_wide;
  window.GetWindowTextW(window_title_wide);
  LOG(INFO) << "A visible window found. hwnd: " << window.m_hWnd << ", title: "
            << win32::WideToUtf8(
                   std::wstring_view(window_title_wide.GetString(),
                                     window_title_wide.GetLength()));

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
}

namespace {
constexpr wchar_t kIMEHotKeyEntryKey[] = L"Keyboard Layout\\Toggle";
constexpr wchar_t kIMEHotKeyEntryValue[] = L"Layout Hotkey";
constexpr wchar_t kIMEHotKeyEntryData[] = L"3";
}  // namespace

// static
bool WinUtil::GetIMEHotKeyDisabled() {
  ATL::CRegKey key;
  LONG result = key.Open(HKEY_CURRENT_USER, kIMEHotKeyEntryKey, KEY_READ);

  // When the key doesn't exist, can return |false| as well.
  if (result != ERROR_SUCCESS) {
    return false;
  }

  std::wstring data(3, 0);
  ULONG num_chars = data.size() + 1;
  result = key.QueryStringValue(kIMEHotKeyEntryValue, data.data(), &num_chars);
  // Returned |num_char| includes null character.
  if (result != ERROR_SUCCESS || num_chars > data.size()) {
    return false;
  }
  data.erase(num_chars - 1);
  return data == kIMEHotKeyEntryData;
}

// static
bool WinUtil::SetIMEHotKeyDisabled(bool disabled) {
  if (WinUtil::GetIMEHotKeyDisabled() == disabled) {
    // Do not need to update this entry.
    return true;
  }

  if (disabled) {
    ATL::CRegKey key;
    LONG result = key.Create(HKEY_CURRENT_USER, kIMEHotKeyEntryKey);
    if (result != ERROR_SUCCESS) {
      return false;
    }

    // set "3"
    result = key.SetStringValue(kIMEHotKeyEntryValue, kIMEHotKeyEntryData);

    return result == ERROR_SUCCESS;
  } else {
    ATL::CRegKey key;
    LONG result =
        key.Open(HKEY_CURRENT_USER, kIMEHotKeyEntryKey, KEY_SET_VALUE | DELETE);
    if (result == ERROR_FILE_NOT_FOUND) {
      return true;  // default value will be used.
    }

    if (result != ERROR_SUCCESS) {
      return false;
    }

    result = key.DeleteValue(kIMEHotKeyEntryValue);

    return (result == ERROR_SUCCESS || result != ERROR_FILE_NOT_FOUND);
  }
}

void WinUtil::KeepJumpListUpToDate() {
  HResult hr(::CoInitializeEx(
      nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
  if (hr.Failed()) {
    DLOG(INFO) << "CoInitializeEx failed. hr = " << hr;
    return;
  }
  InitializeJumpList();
  ::CoUninitialize();
}
}  // namespace gui
}  // namespace mozc
