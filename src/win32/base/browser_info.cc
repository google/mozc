// Copyright 2010-2020, Google Inc.
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

#include "win32/base/browser_info.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>

#include <string>

#include "base/util.h"
#include "base/win_util.h"
#include "win32/base/accessible_object.h"
#include "win32/base/focus_hierarchy_observer.h"

namespace mozc {
namespace win32 {
namespace {

using ::ATL::CComPtr;
using ::ATL::CComQIPtr;
using ::ATL::CComVariant;

wchar_t g_exe_module_name_[4096];
size_t g_exe_module_name_len_ = 0;

int g_exe_module_ver_major_ = 0;
int g_exe_module_ver_minor_ = 0;
int g_exe_module_ver_build_ = 0;
int g_exe_module_ver_revision_ = 0;
bool g_exe_module_ver_initialized_ = false;

BrowserInfo::BrowserType g_browser_type_ = BrowserInfo::kBrowserTypeUnknown;
bool g_browser_type_initialized_ = false;

std::wstring GetProcessModuleName() {
  if (g_exe_module_name_len_ == 0) {
    return L"";
  }
  return std::wstring(g_exe_module_name_, g_exe_module_name_len_);
}

}  // namespace

// static
BrowserInfo::BrowserType BrowserInfo::GetBrowerType() {
  if (!g_browser_type_initialized_) {
    bool loder_locked = false;
    if (!WinUtil::IsDLLSynchronizationHeld(&loder_locked) || loder_locked) {
      return kBrowserTypeUnknown;
    }
    string exe_path_utf8;
    Util::WideToUTF8(GetProcessModuleName(), &exe_path_utf8);
    Util::LowerString(&exe_path_utf8);
    if (Util::EndsWith(exe_path_utf8, "chrome.exe")) {
      g_browser_type_ = kBrowserTypeChrome;
    } else if (Util::EndsWith(exe_path_utf8, "firefox.exe")) {
      g_browser_type_ = kBrowserTypeFirefox;
    } else if (Util::EndsWith(exe_path_utf8, "iexplore.exe")) {
      g_browser_type_ = kBrowserTypeIE;
    } else if (Util::EndsWith(exe_path_utf8, "opera.exe")) {
      g_browser_type_ = kBrowserTypeOpera;
    } else {
      g_browser_type_ = kBrowserTypeUnknown;
    }
    g_browser_type_initialized_ = true;
  }
  return g_browser_type_;
}

// static
bool BrowserInfo::IsInIncognitoMode(
    const FocusHierarchyObserver &focus_hierarchy_observer) {
  if (GetBrowerType() == kBrowserTypeUnknown) {
    return false;
  }

  bool loder_locked = false;
  if (!WinUtil::IsDLLSynchronizationHeld(&loder_locked) || loder_locked) {
    return false;
  }

  const string root_window_name = focus_hierarchy_observer.GetRootWindowName();
  if (root_window_name.empty()) {
    return false;
  }

  const char *sufix_ja = nullptr;
  const char *sufix_en = nullptr;
  switch (GetBrowerType()) {
    case kBrowserTypeChrome:
      sufix_ja = "（シークレット モード）";
      sufix_en = "(Incognito)";
      break;
    case kBrowserTypeFirefox:
      sufix_ja = " (プライベートブラウジング)";
      sufix_en = "(Private Browsing)";
      break;
    case kBrowserTypeIE:
      sufix_ja = "[InPrivate]";
      sufix_en = "[InPrivate]";
      break;
  }
  if (sufix_ja != nullptr && Util::EndsWith(root_window_name, sufix_ja)) {
    return true;
  }
  if (sufix_en != nullptr && Util::EndsWith(root_window_name, sufix_en)) {
    return true;
  }
  return false;
}

// static
bool BrowserInfo::IsOnChromeOmnibox(
    const FocusHierarchyObserver &focus_hierarchy_observer) {
  if (GetBrowerType() != kBrowserTypeChrome) {
    return false;
  }

  bool loder_locked = false;
  if (!WinUtil::IsDLLSynchronizationHeld(&loder_locked) || loder_locked) {
    return false;
  }

  const auto &ui_hierarchy = focus_hierarchy_observer.GetUIHierarchy();
  if (ui_hierarchy.size() == 0) {
    return false;
  }
  const auto &current_ui_element = ui_hierarchy.front();
  if (!current_ui_element.is_builtin_role ||
      current_ui_element.role != "ROLE_SYSTEM_TEXT") {
    return false;
  }
  const char kOmniboxDescJa[] = "アドレス検索バー";
  if (current_ui_element.name == kOmniboxDescJa) {
    return true;
  }
  const char kOmniboxDescEn[] = "Address and search bar";
  if (current_ui_element.name == kOmniboxDescEn) {
    return true;
  }
  return false;
}

// static
void BrowserInfo::OnDllProcessAttach(HINSTANCE module_handle,
                                     bool static_loading) {
  const DWORD copied_len_without_null = ::GetModuleFileName(
      nullptr, g_exe_module_name_, arraysize(g_exe_module_name_));
  if ((copied_len_without_null + 1) < arraysize(g_exe_module_name_)) {
    g_exe_module_name_len_ = copied_len_without_null;
  }
}

// static
void BrowserInfo::OnDllProcessDetach(HINSTANCE module_handle,
                                     bool process_shutdown) {}

}  // namespace win32
}  // namespace mozc
