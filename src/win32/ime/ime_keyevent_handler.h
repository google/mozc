// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_WIN32_IME_IME_KEYEVENT_HANDLER_
#define MOZC_WIN32_IME_IME_KEYEVENT_HANDLER_

#include <windows.h>

#include "base/base.h"
#include "base/scoped_ptr.h"
#include "testing/base/public/gunit_prod.h"
// for FRIEND_TEST()

namespace mozc {
namespace client {
class ClientInterface;
}  // namespace client

namespace commands {
class KeyEvent;
class Output;
}  // namespace commands

namespace win32 {
class KeyboardStatus;
class LParamKeyInfo;
class VirtualKey;
class Win32KeyboardInterface;
struct ImeState;
struct ImeBehavior;

struct KeyEventHandlerResult {
  bool should_be_eaten;
  bool should_be_sent_to_server;
  bool succeeded;
  KeyEventHandlerResult();
};

// TODO(yukawa): Refactor to support NotifyIME and UI messages.
class KeyEventHandler {
 public:
  // Updates |behavior->prefer_kana_input| based on the key and IME open
  // status.  Currently, key down event of VK_DBE_ROMAN or VK_DBE_NOROMAN
  // flips the input style when both |state.open| and
  // |behavior->use_kanji_key_to_toggle_input_style| is true.
  static void UpdateBehaviorInImeProcessKey(
      const VirtualKey &virtual_key,
      const LParamKeyInfo &lparam,
      const ImeState &state,
      ImeBehavior *behavior);

  static KeyEventHandlerResult ImeProcessKey(
      const VirtualKey &virtual_key,
      const LParamKeyInfo &lparam,
      const KeyboardStatus &keyboard_status,
      const ImeBehavior &behavior,
      const ImeState &initial_state,
      mozc::client::ClientInterface *client,
      Win32KeyboardInterface *keyboard,
      ImeState *next_state,
      commands::Output *output);

  static KeyEventHandlerResult ImeToAsciiEx(
      const VirtualKey &virtual_key,
      BYTE scan_code,
      bool is_key_down,
      const KeyboardStatus &keyboard_status,
      const ImeBehavior &behavior,
      const ImeState &initial_state,
      mozc::client::ClientInterface *client,
      Win32KeyboardInterface *keyboard,
      ImeState *next_state,
      commands::Output *output);

 private:
  static KeyEventHandlerResult HandleKey(
      const VirtualKey &virtual_key,
      BYTE scan_code,
      bool is_key_down,
      const KeyboardStatus &keyboard_status,
      const ImeBehavior &behavior,
      const ImeState &initial_state,
      Win32KeyboardInterface *keyboard,
      mozc::commands::KeyEvent *key);

  static bool ConvertToKeyEvent(
      const VirtualKey &virtual_key,
      BYTE scan_code,
      bool is_key_down,
      bool is_menu_active,
      const ImeBehavior &behavior,
      const ImeState &ime_state,
      const KeyboardStatus &keyboard_status,
      Win32KeyboardInterface *keyboard,
      commands::KeyEvent *key);

  // This function updates the current keyboard status so that a user will not
  // be bothered with unexpected Kana-lock.  See b/2601927, b/2521571,
  // b/2487817 and b/2405901 for details.  Thanks to "Kana-lock Free"
  // technique (see b/3046717 for details), Mozc works well even Kana-lock is
  // unlocked.
  // It is also highly recommended to call this function just after the IME
  // starts to handle key event because there might be no chance to unlock an
  // unexpected Kana-Lock except for the key event handler in some tricky
  // cases.  In such a case, you can use the returned |new_keyboard_status|
  // for subsequent key handlers so that they can behave as if the Kana-Lock
  // was unlocked when the key event occurred.
  static void UnlockKanaLock(
      const KeyboardStatus &keyboard_status,
      Win32KeyboardInterface *keyboard,
      KeyboardStatus *new_keyboard_status);

  // Span Tool if launch_tool_mode is set in |output|.
  static void MaybeSpawnTool(mozc::client::ClientInterface *client,
                             commands::Output *output);

  FRIEND_TEST(ImeKeyeventHandlerTest, HankakuZenkakuTest);
  FRIEND_TEST(ImeKeyeventHandlerTest, CustomActivationKeyTest);
  FRIEND_TEST(ImeKeyeventHandlerTest, Issue2903247_KeyUpShouldNotBeEaten);
  FRIEND_TEST(ImeKeyeventHandlerTest, ClearKanaLockInAlphanumericMode);
  FRIEND_TEST(ImeKeyeventHandlerTest, ClearKanaLockEvenWhenIMEIsDisabled);
  FRIEND_TEST(ImeKeyeventHandlerTest,
              ProtocolAnomaly_ModiferKeyMayBeSentOnKeyUp);
  FRIEND_TEST(ImeKeyeventHandlerTest,
              ProtocolAnomaly_ModifierShiftShouldBeRemovedForPrintableChar);
  FRIEND_TEST(ImeKeyeventHandlerTest,
              ProtocolAnomaly_ModifierKeysShouldBeRemovedAsForSomeSpecialKeys);
  FRIEND_TEST(ImeKeyeventHandlerTest,
              ProtocolAnomaly_KeyCodeIsFullWidthHiraganaWhenKanaLockIsEnabled);
  FRIEND_TEST(ImeKeyeventHandlerTest,
              CheckKeyCodeWhenAlphabeticalKeyIsPressedWithCtrl);
  FRIEND_TEST(ImeKeyeventHandlerTest,
              Issue2801503_ModeChangeWhenIMEIsGoingToBeTurnedOff);

  DISALLOW_COPY_AND_ASSIGN(KeyEventHandler);
};
}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_IME_IME_KEYEVENT_HANDLER_
