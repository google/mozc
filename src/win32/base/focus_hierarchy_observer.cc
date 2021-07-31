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

#include "win32/base/focus_hierarchy_observer.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlcom.h>

#include <memory>

#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"
#include "win32/base/accessible_object.h"
#include "win32/base/browser_info.h"

namespace mozc {
namespace win32 {
namespace {

using ::ATL::CComBSTR;
using ::ATL::CComPtr;
using ::ATL::CComQIPtr;
using ::ATL::CComVariant;

using ::std::unique_ptr;

constexpr size_t kMaxHierarchyLevel = 50;

DWORD g_tls_index = TLS_OUT_OF_INDEXES;
HMODULE g_module_handle = nullptr;

std::string UTF16ToUTF8(const std::wstring &str) {
  std::string utf8;
  Util::WideToUTF8(str, &utf8);
  return utf8;
}

std::string GetWindowTestAsUTF8(HWND window_handle) {
  const int text_len = ::GetWindowTextLengthW(window_handle);
  if (text_len <= 0) {
    return "";
  }
  const int buffer_len = text_len + 1;
  unique_ptr<wchar_t[]> buffer(new wchar_t[buffer_len]);
  const int copied_len_without_null =
      ::GetWindowText(window_handle, buffer.get(), buffer_len);
  if (copied_len_without_null <= 0 ||
      copied_len_without_null + 1 > buffer_len) {
    return "";
  }
  return UTF16ToUTF8(std::wstring(buffer.get(), copied_len_without_null));
}

std::string GetWindowClassNameAsUTF8(HWND window_handle) {
  constexpr int kBufferLen = 256 + 1;
  unique_ptr<wchar_t[]> buffer(new wchar_t[kBufferLen]);
  const int copied_len_without_null =
      ::GetClassNameW(window_handle, buffer.get(), kBufferLen);
  if (copied_len_without_null <= 0 ||
      copied_len_without_null + 1 > kBufferLen) {
    return "";
  }
  return UTF16ToUTF8(std::wstring(buffer.get(), copied_len_without_null));
}

DWORD GetProcessIdFromWindow(HWND window_handle) {
  DWORD process_id = 0;
  if (::GetWindowThreadProcessId(window_handle, &process_id) == 0) {
    return 0;
  }
  return process_id;
}

void FillWindowInfo(
    HWND window_handle,
    std::vector<FocusHierarchyObserver::WindowInfo> *window_hierarchy) {
  if (window_handle == nullptr) {
    // error
    window_hierarchy->clear();
    return;
  }
  const HWND ancestor = ::GetAncestor(window_handle, GA_ROOT);
  if (ancestor == nullptr) {
    // error
    window_hierarchy->clear();
    return;
  }
  while (true) {
    if (window_hierarchy->size() > kMaxHierarchyLevel) {
      window_hierarchy->clear();
      return;
    }
    FocusHierarchyObserver::WindowInfo element;
    element.window_handle = window_handle;
    element.title = GetWindowTestAsUTF8(window_handle);
    element.class_name = GetWindowClassNameAsUTF8(window_handle);
    element.process_id = GetProcessIdFromWindow(window_handle);
    window_hierarchy->push_back(element);
    if (window_handle == ancestor) {
      return;
    }
    window_handle = ::GetParent(window_handle);
    if (window_handle == nullptr) {
      return;
    }
  }
  DCHECK(false) << "must not reach here.";
}

void FillAccessibleInfo(AccessibleObject accessible, HWND focused_window_handle,
                        std::vector<AccessibleObjectInfo> *hierarchy) {
  if (!accessible.IsValid()) {
    return;
  }

  hierarchy->push_back(accessible.GetInfo());

  while (true) {
    if (hierarchy->size() > kMaxHierarchyLevel) {
      hierarchy->clear();
      break;
    }
    AccessibleObject parent = accessible.GetParent();
    if (!parent.IsValid()) {
      break;
    }
    HWND parent_window_handle = nullptr;
    if (!parent.GetWindowHandle(&parent_window_handle) ||
        focused_window_handle != parent_window_handle) {
      // finish!
      break;
    }
    hierarchy->push_back(parent.GetInfo());
    accessible = parent;
  }
}

class ThreadLocalInfo {
 public:
  std::vector<AccessibleObjectInfo> ui_hierarchy() const {
    return ui_hierarchy_;
  }
  std::vector<FocusHierarchyObserver::WindowInfo> window_hierarchy() const {
    return window_hierarchy_;
  }
  std::string root_window_name() const { return root_window_name_; }

