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

#include "unix/ibus/key_translator.h"

#include <ibus.h>

#include "base/logging.h"

namespace {

const struct SpecialKeyMap {
  guint from;
  mozc::commands::KeyEvent::SpecialKey to;
} special_key_map[] = {
  {IBUS_space, mozc::commands::KeyEvent::SPACE},
  {IBUS_Return, mozc::commands::KeyEvent::ENTER},
  {IBUS_Left, mozc::commands::KeyEvent::LEFT},
  {IBUS_Right, mozc::commands::KeyEvent::RIGHT},
  {IBUS_Up, mozc::commands::KeyEvent::UP},
  {IBUS_Down, mozc::commands::KeyEvent::DOWN},
  {IBUS_Escape, mozc::commands::KeyEvent::ESCAPE},
  {IBUS_Delete, mozc::commands::KeyEvent::DEL},
  {IBUS_BackSpace, mozc::commands::KeyEvent::BACKSPACE},
  {IBUS_Insert, mozc::commands::KeyEvent::INSERT},
  {IBUS_Henkan, mozc::commands::KeyEvent::HENKAN},
  {IBUS_Muhenkan, mozc::commands::KeyEvent::MUHENKAN},
  {IBUS_Hiragana, mozc::commands::KeyEvent::KANA},
  {IBUS_Katakana, mozc::commands::KeyEvent::KANA},
  {IBUS_Eisu_toggle, mozc::commands::KeyEvent::EISU},
  {IBUS_Home, mozc::commands::KeyEvent::HOME},
  {IBUS_End, mozc::commands::KeyEvent::END},
  {IBUS_Tab, mozc::commands::KeyEvent::TAB},
  {IBUS_F1, mozc::commands::KeyEvent::F1},
  {IBUS_F2, mozc::commands::KeyEvent::F2},
  {IBUS_F3, mozc::commands::KeyEvent::F3},
  {IBUS_F4, mozc::commands::KeyEvent::F4},
  {IBUS_F5, mozc::commands::KeyEvent::F5},
  {IBUS_F6, mozc::commands::KeyEvent::F6},
  {IBUS_F7, mozc::commands::KeyEvent::F7},
  {IBUS_F8, mozc::commands::KeyEvent::F8},
  {IBUS_F9, mozc::commands::KeyEvent::F9},
  {IBUS_F10, mozc::commands::KeyEvent::F10},
  {IBUS_F11, mozc::commands::KeyEvent::F11},
  {IBUS_F12, mozc::commands::KeyEvent::F12},
  {IBUS_F13, mozc::commands::KeyEvent::F13},
  {IBUS_F14, mozc::commands::KeyEvent::F14},
  {IBUS_F15, mozc::commands::KeyEvent::F15},
  {IBUS_F16, mozc::commands::KeyEvent::F16},
  {IBUS_F17, mozc::commands::KeyEvent::F17},
  {IBUS_F18, mozc::commands::KeyEvent::F18},
  {IBUS_F19, mozc::commands::KeyEvent::F19},
  {IBUS_F20, mozc::commands::KeyEvent::F20},
  {IBUS_F21, mozc::commands::KeyEvent::F21},
  {IBUS_F22, mozc::commands::KeyEvent::F22},
  {IBUS_F23, mozc::commands::KeyEvent::F23},
  {IBUS_F24, mozc::commands::KeyEvent::F24},
  {IBUS_Page_Up, mozc::commands::KeyEvent::PAGE_UP},
  {IBUS_Page_Down, mozc::commands::KeyEvent::PAGE_DOWN},

  // Keypad (10-key).
  {IBUS_KP_0, mozc::commands::KeyEvent::NUMPAD0},
  {IBUS_KP_1, mozc::commands::KeyEvent::NUMPAD1},
  {IBUS_KP_2, mozc::commands::KeyEvent::NUMPAD2},
  {IBUS_KP_3, mozc::commands::KeyEvent::NUMPAD3},
  {IBUS_KP_4, mozc::commands::KeyEvent::NUMPAD4},
  {IBUS_KP_5, mozc::commands::KeyEvent::NUMPAD5},
  {IBUS_KP_6, mozc::commands::KeyEvent::NUMPAD6},
  {IBUS_KP_7, mozc::commands::KeyEvent::NUMPAD7},
  {IBUS_KP_8, mozc::commands::KeyEvent::NUMPAD8},
  {IBUS_KP_9, mozc::commands::KeyEvent::NUMPAD9},
  {IBUS_KP_Equal, mozc::commands::KeyEvent::EQUALS},  // [=]
  {IBUS_KP_Multiply, mozc::commands::KeyEvent::MULTIPLY},  // [*]
  {IBUS_KP_Add, mozc::commands::KeyEvent::ADD},  // [+]
  {IBUS_KP_Separator, mozc::commands::KeyEvent::SEPARATOR},  // enter
  {IBUS_KP_Subtract, mozc::commands::KeyEvent::SUBTRACT},  // [-]
  {IBUS_KP_Decimal, mozc::commands::KeyEvent::DECIMAL},  // [.]
  {IBUS_KP_Divide, mozc::commands::KeyEvent::DIVIDE},  // [/]
  {IBUS_KP_Space, mozc::commands::KeyEvent::SPACE},
  {IBUS_KP_Tab, mozc::commands::KeyEvent::TAB},
  {IBUS_KP_Enter, mozc::commands::KeyEvent::ENTER},
  {IBUS_KP_Home, mozc::commands::KeyEvent::HOME},
  {IBUS_KP_Left, mozc::commands::KeyEvent::LEFT},
  {IBUS_KP_Up, mozc::commands::KeyEvent::UP},
  {IBUS_KP_Right, mozc::commands::KeyEvent::RIGHT},
  {IBUS_KP_Down, mozc::commands::KeyEvent::DOWN},
  {IBUS_KP_Page_Up, mozc::commands::KeyEvent::PAGE_UP},
  {IBUS_KP_Page_Down, mozc::commands::KeyEvent::PAGE_DOWN},
  {IBUS_KP_End, mozc::commands::KeyEvent::END},
  {IBUS_KP_Delete, mozc::commands::KeyEvent::DEL},
  {IBUS_KP_Insert, mozc::commands::KeyEvent::INSERT},
  {IBUS_Caps_Lock, mozc::commands::KeyEvent::CAPS_LOCK},

  // Shift+TAB.
  {IBUS_ISO_Left_Tab, mozc::commands::KeyEvent::TAB},

  // TODO(mazda): Handle following keys?
  //   - IBUS_Kana_Lock? IBUS_KEY_Kana_Shift?
};

const struct ModifierKeyMap {
  guint from;
  mozc::commands::KeyEvent::ModifierKey to;
} modifier_key_map[] = {
  {IBUS_Shift_L, mozc::commands::KeyEvent::SHIFT},
  {IBUS_Shift_R, mozc::commands::KeyEvent::SHIFT},
  {IBUS_Control_L, mozc::commands::KeyEvent::CTRL},
  {IBUS_Control_R, mozc::commands::KeyEvent::CTRL},
  {IBUS_Alt_L, mozc::commands::KeyEvent::ALT},
  {IBUS_Alt_R, mozc::commands::KeyEvent::ALT},
};

const struct ModifierMaskMap {
  guint from;
  mozc::commands::KeyEvent::ModifierKey to;
} modifier_mask_map[] = {
  {IBUS_SHIFT_MASK, mozc::commands::KeyEvent::SHIFT},
  {IBUS_CONTROL_MASK, mozc::commands::KeyEvent::CTRL},
  {IBUS_MOD1_MASK, mozc::commands::KeyEvent::ALT},
};

// TODO:Add kana_map_dv to support Dvoraklayout.
const struct KanaMap {
  guint code;
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
namespace ibus {

KeyTranslator::KeyTranslator() {
  Init();
}

KeyTranslator::~KeyTranslator() {
}

// TODO(nona): Fix 'Shift-0' behavior b/4338394
bool KeyTranslator::Translate(guint keyval,
                              guint keycode,
                              guint modifiers,
                              config::Config::PreeditMethod method,
                              bool layout_is_jp,
                              commands::KeyEvent *out_event) const {
  DCHECK(out_event) << "out_event is NULL";
  out_event->Clear();

  string kana_key_string;
  if ((method == config::Config::KANA) && IsKanaAvailable(
          keyval, keycode, modifiers, layout_is_jp, &kana_key_string)) {
    out_event->set_key_code(keyval);
    out_event->set_key_string(kana_key_string);
  } else if (IsAscii(keyval, keycode, modifiers)) {
    out_event->set_key_code(keyval);
  } else if (IsModifierKey(keyval, keycode, modifiers)) {
    ModifierKeyMap::const_iterator i = modifier_key_map_.find(keyval);
    DCHECK(i != modifier_key_map_.end());
    out_event->add_modifier_keys((*i).second);
  } else if (IsSpecialKey(keyval, keycode, modifiers)) {
    SpecialKeyMap::const_iterator i = special_key_map_.find(keyval);
    DCHECK(i != special_key_map_.end());
    out_event->set_special_key((*i).second);
  } else {
    VLOG(1) << "Unknown keyval: " << keyval;
    return false;
  }

  for (ModifierKeyMap::const_iterator i = modifier_mask_map_.begin();
       i != modifier_mask_map_.end();
       ++i) {
    // Do not set a SHIFT modifier when |keyval| is a printable key by following
    // the Mozc's rule.
    if (((*i).second == commands::KeyEvent::SHIFT) &&
        IsPrintable(keyval, keycode, modifiers)) {
        continue;
     }

    if ((*i).first & modifiers) {
      out_event->add_modifier_keys((*i).second);
    }
  }

  return true;
}

void KeyTranslator::Init() {
  for (int i = 0; i < arraysize(special_key_map); ++i) {
    CHECK(special_key_map_.insert(make_pair(special_key_map[i].from,
                                            special_key_map[i].to)).second);
  }
  for (int i = 0; i < arraysize(modifier_key_map); ++i) {
    CHECK(modifier_key_map_.insert(make_pair(modifier_key_map[i].from,
                                             modifier_key_map[i].to)).second);
  }
  for (int i = 0; i < arraysize(modifier_mask_map); ++i) {
    CHECK(modifier_mask_map_.insert(make_pair(modifier_mask_map[i].from,
                                              modifier_mask_map[i].to)).second);
  }
  for (int i = 0; i < arraysize(kana_map_jp); ++i) {
    CHECK(kana_map_jp_.insert(
        make_pair(kana_map_jp[i].code, make_pair(
            kana_map_jp[i].no_shift, kana_map_jp[i].shift))).second);
  }
  for (int i = 0; i < arraysize(kana_map_us); ++i) {
    CHECK(kana_map_us_.insert(
        make_pair(kana_map_us[i].code, make_pair(
            kana_map_us[i].no_shift, kana_map_us[i].shift))).second);
  }
}

bool KeyTranslator::IsModifierKey(guint keyval,
                                  guint keycode,
                                  guint modifiers) const {
  return modifier_key_map_.find(keyval) != modifier_key_map_.end();
}

bool KeyTranslator::IsSpecialKey(guint keyval,
                                 guint keycode,
                                 guint modifiers) const {
  return special_key_map_.find(keyval) != special_key_map_.end();
}

bool KeyTranslator::IsKanaAvailable(guint keyval,
                                    guint keycode,
                                    guint modifiers,
                                    bool layout_is_jp,
                                    string *out) const {
  if ((modifiers & IBUS_CONTROL_MASK) || (modifiers & IBUS_MOD1_MASK)) {
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
      if (keycode == IBUS_bar) {
        *out = "\xe3\x83\xbc";  // "ー"
      } else {
        *out = "\xe3\x82\x8d";  // "ろ"
      }
    } else {
      *out = (modifiers & IBUS_SHIFT_MASK) ?
          iter->second.second : iter->second.first;
    }
  }
  return true;
}

// TODO(nona): resolve S-'0' problem (b/4338394).
// TODO(nona): Current printable detection is weak. To enhance accuracy, use xkb
// key map
bool KeyTranslator::IsPrintable(guint keyval, guint keycode, guint modifiers) {
  if ((modifiers & IBUS_CONTROL_MASK) || (modifiers & IBUS_MOD1_MASK)) {
    return false;
  }
  return IsAscii(keyval, keycode, modifiers);
}

bool KeyTranslator::IsAscii(guint keyval, guint keycode, guint modifiers) {
  return (keyval > IBUS_space &&
          // Note: Space key (0x20) is a special key in Mozc.
          keyval <= IBUS_asciitilde);  // 0x7e.
}

}  // namespace ibus
}  // namespace mozc
