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

#include <optional>
#include <string>
#include <tuple>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/container/flat_map.h"
#include "base/vlog.h"

namespace mozc {
namespace {
constexpr auto kSpecialKeyMap =
    CreateFlatMap<uint, commands::KeyEvent::SpecialKey>({
        {IBUS_space, commands::KeyEvent::SPACE},
        {IBUS_Return, commands::KeyEvent::ENTER},
        {IBUS_Left, commands::KeyEvent::LEFT},
        {IBUS_Right, commands::KeyEvent::RIGHT},
        {IBUS_Up, commands::KeyEvent::UP},
        {IBUS_Down, commands::KeyEvent::DOWN},
        {IBUS_Escape, commands::KeyEvent::ESCAPE},
        {IBUS_Delete, commands::KeyEvent::DEL},
        {IBUS_BackSpace, commands::KeyEvent::BACKSPACE},
        {IBUS_Insert, commands::KeyEvent::INSERT},
        {IBUS_Henkan, commands::KeyEvent::HENKAN},
        {IBUS_Muhenkan, commands::KeyEvent::MUHENKAN},
        {IBUS_Hiragana, commands::KeyEvent::KANA},
        {IBUS_Hiragana_Katakana, commands::KeyEvent::KANA},
        {IBUS_Katakana, commands::KeyEvent::KATAKANA},
        {IBUS_Zenkaku, commands::KeyEvent::HANKAKU},
        {IBUS_Hankaku, commands::KeyEvent::HANKAKU},
        {IBUS_Zenkaku_Hankaku, commands::KeyEvent::HANKAKU},
        {IBUS_Eisu_toggle, commands::KeyEvent::EISU},
        {IBUS_Home, commands::KeyEvent::HOME},
        {IBUS_End, commands::KeyEvent::END},
        {IBUS_Tab, commands::KeyEvent::TAB},
        {IBUS_F1, commands::KeyEvent::F1},
        {IBUS_F2, commands::KeyEvent::F2},
        {IBUS_F3, commands::KeyEvent::F3},
        {IBUS_F4, commands::KeyEvent::F4},
        {IBUS_F5, commands::KeyEvent::F5},
        {IBUS_F6, commands::KeyEvent::F6},
        {IBUS_F7, commands::KeyEvent::F7},
        {IBUS_F8, commands::KeyEvent::F8},
        {IBUS_F9, commands::KeyEvent::F9},
        {IBUS_F10, commands::KeyEvent::F10},
        {IBUS_F11, commands::KeyEvent::F11},
        {IBUS_F12, commands::KeyEvent::F12},
        {IBUS_F13, commands::KeyEvent::F13},
        {IBUS_F14, commands::KeyEvent::F14},
        {IBUS_F15, commands::KeyEvent::F15},
        {IBUS_F16, commands::KeyEvent::F16},
        {IBUS_F17, commands::KeyEvent::F17},
        {IBUS_F18, commands::KeyEvent::F18},
        {IBUS_F19, commands::KeyEvent::F19},
        {IBUS_F20, commands::KeyEvent::F20},
        {IBUS_F21, commands::KeyEvent::F21},
        {IBUS_F22, commands::KeyEvent::F22},
        {IBUS_F23, commands::KeyEvent::F23},
        {IBUS_F24, commands::KeyEvent::F24},
        {IBUS_Page_Up, commands::KeyEvent::PAGE_UP},
        {IBUS_Page_Down, commands::KeyEvent::PAGE_DOWN},

        // Keypad (10-key).
        {IBUS_KP_0, commands::KeyEvent::NUMPAD0},
        {IBUS_KP_1, commands::KeyEvent::NUMPAD1},
        {IBUS_KP_2, commands::KeyEvent::NUMPAD2},
        {IBUS_KP_3, commands::KeyEvent::NUMPAD3},
        {IBUS_KP_4, commands::KeyEvent::NUMPAD4},
        {IBUS_KP_5, commands::KeyEvent::NUMPAD5},
        {IBUS_KP_6, commands::KeyEvent::NUMPAD6},
        {IBUS_KP_7, commands::KeyEvent::NUMPAD7},
        {IBUS_KP_8, commands::KeyEvent::NUMPAD8},
        {IBUS_KP_9, commands::KeyEvent::NUMPAD9},
        {IBUS_KP_Equal, commands::KeyEvent::EQUALS},         // [=]
        {IBUS_KP_Multiply, commands::KeyEvent::MULTIPLY},    // [*]
        {IBUS_KP_Add, commands::KeyEvent::ADD},              // [+]
        {IBUS_KP_Separator, commands::KeyEvent::SEPARATOR},  // enter
        {IBUS_KP_Subtract, commands::KeyEvent::SUBTRACT},    // [-]
        {IBUS_KP_Decimal, commands::KeyEvent::DECIMAL},      // [.]
        {IBUS_KP_Divide, commands::KeyEvent::DIVIDE},        // [/]
        {IBUS_KP_Space, commands::KeyEvent::SPACE},
        {IBUS_KP_Tab, commands::KeyEvent::TAB},
        {IBUS_KP_Enter, commands::KeyEvent::ENTER},
        {IBUS_KP_Home, commands::KeyEvent::HOME},
        {IBUS_KP_Left, commands::KeyEvent::LEFT},
        {IBUS_KP_Up, commands::KeyEvent::UP},
        {IBUS_KP_Right, commands::KeyEvent::RIGHT},
        {IBUS_KP_Down, commands::KeyEvent::DOWN},
        {IBUS_KP_Page_Up, commands::KeyEvent::PAGE_UP},
        {IBUS_KP_Page_Down, commands::KeyEvent::PAGE_DOWN},
        {IBUS_KP_End, commands::KeyEvent::END},
        {IBUS_KP_Delete, commands::KeyEvent::DEL},
        {IBUS_KP_Insert, commands::KeyEvent::INSERT},
        {IBUS_Caps_Lock, commands::KeyEvent::CAPS_LOCK},

        // Shift+TAB.
        {IBUS_ISO_Left_Tab, commands::KeyEvent::TAB},

        // On Linux (X / Wayland), Hangul and Hanja are identical with
        // ImeOn and ImeOff.
        // https://github.com/google/mozc/issues/552
        //
        // Hangul == Lang1 (USB HID) / ImeOn (Windows) / Kana (macOS)
        {IBUS_Hangul, commands::KeyEvent::ON},
        // Hanja == Lang2 (USB HID) / ImeOff (Windows) / Eisu (macOS)
        {IBUS_Hangul_Hanja, commands::KeyEvent::OFF},

        // TODO(mazda): Handle following keys?
        //   - IBUS_Kana_Lock? IBUS_KEY_Kana_Shift?
    });

constexpr auto kIbusModifierMaskMap = CreateFlatMap<uint, uint>({
    {IBUS_Shift_L, IBUS_SHIFT_MASK},
    {IBUS_Shift_R, IBUS_SHIFT_MASK},
    {IBUS_Control_L, IBUS_CONTROL_MASK},
    {IBUS_Control_R, IBUS_CONTROL_MASK},
    {IBUS_Alt_L, IBUS_MOD1_MASK},
    {IBUS_Alt_R, IBUS_MOD1_MASK},
});

struct Kana {
  absl::string_view non_shift;
  absl::string_view shift;

