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

#include "win32/base/keyevent_handler.h"

#include <ime.h>
#include <string.h>
#include <windows.h>

#include <set>
#include <string>

#include "absl/log/check.h"
#include "base/japanese_util.h"
#include "base/util.h"
#include "base/win32/wide_char.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "session/key_info_util.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/input_state.h"
#include "win32/base/keyboard.h"

namespace mozc {
namespace win32 {
using commands::KeyEvent;
using commands::Output;

namespace {

// The Mozc protocol has expected the client to send a key event with
// |KeyEvent::HANKAKU| special key as if there was single Hankaku/Zenkaku
// key.  This is why we map both VK_DBE_SBCSCHAR and VK_DBE_DBCSCHAR into
// KeyEvent::HANKAKU.
const KeyEvent::SpecialKey kSpecialKeyMap[] = {
    KeyEvent::NO_SPECIALKEY,  // 0x00:
    KeyEvent::NO_SPECIALKEY,  // 0x01: VK_LBUTTON
    KeyEvent::NO_SPECIALKEY,  // 0x02: VK_RBUTTON
    KeyEvent::NO_SPECIALKEY,  // 0x03: VK_CANCEL
    KeyEvent::NO_SPECIALKEY,  // 0x04: VK_MBUTTON
    KeyEvent::NO_SPECIALKEY,  // 0x05: VK_XBUTTON1
    KeyEvent::NO_SPECIALKEY,  // 0x06: VK_XBUTTON2
    KeyEvent::NO_SPECIALKEY,  // 0x07:
    KeyEvent::BACKSPACE,      // 0x08: VK_BACK
    KeyEvent::TAB,            // 0x09: VK_TAB
    KeyEvent::NO_SPECIALKEY,  // 0x0A:
    KeyEvent::NO_SPECIALKEY,  // 0x0B:
    KeyEvent::CLEAR,          // 0x0C: VK_CLEAR
    KeyEvent::ENTER,          // 0x0D: VK_RETURN
    KeyEvent::NO_SPECIALKEY,  // 0x0E:
    KeyEvent::NO_SPECIALKEY,  // 0x0F:
    KeyEvent::NO_SPECIALKEY,  // 0x10: VK_SHIFT
    KeyEvent::NO_SPECIALKEY,  // 0x11: VK_CONTROL
    KeyEvent::NO_SPECIALKEY,  // 0x12: VK_MENU
    KeyEvent::NO_SPECIALKEY,  // 0x13: VK_PAUSE
    KeyEvent::CAPS_LOCK,      // 0x14: VK_CAPITAL
    KeyEvent::NO_SPECIALKEY,  // 0x15: VK_HANGUL, VK_KANA
    KeyEvent::ON,             // 0x16: VK_IME_ON
    KeyEvent::NO_SPECIALKEY,  // 0x17: VK_JUNJA
    KeyEvent::NO_SPECIALKEY,  // 0x18: VK_FINAL
    // VK_KANJI is very special in IMM32 mode. It activates IME in OS-side
    // regardless of the actual key binding in an IME. On the other hand,
    // this automatic activation does not happen in TSF mode. To work around
    // this anomaly, we map VK_KANJI to KeyEvent::NO_SPECIALKEY instead of
    // KeyEvent::KANJI in IMM32 mode and map VK_KANJI to KeyEvent::HANKAKU
    // via VK_DBE_DBCSCHAR in TSF mode. See b/7970379 for the background.
    KeyEvent::NO_SPECIALKEY,  // 0x19: VK_HANJA, VK_KANJI
    KeyEvent::OFF,            // 0x1A: VK_IME_OFF
    KeyEvent::ESCAPE,         // 0x1B: VK_ESCAPE
    KeyEvent::HENKAN,         // 0x1C: VK_CONVERT
    KeyEvent::MUHENKAN,       // 0x1D: VK_NONCONVERT
    KeyEvent::NO_SPECIALKEY,  // 0x1E: VK_ACCEPT
    KeyEvent::NO_SPECIALKEY,  // 0x1F: VK_MODECHANGE
    KeyEvent::SPACE,          // 0x20: VK_SPACE
    KeyEvent::PAGE_UP,        // 0x21: VK_PRIOR
    KeyEvent::PAGE_DOWN,      // 0x22: VK_NEXT
    KeyEvent::END,            // 0x23: VK_END
    KeyEvent::HOME,           // 0x24: VK_HOME
    KeyEvent::LEFT,           // 0x25: VK_LEFT
    KeyEvent::UP,             // 0x26: VK_UP
    KeyEvent::RIGHT,          // 0x27: VK_RIGHT
    KeyEvent::DOWN,           // 0x28: VK_DOWN
    KeyEvent::NO_SPECIALKEY,  // 0x29: VK_SELECT
    KeyEvent::NO_SPECIALKEY,  // 0x2A: VK_PRINT
    KeyEvent::NO_SPECIALKEY,  // 0x2B: VK_EXECUTE
    KeyEvent::NO_SPECIALKEY,  // 0x2C: VK_SNAPSHOT
    KeyEvent::INSERT,         // 0x2D: VK_INSERT
    KeyEvent::DEL,            // 0x2E: VK_DELETE
    KeyEvent::NO_SPECIALKEY,  // 0x2F: VK_HELP
    KeyEvent::NO_SPECIALKEY,  // 0x30: VK_0
    KeyEvent::NO_SPECIALKEY,  // 0x31: VK_1
    KeyEvent::NO_SPECIALKEY,  // 0x32: VK_2
    KeyEvent::NO_SPECIALKEY,  // 0x33: VK_3
    KeyEvent::NO_SPECIALKEY,  // 0x34: VK_4
    KeyEvent::NO_SPECIALKEY,  // 0x35: VK_5
    KeyEvent::NO_SPECIALKEY,  // 0x36: VK_6
    KeyEvent::NO_SPECIALKEY,  // 0x37: VK_7
    KeyEvent::NO_SPECIALKEY,  // 0x38: VK_8
    KeyEvent::NO_SPECIALKEY,  // 0x39: VK_9
    KeyEvent::NO_SPECIALKEY,  // 0x3A:
    KeyEvent::NO_SPECIALKEY,  // 0x3B:
    KeyEvent::NO_SPECIALKEY,  // 0x3C:
    KeyEvent::NO_SPECIALKEY,  // 0x3D:
    KeyEvent::NO_SPECIALKEY,  // 0x3E:
    KeyEvent::NO_SPECIALKEY,  // 0x3F:
    KeyEvent::NO_SPECIALKEY,  // 0x40:
    KeyEvent::NO_SPECIALKEY,  // 0x41: VK_A
    KeyEvent::NO_SPECIALKEY,  // 0x42: VK_B
    KeyEvent::NO_SPECIALKEY,  // 0x43: VK_C
    KeyEvent::NO_SPECIALKEY,  // 0x44: VK_D
    KeyEvent::NO_SPECIALKEY,  // 0x45: VK_E
    KeyEvent::NO_SPECIALKEY,  // 0x46: VK_F
    KeyEvent::NO_SPECIALKEY,  // 0x47: VK_G
    KeyEvent::NO_SPECIALKEY,  // 0x48: VK_H
    KeyEvent::NO_SPECIALKEY,  // 0x49: VK_I
    KeyEvent::NO_SPECIALKEY,  // 0x4A: VK_J
    KeyEvent::NO_SPECIALKEY,  // 0x4B: VK_K
    KeyEvent::NO_SPECIALKEY,  // 0x4C: VK_L
    KeyEvent::NO_SPECIALKEY,  // 0x4D: VK_M
    KeyEvent::NO_SPECIALKEY,  // 0x4E: VK_N
    KeyEvent::NO_SPECIALKEY,  // 0x4F: VK_O
    KeyEvent::NO_SPECIALKEY,  // 0x50: VK_P
    KeyEvent::NO_SPECIALKEY,  // 0x51: VK_Q
    KeyEvent::NO_SPECIALKEY,  // 0x52: VK_R
    KeyEvent::NO_SPECIALKEY,  // 0x53: VK_S
    KeyEvent::NO_SPECIALKEY,  // 0x54: VK_T
    KeyEvent::NO_SPECIALKEY,  // 0x55: VK_U
    KeyEvent::NO_SPECIALKEY,  // 0x56: VK_V
    KeyEvent::NO_SPECIALKEY,  // 0x57: VK_W
    KeyEvent::NO_SPECIALKEY,  // 0x58: VK_X
    KeyEvent::NO_SPECIALKEY,  // 0x59: VK_Y
    KeyEvent::NO_SPECIALKEY,  // 0x5A: VK_Z
    KeyEvent::NO_SPECIALKEY,  // 0x5B: VK_LWIN
    KeyEvent::NO_SPECIALKEY,  // 0x5C: VK_RWIN
    KeyEvent::NO_SPECIALKEY,  // 0x5D: VK_APPS
    KeyEvent::NO_SPECIALKEY,  // 0x5E:
    KeyEvent::NO_SPECIALKEY,  // 0x5F: VK_SLEEP
    KeyEvent::NUMPAD0,        // 0x60: VK_NUMPAD0
    KeyEvent::NUMPAD1,        // 0x61: VK_NUMPAD1
    KeyEvent::NUMPAD2,        // 0x62: VK_NUMPAD2
    KeyEvent::NUMPAD3,        // 0x63: VK_NUMPAD3
    KeyEvent::NUMPAD4,        // 0x64: VK_NUMPAD4
    KeyEvent::NUMPAD5,        // 0x65: VK_NUMPAD5
    KeyEvent::NUMPAD6,        // 0x66: VK_NUMPAD6
    KeyEvent::NUMPAD7,        // 0x67: VK_NUMPAD7
    KeyEvent::NUMPAD8,        // 0x68: VK_NUMPAD8
    KeyEvent::NUMPAD9,        // 0x69: VK_NUMPAD9
    KeyEvent::MULTIPLY,       // 0x6A: VK_MULTIPLY
    KeyEvent::ADD,            // 0x6B: VK_ADD
    KeyEvent::SEPARATOR,      // 0x6C: VK_SEPARATOR
    KeyEvent::SUBTRACT,       // 0x6D: VK_SUBTRACT
    KeyEvent::DECIMAL,        // 0x6E: VK_DECIMAL
    KeyEvent::DIVIDE,         // 0x6F: VK_DIVIDE
    KeyEvent::F1,             // 0x70: VK_F1
    KeyEvent::F2,             // 0x71: VK_F2
    KeyEvent::F3,             // 0x72: VK_F3
    KeyEvent::F4,             // 0x73: VK_F4
    KeyEvent::F5,             // 0x74: VK_F5
    KeyEvent::F6,             // 0x75: VK_F6
    KeyEvent::F7,             // 0x76: VK_F7
    KeyEvent::F8,             // 0x77: VK_F8
    KeyEvent::F9,             // 0x78: VK_F9
    KeyEvent::F10,            // 0x79: VK_F10
    KeyEvent::F11,            // 0x7A: VK_F11
    KeyEvent::F12,            // 0x7B: VK_F12
    KeyEvent::F13,            // 0x7C: VK_F13
    KeyEvent::F14,            // 0x7D: VK_F14
    KeyEvent::F15,            // 0x7E: VK_F15
    KeyEvent::F16,            // 0x7F: VK_F16
    KeyEvent::F17,            // 0x80: VK_F17
    KeyEvent::F18,            // 0x81: VK_F18
    KeyEvent::F19,            // 0x82: VK_F19
    KeyEvent::F20,            // 0x83: VK_F20
    KeyEvent::F21,            // 0x84: VK_F21
    KeyEvent::F22,            // 0x85: VK_F22
    KeyEvent::F23,            // 0x86: VK_F23
    KeyEvent::F24,            // 0x87: VK_F24
    KeyEvent::NO_SPECIALKEY,  // 0x88:
    KeyEvent::NO_SPECIALKEY,  // 0x89:
    KeyEvent::NO_SPECIALKEY,  // 0x8A:
    KeyEvent::NO_SPECIALKEY,  // 0x8B:
    KeyEvent::NO_SPECIALKEY,  // 0x8C:
    KeyEvent::NO_SPECIALKEY,  // 0x8D:
    KeyEvent::NO_SPECIALKEY,  // 0x8E:
    KeyEvent::NO_SPECIALKEY,  // 0x8F:
    KeyEvent::NO_SPECIALKEY,  // 0x90: VK_NUMLOCK
    KeyEvent::NO_SPECIALKEY,  // 0x91: VK_SCROLL
    KeyEvent::NO_SPECIALKEY,  // 0x92: VK_OEM_FJ_JISHO, VK_OEM_NEC_EQUAL
    KeyEvent::NO_SPECIALKEY,  // 0x93: VK_OEM_FJ_MASSHOU
    KeyEvent::NO_SPECIALKEY,  // 0x94: VK_OEM_FJ_TOUROKU
    KeyEvent::NO_SPECIALKEY,  // 0x95: VK_OEM_FJ_LOYA
    KeyEvent::NO_SPECIALKEY,  // 0x96: VK_OEM_FJ_ROYA
    KeyEvent::NO_SPECIALKEY,  // 0x97:
    KeyEvent::NO_SPECIALKEY,  // 0x98:
    KeyEvent::NO_SPECIALKEY,  // 0x99:
    KeyEvent::NO_SPECIALKEY,  // 0x9A:
    KeyEvent::NO_SPECIALKEY,  // 0x9B:
    KeyEvent::NO_SPECIALKEY,  // 0x9C:
    KeyEvent::NO_SPECIALKEY,  // 0x9D:
    KeyEvent::NO_SPECIALKEY,  // 0x9E:
    KeyEvent::NO_SPECIALKEY,  // 0x9F:
    KeyEvent::NO_SPECIALKEY,  // 0xA0: VK_LSHIFT
    KeyEvent::NO_SPECIALKEY,  // 0xA1: VK_RSHIFT
    KeyEvent::NO_SPECIALKEY,  // 0xA2: VK_LCONTROL
    KeyEvent::NO_SPECIALKEY,  // 0xA3: VK_RCONTROL
    KeyEvent::NO_SPECIALKEY,  // 0xA4: VK_LMENU
    KeyEvent::NO_SPECIALKEY,  // 0xA5: VK_RMENU
    KeyEvent::NO_SPECIALKEY,  // 0xA6: VK_BROWSER_BACK
    KeyEvent::NO_SPECIALKEY,  // 0xA7: VK_BROWSER_FORWARD
    KeyEvent::NO_SPECIALKEY,  // 0xA8: VK_BROWSER_REFRESH
    KeyEvent::NO_SPECIALKEY,  // 0xA9: VK_BROWSER_STOP
    KeyEvent::NO_SPECIALKEY,  // 0xAA: VK_BROWSER_SEARCH
    KeyEvent::NO_SPECIALKEY,  // 0xAB: VK_BROWSER_FAVORITES
    KeyEvent::NO_SPECIALKEY,  // 0xAC: VK_BROWSER_HOME
    KeyEvent::NO_SPECIALKEY,  // 0xAD: VK_VOLUME_MUTE
    KeyEvent::NO_SPECIALKEY,  // 0xAE: VK_VOLUME_DOWN
    KeyEvent::NO_SPECIALKEY,  // 0xAF: VK_VOLUME_UP
    KeyEvent::NO_SPECIALKEY,  // 0xB0: VK_MEDIA_NEXT_TRACK
    KeyEvent::NO_SPECIALKEY,  // 0xB1: VK_MEDIA_PREV_TRACK
    KeyEvent::NO_SPECIALKEY,  // 0xB2: VK_MEDIA_STOP
    KeyEvent::NO_SPECIALKEY,  // 0xB3: VK_MEDIA_PLAY_PAUSE
    KeyEvent::NO_SPECIALKEY,  // 0xB4: VK_LAUNCH_MAIL
    KeyEvent::NO_SPECIALKEY,  // 0xB5: VK_LAUNCH_MEDIA_SELECT
    KeyEvent::NO_SPECIALKEY,  // 0xB6: VK_LAUNCH_APP1
    KeyEvent::NO_SPECIALKEY,  // 0xB7: VK_LAUNCH_APP2
    KeyEvent::NO_SPECIALKEY,  // 0xB8:
    KeyEvent::NO_SPECIALKEY,  // 0xB9:
    KeyEvent::NO_SPECIALKEY,  // 0xBA: VK_OEM_1
    KeyEvent::NO_SPECIALKEY,  // 0xBB: VK_OEM_PLUS
    KeyEvent::NO_SPECIALKEY,  // 0xBC: VK_OEM_COMMA
    KeyEvent::NO_SPECIALKEY,  // 0xBD: VK_OEM_MINUS
    KeyEvent::NO_SPECIALKEY,  // 0xBE: VK_OEM_PERIOD
    KeyEvent::NO_SPECIALKEY,  // 0xBF: VK_OEM_2
    KeyEvent::NO_SPECIALKEY,  // 0xC0: VK_OEM_3
    KeyEvent::NO_SPECIALKEY,  // 0xC1: VK_ABNT_C1
    // The numpad comma on the Apple Japanese 109 keyboard is somehow mapped
    // into VK_ABNT_C2, which is only defined in kbd.h. See also
    // http://blogs.msdn.com/b/michkap/archive/2006/10/07/799605.aspx See also
    // b/6639635.
    KeyEvent::COMMA,          // 0xC2: VK_ABNT_C2
    KeyEvent::NO_SPECIALKEY,  // 0xC3:
    KeyEvent::NO_SPECIALKEY,  // 0xC4:
    KeyEvent::NO_SPECIALKEY,  // 0xC5:
    KeyEvent::NO_SPECIALKEY,  // 0xC6:
    KeyEvent::NO_SPECIALKEY,  // 0xC7:
    KeyEvent::NO_SPECIALKEY,  // 0xC8:
    KeyEvent::NO_SPECIALKEY,  // 0xC9:
    KeyEvent::NO_SPECIALKEY,  // 0xCA:
    KeyEvent::NO_SPECIALKEY,  // 0xCB:
    KeyEvent::NO_SPECIALKEY,  // 0xCC:
    KeyEvent::NO_SPECIALKEY,  // 0xCD:
    KeyEvent::NO_SPECIALKEY,  // 0xCE:
    KeyEvent::NO_SPECIALKEY,  // 0xCF:
    KeyEvent::NO_SPECIALKEY,  // 0xD0:
    KeyEvent::NO_SPECIALKEY,  // 0xD1:
    KeyEvent::NO_SPECIALKEY,  // 0xD2:
    KeyEvent::NO_SPECIALKEY,  // 0xD3:
    KeyEvent::NO_SPECIALKEY,  // 0xD4:
    KeyEvent::NO_SPECIALKEY,  // 0xD5:
    KeyEvent::NO_SPECIALKEY,  // 0xD6:
    KeyEvent::NO_SPECIALKEY,  // 0xD7:
    KeyEvent::NO_SPECIALKEY,  // 0xD8:
    KeyEvent::NO_SPECIALKEY,  // 0xD9:
    KeyEvent::NO_SPECIALKEY,  // 0xDA:
    KeyEvent::NO_SPECIALKEY,  // 0xDB: VK_OEM_4
    KeyEvent::NO_SPECIALKEY,  // 0xDC: VK_OEM_5
    KeyEvent::NO_SPECIALKEY,  // 0xDD: VK_OEM_6
    KeyEvent::NO_SPECIALKEY,  // 0xDE: VK_OEM_7
    KeyEvent::NO_SPECIALKEY,  // 0xDF: VK_OEM_8
    KeyEvent::NO_SPECIALKEY,  // 0xE0:
    KeyEvent::NO_SPECIALKEY,  // 0xE1: VK_OEM_AX
    KeyEvent::NO_SPECIALKEY,  // 0xE2: VK_OEM_102
    KeyEvent::NO_SPECIALKEY,  // 0xE3: VK_ICO_HELP
    KeyEvent::NO_SPECIALKEY,  // 0xE4: VK_ICO_00
    KeyEvent::NO_SPECIALKEY,  // 0xE5: VK_PROCESSKEY
    KeyEvent::NO_SPECIALKEY,  // 0xE6: VK_ICO_CLEAR
    KeyEvent::NO_SPECIALKEY,  // 0xE7: VK_PACKET
    KeyEvent::NO_SPECIALKEY,  // 0xE8:
    KeyEvent::NO_SPECIALKEY,  // 0xE9:
    KeyEvent::NO_SPECIALKEY,  // 0xEA:
    KeyEvent::NO_SPECIALKEY,  // 0xEB:
    KeyEvent::NO_SPECIALKEY,  // 0xEC:
    KeyEvent::NO_SPECIALKEY,  // 0xED:
    KeyEvent::NO_SPECIALKEY,  // 0xEE:
    KeyEvent::NO_SPECIALKEY,  // 0xEF:
    KeyEvent::EISU,           // 0xF0: VK_DBE_ALPHANUMERIC
    KeyEvent::KATAKANA,       // 0xF1: VK_DBE_KATAKANA
    KeyEvent::KANA,           // 0xF2: VK_DBE_HIRAGANA
    KeyEvent::HANKAKU,        // 0xF3: VK_DBE_SBCSCHAR
    KeyEvent::HANKAKU,        // 0xF4: VK_DBE_DBCSCHAR
    KeyEvent::NO_SPECIALKEY,  // 0xF5: VK_DBE_ROMAN
    KeyEvent::NO_SPECIALKEY,  // 0xF6: VK_DBE_NOROMAN
    KeyEvent::NO_SPECIALKEY,  // 0xF7: VK_DBE_ENTERWORDREGISTERMODE
    KeyEvent::NO_SPECIALKEY,  // 0xF8: VK_DBE_ENTERIMECONFIGMODE
    KeyEvent::NO_SPECIALKEY,  // 0xF9: VK_DBE_FLUSHSTRING
    KeyEvent::NO_SPECIALKEY,  // 0xFA: VK_DBE_CODEINPUT
    KeyEvent::NO_SPECIALKEY,  // 0xFB: VK_DBE_NOCODEINPUT
    KeyEvent::NO_SPECIALKEY,  // 0xFC: VK_DBE_DETERMINESTRING
    KeyEvent::NO_SPECIALKEY,  // 0xFD: VK_DBE_ENTERDLGCONVERSIONMODE
    KeyEvent::NO_SPECIALKEY,  // 0xFE:
    KeyEvent::NO_SPECIALKEY,  // 0xFF:
};

void ClearModifyerKeyIfNeeded(const KeyEvent *key,
                              std::set<KeyEvent::ModifierKey> *modifiers) {
  if (key == nullptr) {
    return;
  }
  if (modifiers == nullptr) {
    return;
  }
  if (key->has_special_key()) {
    switch (key->special_key()) {
      // Clear modifier keys when the key is filtered in
      // KeyBindingFilter::Encode in gui/config_dialog/keybinding_editor.cc
      case KeyEvent::EISU:      // VK_DBE_ALPHANUMERIC
      case KeyEvent::HANKAKU:   // VK_DBE_SBCSCHAR or VK_DBE_DBCSCHAR
      case KeyEvent::KANA:      // VK_DBE_HIRAGANA
      case KeyEvent::KATAKANA:  // VK_DBE_KATAKANA
        modifiers->clear();
        break;
      default:
        // Do nothing.
        break;
    }
  }
}

// See b/2576120 for details.
bool IsNotimplementedKey(const VirtualKey &virtual_key) {
  switch (virtual_key.virtual_key()) {
    case VK_DBE_ROMAN:    // Changes the mode to Roman characters.
    case VK_DBE_NOROMAN:  // Changes the mode to non-Roman characters.
      // Cuurently these keys are handled by the client, NOT the server.
      // See b/3118905
      // TODO(yukawa, komatsu): Handle these keys in the server.
      return true;
    case VK_DBE_ENTERWORDREGISTERMODE:
      // Activates the word registration dialog box.
      // Ctrl+Alt+Muhenkan on 106 Japanese Keyboard.
      return true;
    case VK_DBE_ENTERIMECONFIGMODE:
      // Activates a dialog box for setting up an IME environment.
      // Ctrl+Alt+Hankaku/Zenkaku on 106 Japanese Keyboard.
      return true;
    case VK_DBE_FLUSHSTRING:
      // Deletes the undetermined string without determining it.
      return true;
    case VK_DBE_CODEINPUT:
      // Changes the mode to code input.
      return true;
    case VK_DBE_NOCODEINPUT:
      // Changes the mode to no-code input.
      return true;
    case VK_DBE_DETERMINESTRING:
      return true;
    case VK_DBE_ENTERDLGCONVERSIONMODE:
      return true;
    default:
      return false;
  }
}

bool ConvertToKeyEventMain(const VirtualKey &virtual_key, BYTE scan_code,
                           bool is_key_down, bool is_menu_active,
                           const InputBehavior &behavior,
                           const InputState &ime_state,
                           const KeyboardStatus &keyboard_status,
                           Win32KeyboardInterface *keyboard,
                           commands::KeyEvent *key,
                           std::set<KeyEvent::ModifierKey> *modifer_keys) {
  if (key == nullptr) {
    return false;
  }
  key->Clear();

  if (modifer_keys == nullptr) {
    return false;
  }
  modifer_keys->clear();

  // Support VK_PACKET.
  if (virtual_key.virtual_key() == VK_PACKET) {
    const char32_t character = virtual_key.unicode_char();
    const std::string utf8_characters = Util::CodepointToUtf8(character);
    if (utf8_characters.empty()) {
      return false;
    }
    // Setting |key_string| only to pass an arbitrary character to the
    // converter.
    key->set_key_string(utf8_characters);
    return true;
  }

  // TODO(yukawa): Distinguish left key from right key to fix b/2674446.
  if (keyboard_status.IsPressed(VK_SHIFT)) {
    modifer_keys->insert(KeyEvent::SHIFT);
  }
  if (keyboard_status.IsPressed(VK_CONTROL)) {
    modifer_keys->insert(KeyEvent::CTRL);
  }
  if (keyboard_status.IsPressed(VK_MENU)) {
    modifer_keys->insert(KeyEvent::ALT);
  }
  if (keyboard_status.IsPressed(VK_CAPITAL)) {
    modifer_keys->insert(KeyEvent::CAPS);
  }

  const KeyEvent::SpecialKey special_key =
      kSpecialKeyMap[virtual_key.virtual_key()];
  if (special_key != KeyEvent::NO_SPECIALKEY) {
    key->set_special_key(special_key);

    // Currently the Mozc server expects that the modifier keys are always
    // empty for some special keys.  So we have to clear the modifier keys if
    // needed.
    ClearModifyerKeyIfNeeded(key, modifer_keys);
    return true;
  }

  // Modifier keys should be handled.
  // Due to the anomaly of Mozc protocol, |modifer_keys| should be set even
  // when this is the key-up message of the modifier key.
  switch (virtual_key.virtual_key()) {
    case VK_SHIFT:
      modifer_keys->insert(KeyEvent::SHIFT);
      return true;
    case VK_CONTROL:
      modifer_keys->insert(KeyEvent::CTRL);
      return true;
    case VK_MENU:
      modifer_keys->insert(KeyEvent::ALT);
      return true;
    case VK_CAPITAL:
      modifer_keys->insert(KeyEvent::CAPS);
      return true;
  }

  // The high-order bit of this value is set if the key is up.
  // http://msdn.microsoft.com/en-us/library/ms646322.aspx
  const UINT to_unicode_scancode =
      static_cast<UINT>(scan_code) | (is_key_down ? 0 : 0x8000);

  const DWORD to_unicode_flag = (is_menu_active ? 1 : 0);

  KeyboardStatus keyboard_status_wo_kana_lock = keyboard_status;
  keyboard_status_wo_kana_lock.SetState(VK_KANA, 0);

  bool has_valid_key_string = false;

  // Instead of using the actual toggle state of Kana-lock key, an expected
  // toggle state of the Kana-lock is emulated based on the IME open/close
  // state and conversion mode.  See b/3046717 for details.
  // Note that we never set |key_string| when Ctrl key is pressed because
  // no valid Kana character will be generated with Ctrl key. See b/9684668.
  const bool use_kana_input =
      behavior.prefer_kana_input && ime_state.open &&
      !keyboard_status_wo_kana_lock.IsPressed(VK_CONTROL) &&
      ((ime_state.logical_conversion_mode & IME_CMODE_NATIVE) ==
       IME_CMODE_NATIVE);

  if (use_kana_input) {
    // Make a snapshot of keyboard state, then update it so that the
    // Kana-lock state is 'locked'.
    KeyboardStatus keyboard_status_w_kana_lock = keyboard_status;
    const BYTE kKeyPressed = 0x80;
    const BYTE kKeyToggled = 0x1;
    keyboard_status_w_kana_lock.SetState(VK_KANA, kKeyPressed | kKeyToggled);

    WCHAR kana_codes[16] = {};
    const int kana_locked_length = keyboard->ToUnicode(
        virtual_key.virtual_key(), to_unicode_scancode,
        keyboard_status_w_kana_lock.status(), &kana_codes[0],
        std::size(kana_codes), to_unicode_flag);
    if (kana_locked_length != 1) {
      return false;
    }
    const wchar_t kana_code = kana_codes[0];

    // TODO(yukawa): Move the following character conversion logic into
    //   mozc::Util as HalfWidthKatakanaToHiragana.
    const wchar_t whalf_katakana[] = {static_cast<wchar_t>(kana_code), L'\0'};
    const std::string half_katakana = WideToUtf8(whalf_katakana);
    std::string full_katakana =
        mozc::japanese_util::HalfWidthKatakanaToFullWidthKatakana(
            half_katakana.c_str());
    std::string full_hiragana =
        mozc::japanese_util::KatakanaToHiragana(full_katakana.c_str());
    key->set_key_string(full_hiragana);
    has_valid_key_string = true;
  }

  // To conform to Mozc protocol, we need special hack for VK_A, VK_B, ...,
  // VK_Z.  For these keys, currently we cannot support the current keyboard
  // layout.   For example, imagine a keyboard layout which generates
  // characters as follows.
  //    VK_A         -> 'a'
  //    VK_A + SHIFT -> '('
  // Unfortunately, the current Mozc protocol cannot handle these cases because
  // there is serious ambiguity between 'Key' and 'Character' in Mozc key
  // bindings.
  const bool is_vk_alpha =
      ('A' <= virtual_key.virtual_key() && virtual_key.virtual_key() <= 'Z');
  if (is_vk_alpha) {
    const char keycode = static_cast<char>(virtual_key.virtual_key());
    const size_t index = (keycode - 'A');
    if (keyboard_status_wo_kana_lock.IsToggled(VK_CAPITAL)) {
      // CapsLock is enabled.
      modifer_keys->insert(KeyEvent::CAPS);
      if (keyboard_status_wo_kana_lock.IsPressed(VK_SHIFT)) {
        if (!keyboard_status_wo_kana_lock.IsPressed(VK_CONTROL)) {
          modifer_keys->erase(KeyEvent::SHIFT);
        }
        key->set_key_code('a' + index);
      } else {
        key->set_key_code('A' + index);
      }
      if (keyboard_status_wo_kana_lock.IsPressed(VK_CONTROL)) {
        modifer_keys->insert(KeyEvent::CTRL);
        return true;
      }
      return true;
    }

    // CapsLock is not enabled.
    DCHECK(!keyboard_status_wo_kana_lock.IsPressed(VK_CAPITAL));
    if (keyboard_status_wo_kana_lock.IsPressed(VK_CONTROL)) {
      modifer_keys->insert(KeyEvent::CTRL);
      key->set_key_code('a' + index);
      return true;
    }
    if (keyboard_status_wo_kana_lock.IsPressed(VK_SHIFT)) {
      // In this cases, SHIFT modifier should be removed.
      modifer_keys->erase(KeyEvent::SHIFT);
      key->set_key_code('A' + index);
      return true;
    }

    key->set_key_code('a' + index);
    return true;
  }

  // Mozc key binding tool does not allow to use a key combination
  // |KeyEvent::CTRL| + |KeyEvent::Shift| + X except that X is VK_A, ..., or,
  // VK_Z, or other special keys defined in Mozc protocol such as backspace
  // or space.
  if (keyboard_status_wo_kana_lock.IsPressed(VK_SHIFT) &&
      keyboard_status_wo_kana_lock.IsPressed(VK_CONTROL)) {
    // Assume the server does not support this key combination.
    DCHECK(!is_vk_alpha);
    return false;
  }

  // To conform to Mozc protocol, we have to clear control modifier before
  // obtaining the unicode text.  For example, the mozc server expects Ctrl+^
  // to be sent as '^' + |KeyEvent::CTRL|.
  if (keyboard_status_wo_kana_lock.IsPressed(VK_CONTROL)) {
    // We can assume the shiftkey is not pressed here.
    DCHECK(!keyboard_status_wo_kana_lock.IsPressed(VK_SHIFT));

    keyboard_status_wo_kana_lock.SetState(VK_CONTROL, 0);
    keyboard_status_wo_kana_lock.SetState(VK_LCONTROL, 0);
    keyboard_status_wo_kana_lock.SetState(VK_RCONTROL, 0);
  }

  WCHAR codes[16] = {};
  int kana_unlocked_length =
      keyboard->ToUnicode(virtual_key.virtual_key(), to_unicode_scancode,
                          keyboard_status_wo_kana_lock.status(), &codes[0],
                          std::size(codes), to_unicode_flag);

  // A workaround for b/3029665.
  // Keyboard drivers of the JIS keyboard do not produce a key code for
  // some key combinations such as SHIFT + '0' but the mozc server requires
  // the client to set a key code if it is in the kana mode.
  // So change the keycode for SHIFT + X to the one without SHIFT.
  // TODO(komatsu): Clarify the expected algorithm for the client.
  if ((kana_unlocked_length == 0) &&
      keyboard_status_wo_kana_lock.IsPressed(VK_SHIFT) &&
      has_valid_key_string) {
    // Remove shift key.
    keyboard_status_wo_kana_lock.SetState(VK_SHIFT, 0);
    keyboard_status_wo_kana_lock.SetState(VK_LSHIFT, 0);
    keyboard_status_wo_kana_lock.SetState(VK_RSHIFT, 0);

    kana_unlocked_length =
        keyboard->ToUnicode(virtual_key.virtual_key(), to_unicode_scancode,
                            keyboard_status_wo_kana_lock.status(), &codes[0],
                            std::size(codes), to_unicode_flag);
  }

  if (kana_unlocked_length != 1) {
    return false;
  }

  // Remove the SHIFT modifier if CapsLock is not locked.
  if (modifer_keys->find(KeyEvent::CAPS) == modifer_keys->end()) {
    modifer_keys->erase(KeyEvent::SHIFT);
  }

  key->set_key_code(codes[0]);

  return true;
}

}  // namespace

KeyEventHandlerResult::KeyEventHandlerResult()
    : should_be_eaten(false),
      should_be_sent_to_server(false),
      succeeded(false) {}

KeyEventHandlerResult KeyEventHandler::HandleKey(
    const VirtualKey &virtual_key, BYTE scan_code, bool is_key_down,
    const KeyboardStatus &initial_status, const InputBehavior &behavior,
    const InputState &ime_state, Win32KeyboardInterface *keyboard,
    mozc::commands::KeyEvent *key) {
  KeyEventHandlerResult result;

  if (key == nullptr) {
    result.succeeded = false;
    result.should_be_eaten = false;
    result.should_be_sent_to_server = false;
    return result;
  }

  if (behavior.disabled) {
    result.succeeded = true;
    result.should_be_eaten = false;
    result.should_be_sent_to_server = false;
    return result;
  }

  // There exist some keys which are ideally handled but the server has not
  // supported them yet.  In order not to pass these key events to the
  // application, we will trap them but do not send them to the server.
  if (IsNotimplementedKey(virtual_key)) {
    result.succeeded = true;
    result.should_be_eaten = true;
    result.should_be_sent_to_server = false;
    return result;
  }

  if (!ConvertToKeyEvent(virtual_key, scan_code, is_key_down, false, behavior,
                         ime_state, initial_status, keyboard, key)) {
    result.succeeded = true;
    result.should_be_eaten = false;
    result.should_be_sent_to_server = false;
    return result;
  }

  // We do not handle key message unless the key is one of force activation
  // keys.
  if (!ime_state.open) {
    // TODO(yukawa): Treat VK_PACKET as a direct mode key.
    const bool is_direct_mode_command =
        is_key_down &&
        KeyInfoUtil::ContainsKey(behavior.direct_mode_keys, *key);
    if (!is_direct_mode_command) {
      result.succeeded = true;
      result.should_be_eaten = false;
      result.should_be_sent_to_server = false;
      return result;
    }

    // For historical reasons, pass the visible conversion mode to the
    // converter.
    const DWORD repotring_mode = ime_state.visible_conversion_mode;
    commands::CompositionMode mozc_mode = commands::DIRECT;
    if (!ConversionModeUtil::GetMozcModeFromNativeMode(repotring_mode,
                                                       &mozc_mode)) {
      result.succeeded = false;
      result.should_be_eaten = false;
      result.should_be_sent_to_server = false;
      return result;
    }
    key->set_activated(ime_state.open);
    key->set_mode(mozc_mode);
    result.succeeded = true;
    result.should_be_eaten = true;
    result.should_be_sent_to_server = true;
    return result;
  }

  DCHECK(ime_state.open);

  // For historical reasons, pass the visible conversion mode to the converter.
  const DWORD repotring_mode = ime_state.visible_conversion_mode;

  commands::CompositionMode mozc_mode = commands::HIRAGANA;
  if (!ConversionModeUtil::GetMozcModeFromNativeMode(repotring_mode,
                                                     &mozc_mode)) {
    result.succeeded = false;
    result.should_be_eaten = false;
    result.should_be_sent_to_server = false;
    return result;
  }

  key->set_activated(ime_state.open);
  key->set_mode(mozc_mode);

  switch (virtual_key.virtual_key()) {
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
      if (is_key_down) {
        // Will not eat this message.
        result.succeeded = true;
        result.should_be_eaten = false;
        result.should_be_sent_to_server = false;
        return result;
      }
      if (ime_state.last_down_key.virtual_key() != virtual_key.virtual_key()) {
        // Will not eat this message.
        result.succeeded = true;
        result.should_be_eaten = false;
        result.should_be_sent_to_server = false;
        return result;
      }
      // We will send this message to the server.
      result.succeeded = true;
      result.should_be_eaten = true;
      result.should_be_sent_to_server = true;
      return result;
      break;
  }

  // As commented above, we do not send key-up messages in general. Few
  // exceptional cases have already been examined.
  if (!is_key_down) {
    result.succeeded = true;
    result.should_be_eaten = false;
    result.should_be_sent_to_server = false;
    return result;
  }

  // We will send this message to the server.
  result.succeeded = true;
  result.should_be_eaten = true;
  result.should_be_sent_to_server = true;
  return result;
}

KeyEventHandlerResult KeyEventHandler::ImeProcessKey(
    const VirtualKey &virtual_key, BYTE scan_code, bool is_key_down,
    const KeyboardStatus &keyboard_status, const InputBehavior &behavior,
    const InputState &initial_state, const commands::Context &context,
    client::ClientInterface *client, Win32KeyboardInterface *keyboard,
    InputState *next_state, commands::Output *output) {
  // We will update kana-lock state even if the IME is disabled, including
  // safe-mode.
  KeyboardStatus new_keyboard_status;
  UnlockKanaLock(keyboard_status, keyboard, &new_keyboard_status);

  InputState dummy_state;
  if (next_state == nullptr) {
    next_state = &dummy_state;
  }
  *next_state = initial_state;

  Output dummy_output;
  if (output == nullptr) {
    output = &dummy_output;
  }
  output->Clear();

  // Although Mozc has not explicitly supported any key-up message, there exist
  // some situations where the client has to send key message when it receives
  // a key-up message.  Currently we have following exceptions.
  // - Shift/Control/Alt keys
  //    The Mozc protocol had originally allowed the client to ignore key-up
  //    events of these modifier keys but later has changed to expect the
  //    client to send a key message which contains only modifiers field and
  //    mode field to support b/2269058 and b/1995170.
  if (is_key_down) {
    // This is an ugly workaround to determine which key-up message for a
    // modifier key should be sent to the server.  Currently, the Mozc server
    // expects the client to send such a key-up message only when a modifier
    // key is released just after the same key is pressed, that is, any other
    // key is not pressed between the key-down and key-up of a modifier key.
    // Here are some examples, where [D] and [U] mean 'key down' and 'key up'.
    //   (1) [D]Shift -> [D]A -> [U]Shift -> [U]A
    //      In this case, only 'A' will be sent to the server
    //   (2) [D]Shift -> [U]Shift -> [D]A -> [U]A
    //      In this case, 'Shift' and 'A' will be sent to the server.
    //   (3) [D]Shift -> [D]Control -> [U]Shift -> [U]Control
    //      In this case, no key message will be sent to the server.
    //   (4) [D]Shift -> [D]Control -> [U]Control -> [U]Shift
    //      In this case, 'Control+Shift' will be sent to the server.  Note
    //      that |KeyEvent::modifier_keys| will contain all the modifier keys
    //      when the client receives '[U]Control'.
    // Unfortunately, it is currently client's responsibility to remember the
    // key sequence to generate appropriate key messages as expected by the
    // server.  Strictly speaking, the Mozc client is actually stateful in
    // this sense.
    next_state->last_down_key = virtual_key;
  }

  KeyEvent key;
  KeyEventHandlerResult result =
      HandleKey(virtual_key, scan_code, is_key_down, keyboard_status, behavior,
                initial_state, keyboard, &key);

  if (!result.succeeded) {
    return result;
  }

  if (!result.should_be_sent_to_server) {
    return result;
  }

  if (!client->TestSendKeyWithContext(key, context, output)) {
    result.succeeded = false;
    return result;
  }

  if (!output->has_consumed()) {
    result.succeeded = false;
    return result;
  }

  if (output->has_status()) {
    if (!mozc::win32::ConversionModeUtil::ConvertStatusFromMozcToNative(
            output->status(), behavior.prefer_kana_input, &next_state->open,
            &next_state->logical_conversion_mode,
            &next_state->visible_conversion_mode)) {
      result.succeeded = false;
      return result;
    }
  }

  result.should_be_eaten = output->consumed();
  return result;
}

KeyEventHandlerResult KeyEventHandler::ImeToAsciiEx(
    const VirtualKey &virtual_key, BYTE scan_code, bool is_key_down,
    const KeyboardStatus &keyboard_status, const InputBehavior &behavior,
    const InputState &initial_state, const commands::Context &context,
    client::ClientInterface *client, Win32KeyboardInterface *keyboard,
    InputState *next_state, commands::Output *output) {
  KeyboardStatus new_keyboard_status;
  UnlockKanaLock(keyboard_status, keyboard, &new_keyboard_status);

  InputState dummy_state;
  if (next_state == nullptr) {
    next_state = &dummy_state;
  }
  *next_state = initial_state;

  Output dummy_output;
  if (output == nullptr) {
    output = &dummy_output;
  }
  output->Clear();

  // Although Mozc has not explicitly supported any key-up message, there exist
  // some situations where the client has to send key message when it receives
  // a key-up message.  Currently we have following exceptions.
  // - Shift/Control/Alt keys
  //    The Mozc protocol had originally allowed the client to ignore key-up
  //    events of these modifier keys but later has changed to expect the
  //    client to send a key message which contains only modifiers field and
  //    mode field to support b/2269058 and b/1995170.
  if (is_key_down) {
    // This is an ugly workaround to determine which key-up message for a
    // modifier key should be sent to the server.  Currently, the Mozc server
    // expects the client to send such a key-up message only when a modifier
    // key is released just after the same key is pressed, that is, any other
    // key is not pressed between the key-down and key-up of a modifier key.
    // Here are some examples, where [D] and [U] mean 'key down' and 'key up'.
    //   (1) [D]Shift -> [D]A -> [U]Shift -> [U]A
    //      In this case, only 'A' will be sent to the server
    //   (2) [D]Shift -> [U]Shift -> [D]A -> [U]A
    //      In this case, 'Shift' and 'A' will be sent to the server.
    //   (3) [D]Shift -> [D]Control -> [U]Shift -> [U]Control
    //      In this case, no key message will be sent to the server.
    //   (4) [D]Shift -> [D]Control -> [U]Control -> [U]Shift
    //      In this case, 'Control+Shift' will be sent to the server.  Note
    //      that |KeyEvent::modifier_keys| will contain all the modifier keys
    //      when the client receives '[U]Control'.
    // Unfortunately, it is currently client's responsibility to remember the
    // key sequence to generate appropriate key messages as expected by the
    // server.  Strictly speaking, the Mozc client is actually stateful in
    // this sense.
    next_state->last_down_key = virtual_key;
  }

  KeyEvent key;
  KeyEventHandlerResult result =
      HandleKey(virtual_key, scan_code, is_key_down, keyboard_status, behavior,
                initial_state, keyboard, &key);

  if (!result.succeeded) {
    return result;
  }

  if (!result.should_be_sent_to_server) {
    return result;
  }

  if (!client->SendKeyWithContext(key, context, output)) {
    result.succeeded = false;
    return result;
  }

  // Launch tool if required.
  MaybeSpawnTool(client, output);

  if (!output->has_consumed()) {
    result.succeeded = false;
    return result;
  }

  if (output->has_status()) {
    if (!mozc::win32::ConversionModeUtil::ConvertStatusFromMozcToNative(
            output->status(), behavior.prefer_kana_input, &next_state->open,
            &next_state->logical_conversion_mode,
            &next_state->visible_conversion_mode)) {
      result.succeeded = false;
      return result;
    }
  }

  result.should_be_eaten = output->consumed();
  return result;
}

bool KeyEventHandler::ConvertToKeyEvent(
    const VirtualKey &virtual_key, BYTE scan_code, bool is_key_down,
    bool is_menu_active, const InputBehavior &behavior,
    const InputState &ime_state, const KeyboardStatus &keyboard_status,
    Win32KeyboardInterface *keyboard, mozc::commands::KeyEvent *key) {
  // Since Mozc protocol requires tricky conditions for modifiers, using set
  // container makes the main part of key event conversion simple rather
  // than using vector-like container.
  std::set<KeyEvent::ModifierKey> modifiers;
  const bool result = ConvertToKeyEventMain(
      virtual_key, scan_code, is_key_down, is_menu_active, behavior, ime_state,
      keyboard_status, keyboard, key, &modifiers);
  if (!result) {
    return false;
  }

  // Updates |modifier_keys| field based on the returned set of modifier keys.
  for (std::set<KeyEvent::ModifierKey>::const_iterator i = modifiers.begin();
       i != modifiers.end(); ++i) {
    key->add_modifier_keys(*i);
  }
  return true;
}

void KeyEventHandler::UnlockKanaLock(const KeyboardStatus &keyboard_status,
                                     Win32KeyboardInterface *keyboard,
                                     KeyboardStatus *new_keyboard_status) {
  KeyboardStatus dummy_status;
  if (new_keyboard_status == nullptr) {
    new_keyboard_status = &dummy_status;
  }

  *new_keyboard_status = keyboard_status;

  if (keyboard->IsKanaLocked(keyboard_status)) {
    new_keyboard_status->SetState(VK_KANA, 0);
    keyboard->SetKeyboardState(*new_keyboard_status);
  }
}

void KeyEventHandler::MaybeSpawnTool(mozc::client::ClientInterface *client,
                                     commands::Output *output) {
  // URL handling:
  // When Output::url is set, MozcTool is supposed to be launched by client.
  // At this moment, we disable this feature as it may cause security hole.
  // if (output->has_url()) {
  //   client->OpenBrowser(output->url());
  //   output->clear_url();
  // }

  // launch_tool_mode handling:
  // When Output::launch_tool_mode is set, MozcTool is supposed to be launched
  // by client with specified mode.
  // TODO(team):  move it to better place.
  if (output->has_launch_tool_mode()) {
    std::string mode;
    switch (output->launch_tool_mode()) {
      case commands::Output::CONFIG_DIALOG:
        mode = "config_dialog";
        break;
      case commands::Output::WORD_REGISTER_DIALOG:
        mode = "word_register_dialog";
        break;
      case commands::Output::DICTIONARY_TOOL:
        mode = "dictionary_tool";
        break;
      case commands::Output::NO_TOOL:
      default:
        // Do nothing.
        break;
    }
    output->clear_launch_tool_mode();
    if (!mode.empty()) {
      client->LaunchTool(mode, "");
    }
  }
}

void KeyEventHandler::UpdateBehaviorInImeProcessKey(
    const VirtualKey &virtual_key, bool is_key_down,
    const InputState &initial_state, InputBehavior *behavior) {
  if (behavior == nullptr) {
    return;
  }

  if (!initial_state.open) {
    return;
  }

  if (!behavior->use_romaji_key_to_toggle_input_style) {
    return;
  }

  switch (virtual_key.virtual_key()) {
    // Do not discriminate between VK_DBE_ROMAN and VK_DBE_NOROMAN because
    // these key states are not per-thread nor per-process but system-wide
    // or session-wide, which means that any key stroke in another
    // thread/process may change the global toggle state at any time.
    case VK_DBE_ROMAN:
    case VK_DBE_NOROMAN:
      if (is_key_down) {
        // Flip the input style if this is key down event.
        behavior->prefer_kana_input = !behavior->prefer_kana_input;
      }
      break;
    default:
      // Otherwise, do nothing.
      break;
  }
}
}  // namespace win32
}  // namespace mozc
