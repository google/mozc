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

// workaround
#define _FCITX_LOG_H_

#include "unix/fcitx/fcitx_key_translator.h"

#include "base/logging.h"
#include <fcitx/ime.h>

namespace {

const FcitxKeySym modifier_keys[] = {
    FcitxKey_Alt_L,
    FcitxKey_Alt_R,
    FcitxKey_Caps_Lock,
    FcitxKey_Control_L,
    FcitxKey_Control_R,
    FcitxKey_Hyper_L,
    FcitxKey_Hyper_R,
    FcitxKey_Meta_L,
    FcitxKey_Meta_R,
    FcitxKey_Shift_L,
    FcitxKey_Shift_Lock,
    FcitxKey_Shift_R,
    FcitxKey_Super_L,
    FcitxKey_Super_R,
};

const struct SpecialKeyMap {
    FcitxKeySym from;
    mozc::commands::KeyEvent::SpecialKey to;
} sp_key_map[] = {
    {FcitxKey_space, mozc::commands::KeyEvent::SPACE},
    {FcitxKey_Return, mozc::commands::KeyEvent::ENTER},
    {FcitxKey_Left, mozc::commands::KeyEvent::LEFT},
    {FcitxKey_Right, mozc::commands::KeyEvent::RIGHT},
    {FcitxKey_Up, mozc::commands::KeyEvent::UP},
    {FcitxKey_Down, mozc::commands::KeyEvent::DOWN},
    {FcitxKey_Escape, mozc::commands::KeyEvent::ESCAPE},
    {FcitxKey_Delete, mozc::commands::KeyEvent::DEL},
    {FcitxKey_BackSpace, mozc::commands::KeyEvent::BACKSPACE},
    {FcitxKey_Insert, mozc::commands::KeyEvent::INSERT},
    {FcitxKey_Henkan, mozc::commands::KeyEvent::HENKAN},
    {FcitxKey_Muhenkan, mozc::commands::KeyEvent::MUHENKAN},
    {FcitxKey_Hiragana, mozc::commands::KeyEvent::KANA},
    // FcitxKey_Hiragana_Katakana requires special treatment.
    // See FcitxKeyTranslator::NormalizeHiraganaKatakanaKeyWithShift.
    {FcitxKey_Hiragana_Katakana, mozc::commands::KeyEvent::KANA},
    {FcitxKey_Katakana, mozc::commands::KeyEvent::KATAKANA},
    {FcitxKey_Eisu_toggle, mozc::commands::KeyEvent::EISU},
    {FcitxKey_Home, mozc::commands::KeyEvent::HOME},
    {FcitxKey_End, mozc::commands::KeyEvent::END},
    {FcitxKey_Tab, mozc::commands::KeyEvent::TAB},
    {FcitxKey_F1, mozc::commands::KeyEvent::F1},
    {FcitxKey_F2, mozc::commands::KeyEvent::F2},
    {FcitxKey_F3, mozc::commands::KeyEvent::F3},
    {FcitxKey_F4, mozc::commands::KeyEvent::F4},
    {FcitxKey_F5, mozc::commands::KeyEvent::F5},
    {FcitxKey_F6, mozc::commands::KeyEvent::F6},
    {FcitxKey_F7, mozc::commands::KeyEvent::F7},
    {FcitxKey_F8, mozc::commands::KeyEvent::F8},
    {FcitxKey_F9, mozc::commands::KeyEvent::F9},
    {FcitxKey_F10, mozc::commands::KeyEvent::F10},
    {FcitxKey_F11, mozc::commands::KeyEvent::F11},
    {FcitxKey_F12, mozc::commands::KeyEvent::F12},
    {FcitxKey_F13, mozc::commands::KeyEvent::F13},
    {FcitxKey_F14, mozc::commands::KeyEvent::F14},
    {FcitxKey_F15, mozc::commands::KeyEvent::F15},
    {FcitxKey_F16, mozc::commands::KeyEvent::F16},
    {FcitxKey_F17, mozc::commands::KeyEvent::F17},
    {FcitxKey_F18, mozc::commands::KeyEvent::F18},
    {FcitxKey_F19, mozc::commands::KeyEvent::F19},
    {FcitxKey_F20, mozc::commands::KeyEvent::F20},
    {FcitxKey_F21, mozc::commands::KeyEvent::F21},
    {FcitxKey_F22, mozc::commands::KeyEvent::F22},
    {FcitxKey_F23, mozc::commands::KeyEvent::F23},
    {FcitxKey_F24, mozc::commands::KeyEvent::F24},
    {FcitxKey_Page_Up, mozc::commands::KeyEvent::PAGE_UP},
    {FcitxKey_Page_Down, mozc::commands::KeyEvent::PAGE_DOWN},

    // Keypad (10-key).
    {FcitxKey_KP_0, mozc::commands::KeyEvent::NUMPAD0},
    {FcitxKey_KP_1, mozc::commands::KeyEvent::NUMPAD1},
    {FcitxKey_KP_2, mozc::commands::KeyEvent::NUMPAD2},
    {FcitxKey_KP_3, mozc::commands::KeyEvent::NUMPAD3},
    {FcitxKey_KP_4, mozc::commands::KeyEvent::NUMPAD4},
    {FcitxKey_KP_5, mozc::commands::KeyEvent::NUMPAD5},
    {FcitxKey_KP_6, mozc::commands::KeyEvent::NUMPAD6},
    {FcitxKey_KP_7, mozc::commands::KeyEvent::NUMPAD7},
    {FcitxKey_KP_8, mozc::commands::KeyEvent::NUMPAD8},
    {FcitxKey_KP_9, mozc::commands::KeyEvent::NUMPAD9},
    {FcitxKey_KP_Equal, mozc::commands::KeyEvent::EQUALS},  // [=]
    {FcitxKey_KP_Multiply, mozc::commands::KeyEvent::MULTIPLY},  // [*]
    {FcitxKey_KP_Add, mozc::commands::KeyEvent::ADD},  // [+]
    {FcitxKey_KP_Separator, mozc::commands::KeyEvent::SEPARATOR},  // enter
    {FcitxKey_KP_Subtract, mozc::commands::KeyEvent::SUBTRACT},  // [-]
    {FcitxKey_KP_Decimal, mozc::commands::KeyEvent::DECIMAL},  // [.]
    {FcitxKey_KP_Divide, mozc::commands::KeyEvent::DIVIDE},  // [/]
    {FcitxKey_KP_Space, mozc::commands::KeyEvent::SPACE},
    {FcitxKey_KP_Tab, mozc::commands::KeyEvent::TAB},
    {FcitxKey_KP_Enter, mozc::commands::KeyEvent::ENTER},
    {FcitxKey_KP_Home, mozc::commands::KeyEvent::HOME},
    {FcitxKey_KP_Left, mozc::commands::KeyEvent::LEFT},
    {FcitxKey_KP_Up, mozc::commands::KeyEvent::UP},
    {FcitxKey_KP_Right, mozc::commands::KeyEvent::RIGHT},
    {FcitxKey_KP_Down, mozc::commands::KeyEvent::DOWN},
    {FcitxKey_KP_Page_Up, mozc::commands::KeyEvent::PAGE_UP},
    {FcitxKey_KP_Page_Down, mozc::commands::KeyEvent::PAGE_DOWN},
    {FcitxKey_KP_End, mozc::commands::KeyEvent::END},
    {FcitxKey_KP_Delete, mozc::commands::KeyEvent::DEL},
    {FcitxKey_KP_Insert, mozc::commands::KeyEvent::INSERT},

    // Shift+TAB.
    {FcitxKey_ISO_Left_Tab, mozc::commands::KeyEvent::TAB},

    //   - FcitxKey_Kana_Lock? FcitxKey_Kana_Shift?
};

const struct SpecialAsciiMap {
    uint32 from;
    uint32 to;
} sp_ascii_map[] = {
    {FcitxKey_KP_Equal, '='},
};

const struct KanaMap {
    uint32 keysym;
    const char *kana;
} kKanaMapJp[] = {
    { '1' , "\xe3\x81\xac" },  // "ぬ"
    { '!' , "\xe3\x81\xac" },  // "ぬ"
    { '2' , "\xe3\x81\xb5" },  // "ふ"
    { '\"', "\xe3\x81\xb5" },  // "ふ"
    { '3' , "\xe3\x81\x82" },  // "あ"
    { '#' , "\xe3\x81\x81" },  // "ぁ"
    { '4' , "\xe3\x81\x86" },  // "う"
    { '$' , "\xe3\x81\x85" },  // "ぅ"
    { '5' , "\xe3\x81\x88" },  // "え"
    { '%' , "\xe3\x81\x87" },  // "ぇ"
    { '6' , "\xe3\x81\x8a" },  // "お"
    { '&' , "\xe3\x81\x89" },  // "ぉ"
    { '7' , "\xe3\x82\x84" },  // "や"
    { '\'', "\xe3\x82\x83" },  // "ゃ"
    { '8' , "\xe3\x82\x86" },  // "ゆ"
    { '(' , "\xe3\x82\x85" },  // "ゅ"
    { '9' , "\xe3\x82\x88" },  // "よ"
    { ')' , "\xe3\x82\x87" },  // "ょ"
    { '0' , "\xe3\x82\x8f" },  // "わ"
    // Shift+0 is usually mapped to tilde by XKB.
    { '-' , "\xe3\x81\xbb" },  // "ほ"
    { '=' , "\xe3\x81\xbb" },  // "ほ"
    { '^' , "\xe3\x81\xb8" },  // "へ"
    { '~' , "\xe3\x82\x92" },  // "を"
    { '|' , "\xe3\x83\xbc" },  // "ー"
    { 'q' , "\xe3\x81\x9f" },  // "た"
    { 'Q' , "\xe3\x81\x9f" },  // "た"
    { 'w' , "\xe3\x81\xa6" },  // "て"
    { 'W' , "\xe3\x81\xa6" },  // "て"
    { 'e' , "\xe3\x81\x84" },  // "い"
    { 'E' , "\xe3\x81\x83" },  // "ぃ"
    { 'r' , "\xe3\x81\x99" },  // "す"
    { 'R' , "\xe3\x81\x99" },  // "す"
    { 't' , "\xe3\x81\x8b" },  // "か"
    { 'T' , "\xe3\x81\x8b" },  // "か"
    { 'y' , "\xe3\x82\x93" },  // "ん"
    { 'Y' , "\xe3\x82\x93" },  // "ん"
    { 'u' , "\xe3\x81\xaa" },  // "な"
    { 'U' , "\xe3\x81\xaa" },  // "な"
    { 'i' , "\xe3\x81\xab" },  // "に"
    { 'I' , "\xe3\x81\xab" },  // "に"
    { 'o' , "\xe3\x82\x89" },  // "ら"
    { 'O' , "\xe3\x82\x89" },  // "ら"
    { 'p' , "\xe3\x81\x9b" },  // "せ"
    { 'P' , "\xe3\x81\x9b" },  // "せ"
    { '@' , "\xe3\x82\x9b" },  // "゛"
    { '`' , "\xe3\x82\x9b" },  // "゛"
    { '[' , "\xe3\x82\x9c" },  // "゜"
    { '{' , "\xe3\x80\x8c" },  // "「"
    { 'a' , "\xe3\x81\xa1" },  // "ち"
    { 'A' , "\xe3\x81\xa1" },  // "ち"
    { 's' , "\xe3\x81\xa8" },  // "と"
    { 'S' , "\xe3\x81\xa8" },  // "と"
    { 'd' , "\xe3\x81\x97" },  // "し"
    { 'D' , "\xe3\x81\x97" },  // "し"
    { 'f' , "\xe3\x81\xaf" },  // "は"
    { 'F' , "\xe3\x81\xaf" },  // "は"
    { 'g' , "\xe3\x81\x8d" },  // "き"
    { 'G' , "\xe3\x81\x8d" },  // "き"
    { 'h' , "\xe3\x81\x8f" },  // "く"
    { 'H' , "\xe3\x81\x8f" },  // "く"
    { 'j' , "\xe3\x81\xbe" },  // "ま"
    { 'J' , "\xe3\x81\xbe" },  // "ま"
    { 'k' , "\xe3\x81\xae" },  // "の"
    { 'K' , "\xe3\x81\xae" },  // "の"
    { 'l' , "\xe3\x82\x8a" },  // "り"
    { 'L' , "\xe3\x82\x8a" },  // "り"
    { ';' , "\xe3\x82\x8c" },  // "れ"
    { '+' , "\xe3\x82\x8c" },  // "れ"
    { ':' , "\xe3\x81\x91" },  // "け"
    { '*' , "\xe3\x81\x91" },  // "け"
    { ']' , "\xe3\x82\x80" },  // "む"
    { '}' , "\xe3\x80\x8d" },  // "」"
    { 'z' , "\xe3\x81\xa4" },  // "つ"
    { 'Z' , "\xe3\x81\xa3" },  // "っ"
    { 'x' , "\xe3\x81\x95" },  // "さ"
    { 'X' , "\xe3\x81\x95" },  // "さ"
    { 'c' , "\xe3\x81\x9d" },  // "そ"
    { 'C' , "\xe3\x81\x9d" },  // "そ"
    { 'v' , "\xe3\x81\xb2" },  // "ひ"
    { 'V' , "\xe3\x81\xb2" },  // "ひ"
    { 'b' , "\xe3\x81\x93" },  // "こ"
    { 'B' , "\xe3\x81\x93" },  // "こ"
    { 'n' , "\xe3\x81\xbf" },  // "み"
    { 'N' , "\xe3\x81\xbf" },  // "み"
    { 'm' , "\xe3\x82\x82" },  // "も"
    { 'M' , "\xe3\x82\x82" },  // "も"
    { ',' , "\xe3\x81\xad" },  // "ね"
    { '<' , "\xe3\x80\x81" },  // "、"
    { '.' , "\xe3\x82\x8b" },  // "る"
    { '>' , "\xe3\x80\x82" },  // "。"
    { '/' , "\xe3\x82\x81" },  // "め"
    { '?' , "\xe3\x83\xbb" },  // "・"
    { '_' , "\xe3\x82\x8d" },  // "ろ"
    // A backslash is handled in a special way because it is input by
    // two different keys (the one next to Backslash and the one next
    // to Right Shift).
    { '\\', "" },
}, kKanaMapUs[] = {
    { '`' , "\xe3\x82\x8d" },  // "ろ" - Different from the Jp mapping.
    { '~' , "\xe3\x82\x8d" },  // "ろ" - Different from the Jp mapping.
    { '1' , "\xe3\x81\xac" },  // "ぬ"
    { '!' , "\xe3\x81\xac" },  // "ぬ"
    { '2' , "\xe3\x81\xb5" },  // "ふ"
    { '@' , "\xe3\x81\xb5" },  // "ふ"
    { '3' , "\xe3\x81\x82" },  // "あ"
    { '#' , "\xe3\x81\x81" },  // "ぁ"
    { '4' , "\xe3\x81\x86" },  // "う"
    { '$' , "\xe3\x81\x85" },  // "ぅ"
    { '5' , "\xe3\x81\x88" },  // "え"
    { '%' , "\xe3\x81\x87" },  // "ぇ"
    { '6' , "\xe3\x81\x8a" },  // "お"
    { '^' , "\xe3\x81\x89" },  // "ぉ"
    { '7' , "\xe3\x82\x84" },  // "や"
    { '&' , "\xe3\x82\x83" },  // "ゃ"
    { '8' , "\xe3\x82\x86" },  // "ゆ"
    { '*' , "\xe3\x82\x85" },  // "ゅ"
    { '9' , "\xe3\x82\x88" },  // "よ"
    { '(' , "\xe3\x82\x87" },  // "ょ"
    { '0' , "\xe3\x82\x8f" },  // "わ"
    { ')' , "\xe3\x82\x92" },  // "を"
    { '-' , "\xe3\x81\xbb" },  // "ほ"
    { '_' , "\xe3\x83\xbc" },  // "ー" - Different from the Jp mapping.
    { '=' , "\xe3\x81\xb8" },  // "へ"
    { '+' , "\xe3\x81\xb8" },  // "へ"
    { 'q' , "\xe3\x81\x9f" },  // "た"
    { 'Q' , "\xe3\x81\x9f" },  // "た"
    { 'w' , "\xe3\x81\xa6" },  // "て"
    { 'W' , "\xe3\x81\xa6" },  // "て"
    { 'e' , "\xe3\x81\x84" },  // "い"
    { 'E' , "\xe3\x81\x83" },  // "ぃ"
    { 'r' , "\xe3\x81\x99" },  // "す"
    { 'R' , "\xe3\x81\x99" },  // "す"
    { 't' , "\xe3\x81\x8b" },  // "か"
    { 'T' , "\xe3\x81\x8b" },  // "か"
    { 'y' , "\xe3\x82\x93" },  // "ん"
    { 'Y' , "\xe3\x82\x93" },  // "ん"
    { 'u' , "\xe3\x81\xaa" },  // "な"
    { 'U' , "\xe3\x81\xaa" },  // "な"
    { 'i' , "\xe3\x81\xab" },  // "に"
    { 'I' , "\xe3\x81\xab" },  // "に"
    { 'o' , "\xe3\x82\x89" },  // "ら"
    { 'O' , "\xe3\x82\x89" },  // "ら"
    { 'p' , "\xe3\x81\x9b" },  // "せ"
    { 'P' , "\xe3\x81\x9b" },  // "せ"
    { '[' , "\xe3\x82\x9b" },  // "゛"
    { '{' , "\xe3\x82\x9b" },  // "゛"
    { ']' , "\xe3\x82\x9c" },  // "゜"
    { '}' , "\xe3\x80\x8c" },  // "「"
    { '\\', "\xe3\x82\x80" },  // "む" - Different from the Jp mapping.
    { '|' , "\xe3\x80\x8d" },  // "」" - Different from the Jp mapping.
    { 'a' , "\xe3\x81\xa1" },  // "ち"
    { 'A' , "\xe3\x81\xa1" },  // "ち"
    { 's' , "\xe3\x81\xa8" },  // "と"
    { 'S' , "\xe3\x81\xa8" },  // "と"
    { 'd' , "\xe3\x81\x97" },  // "し"
    { 'D' , "\xe3\x81\x97" },  // "し"
    { 'f' , "\xe3\x81\xaf" },  // "は"
    { 'F' , "\xe3\x81\xaf" },  // "は"
    { 'g' , "\xe3\x81\x8d" },  // "き"
    { 'G' , "\xe3\x81\x8d" },  // "き"
    { 'h' , "\xe3\x81\x8f" },  // "く"
    { 'H' , "\xe3\x81\x8f" },  // "く"
    { 'j' , "\xe3\x81\xbe" },  // "ま"
    { 'J' , "\xe3\x81\xbe" },  // "ま"
    { 'k' , "\xe3\x81\xae" },  // "の"
    { 'K' , "\xe3\x81\xae" },  // "の"
    { 'l' , "\xe3\x82\x8a" },  // "り"
    { 'L' , "\xe3\x82\x8a" },  // "り"
    { ';' , "\xe3\x82\x8c" },  // "れ"
    { ':' , "\xe3\x82\x8c" },  // "れ"
    { '\'', "\xe3\x81\x91" },  // "け"
    { '\"', "\xe3\x81\x91" },  // "け"
    { 'z' , "\xe3\x81\xa4" },  // "つ"
    { 'Z' , "\xe3\x81\xa3" },  // "っ"
    { 'x' , "\xe3\x81\x95" },  // "さ"
    { 'X' , "\xe3\x81\x95" },  // "さ"
    { 'c' , "\xe3\x81\x9d" },  // "そ"
    { 'C' , "\xe3\x81\x9d" },  // "そ"
    { 'v' , "\xe3\x81\xb2" },  // "ひ"
    { 'V' , "\xe3\x81\xb2" },  // "ひ"
    { 'b' , "\xe3\x81\x93" },  // "こ"
    { 'B' , "\xe3\x81\x93" },  // "こ"
    { 'n' , "\xe3\x81\xbf" },  // "み"
    { 'N' , "\xe3\x81\xbf" },  // "み"
    { 'm' , "\xe3\x82\x82" },  // "も"
    { 'M' , "\xe3\x82\x82" },  // "も"
    { ',' , "\xe3\x81\xad" },  // "ね"
    { '<' , "\xe3\x80\x81" },  // "、"
    { '.' , "\xe3\x82\x8b" },  // "る"
    { '>' , "\xe3\x80\x82" },  // "。"
    { '/' , "\xe3\x82\x81" },  // "め"
    { '?' , "\xe3\x83\xbb" },  // "・"
};

}  // namespace

