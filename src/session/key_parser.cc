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

// Parser of key events

#include "session/key_parser.h"

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "session/commands.pb.h"
#include "session/internal/keymap_interface.h"

namespace mozc {

namespace {

class KeyParserData {
 public:
  KeyParserData() {
    InitData();
  }

  const map<string, commands::KeyEvent::SpecialKey> &keycode_map() {
    return keycode_map_;
  }
  const map<string, commands::KeyEvent::ModifierKey> &modifiers_map() {
    return modifiers_map_;
  }

 private:
  void InitData() {
    //  CHECK(keymap::KeyType::NUM_KEYTYPES < static_cast<int32>(' '));
    VLOG(1) << "Init KeyParser Data";

    modifiers_map_["ctrl"] = commands::KeyEvent::CTRL;
    modifiers_map_["control"] = commands::KeyEvent::CTRL;
    modifiers_map_["alt"] = commands::KeyEvent::ALT;
    modifiers_map_["option"] = commands::KeyEvent::ALT;
    modifiers_map_["meta"] = commands::KeyEvent::ALT;
    modifiers_map_["super"] = commands::KeyEvent::ALT;
    modifiers_map_["hyper"] = commands::KeyEvent::ALT;
    modifiers_map_["shift"] = commands::KeyEvent::SHIFT;
    modifiers_map_["caps"] = commands::KeyEvent::CAPS;
    modifiers_map_["keydown"] = commands::KeyEvent::KEY_DOWN;
    modifiers_map_["keyup"] = commands::KeyEvent::KEY_UP;

    keycode_map_["on"] = commands::KeyEvent::ON;
    keycode_map_["off"] = commands::KeyEvent::OFF;
    keycode_map_["left"] = commands::KeyEvent::LEFT;
    keycode_map_["down"] = commands::KeyEvent::DOWN;
    keycode_map_["up"] = commands::KeyEvent::UP;
    keycode_map_["right"] = commands::KeyEvent::RIGHT;
    keycode_map_["enter"] = commands::KeyEvent::ENTER;
    keycode_map_["return"] = commands::KeyEvent::ENTER;
    keycode_map_["esc"] = commands::KeyEvent::ESCAPE;
    keycode_map_["escape"] = commands::KeyEvent::ESCAPE;
    keycode_map_["delete"] = commands::KeyEvent::DEL;
    keycode_map_["del"] = commands::KeyEvent::DEL;
    keycode_map_["bs"] = commands::KeyEvent::BACKSPACE;
    keycode_map_["backspace"] = commands::KeyEvent::BACKSPACE;
    keycode_map_["henkan"] = commands::KeyEvent::HENKAN;
    keycode_map_["muhenkan"] = commands::KeyEvent::MUHENKAN;
    keycode_map_["kana"] = commands::KeyEvent::KANA;
    keycode_map_["hiragana"] = commands::KeyEvent::KANA;
    keycode_map_["katakana"] = commands::KeyEvent::KATAKANA;
    keycode_map_["eisu"] = commands::KeyEvent::EISU;
    keycode_map_["home"] = commands::KeyEvent::HOME;
    keycode_map_["end"] = commands::KeyEvent::END;
    keycode_map_["space"] = commands::KeyEvent::SPACE;
    keycode_map_["ascii"] = commands::KeyEvent::ASCII;
    keycode_map_["tab"] = commands::KeyEvent::TAB;
    keycode_map_["pageup"] = commands::KeyEvent::PAGE_UP;
    keycode_map_["pagedown"] = commands::KeyEvent::PAGE_DOWN;
    keycode_map_["insert"] = commands::KeyEvent::INSERT;
    keycode_map_["hankaku"] = commands::KeyEvent::HANKAKU;
    keycode_map_["zenkaku"] = commands::KeyEvent::HANKAKU;
    keycode_map_["hankaku/zenkaku"] = commands::KeyEvent::HANKAKU;
    keycode_map_["kanji"] = commands::KeyEvent::KANJI;

    keycode_map_["f1"] = commands::KeyEvent::F1;
    keycode_map_["f2"] = commands::KeyEvent::F2;
    keycode_map_["f3"] = commands::KeyEvent::F3;
    keycode_map_["f4"] = commands::KeyEvent::F4;
    keycode_map_["f5"] = commands::KeyEvent::F5;
    keycode_map_["f6"] = commands::KeyEvent::F6;
    keycode_map_["f7"] = commands::KeyEvent::F7;
    keycode_map_["f8"] = commands::KeyEvent::F8;
    keycode_map_["f9"] = commands::KeyEvent::F9;
    keycode_map_["f10"] = commands::KeyEvent::F10;
    keycode_map_["f11"] = commands::KeyEvent::F11;
    keycode_map_["f12"] = commands::KeyEvent::F12;
    keycode_map_["f13"] = commands::KeyEvent::F13;
    keycode_map_["f14"] = commands::KeyEvent::F14;
    keycode_map_["f15"] = commands::KeyEvent::F15;
    keycode_map_["f16"] = commands::KeyEvent::F16;
    keycode_map_["f17"] = commands::KeyEvent::F17;
    keycode_map_["f18"] = commands::KeyEvent::F18;
    keycode_map_["f19"] = commands::KeyEvent::F19;
    keycode_map_["f20"] = commands::KeyEvent::F20;
    keycode_map_["f21"] = commands::KeyEvent::F21;
    keycode_map_["f22"] = commands::KeyEvent::F22;
    keycode_map_["f23"] = commands::KeyEvent::F23;
    keycode_map_["f24"] = commands::KeyEvent::F24;

    keycode_map_["numpad0"] = commands::KeyEvent::NUMPAD0;
    keycode_map_["numpad1"] = commands::KeyEvent::NUMPAD1;
    keycode_map_["numpad2"] = commands::KeyEvent::NUMPAD2;
    keycode_map_["numpad3"] = commands::KeyEvent::NUMPAD3;
    keycode_map_["numpad4"] = commands::KeyEvent::NUMPAD4;
    keycode_map_["numpad5"] = commands::KeyEvent::NUMPAD5;
    keycode_map_["numpad6"] = commands::KeyEvent::NUMPAD6;
    keycode_map_["numpad7"] = commands::KeyEvent::NUMPAD7;
    keycode_map_["numpad8"] = commands::KeyEvent::NUMPAD8;
    keycode_map_["numpad9"] = commands::KeyEvent::NUMPAD9;

    keycode_map_["multiply"] = commands::KeyEvent::MULTIPLY;
    keycode_map_["add"] = commands::KeyEvent::ADD;
    keycode_map_["separator"] = commands::KeyEvent::SEPARATOR;
    keycode_map_["subtract"] = commands::KeyEvent::SUBTRACT;
    keycode_map_["decimal"] = commands::KeyEvent::DECIMAL;
    keycode_map_["divide"] = commands::KeyEvent::DIVIDE;
    keycode_map_["equals"] = commands::KeyEvent::EQUALS;
  }

