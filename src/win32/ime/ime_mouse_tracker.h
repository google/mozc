// Copyright 2010-2011, Google Inc.
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

#ifndef MOZC_WIN32_IME_IME_MOUSE_TRACKER_H_
#define MOZC_WIN32_IME_IME_MOUSE_TRACKER_H_

#include <windows.h>

#include <string>

#include "base/port.h"

namespace mozc {
namespace win32 {
// This class monitors WM_LBUTTONDOWN event dispatched into any window in the
// current thread.
class ThreadLocalMouseTracker {
 public:
  // This method must be called in each UI thread to initialize
  // internal data for the thread. Second call will be ignored.
  // This method is thread-safe.
  static void EnsureInstalled();

  // This method must be called in each UI thread to initialize
  // internal data for the thread. Second call will be ignored.
  // This method is thread-safe.
  static void EnsureUninstalled();

  // Returns true if one or more WM_LBUTTONDOWN event was dispatched to any
  // window in this thread after the last call of ResetWasLeftButtonPressed
  // method.
  // This method is thread-safe.
  static bool WasLeftButtonPressed();

  // Reset the flag to track WM_LBUTTONDOWN event for this thread.
  // This method is thread-safe.
  static void ResetWasLeftButtonPressed();

  // This method must be called whenever DllMain receives DLL_PROCESS_ATTACH.
  // Although this method is not thread-safe, you can rely on the
  // thread-safety of DllMain.
  static void OnDllProcessAttach(HINSTANCE instance, bool static_loading);

  // This method must be called whenever DllMain receives DLL_PROCESS_DETACH.
  // Although this method is not thread-safe, you can rely on the
  // thread-safety of DllMain.
  static void OnDllProcessDetach(HINSTANCE instance, bool process_shutdown);

  // Although this method is not thread-safe, you can rely on the
  // thread-safety of Win32 message hook.
  static LRESULT CALLBACK HookMouseProc(
      int code, WPARAM wParam, LPARAM lParam);

  // For unit test.  This method is not thread-safe.
  // TODO(yukawa): Create mock interface to wrap this kind of Win32 APIs.
  static void set_do_not_call_call_next_hook_ex(bool flag);

 private:
  static DWORD tls_index_;

  // If this flag is true, CallNextHookEx API will not be called.
  // This flag must be true only in unit test.
  static bool do_not_call_call_next_hook_ex_;

  DISALLOW_COPY_AND_ASSIGN(ThreadLocalMouseTracker);
};
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_IME_IME_MOUSE_TRACKER_H_