  void SyncFocusHierarchy() {
    const HWND focused_window = ::GetFocus();
    if (GetProcessIdFromWindow(focused_window) != ::GetCurrentProcessId()) {
      return;
    }

    AccessibleObject accesible = AccessibleObject::FromWindow(focused_window);
    if (accesible.IsValid()) {
      AccessibleObject focused_accesible = accesible.GetFocus();
      if (focused_accesible.IsValid()) {
        accesible = focused_accesible;
      }
    }
    OnInitialize(focused_window, accesible);
  }

  void AddRef() { ++ref_count_; }

  void Release() {
    --ref_count_;
    if (ref_count_ <= 0) {
      delete this;
    }
  }

  static ThreadLocalInfo *Self() {
    if (g_tls_index == TLS_OUT_OF_INDEXES) {
      return nullptr;
    }
    return static_cast<ThreadLocalInfo *>(::TlsGetValue(g_tls_index));
  }

  static ThreadLocalInfo *EnsureExists() {
    DCHECK_NE(TLS_OUT_OF_INDEXES, g_tls_index);
    auto *info = static_cast<ThreadLocalInfo *>(::TlsGetValue(g_tls_index));
    if (info == nullptr) {
      info = ThreadLocalInfo::Create();
    }
    return info;
  }

 private:
  explicit ThreadLocalInfo(HWINEVENTHOOK hook_handle)
      : ref_count_(0), hook_handle_(hook_handle) {
    ::TlsSetValue(g_tls_index, this);
  }

  ~ThreadLocalInfo() {
    if (hook_handle_ != nullptr) {
      ::UnhookWinEvent(hook_handle_);
      hook_handle_ = nullptr;
    }
    ::TlsSetValue(g_tls_index, nullptr);
  }

  static ThreadLocalInfo *Create() {
    if (g_module_handle == nullptr) {
      return nullptr;
    }

    auto hook_handle = ::SetWinEventHook(
        EVENT_OBJECT_FOCUS, EVENT_OBJECT_FOCUS, g_module_handle, WinEventProc,
        ::GetCurrentProcessId(), ::GetCurrentThreadId(), WINEVENT_INCONTEXT);

    if (hook_handle == nullptr) {
      return nullptr;
    }

    return new ThreadLocalInfo(hook_handle);
  }

  static void CALLBACK WinEventProc(HWINEVENTHOOK hook_handle, DWORD event,
                                    HWND window_handle, LONG object_id,
                                    LONG child_id, DWORD event_thread,
                                    DWORD event_time) {
    if (g_module_handle == nullptr) {
      return;
    }

    if (!::IsWindow(window_handle)) {
      return;
    }

    if (event != EVENT_OBJECT_FOCUS) {
      return;
    }

    const DWORD window_process_id = GetProcessIdFromWindow(window_handle);
    if (window_process_id != ::GetCurrentProcessId()) {
      // avoid interprocess call.
      return;
    }

    auto *self = Self();
    if (self == nullptr) {
      return;
    }

    AccessibleObject accessible;
    {
      CComPtr<IAccessible> container;
      CComVariant child;
      HRESULT result = ::AccessibleObjectFromEvent(
          window_handle, object_id, child_id, &container, &child);
      if (SUCCEEDED(result) && (container != nullptr) && (child.vt == VT_I4)) {
        accessible = AccessibleObject(container, child.lVal);
      }
    }

    self->OnInitialize(window_handle, accessible);
  }

  void OnInitialize(HWND window_handle, AccessibleObject accessible) {
    ui_hierarchy_.clear();
    window_hierarchy_.clear();
    root_window_name_.clear();

    FillWindowInfo(window_handle, &window_hierarchy_);
    FillAccessibleInfo(accessible, window_handle, &ui_hierarchy_);

    if (!window_hierarchy_.empty()) {
      const HWND root_window_handle = window_hierarchy_.back().window_handle;
      const DWORD root_window_process_id =
          GetProcessIdFromWindow(root_window_handle);
      if (root_window_process_id != ::GetCurrentProcessId()) {
        // avoid interprocess call
        root_window_name_ = GetWindowTestAsUTF8(root_window_handle);
      } else {
        const AccessibleObject root_object =
            AccessibleObject::FromWindow(root_window_handle);
        if (root_object.IsValid()) {
          root_window_name_ = root_object.GetInfo().name;
        } else {
          root_window_name_ = GetWindowTestAsUTF8(root_window_handle);
        }
      }
    }
  }

