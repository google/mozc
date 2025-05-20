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

#include "protocol/commands.pb.h"
#include "testing/gunit.h"

using mozc::commands::KeyEvent;

struct TestCase {
  std::string title;
  NSString *characters;
  NSString *unmodCharacters;
  int flags;
  uint16_t keyCode;
  std::string expected;
};

class KeyCodeMapTest : public testing::Test {
 protected:
  void SetUp() { keyCodeMap_ = [[KeyCodeMap alloc] init]; }

  void KanaMode() { keyCodeMap_.inputMode = KANA; }

  bool CreateKeyEvent(NSString *characters, NSString *unmodCharacters, int flags, uint16_t keyCode,
                      KeyEvent *mozcKeyEvent) {
    if (mozcKeyEvent == nullptr) {
      return false;
    }

    NSEventType type = NSKeyDown;
    // if the key event is just pressing modifiers, the type should be
    // NSFlagsChanged.
    if (characters == nil && unmodCharacters == nil) {
      type = NSFlagsChanged;
    }

    if (characters == nil) {
      characters = @"";
    }
    if (unmodCharacters == nil) {
      unmodCharacters = [characters copy];
    }

    NSEvent *event = [NSEvent keyEventWithType:type
                                      location:NSZeroPoint
                                 modifierFlags:flags
                                     timestamp:0.0
                                  windowNumber:0
                                       context:nil
                                    characters:characters
                   charactersIgnoringModifiers:unmodCharacters
                                     isARepeat:NO
                                       keyCode:keyCode];
    return [keyCodeMap_ getMozcKeyCodeFromKeyEvent:event toMozcKeyEvent:mozcKeyEvent];
  }

  bool CreateKeyEventFromTestCase(const TestCase &testCase, KeyEvent *mozcKeyEvent) {
    return CreateKeyEvent(testCase.characters, testCase.unmodCharacters, testCase.flags,
                          testCase.keyCode, mozcKeyEvent);
  }

 private:
  KeyCodeMap *keyCodeMap_;
};

static const TestCase kKeyEventTestCases[] = {
    {"normal a", @"a", nil, 0, kVK_ANSI_A, "key_code: 97\n"},
    {"\\S-a", @"A", @"a", NSShiftKeyMask, kVK_ANSI_A, "key_code: 65\n"},
    {"\\C-a", @"\x01", @"a", NSControlKeyMask, kVK_ANSI_A,
     "key_code: 97\n"
     "modifier_keys: CTRL\n"},
    {"\\S-C-a", @"a", nil, NSControlKeyMask | NSShiftKeyMask, kVK_ANSI_A,
     "key_code: 97\n"
     "modifier_keys: SHIFT\n"
     "modifier_keys: CTRL\n"},
    {"Tab key", @"\x09", nil, 0, kVK_Tab, "special_key: TAB\n"},
    {"\\S-Tab", @"\x09", nil, NSShiftKeyMask, kVK_Tab,
     "special_key: TAB\n"
     "modifier_keys: SHIFT\n"},
    {"function key", @"\x10", nil, 0, kVK_F1, "special_key: F1\n"},
    {"tenkey", @"0", nil, 0, kVK_ANSI_Keypad0, "special_key: NUMPAD0\n"}};

