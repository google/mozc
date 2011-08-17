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

#include <windows.h>
#include <ime.h>
#include <msctf.h>

#include <string>

#include "base/thread.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/ime/ime_mouse_tracker.h"

namespace mozc {
namespace win32 {
namespace {
const MOUSEHOOKSTRUCT kMouseHookInfo = {
  {100, 200},  // pt
  NULL,        // hwnd
  HTNOWHERE,   // wHitTestCode
  NULL         // dwExtraInfo
};

class AlwaysOnTestThread : public Thread {
 public:
  void Run() {
    ThreadLocalMouseTracker::EnsureInstalled();
    EXPECT_FALSE(ThreadLocalMouseTracker::WasLeftButtonPressed());

    // WM_LBUTTONDOWN must be recorded by the ThreadLocalMouseTracker.
    ThreadLocalMouseTracker::HookMouseProc(
        HC_ACTION, WM_LBUTTONDOWN, reinterpret_cast<LPARAM>(&kMouseHookInfo));
    EXPECT_TRUE(ThreadLocalMouseTracker::WasLeftButtonPressed());

    // Wait 5 seconds to check if Thread Local Storage works well.
    Util::Sleep(5000);

    ThreadLocalMouseTracker::EnsureUninstalled();
  }
};

class OnOffTestThread : public Thread {
 public:
  void Run() {
    ThreadLocalMouseTracker::EnsureInstalled();
    EXPECT_FALSE(ThreadLocalMouseTracker::WasLeftButtonPressed());

    // WM_LBUTTONDOWN must be recorded by the ThreadLocalMouseTracker.
    ThreadLocalMouseTracker::HookMouseProc(
        HC_ACTION, WM_LBUTTONDOWN, reinterpret_cast<LPARAM>(&kMouseHookInfo));
    EXPECT_TRUE(ThreadLocalMouseTracker::WasLeftButtonPressed());

    ThreadLocalMouseTracker::ResetWasLeftButtonPressed();
    EXPECT_FALSE(ThreadLocalMouseTracker::WasLeftButtonPressed());

    // WM_RBUTTONDOWN must not be recorded by the ThreadLocalMouseTracker.
    ThreadLocalMouseTracker::HookMouseProc(
        HC_ACTION, WM_RBUTTONDOWN, reinterpret_cast<LPARAM>(&kMouseHookInfo));
    EXPECT_FALSE(ThreadLocalMouseTracker::WasLeftButtonPressed());

    // Currently WM_LBUTTONDBLCLK is not recorded.
    ThreadLocalMouseTracker::HookMouseProc(
        HC_ACTION, WM_LBUTTONDBLCLK,
        reinterpret_cast<LPARAM>(&kMouseHookInfo));
    EXPECT_FALSE(ThreadLocalMouseTracker::WasLeftButtonPressed());

    // Check WM_LBUTTONDOWN again.
    ThreadLocalMouseTracker::HookMouseProc(
        HC_ACTION, WM_LBUTTONDOWN, reinterpret_cast<LPARAM>(&kMouseHookInfo));
    EXPECT_TRUE(ThreadLocalMouseTracker::WasLeftButtonPressed());

    ThreadLocalMouseTracker::EnsureUninstalled();
  }
 private:
};
}  // anonymous namespace

class MouseTrackerTest : public testing::Test {
 protected:
  static void SetUpTestCase() {
    // Prevent ThreadLocalMouseTracker from calling CallNextHookEx API
    // so that unit tests can call ThreadLocalMouseTracker::HookMouseProc
    // without setting up test windows including message pump.
    ThreadLocalMouseTracker::set_do_not_call_call_next_hook_ex(true);

    ThreadLocalMouseTracker::OnDllProcessAttach(NULL, true);
  }
  static void TearDownTestCase() {
    ThreadLocalMouseTracker::OnDllProcessDetach(NULL, true);
  }
};

TEST_F(MouseTrackerTest, BasicTest) {
  AlwaysOnTestThread always_on_test_thread;
  always_on_test_thread.Start();
  Util::Sleep(1000);

  OnOffTestThread on_off_test_thread;
  on_off_test_thread.Start();

  always_on_test_thread.Join();
  on_off_test_thread.Join();
}
}  // namespace win32
}  // namespace mozc
