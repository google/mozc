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

#include "session/key_event_util.h"

#include <cctype>

#include "session/commands.pb.h"

namespace mozc {
using commands::KeyEvent;

namespace {
const uint32 kAltMask =
    KeyEvent::ALT | KeyEvent::LEFT_ALT | KeyEvent::RIGHT_ALT;
const uint32 kCtrlMask =
    KeyEvent::CTRL | KeyEvent::LEFT_CTRL | KeyEvent::RIGHT_CTRL;
const uint32 kShiftMask =
    KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT | KeyEvent::RIGHT_SHIFT;
const uint32 kCapsMask = KeyEvent::CAPS;

uint32 DropCapsFromModifiers(uint32 modifiers) {
  return modifiers & ~kCapsMask;
}
}  // namespace

uint32 KeyEventUtil::GetModifiers(const KeyEvent &key_event) {
  uint32 modifiers = 0;
  if (key_event.has_modifiers()) {
    modifiers = key_event.modifiers();
  } else {
    for (size_t i = 0; i < key_event.modifier_keys_size(); ++i) {
      modifiers |= key_event.modifier_keys(i);
    }
  }
  return modifiers;
}

bool KeyEventUtil::GetKeyInformation(const KeyEvent &key_event,
                                     KeyInformation *key) {
  DCHECK(key);

  const uint16 modifier_keys = static_cast<uint16>(GetModifiers(key_event));
  const uint16 special_key = key_event.has_special_key() ?
      key_event.special_key() : commands::KeyEvent::NO_SPECIALKEY;
  const uint32 key_code = key_event.has_key_code() ?
      key_event.key_code() : 0;

  // Make sure the translation from the obsolete spesification.
  // key_code should no longer contain control characters.
  if (0 < key_code && key_code <= 32) {
    return false;
  }

  *key =
      (static_cast<KeyInformation>(modifier_keys) << 48) |
      (static_cast<KeyInformation>(special_key) << 32) |
      (static_cast<KeyInformation>(key_code));

  return true;
}

void KeyEventUtil::NormalizeCaps(const commands::KeyEvent &key_event,
                                 commands::KeyEvent *new_key_event) {
  DCHECK(new_key_event);

  const uint32 kIgnorableModifierMask = commands::KeyEvent::CAPS;

  NormalizeKeyEventInternal(key_event, kIgnorableModifierMask, new_key_event);
}

void KeyEventUtil::NormalizeKeyEvent(const commands::KeyEvent &key_event,
                                     commands::KeyEvent *new_key_event) {
  DCHECK(new_key_event);

  const uint32 kIgnorableModifierMask =
      (commands::KeyEvent::CAPS |
       commands::KeyEvent::LEFT_ALT | commands::KeyEvent::RIGHT_ALT |
       commands::KeyEvent::LEFT_CTRL | commands::KeyEvent::RIGHT_CTRL |
       commands::KeyEvent::LEFT_SHIFT | commands::KeyEvent::RIGHT_SHIFT);

  NormalizeKeyEventInternal(key_event, kIgnorableModifierMask, new_key_event);
}

void KeyEventUtil::NormalizeKeyEventInternal(
    const commands::KeyEvent &key_event, uint32 ignorable_modifier_mask,
    commands::KeyEvent *new_key_event) {
  DCHECK(new_key_event);
  new_key_event->CopyFrom(key_event);

  // CTRL (or ALT, SHIFT) should be set on modifier_keys when
  // LEFT (or RIGHT) ctrl is set.
  // LEFT_CTRL (or others) is not handled on Japanese, so we remove these.

  // Remvoes ignorable keys.
  new_key_event->clear_modifier_keys();
  for (size_t i = 0; i < key_event.modifier_keys_size(); ++i) {
    const commands::KeyEvent::ModifierKey mod_key = key_event.modifier_keys(i);
    if (!(ignorable_modifier_mask & mod_key)) {
      new_key_event->add_modifier_keys(mod_key);
    }
  }

  // Reverts the flip of alphabetical key events caused by CapsLock.
  const uint32 original_modifiers = GetModifiers(key_event);
  if ((original_modifiers & commands::KeyEvent::CAPS) &&
      key_event.has_key_code()) {
    const int key_code = key_event.key_code();
    if ('A' <= key_code && key_code <= 'Z') {
      new_key_event->set_key_code(key_code - 'A' + 'a');
    } else if ('a' <= key_code && key_code <= 'z') {
      new_key_event->set_key_code(key_code - 'a' + 'A');
    }
  }
}

bool KeyEventUtil::MaybeGetKeyStub(const commands::KeyEvent &key_event,
                                   KeyInformation *key) {
  DCHECK(key);

  // If any modifier keys were pressed, this function does nothing.
  if (KeyEventUtil::GetModifiers(key_event) != 0) {
    return false;
  }

  // No stub rule is supported for special keys yet.
  if (key_event.has_special_key()) {
    return false;
  }

  if (!key_event.has_key_code() || key_event.key_code() <= 32) {
    return false;
  }

  commands::KeyEvent stub_key_event;
  stub_key_event.set_special_key(commands::KeyEvent::ASCII);
  if (!GetKeyInformation(stub_key_event, key)) {
    return false;
  }

  return true;
}

bool KeyEventUtil::HasAlt(uint32 modifiers) {
  return modifiers & kAltMask;
}

bool KeyEventUtil::HasCtrl(uint32 modifiers) {
  return modifiers & kCtrlMask;
}

bool KeyEventUtil::HasShift(uint32 modifiers) {
  return modifiers & kShiftMask;
}

bool KeyEventUtil::HasCaps(uint32 modifiers) {
  return modifiers & kCapsMask;
}

bool KeyEventUtil::IsAlt(uint32 modifiers) {
  modifiers = DropCapsFromModifiers(modifiers);
  return (modifiers & kAltMask) && ((modifiers & kAltMask) == modifiers);
}

bool KeyEventUtil::IsCtrl(uint32 modifiers) {
  modifiers = DropCapsFromModifiers(modifiers);
  return (modifiers & kCtrlMask) && ((modifiers & kCtrlMask) == modifiers);
}

bool KeyEventUtil::IsShift(uint32 modifiers) {
  modifiers = DropCapsFromModifiers(modifiers);
  return (modifiers & kShiftMask) && ((modifiers & kShiftMask) == modifiers);
}

bool KeyEventUtil::IsAltCtrl(uint32 modifiers) {
  modifiers = DropCapsFromModifiers(modifiers);
  return (modifiers & kAltMask) &&
      (modifiers & kCtrlMask) &&
      ((modifiers & (kAltMask | kCtrlMask)) == modifiers);
}

bool KeyEventUtil::IsAltShift(uint32 modifiers) {
  modifiers = DropCapsFromModifiers(modifiers);
  return (modifiers & kAltMask) &&
      (modifiers & kShiftMask) &&
      ((modifiers & (kAltMask | kShiftMask)) == modifiers);
}

bool KeyEventUtil::IsCtrlShift(uint32 modifiers) {
  modifiers = DropCapsFromModifiers(modifiers);
  return (modifiers & kCtrlMask) &&
      (modifiers & kShiftMask) &&
      ((modifiers & (kCtrlMask | kShiftMask)) == modifiers);
}

bool KeyEventUtil::IsAltCtrlShift(uint32 modifiers) {
  modifiers = DropCapsFromModifiers(modifiers);
  return (modifiers & kAltMask) &&
      (modifiers & kCtrlMask) &&
      (modifiers & kShiftMask) &&
      ((modifiers & (kAltMask | kCtrlMask | kShiftMask)) == modifiers);
}

bool KeyEventUtil::IsLowerAlphabet(const commands::KeyEvent &key_event) {
  if (!key_event.has_key_code()) {
    return false;
  }

  const uint32 key_code = key_event.key_code();
  const uint32 modifier_keys = GetModifiers(key_event);
  const bool change_case = (HasShift(modifier_keys) != HasCaps(modifier_keys));

  if (change_case) {
    return isupper(key_code);
  } else {
    return islower(key_code);
  }
}

bool KeyEventUtil::IsUpperAlphabet(const commands::KeyEvent &key_event) {
  if (!key_event.has_key_code()) {
    return false;
  }

  const uint32 key_code = key_event.key_code();
  const uint32 modifier_keys = GetModifiers(key_event);
  const bool change_case = (HasShift(modifier_keys) != HasCaps(modifier_keys));

  if (change_case) {
    return islower(key_code);
  } else {
    return isupper(key_code);
  }
}

}  // namespace mozc