static inline char get_ascii_code(FcitxKeySym sym)
{
    if (sym >= FcitxKey_space && sym <= FcitxKey_asciitilde)
        return (char) (sym & 0xff);

    if (sym >= FcitxKey_KP_0 && sym <= FcitxKey_KP_9)
        return (char) ((sym - FcitxKey_KP_0 + FcitxKey_0) & 0xff);

    if (sym == FcitxKey_Return)
        return 0x0d;
    if (sym == FcitxKey_Linefeed)
        return 0x0a;
    if (sym == FcitxKey_Tab)
        return 0x09;
    if (sym == FcitxKey_BackSpace)
        return 0x08;
    if (sym == FcitxKey_Escape)
        return 0x1b;

    return 0;
}

namespace mozc {

namespace fcitx {

KeyTranslator::KeyTranslator() {
    InitializeKeyMaps();
    input = 0;
    layout = FcitxMozcLayout_Japanese;
}

KeyTranslator::~KeyTranslator() {
}

void KeyTranslator::Translate(
    FcitxKeySym origsym, unsigned int origstate, mozc::config::Config::PreeditMethod method,
    mozc::commands::KeyEvent *out_event) const {
    FcitxKeySym sym;
    unsigned int state;
    NormalizeHiraganaKatakanaKeyWithShift(origsym, origstate, &sym, &state);
    DCHECK(CanConvert(sym, state));
    if (!CanConvert(sym, state)) {
        LOG(ERROR) << "Can't handle the key: " << sym;
        return;
    }
    DCHECK(out_event);
    if (!out_event) {
        return;
    }
    out_event->Clear();


    if ((state & FcitxKeyState_Ctrl) != 0) {
        out_event->add_modifier_keys(mozc::commands::KeyEvent::CTRL);
    }
    if ((state & FcitxKeyState_Alt) != 0) {
        out_event->add_modifier_keys(mozc::commands::KeyEvent::ALT);
    }
    if (!IsAscii(sym, state) && (state & FcitxKeyState_Shift) != 0) {
        out_event->add_modifier_keys(mozc::commands::KeyEvent::SHIFT);
    }

    mozc::commands::KeyEvent::SpecialKey sp_key;
    uint32 sp_ascii;
    string key_string;

    if (IsSpecialKey(sym, state, &sp_key)) {
        out_event->set_special_key(sp_key);
    } else if (IsSpecialAscii(sym, state, &sp_ascii)) {
        out_event->set_key_code(sp_ascii);
    } else if (method == mozc::config::Config::KANA &&
               IsKanaAvailable(sym, state, &key_string)) {
        DCHECK(IsAscii(sym, state));
        out_event->set_key_code(get_ascii_code(sym));
        out_event->set_key_string(key_string);
    } else {
        DCHECK(IsAscii(sym, state));
        out_event->set_key_code(get_ascii_code(sym));
    }
    return;
}

void KeyTranslator::TranslateClick(
    int32 unique_id, mozc::commands::SessionCommand *out_command) const {
    DCHECK(out_command);
    if (out_command) {
        out_command->set_type(mozc::commands::SessionCommand::SELECT_CANDIDATE);
        out_command->set_id(unique_id);
    }
    return;
}

bool KeyTranslator::CanConvert(FcitxKeySym sym, unsigned int state) const {
    if (IsModifierKey(sym, state)) {
        VLOG(1) << "modifier key";
        return false;
    }
    if (IsAscii(sym, state) || IsSpecialKey(sym, state, NULL) || IsSpecialAscii(sym, state, NULL)) {
        return true;
    }

    char buf[64];
    ::snprintf(
        buf, sizeof(buf), "Key code Mozc doesn't know (0x%08x).", sym);
    LOG(ERROR) << buf;
    return false;
}

void KeyTranslator::InitializeKeyMaps() {
    for (int i = 0; i < arraysize(sp_key_map); ++i) {
        CHECK(special_key_map_.insert(make_pair(sp_key_map[i].from,
                                                sp_key_map[i].to)).second);
    }
    for (int i = 0; i < arraysize(sp_ascii_map); ++i) {
        CHECK(special_ascii_map_.insert(make_pair(sp_ascii_map[i].from,
                                        sp_ascii_map[i].to)).second);
    }
    for (int i = 0; i < arraysize(modifier_keys); ++i) {
        CHECK(modifier_keys_.insert(modifier_keys[i]).second);
    }
    for (int i = 0; i < arraysize(kKanaMapJp); ++i) {
        CHECK(kana_map_jp_.insert(
                  make_pair(kKanaMapJp[i].keysym, kKanaMapJp[i].kana)).second);
    }
    for (int i = 0; i < arraysize(kKanaMapUs); ++i) {
        CHECK(kana_map_us_.insert(
                  make_pair(kKanaMapUs[i].keysym, kKanaMapUs[i].kana)).second);
    }
}

bool KeyTranslator::IsModifierKey(FcitxKeySym sym, unsigned int state) const {
    return modifier_keys_.find(sym) != modifier_keys_.end();
}

bool KeyTranslator::IsSpecialKey(
    FcitxKeySym sym, unsigned int state,
    mozc::commands::KeyEvent::SpecialKey *out) const {
    map<uint32, mozc::commands::KeyEvent::SpecialKey>::const_iterator iter =
        special_key_map_.find(sym);
    if (iter == special_key_map_.end()) {
        return false;
    }
    if (out) {
        *out = iter->second;
    }
    return true;
}

// static
void KeyTranslator::NormalizeHiraganaKatakanaKeyWithShift(
    FcitxKeySym origsym, unsigned int origstate, FcitxKeySym* sym, unsigned int* state) {
    if (origsym == FcitxKey_Hiragana_Katakana) {
        *sym = FcitxKey_Katakana;
        *state = origstate & ~FcitxKeyState_Shift;
    }
    else {
        *sym = origsym;
        *state = origstate;
    }
}



bool KeyTranslator::IsSpecialAscii(
    FcitxKeySym sym, unsigned int state, uint32 *out) const {
    map<uint32, uint32>::const_iterator iter = special_ascii_map_.find(sym);
    if (iter == special_ascii_map_.end()) {
        return false;
    }
    if (out) {
        *out = iter->second;
    }
    return true;
}

void KeyTranslator::SetLayout(FcitxMozcLayout l)
{
    layout = l;
}

void KeyTranslator::SetInputState(FcitxInputState* i)
{
    input = i;
}


bool KeyTranslator::IsKanaAvailable(
    FcitxKeySym sym, unsigned int state, string *out) const {
    if ((state & FcitxKeyState_Ctrl) != 0 || (state & FcitxKeyState_Alt) != 0) {
        return false;
    }
    const map<uint32, const char *> &kana_map =
        IsJapaneseLayout(layout) ? kana_map_jp_ : kana_map_us_;

    // We call get_ascii_code() to support clients that does not send the shift
    // modifier. By calling the function, both "Shift + 3" and "#" would be
    // normalized to '#'.
    const char ascii_code = get_ascii_code(sym);

    map<uint32, const char *>::const_iterator iter = kana_map.find(ascii_code);
    if (iter == kana_map.end()) {
        return false;
    }
    if (out) {
        if (ascii_code == '\\' && IsJapaneseLayout(layout)) {
            uint32_t keycode = 0;
            if (input)
                keycode = FcitxInputStateGetKeyCode(input);
            if (keycode == FcitxKey_bar) {
                *out = "\xe3\x83\xbc";  // "ー"
            } else {
                *out = "\xe3\x82\x8d";  // "ろ"
            }
        } else {
            *out = iter->second;
        }
    }
    return true;
}

/* static */
bool KeyTranslator::IsAscii(FcitxKeySym sym, unsigned int state) {
    // get_ascii_code(sym) returns non-zero value for SPACE, ENTER, LineFeed,
    // TAB, BACKSPACE, ESCAPE, and Keypad codes. So we don't use it here.
    return (sym > FcitxKey_space &&
            // Note: Space key (0x20) is a special key in Mozc.
            sym <= FcitxKey_asciitilde);  // 0x7e.
}

/* static */
bool KeyTranslator::IsJapaneseLayout(FcitxMozcLayout layout) {
    // We guess that most people using the Kana input mode uses Japanese
    // keyboards, so we prefer applying the Japanese layout.
    return layout == FcitxMozcLayout_Japanese;
}

} // namespace fcitx

}  // namespace mozc
