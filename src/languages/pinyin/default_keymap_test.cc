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

#include "languages/pinyin/default_keymap.h"

#include <string>

#include "languages/pinyin/keymap.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace pinyin {
namespace keymap {
namespace {

const char *kSpecialKeys[] = {
  "SPACE",
  "ENTER",
  "LEFT",
  "RIGHT",
  "UP",
  "DOWN",
  "ESCAPE",
  "DEL",
  "BACKSPACE",
  "HOME",
  "END",
  "TAB",
  "PAGEUP",
  "PAGEDOWN",
};

typedef map<string, KeyCommand> KeyTable;

KeyCommand GetKeyCommand(const string &key_string, ConverterState state) {
  commands::KeyEvent key_event;
  EXPECT_TRUE(KeyParser::ParseKey(key_string, &key_event));
  KeyCommand key_command;
  EXPECT_TRUE(DefaultKeymap::GetCommand(key_event, state, &key_command));
  return key_command;
}

void TestSpecialKey(const KeyTable &key_table, const string &modifiers) {
  // Converter is active.
  for (size_t i = 0; i < ARRAYSIZE(kSpecialKeys); ++i) {
    const string key_string = modifiers + ' ' + kSpecialKeys[i];
    SCOPED_TRACE(key_string + " (converter is active)");
    const KeyCommand key_command = GetKeyCommand(key_string, ACTIVE);

    const KeyTable::const_iterator &iter = key_table.find(kSpecialKeys[i]);
    if (iter == key_table.end()) {
      EXPECT_EQ(DO_NOTHING_WITH_CONSUME, key_command);
    } else {
      EXPECT_EQ(iter->second, key_command);
    }
  }

  // Converter is NOT active.  All KeyEvent should NOT be consumed.
  for (size_t i = 0; i < ARRAYSIZE(kSpecialKeys); ++i) {
    const string key_string = modifiers + ' ' + kSpecialKeys[i];
    SCOPED_TRACE(key_string + " (converter is NOT active)");
    const KeyCommand key_command = GetKeyCommand(key_string, INACTIVE);
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME, key_command);
  }
};

}  // namespace

