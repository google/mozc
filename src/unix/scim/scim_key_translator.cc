// Copyright 2010, Google Inc.
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

#include "unix/scim/scim_key_translator.h"

#include "base/logging.h"

namespace {

const uint32 modifier_keys[] = {
  scim::SCIM_KEY_Alt_L,
  scim::SCIM_KEY_Alt_R,
  scim::SCIM_KEY_Caps_Lock,
  scim::SCIM_KEY_Control_L,
  scim::SCIM_KEY_Control_R,
  scim::SCIM_KEY_Hyper_L,
  scim::SCIM_KEY_Hyper_R,
  scim::SCIM_KEY_Meta_L,
  scim::SCIM_KEY_Meta_R,
  scim::SCIM_KEY_Shift_L,
  scim::SCIM_KEY_Shift_Lock,
  scim::SCIM_KEY_Shift_R,
  scim::SCIM_KEY_Super_L,
  scim::SCIM_KEY_Super_R,
};

const struct SpecialKeyMap {
  uint32 from;
  mozc::commands::KeyEvent::SpecialKey to;
} sp_key_map[] = {
  {scim::SCIM_KEY_space, mozc::commands::KeyEvent::SPACE},
  {scim::SCIM_KEY_Return, mozc::commands::KeyEvent::ENTER},
  {scim::SCIM_KEY_Left, mozc::commands::KeyEvent::LEFT},
  {scim::SCIM_KEY_Right, mozc::commands::KeyEvent::RIGHT},
  {scim::SCIM_KEY_Up, mozc::commands::KeyEvent::UP},
  {scim::SCIM_KEY_Down, mozc::commands::KeyEvent::DOWN},
  {scim::SCIM_KEY_Escape, mozc::commands::KeyEvent::ESCAPE},
  {scim::SCIM_KEY_Delete, mozc::commands::KeyEvent::DEL},
  {scim::SCIM_KEY_BackSpace, mozc::commands::KeyEvent::BACKSPACE},
  {scim::SCIM_KEY_Insert, mozc::commands::KeyEvent::INSERT},
  {scim::SCIM_KEY_Henkan, mozc::commands::KeyEvent::HENKAN},
  {scim::SCIM_KEY_Muhenkan, mozc::commands::KeyEvent::MUHENKAN},
  // NOTE(komatsu): There is scim::SCIM_KEY_Hiragana_Katakana too.
  {scim::SCIM_KEY_Hiragana, mozc::commands::KeyEvent::KANA},
  {scim::SCIM_KEY_Eisu_toggle, mozc::commands::KeyEvent::EISU},
  {scim::SCIM_KEY_Home, mozc::commands::KeyEvent::HOME},
  {scim::SCIM_KEY_End, mozc::commands::KeyEvent::END},
  {scim::SCIM_KEY_Tab, mozc::commands::KeyEvent::TAB},
  {scim::SCIM_KEY_F1, mozc::commands::KeyEvent::F1},
  {scim::SCIM_KEY_F2, mozc::commands::KeyEvent::F2},
  {scim::SCIM_KEY_F3, mozc::commands::KeyEvent::F3},
  {scim::SCIM_KEY_F4, mozc::commands::KeyEvent::F4},
  {scim::SCIM_KEY_F5, mozc::commands::KeyEvent::F5},
  {scim::SCIM_KEY_F6, mozc::commands::KeyEvent::F6},
  {scim::SCIM_KEY_F7, mozc::commands::KeyEvent::F7},
  {scim::SCIM_KEY_F8, mozc::commands::KeyEvent::F8},
  {scim::SCIM_KEY_F9, mozc::commands::KeyEvent::F9},
  {scim::SCIM_KEY_F10, mozc::commands::KeyEvent::F10},
  {scim::SCIM_KEY_F11, mozc::commands::KeyEvent::F11},
  {scim::SCIM_KEY_F12, mozc::commands::KeyEvent::F12},
  {scim::SCIM_KEY_F13, mozc::commands::KeyEvent::F13},
  {scim::SCIM_KEY_F14, mozc::commands::KeyEvent::F14},
  {scim::SCIM_KEY_F15, mozc::commands::KeyEvent::F15},
  {scim::SCIM_KEY_F16, mozc::commands::KeyEvent::F16},
  {scim::SCIM_KEY_F17, mozc::commands::KeyEvent::F17},
  {scim::SCIM_KEY_F18, mozc::commands::KeyEvent::F18},
  {scim::SCIM_KEY_F19, mozc::commands::KeyEvent::F19},
  {scim::SCIM_KEY_F20, mozc::commands::KeyEvent::F20},
  {scim::SCIM_KEY_F21, mozc::commands::KeyEvent::F21},
  {scim::SCIM_KEY_F22, mozc::commands::KeyEvent::F22},
  {scim::SCIM_KEY_F23, mozc::commands::KeyEvent::F23},
  {scim::SCIM_KEY_F24, mozc::commands::KeyEvent::F24},
  {scim::SCIM_KEY_Page_Up, mozc::commands::KeyEvent::PAGE_UP},
  {scim::SCIM_KEY_Page_Down, mozc::commands::KeyEvent::PAGE_DOWN},

  // Keypad (10-key).
  {scim::SCIM_KEY_KP_0, mozc::commands::KeyEvent::NUMPAD0},
  {scim::SCIM_KEY_KP_1, mozc::commands::KeyEvent::NUMPAD1},
  {scim::SCIM_KEY_KP_2, mozc::commands::KeyEvent::NUMPAD2},
  {scim::SCIM_KEY_KP_3, mozc::commands::KeyEvent::NUMPAD3},
  {scim::SCIM_KEY_KP_4, mozc::commands::KeyEvent::NUMPAD4},
  {scim::SCIM_KEY_KP_5, mozc::commands::KeyEvent::NUMPAD5},
  {scim::SCIM_KEY_KP_6, mozc::commands::KeyEvent::NUMPAD6},
  {scim::SCIM_KEY_KP_7, mozc::commands::KeyEvent::NUMPAD7},
  {scim::SCIM_KEY_KP_8, mozc::commands::KeyEvent::NUMPAD8},
  {scim::SCIM_KEY_KP_9, mozc::commands::KeyEvent::NUMPAD9},
  {scim::SCIM_KEY_KP_Equal, mozc::commands::KeyEvent::EQUALS},  // [=]
  {scim::SCIM_KEY_KP_Multiply, mozc::commands::KeyEvent::MULTIPLY},  // [*]
  {scim::SCIM_KEY_KP_Add, mozc::commands::KeyEvent::ADD},  // [+]
  {scim::SCIM_KEY_KP_Separator, mozc::commands::KeyEvent::SEPARATOR},  // enter
  {scim::SCIM_KEY_KP_Subtract, mozc::commands::KeyEvent::SUBTRACT},  // [-]
  {scim::SCIM_KEY_KP_Decimal, mozc::commands::KeyEvent::DECIMAL},  // [.]
  {scim::SCIM_KEY_KP_Divide, mozc::commands::KeyEvent::DIVIDE},  // [/]
  {scim::SCIM_KEY_KP_Space, mozc::commands::KeyEvent::SPACE},
  {scim::SCIM_KEY_KP_Tab, mozc::commands::KeyEvent::TAB},
  {scim::SCIM_KEY_KP_Enter, mozc::commands::KeyEvent::ENTER},
  {scim::SCIM_KEY_KP_Home, mozc::commands::KeyEvent::HOME},
  {scim::SCIM_KEY_KP_Left, mozc::commands::KeyEvent::LEFT},
  {scim::SCIM_KEY_KP_Up, mozc::commands::KeyEvent::UP},
  {scim::SCIM_KEY_KP_Right, mozc::commands::KeyEvent::RIGHT},
  {scim::SCIM_KEY_KP_Down, mozc::commands::KeyEvent::DOWN},
  {scim::SCIM_KEY_KP_Page_Up, mozc::commands::KeyEvent::PAGE_UP},
  {scim::SCIM_KEY_KP_Page_Down, mozc::commands::KeyEvent::PAGE_DOWN},
  {scim::SCIM_KEY_KP_End, mozc::commands::KeyEvent::END},
  {scim::SCIM_KEY_KP_Delete, mozc::commands::KeyEvent::DEL},
  {scim::SCIM_KEY_KP_Insert, mozc::commands::KeyEvent::INSERT},

  // Shift+TAB.
  {scim::SCIM_KEY_ISO_Left_Tab, mozc::commands::KeyEvent::TAB},

  // TODO(yusukes): Handle following keys?
  //   - SCIM_KEY_Kana_Lock? SCIM_KEY_Kana_Shift?
};

const struct SpecialAsciiMap {
  uint32 from;
  uint32 to;
} sp_ascii_map[] = {
  {scim::SCIM_KEY_KP_Equal, '='},
};

struct KanaMap {
  uint32 code;
  string no_shift, shift;
};

// TODO(yusukes): We should merge the two identical mappings in unix/ibus/ and
// unix/scim/.
const KanaMap kana_map_jp[] = {
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
};

const KanaMap kana_map_us[] = {
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

namespace mozc_unix_scim {

ScimKeyTranslator::ScimKeyTranslator() {
  InitializeKeyMaps();
}

ScimKeyTranslator::~ScimKeyTranslator() {
}

void ScimKeyTranslator::Translate(
    const scim::KeyEvent &key, mozc::config::Config::PreeditMethod method,
    mozc::commands::KeyEvent *out_event) const {
  DCHECK(CanConvert(key));
  if (!CanConvert(key)) {
    LOG(ERROR) << "Can't handle the key: " << key.code;
    return;
  }
  DCHECK(out_event);
  if (!out_event) {
    return;
  }

  if (key.is_control_down()) {
    out_event->add_modifier_keys(mozc::commands::KeyEvent::CTRL);
  }
  if (key.is_alt_down()) {
    out_event->add_modifier_keys(mozc::commands::KeyEvent::ALT);
  }
  if (!IsAscii(key) && key.is_shift_down()) {
    out_event->add_modifier_keys(mozc::commands::KeyEvent::SHIFT);
  }

  mozc::commands::KeyEvent::SpecialKey sp_key;
  uint32 sp_ascii;
  string key_string;

  if (IsSpecialKey(key, &sp_key)) {
    out_event->set_special_key(sp_key);
  } else if (IsSpecialAscii(key, &sp_ascii)) {
    out_event->set_key_code(sp_ascii);
  } else if (method == mozc::config::Config::KANA &&
             IsKanaAvailable(key, &key_string)) {
    DCHECK(IsAscii(key));
    out_event->set_key_code(key.get_ascii_code());
    out_event->set_key_string(key_string);
  } else {
    DCHECK(IsAscii(key));
    out_event->set_key_code(key.get_ascii_code());
  }
  return;
}

void ScimKeyTranslator::TranslateClick(
    int32 unique_id, mozc::commands::SessionCommand *out_command) const {
  DCHECK(out_command);
  if (out_command) {
    out_command->set_type(mozc::commands::SessionCommand::SELECT_CANDIDATE);
    out_command->set_id(unique_id);
  }
  return;
}

bool ScimKeyTranslator::CanConvert(const scim::KeyEvent &key) const {
  if (key.is_key_release() || IsModifierKey(key)) {
    return false;
  }
  if (IsAscii(key) || IsSpecialKey(key, NULL) || IsSpecialAscii(key, NULL)) {
    return true;
  }

  char buf[64];
  ::snprintf(
      buf, sizeof(buf), "Key code Mozc doesn't know (0x%08x).", key.code);
  LOG(ERROR) << buf;
  return false;
}

void ScimKeyTranslator::InitializeKeyMaps() {
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
  for (int i = 0; i < arraysize(kana_map_jp); ++i) {
    KanaMapEntry entry(kana_map_jp[i].no_shift, kana_map_jp[i].shift);
    CHECK(kana_map_jp_.insert(make_pair(kana_map_jp[i].code, entry)).second);
  }
  for (int i = 0; i < arraysize(kana_map_us); ++i) {
    KanaMapEntry entry(kana_map_us[i].no_shift, kana_map_us[i].shift);
    CHECK(kana_map_us_.insert(make_pair(kana_map_us[i].code, entry)).second);
  }
}

bool ScimKeyTranslator::IsModifierKey(const scim::KeyEvent &key) const {
  return modifier_keys_.find(key.code) != modifier_keys_.end();
}

bool ScimKeyTranslator::IsSpecialKey(
    const scim::KeyEvent &key,
    mozc::commands::KeyEvent::SpecialKey *out) const {
  map<uint32, mozc::commands::KeyEvent::SpecialKey>::const_iterator iter =
      special_key_map_.find(key.code);
  if (iter == special_key_map_.end()) {
    return false;
  }
  if (out) {
    *out = iter->second;
  }
  return true;
}

bool ScimKeyTranslator::IsSpecialAscii(
    const scim::KeyEvent &key, uint32 *out) const {
  map<uint32, uint32>::const_iterator iter = special_ascii_map_.find(key.code);
  if (iter == special_ascii_map_.end()) {
    return false;
  }
  if (out) {
    *out = iter->second;
  }
  return true;
}

bool ScimKeyTranslator::IsKanaAvailable(
    const scim::KeyEvent &key, string *out) const {
  if (key.is_control_down() || key.is_alt_down()) {
    return false;
  }
  const map<uint32, KanaMapEntry> &kana_map =
      IsJapaneseLayout(key.layout) ? kana_map_jp_ : kana_map_us_;
  map<uint32, KanaMapEntry>::const_iterator iter =
      kana_map.find(key.code);
  if (iter == kana_map.end()) {
    return false;
  }
  if (out) {
    if (key.code == '\\' && IsJapaneseLayout(key.layout)) {
      if ((key.mask & scim::SCIM_KEY_QuirkKanaRoMask) != 0) {
        *out = "\xe3\x82\x8d";  // "ろ"
      } else {
        *out = "\xe3\x83\xbc";  // "ー"
      }
    } else {
      *out = key.is_shift_down() ? iter->second.shift : iter->second.no_shift;
    }
  }
  return true;
}

/* static */
bool ScimKeyTranslator::IsAscii(const scim::KeyEvent &key) {
  // key.get_ascii_code() returns non-zero value for SPACE, ENTER, LineFeed,
  // TAB, BACKSPACE, ESCAPE, and Keypad codes. So we don't use it here.
  return (key.code > scim::SCIM_KEY_space &&
          // Note: Space key (0x20) is a special key in Mozc.
          key.code <= scim::SCIM_KEY_asciitilde);  // 0x7e.
}

/* static */
bool ScimKeyTranslator::IsJapaneseLayout(uint16 layout) {
  // We guess that most people using the Kana input mode uses Japanese
  // keyboards, so we prefer applying the Japanese layout.
  return layout == scim::SCIM_KEYBOARD_Unknown ||
      layout == scim::SCIM_KEYBOARD_Japanese;
}

}  // namespace mozc_unix_scim;