  int ref_count_;
  HWINEVENTHOOK hook_handle_;
  std::vector<AccessibleObjectInfo> ui_hierarchy_;
  std::vector<FocusHierarchyObserver::WindowInfo> window_hierarchy_;
  std::string root_window_name_;
};

bool TlsAvailable() { return g_tls_index != TLS_OUT_OF_INDEXES; }

class FocusHierarchyObserverImpl : public FocusHierarchyObserver {
 public:
  static FocusHierarchyObserver *Create() {
    if (!TlsAvailable()) {
      return nullptr;
    }
    auto *info = ThreadLocalInfo::EnsureExists();
    info->AddRef();
    return new FocusHierarchyObserverImpl();
  }

 private:
  FocusHierarchyObserverImpl() {}

  virtual ~FocusHierarchyObserverImpl() {
    auto *self = ThreadLocalInfo::Self();
    if (self != nullptr) {
      self->Release();
    }
  }

  // FocusHierarchyObserver overrides:
  virtual void SyncFocusHierarchy() {
    auto *self = ThreadLocalInfo::Self();
    if (self == nullptr) {
      return;
    }
    self->SyncFocusHierarchy();
  }
  virtual bool IsAbailable() const {
    return ThreadLocalInfo::Self() != nullptr;
  }
  virtual std::vector<AccessibleObjectInfo> GetUIHierarchy() const {
    auto *self = ThreadLocalInfo::Self();
    if (self == nullptr) {
      return std::vector<AccessibleObjectInfo>();
    }
    return self->ui_hierarchy();
  }
  virtual std::vector<WindowInfo> GetWindowHierarchy() const {
    auto *self = ThreadLocalInfo::Self();
    if (self == nullptr) {
      return std::vector<FocusHierarchyObserver::WindowInfo>();
    }
    return self->window_hierarchy();
  }
  virtual std::string GetRootWindowName() const {
    auto *self = ThreadLocalInfo::Self();
    if (self == nullptr) {
      return "";
    }
    return self->root_window_name();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusHierarchyObserverImpl);
};

class FocusHierarchyObserverNullImpl : public FocusHierarchyObserver {
 public:
  FocusHierarchyObserverNullImpl() {}
  virtual ~FocusHierarchyObserverNullImpl() {}

 private:
  // FocusHierarchyObserver overrides:
  virtual void SyncFocusHierarchy() {}
  virtual bool IsAbailable() const { return false; }
  virtual std::vector<AccessibleObjectInfo> GetUIHierarchy() const {
    return std::vector<AccessibleObjectInfo>();
  }
  virtual std::vector<WindowInfo> GetWindowHierarchy() const {
    return std::vector<WindowInfo>();
  }
  virtual std::string GetRootWindowName() const { return ""; }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusHierarchyObserverNullImpl);
};

}  // namespace

FocusHierarchyObserver::~FocusHierarchyObserver() {}

FocusHierarchyObserver::WindowInfo::WindowInfo()
    : window_handle(nullptr), process_id(0) {}

// static
void FocusHierarchyObserver::OnDllProcessAttach(HINSTANCE module_handle,
                                                bool static_loading) {
  g_tls_index = ::TlsAlloc();
  g_module_handle = module_handle;
}

// static
void FocusHierarchyObserver::OnDllProcessDetach(HINSTANCE module_handle,
                                                bool process_shutdown) {
  if (g_tls_index != TLS_OUT_OF_INDEXES) {
    ::TlsFree(g_tls_index);
    g_tls_index = TLS_OUT_OF_INDEXES;
  }
  g_module_handle = nullptr;
}

// static
FocusHierarchyObserver *FocusHierarchyObserver::Create() {
  // Note: Currently FocusHierarchyObserver is enabled only with Chromium.
  // TODO(yukawa): Extend the target applications.

  // The following code may affect the issue that the suggest window is not
  // shown in Chromium.  As a workaround, this function always returns
  // new FocusHierarchyObserverNullImpl();
  //
  // TODO: Reactivate the following code when b/23803984 is properly fixed.
  //
  // if (BrowserInfo::GetBrowerType() != BrowserInfo::kBrowserTypeChrome) {
  //   return new FocusHierarchyObserverNullImpl();
  // }
  //
  // auto *impl = FocusHierarchyObserverImpl::Create();
  // if (impl != nullptr) {
  //   return impl;
  // }
  return new FocusHierarchyObserverNullImpl();
}

}  // namespace win32
}  // namespace mozc
