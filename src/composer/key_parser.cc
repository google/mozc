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

// Parser of key events

#include "composer/key_parser.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "protocol/commands.pb.h"
#include "absl/strings/str_split.h"

namespace mozc {
using commands::KeyEvent;

namespace {

typedef std::map<std::string, KeyEvent::SpecialKey> SpecialKeysMap;
typedef std::map<std::string, std::vector<KeyEvent::ModifierKey>> ModifiersMap;

class KeyParserData {
 public:
  KeyParserData() { InitData(); }

  const SpecialKeysMap &keycode_map() { return keycode_map_; }
  const ModifiersMap &modifiers_map() { return modifiers_map_; }

 private:
  void InitData() {
    //  CHECK(keymap::KeyType::NUM_KEYTYPES < static_cast<int32_t>(' '));
    VLOG(1) << "Init KeyParser Data";

    modifiers_map_["ctrl"] = {KeyEvent::CTRL};
    modifiers_map_["control"] = {KeyEvent::CTRL};
    modifiers_map_["alt"] = {KeyEvent::ALT};
    modifiers_map_["option"] = {KeyEvent::ALT};
    modifiers_map_["meta"] = {KeyEvent::ALT};
    modifiers_map_["super"] = {KeyEvent::ALT};
    modifiers_map_["hyper"] = {KeyEvent::ALT};
    modifiers_map_["shift"] = {KeyEvent::SHIFT};
    modifiers_map_["caps"] = {KeyEvent::CAPS};
    modifiers_map_["keydown"] = {KeyEvent::KEY_DOWN};
    modifiers_map_["keyup"] = {KeyEvent::KEY_UP};

    modifiers_map_["leftctrl"] = {KeyEvent::CTRL, KeyEvent::LEFT_CTRL};
    modifiers_map_["rightctrl"] = {KeyEvent::CTRL, KeyEvent::RIGHT_CTRL};
    modifiers_map_["leftalt"] = {KeyEvent::ALT, KeyEvent::LEFT_ALT};
    modifiers_map_["rightalt"] = {KeyEvent::ALT, KeyEvent::RIGHT_ALT};
    modifiers_map_["leftshift"] = {KeyEvent::SHIFT, KeyEvent::LEFT_SHIFT};
    modifiers_map_["rightshift"] = {KeyEvent::SHIFT, KeyEvent::RIGHT_SHIFT};

    keycode_map_["on"] = KeyEvent::ON;
    keycode_map_["off"] = KeyEvent::OFF;
    keycode_map_["left"] = KeyEvent::LEFT;
    keycode_map_["down"] = KeyEvent::DOWN;
    keycode_map_["up"] = KeyEvent::UP;
    keycode_map_["right"] = KeyEvent::RIGHT;
    keycode_map_["enter"] = KeyEvent::ENTER;
    keycode_map_["return"] = KeyEvent::ENTER;
    keycode_map_["esc"] = KeyEvent::ESCAPE;
    keycode_map_["escape"] = KeyEvent::ESCAPE;
    keycode_map_["delete"] = KeyEvent::DEL;
    keycode_map_["del"] = KeyEvent::DEL;
    keycode_map_["bs"] = KeyEvent::BACKSPACE;
    keycode_map_["backspace"] = KeyEvent::BACKSPACE;
    keycode_map_["henkan"] = KeyEvent::HENKAN;
    keycode_map_["muhenkan"] = KeyEvent::MUHENKAN;
    keycode_map_["kana"] = KeyEvent::KANA;
    keycode_map_["hiragana"] = KeyEvent::KANA;
    keycode_map_["katakana"] = KeyEvent::KATAKANA;
    keycode_map_["eisu"] = KeyEvent::EISU;
    keycode_map_["home"] = KeyEvent::HOME;
    keycode_map_["end"] = KeyEvent::END;
    keycode_map_["space"] = KeyEvent::SPACE;
    keycode_map_["ascii"] = KeyEvent::TEXT_INPUT;  // deprecated
    keycode_map_["textinput"] = KeyEvent::TEXT_INPUT;
    keycode_map_["tab"] = KeyEvent::TAB;
    keycode_map_["pageup"] = KeyEvent::PAGE_UP;
    keycode_map_["pagedown"] = KeyEvent::PAGE_DOWN;
    keycode_map_["insert"] = KeyEvent::INSERT;
    keycode_map_["hankaku"] = KeyEvent::HANKAKU;
    keycode_map_["zenkaku"] = KeyEvent::HANKAKU;
    keycode_map_["hankaku/zenkaku"] = KeyEvent::HANKAKU;
    keycode_map_["kanji"] = KeyEvent::KANJI;

    keycode_map_["f1"] = KeyEvent::F1;
    keycode_map_["f2"] = KeyEvent::F2;
    keycode_map_["f3"] = KeyEvent::F3;
    keycode_map_["f4"] = KeyEvent::F4;
    keycode_map_["f5"] = KeyEvent::F5;
    keycode_map_["f6"] = KeyEvent::F6;
    keycode_map_["f7"] = KeyEvent::F7;
    keycode_map_["f8"] = KeyEvent::F8;
    keycode_map_["f9"] = KeyEvent::F9;
    keycode_map_["f10"] = KeyEvent::F10;
    keycode_map_["f11"] = KeyEvent::F11;
    keycode_map_["f12"] = KeyEvent::F12;
    keycode_map_["f13"] = KeyEvent::F13;
    keycode_map_["f14"] = KeyEvent::F14;
    keycode_map_["f15"] = KeyEvent::F15;
    keycode_map_["f16"] = KeyEvent::F16;
    keycode_map_["f17"] = KeyEvent::F17;
    keycode_map_["f18"] = KeyEvent::F18;
    keycode_map_["f19"] = KeyEvent::F19;
    keycode_map_["f20"] = KeyEvent::F20;
    keycode_map_["f21"] = KeyEvent::F21;
    keycode_map_["f22"] = KeyEvent::F22;
    keycode_map_["f23"] = KeyEvent::F23;
    keycode_map_["f24"] = KeyEvent::F24;

    keycode_map_["numpad0"] = KeyEvent::NUMPAD0;
    keycode_map_["numpad1"] = KeyEvent::NUMPAD1;
    keycode_map_["numpad2"] = KeyEvent::NUMPAD2;
    keycode_map_["numpad3"] = KeyEvent::NUMPAD3;
    keycode_map_["numpad4"] = KeyEvent::NUMPAD4;
    keycode_map_["numpad5"] = KeyEvent::NUMPAD5;
    keycode_map_["numpad6"] = KeyEvent::NUMPAD6;
    keycode_map_["numpad7"] = KeyEvent::NUMPAD7;
    keycode_map_["numpad8"] = KeyEvent::NUMPAD8;
    keycode_map_["numpad9"] = KeyEvent::NUMPAD9;

    keycode_map_["multiply"] = KeyEvent::MULTIPLY;
    keycode_map_["add"] = KeyEvent::ADD;
    keycode_map_["separator"] = KeyEvent::SEPARATOR;
    keycode_map_["subtract"] = KeyEvent::SUBTRACT;
    keycode_map_["decimal"] = KeyEvent::DECIMAL;
    keycode_map_["divide"] = KeyEvent::DIVIDE;
    keycode_map_["equals"] = KeyEvent::EQUALS;
    keycode_map_["comma"] = KeyEvent::COMMA;
    keycode_map_["clear"] = KeyEvent::CLEAR;

    keycode_map_["virtualleft"] = KeyEvent::VIRTUAL_LEFT;
    keycode_map_["virtualright"] = KeyEvent::VIRTUAL_RIGHT;
    keycode_map_["virtualenter"] = KeyEvent::VIRTUAL_ENTER;
    keycode_map_["virtualup"] = KeyEvent::VIRTUAL_UP;
    keycode_map_["virtualdown"] = KeyEvent::VIRTUAL_DOWN;

    // Meant to be used for any other special keys.
    keycode_map_["undefinedkey"] = KeyEvent::UNDEFINED_KEY;
  }

