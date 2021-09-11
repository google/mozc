// Copyright 2010-2021, Google Inc.
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

#include <map>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/port.h"

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
    {IBUS_Hiragana_Katakana, mozc::commands::KeyEvent::KANA},
    {IBUS_Katakana, mozc::commands::KeyEvent::KATAKANA},
    {IBUS_Zenkaku, mozc::commands::KeyEvent::HANKAKU},
    {IBUS_Hankaku, mozc::commands::KeyEvent::HANKAKU},
    {IBUS_Zenkaku_Hankaku, mozc::commands::KeyEvent::HANKAKU},
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
    {IBUS_KP_Equal, mozc::commands::KeyEvent::EQUALS},         // [=]
    {IBUS_KP_Multiply, mozc::commands::KeyEvent::MULTIPLY},    // [*]
    {IBUS_KP_Add, mozc::commands::KeyEvent::ADD},              // [+]
    {IBUS_KP_Separator, mozc::commands::KeyEvent::SEPARATOR},  // enter
    {IBUS_KP_Subtract, mozc::commands::KeyEvent::SUBTRACT},    // [-]
    {IBUS_KP_Decimal, mozc::commands::KeyEvent::DECIMAL},      // [.]
    {IBUS_KP_Divide, mozc::commands::KeyEvent::DIVIDE},        // [/]
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

const struct ModifierKeyMapData {
  guint from;
  mozc::commands::KeyEvent::ModifierKey to;
} modifier_key_map_data[] = {
    {IBUS_Shift_L, mozc::commands::KeyEvent::SHIFT},
    {IBUS_Shift_R, mozc::commands::KeyEvent::SHIFT},
    {IBUS_Control_L, mozc::commands::KeyEvent::CTRL},
    {IBUS_Control_R, mozc::commands::KeyEvent::CTRL},
    {IBUS_Alt_L, mozc::commands::KeyEvent::ALT},
    {IBUS_Alt_R, mozc::commands::KeyEvent::ALT},
    {IBUS_LOCK_MASK, mozc::commands::KeyEvent::CAPS},
};

const struct ModifierMaskMapData {
  guint from;
  mozc::commands::KeyEvent::ModifierKey to;
} modifier_mask_map_data[] = {
    {IBUS_SHIFT_MASK, mozc::commands::KeyEvent::SHIFT},
    {IBUS_CONTROL_MASK, mozc::commands::KeyEvent::CTRL},
    {IBUS_MOD1_MASK, mozc::commands::KeyEvent::ALT},
};