TEST(DefaultKeymapTest, AlphabetKey) {
  {  // Converter is active
    EXPECT_EQ(INSERT, GetKeyCommand("a", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME, GetKeyCommand("A", ACTIVE));
    EXPECT_EQ(TOGGLE_SIMPLIFIED_CHINESE_MODE,
              GetKeyCommand("CTRL SHIFT f", ACTIVE));
    EXPECT_EQ(TOGGLE_SIMPLIFIED_CHINESE_MODE,
              GetKeyCommand("CTRL SHIFT F", ACTIVE));

    EXPECT_EQ(DO_NOTHING_WITH_CONSUME, GetKeyCommand("ALT a", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME, GetKeyCommand("CTRL a", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("ALT CTRL a", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("ALT SHIFT a", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("CTRL SHIFT a", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("ALT CTRL SHIFT a", ACTIVE));
  }

  {  // Converter is NOT active
    EXPECT_EQ(INSERT, GetKeyCommand("a", INACTIVE));
    EXPECT_EQ(INSERT, GetKeyCommand("A", INACTIVE));
    EXPECT_EQ(TOGGLE_SIMPLIFIED_CHINESE_MODE,
              GetKeyCommand("CTRL SHIFT f", INACTIVE));
    EXPECT_EQ(TOGGLE_SIMPLIFIED_CHINESE_MODE,
              GetKeyCommand("CTRL SHIFT F", INACTIVE));

    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT a", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("CTRL a", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT CTRL a", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT SHIFT a", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("CTRL SHIFT a", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT CTRL SHIFT a", INACTIVE));
  }
}

TEST(DefaultKeymapTest, NumberKey) {
  {  // Converter is active
    EXPECT_EQ(SELECT_CANDIDATE, GetKeyCommand("1", ACTIVE));

    EXPECT_EQ(DO_NOTHING_WITH_CONSUME, GetKeyCommand("ALT 1", ACTIVE));
    EXPECT_EQ(CLEAR_CANDIDATE_FROM_HISTORY,
              GetKeyCommand("CTRL 1", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("SHIFT 1", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("ALT CTRL 1", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("ALT SHIFT 1", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("CTRL SHIFT 1", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("ALT CTRL SHIFT 1", ACTIVE));
  }

  {  // Converter is NOT active
    EXPECT_EQ(INSERT, GetKeyCommand("1", INACTIVE));

    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT 1", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("CTRL 1", INACTIVE));
    EXPECT_EQ(INSERT, GetKeyCommand("SHIFT 1", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT CTRL 1", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT SHIFT 1", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("CTRL SHIFT 1", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT CTRL SHIFT 1", INACTIVE));
  }
}

TEST(DefaultKeymapTest, PunctuationKey) {
  {  // Converter is active
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME, GetKeyCommand("!", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME, GetKeyCommand("`", ACTIVE));

    EXPECT_EQ(DO_NOTHING_WITH_CONSUME, GetKeyCommand("ALT !", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME, GetKeyCommand("CTRL !", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("SHIFT !", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("ALT CTRL !", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("ALT SHIFT !", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("CTRL SHIFT !", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITH_CONSUME,
              GetKeyCommand("ALT CTRL SHIFT !", ACTIVE));
  }

  {  // Converter is NOT active
    EXPECT_EQ(INSERT_PUNCTUATION, GetKeyCommand("!", INACTIVE));
    EXPECT_EQ(TURN_ON_PUNCTUATION_MODE, GetKeyCommand("`", INACTIVE));

    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT !", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("CTRL !", INACTIVE));
    EXPECT_EQ(INSERT, GetKeyCommand("SHIFT !", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT CTRL !", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT SHIFT !", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("CTRL SHIFT !", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT CTRL SHIFT !", INACTIVE));
  }
}

TEST(DefaultKeymapTest, SpecialKeyWithoutAltAndCtrl) {
  KeyTable key_table;
  key_table.insert(make_pair("ENTER", COMMIT));
  key_table.insert(make_pair("SPACE", SELECT_FOCUSED_CANDIDATE));
  key_table.insert(make_pair("UP", FOCUS_CANDIDATE_PREV));
  key_table.insert(make_pair("DOWN", FOCUS_CANDIDATE_NEXT));
  key_table.insert(make_pair("RIGHT", MOVE_CURSOR_RIGHT));
  key_table.insert(make_pair("LEFT", MOVE_CURSOR_LEFT));
  key_table.insert(make_pair("PAGEUP", FOCUS_CANDIDATE_PREV_PAGE));
  key_table.insert(make_pair("PAGEDOWN", FOCUS_CANDIDATE_NEXT_PAGE));
  key_table.insert(make_pair("HOME", MOVE_CURSOR_TO_BEGINNING));
  key_table.insert(make_pair("END", MOVE_CURSOR_TO_END));
  key_table.insert(make_pair("BACKSPACE", REMOVE_CHAR_BEFORE));
  key_table.insert(make_pair("DEL", REMOVE_CHAR_AFTER));
  key_table.insert(make_pair("ESCAPE", CLEAR));
  key_table.insert(make_pair("TAB", FOCUS_CANDIDATE_NEXT_PAGE));

  TestSpecialKey(key_table, "");
  TestSpecialKey(key_table, "SHIFT");
}

TEST(DefaultKeymapTest, SpecialKeyWithAlt) {
  KeyTable key_table;
  key_table.insert(make_pair("UP", FOCUS_CANDIDATE_PREV_PAGE));
  key_table.insert(make_pair("DOWN", FOCUS_CANDIDATE_NEXT_PAGE));

  TestSpecialKey(key_table, "ALT");
  TestSpecialKey(key_table, "ALT SHIFT");
}

TEST(DefaultKeymapTest, SpecialKeyWithCtrl) {
  KeyTable key_table;
  key_table.insert(make_pair("RIGHT", MOVE_CURSOR_RIGHT_BY_WORD));
  key_table.insert(make_pair("LEFT", MOVE_CURSOR_LEFT_BY_WORD));
  key_table.insert(make_pair("BACKSPACE", REMOVE_WORD_BEFORE));
  key_table.insert(make_pair("DEL", REMOVE_WORD_AFTER));

  TestSpecialKey(key_table, "CTRL");
  TestSpecialKey(key_table, "CTRL SHIFT");
}

TEST(DefaultKeymapTest, SpecialKeyWithAltAndCtrl) {
  KeyTable key_table;
  key_table.insert(make_pair("UP", MOVE_CURSOR_TO_BEGINNING));
  key_table.insert(make_pair("DOWN", MOVE_CURSOR_TO_END));

  TestSpecialKey(key_table, "ALT CTRL");
  TestSpecialKey(key_table, "ALT CTRL SHIFT");
}

TEST(DefaultKeymapTest, ModifierKey) {
  {  // Converter is active
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME, GetKeyCommand("ALT", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("CTRL", ACTIVE));
    EXPECT_EQ(TOGGLE_DIRECT_MODE, GetKeyCommand("SHIFT", ACTIVE));

    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT CTRL", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT SHIFT", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME ,
              GetKeyCommand("CTRL SHIFT", ACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME ,
              GetKeyCommand("ALT CTRL SHIFT", ACTIVE));
  }

  {  // Converter is NOT active
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("CTRL", INACTIVE));
    EXPECT_EQ(TOGGLE_DIRECT_MODE, GetKeyCommand("SHIFT", INACTIVE));

    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT CTRL", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME,
              GetKeyCommand("ALT SHIFT", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME ,
              GetKeyCommand("CTRL SHIFT", INACTIVE));
    EXPECT_EQ(DO_NOTHING_WITHOUT_CONSUME ,
              GetKeyCommand("ALT CTRL SHIFT", INACTIVE));
  }
}

}  // namespace keymap
}  // namespace pinyin
}  // namespace mozc
