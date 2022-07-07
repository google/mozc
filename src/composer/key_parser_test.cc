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
#include <iterator>
#include <string>
#include <utility>

#include "base/port.h"
#include "composer/key_event_util.h"
#include "protocol/commands.pb.h"
#include "testing/base/public/gunit.h"

namespace mozc {

TEST(KeyParserTest, KeyCode) {
  commands::KeyEvent key_event;

  EXPECT_TRUE(KeyParser::ParseKey("a", &key_event));
  EXPECT_EQ('a', key_event.key_code());

  key_event.Clear();
  EXPECT_TRUE(KeyParser::ParseKey("A", &key_event));
  EXPECT_EQ('A', key_event.key_code());

  // "あ" (not half width)
  constexpr char32 kHiraganaA = 0x3042;
  key_event.Clear();
  EXPECT_TRUE(KeyParser::ParseKey("あ", &key_event));
  EXPECT_EQ(kHiraganaA, key_event.key_code());
}

TEST(KeyParserTest, ModifierKeys) {
  const std::pair<std::string, uint32_t> kTestData[] = {
      std::make_pair("ctrl", commands::KeyEvent::CTRL),
      std::make_pair("leftctrl",
                     commands::KeyEvent::CTRL | commands::KeyEvent::LEFT_CTRL),
      std::make_pair("rightctrl",
                     commands::KeyEvent::CTRL | commands::KeyEvent::RIGHT_CTRL),

      std::make_pair("alt", commands::KeyEvent::ALT),
      std::make_pair("leftalt",
                     commands::KeyEvent::ALT | commands::KeyEvent::LEFT_ALT),
      std::make_pair("rightalt",
                     commands::KeyEvent::ALT | commands::KeyEvent::RIGHT_ALT),

      std::make_pair("shift", commands::KeyEvent::SHIFT),
      std::make_pair("leftshift", commands::KeyEvent::SHIFT |
                                      commands::KeyEvent::LEFT_SHIFT),
      std::make_pair("rightshift", commands::KeyEvent::SHIFT |
                                       commands::KeyEvent::RIGHT_SHIFT),

      std::make_pair("caps", commands::KeyEvent::CAPS),
      std::make_pair("keydown", commands::KeyEvent::KEY_DOWN),
      std::make_pair("keyup", commands::KeyEvent::KEY_UP),

      std::make_pair("SHIFT", commands::KeyEvent::SHIFT),
  };

  for (size_t i = 0; i < std::size(kTestData); ++i) {
    SCOPED_TRACE(kTestData[i].first);
    commands::KeyEvent key_event;
    EXPECT_TRUE(KeyParser::ParseKey(kTestData[i].first, &key_event));
    EXPECT_EQ(kTestData[i].second, KeyEventUtil::GetModifiers(key_event));
  }
}

TEST(KeyParserTest, MultipleModifierKeys) {
  commands::KeyEvent key_event;
  EXPECT_TRUE(KeyParser::ParseKey("LeftCtrl RightCtrl", &key_event));
  EXPECT_EQ(3, key_event.modifier_keys_size());
  EXPECT_EQ((commands::KeyEvent::CTRL | commands::KeyEvent::LEFT_CTRL |
             commands::KeyEvent::RIGHT_CTRL),
            KeyEventUtil::GetModifiers(key_event));
}

TEST(KeyParserTest, SpecialKeys) {
  const std::pair<std::string, commands::KeyEvent::SpecialKey> kTestData[] = {
      std::make_pair("on", commands::KeyEvent::ON),
      std::make_pair("off", commands::KeyEvent::OFF),
      std::make_pair("left", commands::KeyEvent::LEFT),
      std::make_pair("down", commands::KeyEvent::DOWN),
      std::make_pair("up", commands::KeyEvent::UP),
      std::make_pair("right", commands::KeyEvent::RIGHT),
      std::make_pair("enter", commands::KeyEvent::ENTER),
      std::make_pair("return", commands::KeyEvent::ENTER),
      std::make_pair("esc", commands::KeyEvent::ESCAPE),
      std::make_pair("escape", commands::KeyEvent::ESCAPE),
      std::make_pair("delete", commands::KeyEvent::DEL),
      std::make_pair("del", commands::KeyEvent::DEL),
      std::make_pair("bs", commands::KeyEvent::BACKSPACE),
      std::make_pair("backspace", commands::KeyEvent::BACKSPACE),
      std::make_pair("henkan", commands::KeyEvent::HENKAN),
      std::make_pair("muhenkan", commands::KeyEvent::MUHENKAN),
      std::make_pair("kana", commands::KeyEvent::KANA),
      std::make_pair("hiragana", commands::KeyEvent::KANA),
      std::make_pair("katakana", commands::KeyEvent::KATAKANA),
      std::make_pair("eisu", commands::KeyEvent::EISU),
      std::make_pair("home", commands::KeyEvent::HOME),
      std::make_pair("end", commands::KeyEvent::END),
      std::make_pair("space", commands::KeyEvent::SPACE),
      std::make_pair("ascii", commands::KeyEvent::TEXT_INPUT),  // deprecated
      std::make_pair("textinput", commands::KeyEvent::TEXT_INPUT),
      std::make_pair("tab", commands::KeyEvent::TAB),
      std::make_pair("pageup", commands::KeyEvent::PAGE_UP),
      std::make_pair("pagedown", commands::KeyEvent::PAGE_DOWN),
      std::make_pair("insert", commands::KeyEvent::INSERT),
      std::make_pair("hankaku", commands::KeyEvent::HANKAKU),
      std::make_pair("zenkaku", commands::KeyEvent::HANKAKU),
      std::make_pair("hankaku/zenkaku", commands::KeyEvent::HANKAKU),
      std::make_pair("kanji", commands::KeyEvent::KANJI),

      std::make_pair("f1", commands::KeyEvent::F1),
      std::make_pair("f2", commands::KeyEvent::F2),
      std::make_pair("f3", commands::KeyEvent::F3),
      std::make_pair("f4", commands::KeyEvent::F4),
      std::make_pair("f5", commands::KeyEvent::F5),
      std::make_pair("f6", commands::KeyEvent::F6),
      std::make_pair("f7", commands::KeyEvent::F7),
      std::make_pair("f8", commands::KeyEvent::F8),
      std::make_pair("f9", commands::KeyEvent::F9),
      std::make_pair("f10", commands::KeyEvent::F10),
      std::make_pair("f11", commands::KeyEvent::F11),
      std::make_pair("f12", commands::KeyEvent::F12),
      std::make_pair("f13", commands::KeyEvent::F13),
      std::make_pair("f14", commands::KeyEvent::F14),
      std::make_pair("f15", commands::KeyEvent::F15),
      std::make_pair("f16", commands::KeyEvent::F16),
      std::make_pair("f17", commands::KeyEvent::F17),
      std::make_pair("f18", commands::KeyEvent::F18),
      std::make_pair("f19", commands::KeyEvent::F19),
      std::make_pair("f20", commands::KeyEvent::F20),
      std::make_pair("f21", commands::KeyEvent::F21),
      std::make_pair("f22", commands::KeyEvent::F22),
      std::make_pair("f23", commands::KeyEvent::F23),
      std::make_pair("f24", commands::KeyEvent::F24),

      std::make_pair("numpad0", commands::KeyEvent::NUMPAD0),
      std::make_pair("numpad1", commands::KeyEvent::NUMPAD1),
      std::make_pair("numpad2", commands::KeyEvent::NUMPAD2),
      std::make_pair("numpad3", commands::KeyEvent::NUMPAD3),
      std::make_pair("numpad4", commands::KeyEvent::NUMPAD4),
      std::make_pair("numpad5", commands::KeyEvent::NUMPAD5),
      std::make_pair("numpad6", commands::KeyEvent::NUMPAD6),
      std::make_pair("numpad7", commands::KeyEvent::NUMPAD7),
      std::make_pair("numpad8", commands::KeyEvent::NUMPAD8),
      std::make_pair("numpad9", commands::KeyEvent::NUMPAD9),

      std::make_pair("multiply", commands::KeyEvent::MULTIPLY),
      std::make_pair("add", commands::KeyEvent::ADD),
      std::make_pair("separator", commands::KeyEvent::SEPARATOR),
      std::make_pair("subtract", commands::KeyEvent::SUBTRACT),
      std::make_pair("decimal", commands::KeyEvent::DECIMAL),
      std::make_pair("divide", commands::KeyEvent::DIVIDE),
      std::make_pair("equals", commands::KeyEvent::EQUALS),
      std::make_pair("comma", commands::KeyEvent::COMMA),

      std::make_pair("on", commands::KeyEvent::ON),
  };

  for (size_t i = 0; i < std::size(kTestData); ++i) {
    SCOPED_TRACE(kTestData[i].first);
    commands::KeyEvent key_event;
    EXPECT_TRUE(KeyParser::ParseKey(kTestData[i].first, &key_event));
    EXPECT_EQ(kTestData[i].second, key_event.special_key());
  }
}

TEST(KeyParserTest, Combination) {
  commands::KeyEvent key_event;

  EXPECT_TRUE(KeyParser::ParseKey("LeftShift CTRL a", &key_event));
  EXPECT_EQ('a', key_event.key_code());
  EXPECT_EQ(commands::KeyEvent::LEFT_SHIFT | commands::KeyEvent::SHIFT |
                commands::KeyEvent::CTRL,
            KeyEventUtil::GetModifiers(key_event));

  EXPECT_TRUE(KeyParser::ParseKey("rightalt On", &key_event));
  EXPECT_EQ(commands::KeyEvent::ON, key_event.special_key());
  EXPECT_EQ(commands::KeyEvent::RIGHT_ALT | commands::KeyEvent::ALT,
            KeyEventUtil::GetModifiers(key_event));

  EXPECT_TRUE(KeyParser::ParseKey("SHIFT on a", &key_event));
  EXPECT_EQ('a', key_event.key_code());
  EXPECT_EQ(commands::KeyEvent::ON, key_event.special_key());
  EXPECT_EQ(commands::KeyEvent::SHIFT, KeyEventUtil::GetModifiers(key_event));
}

}  // namespace mozc
