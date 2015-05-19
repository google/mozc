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

#include "win32/ime/ime_mouse_tracker.h"

namespace mozc {
namespace win32 {
namespace {
struct MouseTrackerInfo {
  HHOOK mouse_hook_handle;
  bool left_button_pressed;
  MouseTrackerInfo()
    : mouse_hook_handle(nullptr),
      left_button_pressed(false) {
  }
};

// ThreadLocalMouseTracker relies on WH_MOUSE as per-thread hook, which means
// 1) HHOOK must be maintained for each thread
// 2) MouseHookProc must users per-thread data storage to store information
//    received by the hook.
// This is why ThreadLocalStorage (TLS) is used here.
MouseTrackerInfo *GetThreadLocalTrackerInfo(DWORD tls_index) {
  if (tls_index == 0) {
    return nullptr;
  }
  return static_cast<MouseTrackerInfo *>(::TlsGetValue(tls_index));
}

// See the comment for GetThreadLocalTrackerInfo about the reason why
// MouseTrackerInfo is stored in the TLS.
void SetThreadLocalTrackerInfo(DWORD tls_index, MouseTrackerInfo *info) {
  if (tls_index == 0) {
    return;
  }
  ::TlsSetValue(tls_index, info);
}
}

LRESULT CALLBACK ThreadLocalMouseTracker::HookMouseProc(
  int code, WPARAM wParam, LPARAM lParam) {
  MouseTrackerInfo *info = GetThreadLocalTrackerInfo(tls_index_);
  if (info == nullptr) {
    return 0;
  }

  const HHOOK mouse_hook_handle = info->mouse_hook_handle;
  if (mouse_hook_handle == nullptr) {
    return 0;
  }

  if (wParam == WM_LBUTTONDOWN) {
    info->left_button_pressed = true;
  }

  // To keep unit tests simple, stop calling CallNextHookEx API here if this
  // special flag is true.
  // TODO(yukawa): Replace this check with mock interface which wraps
  //   CallNextHookEx API.
  if (do_not_call_call_next_hook_ex_) {
    return 0;
  }

  return ::CallNextHookEx(mouse_hook_handle, code, wParam, lParam);
}

void ThreadLocalMouseTracker::EnsureInstalled() {
  // Ensure the current TLS has the pointer to an instance of MouseTrackerInfo.
  MouseTrackerInfo *info = GetThreadLocalTrackerInfo(tls_index_);
  if (info == nullptr) {
    info = new MouseTrackerInfo();
    SetThreadLocalTrackerInfo(tls_index_, info);
  }

  if (info->mouse_hook_handle != nullptr) {
    return;
  }

  info->mouse_hook_handle = ::SetWindowsHookEx(
      WH_MOUSE, HookMouseProc, nullptr, ::GetCurrentThreadId());
}

void ThreadLocalMouseTracker::EnsureUninstalled() {
  MouseTrackerInfo *info = GetThreadLocalTrackerInfo(tls_index_);
  if (info == nullptr) {
    // Already uninstalled.  Nothing to do.
    return;
  }
  SetThreadLocalTrackerInfo(tls_index_, nullptr);

  const HHOOK mouse_hook_handle = info->mouse_hook_handle;
  delete info;

  if (mouse_hook_handle == nullptr) {
    return;
  }
  ::UnhookWindowsHookEx(mouse_hook_handle);
}

bool ThreadLocalMouseTracker::WasLeftButtonPressed() {
  MouseTrackerInfo *info = GetThreadLocalTrackerInfo(tls_index_);
  if (info == nullptr) {
    return false;
  }
  return info->left_button_pressed;
}

void ThreadLocalMouseTracker::ResetWasLeftButtonPressed() {
  MouseTrackerInfo *info = GetThreadLocalTrackerInfo(tls_index_);
  if (info == nullptr) {
    return;
  }
  info->left_button_pressed = false;
}

void ThreadLocalMouseTracker::OnDllProcessAttach(
    HINSTANCE instance, bool static_loading) {
  if (tls_index_ == 0) {
    tls_index_ = ::TlsAlloc();
  }
}

void ThreadLocalMouseTracker::OnDllProcessDetach(
    HINSTANCE instance, bool process_shutdown) {
  if (tls_index_ != 0) {
    MouseTrackerInfo *info =
        static_cast<MouseTrackerInfo *>(::TlsGetValue(tls_index_));
    if (info != nullptr) {
      delete info;
      info = nullptr;
      ::TlsSetValue(tls_index_, nullptr);
    }
    ::TlsFree(tls_index_);
    tls_index_ = 0;
  }
}

void ThreadLocalMouseTracker::set_do_not_call_call_next_hook_ex(bool flag) {
  do_not_call_call_next_hook_ex_ = flag;
}

bool ThreadLocalMouseTracker::do_not_call_call_next_hook_ex_ = false;
DWORD ThreadLocalMouseTracker::tls_index_ = 0;
}  // namespace win32
}  // namespace mozc
