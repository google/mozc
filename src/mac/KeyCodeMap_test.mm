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

#import "mac/KeyCodeMap.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#include <cstdint>

#include "base/protobuf/text_format.h"
#include "protocol/commands.pb.h"
#include "testing/gunit.h"

using mozc::commands::KeyEvent;

struct TestCase {
  absl::string_view title;
  const char *characters;
  const char *unmod_characters;
  int flags;
  uint16_t keyCode;
  absl::string_view expected;
};

class KeyCodeMapTest : public testing::Test {
 protected:
  void SetUp() override { keyCodeMap_ = [[KeyCodeMap alloc] init]; }

  void KanaMode() { keyCodeMap_.inputMode = KANA; }

  bool CreateKeyEvent(const char *characters, const char *unmod_characters, int flags,
                      uint16_t keyCode, KeyEvent *mozcKeyEvent) {
    if (mozcKeyEvent == nullptr) {
      return false;
    }

    NSEventType type = NSKeyDown;
    // if the key event is just pressing modifiers, the type should be
    // NSFlagsChanged.
    if (characters == nullptr && unmod_characters == nullptr) {
      type = NSFlagsChanged;
    }

    if (characters == nullptr) {
      characters = "";
    }
    if (unmod_characters == nullptr) {
      unmod_characters = characters;
    }

    NSEvent *event = [NSEvent keyEventWithType:type
                                      location:NSZeroPoint
                                 modifierFlags:flags
                                     timestamp:0.0
                                  windowNumber:0
                                       context:nil
                                    characters:@(characters)
                   charactersIgnoringModifiers:@(unmod_characters)
                                     isARepeat:NO
                                       keyCode:keyCode];
    return [keyCodeMap_ getMozcKeyCodeFromKeyEvent:event toMozcKeyEvent:mozcKeyEvent];
  }

  bool CreateKeyEventFromTestCase(const TestCase &testCase, KeyEvent *mozcKeyEvent) {
    return CreateKeyEvent(testCase.characters, testCase.unmod_characters, testCase.flags,
                          testCase.keyCode, mozcKeyEvent);
  }

  std::string GetDebugString(const KeyEvent &event) {
    std::string output;
    mozc::protobuf::TextFormat::PrintToString(event, &output);
    return output;
  }

 private:
  KeyCodeMap *keyCodeMap_;
};

// Test for romaji typing
TEST_F(KeyCodeMapTest, NormaKeyEvent) {
  const TestCase test_cases[] = {
      {"normal a", "a", nullptr, 0, kVK_ANSI_A, "key_code: 97\n"},
      {"\\S-a", "A", "a", NSShiftKeyMask, kVK_ANSI_A, "key_code: 65\n"},
      {"\\C-a", "\x01", "a", NSControlKeyMask, kVK_ANSI_A,
       "key_code: 97\n"
       "modifier_keys: CTRL\n"},
      {"\\S-C-a", "a", nullptr, NSControlKeyMask | NSShiftKeyMask, kVK_ANSI_A,
       "key_code: 97\n"
       "modifier_keys: SHIFT\n"
       "modifier_keys: CTRL\n"},
      {"Tab key", "\x09", nullptr, 0, kVK_Tab, "special_key: TAB\n"},
      {"\\S-Tab", "\x09", nullptr, NSShiftKeyMask, kVK_Tab,
       "special_key: TAB\n"
       "modifier_keys: SHIFT\n"},
      {"function key", "\x10", nullptr, 0, kVK_F1, "special_key: F1\n"},
      {"tenkey", "0", nullptr, 0, kVK_ANSI_Keypad0, "special_key: NUMPAD0\n"}};

  KeyEvent event;
  for (const TestCase &test_case : test_cases) {
    event.Clear();
    EXPECT_TRUE(CreateKeyEventFromTestCase(test_case, &event));
    EXPECT_EQ(GetDebugString(event), test_case.expected) << test_case.title;
  }
}

