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

#include "unix/fcitx/fcitx_key_translator.h"

#include "base/logging.h"

namespace {

const struct SpecialKeyMap {
  uint32 from;
  mozc::commands::KeyEvent::SpecialKey to;
} special_key_map[] = {
  {FcitxKey_VoidSymbol, mozc::commands::KeyEvent::NO_SPECIALKEY},
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
  {FcitxKey_Hiragana_Katakana, mozc::commands::KeyEvent::KANA},
  {FcitxKey_Katakana, mozc::commands::KeyEvent::KATAKANA},
  {FcitxKey_Zenkaku, mozc::commands::KeyEvent::HANKAKU},
  {FcitxKey_Hankaku, mozc::commands::KeyEvent::HANKAKU},
  {FcitxKey_Zenkaku_Hankaku, mozc::commands::KeyEvent::HANKAKU},
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
  {FcitxKey_Caps_Lock, mozc::commands::KeyEvent::CAPS_LOCK},

  // Shift+TAB.
  {FcitxKey_ISO_Left_Tab, mozc::commands::KeyEvent::TAB},

  // TODO(mazda): Handle following keys?
  //   - FcitxKey_Kana_Lock? FcitxKey_KEY_Kana_Shift?
};

const struct ModifierKeyMap {
  uint32 from;
  mozc::commands::KeyEvent::ModifierKey to;
} modifier_key_map[] = {
  {FcitxKey_Shift_L, mozc::commands::KeyEvent::LEFT_SHIFT},
  {FcitxKey_Shift_R, mozc::commands::KeyEvent::RIGHT_SHIFT},
  {FcitxKey_Control_L, mozc::commands::KeyEvent::LEFT_CTRL},
  {FcitxKey_Control_R, mozc::commands::KeyEvent::RIGHT_CTRL},
  {FcitxKey_Alt_L, mozc::commands::KeyEvent::LEFT_ALT},
  {FcitxKey_Alt_R, mozc::commands::KeyEvent::RIGHT_ALT},
  {FcitxKeyState_CapsLock, mozc::commands::KeyEvent::CAPS},
};

const struct ModifierMaskMap {
  uint32 from;
  mozc::commands::KeyEvent::ModifierKey to;
} modifier_mask_map[] = {
  {FcitxKeyState_Shift, mozc::commands::KeyEvent::SHIFT},
  {FcitxKeyState_Ctrl, mozc::commands::KeyEvent::CTRL},
  {FcitxKeyState_Alt, mozc::commands::KeyEvent::ALT},
};

// TODO(team): Add kana_map_dv to support Dvoraklayout.
const struct KanaMap {
  uint32 code;
  const char *no_shift;
  const char *shift;
} kana_map_jp[] = {
  { '1' , "\xe3\x81\xac", "\xe3\x81\xac" },  // "ぬ", "ぬ"
  { '!' , "\xe3\x81\xac", "\xe3\x81\xac" },  // "ぬ", "ぬ"
  { '2' , "\xe3\x81\xb5", "\xe3\x81\xb5" },  // "ふ", "ふ"
  { '\"', "\xe3\x81\xb5", "\xe3\x81\xb5" },  // "ふ", "ふ"
  { '3' , "\xe3\x81\x82", "\xe3\x81\x81" },  // "あ", "ぁ"
  { '#' , "\xe3\x81\x82", "\xe3\x81\x81" },  // "あ", "ぁ"
  { '4' , "\xe3\x81\x86", "\xe3\x81\x85" },  // "う", "ぅ"
  { '$' , "\xe3\x81\x86", "\xe3\x81\x85" },  // "う", "ぅ"
  { '5' , "\xe3\x81\x88", "\xe3\x81\x87" },  // "え", "ぇ"
  { '%' , "\xe3\x81\x88", "\xe3\x81\x87" },  // "え", "ぇ"
  { '6' , "\xe3\x81\x8a", "\xe3\x81\x89" },  // "お", "ぉ"
  { '&' , "\xe3\x81\x8a", "\xe3\x81\x89" },  // "お", "ぉ"
  { '7' , "\xe3\x82\x84", "\xe3\x82\x83" },  // "や", "ゃ"
  { '\'', "\xe3\x82\x84", "\xe3\x82\x83" },  // "や", "ゃ"
  { '8' , "\xe3\x82\x86", "\xe3\x82\x85" },  // "ゆ", "ゅ"
  { '(' , "\xe3\x82\x86", "\xe3\x82\x85" },  // "ゆ", "ゅ"
  { '9' , "\xe3\x82\x88", "\xe3\x82\x87" },  // "よ", "ょ"
  { ')' , "\xe3\x82\x88", "\xe3\x82\x87" },  // "よ", "ょ"
  { '0' , "\xe3\x82\x8f", "\xe3\x82\x92" },  // "わ", "を"
  { '-' , "\xe3\x81\xbb", "\xe3\x81\xbb" },  // "ほ", "ほ"
  { '=' , "\xe3\x81\xbb", "\xe3\x81\xbb" },  // "ほ", "ほ"
  { '^' , "\xe3\x81\xb8", "\xe3\x82\x92" },  // "へ", "を"
  { '~' , "\xe3\x81\xb8", "\xe3\x82\x92" },  // "へ", "を"
  { '|' , "\xe3\x83\xbc", "\xe3\x83\xbc" },  // "ー", "ー"
  { 'q' , "\xe3\x81\x9f", "\xe3\x81\x9f" },  // "た", "た"
  { 'Q' , "\xe3\x81\x9f", "\xe3\x81\x9f" },  // "た", "た"
  { 'w' , "\xe3\x81\xa6", "\xe3\x81\xa6" },  // "て", "て"
  { 'W' , "\xe3\x81\xa6", "\xe3\x81\xa6" },  // "て", "て"
  { 'e' , "\xe3\x81\x84", "\xe3\x81\x83" },  // "い", "ぃ"
  { 'E' , "\xe3\x81\x84", "\xe3\x81\x83" },  // "い", "ぃ"
  { 'r' , "\xe3\x81\x99", "\xe3\x81\x99" },  // "す", "す"
  { 'R' , "\xe3\x81\x99", "\xe3\x81\x99" },  // "す", "す"
  { 't' , "\xe3\x81\x8b", "\xe3\x81\x8b" },  // "か", "か"
  { 'T' , "\xe3\x81\x8b", "\xe3\x81\x8b" },  // "か", "か"
  { 'y' , "\xe3\x82\x93", "\xe3\x82\x93" },  // "ん", "ん"
  { 'Y' , "\xe3\x82\x93", "\xe3\x82\x93" },  // "ん", "ん"
  { 'u' , "\xe3\x81\xaa", "\xe3\x81\xaa" },  // "な", "な"
  { 'U' , "\xe3\x81\xaa", "\xe3\x81\xaa" },  // "な", "な"
  { 'i' , "\xe3\x81\xab", "\xe3\x81\xab" },  // "に", "に"
  { 'I' , "\xe3\x81\xab", "\xe3\x81\xab" },  // "に", "に"
  { 'o' , "\xe3\x82\x89", "\xe3\x82\x89" },  // "ら", "ら"
  { 'O' , "\xe3\x82\x89", "\xe3\x82\x89" },  // "ら", "ら"
  { 'p' , "\xe3\x81\x9b", "\xe3\x81\x9b" },  // "せ", "せ"
  { 'P' , "\xe3\x81\x9b", "\xe3\x81\x9b" },  // "せ", "せ"
  { '@' , "\xe3\x82\x9b", "\xe3\x82\x9b" },  // "゛", "゛"
  { '`' , "\xe3\x82\x9b", "\xe3\x82\x9b" },  // "゛", "゛"
  { '[' , "\xe3\x82\x9c", "\xe3\x80\x8c" },  // "゜", "「"
  { '{' , "\xe3\x82\x9c", "\xe3\x80\x8c" },  // "゜", "「"
  { 'a' , "\xe3\x81\xa1", "\xe3\x81\xa1" },  // "ち", "ち"
  { 'A' , "\xe3\x81\xa1", "\xe3\x81\xa1" },  // "ち", "ち"
  { 's' , "\xe3\x81\xa8", "\xe3\x81\xa8" },  // "と", "と"
  { 'S' , "\xe3\x81\xa8", "\xe3\x81\xa8" },  // "と", "と"
  { 'd' , "\xe3\x81\x97", "\xe3\x81\x97" },  // "し", "し"
  { 'D' , "\xe3\x81\x97", "\xe3\x81\x97" },  // "し", "し"
  { 'f' , "\xe3\x81\xaf", "\xe3\x81\xaf" },  // "は", "は"
  { 'F' , "\xe3\x81\xaf", "\xe3\x81\xaf" },  // "は", "は"
  { 'g' , "\xe3\x81\x8d", "\xe3\x81\x8d" },  // "き", "き"
  { 'G' , "\xe3\x81\x8d", "\xe3\x81\x8d" },  // "き", "き"
  { 'h' , "\xe3\x81\x8f", "\xe3\x81\x8f" },  // "く", "く"
  { 'H' , "\xe3\x81\x8f", "\xe3\x81\x8f" },  // "く", "く"
  { 'j' , "\xe3\x81\xbe", "\xe3\x81\xbe" },  // "ま", "ま"
  { 'J' , "\xe3\x81\xbe", "\xe3\x81\xbe" },  // "ま", "ま"
  { 'k' , "\xe3\x81\xae", "\xe3\x81\xae" },  // "の", "の"
  { 'K' , "\xe3\x81\xae", "\xe3\x81\xae" },  // "の", "の"
  { 'l' , "\xe3\x82\x8a", "\xe3\x82\x8a" },  // "り", "り"
  { 'L' , "\xe3\x82\x8a", "\xe3\x82\x8a" },  // "り", "り"
  { ';' , "\xe3\x82\x8c", "\xe3\x82\x8c" },  // "れ", "れ"
  { '+' , "\xe3\x82\x8c", "\xe3\x82\x8c" },  // "れ", "れ"
  { ':' , "\xe3\x81\x91", "\xe3\x81\x91" },  // "け", "け"
  { '*' , "\xe3\x81\x91", "\xe3\x81\x91" },  // "け", "け"
  { ']' , "\xe3\x82\x80", "\xe3\x80\x8d" },  // "む", "」"
  { '}' , "\xe3\x82\x80", "\xe3\x80\x8d" },  // "む", "」"
  { 'z' , "\xe3\x81\xa4", "\xe3\x81\xa3" },  // "つ", "っ"
  { 'Z' , "\xe3\x81\xa4", "\xe3\x81\xa3" },  // "つ", "っ"
  { 'x' , "\xe3\x81\x95", "\xe3\x81\x95" },  // "さ", "さ"
  { 'X' , "\xe3\x81\x95", "\xe3\x81\x95" },  // "さ", "さ"
  { 'c' , "\xe3\x81\x9d", "\xe3\x81\x9d" },  // "そ", "そ"
  { 'C' , "\xe3\x81\x9d", "\xe3\x81\x9d" },  // "そ", "そ"
  { 'v' , "\xe3\x81\xb2", "\xe3\x81\xb2" },  // "ひ", "ひ"
  { 'V' , "\xe3\x81\xb2", "\xe3\x81\xb2" },  // "ひ", "ひ"
  { 'b' , "\xe3\x81\x93", "\xe3\x81\x93" },  // "こ", "こ"
  { 'B' , "\xe3\x81\x93", "\xe3\x81\x93" },  // "こ", "こ"
  { 'n' , "\xe3\x81\xbf", "\xe3\x81\xbf" },  // "み", "み"
  { 'N' , "\xe3\x81\xbf", "\xe3\x81\xbf" },  // "み", "み"
  { 'm' , "\xe3\x82\x82", "\xe3\x82\x82" },  // "も", "も"
  { 'M' , "\xe3\x82\x82", "\xe3\x82\x82" },  // "も", "も"
  { ',' , "\xe3\x81\xad", "\xe3\x80\x81" },  // "ね", "、"
  { '<' , "\xe3\x81\xad", "\xe3\x80\x81" },  // "ね", "、"
  { '.' , "\xe3\x82\x8b", "\xe3\x80\x82" },  // "る", "。"
  { '>' , "\xe3\x82\x8b", "\xe3\x80\x82" },  // "る", "。"
  { '/' , "\xe3\x82\x81", "\xe3\x83\xbb" },  // "め", "・"
  { '?' , "\xe3\x82\x81", "\xe3\x83\xbb" },  // "め", "・"
  { '_' , "\xe3\x82\x8d", "\xe3\x82\x8d" },  // "ろ", "ろ"
  // A backslash is handled in a special way because it is input by
  // two different keys (the one next to Backslash and the one next
  // to Right Shift).
  { '\\', "", "" },
}, kana_map_us[] = {
  { '`' , "\xe3\x82\x8d", "\xe3\x82\x8d" },  // "ろ", "ろ"
  { '~' , "\xe3\x82\x8d", "\xe3\x82\x8d" },  // "ろ", "ろ"
  { '1' , "\xe3\x81\xac", "\xe3\x81\xac" },  // "ぬ", "ぬ"
  { '!' , "\xe3\x81\xac", "\xe3\x81\xac" },  // "ぬ", "ぬ"
  { '2' , "\xe3\x81\xb5", "\xe3\x81\xb5" },  // "ふ", "ふ"
  { '@' , "\xe3\x81\xb5", "\xe3\x81\xb5" },  // "ふ", "ふ"
  { '3' , "\xe3\x81\x82", "\xe3\x81\x81" },  // "あ", "ぁ"
  { '#' , "\xe3\x81\x82", "\xe3\x81\x81" },  // "あ", "ぁ"
  { '4' , "\xe3\x81\x86", "\xe3\x81\x85" },  // "う", "ぅ"
  { '$' , "\xe3\x81\x86", "\xe3\x81\x85" },  // "う", "ぅ"
  { '5' , "\xe3\x81\x88", "\xe3\x81\x87" },  // "え", "ぇ"
  { '%' , "\xe3\x81\x88", "\xe3\x81\x87" },  // "え", "ぇ"
  { '6' , "\xe3\x81\x8a", "\xe3\x81\x89" },  // "お", "ぉ"
  { '^' , "\xe3\x81\x8a", "\xe3\x81\x89" },  // "お", "ぉ"
  { '7' , "\xe3\x82\x84", "\xe3\x82\x83" },  // "や", "ゃ"
  { '&' , "\xe3\x82\x84", "\xe3\x82\x83" },  // "や", "ゃ"
  { '8' , "\xe3\x82\x86", "\xe3\x82\x85" },  // "ゆ", "ゅ"
  { '*' , "\xe3\x82\x86", "\xe3\x82\x85" },  // "ゆ", "ゅ"
  { '9' , "\xe3\x82\x88", "\xe3\x82\x87" },  // "よ", "ょ"
  { '(' , "\xe3\x82\x88", "\xe3\x82\x87" },  // "よ", "ょ"
  { '0' , "\xe3\x82\x8f", "\xe3\x82\x92" },  // "わ", "を"
  { ')' , "\xe3\x82\x8f", "\xe3\x82\x92" },  // "わ", "を"
  { '-' , "\xe3\x81\xbb", "\xe3\x83\xbc" },  // "ほ", "ー"
  { '_' , "\xe3\x81\xbb", "\xe3\x83\xbc" },  // "ほ", "ー"
  { '=' , "\xe3\x81\xb8", "\xe3\x81\xb8" },  // "へ", "へ"
  { '+' , "\xe3\x81\xb8", "\xe3\x81\xb8" },  // "へ", "へ"
  { 'q' , "\xe3\x81\x9f", "\xe3\x81\x9f" },  // "た", "た"
  { 'Q' , "\xe3\x81\x9f", "\xe3\x81\x9f" },  // "た", "た"
  { 'w' , "\xe3\x81\xa6", "\xe3\x81\xa6" },  // "て", "て"
  { 'W' , "\xe3\x81\xa6", "\xe3\x81\xa6" },  // "て", "て"
  { 'e' , "\xe3\x81\x84", "\xe3\x81\x83" },  // "い", "ぃ"
  { 'E' , "\xe3\x81\x84", "\xe3\x81\x83" },  // "い", "ぃ"
  { 'r' , "\xe3\x81\x99", "\xe3\x81\x99" },  // "す", "す"
  { 'R' , "\xe3\x81\x99", "\xe3\x81\x99" },  // "す", "す"
  { 't' , "\xe3\x81\x8b", "\xe3\x81\x8b" },  // "か", "か"
  { 'T' , "\xe3\x81\x8b", "\xe3\x81\x8b" },  // "か", "か"
  { 'y' , "\xe3\x82\x93", "\xe3\x82\x93" },  // "ん", "ん"
  { 'Y' , "\xe3\x82\x93", "\xe3\x82\x93" },  // "ん", "ん"
  { 'u' , "\xe3\x81\xaa", "\xe3\x81\xaa" },  // "な", "な"
  { 'U' , "\xe3\x81\xaa", "\xe3\x81\xaa" },  // "な", "な"
  { 'i' , "\xe3\x81\xab", "\xe3\x81\xab" },  // "に", "に"
  { 'I' , "\xe3\x81\xab", "\xe3\x81\xab" },  // "に", "に"
  { 'o' , "\xe3\x82\x89", "\xe3\x82\x89" },  // "ら", "ら"
  { 'O' , "\xe3\x82\x89", "\xe3\x82\x89" },  // "ら", "ら"
  { 'p' , "\xe3\x81\x9b", "\xe3\x81\x9b" },  // "せ", "せ"
  { 'P' , "\xe3\x81\x9b", "\xe3\x81\x9b" },  // "せ", "せ"
  { '[' , "\xe3\x82\x9b", "\xe3\x82\x9b" },  // "゛", "゛"
  { '{' , "\xe3\x82\x9b", "\xe3\x82\x9b" },  // "゛", "゛"
  { ']' , "\xe3\x82\x9c", "\xe3\x80\x8c" },  // "゜", "「"
  { '}' , "\xe3\x82\x9c", "\xe3\x80\x8c" },  // "゜", "「"
  { '\\', "\xe3\x82\x80", "\xe3\x80\x8d" },  // "む", "」"
  { '|' , "\xe3\x82\x80", "\xe3\x80\x8d" },  // "む", "」"
  { 'a' , "\xe3\x81\xa1", "\xe3\x81\xa1" },  // "ち", "ち"
  { 'A' , "\xe3\x81\xa1", "\xe3\x81\xa1" },  // "ち", "ち"
  { 's' , "\xe3\x81\xa8", "\xe3\x81\xa8" },  // "と", "と"
  { 'S' , "\xe3\x81\xa8", "\xe3\x81\xa8" },  // "と", "と"
  { 'd' , "\xe3\x81\x97", "\xe3\x81\x97" },  // "し", "し"
  { 'D' , "\xe3\x81\x97", "\xe3\x81\x97" },  // "し", "し"
  { 'f' , "\xe3\x81\xaf", "\xe3\x81\xaf" },  // "は", "は"
  { 'F' , "\xe3\x81\xaf", "\xe3\x81\xaf" },  // "は", "は"
  { 'g' , "\xe3\x81\x8d", "\xe3\x81\x8d" },  // "き", "き"
  { 'G' , "\xe3\x81\x8d", "\xe3\x81\x8d" },  // "き", "き"
  { 'h' , "\xe3\x81\x8f", "\xe3\x81\x8f" },  // "く", "く"
  { 'H' , "\xe3\x81\x8f", "\xe3\x81\x8f" },  // "く", "く"
  { 'j' , "\xe3\x81\xbe", "\xe3\x81\xbe" },  // "ま", "ま"
  { 'J' , "\xe3\x81\xbe", "\xe3\x81\xbe" },  // "ま", "ま"
  { 'k' , "\xe3\x81\xae", "\xe3\x81\xae" },  // "の", "の"
  { 'K' , "\xe3\x81\xae", "\xe3\x81\xae" },  // "の", "の"
  { 'l' , "\xe3\x82\x8a", "\xe3\x82\x8a" },  // "り", "り"
  { 'L' , "\xe3\x82\x8a", "\xe3\x82\x8a" },  // "り", "り"
  { ';' , "\xe3\x82\x8c", "\xe3\x82\x8c" },  // "れ", "れ"
  { ':' , "\xe3\x82\x8c", "\xe3\x82\x8c" },  // "れ", "れ"
  { '\'', "\xe3\x81\x91", "\xe3\x81\x91" },  // "け", "け"
  { '\"', "\xe3\x81\x91", "\xe3\x81\x91" },  // "け", "け"
  { 'z' , "\xe3\x81\xa4", "\xe3\x81\xa3" },  // "つ", "っ"
  { 'Z' , "\xe3\x81\xa4", "\xe3\x81\xa3" },  // "つ", "っ"
  { 'x' , "\xe3\x81\x95", "\xe3\x81\x95" },  // "さ", "さ"
  { 'X' , "\xe3\x81\x95", "\xe3\x81\x95" },  // "さ", "さ"
  { 'c' , "\xe3\x81\x9d", "\xe3\x81\x9d" },  // "そ", "そ"
  { 'C' , "\xe3\x81\x9d", "\xe3\x81\x9d" },  // "そ", "そ"
  { 'v' , "\xe3\x81\xb2", "\xe3\x81\xb2" },  // "ひ", "ひ"
  { 'V' , "\xe3\x81\xb2", "\xe3\x81\xb2" },  // "ひ", "ひ"
  { 'b' , "\xe3\x81\x93", "\xe3\x81\x93" },  // "こ", "こ"
  { 'B' , "\xe3\x81\x93", "\xe3\x81\x93" },  // "こ", "こ"
  { 'n' , "\xe3\x81\xbf", "\xe3\x81\xbf" },  // "み", "み"
  { 'N' , "\xe3\x81\xbf", "\xe3\x81\xbf" },  // "み", "み"
  { 'm' , "\xe3\x82\x82", "\xe3\x82\x82" },  // "も", "も"
  { 'M' , "\xe3\x82\x82", "\xe3\x82\x82" },  // "も", "も"
  { ',' , "\xe3\x81\xad", "\xe3\x80\x81" },  // "ね", "、"
  { '<' , "\xe3\x81\xad", "\xe3\x80\x81" },  // "ね", "、"
  { '.' , "\xe3\x82\x8b", "\xe3\x80\x82" },  // "る", "。"
  { '>' , "\xe3\x82\x8b", "\xe3\x80\x82" },  // "る", "。"
  { '/' , "\xe3\x82\x81", "\xe3\x83\xbb" },  // "め", "・"
  { '?' , "\xe3\x82\x81", "\xe3\x83\xbb" },  // "め", "・"
};

}  // namespace

