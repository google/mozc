// Copyright 2010-2012, Google Inc.
// Copyright 2012~2013, Weng Xuetian <wengxt@gmail.com>
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

#ifndef MOZC_UNIX_FCITX_FCITX_KEY_TRANSLATOR_H_
#define MOZC_UNIX_FCITX_FCITX_KEY_TRANSLATOR_H_

#include <cstdint>
#include <map>
#include <set>
#include <string>

#include <fcitx-config/hotkey.h>

#include "base/port.h"
#include "protocol/commands.pb.h"
#include <fcitx/ime.h>

namespace mozc {

namespace fcitx {

// This class is responsible for converting scim::KeyEvent object (defined in
// /usr/include/scim-1.0/scim_event.h) to IPC input for mozc_server.
class KeyTranslator {
public:
  KeyTranslator();
  KeyTranslator(const KeyTranslator &) = delete;
  virtual ~KeyTranslator();

  // Converts scim_key into Mozc key code and stores them on out_translated.
  // scim_key must satisfy the following precondition: CanConvert(scim_key)
  bool Translate(FcitxKeySym keyval,
                 uint32_t keycode,
                 uint32_t modifiers,
                 mozc::config::Config::PreeditMethod method,
                 bool layout_is_jp,
                 mozc::commands::KeyEvent *out_event) const;

private:
  typedef std::map<uint32_t, commands::KeyEvent::SpecialKey> SpecialKeyMap;
  typedef std::map<uint32_t, commands::KeyEvent::ModifierKey> ModifierKeyMap;
  typedef std::map<uint32_t, std::pair<std::string, std::string> > KanaMap;

  // Returns true iff key is modifier key such as SHIFT, ALT, or CAPSLOCK.
  bool IsModifierKey(uint32_t keyval,
                     uint32_t keycode,
                     uint32_t modifiers) const;

  // Returns true iff key is special key such as ENTER, ESC, or PAGE_UP.
  bool IsSpecialKey(uint32_t keyval,
                    uint32_t keycode,
                    uint32_t modifiers) const;

  // Returns true iff |keyval| is a key with a kana assigned.
  bool IsKanaAvailable(uint32_t keyval,
                       uint32_t keycode,
                       uint32_t modifiers,
                       bool layout_is_jp,
                       std::string *out) const;

  // Returns true iff key is ASCII such as '0', 'A', or '!'.
  static bool IsAscii(uint32_t keyval,
                      uint32_t keycode,
                      uint32_t modifiers);

  // Returns true iff key is printable.
  static bool IsPrintable(uint32_t keyval, uint32_t keycode, uint32_t modifiers);

  // Returns true iff key is HiraganaKatakana with shift modifier.
  static bool IsHiraganaKatakanaKeyWithShift(uint32_t keyval,
                                             uint32_t keycode,
                                             uint32_t modifiers);

  // Initializes private fields.
  void Init();

  // Stores a mapping from ibus keys to Mozc's special keys.
  SpecialKeyMap special_key_map_;
  // Stores a mapping from ibus modifier keys to Mozc's modifier keys.
  ModifierKeyMap modifier_key_map_;
  // Stores a mapping from ibus modifier masks to Mozc's modifier keys.
  ModifierKeyMap modifier_mask_map_;
  // Stores a mapping from ASCII to Kana character. For example, ASCII character
  // '4' is mapped to Japanese 'Hiragana Letter U' (without Shift modifier) and
  // 'Hiragana Letter Small U' (with Shift modifier).
  KanaMap kana_map_jp_;  // mapping for JP keyboard.
  KanaMap kana_map_us_;  // mapping for US keyboard.
};

}  // namespace fcitx

}  // namespace mozc

#endif  // MOZC_UNIX_FCITX_FCITX_KEY_TRANSLATOR_H_