static const TestCase kKanaTypingTestCases[] = {
    {"a -> ち", @"a", nil, 0, kVK_ANSI_A,
     "key_code: 97\n"
     "key_string: \"\\343\\201\\241\"\n"},
    {"yen mark", @"¥", nil, 0, kVK_JIS_Yen,
     "key_code: 92\n"
     "key_string: \"\\343\\203\\274\"\n"},
    {"\\S-2 -> ふ", @"@", @"2", NSShiftKeyMask, kVK_ANSI_2,
     "key_code: 64\n"
     "key_string: \"\\343\\201\\265\"\n"},
    {"\\C-a -> \\C-a", @"\x01", @"a", NSControlKeyMask, kVK_ANSI_A,
     "key_code: 97\n"
     "modifier_keys: CTRL\n"
     "key_string: \"\\343\\201\\241\"\n"},
    {"\\S-0 -> を", @"0", nil, NSShiftKeyMask, kVK_ANSI_0,
     "key_code: 48\n"
     "key_string: \"\\343\\202\\222\"\n"},
    {"yen mark -> ー", @"￥", nil, 0, kVK_JIS_Yen, "key_string: \"\\343\\203\\274\"\n"},
    {"underscore -> ろ", @"_", nil, 0, kVK_JIS_Underscore,
     "key_code: 95\n"
     "key_string: \"\\343\\202\\215\"\n"},
    {"@ -> ゛ in JIS keyboard", @"@", nil, 0, kVK_ANSI_LeftBracket,
     "key_code: 64\n"
     "key_string: \"\\343\\202\\233\"\n"},
    {"[ -> ゛ in US keyboard", @"[", nil, 0, kVK_ANSI_LeftBracket,
     "key_code: 91\n"
     "key_string: \"\\343\\202\\233\"\n"},
};

// Test for romaji typing
TEST_F(KeyCodeMapTest, NormaKeyEvent) {
  KeyEvent event;
  for (int i = 0; i < std::size(kKeyEventTestCases); ++i) {
    const TestCase &testCase = kKeyEventTestCases[i];
    event.Clear();
    EXPECT_TRUE(CreateKeyEventFromTestCase(testCase, &event));
    EXPECT_EQ(event.DebugString(), testCase.expected) << testCase.title;
  }
}

// Test for kana typing
TEST_F(KeyCodeMapTest, KanaEvent) {
  KanaMode();
  KeyEvent event;
  for (int i = 0; i < std::size(kKanaTypingTestCases); ++i) {
    const TestCase &testCase = kKanaTypingTestCases[i];
    event.Clear();
    EXPECT_TRUE(CreateKeyEventFromTestCase(testCase, &event));
    EXPECT_EQ(event.DebugString(), testCase.expected) << testCase.title;
  }
}

// Test for modifier key events
TEST_F(KeyCodeMapTest, Modifiers) {
  KeyEvent event;
  TestCase testCase;
  // Press shift key
  EXPECT_FALSE(CreateKeyEvent(nil, nil, NSShiftKeyMask, kVK_Shift, &event));

  // Release the shift key -> emit Shift-key event
  event.Clear();
  EXPECT_TRUE(CreateKeyEvent(nil, nil, 0, kVK_Shift, &event));
  EXPECT_EQ(event.DebugString(), "modifier_keys: SHIFT\n");

  // Press shift key
  event.Clear();
  EXPECT_FALSE(CreateKeyEvent(nil, nil, NSShiftKeyMask, kVK_Shift, &event));

  // Press control key
  event.Clear();
  EXPECT_FALSE(CreateKeyEvent(nil, nil, NSShiftKeyMask | NSControlKeyMask, kVK_Control, &event));

  // Release shift key
  event.Clear();
  EXPECT_FALSE(CreateKeyEvent(nil, nil, NSControlKeyMask, kVK_Control, &event));

  // Release control key -> emit Control + Shift
  event.Clear();
  EXPECT_TRUE(CreateKeyEvent(nil, nil, 0, kVK_Control, &event));
  EXPECT_EQ(event.DebugString(), "modifier_keys: SHIFT\nmodifier_keys: CTRL\n");

  // Press control key
  event.Clear();
  EXPECT_FALSE(CreateKeyEvent(nil, nil, NSControlKeyMask, kVK_Control, &event));

  // Press a -> emit \C-a
  event.Clear();
  EXPECT_TRUE(CreateKeyEvent(@"a", nil, NSControlKeyMask, kVK_ANSI_A, &event));
  EXPECT_EQ(event.DebugString(), "key_code: 97\nmodifier_keys: CTRL\n");

  // Release control key -> Doesn't emit any key events
  event.Clear();
  EXPECT_FALSE(CreateKeyEvent(nil, nil, 0, kVK_Control, &event));
}
