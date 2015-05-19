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

#include <string>
#include <utility>

#include "base/base.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"
#include "testing/base/public/gunit.h"

namespace mozc {

namespace {
uint32 UnifyModifiers(commands::KeyEvent key_event) {
  uint32 modifiers = 0;
  for (size_t i = 0; i < key_event.modifier_keys_size(); ++i) {
    modifiers |= key_event.modifier_keys(i);
  }
  return modifiers;
}
}  // namespace

TEST(KeyParserTest, KeyCode) {
  commands::KeyEvent key_event;

  EXPECT_TRUE(KeyParser::ParseKey("a", &key_event));
  EXPECT_EQ('a', key_event.key_code());

  key_event.Clear();
  EXPECT_TRUE(KeyParser::ParseKey("A", &key_event));
  EXPECT_EQ('A', key_event.key_code());

  // "あ" (not half width)
  key_event.Clear();
  EXPECT_FALSE(KeyParser::ParseKey("\xE3\x81\x82", &key_event));
}

TEST(KeyParserTest, ModifierKeys) {
  const pair<string, uint32> kTestData[] = {
    make_pair("ctrl", commands::KeyEvent::CTRL),
    make_pair("leftctrl",
              commands::KeyEvent::CTRL | commands::KeyEvent::LEFT_CTRL),
    make_pair("rightctrl",
              commands::KeyEvent::CTRL | commands::KeyEvent::RIGHT_CTRL),

    make_pair("alt", commands::KeyEvent::ALT),
    make_pair("leftalt",
              commands::KeyEvent::ALT | commands::KeyEvent::LEFT_ALT),
    make_pair("rightalt",
              commands::KeyEvent::ALT | commands::KeyEvent::RIGHT_ALT),

    make_pair("shift", commands::KeyEvent::SHIFT),
    make_pair("leftshift",
              commands::KeyEvent::SHIFT | commands::KeyEvent::LEFT_SHIFT),
    make_pair("rightshift",
              commands::KeyEvent::SHIFT | commands::KeyEvent::RIGHT_SHIFT),

    make_pair("caps", commands::KeyEvent::CAPS),
    make_pair("keydown", commands::KeyEvent::KEY_DOWN),
    make_pair("keyup", commands::KeyEvent::KEY_UP),

    make_pair("SHIFT", commands::KeyEvent::SHIFT),
  };

  for (size_t i = 0; i < ARRAYSIZE(kTestData); ++i) {
    SCOPED_TRACE(kTestData[i].first);
    commands::KeyEvent key_event;
    EXPECT_TRUE(KeyParser::ParseKey(kTestData[i].first, &key_event));
    EXPECT_EQ(kTestData[i].second, UnifyModifiers(key_event));
  }
}

TEST(KeyParserTest, MultipleModifierKeys) {
  commands::KeyEvent key_event;
  EXPECT_TRUE(KeyParser::ParseKey("LeftCtrl RightCtrl", &key_event));
  EXPECT_EQ(3, key_event.modifier_keys_size());
  EXPECT_EQ((commands::KeyEvent::CTRL | commands::KeyEvent::LEFT_CTRL |
             commands::KeyEvent::RIGHT_CTRL),
            UnifyModifiers(key_event));
}

TEST(KeyParserTest, SpecialKeys) {
  const pair<string, commands::KeyEvent::SpecialKey> kTestData[] = {
    make_pair("on", commands::KeyEvent::ON),
    make_pair("off", commands::KeyEvent::OFF),
    make_pair("left", commands::KeyEvent::LEFT),
    make_pair("down", commands::KeyEvent::DOWN),
    make_pair("up", commands::KeyEvent::UP),
    make_pair("right", commands::KeyEvent::RIGHT),
    make_pair("enter", commands::KeyEvent::ENTER),
    make_pair("return", commands::KeyEvent::ENTER),
    make_pair("esc", commands::KeyEvent::ESCAPE),
    make_pair("escape", commands::KeyEvent::ESCAPE),
    make_pair("delete", commands::KeyEvent::DEL),
    make_pair("del", commands::KeyEvent::DEL),
    make_pair("bs", commands::KeyEvent::BACKSPACE),
    make_pair("backspace", commands::KeyEvent::BACKSPACE),
    make_pair("henkan", commands::KeyEvent::HENKAN),
    make_pair("muhenkan", commands::KeyEvent::MUHENKAN),
    make_pair("kana", commands::KeyEvent::KANA),
    make_pair("hiragana", commands::KeyEvent::KANA),
    make_pair("katakana", commands::KeyEvent::KATAKANA),
    make_pair("eisu", commands::KeyEvent::EISU),
    make_pair("home", commands::KeyEvent::HOME),
    make_pair("end", commands::KeyEvent::END),
    make_pair("space", commands::KeyEvent::SPACE),
    make_pair("ascii", commands::KeyEvent::ASCII),
    make_pair("tab", commands::KeyEvent::TAB),
    make_pair("pageup", commands::KeyEvent::PAGE_UP),
    make_pair("pagedown", commands::KeyEvent::PAGE_DOWN),
    make_pair("insert", commands::KeyEvent::INSERT),
    make_pair("hankaku", commands::KeyEvent::HANKAKU),
    make_pair("zenkaku", commands::KeyEvent::HANKAKU),
    make_pair("hankaku/zenkaku", commands::KeyEvent::HANKAKU),
    make_pair("kanji", commands::KeyEvent::KANJI),

    make_pair("f1", commands::KeyEvent::F1),
    make_pair("f2", commands::KeyEvent::F2),
    make_pair("f3", commands::KeyEvent::F3),
    make_pair("f4", commands::KeyEvent::F4),
    make_pair("f5", commands::KeyEvent::F5),
    make_pair("f6", commands::KeyEvent::F6),
    make_pair("f7", commands::KeyEvent::F7),
    make_pair("f8", commands::KeyEvent::F8),
    make_pair("f9", commands::KeyEvent::F9),
    make_pair("f10", commands::KeyEvent::F10),
    make_pair("f11", commands::KeyEvent::F11),
    make_pair("f12", commands::KeyEvent::F12),
    make_pair("f13", commands::KeyEvent::F13),
    make_pair("f14", commands::KeyEvent::F14),
    make_pair("f15", commands::KeyEvent::F15),
    make_pair("f16", commands::KeyEvent::F16),
    make_pair("f17", commands::KeyEvent::F17),
    make_pair("f18", commands::KeyEvent::F18),
    make_pair("f19", commands::KeyEvent::F19),
    make_pair("f20", commands::KeyEvent::F20),
    make_pair("f21", commands::KeyEvent::F21),
    make_pair("f22", commands::KeyEvent::F22),
    make_pair("f23", commands::KeyEvent::F23),
    make_pair("f24", commands::KeyEvent::F24),

    make_pair("numpad0", commands::KeyEvent::NUMPAD0),
    make_pair("numpad1", commands::KeyEvent::NUMPAD1),
    make_pair("numpad2", commands::KeyEvent::NUMPAD2),
    make_pair("numpad3", commands::KeyEvent::NUMPAD3),
    make_pair("numpad4", commands::KeyEvent::NUMPAD4),
    make_pair("numpad5", commands::KeyEvent::NUMPAD5),
    make_pair("numpad6", commands::KeyEvent::NUMPAD6),
    make_pair("numpad7", commands::KeyEvent::NUMPAD7),
    make_pair("numpad8", commands::KeyEvent::NUMPAD8),
    make_pair("numpad9", commands::KeyEvent::NUMPAD9),

    make_pair("multiply", commands::KeyEvent::MULTIPLY),
    make_pair("add", commands::KeyEvent::ADD),
    make_pair("separator", commands::KeyEvent::SEPARATOR),
    make_pair("subtract", commands::KeyEvent::SUBTRACT),
    make_pair("decimal", commands::KeyEvent::DECIMAL),
    make_pair("divide", commands::KeyEvent::DIVIDE),
    make_pair("equals", commands::KeyEvent::EQUALS),

    make_pair("on", commands::KeyEvent::ON),
  };

  for (size_t i = 0; i < ARRAYSIZE(kTestData); ++i) {
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
            UnifyModifiers(key_event));

  EXPECT_TRUE(KeyParser::ParseKey("rightalt On", &key_event));
  EXPECT_EQ(commands::KeyEvent::ON, key_event.special_key());
  EXPECT_EQ(commands::KeyEvent::RIGHT_ALT | commands::KeyEvent::ALT,
            UnifyModifiers(key_event));

  EXPECT_TRUE(KeyParser::ParseKey("SHIFT on a", &key_event));
  EXPECT_EQ('a', key_event.key_code());
  EXPECT_EQ(commands::KeyEvent::ON, key_event.special_key());
  EXPECT_EQ(commands::KeyEvent::SHIFT, UnifyModifiers(key_event));
}

}  // namespace mozc