// Test for kana typing
TEST_F(KeyCodeMapTest, KanaEvent) {
  const TestCase test_cases[] = {
      {"a -> ち", "a", nullptr, 0, kVK_ANSI_A,
       "key_code: 97\n"
       "key_string: \"\\343\\201\\241\"\n"},
      {"yen mark", "¥", nullptr, 0, kVK_JIS_Yen,
       "key_code: 92\n"
       "key_string: \"\\343\\203\\274\"\n"},
      {"\\S-2 -> ふ", "@", "2", NSShiftKeyMask, kVK_ANSI_2,
       "key_code: 64\n"
       "key_string: \"\\343\\201\\265\"\n"},
      {"\\C-a -> \\C-a", "\x01", "a", NSControlKeyMask, kVK_ANSI_A,
       "key_code: 97\n"
       "modifier_keys: CTRL\n"
       "key_string: \"\\343\\201\\241\"\n"},
      {"\\S-0 -> を", "0", nullptr, NSShiftKeyMask, kVK_ANSI_0,
       "key_code: 48\n"
       "key_string: \"\\343\\202\\222\"\n"},
      {"yen mark -> ー", "￥", nullptr, 0, kVK_JIS_Yen, "key_string: \"\\343\\203\\274\"\n"},
      {"underscore -> ろ", "_", nullptr, 0, kVK_JIS_Underscore,
       "key_code: 95\n"
       "key_string: \"\\343\\202\\215\"\n"},
      {"@ -> ゛ in JIS keyboard", "@", nullptr, 0, kVK_ANSI_LeftBracket,
       "key_code: 64\n"
       "key_string: \"\\343\\202\\233\"\n"},
      {"[ -> ゛ in US keyboard", "[", nullptr, 0, kVK_ANSI_LeftBracket,
       "key_code: 91\n"
       "key_string: \"\\343\\202\\233\"\n"},
  };

  KanaMode();
  KeyEvent event;
  for (const TestCase &test_case : test_cases) {
    event.Clear();
    EXPECT_TRUE(CreateKeyEventFromTestCase(test_case, &event));
    EXPECT_EQ(GetDebugString(event), test_case.expected) << test_case.title;
  }
}

// Test for modifier key events
TEST_F(KeyCodeMapTest, Modifiers) {
  KeyEvent event;
  TestCase testCase;
  // Press shift key
  EXPECT_FALSE(CreateKeyEvent(nullptr, nullptr, NSShiftKeyMask, kVK_Shift, &event));

  // Release the shift key -> emit Shift-key event
  event.Clear();
  EXPECT_TRUE(CreateKeyEvent(nullptr, nullptr, 0, kVK_Shift, &event));
  EXPECT_EQ(GetDebugString(event), "modifier_keys: SHIFT\n");

  // Press shift key
  event.Clear();
  EXPECT_FALSE(CreateKeyEvent(nullptr, nullptr, NSShiftKeyMask, kVK_Shift, &event));

  // Press control key
  event.Clear();
  EXPECT_FALSE(
      CreateKeyEvent(nullptr, nullptr, NSShiftKeyMask | NSControlKeyMask, kVK_Control, &event));

  // Release shift key
  event.Clear();
  EXPECT_FALSE(CreateKeyEvent(nullptr, nullptr, NSControlKeyMask, kVK_Control, &event));

  // Release control key -> emit Control + Shift
  event.Clear();
  EXPECT_TRUE(CreateKeyEvent(nullptr, nullptr, 0, kVK_Control, &event));
  EXPECT_EQ(GetDebugString(event), "modifier_keys: SHIFT\nmodifier_keys: CTRL\n");

  // Press control key
  event.Clear();
  EXPECT_FALSE(CreateKeyEvent(nullptr, nullptr, NSControlKeyMask, kVK_Control, &event));

  // Press a -> emit \C-a
  event.Clear();
  EXPECT_TRUE(CreateKeyEvent("a", nullptr, NSControlKeyMask, kVK_ANSI_A, &event));
  EXPECT_EQ(GetDebugString(event), "key_code: 97\nmodifier_keys: CTRL\n");

  // Release control key -> Doesn't emit any key events
  event.Clear();
  EXPECT_FALSE(CreateKeyEvent(nullptr, nullptr, 0, kVK_Control, &event));
}
