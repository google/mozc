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

// Utilities for commands::KeyEvent.

#ifndef MOZC_COMPOSER_KEY_EVENT_UTIL_H_
#define MOZC_COMPOSER_KEY_EVENT_UTIL_H_

#include <cstdint>

#include "protocol/commands.pb.h"

namespace mozc {

using KeyInformation = uint64_t;

// This is pure static class.
class KeyEventUtil {
 public:
  KeyEventUtil() = delete;
  KeyEventUtil(const KeyEventUtil&) = delete;
  KeyEventUtil& operator=(const KeyEventUtil&) = delete;

  static uint32_t GetModifiers(const commands::KeyEvent& key_event);

  // |Modifiers(16bit)|SpecialKey(16bit)|Unicode(32bit)|
  static bool GetKeyInformation(const commands::KeyEvent& key_event,
                                KeyInformation* key);

  // Normalizes given key event for key command looking-up. This function does
  // - remove commands::KeyEvent::CAPS from the modifier keys
  // - revert the flip of alphabetical key code caused by CapsLock
  // so that shortcut keys can be used as if CapsLock was not enabled. b/5627459
  // In addition, this function removes left / right specified modifiers.
  static void NormalizeModifiers(const commands::KeyEvent& key_event,
                                 commands::KeyEvent* new_key_event);

  // Normalizes a numpad key to a normal key (e.g. NUMPAD0 => '0')
  static void NormalizeNumpadKey(const commands::KeyEvent& key_event,
                                 commands::KeyEvent* new_key_event);

  // Removes modifier keys which are specified by |remove_modifiers|.
  static void RemoveModifiers(const commands::KeyEvent& key_event,
                              uint32_t remove_modifiers,
                              commands::KeyEvent* new_key_event);

  // Returns a fallback keyevent generated from key_event. In the
  // current implementation, if the input key_event does not contains
  // any special keys or modifier keys, that printable key will be replaced
  // with the ASCII special key.
  static bool MaybeGetKeyStub(const commands::KeyEvent& key_event,
                              KeyInformation* key);

  static bool HasAlt(uint32_t modifiers);
  static bool HasCtrl(uint32_t modifiers);
  static bool HasShift(uint32_t modifiers);
  static bool HasCaps(uint32_t modifiers);

  // These functions doesn't consider CAPS.
  static bool IsAlt(uint32_t modifiers);
  static bool IsCtrl(uint32_t modifiers);
  static bool IsShift(uint32_t modifiers);
  static bool IsAltCtrl(uint32_t modifiers);
  static bool IsAltShift(uint32_t modifiers);
  static bool IsCtrlShift(uint32_t modifiers);
  static bool IsAltCtrlShift(uint32_t modifiers);

  static bool IsLowerAlphabet(const commands::KeyEvent& key_event);
  static bool IsUpperAlphabet(const commands::KeyEvent& key_event);
  static bool IsNumpadKey(const commands::KeyEvent& key_event);
};

}  // namespace mozc

#endif  // MOZC_COMPOSER_KEY_EVENT_UTIL_H_
