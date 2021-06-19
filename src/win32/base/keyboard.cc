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

#include "win32/base/keyboard.h"

#include <memory>

#include "base/logging.h"

namespace mozc {
namespace win32 {

namespace {

using std::unique_ptr;

BYTE ParseVirtualKey(UINT combined_virtual_key) {
  const uint16 loword = LOWORD(combined_virtual_key);
  if (loword <= 0xff) {
    return loword;
  }
  DLOG(INFO) << "Unexpected VK found. VK = " << loword;
  return 0;
}

wchar_t ParseWideChar(UINT combined_virtual_key) {
  if (ParseVirtualKey(combined_virtual_key) == VK_PACKET) {
    return HIWORD(combined_virtual_key);
  } else {
    return 0;
  }
}

}  // namespace

LParamKeyInfo::LParamKeyInfo() : lparam_(0) {}

LParamKeyInfo::LParamKeyInfo(LPARAM lparam) : lparam_(lparam) {}

int LParamKeyInfo::GetKeyRepeatCount() const { return (lparam_ & 0xffff); }

BYTE LParamKeyInfo::GetScanCode() const {
  return static_cast<BYTE>((lparam_ >> 16) & 0xff);
}

bool LParamKeyInfo::IsExtendedKey() const {
  // http://msdn.microsoft.com/en-us/library/ms646267.aspx#_win32_Keystroke_Message_Flags
  return ((lparam_ >> 24) & 0x1) == 0x1;
}

bool LParamKeyInfo::HasContextCode() const {
  // http://msdn.microsoft.com/en-us/library/ms646267.aspx#_win32_Keystroke_Message_Flags
  return ((lparam_ >> 29) & 0x1) == 0x1;
}

bool LParamKeyInfo::IsPreviousStateDwon() const {
  // http://msdn.microsoft.com/en-us/library/ms646267.aspx#_win32_Keystroke_Message_Flags
  return ((lparam_ >> 30) & 0x1) == 0x1;
}

bool LParamKeyInfo::IsInTansitionState() const {
  // http://msdn.microsoft.com/en-us/library/ms646267.aspx#_win32_Keystroke_Message_Flags
  return ((lparam_ >> 31) & 0x1) == 0x1;
}

bool LParamKeyInfo::IsKeyDownInImeProcessKey() const {
  return ((lparam_ >> 31) & 0x1) == 0x0;
}

LPARAM LParamKeyInfo::lparam() const { return lparam_; }

VirtualKey::VirtualKey()
    : virtual_key_(0), wide_char_(L'\0'), unicode_char_(L'\0') {}

VirtualKey::VirtualKey(BYTE virtual_key, wchar_t wide_char, char32 unicode_char)
    : virtual_key_(virtual_key),
      wide_char_(wide_char),
      unicode_char_(unicode_char) {}

VirtualKey VirtualKey::FromVirtualKey(BYTE virtual_key) {
  return VirtualKey(virtual_key, L'\0', L'\0');
}

VirtualKey VirtualKey::FromCombinedVirtualKey(UINT combined_virtual_key) {
  const BYTE vk = ParseVirtualKey(combined_virtual_key);
  const wchar_t wchar = ParseWideChar(combined_virtual_key);
  char32 unicode_char = 0;
  if (!IS_HIGH_SURROGATE(wchar) && !IS_LOW_SURROGATE(wchar)) {
    unicode_char = wchar;
  }

  return VirtualKey(vk, wchar, unicode_char);
}

VirtualKey VirtualKey::FromUnicode(char32 unicode_char) {
  wchar_t wchar = L'\0';
  if (unicode_char <= 0xffff) {
    wchar = static_cast<wchar_t>(unicode_char);
  }
  return VirtualKey(VK_PACKET, wchar, unicode_char);
}

wchar_t VirtualKey::wide_char() const { return wide_char_; }

char32 VirtualKey::unicode_char() const { return unicode_char_; }

BYTE VirtualKey::virtual_key() const { return virtual_key_; }

class DefaultKeyboardInterface : public Win32KeyboardInterface {
 public:
  // [Overrides]
  virtual bool IsKanaLocked(const KeyboardStatus &keyboard_state) {
    return keyboard_state.IsToggled(VK_KANA);
  }

  // [Overrides]
  virtual bool SetKeyboardState(const KeyboardStatus &keyboard_state) {
    KeyboardStatus copy = keyboard_state;
    const bool result = (::SetKeyboardState(copy.mutable_status()) != FALSE);
    if (!result) {
      const int error = ::GetLastError();
      LOG(ERROR) << "SetKeyboardState failed. error = " << error;
    }
    return result;
  }

  // [Overrides]
  virtual bool GetKeyboardState(KeyboardStatus *keyboard_state) {
    if (keyboard_state == nullptr) {
      return false;
    }
    const bool result =
        (::GetKeyboardState(keyboard_state->mutable_status()) != FALSE);
    if (!result) {
      const int error = ::GetLastError();
      LOG(ERROR) << "GetKeyboardState failed. error = " << error;
    }
    return result;
  }

  // [Overrides]
  virtual bool AsyncIsKeyPressed(int virtual_key) {
    // The highest bit represents if the key is pressed or not.
    return ::GetAsyncKeyState(virtual_key) < 0;
  }

  // [Overrides]
  virtual int ToUnicode(__in UINT wVirtKey, __in UINT wScanCode,
                        __in_bcount_opt(256) CONST BYTE *lpKeyState,
                        __out_ecount(cchBuff) LPWSTR pwszBuff, __in int cchBuff,
                        __in UINT wFlags) {
    return ::ToUnicode(wVirtKey, wScanCode, lpKeyState, pwszBuff, cchBuff,
                       wFlags);
  }

