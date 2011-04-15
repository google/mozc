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

// Normalizer of key events

#ifndef MOZC_SESSION_KEY_EVENT_NORMALIZER_H_
#define MOZC_SESSION_KEY_EVENT_NORMALIZER_H_

#include "base/base.h"

namespace mozc {
namespace commands {
class KeyEvent;
}

class KeyEventNormalizer {
 public:
  static bool ToUint64(const commands::KeyEvent &key_event, uint64 *key) {
    const uint16 modifier_keys = GetModifiers(key_event);
    const uint16 special_key = key_event.has_special_key() ?
      key_event.special_key() : commands::KeyEvent::NO_SPECIALKEY;
    const uint32 key_code = key_event.has_key_code() ?
      key_event.key_code() : 0;

    // Make sure the translation from the obsolete spesification.
    // key_code should no longer contain control characters.
    if (0 < key_code && key_code <= 32) {
      return false;
    }

    // uint64 = |Modifiers(16bit)|SpecialKey(16bit)|Unicode(32bit)|.
    *key = (static_cast<uint64>(modifier_keys) << 48) +
      (static_cast<uint64>(special_key) << 32) + static_cast<uint64>(key_code);
    return true;
  }
 private:
  static uint16 GetModifiers(const commands::KeyEvent &key_event) {
    uint16 modifiers = 0;
    if (key_event.has_modifiers()) {
      modifiers = key_event.modifiers();
    } else {
      for (size_t i = 0; i < key_event.modifier_keys_size(); ++i) {
        modifiers |= key_event.modifier_keys(i);
      }
    }
    return modifiers;
  }

  // Should never been allocated.
  DISALLOW_COPY_AND_ASSIGN(KeyEventNormalizer);
};

}  // namespace mozc

#endif  // MOZC_SESSION_KEY_EVENT_NORMALIZER_H_
