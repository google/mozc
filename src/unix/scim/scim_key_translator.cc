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
  if (key.is_key_release()) {
    VLOG(1) << "key release";
    return false;
  }
  if (IsModifierKey(key)) {
    VLOG(1) << "modifier key";
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
  for (int i = 0; i < arraysize(kKanaMapJp); ++i) {
    CHECK(kana_map_jp_.insert(
        make_pair(kKanaMapJp[i].keysym, kKanaMapJp[i].kana)).second);
  }
  for (int i = 0; i < arraysize(kKanaMapUs); ++i) {
    CHECK(kana_map_us_.insert(
        make_pair(kKanaMapUs[i].keysym, kKanaMapUs[i].kana)).second);
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
  const map<uint32, const char *> &kana_map =
      IsJapaneseLayout(key.layout) ? kana_map_jp_ : kana_map_us_;

  // We call get_ascii_code() to support clients that does not send the shift
  // modifier. By calling the function, both "Shift + 3" and "#" would be
  // normalized to '#'.
  const char ascii_code = key.get_ascii_code();

  map<uint32, const char *>::const_iterator iter = kana_map.find(ascii_code);
  if (iter == kana_map.end()) {
    return false;
  }
  if (out) {
    if (ascii_code == '\\' && IsJapaneseLayout(key.layout)) {
      if ((key.mask & scim::SCIM_KEY_QuirkKanaRoMask) != 0) {
        *out = "\xe3\x82\x8d";  // "ろ"
      } else {
        *out = "\xe3\x83\xbc";  // "ー"
      }
    } else {
      *out = iter->second;
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