  map<string, commands::KeyEvent::SpecialKey> keycode_map_;
  map<string, commands::KeyEvent::ModifierKey> modifiers_map_;
};
}  // namespace

bool KeyParser::ParseKey(const string &key_string,
                         commands::KeyEvent *key_event) {
  if (Util::GetFormType(key_string) != mozc::Util::HALF_WIDTH) {
    LOG(ERROR) << "key should be half-width";
    return false;
  }
  vector<string> keys;
  Util::SplitStringUsing(key_string, " ", &keys);
  if (keys.empty()) {
    LOG(ERROR) << "keys is empty";
    return false;
  }
  return KeyParser::ParseKeyVector(keys, key_event);
}

bool KeyParser::ParseKeyVector(const vector<string> &keys,
                               commands::KeyEvent *key_event) {
  CHECK(key_event);

  const map<string, commands::KeyEvent::ModifierKey> &modifiers_map =
    Singleton<KeyParserData>::get()->modifiers_map();
  const map<string, commands::KeyEvent::SpecialKey> &keycode_map =
    Singleton<KeyParserData>::get()->keycode_map();

  key_event->Clear();

  for (size_t i = 0; i < keys.size(); ++i) {
    if (Util::CharsLen(keys[i]) == 1) {
      const char *begin = keys[i].c_str();
      size_t mblen = 0;
      uint16 key_code = Util::UTF8ToUCS2(begin, begin + keys[i].size(), &mblen);
      key_event->set_key_code(key_code);
    } else {
      string key = keys[i];
      Util::LowerString(&key);
      if (modifiers_map.count(key) == 1) {
        key_event->add_modifier_keys(modifiers_map.find(key)->second);
      } else if (keycode_map.count(key) == 1) {
        key_event->set_special_key(keycode_map.find(key)->second);
      } else {
        LOG(ERROR) << "Unknown key: " << keys[i];
        return false;
      }
    }
  }
  return true;
}

}  // namespace mozc
