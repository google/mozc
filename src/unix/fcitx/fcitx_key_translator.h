/***************************************************************************
 *   Copyright (C) 2012~2012 by CSSlayer                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.              *
 ***************************************************************************/

#ifndef MOZC_UNIX_FCITX_FCITX_KEY_TRANSLATOR_H_
#define MOZC_UNIX_FCITX_FCITX_KEY_TRANSLATOR_H_

#include <map>
#include <set>
#include <string>

#include <fcitx-config/hotkey.h>

#include "base/base.h"  // for DISALLOW_COPY_AND_ASSIGN.
#include "session/commands.pb.h"
#include "unix/fcitx/fcitx_config.h"
#include <fcitx/ime.h>

namespace mozc {

namespace fcitx {

// This class is responsible for converting scim::KeyEvent object (defined in
// /usr/include/scim-1.0/scim_event.h) to IPC input for mozc_server.
class KeyTranslator {
public:
  KeyTranslator();
  virtual ~KeyTranslator();

  // Converts scim_key into Mozc key code and stores them on out_translated.
  // scim_key must satisfy the following precondition: CanConvert(scim_key)
  bool Translate(FcitxKeySym keyval,
                 uint32 keycode,
                 uint32 modifiers,
                 mozc::config::Config::PreeditMethod method,
                 bool layout_is_jp,
                 mozc::commands::KeyEvent *out_event) const;

private:
  typedef map<uint32, commands::KeyEvent::SpecialKey> SpecialKeyMap;
  typedef map<uint32, commands::KeyEvent::ModifierKey> ModifierKeyMap;
  typedef map<uint32, pair<string, string> > KanaMap;

  // Returns true iff key is modifier key such as SHIFT, ALT, or CAPSLOCK.
  bool IsModifierKey(uint32 keyval,
                     uint32 keycode,
                     uint32 modifiers) const;

  // Returns true iff key is special key such as ENTER, ESC, or PAGE_UP.
  bool IsSpecialKey(uint32 keyval,
                    uint32 keycode,
                    uint32 modifiers) const;

  // Returns true iff |keyval| is a key with a kana assigned.
  bool IsKanaAvailable(uint32 keyval,
                       uint32 keycode,
                       uint32 modifiers,
                       bool layout_is_jp,
                       string *out) const;

  // Returns true iff key is ASCII such as '0', 'A', or '!'.
  static bool IsAscii(uint32 keyval,
                      uint32 keycode,
                      uint32 modifiers);

  // Returns true iff key is printable.
  static bool IsPrintable(uint32 keyval, uint32 keycode, uint32 modifiers);

  // Returns true iff key is HiraganaKatakana with shift modifier.
  static bool IsHiraganaKatakanaKeyWithShift(uint32 keyval,
                                             uint32 keycode,
                                             uint32 modifiers);

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

  DISALLOW_COPY_AND_ASSIGN(KeyTranslator);
};

}  // namespace fcitx

}  // namespace mozc

#endif  // MOZC_UNIX_FCITX_FCITX_KEY_TRANSLATOR_H_