// TODO(team): Add kana_map_dv to support Dvoraklayout.
const struct KanaMap {
  guint code;
  const char *no_shift;
  const char *shift;
} kana_map_jp[] =
    {
        {'1', "ぬ", "ぬ"},
        {'!', "ぬ", "ぬ"},
        {'2', "ふ", "ふ"},
        {'\"', "ふ", "ふ"},
        {'3', "あ", "ぁ"},
        {'#', "あ", "ぁ"},
        {'4', "う", "ぅ"},
        {'$', "う", "ぅ"},
        {'5', "え", "ぇ"},
        {'%', "え", "ぇ"},
        {'6', "お", "ぉ"},
        {'&', "お", "ぉ"},
        {'7', "や", "ゃ"},
        {'\'', "や", "ゃ"},
        {'8', "ゆ", "ゅ"},
        {'(', "ゆ", "ゅ"},
        {'9', "よ", "ょ"},
        {')', "よ", "ょ"},
        {'0', "わ", "を"},
        {'-', "ほ", "ほ"},
        {'=', "ほ", "ほ"},
        {'^', "へ", "を"},
        {'~', "へ", "を"},
        {'|', "ー", "ー"},
        {'q', "た", "た"},
        {'Q', "た", "た"},
        {'w', "て", "て"},
        {'W', "て", "て"},
        {'e', "い", "ぃ"},
        {'E', "い", "ぃ"},
        {'r', "す", "す"},
        {'R', "す", "す"},
        {'t', "か", "か"},
        {'T', "か", "か"},
        {'y', "ん", "ん"},
        {'Y', "ん", "ん"},
        {'u', "な", "な"},
        {'U', "な", "な"},
        {'i', "に", "に"},
        {'I', "に", "に"},
        {'o', "ら", "ら"},
        {'O', "ら", "ら"},
        {'p', "せ", "せ"},
        {'P', "せ", "せ"},
        {'@', "゛", "゛"},
        {'`', "゛", "゛"},
        {'[', "゜", "「"},
        {'{', "゜", "「"},
        {'a', "ち", "ち"},
        {'A', "ち", "ち"},
        {'s', "と", "と"},
        {'S', "と", "と"},
        {'d', "し", "し"},
        {'D', "し", "し"},
        {'f', "は", "は"},
        {'F', "は", "は"},
        {'g', "き", "き"},
        {'G', "き", "き"},
        {'h', "く", "く"},
        {'H', "く", "く"},
        {'j', "ま", "ま"},
        {'J', "ま", "ま"},
        {'k', "の", "の"},
        {'K', "の", "の"},
        {'l', "り", "り"},
        {'L', "り", "り"},
        {';', "れ", "れ"},
        {'+', "れ", "れ"},
        {':', "け", "け"},
        {'*', "け", "け"},
        {']', "む", "」"},
        {'}', "む", "」"},
        {'z', "つ", "っ"},
        {'Z', "つ", "っ"},
        {'x', "さ", "さ"},
        {'X', "さ", "さ"},
        {'c', "そ", "そ"},
        {'C', "そ", "そ"},
        {'v', "ひ", "ひ"},
        {'V', "ひ", "ひ"},
        {'b', "こ", "こ"},
        {'B', "こ", "こ"},
        {'n', "み", "み"},
        {'N', "み", "み"},
        {'m', "も", "も"},
        {'M', "も", "も"},
        {',', "ね", "、"},
        {'<', "ね", "、"},
        {'.', "る", "。"},
        {'>', "る", "。"},
        {'/', "め", "・"},
        {'?', "め", "・"},
        {'_', "ろ", "ろ"},
        // A backslash is handled in a special way because it is input by
        // two different keys (the one next to Backslash and the one next
        // to Right Shift).
        {'\\', "", ""},
},
  kana_map_us[] = {
      {'`', "ろ", "ろ"},  {'~', "ろ", "ろ"},  {'1', "ぬ", "ぬ"},
      {'!', "ぬ", "ぬ"},  {'2', "ふ", "ふ"},  {'@', "ふ", "ふ"},
      {'3', "あ", "ぁ"},  {'#', "あ", "ぁ"},  {'4', "う", "ぅ"},
      {'$', "う", "ぅ"},  {'5', "え", "ぇ"},  {'%', "え", "ぇ"},
      {'6', "お", "ぉ"},  {'^', "お", "ぉ"},  {'7', "や", "ゃ"},
      {'&', "や", "ゃ"},  {'8', "ゆ", "ゅ"},  {'*', "ゆ", "ゅ"},
      {'9', "よ", "ょ"},  {'(', "よ", "ょ"},  {'0', "わ", "を"},
      {')', "わ", "を"},  {'-', "ほ", "ー"},  {'_', "ほ", "ー"},
      {'=', "へ", "へ"},  {'+', "へ", "へ"},  {'q', "た", "た"},
      {'Q', "た", "た"},  {'w', "て", "て"},  {'W', "て", "て"},
      {'e', "い", "ぃ"},  {'E', "い", "ぃ"},  {'r', "す", "す"},
      {'R', "す", "す"},  {'t', "か", "か"},  {'T', "か", "か"},
      {'y', "ん", "ん"},  {'Y', "ん", "ん"},  {'u', "な", "な"},
      {'U', "な", "な"},  {'i', "に", "に"},  {'I', "に", "に"},
      {'o', "ら", "ら"},  {'O', "ら", "ら"},  {'p', "せ", "せ"},
      {'P', "せ", "せ"},  {'[', "゛", "゛"},  {'{', "゛", "゛"},
      {']', "゜", "「"},  {'}', "゜", "「"},  {'\\', "む", "」"},
      {'|', "む", "」"},  {'a', "ち", "ち"},  {'A', "ち", "ち"},
      {'s', "と", "と"},  {'S', "と", "と"},  {'d', "し", "し"},
      {'D', "し", "し"},  {'f', "は", "は"},  {'F', "は", "は"},
      {'g', "き", "き"},  {'G', "き", "き"},  {'h', "く", "く"},
      {'H', "く", "く"},  {'j', "ま", "ま"},  {'J', "ま", "ま"},
      {'k', "の", "の"},  {'K', "の", "の"},  {'l', "り", "り"},
      {'L', "り", "り"},  {';', "れ", "れ"},  {':', "れ", "れ"},
      {'\'', "け", "け"}, {'\"', "け", "け"}, {'z', "つ", "っ"},
      {'Z', "つ", "っ"},  {'x', "さ", "さ"},  {'X', "さ", "さ"},
      {'c', "そ", "そ"},  {'C', "そ", "そ"},  {'v', "ひ", "ひ"},
      {'V', "ひ", "ひ"},  {'b', "こ", "こ"},  {'B', "こ", "こ"},
      {'n', "み", "み"},  {'N', "み", "み"},  {'m', "も", "も"},
      {'M', "も", "も"},  {',', "ね", "、"},  {'<', "ね", "、"},
      {'.', "る", "。"},  {'>', "る", "。"},  {'/', "め", "・"},
      {'?', "め", "・"},
};

}  // namespace

