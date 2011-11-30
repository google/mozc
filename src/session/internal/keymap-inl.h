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

// Keymap utils of Mozc interface.

#ifndef MOZC_SESSION_INTERNAL_KEYMAP_INL_H_
#define MOZC_SESSION_INTERNAL_KEYMAP_INL_H_

#include "session/internal/keymap.h"

namespace mozc {

namespace commands {
class KeyEvent;
}  // namespace commands

namespace keymap {

template<typename T>
bool KeyMap<T>::GetCommand(const commands::KeyEvent &key_event,
                           T* command) const {
  // Shortcut keys should be available as if CapsLock was not enabled like
  // other IMEs such as MS-IME or ATOK. b/5627459
  commands::KeyEvent normalized_key_event;
  NormalizeKeyEvent(key_event, &normalized_key_event);
  Key key;
  if (!GetKey(normalized_key_event, &key)) {
    return false;
  }

  typename map<Key, T>::const_iterator it;
  it = keymap_.find(key);
  if (it != keymap_.end()) {
    *command = it->second;
    return true;
  }

  if (MaybeGetKeyStub(normalized_key_event, &key)) {
    it = keymap_.find(key);
    if (it != keymap_.end()) {
      *command = it->second;
      return true;
    }
  }
  return false;
}

template<typename T>
bool KeyMap<T>::AddRule(const commands::KeyEvent &key_event,
                        T command) {
  Key key;
  if (!GetKey(key_event, &key)) {
    return false;
  }

  keymap_[key] = command;
  return true;
}

template<typename T>
void KeyMap<T>::Clear() {
  keymap_.clear();
}

}  // namespace mozc
}  // namespace keymap

#endif  // MOZC_SESSION_INTERNAL_KEYMAP_INTERFACE_INL_H_
