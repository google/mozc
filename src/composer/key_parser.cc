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
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/flat_map.h"
#include "base/container/flat_multimap.h"
#include "base/util.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace {

using ::mozc::commands::KeyEvent;

constexpr auto kModifierKeyMap =
    CreateFlatMultimap<absl::string_view, KeyEvent::ModifierKey>({
        {"ctrl", KeyEvent::CTRL},
        {"control", KeyEvent::CTRL},
        {"alt", KeyEvent::ALT},
        {"option", KeyEvent::ALT},
        {"meta", KeyEvent::ALT},
        {"super", KeyEvent::ALT},
        {"hyper", KeyEvent::ALT},
        {"shift", KeyEvent::SHIFT},
        {"caps", KeyEvent::CAPS},
        {"keydown", KeyEvent::KEY_DOWN},
        {"keyup", KeyEvent::KEY_UP},
        {"leftctrl", KeyEvent::CTRL},
        {"leftctrl", KeyEvent::LEFT_CTRL},
        {"rightctrl", KeyEvent::CTRL},
        {"rightctrl", KeyEvent::RIGHT_CTRL},
        {"leftalt", KeyEvent::ALT},
        {"leftalt", KeyEvent::LEFT_ALT},
        {"rightalt", KeyEvent::ALT},
        {"rightalt", KeyEvent::RIGHT_ALT},
        {"leftshift", KeyEvent::SHIFT},
        {"leftshift", KeyEvent::LEFT_SHIFT},
        {"rightshift", KeyEvent::SHIFT},
        {"rightshift", KeyEvent::RIGHT_SHIFT},
    });

constexpr auto kSpecialKeyMap =
    CreateFlatMap<absl::string_view, KeyEvent::SpecialKey>({
        {"on", KeyEvent::ON},
        {"off", KeyEvent::OFF},
        {"left", KeyEvent::LEFT},
        {"down", KeyEvent::DOWN},
        {"up", KeyEvent::UP},
        {"right", KeyEvent::RIGHT},
        {"enter", KeyEvent::ENTER},
        {"return", KeyEvent::ENTER},
        {"esc", KeyEvent::ESCAPE},
        {"escape", KeyEvent::ESCAPE},
        {"delete", KeyEvent::DEL},
        {"del", KeyEvent::DEL},
        {"bs", KeyEvent::BACKSPACE},
        {"backspace", KeyEvent::BACKSPACE},
        {"henkan", KeyEvent::HENKAN},
        {"muhenkan", KeyEvent::MUHENKAN},
        {"kana", KeyEvent::KANA},
        {"hiragana", KeyEvent::KANA},
        {"katakana", KeyEvent::KATAKANA},
        {"eisu", KeyEvent::EISU},
        {"home", KeyEvent::HOME},
        {"end", KeyEvent::END},
        {"space", KeyEvent::SPACE},
        {"ascii", KeyEvent::TEXT_INPUT},  // deprecated
        {"textinput", KeyEvent::TEXT_INPUT},
        {"tab", KeyEvent::TAB},
        {"pageup", KeyEvent::PAGE_UP},
        {"pagedown", KeyEvent::PAGE_DOWN},
        {"insert", KeyEvent::INSERT},
        {"hankaku", KeyEvent::HANKAKU},
        {"zenkaku", KeyEvent::HANKAKU},
        {"hankaku/zenkaku", KeyEvent::HANKAKU},
        {"kanji", KeyEvent::KANJI},

        {"f1", KeyEvent::F1},
        {"f2", KeyEvent::F2},
        {"f3", KeyEvent::F3},
        {"f4", KeyEvent::F4},
        {"f5", KeyEvent::F5},
        {"f6", KeyEvent::F6},
        {"f7", KeyEvent::F7},
        {"f8", KeyEvent::F8},
        {"f9", KeyEvent::F9},
        {"f10", KeyEvent::F10},
        {"f11", KeyEvent::F11},
        {"f12", KeyEvent::F12},
        {"f13", KeyEvent::F13},
        {"f14", KeyEvent::F14},
        {"f15", KeyEvent::F15},
        {"f16", KeyEvent::F16},
        {"f17", KeyEvent::F17},
        {"f18", KeyEvent::F18},
        {"f19", KeyEvent::F19},
        {"f20", KeyEvent::F20},
        {"f21", KeyEvent::F21},
        {"f22", KeyEvent::F22},
        {"f23", KeyEvent::F23},
        {"f24", KeyEvent::F24},

        {"numpad0", KeyEvent::NUMPAD0},
        {"numpad1", KeyEvent::NUMPAD1},
        {"numpad2", KeyEvent::NUMPAD2},
        {"numpad3", KeyEvent::NUMPAD3},
        {"numpad4", KeyEvent::NUMPAD4},
        {"numpad5", KeyEvent::NUMPAD5},
        {"numpad6", KeyEvent::NUMPAD6},
        {"numpad7", KeyEvent::NUMPAD7},
        {"numpad8", KeyEvent::NUMPAD8},
        {"numpad9", KeyEvent::NUMPAD9},

        {"multiply", KeyEvent::MULTIPLY},
        {"add", KeyEvent::ADD},
        {"separator", KeyEvent::SEPARATOR},
        {"subtract", KeyEvent::SUBTRACT},
        {"decimal", KeyEvent::DECIMAL},
        {"divide", KeyEvent::DIVIDE},
        {"equals", KeyEvent::EQUALS},
        {"comma", KeyEvent::COMMA},
        {"clear", KeyEvent::CLEAR},

        {"virtualleft", KeyEvent::VIRTUAL_LEFT},
        {"virtualright", KeyEvent::VIRTUAL_RIGHT},
        {"virtualenter", KeyEvent::VIRTUAL_ENTER},
        {"virtualup", KeyEvent::VIRTUAL_UP},
        {"virtualdown", KeyEvent::VIRTUAL_DOWN},

        // Meant to be used for any other special keys.
        {"undefinedkey", KeyEvent::UNDEFINED_KEY},
    });

}  // namespace

// static
bool KeyParser::ParseKey(const absl::string_view key_string,
                         KeyEvent* key_event) {
  std::vector<std::string> keys =
      absl::StrSplit(key_string, ' ', absl::SkipEmpty());
  if (keys.empty()) {
    LOG(ERROR) << "keys is empty";
    return false;
  }
  return KeyParser::ParseKeyVector(keys, key_event);
}

// static
bool KeyParser::ParseKeyVector(const absl::Span<const std::string> keys,
                               KeyEvent* key_event) {
  CHECK(key_event);

  key_event->Clear();
  absl::btree_set<commands::KeyEvent::ModifierKey> modifiers_set;

  for (const std::string& key : keys) {
    if (Util::CharsLen(key) == 1) {
      if (key_event->has_key_code()) {
        // Multiple keys are not supported.
        return false;
      }
      key_event->set_key_code(Util::Utf8ToCodepoint(key));
      continue;
    }

    std::string lower_key(key);
    Util::LowerString(&lower_key);

    if (absl::Span<const std::pair<absl::string_view, KeyEvent::ModifierKey>>
            modifier_keys = kModifierKeyMap.EqualSpan(lower_key);
        !modifier_keys.empty()) {
      for (auto [_, modifier_key] : modifier_keys) {
        modifiers_set.insert(modifier_key);
      }
      continue;
    }
    if (const KeyEvent::SpecialKey* special_key =
            kSpecialKeyMap.FindOrNull(lower_key);
        special_key != nullptr) {
      if (key_event->has_special_key()) {
        // Multiple special keys are not supported.
        return false;
      }
      key_event->set_special_key(*special_key);
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
