// Copyright 2010-2020, Google Inc.
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

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/util.h"
#include "protocol/commands.pb.h"

namespace mozc {
using commands::KeyEvent;

namespace {

typedef std::map<std::string, KeyEvent::SpecialKey> SpecialKeysMap;
typedef std::multimap<std::string, KeyEvent::ModifierKey> ModifiersMap;

class KeyParserData {
 public:
  KeyParserData() { InitData(); }

  const SpecialKeysMap &keycode_map() { return keycode_map_; }
  const ModifiersMap &modifiers_map() { return modifiers_map_; }

 private:
  void InitData() {
    //  CHECK(keymap::KeyType::NUM_KEYTYPES < static_cast<int32>(' '));
    VLOG(1) << "Init KeyParser Data";

    modifiers_map_.insert(std::make_pair("ctrl", KeyEvent::CTRL));
    modifiers_map_.insert(std::make_pair("control", KeyEvent::CTRL));
    modifiers_map_.insert(std::make_pair("alt", KeyEvent::ALT));
    modifiers_map_.insert(std::make_pair("option", KeyEvent::ALT));
    modifiers_map_.insert(std::make_pair("meta", KeyEvent::ALT));
    modifiers_map_.insert(std::make_pair("super", KeyEvent::ALT));
    modifiers_map_.insert(std::make_pair("hyper", KeyEvent::ALT));
    modifiers_map_.insert(std::make_pair("shift", KeyEvent::SHIFT));
    modifiers_map_.insert(std::make_pair("caps", KeyEvent::CAPS));
    modifiers_map_.insert(std::make_pair("keydown", KeyEvent::KEY_DOWN));
    modifiers_map_.insert(std::make_pair("keyup", KeyEvent::KEY_UP));

    modifiers_map_.insert(std::make_pair("leftctrl", KeyEvent::CTRL));
    modifiers_map_.insert(std::make_pair("leftctrl", KeyEvent::LEFT_CTRL));
    modifiers_map_.insert(std::make_pair("rightctrl", KeyEvent::CTRL));
    modifiers_map_.insert(std::make_pair("rightctrl", KeyEvent::RIGHT_CTRL));
    modifiers_map_.insert(std::make_pair("leftalt", KeyEvent::ALT));
    modifiers_map_.insert(std::make_pair("leftalt", KeyEvent::LEFT_ALT));
    modifiers_map_.insert(std::make_pair("rightalt", KeyEvent::ALT));
    modifiers_map_.insert(std::make_pair("rightalt", KeyEvent::RIGHT_ALT));
    modifiers_map_.insert(std::make_pair("leftshift", KeyEvent::SHIFT));
    modifiers_map_.insert(std::make_pair("leftshift", KeyEvent::LEFT_SHIFT));
    modifiers_map_.insert(std::make_pair("rightshift", KeyEvent::SHIFT));
    modifiers_map_.insert(std::make_pair("rightshift", KeyEvent::RIGHT_SHIFT));

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

bool KeyParser::ParseKey(const std::string &key_string, KeyEvent *key_event) {
  std::vector<std::string> keys;
  Util::SplitStringUsing(key_string, " ", &keys);
  if (keys.empty()) {
    LOG(ERROR) << "keys is empty";
    return false;
  }
  return KeyParser::ParseKeyVector(keys, key_event);
}

bool KeyParser::ParseKeyVector(const std::vector<std::string> &keys,
                               KeyEvent *key_event) {
  CHECK(key_event);

  const ModifiersMap &modifiers_map =
      Singleton<KeyParserData>::get()->modifiers_map();
  const SpecialKeysMap &keycode_map =
      Singleton<KeyParserData>::get()->keycode_map();

  key_event->Clear();
  std::set<commands::KeyEvent::ModifierKey> modifiers_set;

  for (size_t i = 0; i < keys.size(); ++i) {
    if (Util::CharsLen(keys[i]) == 1) {
      char32 key_code = 0;
      if (Util::SplitFirstChar32(keys[i], &key_code, nullptr)) {
        key_event->set_key_code(key_code);
      }
    } else {
      std::string key = keys[i];
      Util::LowerString(&key);

      if (modifiers_map.count(key) > 0) {
        std::pair<ModifiersMap::const_iterator, ModifiersMap::const_iterator>
            range = modifiers_map.equal_range(key);
        for (ModifiersMap::const_iterator iter = range.first;
             iter != range.second; ++iter) {
          modifiers_set.insert(iter->second);
        }
      } else if (keycode_map.count(key) == 1) {
        key_event->set_special_key(keycode_map.find(key)->second);
      } else {
        LOG(ERROR) << "Unknown key: " << keys[i];
        return false;
      }
    }
  }

  for (std::set<commands::KeyEvent::ModifierKey>::iterator iter =
           modifiers_set.begin();
       iter != modifiers_set.end(); ++iter) {
    key_event->add_modifier_keys(*iter);
  }

  return true;
}

}  // namespace mozc
