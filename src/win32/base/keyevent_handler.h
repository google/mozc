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

#ifndef MOZC_WIN32_BASE_KEYEVENT_HANDLER_H_
#define MOZC_WIN32_BASE_KEYEVENT_HANDLER_H_

#include <windows.h>

#include "base/port.h"

namespace mozc {
namespace client {
class ClientInterface;
}  // namespace client

namespace commands {
class Context;
class KeyEvent;
class Output;
}  // namespace commands

namespace win32 {
class KeyboardStatus;
class LParamKeyInfo;
class VirtualKey;
class Win32KeyboardInterface;
struct InputState;
struct InputBehavior;

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
  // |behavior->use_romaji_key_to_toggle_input_style| is true.
  static void UpdateBehaviorInImeProcessKey(const VirtualKey &virtual_key,
                                            bool is_key_down,
                                            const InputState &state,
                                            InputBehavior *behavior);

  static KeyEventHandlerResult ImeProcessKey(
      const VirtualKey &virtual_key, BYTE scan_code, bool is_key_down,
      const KeyboardStatus &keyboard_status, const InputBehavior &behavior,
      const InputState &initial_state, const commands::Context &context,
      client::ClientInterface *client, Win32KeyboardInterface *keyboard,
      InputState *next_state, commands::Output *output);

  static KeyEventHandlerResult ImeToAsciiEx(
      const VirtualKey &virtual_key, BYTE scan_code, bool is_key_down,
      const KeyboardStatus &keyboard_status, const InputBehavior &behavior,
      const InputState &initial_state, const commands::Context &context,
      client::ClientInterface *client, Win32KeyboardInterface *keyboard,
      InputState *next_state, commands::Output *output);

 protected:
  static KeyEventHandlerResult HandleKey(const VirtualKey &virtual_key,
                                         BYTE scan_code, bool is_key_down,
                                         const KeyboardStatus &keyboard_status,
                                         const InputBehavior &behavior,
                                         const InputState &initial_state,
                                         Win32KeyboardInterface *keyboard,
                                         commands::KeyEvent *key);

  static bool ConvertToKeyEvent(const VirtualKey &virtual_key, BYTE scan_code,
                                bool is_key_down, bool is_menu_active,
                                const InputBehavior &behavior,
                                const InputState &ime_state,
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
  static void UnlockKanaLock(const KeyboardStatus &keyboard_status,
                             Win32KeyboardInterface *keyboard,
                             KeyboardStatus *new_keyboard_status);

  // Span Tool if launch_tool_mode is set in |output|.
  static void MaybeSpawnTool(client::ClientInterface *client,
                             commands::Output *output);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(KeyEventHandler);
};
}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_BASE_KEYEVENT_HANDLER_H_