  constexpr bool operator<(const Kana &other) const {
    return std::tie(non_shift, shift) < std::tie(other.non_shift, other.shift);
  }
};

// Stores a mapping from ASCII to Kana character. For example, ASCII character
// '4' is mapped to Japanese 'Hiragana Letter U' (without Shift modifier) and
// 'Hiragana Letter Small U' (with Shift modifier).
// TODO(team): Add kana_map_dv to support Dvoraklayout.
constexpr auto kKanaJpMap = CreateFlatMap<uint, Kana>({
    {'1', {"ぬ", "ぬ"}},
    {'!', {"ぬ", "ぬ"}},
    {'2', {"ふ", "ふ"}},
    {'\"', {"ふ", "ふ"}},
    {'3', {"あ", "ぁ"}},
    {'#', {"あ", "ぁ"}},
    {'4', {"う", "ぅ"}},
    {'$', {"う", "ぅ"}},
    {'5', {"え", "ぇ"}},
    {'%', {"え", "ぇ"}},
    {'6', {"お", "ぉ"}},
    {'&', {"お", "ぉ"}},
    {'7', {"や", "ゃ"}},
    {'\'', {"や", "ゃ"}},
    {'8', {"ゆ", "ゅ"}},
    {'(', {"ゆ", "ゅ"}},
    {'9', {"よ", "ょ"}},
    {')', {"よ", "ょ"}},
    {'0', {"わ", "を"}},
    {'-', {"ほ", "ほ"}},
    {'=', {"ほ", "ほ"}},
    {'^', {"へ", "を"}},
    {'~', {"へ", "を"}},
    {'|', {"ー", "ー"}},
    {'q', {"た", "た"}},
    {'Q', {"た", "た"}},
    {'w', {"て", "て"}},
    {'W', {"て", "て"}},
    {'e', {"い", "ぃ"}},
    {'E', {"い", "ぃ"}},
    {'r', {"す", "す"}},
    {'R', {"す", "す"}},
    {'t', {"か", "か"}},
    {'T', {"か", "か"}},
    {'y', {"ん", "ん"}},
    {'Y', {"ん", "ん"}},
    {'u', {"な", "な"}},
    {'U', {"な", "な"}},
    {'i', {"に", "に"}},
    {'I', {"に", "に"}},
    {'o', {"ら", "ら"}},
    {'O', {"ら", "ら"}},
    {'p', {"せ", "せ"}},
    {'P', {"せ", "せ"}},
    {'@', {"゛", "゛"}},
    {'`', {"゛", "゛"}},
    {'[', {"゜", "「"}},
    {'{', {"゜", "「"}},
    {'a', {"ち", "ち"}},
    {'A', {"ち", "ち"}},
    {'s', {"と", "と"}},
    {'S', {"と", "と"}},
    {'d', {"し", "し"}},
    {'D', {"し", "し"}},
    {'f', {"は", "は"}},
    {'F', {"は", "は"}},
    {'g', {"き", "き"}},
    {'G', {"き", "き"}},
    {'h', {"く", "く"}},
    {'H', {"く", "く"}},
    {'j', {"ま", "ま"}},
    {'J', {"ま", "ま"}},
    {'k', {"の", "の"}},
    {'K', {"の", "の"}},
    {'l', {"り", "り"}},
    {'L', {"り", "り"}},
    {';', {"れ", "れ"}},
    {'+', {"れ", "れ"}},
    {':', {"け", "け"}},
    {'*', {"け", "け"}},
    {']', {"む", "」"}},
    {'}', {"む", "」"}},
    {'z', {"つ", "っ"}},
    {'Z', {"つ", "っ"}},
    {'x', {"さ", "さ"}},
    {'X', {"さ", "さ"}},
    {'c', {"そ", "そ"}},
    {'C', {"そ", "そ"}},
    {'v', {"ひ", "ひ"}},
    {'V', {"ひ", "ひ"}},
    {'b', {"こ", "こ"}},
    {'B', {"こ", "こ"}},
    {'n', {"み", "み"}},
    {'N', {"み", "み"}},
    {'m', {"も", "も"}},
    {'M', {"も", "も"}},
    {',', {"ね", "、"}},
    {'<', {"ね", "、"}},
    {'.', {"る", "。"}},
    {'>', {"る", "。"}},
    {'/', {"め", "・"}},
    {'?', {"め", "・"}},
    {'_', {"ろ", "ろ"}},
    // A backslash is handled in a special way because it is input by
    // two different keys (the one next to Backslash and the one next
    // to Right Shift).
    {'\\', {"ろ", "ろ"}},
    {U'¥', {"ー", "ー"}},  // U+00A5
});

constexpr auto kKanaUsMap = CreateFlatMap<uint, Kana>({
    {'`', {"ろ", "ろ"}},  {'~', {"ろ", "ろ"}},  {'1', {"ぬ", "ぬ"}},
    {'!', {"ぬ", "ぬ"}},  {'2', {"ふ", "ふ"}},  {'@', {"ふ", "ふ"}},
    {'3', {"あ", "ぁ"}},  {'#', {"あ", "ぁ"}},  {'4', {"う", "ぅ"}},
    {'$', {"う", "ぅ"}},  {'5', {"え", "ぇ"}},  {'%', {"え", "ぇ"}},
    {'6', {"お", "ぉ"}},  {'^', {"お", "ぉ"}},  {'7', {"や", "ゃ"}},
    {'&', {"や", "ゃ"}},  {'8', {"ゆ", "ゅ"}},  {'*', {"ゆ", "ゅ"}},
    {'9', {"よ", "ょ"}},  {'(', {"よ", "ょ"}},  {'0', {"わ", "を"}},
    {')', {"わ", "を"}},  {'-', {"ほ", "ー"}},  {'_', {"ほ", "ー"}},
    {'=', {"へ", "へ"}},  {'+', {"へ", "へ"}},  {'q', {"た", "た"}},
    {'Q', {"た", "た"}},  {'w', {"て", "て"}},  {'W', {"て", "て"}},
    {'e', {"い", "ぃ"}},  {'E', {"い", "ぃ"}},  {'r', {"す", "す"}},
    {'R', {"す", "す"}},  {'t', {"か", "か"}},  {'T', {"か", "か"}},
    {'y', {"ん", "ん"}},  {'Y', {"ん", "ん"}},  {'u', {"な", "な"}},
    {'U', {"な", "な"}},  {'i', {"に", "に"}},  {'I', {"に", "に"}},
    {'o', {"ら", "ら"}},  {'O', {"ら", "ら"}},  {'p', {"せ", "せ"}},
    {'P', {"せ", "せ"}},  {'[', {"゛", "゛"}},  {'{', {"゛", "゛"}},
    {']', {"゜", "「"}},  {'}', {"゜", "「"}},  {'\\', {"む", "」"}},
    {'|', {"む", "」"}},  {'a', {"ち", "ち"}},  {'A', {"ち", "ち"}},
    {'s', {"と", "と"}},  {'S', {"と", "と"}},  {'d', {"し", "し"}},
    {'D', {"し", "し"}},  {'f', {"は", "は"}},  {'F', {"は", "は"}},
    {'g', {"き", "き"}},  {'G', {"き", "き"}},  {'h', {"く", "く"}},
    {'H', {"く", "く"}},  {'j', {"ま", "ま"}},  {'J', {"ま", "ま"}},
    {'k', {"の", "の"}},  {'K', {"の", "の"}},  {'l', {"り", "り"}},
    {'L', {"り", "り"}},  {';', {"れ", "れ"}},  {':', {"れ", "れ"}},
    {'\'', {"け", "け"}}, {'\"', {"け", "け"}}, {'z', {"つ", "っ"}},
    {'Z', {"つ", "っ"}},  {'x', {"さ", "さ"}},  {'X', {"さ", "さ"}},
    {'c', {"そ", "そ"}},  {'C', {"そ", "そ"}},  {'v', {"ひ", "ひ"}},
    {'V', {"ひ", "ひ"}},  {'b', {"こ", "こ"}},  {'B', {"こ", "こ"}},
    {'n', {"み", "み"}},  {'N', {"み", "み"}},  {'m', {"も", "も"}},
    {'M', {"も", "も"}},  {',', {"ね", "、"}},  {'<', {"ね", "、"}},
    {'.', {"る", "。"}},  {'>', {"る", "。"}},  {'/', {"め", "・"}},
    {'?', {"め", "・"}},
});

std::optional<absl::string_view> GetKanaValue(uint keyval, bool layout_is_jp,
                                              bool is_shift) {
  const Kana *kana = layout_is_jp ? kKanaJpMap.FindOrNull(keyval)
                                  : kKanaUsMap.FindOrNull(keyval);
  if (kana == nullptr) {
    return std::nullopt;
  }

  return is_shift ? kana->shift : kana->non_shift;
}

}  // namespace

namespace ibus {

// TODO(nona): Fix 'Shift-0' behavior b/4338394
bool KeyTranslator::Translate(uint keyval, uint keycode, uint modifiers,
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
    if (modifiers & IBUS_LOCK_MASK) {
      out_event->add_modifier_keys(commands::KeyEvent::CAPS);
    }
    out_event->set_key_code(keyval);
  } else if (const uint *mask = kIbusModifierMaskMap.FindOrNull(keyval);
             mask != nullptr) {
    // Convert Ibus modifier key to mask (e.g. IBUS_Shift_L to IBUS_SHIFT_MASK)
    modifiers |= *mask;
  } else if (const commands::KeyEvent::SpecialKey *key =
                 kSpecialKeyMap.FindOrNull(keyval);
             key != nullptr) {
    out_event->set_special_key(*key);
  } else {
    MOZC_VLOG(1) << "Unknown keyval: " << keyval;
    return false;
  }