namespace mozc {
namespace fcitx {

KeyTranslator::KeyTranslator() {
  Init();
}

KeyTranslator::~KeyTranslator() {
}

// TODO(nona): Fix 'Shift-0' behavior b/4338394
bool KeyTranslator::Translate(FcitxKeySym keyval,
                              uint32 keycode,
                              uint32 modifiers,
                              config::Config::PreeditMethod method,
                              bool layout_is_jp,
                              commands::KeyEvent *out_event) const {
  DCHECK(out_event) << "out_event is NULL";
  out_event->Clear();

  /* this is key we cannot handle, don't process it */
  if (modifiers & FcitxKeyState_Super)
    return false;

  // Due to historical reasons, many linux ditributions set Hiragana_Katakana
  // key as Hiragana key (which is Katkana key with shift modifier). So, we
  // translate Hiragana_Katanaka key as Hiragana key by mapping table, and
  // Shift + Hiragana_Katakana key as Katakana key by functionally.
  // TODO(nona): Fix process modifier to handle right shift
  if (IsHiraganaKatakanaKeyWithShift(keyval, keycode, modifiers)) {
    modifiers &= ~FcitxKeyState_Shift;
    keyval = FcitxKey_Katakana;
  }
  std::string kana_key_string;
  if ((method == config::Config::KANA) && IsKanaAvailable(
          keyval, keycode, modifiers, layout_is_jp, &kana_key_string)) {
    out_event->set_key_code(keyval);
    out_event->set_key_string(kana_key_string);
  } else if (IsAscii(keyval, keycode, modifiers)) {
    if (FcitxKeyState_CapsLock & modifiers) {
      out_event->add_modifier_keys(commands::KeyEvent::CAPS);
    }
    out_event->set_key_code(keyval);
  } else if (IsModifierKey(keyval, keycode, modifiers)) {
    ModifierKeyMap::const_iterator i = modifier_key_map_.find(keyval);
    DCHECK(i != modifier_key_map_.end());
    out_event->add_modifier_keys(i->second);
  } else if (IsSpecialKey(keyval, keycode, modifiers)) {
    SpecialKeyMap::const_iterator i = special_key_map_.find(keyval);
    DCHECK(i != special_key_map_.end());
    out_event->set_special_key(i->second);
  } else {
    VLOG(1) << "Unknown keyval: " << keyval;
    return false;
  }

  for (ModifierKeyMap::const_iterator i = modifier_mask_map_.begin();
       i != modifier_mask_map_.end(); ++i) {
    // Do not set a SHIFT modifier when |keyval| is a printable key by following
    // the Mozc's rule.
    if ((i->second == commands::KeyEvent::SHIFT) &&
        IsPrintable(keyval, keycode, modifiers)) {
      continue;
    }

    if (i->first & modifiers) {
      out_event->add_modifier_keys(i->second);
    }
  }

  return true;
}

void KeyTranslator::Init() {
  for (int i = 0; i < FCITX_ARRAY_SIZE(special_key_map); ++i) {
    CHECK(special_key_map_.insert(std::make_pair(special_key_map[i].from,
                                            special_key_map[i].to)).second);
  }
  for (int i = 0; i < FCITX_ARRAY_SIZE(modifier_key_map); ++i) {
    CHECK(modifier_key_map_.insert(std::make_pair(modifier_key_map[i].from,
                                             modifier_key_map[i].to)).second);
  }
  for (int i = 0; i < FCITX_ARRAY_SIZE(modifier_mask_map); ++i) {
    CHECK(modifier_mask_map_.insert(std::make_pair(modifier_mask_map[i].from,
                                              modifier_mask_map[i].to)).second);
  }
  for (int i = 0; i < FCITX_ARRAY_SIZE(kana_map_jp); ++i) {
    CHECK(kana_map_jp_.insert(
        std::make_pair(kana_map_jp[i].code, std::make_pair(
            kana_map_jp[i].no_shift, kana_map_jp[i].shift))).second);
  }
  for (int i = 0; i < FCITX_ARRAY_SIZE(kana_map_us); ++i) {
    CHECK(kana_map_us_.insert(
        std::make_pair(kana_map_us[i].code, std::make_pair(
            kana_map_us[i].no_shift, kana_map_us[i].shift))).second);
  }
}

bool KeyTranslator::IsModifierKey(uint32 keyval,
                                  uint32 keycode,
                                  uint32 modifiers) const {
  return modifier_key_map_.find(keyval) != modifier_key_map_.end();
}

bool KeyTranslator::IsSpecialKey(uint32 keyval,
                                 uint32 keycode,
                                 uint32 modifiers) const {
  return special_key_map_.find(keyval) != special_key_map_.end();
}

bool KeyTranslator::IsHiraganaKatakanaKeyWithShift(uint32 keyval,
                                                   uint32 keycode,
                                                   uint32 modifiers) {
  return ((modifiers & FcitxKeyState_Shift) && (keyval == FcitxKey_Hiragana_Katakana));
}

bool KeyTranslator::IsKanaAvailable(uint32 keyval,
                                    uint32 keycode,
                                    uint32 modifiers,
                                    bool layout_is_jp,
                                    std::string *out) const {
  if ((modifiers & FcitxKeyState_Ctrl) || (modifiers & FcitxKeyState_Alt)) {
    return false;
  }
  const KanaMap &kana_map = layout_is_jp ? kana_map_jp_ : kana_map_us_;
  KanaMap::const_iterator iter = kana_map.find(keyval);
  if (iter == kana_map.end()) {
    return false;
  }

  if (out) {
    // When a Japanese keyboard is in use, the yen-sign key and the backslash
    // key generate the same |keyval|. In this case, we have to check |keycode|
    // to return an appropriate string. See the following IBus issue for
    // details: http://code.google.com/p/ibus/issues/detail?id=52
    if (keyval == '\\' && layout_is_jp) {
      if (keycode == 132 || keycode == 133) {
        *out = "\xe3\x83\xbc";  // "ー"
      } else {
        *out = "\xe3\x82\x8d";  // "ろ"
      }
    } else {
      *out = (modifiers & FcitxKeyState_Shift) ?
          iter->second.second : iter->second.first;
    }
  }
  return true;
}

// TODO(nona): resolve S-'0' problem (b/4338394).
// TODO(nona): Current printable detection is weak. To enhance accuracy, use xkb
// key map
bool KeyTranslator::IsPrintable(uint32 keyval, uint32 keycode, uint32 modifiers) {
  if ((modifiers & FcitxKeyState_Ctrl) || (modifiers & FcitxKeyState_Alt)) {
    return false;
  }
  return IsAscii(keyval, keycode, modifiers);
}

bool KeyTranslator::IsAscii(uint32 keyval, uint32 keycode, uint32 modifiers) {
  return (keyval > FcitxKey_space &&
          // Note: Space key (0x20) is a special key in Mozc.
          keyval <= FcitxKey_asciitilde);  // 0x7e.
}

}  // namespace ibus
}  // namespace mozc