namespace mozc {
namespace ibus {

KeyTranslator::KeyTranslator() { Init(); }

KeyTranslator::~KeyTranslator() {}

// TODO(nona): Fix 'Shift-0' behavior b/4338394
bool KeyTranslator::Translate(guint keyval, guint keycode, guint modifiers,
                              config::Config::PreeditMethod method,
                              bool layout_is_jp,
                              commands::KeyEvent *out_event) const {
  DCHECK(out_event) << "out_event is nullptr";
  out_event->Clear();

  // Due to historical reasons, many linux ditributions set Hiragana_Katakana
  // key as Hiragana key (which is Katkana key with shift modifier). So, we
  // translate Hiragana_Katanaka key as Hiragana key by mapping table, and
  // Shift + Hiragana_Katakana key as Katakana key by functionally.
  // TODO(nona): Fix process modifier to handle right shift
  if (IsHiraganaKatakanaKeyWithShift(keyval, keycode, modifiers)) {
    modifiers &= ~IBUS_SHIFT_MASK;
    keyval = IBUS_Katakana;
  }
  std::string kana_key_string;
  if ((method == config::Config::KANA) &&
      IsKanaAvailable(keyval, keycode, modifiers, layout_is_jp,
                      &kana_key_string)) {
    out_event->set_key_code(keyval);
    out_event->set_key_string(kana_key_string);
  } else if (IsAscii(keyval, keycode, modifiers)) {
    if (IBUS_LOCK_MASK & modifiers) {
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
      // Add a modifier key if doesn't exist.
      commands::KeyEvent::ModifierKey modifier = i->second;
      bool found = false;
      for (int i = 0; i < out_event->modifier_keys_size(); ++i) {
        if (modifier == out_event->modifier_keys(i)) {
          found = true;
          break;
        }
      }
      if (!found) {
        out_event->add_modifier_keys(modifier);
      }
    }
  }

  return true;
}

void KeyTranslator::Init() {
  for (int i = 0; i < std::size(special_key_map); ++i) {
    CHECK(special_key_map_
              .insert(std::make_pair(special_key_map[i].from,
                                     special_key_map[i].to))
              .second);
  }
  for (int i = 0; i < std::size(modifier_key_map_data); ++i) {
    CHECK(modifier_key_map_
              .insert(std::make_pair(modifier_key_map_data[i].from,
                                     modifier_key_map_data[i].to))
              .second);
  }
  for (int i = 0; i < std::size(modifier_mask_map_data); ++i) {
    CHECK(modifier_mask_map_
              .insert(std::make_pair(modifier_mask_map_data[i].from,
                                     modifier_mask_map_data[i].to))
              .second);
  }
  for (int i = 0; i < std::size(kana_map_jp); ++i) {
    CHECK(kana_map_jp_
              .insert(std::make_pair(kana_map_jp[i].code,
                                     std::make_pair(kana_map_jp[i].no_shift,
                                                    kana_map_jp[i].shift)))
              .second);
  }
  for (int i = 0; i < std::size(kana_map_us); ++i) {
    CHECK(kana_map_us_
              .insert(std::make_pair(kana_map_us[i].code,
                                     std::make_pair(kana_map_us[i].no_shift,
                                                    kana_map_us[i].shift)))
              .second);
  }
}

bool KeyTranslator::IsModifierKey(guint keyval, guint keycode,
                                  guint modifiers) const {
  return modifier_key_map_.find(keyval) != modifier_key_map_.end();
}

bool KeyTranslator::IsSpecialKey(guint keyval, guint keycode,
                                 guint modifiers) const {
  return special_key_map_.find(keyval) != special_key_map_.end();
}

bool KeyTranslator::IsHiraganaKatakanaKeyWithShift(guint keyval, guint keycode,
                                                   guint modifiers) {
  return ((modifiers & IBUS_SHIFT_MASK) && (keyval == IBUS_Hiragana_Katakana));
}

bool KeyTranslator::IsKanaAvailable(guint keyval, guint keycode,
                                    guint modifiers, bool layout_is_jp,
                                    std::string *out) const {
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
    // details: https://github.com/ibus/ibus/issues/73
    if (keyval == '\\' && layout_is_jp) {
      if (keycode == IBUS_bar) {
        *out = "ー";
      } else {
        *out = "ろ";
      }
    } else {
      *out = (modifiers & IBUS_SHIFT_MASK) ? iter->second.second
                                           : iter->second.first;
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
