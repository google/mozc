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

#include <map>
#include <set>
#include <string>

#include "base/logging.h"

namespace mozc {
namespace {
static const auto kSpecialKeyMap =
    new std::map<uint32_t, commands::KeyEvent::SpecialKey>({
        {FcitxKey_space, commands::KeyEvent::SPACE},
        {FcitxKey_Return, commands::KeyEvent::ENTER},
        {FcitxKey_Left, commands::KeyEvent::LEFT},
        {FcitxKey_Right, commands::KeyEvent::RIGHT},
        {FcitxKey_Up, commands::KeyEvent::UP},
        {FcitxKey_Down, commands::KeyEvent::DOWN},
        {FcitxKey_Escape, commands::KeyEvent::ESCAPE},
        {FcitxKey_Delete, commands::KeyEvent::DEL},
        {FcitxKey_BackSpace, commands::KeyEvent::BACKSPACE},
        {FcitxKey_Insert, commands::KeyEvent::INSERT},
        {FcitxKey_Henkan, commands::KeyEvent::HENKAN},
        {FcitxKey_Muhenkan, commands::KeyEvent::MUHENKAN},
        {FcitxKey_Hiragana, commands::KeyEvent::KANA},
        {FcitxKey_Hiragana_Katakana, commands::KeyEvent::KANA},
        {FcitxKey_Katakana, commands::KeyEvent::KATAKANA},
        {FcitxKey_Zenkaku, commands::KeyEvent::HANKAKU},
        {FcitxKey_Hankaku, commands::KeyEvent::HANKAKU},
        {FcitxKey_Zenkaku_Hankaku, commands::KeyEvent::HANKAKU},
        {FcitxKey_Eisu_toggle, commands::KeyEvent::EISU},
        {FcitxKey_Home, commands::KeyEvent::HOME},
        {FcitxKey_End, commands::KeyEvent::END},
        {FcitxKey_Tab, commands::KeyEvent::TAB},
        {FcitxKey_F1, commands::KeyEvent::F1},
        {FcitxKey_F2, commands::KeyEvent::F2},
        {FcitxKey_F3, commands::KeyEvent::F3},
        {FcitxKey_F4, commands::KeyEvent::F4},
        {FcitxKey_F5, commands::KeyEvent::F5},
        {FcitxKey_F6, commands::KeyEvent::F6},
        {FcitxKey_F7, commands::KeyEvent::F7},
        {FcitxKey_F8, commands::KeyEvent::F8},
        {FcitxKey_F9, commands::KeyEvent::F9},
        {FcitxKey_F10, commands::KeyEvent::F10},
        {FcitxKey_F11, commands::KeyEvent::F11},
        {FcitxKey_F12, commands::KeyEvent::F12},
        {FcitxKey_F13, commands::KeyEvent::F13},
        {FcitxKey_F14, commands::KeyEvent::F14},
        {FcitxKey_F15, commands::KeyEvent::F15},
        {FcitxKey_F16, commands::KeyEvent::F16},
        {FcitxKey_F17, commands::KeyEvent::F17},
        {FcitxKey_F18, commands::KeyEvent::F18},
        {FcitxKey_F19, commands::KeyEvent::F19},
        {FcitxKey_F20, commands::KeyEvent::F20},
        {FcitxKey_F21, commands::KeyEvent::F21},
        {FcitxKey_F22, commands::KeyEvent::F22},
        {FcitxKey_F23, commands::KeyEvent::F23},
        {FcitxKey_F24, commands::KeyEvent::F24},
        {FcitxKey_Page_Up, commands::KeyEvent::PAGE_UP},
        {FcitxKey_Page_Down, commands::KeyEvent::PAGE_DOWN},

        // Keypad (10-key).
        {FcitxKey_KP_0, commands::KeyEvent::NUMPAD0},
        {FcitxKey_KP_1, commands::KeyEvent::NUMPAD1},
        {FcitxKey_KP_2, commands::KeyEvent::NUMPAD2},
        {FcitxKey_KP_3, commands::KeyEvent::NUMPAD3},
        {FcitxKey_KP_4, commands::KeyEvent::NUMPAD4},
        {FcitxKey_KP_5, commands::KeyEvent::NUMPAD5},
        {FcitxKey_KP_6, commands::KeyEvent::NUMPAD6},
        {FcitxKey_KP_7, commands::KeyEvent::NUMPAD7},
        {FcitxKey_KP_8, commands::KeyEvent::NUMPAD8},
        {FcitxKey_KP_9, commands::KeyEvent::NUMPAD9},
        {FcitxKey_KP_Equal, commands::KeyEvent::EQUALS},         // [=]
        {FcitxKey_KP_Multiply, commands::KeyEvent::MULTIPLY},    // [*]
        {FcitxKey_KP_Add, commands::KeyEvent::ADD},              // [+]
        {FcitxKey_KP_Separator, commands::KeyEvent::SEPARATOR},  // enter
        {FcitxKey_KP_Subtract, commands::KeyEvent::SUBTRACT},    // [-]
        {FcitxKey_KP_Decimal, commands::KeyEvent::DECIMAL},      // [.]
        {FcitxKey_KP_Divide, commands::KeyEvent::DIVIDE},        // [/]
        {FcitxKey_KP_Space, commands::KeyEvent::SPACE},
        {FcitxKey_KP_Tab, commands::KeyEvent::TAB},
        {FcitxKey_KP_Enter, commands::KeyEvent::ENTER},
        {FcitxKey_KP_Home, commands::KeyEvent::HOME},
        {FcitxKey_KP_Left, commands::KeyEvent::LEFT},
        {FcitxKey_KP_Up, commands::KeyEvent::UP},
        {FcitxKey_KP_Right, commands::KeyEvent::RIGHT},
        {FcitxKey_KP_Down, commands::KeyEvent::DOWN},
        {FcitxKey_KP_Page_Up, commands::KeyEvent::PAGE_UP},
        {FcitxKey_KP_Page_Down, commands::KeyEvent::PAGE_DOWN},
        {FcitxKey_KP_End, commands::KeyEvent::END},
        {FcitxKey_KP_Delete, commands::KeyEvent::DEL},
        {FcitxKey_KP_Insert, commands::KeyEvent::INSERT},
        {FcitxKey_Caps_Lock, commands::KeyEvent::CAPS_LOCK},

        // Shift+TAB.
        {FcitxKey_ISO_Left_Tab, commands::KeyEvent::TAB},

        // On Linux (X / Wayland), Hangul and Hanja are identical with
        // ImeOn and ImeOff.
        // https://github.com/google/mozc/issues/552
        //
        // Hangul == Lang1 (USB HID) / ImeOn (Windows) / Kana (macOS)
        {FcitxKey_Hangul, commands::KeyEvent::ON},
        // Hanja == Lang2 (USB HID) / ImeOff (Windows) / Eisu (macOS)
        {FcitxKey_Hangul_Hanja, commands::KeyEvent::OFF},

        // TODO(mazda): Handle following keys?
        //   - FcitxKey_Kana_Lock? FcitxKey_KEY_Kana_Shift?
    });

static const auto kIbusModifierMaskMap = new std::map<uint32_t, uint32_t>({
    {FcitxKey_Shift_L, FcitxKeyState_Shift},
    {FcitxKey_Shift_R, FcitxKeyState_Shift},
    {FcitxKey_Control_L, FcitxKeyState_Ctrl},
    {FcitxKey_Control_R, FcitxKeyState_Ctrl},
    {FcitxKey_Alt_L, FcitxKeyState_Alt},
    {FcitxKey_Alt_R, FcitxKeyState_Alt},
});

// Stores a mapping from ASCII to Kana character. For example, ASCII character
// '4' is mapped to Japanese 'Hiragana Letter U' (without Shift modifier) and
// 'Hiragana Letter Small U' (with Shift modifier).
// TODO(team): Add kana_map_dv to support Dvoraklayout.
typedef std::map<uint32_t, std::pair<const char*, const char*>> KanaMap;
static const KanaMap *kKanaJpMap = new KanaMap({
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

static const KanaMap *kKanaUsMap = new KanaMap({
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

const char *GetKanaValue(const KanaMap &kana_map, uint32_t keyval, bool is_shift) {
  KanaMap::const_iterator iter = kana_map.find(keyval);
  if (iter == kana_map.end()) {
    return nullptr;
  }
  return (is_shift) ? iter->second.second : iter->second.first;
}

}  // namespace

namespace fcitx {

// TODO(nona): Fix 'Shift-0' behavior b/4338394
bool KeyTranslator::Translate(FcitxKeySym keyval,
                              uint32_t keycode,
                              uint32_t modifiers,
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
  } else if (auto it = kIbusModifierMaskMap->find(keyval);
             it != kIbusModifierMaskMap->end()) {
    // Convert Ibus modifier key to mask (e.g. FcitxKey_Shift_L to FcitxKeyState_Shift)
    modifiers |= it->second;
  } else if (auto it = kSpecialKeyMap->find(keyval);
             it != kSpecialKeyMap->end()) {
    out_event->set_special_key(it->second);
  } else {
    VLOG(1) << "Unknown keyval: " << keyval;
    return false;
  }

  // Modifier keys
  if (modifiers & FcitxKeyState_Shift && !IsPrintable(keyval, keycode, modifiers)) {
    // Only set a SHIFT modifier when `keyval` is a printable key by following
    // the Mozc's rule.
    out_event->add_modifier_keys(commands::KeyEvent::SHIFT);
  }
  if (modifiers & FcitxKeyState_Ctrl) {
    out_event->add_modifier_keys(commands::KeyEvent::CTRL);
  }
  if (modifiers & FcitxKeyState_Alt) {
    out_event->add_modifier_keys(commands::KeyEvent::ALT);
  }

  return true;
}

bool KeyTranslator::IsHiraganaKatakanaKeyWithShift(uint32_t keyval,
                                                   uint32_t keycode,
                                                   uint32_t modifiers) {
  return ((modifiers & FcitxKeyState_Shift) && (keyval == FcitxKey_Hiragana_Katakana));
}

bool KeyTranslator::IsKanaAvailable(uint32_t keyval,
                                    uint32_t keycode,
                                    uint32_t modifiers,
                                    bool layout_is_jp,
                                    std::string *out) const {
  if ((modifiers & FcitxKeyState_Ctrl) || (modifiers & FcitxKeyState_Alt)) {
    return false;
  }
  const KanaMap &kana_map = layout_is_jp ? *kKanaJpMap : *kKanaUsMap;

  // When a Japanese keyboard is in use, the yen-sign key and the backslash
  // key generate the same |keyval|. In this case, we have to check |keycode|
  // to return an appropriate string. See the following IBus issue for
  // details: https://github.com/ibus/ibus/issues/73
  // Note the difference(8, evdev offset) of keycode value between ibus/fcitx.
  // IBUS_bar was wrongly used in mozc (it's a keysym value, not key code, so
  // the intention is to compare against 124 (a.k.a 124 + 8 here).
  if (layout_is_jp && keyval == '\\' && keycode == 132) {
    keyval = U'¥';  // U+00A5
  }

  const bool is_shift = (modifiers & FcitxKeyState_Shift);
  const char* kana = GetKanaValue(kana_map, keyval, is_shift);

  if (kana == nullptr) {
    return false;
  }
  if (out) {
    out->assign(kana);
  }
  return true;
}

// TODO(nona): resolve S-'0' problem (b/4338394).
// TODO(nona): Current printable detection is weak. To enhance accuracy, use xkb
// key map
bool KeyTranslator::IsPrintable(uint32_t keyval, uint32_t keycode, uint32_t modifiers) {
  if ((modifiers & FcitxKeyState_Ctrl) || (modifiers & FcitxKeyState_Alt)) {
    return false;
  }
  return IsAscii(keyval, keycode, modifiers);
}

bool KeyTranslator::IsAscii(uint32_t keyval, uint32_t keycode, uint32_t modifiers) {
  return (keyval > FcitxKey_space &&
          // Note: Space key (0x20) is a special key in Mozc.
          keyval <= FcitxKey_asciitilde);  // 0x7e.
}

}  // namespace fcitx
}  // namespace mozc