  SpecialKeysMap keycode_map_;
  ModifiersMap modifiers_map_;
};
}  // namespace

// static
bool KeyParser::ParseKey(const std::string &key_string, KeyEvent *key_event) {
  std::vector<std::string> keys =
      absl::StrSplit(key_string, ' ', absl::SkipEmpty());
  if (keys.empty()) {
    LOG(ERROR) << "keys is empty";
    return false;
  }
  return KeyParser::ParseKeyVector(keys, key_event);
}

// static
bool KeyParser::ParseKeyVector(const std::vector<std::string> &keys,
                               KeyEvent *key_event) {
  CHECK(key_event);
  static KeyParserData *parser_data = new KeyParserData();
  const ModifiersMap &modifiers = parser_data->modifiers_map();
  const SpecialKeysMap &specials = parser_data->keycode_map();

  key_event->Clear();
  std::set<commands::KeyEvent::ModifierKey> modifiers_set;

  for (const std::string &key : keys) {
    if (Util::CharsLen(key) == 1) {
      if (key_event->has_key_code()) {
        // Multiple keys are not supported.
        return false;
      }
      key_event->set_key_code(Util::Utf8ToUcs4(key));
      continue;
    }

    std::string lower_key = key;
    Util::LowerString(&lower_key);

    if (const auto &it = modifiers.find(lower_key); it != modifiers.end()) {
      modifiers_set.insert(it->second.begin(), it->second.end());
      continue;
    }
    if (const auto &it = specials.find(lower_key); it != specials.end()) {
      if (key_event->has_special_key()) {
        // Multiple special keys are not supported.
        return false;
      }
      key_event->set_special_key(it->second);
      continue;
    }
    LOG(ERROR) << "Unknown key: " << key;
    return false;
  }

  for (const commands::KeyEvent::ModifierKey modifier : modifiers_set) {
    key_event->add_modifier_keys(modifier);
  }

  return true;
}

std::string KeyParser::GetSpecialKeyString(KeyEvent::SpecialKey key) {
  // Handles exceptional rules that cannot convert from the enum value.
  switch (key) {
    case KeyEvent::DEL:
      return "delete";
    case KeyEvent::KANA:
      return "hiragana";
    case KeyEvent::HANKAKU:
      return "hankaku/zenkaku";
    default:
      // Do nothing.
      break;
  }

  // Transforms the enum value in string by removing '_' and using lower case.
  // e.g. "PAGE_UP" -> "pageup".
  std::string name = KeyEvent::SpecialKey_Name(key);
  // Idiom to remove '_' from `name` (e.g. "PAGE_UP" -> "PAGEUP").
  name.erase(std::remove(name.begin(), name.end(), '_'), name.end());
  Util::LowerString(&name);
  return name;
}

}  // namespace mozc
