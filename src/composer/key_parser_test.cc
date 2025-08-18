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

#include "composer/key_parser.h"

#include <cstdint>
#include <utility>

#include "absl/strings/string_view.h"
#include "composer/key_event_util.h"
#include "protocol/commands.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

using ::mozc::commands::KeyEvent;

TEST(KeyParserTest, KeyCode) {
  KeyEvent key_event;

  EXPECT_TRUE(KeyParser::ParseKey("a", &key_event));
  EXPECT_EQ(key_event.key_code(), 'a');

  key_event.Clear();
  EXPECT_TRUE(KeyParser::ParseKey("A", &key_event));
  EXPECT_EQ(key_event.key_code(), 'A');

  // "あ" (not half width)
  constexpr char32_t kHiraganaA = 0x3042;
  key_event.Clear();
  EXPECT_TRUE(KeyParser::ParseKey("あ", &key_event));
  EXPECT_EQ(key_event.key_code(), kHiraganaA);
}

TEST(KeyParserTest, ModifierKeys) {
  constexpr std::pair<absl::string_view, uint32_t> kTestData[] = {
      {"ctrl", KeyEvent::CTRL},
      {"leftctrl", KeyEvent::CTRL | KeyEvent::LEFT_CTRL},
      {"rightctrl", KeyEvent::CTRL | KeyEvent::RIGHT_CTRL},
      {"alt", KeyEvent::ALT},
      {"leftalt", KeyEvent::ALT | KeyEvent::LEFT_ALT},
      {"rightalt", KeyEvent::ALT | KeyEvent::RIGHT_ALT},
      {"shift", KeyEvent::SHIFT},
      {"leftshift", KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT},
      {"rightshift", KeyEvent::SHIFT | KeyEvent::RIGHT_SHIFT},

      {"caps", KeyEvent::CAPS},
      {"keydown", KeyEvent::KEY_DOWN},
      {"keyup", KeyEvent::KEY_UP},

      {"SHIFT", KeyEvent::SHIFT},
  };

  for (const auto& [name, modifiers] : kTestData) {
    SCOPED_TRACE(name);
    KeyEvent key_event;
    EXPECT_TRUE(KeyParser::ParseKey(name, &key_event));
    EXPECT_EQ(KeyEventUtil::GetModifiers(key_event), modifiers);
  }
}

TEST(KeyParserTest, MultipleModifierKeys) {
  KeyEvent key_event;
  EXPECT_TRUE(KeyParser::ParseKey("LeftCtrl RightCtrl", &key_event));
  EXPECT_EQ(key_event.modifier_keys_size(), 3);
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event),
            KeyEvent::CTRL | KeyEvent::LEFT_CTRL | KeyEvent::RIGHT_CTRL);
}

TEST(KeyParserTest, SpecialKeys) {
  constexpr std::pair<absl::string_view, KeyEvent::SpecialKey> kTestData[] = {
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

      {"on", KeyEvent::ON},
  };

  for (const auto& [name, modifiers] : kTestData) {
    SCOPED_TRACE(name);
    KeyEvent key_event;
    EXPECT_TRUE(KeyParser::ParseKey(name, &key_event));
    EXPECT_EQ(key_event.special_key(), modifiers);
  }
}

TEST(KeyParserTest, Combination) {
  KeyEvent key_event;

  EXPECT_TRUE(KeyParser::ParseKey("LeftShift CTRL a", &key_event));
  EXPECT_EQ(key_event.key_code(), 'a');
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event),
            KeyEvent::LEFT_SHIFT | KeyEvent::SHIFT | KeyEvent::CTRL);

  EXPECT_TRUE(KeyParser::ParseKey("rightalt On", &key_event));
  EXPECT_EQ(key_event.special_key(), KeyEvent::ON);
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event),
            KeyEvent::RIGHT_ALT | KeyEvent::ALT);

  EXPECT_TRUE(KeyParser::ParseKey("SHIFT on a", &key_event));
  EXPECT_EQ(key_event.key_code(), 'a');
  EXPECT_EQ(key_event.special_key(), KeyEvent::ON);
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event), KeyEvent::SHIFT);

  EXPECT_TRUE(KeyParser::ParseKey("alt a", &key_event));
  EXPECT_EQ(key_event.key_code(), 'a');
  EXPECT_FALSE(key_event.has_special_key());
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event), KeyEvent::ALT);

  // meta and hyper are identical to alt.
  EXPECT_TRUE(KeyParser::ParseKey("a meta hyper", &key_event));
  EXPECT_EQ(key_event.key_code(), 'a');
  EXPECT_FALSE(key_event.has_special_key());
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event), KeyEvent::ALT);

  // multiple keys are not supported.
  EXPECT_FALSE(KeyParser::ParseKey("a alt z", &key_event));

  // multiple special keys are not supported.
  EXPECT_FALSE(KeyParser::ParseKey("muhenkan backspace", &key_event));
}

TEST(KeyParserTest, GetSpecialKeyString) {
  // Strings are defined in gui/config_dialog/keybinding_editor.cc
  EXPECT_EQ(KeyEvent::NUM_SPECIALKEYS, 77);
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::ON), "on");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::OFF), "off");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::LEFT), "left");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::DOWN), "down");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::UP), "up");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::RIGHT), "right");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::ENTER), "enter");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::ESCAPE), "escape");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::DEL), "delete");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::BACKSPACE), "backspace");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::HENKAN), "henkan");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::MUHENKAN), "muhenkan");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::KANA), "hiragana");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::KATAKANA), "katakana");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::EISU), "eisu");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::HOME), "home");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::END), "end");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::SPACE), "space");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::TEXT_INPUT), "textinput");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::TAB), "tab");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::PAGE_UP), "pageup");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::PAGE_DOWN), "pagedown");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::INSERT), "insert");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::HANKAKU),
            "hankaku/zenkaku");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::KANJI), "kanji");

  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F1), "f1");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F2), "f2");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F3), "f3");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F4), "f4");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F5), "f5");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F6), "f6");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F7), "f7");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F8), "f8");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F9), "f9");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F10), "f10");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F11), "f11");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F12), "f12");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F13), "f13");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F14), "f14");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F15), "f15");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F16), "f16");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F17), "f17");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F18), "f18");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F19), "f19");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F20), "f20");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F21), "f21");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F22), "f22");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F23), "f23");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::F24), "f24");

  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::NUMPAD0), "numpad0");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::NUMPAD1), "numpad1");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::NUMPAD2), "numpad2");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::NUMPAD3), "numpad3");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::NUMPAD4), "numpad4");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::NUMPAD5), "numpad5");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::NUMPAD6), "numpad6");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::NUMPAD7), "numpad7");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::NUMPAD8), "numpad8");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::NUMPAD9), "numpad9");

  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::MULTIPLY), "multiply");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::ADD), "add");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::SEPARATOR), "separator");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::SUBTRACT), "subtract");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::DECIMAL), "decimal");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::DIVIDE), "divide");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::EQUALS), "equals");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::COMMA), "comma");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::CLEAR), "clear");

  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::VIRTUAL_LEFT),
            "virtualleft");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::VIRTUAL_RIGHT),
            "virtualright");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::VIRTUAL_ENTER),
            "virtualenter");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::VIRTUAL_UP), "virtualup");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::VIRTUAL_DOWN),
            "virtualdown");
  EXPECT_EQ(KeyParser::GetSpecialKeyString(KeyEvent::UNDEFINED_KEY),
            "undefinedkey");
}

}  // namespace
}  // namespace mozc