  // Modifier keys
  if (modifiers & IBUS_SHIFT_MASK && !IsPrintable(keyval, keycode, modifiers)) {
    // Only set a SHIFT modifier when `keyval` is a printable key by following
    // the Mozc's rule.
    out_event->add_modifier_keys(commands::KeyEvent::SHIFT);
  }
  if (modifiers & IBUS_CONTROL_MASK) {
    out_event->add_modifier_keys(commands::KeyEvent::CTRL);
  }
  if (modifiers & IBUS_MOD1_MASK) {
    out_event->add_modifier_keys(commands::KeyEvent::ALT);
  }

  return true;
}

bool KeyTranslator::IsHiraganaKatakanaKeyWithShift(uint keyval, uint keycode,
                                                   uint modifiers) {
  return ((modifiers & IBUS_SHIFT_MASK) && (keyval == IBUS_Hiragana_Katakana));
}

bool KeyTranslator::IsKanaAvailable(uint keyval, uint keycode, uint modifiers,
                                    bool layout_is_jp, std::string *out) const {
  if ((modifiers & IBUS_CONTROL_MASK) || (modifiers & IBUS_MOD1_MASK)) {
    return false;
  }

  // When a Japanese keyboard is in use, the yen-sign key and the backslash
  // key generate the same |keyval|. In this case, we have to check |keycode|
  // to return an appropriate string. See the following IBus issue for
  // details: https://github.com/ibus/ibus/issues/73
  if (layout_is_jp && keyval == '\\' && keycode == IBUS_bar) {
    keyval = U'¥';  // U+00A5
  }

  const bool is_shift = (modifiers & IBUS_SHIFT_MASK);
  std::optional<absl::string_view> kana =
      GetKanaValue(keyval, layout_is_jp, is_shift);

  if (!kana.has_value()) {
    return false;
  }
  if (out) {
    out->assign(*kana);
  }
  return true;
}

// TODO(nona): resolve S-'0' problem (b/4338394).
// TODO(nona): Current printable detection is weak. To enhance accuracy, use xkb
// key map
bool KeyTranslator::IsPrintable(uint keyval, uint keycode, uint modifiers) {
  if ((modifiers & IBUS_CONTROL_MASK) || (modifiers & IBUS_MOD1_MASK)) {
    return false;
  }
  return IsAscii(keyval, keycode, modifiers);
}

bool KeyTranslator::IsAscii(uint keyval, uint keycode, uint modifiers) {
  return (keyval > IBUS_space &&
          // Note: Space key (0x20) is a special key in Mozc.
          keyval <= IBUS_asciitilde);  // 0x7e.
}

}  // namespace ibus
}  // namespace mozc