  // [Overrides]
  virtual UINT SendInput(const std::vector<INPUT> &inputs) {
    if (inputs.size() < 1) {
      return 0;
    }

    // Unfortunately, SendInput API requires LPINPUT (NOT const INPUT *).
    // This is why we make a temporary array with the same data here.
    unique_ptr<INPUT[]> input_array(new INPUT[inputs.size()]);
    for (size_t i = 0; i < inputs.size(); ++i) {
      input_array[i] = inputs[i];
    }

    return ::SendInput(inputs.size(), input_array.get(), sizeof(INPUT));
  }
};

Win32KeyboardInterface *Win32KeyboardInterface::CreateDefault() {
  return new DefaultKeyboardInterface();
}

KeyboardStatus::KeyboardStatus() { memset(&status_[0], 0, arraysize(status_)); }

KeyboardStatus::KeyboardStatus(const BYTE key_status[256]) {
  const errno_t error =
      memcpy_s(&status_[0], arraysize(status_), key_status, arraysize(status_));
  if (error != NO_ERROR) {
    memset(&status_[0], 0, arraysize(status_));
  }
}

BYTE KeyboardStatus::GetState(int virtual_key) const {
  if (virtual_key < 0 || arraysize(status_) <= virtual_key) {
    DLOG(ERROR) << "index out of range. index = " << virtual_key;
    return 0;
  }
  return status_[virtual_key];
}

void KeyboardStatus::SetState(int virtual_key, BYTE value) {
  if (virtual_key < 0 || arraysize(status_) <= virtual_key) {
    DLOG(ERROR) << "index out of range. index = " << virtual_key;
    return;
  }
  status_[virtual_key] = value;
}

bool KeyboardStatus::IsToggled(int virtual_key) const {
  return (GetState(virtual_key) & 0x1) == 0x1;
}

bool KeyboardStatus::IsPressed(int virtual_key) const {
  return (GetState(virtual_key) & 0x80) == 0x80;
}

const BYTE *KeyboardStatus::status() const { return status_; }

BYTE *KeyboardStatus::mutable_status() { return status_; }

size_t KeyboardStatus::status_size() const { return arraysize(status_); }

namespace {
enum KeyModifierFlags {
  kShiftPressed = (1 << 0),
  kCtrlPressed = (1 << 1),
  kAltPressed = (1 << 2),
  kCapsLock = (1 << 3),
  kKanaLock = (1 << 4),
};

// Fallback table for keys which do not generate any printable characters.
const wchar_t kNoCharGenKey[] = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_CANCEL
const wchar_t kCharForVK_03[] = {
    0x0003, 0x0003, 0x0003, 0x0000, 0x0003, 0x0003, 0x0000, 0x0000,
    0x0003, 0x0003, 0x0003, 0x0000, 0x0003, 0x0003, 0x0000, 0x0000,
    0x0003, 0x0003, 0x0003, 0x0000, 0x0003, 0x0003, 0x0000, 0x0000,
    0x0003, 0x0003, 0x0003, 0x0000, 0x0003, 0x0003, 0x0000, 0x0000,
};

// VK_BACK
const wchar_t kCharForVK_08[] = {
    0x0008, 0x0008, 0x007f, 0x0000, 0x0008, 0x0008, 0x0000, 0x0000,
    0x0008, 0x0008, 0x007f, 0x0000, 0x0008, 0x0008, 0x0000, 0x0000,
    0x0008, 0x0008, 0x007f, 0x0000, 0x0008, 0x0008, 0x0000, 0x0000,
    0x0008, 0x0008, 0x007f, 0x0000, 0x0008, 0x0008, 0x0000, 0x0000,
};

// VK_TAB
const wchar_t kCharForVK_09[] = {
    0x0009, 0x0009, 0x0000, 0x0000, 0x0009, 0x0009, 0x0000, 0x0000,
    0x0009, 0x0009, 0x0000, 0x0000, 0x0009, 0x0009, 0x0000, 0x0000,
    0x0009, 0x0009, 0x0000, 0x0000, 0x0009, 0x0009, 0x0000, 0x0000,
    0x0009, 0x0009, 0x0000, 0x0000, 0x0009, 0x0009, 0x0000, 0x0000,
};

// VK_RETURN
const wchar_t kCharForVK_0D[] = {
    0x000d, 0x000d, 0x000a, 0x0000, 0x000d, 0x000d, 0x0000, 0x0000,
    0x000d, 0x000d, 0x000a, 0x0000, 0x000d, 0x000d, 0x0000, 0x0000,
    0x000d, 0x000d, 0x000a, 0x0000, 0x000d, 0x000d, 0x0000, 0x0000,
    0x000d, 0x000d, 0x000a, 0x0000, 0x000d, 0x000d, 0x0000, 0x0000,
};

// VK_ESCAPE
const wchar_t kCharForVK_1B[] = {
    0x001b, 0x001b, 0x001b, 0x0000, 0x001b, 0x001b, 0x0000, 0x0000,
    0x001b, 0x001b, 0x001b, 0x0000, 0x001b, 0x001b, 0x0000, 0x0000,
    0x001b, 0x001b, 0x001b, 0x0000, 0x001b, 0x001b, 0x0000, 0x0000,
    0x001b, 0x001b, 0x001b, 0x0000, 0x001b, 0x001b, 0x0000, 0x0000,
};

// VK_SPACE
const wchar_t kCharForVK_20[] = {
    0x0020, 0x0020, 0x0020, 0x0000, 0x0020, 0x0020, 0x0000, 0x0000,
    0x0020, 0x0020, 0x0020, 0x0000, 0x0020, 0x0020, 0x0000, 0x0000,
    0x0020, 0x0020, 0x0020, 0x0000, 0x0020, 0x0020, 0x0000, 0x0000,
    0x0020, 0x0020, 0x0020, 0x0000, 0x0020, 0x0020, 0x0000, 0x0000,
};

// VK_0
const wchar_t kCharForVK_30[] = {
    0x0030, 0x0000, 0x0000, 0x0000, 0x0030, 0x0000, 0x0000, 0x0000,
    0x0030, 0x0000, 0x0000, 0x0000, 0x0030, 0x0000, 0x0000, 0x0000,
    0xff9c, 0xff66, 0x0000, 0x0000, 0xff9c, 0xff66, 0x0000, 0x0000,
    0xff9c, 0xff66, 0x0000, 0x0000, 0xff9c, 0xff66, 0x0000, 0x0000,
};

// VK_1
const wchar_t kCharForVK_31[] = {
    0x0031, 0x0021, 0x0000, 0x0000, 0x0031, 0x0021, 0x0000, 0x0000,
    0x0031, 0x0021, 0x0000, 0x0000, 0x0031, 0x0021, 0x0000, 0x0000,
    0xff87, 0xff87, 0x0000, 0x0000, 0xff87, 0xff87, 0x0000, 0x0000,
    0xff87, 0xff87, 0x0000, 0x0000, 0xff87, 0xff87, 0x0000, 0x0000,
};

// VK_2
const wchar_t kCharForVK_32[] = {
    0x0032, 0x0022, 0x0000, 0x0000, 0x0032, 0x0022, 0x0000, 0x0000,
    0x0032, 0x0022, 0x0000, 0x0000, 0x0032, 0x0022, 0x0000, 0x0000,
    0xff8c, 0xff8c, 0x0000, 0x0000, 0xff8c, 0xff8c, 0x0000, 0x0000,
    0xff8c, 0xff8c, 0x0000, 0x0000, 0xff8c, 0xff8c, 0x0000, 0x0000,
};

// VK_3
const wchar_t kCharForVK_33[] = {
    0x0033, 0x0023, 0x0000, 0x0000, 0x0033, 0x0023, 0x0000, 0x0000,
    0x0033, 0x0023, 0x0000, 0x0000, 0x0033, 0x0023, 0x0000, 0x0000,
    0xff71, 0xff67, 0x0000, 0x0000, 0xff71, 0xff67, 0x0000, 0x0000,
    0xff71, 0xff67, 0x0000, 0x0000, 0xff71, 0xff67, 0x0000, 0x0000,
};

// VK_4
const wchar_t kCharForVK_34[] = {
    0x0034, 0x0024, 0x0000, 0x0000, 0x0034, 0x0024, 0x0000, 0x0000,
    0x0034, 0x0024, 0x0000, 0x0000, 0x0034, 0x0024, 0x0000, 0x0000,
    0xff73, 0xff69, 0x0000, 0x0000, 0xff73, 0xff69, 0x0000, 0x0000,
    0xff73, 0xff69, 0x0000, 0x0000, 0xff73, 0xff69, 0x0000, 0x0000,
};

// VK_5
const wchar_t kCharForVK_35[] = {
    0x0035, 0x0025, 0x0000, 0x0000, 0x0035, 0x0025, 0x0000, 0x0000,
    0x0035, 0x0025, 0x0000, 0x0000, 0x0035, 0x0025, 0x0000, 0x0000,
    0xff74, 0xff6a, 0x0000, 0x0000, 0xff74, 0xff6a, 0x0000, 0x0000,
    0xff74, 0xff6a, 0x0000, 0x0000, 0xff74, 0xff6a, 0x0000, 0x0000,
};

// VK_6
const wchar_t kCharForVK_36[] = {
    0x0036, 0x0026, 0x0000, 0x001e, 0x0036, 0x0026, 0x0000, 0x0000,
    0x0036, 0x0026, 0x0000, 0x001e, 0x0036, 0x0026, 0x0000, 0x0000,
    0xff75, 0xff6b, 0x0000, 0x001e, 0xff75, 0xff6b, 0x0000, 0x0000,
    0xff75, 0xff6b, 0x0000, 0x001e, 0xff75, 0xff6b, 0x0000, 0x0000,
};

// VK_7
const wchar_t kCharForVK_37[] = {
    0x0037, 0x0027, 0x0000, 0x0000, 0x0037, 0x0027, 0x0000, 0x0000,
    0x0037, 0x0027, 0x0000, 0x0000, 0x0037, 0x0027, 0x0000, 0x0000,
    0xff94, 0xff6c, 0x0000, 0x0000, 0xff94, 0xff6c, 0x0000, 0x0000,
    0xff94, 0xff6c, 0x0000, 0x0000, 0xff94, 0xff6c, 0x0000, 0x0000,
};

// VK_8
const wchar_t kCharForVK_38[] = {
    0x0038, 0x0028, 0x0000, 0x0000, 0x0038, 0x0028, 0x0000, 0x0000,
    0x0038, 0x0028, 0x0000, 0x0000, 0x0038, 0x0028, 0x0000, 0x0000,
    0xff95, 0xff6d, 0x0000, 0x0000, 0xff95, 0xff6d, 0x0000, 0x0000,
    0xff95, 0xff6d, 0x0000, 0x0000, 0xff95, 0xff6d, 0x0000, 0x0000,
};

// VK_9
const wchar_t kCharForVK_39[] = {
    0x0039, 0x0029, 0x0000, 0x0000, 0x0039, 0x0029, 0x0000, 0x0000,
    0x0039, 0x0029, 0x0000, 0x0000, 0x0039, 0x0029, 0x0000, 0x0000,
    0xff96, 0xff6e, 0x0000, 0x0000, 0xff96, 0xff6e, 0x0000, 0x0000,
    0xff96, 0xff6e, 0x0000, 0x0000, 0xff96, 0xff6e, 0x0000, 0x0000,
};

// VK_A
const wchar_t kCharForVK_41[] = {
    0x0061, 0x0041, 0x0001, 0x0001, 0x0061, 0x0041, 0x0000, 0x0000,
    0x0041, 0x0061, 0x0001, 0x0001, 0x0041, 0x0061, 0x0000, 0x0000,
    0xff81, 0xff81, 0x0001, 0x0001, 0xff81, 0xff81, 0x0000, 0x0000,
    0xff81, 0xff81, 0x0001, 0x0001, 0xff81, 0xff81, 0x0000, 0x0000,
};

// VK_B
const wchar_t kCharForVK_42[] = {
    0x0062, 0x0042, 0x0002, 0x0002, 0x0062, 0x0042, 0x0000, 0x0000,
    0x0042, 0x0062, 0x0002, 0x0002, 0x0042, 0x0062, 0x0000, 0x0000,
    0xff7a, 0xff7a, 0x0002, 0x0002, 0xff7a, 0xff7a, 0x0000, 0x0000,
    0xff7a, 0xff7a, 0x0002, 0x0002, 0xff7a, 0xff7a, 0x0000, 0x0000,
};

// VK_C
const wchar_t kCharForVK_43[] = {
    0x0063, 0x0043, 0x0003, 0x0003, 0x0063, 0x0043, 0x0000, 0x0000,
    0x0043, 0x0063, 0x0003, 0x0003, 0x0043, 0x0063, 0x0000, 0x0000,
    0xff7f, 0xff7f, 0x0003, 0x0003, 0xff7f, 0xff7f, 0x0000, 0x0000,
    0xff7f, 0xff7f, 0x0003, 0x0003, 0xff7f, 0xff7f, 0x0000, 0x0000,
};

// VK_D
const wchar_t kCharForVK_44[] = {
    0x0064, 0x0044, 0x0004, 0x0004, 0x0064, 0x0044, 0x0000, 0x0000,
    0x0044, 0x0064, 0x0004, 0x0004, 0x0044, 0x0064, 0x0000, 0x0000,
    0xff7c, 0xff7c, 0x0004, 0x0004, 0xff7c, 0xff7c, 0x0000, 0x0000,
    0xff7c, 0xff7c, 0x0004, 0x0004, 0xff7c, 0xff7c, 0x0000, 0x0000,
};

// VK_E
const wchar_t kCharForVK_45[] = {
    0x0065, 0x0045, 0x0005, 0x0005, 0x0065, 0x0045, 0x0000, 0x0000,
    0x0045, 0x0065, 0x0005, 0x0005, 0x0045, 0x0065, 0x0000, 0x0000,
    0xff72, 0xff68, 0x0005, 0x0005, 0xff72, 0xff68, 0x0000, 0x0000,
    0xff72, 0xff68, 0x0005, 0x0005, 0xff72, 0xff68, 0x0000, 0x0000,
};

// VK_F
const wchar_t kCharForVK_46[] = {
    0x0066, 0x0046, 0x0006, 0x0006, 0x0066, 0x0046, 0x0000, 0x0000,
    0x0046, 0x0066, 0x0006, 0x0006, 0x0046, 0x0066, 0x0000, 0x0000,
    0xff8a, 0xff8a, 0x0006, 0x0006, 0xff8a, 0xff8a, 0x0000, 0x0000,
    0xff8a, 0xff8a, 0x0006, 0x0006, 0xff8a, 0xff8a, 0x0000, 0x0000,
};

// VK_G
const wchar_t kCharForVK_47[] = {
    0x0067, 0x0047, 0x0007, 0x0007, 0x0067, 0x0047, 0x0000, 0x0000,
    0x0047, 0x0067, 0x0007, 0x0007, 0x0047, 0x0067, 0x0000, 0x0000,
    0xff77, 0xff77, 0x0007, 0x0007, 0xff77, 0xff77, 0x0000, 0x0000,
    0xff77, 0xff77, 0x0007, 0x0007, 0xff77, 0xff77, 0x0000, 0x0000,
};

// VK_H
const wchar_t kCharForVK_48[] = {
    0x0068, 0x0048, 0x0008, 0x0008, 0x0068, 0x0048, 0x0000, 0x0000,
    0x0048, 0x0068, 0x0008, 0x0008, 0x0048, 0x0068, 0x0000, 0x0000,
    0xff78, 0xff78, 0x0008, 0x0008, 0xff78, 0xff78, 0x0000, 0x0000,
    0xff78, 0xff78, 0x0008, 0x0008, 0xff78, 0xff78, 0x0000, 0x0000,
};

// VK_I
const wchar_t kCharForVK_49[] = {
    0x0069, 0x0049, 0x0009, 0x0009, 0x0069, 0x0049, 0x0000, 0x0000,
    0x0049, 0x0069, 0x0009, 0x0009, 0x0049, 0x0069, 0x0000, 0x0000,
    0xff86, 0xff86, 0x0009, 0x0009, 0xff86, 0xff86, 0x0000, 0x0000,
    0xff86, 0xff86, 0x0009, 0x0009, 0xff86, 0xff86, 0x0000, 0x0000,
};

// VK_J
const wchar_t kCharForVK_4A[] = {
    0x006a, 0x004a, 0x000a, 0x000a, 0x006a, 0x004a, 0x0000, 0x0000,
    0x004a, 0x006a, 0x000a, 0x000a, 0x004a, 0x006a, 0x0000, 0x0000,
    0xff8f, 0xff8f, 0x000a, 0x000a, 0xff8f, 0xff8f, 0x0000, 0x0000,
    0xff8f, 0xff8f, 0x000a, 0x000a, 0xff8f, 0xff8f, 0x0000, 0x0000,
};

// VK_K
const wchar_t kCharForVK_4B[] = {
    0x006b, 0x004b, 0x000b, 0x000b, 0x006b, 0x004b, 0x0000, 0x0000,
    0x004b, 0x006b, 0x000b, 0x000b, 0x004b, 0x006b, 0x0000, 0x0000,
    0xff89, 0xff89, 0x000b, 0x000b, 0xff89, 0xff89, 0x0000, 0x0000,
    0xff89, 0xff89, 0x000b, 0x000b, 0xff89, 0xff89, 0x0000, 0x0000,
};

// VK_L
const wchar_t kCharForVK_4C[] = {
    0x006c, 0x004c, 0x000c, 0x000c, 0x006c, 0x004c, 0x0000, 0x0000,
    0x004c, 0x006c, 0x000c, 0x000c, 0x004c, 0x006c, 0x0000, 0x0000,
    0xff98, 0xff98, 0x000c, 0x000c, 0xff98, 0xff98, 0x0000, 0x0000,
    0xff98, 0xff98, 0x000c, 0x000c, 0xff98, 0xff98, 0x0000, 0x0000,
};

// VK_M
const wchar_t kCharForVK_4D[] = {
    0x006d, 0x004d, 0x000d, 0x000d, 0x006d, 0x004d, 0x0000, 0x0000,
    0x004d, 0x006d, 0x000d, 0x000d, 0x004d, 0x006d, 0x0000, 0x0000,
    0xff93, 0xff93, 0x000d, 0x000d, 0xff93, 0xff93, 0x0000, 0x0000,
    0xff93, 0xff93, 0x000d, 0x000d, 0xff93, 0xff93, 0x0000, 0x0000,
};

// VK_N
const wchar_t kCharForVK_4E[] = {
    0x006e, 0x004e, 0x000e, 0x000e, 0x006e, 0x004e, 0x0000, 0x0000,
    0x004e, 0x006e, 0x000e, 0x000e, 0x004e, 0x006e, 0x0000, 0x0000,
    0xff90, 0xff90, 0x000e, 0x000e, 0xff90, 0xff90, 0x0000, 0x0000,
    0xff90, 0xff90, 0x000e, 0x000e, 0xff90, 0xff90, 0x0000, 0x0000,
};

// VK_O
const wchar_t kCharForVK_4F[] = {
    0x006f, 0x004f, 0x000f, 0x000f, 0x006f, 0x004f, 0x0000, 0x0000,
    0x004f, 0x006f, 0x000f, 0x000f, 0x004f, 0x006f, 0x0000, 0x0000,
    0xff97, 0xff97, 0x000f, 0x000f, 0xff97, 0xff97, 0x0000, 0x0000,
    0xff97, 0xff97, 0x000f, 0x000f, 0xff97, 0xff97, 0x0000, 0x0000,
};

// VK_P
const wchar_t kCharForVK_50[] = {
    0x0070, 0x0050, 0x0010, 0x0010, 0x0070, 0x0050, 0x0000, 0x0000,
    0x0050, 0x0070, 0x0010, 0x0010, 0x0050, 0x0070, 0x0000, 0x0000,
    0xff7e, 0xff7e, 0x0010, 0x0010, 0xff7e, 0xff7e, 0x0000, 0x0000,
    0xff7e, 0xff7e, 0x0010, 0x0010, 0xff7e, 0xff7e, 0x0000, 0x0000,
};

// VK_Q
const wchar_t kCharForVK_51[] = {
    0x0071, 0x0051, 0x0011, 0x0011, 0x0071, 0x0051, 0x0000, 0x0000,
    0x0051, 0x0071, 0x0011, 0x0011, 0x0051, 0x0071, 0x0000, 0x0000,
    0xff80, 0xff80, 0x0011, 0x0011, 0xff80, 0xff80, 0x0000, 0x0000,
    0xff80, 0xff80, 0x0011, 0x0011, 0xff80, 0xff80, 0x0000, 0x0000,
};

// VK_R
const wchar_t kCharForVK_52[] = {
    0x0072, 0x0052, 0x0012, 0x0012, 0x0072, 0x0052, 0x0000, 0x0000,
    0x0052, 0x0072, 0x0012, 0x0012, 0x0052, 0x0072, 0x0000, 0x0000,
    0xff7d, 0xff7d, 0x0012, 0x0012, 0xff7d, 0xff7d, 0x0000, 0x0000,
    0xff7d, 0xff7d, 0x0012, 0x0012, 0xff7d, 0xff7d, 0x0000, 0x0000,
};

// VK_S
const wchar_t kCharForVK_53[] = {
    0x0073, 0x0053, 0x0013, 0x0013, 0x0073, 0x0053, 0x0000, 0x0000,
    0x0053, 0x0073, 0x0013, 0x0013, 0x0053, 0x0073, 0x0000, 0x0000,
    0xff84, 0xff84, 0x0013, 0x0013, 0xff84, 0xff84, 0x0000, 0x0000,
    0xff84, 0xff84, 0x0013, 0x0013, 0xff84, 0xff84, 0x0000, 0x0000,
};

// VK_T
const wchar_t kCharForVK_54[] = {
    0x0074, 0x0054, 0x0014, 0x0014, 0x0074, 0x0054, 0x0000, 0x0000,
    0x0054, 0x0074, 0x0014, 0x0014, 0x0054, 0x0074, 0x0000, 0x0000,
    0xff76, 0xff76, 0x0014, 0x0014, 0xff76, 0xff76, 0x0000, 0x0000,
    0xff76, 0xff76, 0x0014, 0x0014, 0xff76, 0xff76, 0x0000, 0x0000,
};

// VK_U
const wchar_t kCharForVK_55[] = {
    0x0075, 0x0055, 0x0015, 0x0015, 0x0075, 0x0055, 0x0000, 0x0000,
    0x0055, 0x0075, 0x0015, 0x0015, 0x0055, 0x0075, 0x0000, 0x0000,
    0xff85, 0xff85, 0x0015, 0x0015, 0xff85, 0xff85, 0x0000, 0x0000,
    0xff85, 0xff85, 0x0015, 0x0015, 0xff85, 0xff85, 0x0000, 0x0000,
};

// VK_V
const wchar_t kCharForVK_56[] = {
    0x0076, 0x0056, 0x0016, 0x0016, 0x0076, 0x0056, 0x0000, 0x0000,
    0x0056, 0x0076, 0x0016, 0x0016, 0x0056, 0x0076, 0x0000, 0x0000,
    0xff8b, 0xff8b, 0x0016, 0x0016, 0xff8b, 0xff8b, 0x0000, 0x0000,
    0xff8b, 0xff8b, 0x0016, 0x0016, 0xff8b, 0xff8b, 0x0000, 0x0000,
};

// VK_W
const wchar_t kCharForVK_57[] = {
    0x0077, 0x0057, 0x0017, 0x0017, 0x0077, 0x0057, 0x0000, 0x0000,
    0x0057, 0x0077, 0x0017, 0x0017, 0x0057, 0x0077, 0x0000, 0x0000,
    0xff83, 0xff83, 0x0017, 0x0017, 0xff83, 0xff83, 0x0000, 0x0000,
    0xff83, 0xff83, 0x0017, 0x0017, 0xff83, 0xff83, 0x0000, 0x0000,
};

// VK_X
const wchar_t kCharForVK_58[] = {
    0x0078, 0x0058, 0x0018, 0x0018, 0x0078, 0x0058, 0x0000, 0x0000,
    0x0058, 0x0078, 0x0018, 0x0018, 0x0058, 0x0078, 0x0000, 0x0000,
    0xff7b, 0xff7b, 0x0018, 0x0018, 0xff7b, 0xff7b, 0x0000, 0x0000,
    0xff7b, 0xff7b, 0x0018, 0x0018, 0xff7b, 0xff7b, 0x0000, 0x0000,
};

// VK_Y
const wchar_t kCharForVK_59[] = {
    0x0079, 0x0059, 0x0019, 0x0019, 0x0079, 0x0059, 0x0000, 0x0000,
    0x0059, 0x0079, 0x0019, 0x0019, 0x0059, 0x0079, 0x0000, 0x0000,
    0xff9d, 0xff9d, 0x0019, 0x0019, 0xff9d, 0xff9d, 0x0000, 0x0000,
    0xff9d, 0xff9d, 0x0019, 0x0019, 0xff9d, 0xff9d, 0x0000, 0x0000,
};

// VK_Z
const wchar_t kCharForVK_5A[] = {
    0x007a, 0x005a, 0x001a, 0x001a, 0x007a, 0x005a, 0x0000, 0x0000,
    0x005a, 0x007a, 0x001a, 0x001a, 0x005a, 0x007a, 0x0000, 0x0000,
    0xff82, 0xff6f, 0x001a, 0x001a, 0xff82, 0xff6f, 0x0000, 0x0000,
    0xff82, 0xff6f, 0x001a, 0x001a, 0xff82, 0xff6f, 0x0000, 0x0000,
};

// VK_NUMPAD0
const wchar_t kCharForVK_60[] = {
    0x0030, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0030, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0030, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0030, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD0 (When menu is active)
const wchar_t kCharForVK_60_MenuActive[] = {
    0x0030, 0x0000, 0x0000, 0x0000, 0x0030, 0x0000, 0x0000, 0x0000,
    0x0030, 0x0000, 0x0000, 0x0000, 0x0030, 0x0000, 0x0000, 0x0000,
    0x0030, 0x0000, 0x0000, 0x0000, 0x0030, 0x0000, 0x0000, 0x0000,
    0x0030, 0x0000, 0x0000, 0x0000, 0x0030, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD1
const wchar_t kCharForVK_61[] = {
    0x0031, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0031, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0031, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0031, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD1 (When menu is active)
const wchar_t kCharForVK_61_MenuActive[] = {
    0x0031, 0x0000, 0x0000, 0x0000, 0x0031, 0x0000, 0x0000, 0x0000,
    0x0031, 0x0000, 0x0000, 0x0000, 0x0031, 0x0000, 0x0000, 0x0000,
    0x0031, 0x0000, 0x0000, 0x0000, 0x0031, 0x0000, 0x0000, 0x0000,
    0x0031, 0x0000, 0x0000, 0x0000, 0x0031, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD2
const wchar_t kCharForVK_62[] = {
    0x0032, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0032, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0032, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0032, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD2 (When menu is active)
const wchar_t kCharForVK_62_MenuActive[] = {
    0x0032, 0x0000, 0x0000, 0x0000, 0x0032, 0x0000, 0x0000, 0x0000,
    0x0032, 0x0000, 0x0000, 0x0000, 0x0032, 0x0000, 0x0000, 0x0000,
    0x0032, 0x0000, 0x0000, 0x0000, 0x0032, 0x0000, 0x0000, 0x0000,
    0x0032, 0x0000, 0x0000, 0x0000, 0x0032, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD3
const wchar_t kCharForVK_63[] = {
    0x0033, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0033, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0033, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0033, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD3 (When menu is active)
const wchar_t kCharForVK_63_MenuActive[] = {
    0x0033, 0x0000, 0x0000, 0x0000, 0x0033, 0x0000, 0x0000, 0x0000,
    0x0033, 0x0000, 0x0000, 0x0000, 0x0033, 0x0000, 0x0000, 0x0000,
    0x0033, 0x0000, 0x0000, 0x0000, 0x0033, 0x0000, 0x0000, 0x0000,
    0x0033, 0x0000, 0x0000, 0x0000, 0x0033, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD4
const wchar_t kCharForVK_64[] = {
    0x0034, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0034, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0034, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0034, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD4 (When menu is active)
const wchar_t kCharForVK_64_MenuActive[] = {
    0x0034, 0x0000, 0x0000, 0x0000, 0x0034, 0x0000, 0x0000, 0x0000,
    0x0034, 0x0000, 0x0000, 0x0000, 0x0034, 0x0000, 0x0000, 0x0000,
    0x0034, 0x0000, 0x0000, 0x0000, 0x0034, 0x0000, 0x0000, 0x0000,
    0x0034, 0x0000, 0x0000, 0x0000, 0x0034, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD5
const wchar_t kCharForVK_65[] = {
    0x0035, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0035, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0035, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0035, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD5 (When menu is active)
const wchar_t kCharForVK_65_MenuActive[] = {
    0x0035, 0x0000, 0x0000, 0x0000, 0x0035, 0x0000, 0x0000, 0x0000,
    0x0035, 0x0000, 0x0000, 0x0000, 0x0035, 0x0000, 0x0000, 0x0000,
    0x0035, 0x0000, 0x0000, 0x0000, 0x0035, 0x0000, 0x0000, 0x0000,
    0x0035, 0x0000, 0x0000, 0x0000, 0x0035, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD6
const wchar_t kCharForVK_66[] = {
    0x0036, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0036, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0036, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0036, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD6 (When menu is active)
const wchar_t kCharForVK_66_MenuActive[] = {
    0x0036, 0x0000, 0x0000, 0x0000, 0x0036, 0x0000, 0x0000, 0x0000,
    0x0036, 0x0000, 0x0000, 0x0000, 0x0036, 0x0000, 0x0000, 0x0000,
    0x0036, 0x0000, 0x0000, 0x0000, 0x0036, 0x0000, 0x0000, 0x0000,
    0x0036, 0x0000, 0x0000, 0x0000, 0x0036, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD7
const wchar_t kCharForVK_67[] = {
    0x0037, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0037, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0037, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0037, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD7 (When menu is active)
const wchar_t kCharForVK_67_MenuActive[] = {
    0x0037, 0x0000, 0x0000, 0x0000, 0x0037, 0x0000, 0x0000, 0x0000,
    0x0037, 0x0000, 0x0000, 0x0000, 0x0037, 0x0000, 0x0000, 0x0000,
    0x0037, 0x0000, 0x0000, 0x0000, 0x0037, 0x0000, 0x0000, 0x0000,
    0x0037, 0x0000, 0x0000, 0x0000, 0x0037, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD8
const wchar_t kCharForVK_68[] = {
    0x0038, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0038, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0038, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0038, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD8 (When menu is active)
const wchar_t kCharForVK_68_MenuActive[] = {
    0x0038, 0x0000, 0x0000, 0x0000, 0x0038, 0x0000, 0x0000, 0x0000,
    0x0038, 0x0000, 0x0000, 0x0000, 0x0038, 0x0000, 0x0000, 0x0000,
    0x0038, 0x0000, 0x0000, 0x0000, 0x0038, 0x0000, 0x0000, 0x0000,
    0x0038, 0x0000, 0x0000, 0x0000, 0x0038, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD9
const wchar_t kCharForVK_69[] = {
    0x0039, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0039, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0039, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0039, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
};

// VK_NUMPAD9 (When menu is active)
const wchar_t kCharForVK_69_MenuActive[] = {
    0x0039, 0x0000, 0x0000, 0x0000, 0x0039, 0x0000, 0x0000, 0x0000,
    0x0039, 0x0000, 0x0000, 0x0000, 0x0039, 0x0000, 0x0000, 0x0000,
    0x0039, 0x0000, 0x0000, 0x0000, 0x0039, 0x0000, 0x0000, 0x0000,
    0x0039, 0x0000, 0x0000, 0x0000, 0x0039, 0x0000, 0x0000, 0x0000,
};

// VK_MULTIPLY
const wchar_t kCharForVK_6A[] = {
    0x002a, 0x002a, 0x0000, 0x0000, 0x002a, 0x002a, 0x0000, 0x0000,
    0x002a, 0x002a, 0x0000, 0x0000, 0x002a, 0x002a, 0x0000, 0x0000,
    0x002a, 0x002a, 0x0000, 0x0000, 0x002a, 0x002a, 0x0000, 0x0000,
    0x002a, 0x002a, 0x0000, 0x0000, 0x002a, 0x002a, 0x0000, 0x0000,
};

// VK_ADD
const wchar_t kCharForVK_6B[] = {
    0x002b, 0x002b, 0x0000, 0x0000, 0x002b, 0x002b, 0x0000, 0x0000,
    0x002b, 0x002b, 0x0000, 0x0000, 0x002b, 0x002b, 0x0000, 0x0000,
    0x002b, 0x002b, 0x0000, 0x0000, 0x002b, 0x002b, 0x0000, 0x0000,
    0x002b, 0x002b, 0x0000, 0x0000, 0x002b, 0x002b, 0x0000, 0x0000,
};

// VK_SUBTRACT
const wchar_t kCharForVK_6D[] = {
    0x002d, 0x002d, 0x0000, 0x0000, 0x002d, 0x002d, 0x0000, 0x0000,
    0x002d, 0x002d, 0x0000, 0x0000, 0x002d, 0x002d, 0x0000, 0x0000,
    0x002d, 0x002d, 0x0000, 0x0000, 0x002d, 0x002d, 0x0000, 0x0000,
    0x002d, 0x002d, 0x0000, 0x0000, 0x002d, 0x002d, 0x0000, 0x0000,
};

// VK_DECIMAL
const wchar_t kCharForVK_6E[] = {
    0x002e, 0x002e, 0x0000, 0x0000, 0x002e, 0x002e, 0x0000, 0x0000,
    0x002e, 0x002e, 0x0000, 0x0000, 0x002e, 0x002e, 0x0000, 0x0000,
    0x002e, 0x002e, 0x0000, 0x0000, 0x002e, 0x002e, 0x0000, 0x0000,
    0x002e, 0x002e, 0x0000, 0x0000, 0x002e, 0x002e, 0x0000, 0x0000,
};

// VK_DIVIDE
const wchar_t kCharForVK_6F[] = {
    0x002f, 0x002f, 0x0000, 0x0000, 0x002f, 0x002f, 0x0000, 0x0000,
    0x002f, 0x002f, 0x0000, 0x0000, 0x002f, 0x002f, 0x0000, 0x0000,
    0x002f, 0x002f, 0x0000, 0x0000, 0x002f, 0x002f, 0x0000, 0x0000,
    0x002f, 0x002f, 0x0000, 0x0000, 0x002f, 0x002f, 0x0000, 0x0000,
};

// VK_OEM_1
const wchar_t kCharForVK_BA[] = {
    0x003a, 0x002a, 0x0000, 0x0000, 0x003a, 0x002a, 0x0000, 0x0000,
    0x003a, 0x002a, 0x0000, 0x0000, 0x003a, 0x002a, 0x0000, 0x0000,
    0xff79, 0xff79, 0x0000, 0x0000, 0xff79, 0xff79, 0x0000, 0x0000,
    0xff79, 0xff79, 0x0000, 0x0000, 0xff79, 0xff79, 0x0000, 0x0000,
};

// VK_OEM_PLUS
const wchar_t kCharForVK_BB[] = {
    0x003b, 0x002b, 0x0000, 0x0000, 0x003b, 0x002b, 0x0000, 0x0000,
    0x003b, 0x002b, 0x0000, 0x0000, 0x003b, 0x002b, 0x0000, 0x0000,
    0xff9a, 0xff9a, 0x0000, 0x0000, 0xff9a, 0xff9a, 0x0000, 0x0000,
    0xff9a, 0xff9a, 0x0000, 0x0000, 0xff9a, 0xff9a, 0x0000, 0x0000,
};

// VK_OEM_COMMA
const wchar_t kCharForVK_BC[] = {
    0x002c, 0x003c, 0x0000, 0x0000, 0x002c, 0x003c, 0x0000, 0x0000,
    0x002c, 0x003c, 0x0000, 0x0000, 0x002c, 0x003c, 0x0000, 0x0000,
    0xff88, 0xff64, 0x0000, 0x0000, 0xff88, 0xff64, 0x0000, 0x0000,
    0xff88, 0xff64, 0x0000, 0x0000, 0xff88, 0xff64, 0x0000, 0x0000,
};

// VK_OEM_MINUS
const wchar_t kCharForVK_BD[] = {
    0x002d, 0x003d, 0x0000, 0x001f, 0x002d, 0x003d, 0x0000, 0x0000,
    0x002d, 0x003d, 0x0000, 0x001f, 0x002d, 0x003d, 0x0000, 0x0000,
    0xff8e, 0xff8e, 0x0000, 0x001f, 0xff8e, 0xff8e, 0x0000, 0x0000,
    0xff8e, 0xff8e, 0x0000, 0x001f, 0xff8e, 0xff8e, 0x0000, 0x0000,
};

// VK_OEM_PERIOD
const wchar_t kCharForVK_BE[] = {
    0x002e, 0x003e, 0x0000, 0x0000, 0x002e, 0x003e, 0x0000, 0x0000,
    0x002e, 0x003e, 0x0000, 0x0000, 0x002e, 0x003e, 0x0000, 0x0000,
    0xff99, 0xff61, 0x0000, 0x0000, 0xff99, 0xff61, 0x0000, 0x0000,
    0xff99, 0xff61, 0x0000, 0x0000, 0xff99, 0xff61, 0x0000, 0x0000,
};

// VK_OEM_2
const wchar_t kCharForVK_BF[] = {
    0x002f, 0x003f, 0x0000, 0x0000, 0x002f, 0x003f, 0x0000, 0x0000,
    0x002f, 0x003f, 0x0000, 0x0000, 0x002f, 0x003f, 0x0000, 0x0000,
    0xff92, 0xff65, 0x0000, 0x0000, 0xff92, 0xff65, 0x0000, 0x0000,
    0xff92, 0xff65, 0x0000, 0x0000, 0xff92, 0xff65, 0x0000, 0x0000,
};

// VK_OEM_3
const wchar_t kCharForVK_C0[] = {
    0x0040, 0x0060, 0x0000, 0x0000, 0x0040, 0x0060, 0x0000, 0x0000,
    0x0040, 0x0060, 0x0000, 0x0000, 0x0040, 0x0060, 0x0000, 0x0000,
    0xff9e, 0xff9e, 0x0000, 0x0000, 0xff9e, 0xff9e, 0x0000, 0x0000,
    0xff9e, 0xff9e, 0x0000, 0x0000, 0xff9e, 0xff9e, 0x0000, 0x0000,
};

// VK_OEM_4
const wchar_t kCharForVK_DB[] = {
    0x005b, 0x007b, 0x001b, 0x0000, 0x005b, 0x007b, 0x0000, 0x0000,
    0x005b, 0x007b, 0x001b, 0x0000, 0x005b, 0x007b, 0x0000, 0x0000,
    0xff9f, 0xff62, 0x001b, 0x0000, 0xff9f, 0xff62, 0x0000, 0x0000,
    0xff9f, 0xff62, 0x001b, 0x0000, 0xff9f, 0xff62, 0x0000, 0x0000,
};

// VK_OEM_5
const wchar_t kCharForVK_DC[] = {
    0x005c, 0x007c, 0x001c, 0x0000, 0x005c, 0x007c, 0x0000, 0x0000,
    0x005c, 0x007c, 0x001c, 0x0000, 0x005c, 0x007c, 0x0000, 0x0000,
    0xff70, 0xff70, 0x001c, 0x0000, 0xff70, 0xff70, 0x0000, 0x0000,
    0xff70, 0xff70, 0x001c, 0x0000, 0xff70, 0xff70, 0x0000, 0x0000,
};

// VK_OEM_6
const wchar_t kCharForVK_DD[] = {
    0x005d, 0x007d, 0x001d, 0x0000, 0x005d, 0x007d, 0x0000, 0x0000,
    0x005d, 0x007d, 0x001d, 0x0000, 0x005d, 0x007d, 0x0000, 0x0000,
    0xff91, 0xff63, 0x001d, 0x0000, 0xff91, 0xff63, 0x0000, 0x0000,
    0xff91, 0xff63, 0x001d, 0x0000, 0xff91, 0xff63, 0x0000, 0x0000,
};

// VK_OEM_7
const wchar_t kCharForVK_DE[] = {
    0x005e, 0x007e, 0x0000, 0x0000, 0x005e, 0x007e, 0x0000, 0x0000,
    0x005e, 0x007e, 0x0000, 0x0000, 0x005e, 0x007e, 0x0000, 0x0000,
    0xff8d, 0xff8d, 0x0000, 0x0000, 0xff8d, 0xff8d, 0x0000, 0x0000,
    0xff8d, 0xff8d, 0x0000, 0x0000, 0xff8d, 0xff8d, 0x0000, 0x0000,
};

// VK_OEM_102
const wchar_t kCharForVK_E2[] = {
    0x005c, 0x005f, 0x001c, 0x0000, 0x005c, 0x005f, 0x0000, 0x0000,
    0x005c, 0x005f, 0x001c, 0x0000, 0x005c, 0x005f, 0x0000, 0x0000,
    0xff9b, 0xff9b, 0x001c, 0x0000, 0xff9b, 0xff9b, 0x0000, 0x0000,
    0xff9b, 0xff9b, 0x001c, 0x0000, 0xff9b, 0xff9b, 0x0000, 0x0000,
};

const wchar_t *kCharTable[] = {
    kNoCharGenKey,
    kNoCharGenKey,  // VK_LBUTTON
    kNoCharGenKey,  // VK_RBUTTON
    kCharForVK_03,  // VK_CANCEL
    kNoCharGenKey,  // VK_MBUTTON
    kNoCharGenKey,  // VK_XBUTTON1
    kNoCharGenKey,  // VK_XBUTTON2
    kNoCharGenKey,
    kCharForVK_08,  // VK_BACK
    kCharForVK_09,  // VK_TAB
    kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey,  // VK_CLEAR
    kCharForVK_0D,  // VK_RETURN
    kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey,  // VK_SHIFT
    kNoCharGenKey,  // VK_CONTROL
    kNoCharGenKey,  // VK_MENU
    kNoCharGenKey,  // VK_PAUSE
    kNoCharGenKey,  // VK_CAPITAL
    kNoCharGenKey,  // VK_HANGUL, VK_KANA
    kNoCharGenKey,  // VK_IME_ON
    kNoCharGenKey,  // VK_JUNJA
    kNoCharGenKey,  // VK_FINAL
    kNoCharGenKey,  // VK_HANJA, VK_KANJI
    kNoCharGenKey,  // VK_IME_OFF
    kCharForVK_1B,  // VK_ESCAPE
    kNoCharGenKey,  // VK_CONVERT
    kNoCharGenKey,  // VK_NONCONVERT
    kNoCharGenKey,  // VK_ACCEPT
    kNoCharGenKey,  // VK_MODECHANGE
    kCharForVK_20,  // VK_SPACE
    kNoCharGenKey,  // VK_PRIOR
    kNoCharGenKey,  // VK_NEXT
    kNoCharGenKey,  // VK_END
    kNoCharGenKey,  // VK_HOME
    kNoCharGenKey,  // VK_LEFT
    kNoCharGenKey,  // VK_UP
    kNoCharGenKey,  // VK_RIGHT
    kNoCharGenKey,  // VK_DOWN
    kNoCharGenKey,  // VK_SELECT
    kNoCharGenKey,  // VK_PRINT
    kNoCharGenKey,  // VK_EXECUTE
    kNoCharGenKey,  // VK_SNAPSHOT
    kNoCharGenKey,  // VK_INSERT
    kNoCharGenKey,  // VK_DELETE
    kNoCharGenKey,  // VK_HELP
    kCharForVK_30,  // VK_0
    kCharForVK_31,  // VK_1
    kCharForVK_32,  // VK_2
    kCharForVK_33,  // VK_3
    kCharForVK_34,  // VK_4
    kCharForVK_35,  // VK_5
    kCharForVK_36,  // VK_6
    kCharForVK_37,  // VK_7
    kCharForVK_38,  // VK_8
    kCharForVK_39,  // VK_9
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey, kNoCharGenKey,
    kCharForVK_41,  // VK_A
    kCharForVK_42,  // VK_B
    kCharForVK_43,  // VK_C
    kCharForVK_44,  // VK_D
    kCharForVK_45,  // VK_E
    kCharForVK_46,  // VK_F
    kCharForVK_47,  // VK_G
    kCharForVK_48,  // VK_H
    kCharForVK_49,  // VK_I
    kCharForVK_4A,  // VK_J
    kCharForVK_4B,  // VK_K
    kCharForVK_4C,  // VK_L
    kCharForVK_4D,  // VK_M
    kCharForVK_4E,  // VK_N
    kCharForVK_4F,  // VK_O
    kCharForVK_50,  // VK_P
    kCharForVK_51,  // VK_Q
    kCharForVK_52,  // VK_R
    kCharForVK_53,  // VK_S
    kCharForVK_54,  // VK_T
    kCharForVK_55,  // VK_U
    kCharForVK_56,  // VK_V
    kCharForVK_57,  // VK_W
    kCharForVK_58,  // VK_X
    kCharForVK_59,  // VK_Y
    kCharForVK_5A,  // VK_Z
    kNoCharGenKey,  // VK_LWIN
    kNoCharGenKey,  // VK_RWIN
    kNoCharGenKey,  // VK_APPS
    kNoCharGenKey,
    kNoCharGenKey,  // VK_SLEEP
    kCharForVK_60,  // VK_NUMPAD0
    kCharForVK_61,  // VK_NUMPAD1
    kCharForVK_62,  // VK_NUMPAD2
    kCharForVK_63,  // VK_NUMPAD3
    kCharForVK_64,  // VK_NUMPAD4
    kCharForVK_65,  // VK_NUMPAD5
    kCharForVK_66,  // VK_NUMPAD6
    kCharForVK_67,  // VK_NUMPAD7
    kCharForVK_68,  // VK_NUMPAD8
    kCharForVK_69,  // VK_NUMPAD9
    kCharForVK_6A,  // VK_MULTIPLY
    kCharForVK_6B,  // VK_ADD
    kNoCharGenKey,  // VK_SEPARATOR
    kCharForVK_6D,  // VK_SUBTRACT
    kCharForVK_6E,  // VK_DECIMAL
    kCharForVK_6F,  // VK_DIVIDE
    kNoCharGenKey,  // VK_F1
    kNoCharGenKey,  // VK_F2
    kNoCharGenKey,  // VK_F3
    kNoCharGenKey,  // VK_F4
    kNoCharGenKey,  // VK_F5
    kNoCharGenKey,  // VK_F6
    kNoCharGenKey,  // VK_F7
    kNoCharGenKey,  // VK_F8
    kNoCharGenKey,  // VK_F9
    kNoCharGenKey,  // VK_F10
    kNoCharGenKey,  // VK_F11
    kNoCharGenKey,  // VK_F12
    kNoCharGenKey,  // VK_F13
    kNoCharGenKey,  // VK_F14
    kNoCharGenKey,  // VK_F15
    kNoCharGenKey,  // VK_F16
    kNoCharGenKey,  // VK_F17
    kNoCharGenKey,  // VK_F18
    kNoCharGenKey,  // VK_F19
    kNoCharGenKey,  // VK_F20
    kNoCharGenKey,  // VK_F21
    kNoCharGenKey,  // VK_F22
    kNoCharGenKey,  // VK_F23
    kNoCharGenKey,  // VK_F24
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey,  // VK_NUMLOCK
    kNoCharGenKey,  // VK_SCROLL
    kNoCharGenKey,  // VK_OEM_FJ_JISHO, VK_OEM_NEC_EQUAL
    kNoCharGenKey,  // VK_OEM_FJ_MASSHOU
    kNoCharGenKey,  // VK_OEM_FJ_TOUROKU
    kNoCharGenKey,  // VK_OEM_FJ_LOYA
    kNoCharGenKey,  // VK_OEM_FJ_ROYA
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey,  // VK_LSHIFT
    kNoCharGenKey,  // VK_RSHIFT
    kNoCharGenKey,  // VK_LCONTROL
    kNoCharGenKey,  // VK_RCONTROL
    kNoCharGenKey,  // VK_LMENU
    kNoCharGenKey,  // VK_RMENU
    kNoCharGenKey,  // VK_BROWSER_BACK
    kNoCharGenKey,  // VK_BROWSER_FORWARD
    kNoCharGenKey,  // VK_BROWSER_REFRESH
    kNoCharGenKey,  // VK_BROWSER_STOP
    kNoCharGenKey,  // VK_BROWSER_SEARCH
    kNoCharGenKey,  // VK_BROWSER_FAVORITES
    kNoCharGenKey,  // VK_BROWSER_HOME
    kNoCharGenKey,  // VK_VOLUME_MUTE
    kNoCharGenKey,  // VK_VOLUME_DOWN
    kNoCharGenKey,  // VK_VOLUME_UP
    kNoCharGenKey,  // VK_MEDIA_NEXT_TRACK
    kNoCharGenKey,  // VK_MEDIA_PREV_TRACK
    kNoCharGenKey,  // VK_MEDIA_STOP
    kNoCharGenKey,  // VK_MEDIA_PLAY_PAUSE
    kNoCharGenKey,  // VK_LAUNCH_MAIL
    kNoCharGenKey,  // VK_LAUNCH_MEDIA_SELECT
    kNoCharGenKey,  // VK_LAUNCH_APP1
    kNoCharGenKey,  // VK_LAUNCH_APP2
    kNoCharGenKey, kNoCharGenKey,
    kCharForVK_BA,  // VK_OEM_1
    kCharForVK_BB,  // VK_OEM_PLUS
    kCharForVK_BC,  // VK_OEM_COMMA
    kCharForVK_BD,  // VK_OEM_MINUS
    kCharForVK_BE,  // VK_OEM_PERIOD
    kCharForVK_BF,  // VK_OEM_2
    kCharForVK_C0,  // VK_OEM_3
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey,
    kCharForVK_DB,  // VK_OEM_4
    kCharForVK_DC,  // VK_OEM_5
    kCharForVK_DD,  // VK_OEM_6
    kCharForVK_DE,  // VK_OEM_7
    kNoCharGenKey,  // VK_OEM_8
    kNoCharGenKey,
    kNoCharGenKey,  // VK_OEM_AX
    kCharForVK_E2,  // VK_OEM_102
    kNoCharGenKey,  // VK_ICO_HELP
    kNoCharGenKey,  // VK_ICO_00
    kNoCharGenKey,  // VK_PROCESSKEY
    kNoCharGenKey,  // VK_ICO_CLEAR
    kNoCharGenKey,  // VK_PACKET
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey, kNoCharGenKey, kNoCharGenKey,
    kNoCharGenKey,  // VK_DBE_ALPHANUMERIC
    kNoCharGenKey,  // VK_DBE_KATAKANA
    kNoCharGenKey,  // VK_DBE_HIRAGANA
    kNoCharGenKey,  // VK_DBE_SBCSCHAR
    kNoCharGenKey,  // VK_DBE_DBCSCHAR
    kNoCharGenKey,  // VK_DBE_ROMAN
    kNoCharGenKey,  // VK_DBE_NOROMAN
    kNoCharGenKey,  // VK_DBE_ENTERWORDREGISTERMODE
    kNoCharGenKey,  // VK_DBE_ENTERIMECONFIGMODE
    kNoCharGenKey,  // VK_DBE_FLUSHSTRING
    kNoCharGenKey,  // VK_DBE_CODEINPUT
    kNoCharGenKey,  // VK_DBE_NOCODEINPUT
    kNoCharGenKey,  // VK_DBE_DETERMINESTRING
    kNoCharGenKey,  // VK_DBE_ENTERDLGCONVERSIONMODE
    kNoCharGenKey, kNoCharGenKey,
};

const wchar_t *kCharTableMenuActive[] = {
    kNoCharGenKey,
    kNoCharGenKey,  // VK_LBUTTON
    kNoCharGenKey,  // VK_RBUTTON
    kCharForVK_03,  // VK_CANCEL
    kNoCharGenKey,  // VK_MBUTTON
    kNoCharGenKey,  // VK_XBUTTON1
    kNoCharGenKey,  // VK_XBUTTON2
    kNoCharGenKey,
    kCharForVK_08,  // VK_BACK
    kCharForVK_09,  // VK_TAB
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,  // VK_CLEAR
    kCharForVK_0D,  // VK_RETURN
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,  // VK_SHIFT
    kNoCharGenKey,  // VK_CONTROL
    kNoCharGenKey,  // VK_MENU
    kNoCharGenKey,  // VK_PAUSE
    kNoCharGenKey,  // VK_CAPITAL
    kNoCharGenKey,  // VK_HANGUL, VK_KANA
    kNoCharGenKey,  // VK_IME_ON
    kNoCharGenKey,  // VK_JUNJA
    kNoCharGenKey,  // VK_FINAL
    kNoCharGenKey,  // VK_HANJA, VK_KANJI
    kNoCharGenKey,  // VK_IME_OFF
    kCharForVK_1B,  // VK_ESCAPE
    kNoCharGenKey,  // VK_CONVERT
    kNoCharGenKey,  // VK_NONCONVERT
    kNoCharGenKey,  // VK_ACCEPT
    kNoCharGenKey,  // VK_MODECHANGE
    kCharForVK_20,  // VK_SPACE
    kNoCharGenKey,  // VK_PRIOR
    kNoCharGenKey,  // VK_NEXT
    kNoCharGenKey,  // VK_END
    kNoCharGenKey,  // VK_HOME
    kNoCharGenKey,  // VK_LEFT
    kNoCharGenKey,  // VK_UP
    kNoCharGenKey,  // VK_RIGHT
    kNoCharGenKey,  // VK_DOWN
    kNoCharGenKey,  // VK_SELECT
    kNoCharGenKey,  // VK_PRINT
    kNoCharGenKey,  // VK_EXECUTE
    kNoCharGenKey,  // VK_SNAPSHOT
    kNoCharGenKey,  // VK_INSERT
    kNoCharGenKey,  // VK_DELETE
    kNoCharGenKey,  // VK_HELP
    kCharForVK_30,  // VK_0
    kCharForVK_31,  // VK_1
    kCharForVK_32,  // VK_2
    kCharForVK_33,  // VK_3
    kCharForVK_34,  // VK_4
    kCharForVK_35,  // VK_5
    kCharForVK_36,  // VK_6
    kCharForVK_37,  // VK_7
    kCharForVK_38,  // VK_8
    kCharForVK_39,  // VK_9
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kCharForVK_41,  // VK_A
    kCharForVK_42,  // VK_B
    kCharForVK_43,  // VK_C
    kCharForVK_44,  // VK_D
    kCharForVK_45,  // VK_E
    kCharForVK_46,  // VK_F
    kCharForVK_47,  // VK_G
    kCharForVK_48,  // VK_H
    kCharForVK_49,  // VK_I
    kCharForVK_4A,  // VK_J
    kCharForVK_4B,  // VK_K
    kCharForVK_4C,  // VK_L
    kCharForVK_4D,  // VK_M
    kCharForVK_4E,  // VK_N
    kCharForVK_4F,  // VK_O
    kCharForVK_50,  // VK_P
    kCharForVK_51,  // VK_Q
    kCharForVK_52,  // VK_R
    kCharForVK_53,  // VK_S
    kCharForVK_54,  // VK_T
    kCharForVK_55,  // VK_U
    kCharForVK_56,  // VK_V
    kCharForVK_57,  // VK_W
    kCharForVK_58,  // VK_X
    kCharForVK_59,  // VK_Y
    kCharForVK_5A,  // VK_Z
    kNoCharGenKey,  // VK_LWIN
    kNoCharGenKey,  // VK_RWIN
    kNoCharGenKey,  // VK_APPS
    kNoCharGenKey,
    kNoCharGenKey,             // VK_SLEEP
    kCharForVK_60_MenuActive,  // VK_NUMPAD0
    kCharForVK_61_MenuActive,  // VK_NUMPAD1
    kCharForVK_62_MenuActive,  // VK_NUMPAD2
    kCharForVK_63_MenuActive,  // VK_NUMPAD3
    kCharForVK_64_MenuActive,  // VK_NUMPAD4
    kCharForVK_65_MenuActive,  // VK_NUMPAD5
    kCharForVK_66_MenuActive,  // VK_NUMPAD6
    kCharForVK_67_MenuActive,  // VK_NUMPAD7
    kCharForVK_68_MenuActive,  // VK_NUMPAD8
    kCharForVK_69_MenuActive,  // VK_NUMPAD9
    kCharForVK_6A,             // VK_MULTIPLY
    kCharForVK_6B,             // VK_ADD
    kNoCharGenKey,             // VK_SEPARATOR
    kCharForVK_6D,             // VK_SUBTRACT
    kCharForVK_6E,             // VK_DECIMAL
    kCharForVK_6F,             // VK_DIVIDE
    kNoCharGenKey,             // VK_F1
    kNoCharGenKey,             // VK_F2
    kNoCharGenKey,             // VK_F3
    kNoCharGenKey,             // VK_F4
    kNoCharGenKey,             // VK_F5
    kNoCharGenKey,             // VK_F6
    kNoCharGenKey,             // VK_F7
    kNoCharGenKey,             // VK_F8
    kNoCharGenKey,             // VK_F9
    kNoCharGenKey,             // VK_F10
    kNoCharGenKey,             // VK_F11
    kNoCharGenKey,             // VK_F12
    kNoCharGenKey,             // VK_F13
    kNoCharGenKey,             // VK_F14
    kNoCharGenKey,             // VK_F15
    kNoCharGenKey,             // VK_F16
    kNoCharGenKey,             // VK_F17
    kNoCharGenKey,             // VK_F18
    kNoCharGenKey,             // VK_F19
    kNoCharGenKey,             // VK_F20
    kNoCharGenKey,             // VK_F21
    kNoCharGenKey,             // VK_F22
    kNoCharGenKey,             // VK_F23
    kNoCharGenKey,             // VK_F24
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,  // VK_NUMLOCK
    kNoCharGenKey,  // VK_SCROLL
    kNoCharGenKey,  // VK_OEM_FJ_JISHO, VK_OEM_NEC_EQUAL
    kNoCharGenKey,  // VK_OEM_FJ_MASSHOU
    kNoCharGenKey,  // VK_OEM_FJ_TOUROKU
    kNoCharGenKey,  // VK_OEM_FJ_LOYA
    kNoCharGenKey,  // VK_OEM_FJ_ROYA
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,  // VK_LSHIFT
    kNoCharGenKey,  // VK_RSHIFT
    kNoCharGenKey,  // VK_LCONTROL
    kNoCharGenKey,  // VK_RCONTROL
    kNoCharGenKey,  // VK_LMENU
    kNoCharGenKey,  // VK_RMENU
    kNoCharGenKey,  // VK_BROWSER_BACK
    kNoCharGenKey,  // VK_BROWSER_FORWARD
    kNoCharGenKey,  // VK_BROWSER_REFRESH
    kNoCharGenKey,  // VK_BROWSER_STOP
    kNoCharGenKey,  // VK_BROWSER_SEARCH
    kNoCharGenKey,  // VK_BROWSER_FAVORITES
    kNoCharGenKey,  // VK_BROWSER_HOME
    kNoCharGenKey,  // VK_VOLUME_MUTE
    kNoCharGenKey,  // VK_VOLUME_DOWN
    kNoCharGenKey,  // VK_VOLUME_UP
    kNoCharGenKey,  // VK_MEDIA_NEXT_TRACK
    kNoCharGenKey,  // VK_MEDIA_PREV_TRACK
    kNoCharGenKey,  // VK_MEDIA_STOP
    kNoCharGenKey,  // VK_MEDIA_PLAY_PAUSE
    kNoCharGenKey,  // VK_LAUNCH_MAIL
    kNoCharGenKey,  // VK_LAUNCH_MEDIA_SELECT
    kNoCharGenKey,  // VK_LAUNCH_APP1
    kNoCharGenKey,  // VK_LAUNCH_APP2
    kNoCharGenKey,
    kNoCharGenKey,
    kCharForVK_BA,  // VK_OEM_1
    kCharForVK_BB,  // VK_OEM_PLUS
    kCharForVK_BC,  // VK_OEM_COMMA
    kCharForVK_BD,  // VK_OEM_MINUS
    kCharForVK_BE,  // VK_OEM_PERIOD
    kCharForVK_BF,  // VK_OEM_2
    kCharForVK_C0,  // VK_OEM_3
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kCharForVK_DB,  // VK_OEM_4
    kCharForVK_DC,  // VK_OEM_5
    kCharForVK_DD,  // VK_OEM_6
    kCharForVK_DE,  // VK_OEM_7
    kNoCharGenKey,  // VK_OEM_8
    kNoCharGenKey,
    kNoCharGenKey,  // VK_OEM_AX
    kCharForVK_E2,  // VK_OEM_102
    kNoCharGenKey,  // VK_ICO_HELP
    kNoCharGenKey,  // VK_ICO_00
    kNoCharGenKey,  // VK_PROCESSKEY
    kNoCharGenKey,  // VK_ICO_CLEAR
    kNoCharGenKey,  // VK_PACKET
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,
    kNoCharGenKey,  // VK_DBE_ALPHANUMERIC
    kNoCharGenKey,  // VK_DBE_KATAKANA
    kNoCharGenKey,  // VK_DBE_HIRAGANA
    kNoCharGenKey,  // VK_DBE_SBCSCHAR
    kNoCharGenKey,  // VK_DBE_DBCSCHAR
    kNoCharGenKey,  // VK_DBE_ROMAN
    kNoCharGenKey,  // VK_DBE_NOROMAN
    kNoCharGenKey,  // VK_DBE_ENTERWORDREGISTERMODE
    kNoCharGenKey,  // VK_DBE_ENTERIMECONFIGMODE
    kNoCharGenKey,  // VK_DBE_FLUSHSTRING
    kNoCharGenKey,  // VK_DBE_CODEINPUT
    kNoCharGenKey,  // VK_DBE_NOCODEINPUT
    kNoCharGenKey,  // VK_DBE_DETERMINESTRING
    kNoCharGenKey,  // VK_DBE_ENTERDLGCONVERSIONMODE
    kNoCharGenKey,
    kNoCharGenKey,
};
}  // namespace

wchar_t JapaneseKeyboardLayoutEmulator::GetCharacterForKeyDown(
    BYTE virtual_key, const BYTE keyboard_state[256], bool is_menu_active) {
  if (virtual_key == VK_PACKET) {
    return L'\0';
  }

  int index = 0;
  const KeyboardStatus keystate(keyboard_state);
  if (keystate.IsPressed(VK_SHIFT)) {
    index |= kShiftPressed;
  }
  if (keystate.IsPressed(VK_CONTROL)) {
    index |= kCtrlPressed;
  }
  if (keystate.IsPressed(VK_MENU)) {
    index |= kAltPressed;
  }
  if (keystate.IsToggled(VK_CAPITAL)) {
    index |= kCapsLock;
  }
  if (keystate.IsPressed(VK_KANA)) {
    index |= kKanaLock;
  }

  // As far as we have observed with built-in Japanese keyboard layout on
  // Windows Vista, we can ignore the following modifiers in terms of ToUnicode
  // API.
  // - VK_LWIN / VK_RWIN
  // - VK_NUMLOCK
  // - VK_SCROLL
  // We can also assume there is no difference between left/right modifiers
  // for built-in Japanese keyboard layout.
  // - VK_LSHIFT/VK_RSHIFT
  // - VK_LCONTROL/VK_RCONTROL
  // - VK_LMENU/VK_RMENU

  DCHECK(virtual_key < arraysize(kCharTable));
  if (is_menu_active) {
    DCHECK_LE(0, index);
    DCHECK(index < arraysize(kNoCharGenKey));
    return kCharTableMenuActive[virtual_key][index];
  } else {
    DCHECK_LE(0, index);
    DCHECK(index < arraysize(kNoCharGenKey));
    return kCharTable[virtual_key][index];
  }
}

int JapaneseKeyboardLayoutEmulator::ToUnicode(UINT virtual_key, UINT scan_code,
                                              CONST BYTE *key_state,
                                              LPWSTR character_buffer,
                                              int character_buffer_size,
                                              UINT flags) {
  const bool is_menu_active = ((flags & 0x1) == 0x1);

  DCHECK_LE(virtual_key, 0xff);
  const BYTE normalized_virtual_key = (virtual_key & 0xff);

  // As far as we have observed with built-in Japanese keyboard layout on
  // Windows Vista, ::ToUnicode returns a null character when VK_PACKET is
  // specified.
  // TODO(yukawa): Actually the returned value of VK_PACKET depends on
  //   |wScanCode|.  More investigation needed.
  if (normalized_virtual_key == VK_PACKET) {
    if (character_buffer_size < 1) {
      // If the buffer size is insufficient, ::ToUnicode returns 0.
      return 0;
    }
    character_buffer[0] = L'\0';
    return 1;
  }

  // The high-order bit of this value is set if the key is up.
  // http://msdn.microsoft.com/en-us/library/ms646322.aspx
  const bool is_key_down = ((scan_code & 0x8000) == 0);

  // As far as we have observed with built-in Japanese keyboard layout on
  // Windows Vista, there is no key to generate characters when the key is
  // released.
  if (!is_key_down) {
    return 0;
  }

  const wchar_t character =
      GetCharacterForKeyDown(normalized_virtual_key, key_state, is_menu_active);
  // GetCharacterForKeyDown returns '\0' when no character is generated.
  if (character == L'\0') {
    return 0;
  }

  if (character_buffer_size < 1) {
    // If the buffer size is insufficient, ::ToUnicode returns 0.
    return 0;
  }
  character_buffer[0] = character;
  return 1;
}
}  // namespace win32
}  // namespace mozc
