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
    ~KeyTranslator();

    // Converts scim_key into Mozc key code and stores them on out_translated.
    // scim_key must satisfy the following precondition: CanConvert(scim_key)
    void Translate(FcitxKeySym sym,
                   unsigned int state,
                   mozc::config::Config::PreeditMethod method,
                   mozc::commands::KeyEvent *out_event) const;

    // Converts 'left click on a candidate window' into Mozc message.
    // unique_id: Unique identifier of the clicked candidate.
    void TranslateClick(int32 unique_id,
                        mozc::commands::SessionCommand *out_command) const;

    // Returns true iff scim_key can be converted to mozc::commands::Input.
    // Note: some keys and key events, such as 'key released', 'modifier key
    // pressed', or 'special key that Mozc doesn't know pressed' cannot be
    // converted to mozc::commands::Input.
    bool CanConvert(FcitxKeySym sym, unsigned int state) const;

    void SetLayout(FcitxMozcLayout l);

    void SetInputState(FcitxInputState* i);

private:
    // Returns true iff scim_key is modifier key such as SHIFT, ALT, or CAPSLOCK.
    bool IsModifierKey(FcitxKeySym sym, unsigned int state) const;

    // Returns true iff scim_key is special key such as ENTER, ESC, or PAGE_UP.
    bool IsSpecialKey(FcitxKeySym sym, unsigned int state,
                      mozc::commands::KeyEvent::SpecialKey *out) const;

    // Returns a normalized key event iff key is HiraganaKatakana with shift
    // modifier. See http://code.google.com/p/mozc/issues/detail?id=136 for
    // the background information.
    // Otherwire returns the original key.
    static void NormalizeHiraganaKatakanaKeyWithShift(
        FcitxKeySym origsym, unsigned int origstate, FcitxKeySym* sym, unsigned int* state);

    // Returns true iff scim_key is special key that can be converted to ASCII.
    // For example, scim::FCITX_KEY_KP_0 (numeric keypad zero) in FCITX can be
    // treated as ASCII code '0' in Mozc.
    bool IsSpecialAscii(FcitxKeySym sym, unsigned int state, uint32 *out) const;

    // Returns true iff scim_key is key with a kana assigned.
    bool IsKanaAvailable(FcitxKeySym sym, unsigned int state, string *out) const;

    // Returns true iff scim_key is ASCII such as '0', 'A', or '!'.
    static bool IsAscii(FcitxKeySym sym, unsigned int state);

    // Returns true iff kana_map_jp_ is to be used.
    static bool IsJapaneseLayout(FcitxMozcLayout layout);

    // Initializes private fields.
    void InitializeKeyMaps();

    map<uint32, mozc::commands::KeyEvent::SpecialKey> special_key_map_;
    set<uint32> modifier_keys_;
    map<uint32, uint32> special_ascii_map_;
    map<uint32, const char *> kana_map_jp_;
    map<uint32, const char *> kana_map_us_;
    FcitxInputState* input;
    FcitxMozcLayout layout;

    DISALLOW_COPY_AND_ASSIGN(KeyTranslator);
};

}  // namespace fcitx

}  // namespace mozc

#endif  // MOZC_UNIX_FCITX_FCITX_KEY_TRANSLATOR_H_
