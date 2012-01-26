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

#ifndef MOZC_WIN32_IME_IME_KEYBOARD_
#define MOZC_WIN32_IME_IME_KEYBOARD_

#include <windows.h>

#include <vector>

#include "base/port.h"

namespace mozc {
namespace win32 {
class KeyboardStatus {
 public:
  KeyboardStatus();
  explicit KeyboardStatus(const BYTE key_status[256]);
  BYTE GetState(int virtual_key) const;
  void SetState(int virtual_key, BYTE value);
  bool IsToggled(int virtual_key) const;
  bool IsPressed(int virtual_key) const;
  const BYTE *status() const;
  BYTE *mutable_status();
  size_t status_size() const;
 private:
  BYTE status_[256];
};

class LParamKeyInfo {
 public:
  LParamKeyInfo();
  explicit LParamKeyInfo(LPARAM lparam);
  int GetKeyRepeatCount() const;
  BYTE GetScanCode() const;
  bool IsExtendedKey() const;
  bool HasContextCode() const;
  bool IsPreviousStateDwon() const;
  bool IsInTansitionState() const;
  // In ImeProcessKey callback, the highest bit represents if this is key-down
  // event or not.  You should not use this value in other situations including
  // WM_KEYDOWN/WM_KEYUP event handler.
  // Returns true if this is key-down event assuming this is the LPARAM passed
  // to ImeProcessKey callback.
  bool IsKeyDownInImeProcessKey() const;
  LPARAM lparam() const;
 private:
  LPARAM lparam_;
};

class VirtualKey {
 public:
  VirtualKey();

  // Construct an instance from a given |virtual_key|.
  // You cannot specify VK_PACKET for |virtual_key|.
  static VirtualKey FromVirtualKey(BYTE virtual_key);
  // Construct an instance from a given |combined_virtual_key|.
  // If the low word of |combined_virtual_key| is VK_PACKET,
  // the high word will be used as |wide_char_|.
  // Otherwise, the lowest byte of |combined_virtual_key| will be
  // used as |virtual_key_|.
  static VirtualKey FromCombinedVirtualKey(UINT combined_virtual_key);
  // Construct an instance from a given ucs4 character.
  // In this case, |virtual_key_| will be set to VK_PACKET.
  static VirtualKey FromUnicode(char32 unicode);

  wchar_t wide_char() const;
  char32 unicode_char() const;
  BYTE virtual_key() const;

 private:
  VirtualKey(BYTE virtual_key, wchar_t wide_char, char32 unicode_char);
  char32 unicode_char_;
  wchar_t wide_char_;
  BYTE virtual_key_;
};

// We intentionally wrap some APIs as virthal methods so that unit tests can
// inject their own mock into the key handler.  You can implement each method
// as a redirector to the corresponding API for production, or implement it as
// a mock which emulates the API predictably for unit tests.
class Win32KeyboardInterface {
 public:
  virtual ~Win32KeyboardInterface() {}

  // Injection point for keyboard_state.IsPressed(VK_KANA).
  virtual bool IsKanaLocked(const KeyboardStatus &keyboard_state) = 0;

  // Injection point for SetKeyboardState API.
  virtual bool SetKeyboardState(const KeyboardStatus &keyboard_state) = 0;

  // Injection point for GetKeyboardState API.
  virtual bool GetKeyboardState(KeyboardStatus *keyboard_state) = 0;

  // Injection point for GetAsyncKeyState API.
  virtual bool AsyncIsKeyPressed(int virtual_key) = 0;

  // Injection point for ToUnicode API.
  virtual int ToUnicode(
    __in UINT wVirtKey,
    __in UINT wScanCode,
    __in_bcount_opt(256) CONST BYTE *lpKeyState,
    __out_ecount(cchBuff) LPWSTR pwszBuff,
    __in int cchBuff,
    __in UINT wFlags) = 0;

  // Injection point for SendInput API.
  virtual UINT SendInput(const vector<INPUT> &inputs) = 0;

  static Win32KeyboardInterface *CreateDefault();
};

class JapaneseKeyboardLayoutEmulator {
 public:
  // This methods emulates ToUnicode API as if the current keyboard layout was
  // Japanese keyboard.  Currently this emulation ignores |scan_code|.
  static int ToUnicode(
    __in UINT virtual_key,
    __in UINT scan_code,
    __in_bcount_opt(256) CONST BYTE *key_state,
    __out_ecount(character_buffer_size) LPWSTR character_buffer,
    __in int character_buffer_size,
    __in UINT flags);

  // Returns generated character for Japanese keyboard layout based on the
  // given keyboard state.  Returns '\0' if no character is generated.
  // Note that built-in Japanese keyboard layout generates at most 1 character
  // for any key combination, and there is no key to generate '\0', as far as
  // we have observed with the built-in layout on Windows Vista.
  static wchar_t GetCharacterForKeyDown(
    BYTE virtual_key,
    const BYTE keyboard_state[256],
    bool is_menu_active);
 private:
  DISALLOW_COPY_AND_ASSIGN(JapaneseKeyboardLayoutEmulator);
};
}  // namespace win32
}  // namespace mozc
#endif  // MOZC_WIN32_IME_IME_KEYBOARD_
