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

#include "session/key_event_util.h"

#include "base/util.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"
#include "testing/base/public/gunit.h"

namespace mozc {
using commands::KeyEvent;

TEST(KeyEventUtilTest, GetModifiers) {
  commands::KeyEvent key_event;

  KeyParser::ParseKey("a", &key_event);
  EXPECT_EQ(0, KeyEventUtil::GetModifiers(key_event));

  KeyParser::ParseKey("Alt", &key_event);
  EXPECT_EQ(KeyEvent::ALT, KeyEventUtil::GetModifiers(key_event));

  KeyParser::ParseKey("Ctrl", &key_event);
  EXPECT_EQ(KeyEvent::CTRL, KeyEventUtil::GetModifiers(key_event));

  KeyParser::ParseKey("Shift", &key_event);
  EXPECT_EQ(KeyEvent::SHIFT, KeyEventUtil::GetModifiers(key_event));

  KeyParser::ParseKey("Caps", &key_event);
  EXPECT_EQ(KeyEvent::CAPS, KeyEventUtil::GetModifiers(key_event));

  KeyParser::ParseKey("LeftAlt RightAlt", &key_event);
  EXPECT_EQ((KeyEvent::ALT | KeyEvent::LEFT_ALT | KeyEvent::RIGHT_ALT),
            KeyEventUtil::GetModifiers(key_event));

  KeyParser::ParseKey("LeftAlt Ctrl RightShift", &key_event);
  EXPECT_EQ((KeyEvent::ALT | KeyEvent::LEFT_ALT | KeyEvent::CTRL |
             KeyEvent::SHIFT | KeyEvent::RIGHT_SHIFT),
            KeyEventUtil::GetModifiers(key_event));
}

TEST(KeyEventUtilTest, GetKeyInformation) {
  const char *kTestKeys[] = {
    "a",
    "Space",
    "Shift",
    "Shift a",
    "Shift Space",
    "Space a",
    "LeftShift Space a",
  };

  commands::KeyEvent key_event;
  uint64 output;

  for (size_t i = 0; i < ARRAYSIZE(kTestKeys); ++i) {
    SCOPED_TRACE(kTestKeys[i]);
    KeyParser::ParseKey(kTestKeys[i], &key_event);
    ASSERT_TRUE(KeyEventUtil::GetKeyInformation(key_event, &output));

    uint64 expected = 0;
    if (key_event.has_key_code()) {
      expected |= static_cast<uint64>(key_event.key_code());
    }
    if (key_event.has_special_key()) {
      expected |= static_cast<uint64>(key_event.special_key()) << 32;
    }
    expected |=
        static_cast<uint64>(KeyEventUtil::GetModifiers(key_event)) << 48;

    EXPECT_EQ(expected, output);
  }

  const uint32 kEscapeKeyCode = 27;
  key_event.Clear();
  key_event.set_key_code(kEscapeKeyCode);
  // Escape key should not set on key_code field.
  EXPECT_FALSE(KeyEventUtil::GetKeyInformation(key_event, &output));
}

TEST(KeyEventUtilTest, NormalizeCaps) {
  using commands::KeyEvent;

  KeyEvent key_event;
  KeyEvent normalized_key_event;

  {  // Does nothing since key_event doesn't have CapsLock.
    KeyParser::ParseKey("H", &key_event);
    ASSERT_EQ(0, key_event.modifier_keys_size());
    ASSERT_EQ('H', key_event.key_code());

    KeyEventUtil::NormalizeCaps(key_event, &normalized_key_event);
    EXPECT_EQ(0, normalized_key_event.modifier_keys_size());
    EXPECT_EQ('H', normalized_key_event.key_code());
  }

  {  // Removes caps
    KeyParser::ParseKey("CAPS H", &key_event);
    ASSERT_EQ(1, key_event.modifier_keys_size());
    ASSERT_EQ(KeyEvent::CAPS, KeyEventUtil::GetModifiers(key_event));
    ASSERT_EQ('H', key_event.key_code());

    KeyEventUtil::NormalizeCaps(key_event, &normalized_key_event);
    EXPECT_EQ(0, normalized_key_event.modifier_keys_size());
    EXPECT_EQ('h', normalized_key_event.key_code());
  }

  {  // Doesn't remove left or right shift.
    KeyParser::ParseKey("LeftShift RightShift", &key_event);
    ASSERT_EQ(3, key_event.modifier_keys_size());
    ASSERT_EQ((KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT | KeyEvent::RIGHT_SHIFT),
              KeyEventUtil::GetModifiers(key_event));

    KeyEventUtil::NormalizeCaps(key_event, &normalized_key_event);
    EXPECT_EQ(3, normalized_key_event.modifier_keys_size());
    ASSERT_EQ((KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT | KeyEvent::RIGHT_SHIFT),
              KeyEventUtil::GetModifiers(normalized_key_event));
  }

  {  // Removes caps and doesn't remove left shift.
    KeyParser::ParseKey("LeftShift Caps", &key_event);
    ASSERT_EQ(3, key_event.modifier_keys_size());
    ASSERT_EQ((KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT | KeyEvent::CAPS),
              KeyEventUtil::GetModifiers(key_event));

    KeyEventUtil::NormalizeCaps(key_event, &normalized_key_event);
    EXPECT_EQ(2, normalized_key_event.modifier_keys_size());
    ASSERT_EQ((KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT),
              KeyEventUtil::GetModifiers(normalized_key_event));
  }
}

TEST(KeyEventUtilTest, NormalizeKeyEvent) {
  using commands::KeyEvent;

  KeyEvent key_event;
  KeyEvent normalized_key_event;

  {  // Removes caps
    KeyParser::ParseKey("CAPS H", &key_event);
    ASSERT_EQ(1, key_event.modifier_keys_size());
    ASSERT_EQ(KeyEvent::CAPS, KeyEventUtil::GetModifiers(key_event));
    ASSERT_EQ('H', key_event.key_code());

    KeyEventUtil::NormalizeKeyEvent(key_event, &normalized_key_event);
    EXPECT_EQ(0, normalized_key_event.modifier_keys_size());
    EXPECT_EQ('h', normalized_key_event.key_code());
  }

  {  // Removes left_shift
    KeyParser::ParseKey("LeftShift", &key_event);
    ASSERT_EQ(2, key_event.modifier_keys_size());
    ASSERT_EQ((KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT),
              KeyEventUtil::GetModifiers(key_event));

    KeyEventUtil::NormalizeKeyEvent(key_event, &normalized_key_event);
    EXPECT_EQ(1, normalized_key_event.modifier_keys_size());
    ASSERT_EQ(KeyEvent::SHIFT,
              KeyEventUtil::GetModifiers(normalized_key_event));
  }

  {  // Removes caps and left_shift
    KeyParser::ParseKey("CAPS LeftShift H", &key_event);
    ASSERT_EQ(3, key_event.modifier_keys_size());
    ASSERT_EQ((KeyEvent::CAPS | KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT),
              KeyEventUtil::GetModifiers(key_event));

    ASSERT_EQ('H', key_event.key_code());

    KeyEventUtil::NormalizeKeyEvent(key_event, &normalized_key_event);
    EXPECT_EQ(1, normalized_key_event.modifier_keys_size());
    EXPECT_EQ(KeyEvent::SHIFT,
              KeyEventUtil::GetModifiers(normalized_key_event));
    EXPECT_EQ('h', normalized_key_event.key_code());
  }
}

TEST(KeyEventUtilTest, MaybeGetKeyStub) {
  commands::KeyEvent key_event;
  KeyInformation key;

  KeyParser::ParseKey("Shift", &key_event);
  EXPECT_FALSE(KeyEventUtil::MaybeGetKeyStub(key_event, &key));

  KeyParser::ParseKey("Space", &key_event);
  EXPECT_FALSE(KeyEventUtil::MaybeGetKeyStub(key_event, &key));

  const uint32 kEscapeKeyCode = 27;
  key_event.Clear();
  key_event.set_key_code(kEscapeKeyCode);
  EXPECT_FALSE(KeyEventUtil::MaybeGetKeyStub(key_event, &key));

  KeyParser::ParseKey("a", &key_event);
  EXPECT_TRUE(KeyEventUtil::MaybeGetKeyStub(key_event, &key));
  EXPECT_EQ(static_cast<KeyInformation>(commands::KeyEvent::ASCII) << 32, key);
}

TEST(KeyEventUtilTest, HasModifiers) {
  EXPECT_TRUE(KeyEventUtil::HasAlt(KeyEvent::ALT));
  EXPECT_TRUE(KeyEventUtil::HasAlt(KeyEvent::LEFT_ALT));
  EXPECT_TRUE(KeyEventUtil::HasAlt(KeyEvent::ALT | KeyEvent::CTRL));
  EXPECT_FALSE(KeyEventUtil::HasAlt(0));
  EXPECT_FALSE(KeyEventUtil::HasAlt(KeyEvent::CTRL));
  EXPECT_FALSE(KeyEventUtil::HasAlt(KeyEvent::SHIFT));

  EXPECT_TRUE(KeyEventUtil::HasCtrl(KeyEvent::CTRL));
  EXPECT_TRUE(KeyEventUtil::HasCtrl(KeyEvent::LEFT_CTRL));
  EXPECT_TRUE(KeyEventUtil::HasCtrl(KeyEvent::CTRL | KeyEvent::SHIFT));
  EXPECT_FALSE(KeyEventUtil::HasCtrl(0));
  EXPECT_FALSE(KeyEventUtil::HasCtrl(KeyEvent::ALT));
  EXPECT_FALSE(KeyEventUtil::HasCtrl(KeyEvent::SHIFT));

  EXPECT_TRUE(KeyEventUtil::HasShift(KeyEvent::SHIFT));
  EXPECT_TRUE(KeyEventUtil::HasShift(KeyEvent::LEFT_SHIFT));
  EXPECT_TRUE(KeyEventUtil::HasShift(KeyEvent::SHIFT | KeyEvent::ALT));
  EXPECT_FALSE(KeyEventUtil::HasShift(0));
  EXPECT_FALSE(KeyEventUtil::HasShift(KeyEvent::ALT));
  EXPECT_FALSE(KeyEventUtil::HasShift(KeyEvent::CTRL));

  EXPECT_TRUE(KeyEventUtil::HasCaps(KeyEvent::CAPS));
  EXPECT_TRUE(KeyEventUtil::HasCaps(KeyEvent::CAPS | KeyEvent::ALT));
  EXPECT_FALSE(KeyEventUtil::HasCaps(0));
  EXPECT_FALSE(KeyEventUtil::HasCaps(KeyEvent::CTRL));
}

namespace {
const struct IsModifiersTestData {
  uint32 modifiers;
  bool is_alt;
  bool is_ctrl;
  bool is_shift;
  bool is_alt_ctrl;
  bool is_alt_shift;
  bool is_ctrl_shift;
  bool is_alt_ctrl_shift;
} kIsModifiersTestData[] = {
  {
    0,
    false, false, false, false, false, false, false
  }, {
    KeyEvent::ALT,
    true, false, false, false, false, false, false,
  }, {
    KeyEvent::CTRL,
    false, true, false, false, false, false, false,
  }, {
    KeyEvent::SHIFT,
    false, false, true, false, false, false, false,
  }, {
    KeyEvent::ALT | KeyEvent::CTRL,
    false, false, false, true, false, false, false,
  }, {
    KeyEvent::ALT | KeyEvent::SHIFT,
    false, false, false, false, true, false, false,
  }, {
    KeyEvent::CTRL | KeyEvent::SHIFT,
    false, false, false, false, false, true, false,
  }, {
    KeyEvent::ALT | KeyEvent::CTRL | KeyEvent::SHIFT,
    false, false, false, false, false, false, true,
  }, {
    KeyEvent::LEFT_ALT,
    true, false, false, false, false, false, false,
  }, {
    KeyEvent::ALT | KeyEvent::LEFT_ALT | KeyEvent::RIGHT_ALT,
    true, false, false, false, false, false, false,
  }, {
    KeyEvent::CAPS,
    false, false, false, false, false, false, false,
  }, {
    KeyEvent::ALT | KeyEvent::CAPS,
    true, false, false, false, false, false, false,
  },
};
}  // namespace

TEST(KeyEventUtilTest, IsModifiers) {
  for (size_t i = 0; i < ARRAYSIZE(kIsModifiersTestData); ++i) {
    const IsModifiersTestData &data = kIsModifiersTestData[i];
    SCOPED_TRACE(Util::StringPrintf("index: %d", static_cast<int>(i)));

    EXPECT_EQ(data.is_alt, KeyEventUtil::IsAlt(data.modifiers));
    EXPECT_EQ(data.is_ctrl, KeyEventUtil::IsCtrl(data.modifiers));
    EXPECT_EQ(data.is_shift, KeyEventUtil::IsShift(data.modifiers));
    EXPECT_EQ(data.is_alt_ctrl, KeyEventUtil::IsAltCtrl(data.modifiers));
    EXPECT_EQ(data.is_alt_shift, KeyEventUtil::IsAltShift(data.modifiers));
    EXPECT_EQ(data.is_ctrl_shift, KeyEventUtil::IsCtrlShift(data.modifiers));
    EXPECT_EQ(data.is_alt_ctrl_shift,
              KeyEventUtil::IsAltCtrlShift(data.modifiers));
  }
}

const struct IsLowerUpperAlphabetTestData {
  const char *key;
  bool is_lower;
  bool is_upper;
} kIsLowerUpperAlphabetTestData[] = {
  { "a",            true,  false },
  { "A",            false, true },
  { "Shift a",      false, true },
  { "Shift A",      true,  false },
  { "Shift Caps a", true,  false },
  { "Shift Caps A", false, true },
  { "0",            false, false },
  { "Shift",        false, false },
  { "Caps",         false, false },
  { "Space",        false, false },
};

TEST(KeyEventUtilTest, IsLowerUpperAlphabet) {
  for (size_t i = 0; i < ARRAYSIZE(kIsLowerUpperAlphabetTestData); ++i) {
    const IsLowerUpperAlphabetTestData &data = kIsLowerUpperAlphabetTestData[i];
    SCOPED_TRACE(data.key);
    commands::KeyEvent key_event;
    KeyParser::ParseKey(data.key, &key_event);
    EXPECT_EQ(data.is_lower, KeyEventUtil::IsLowerAlphabet(key_event));
    EXPECT_EQ(data.is_upper, KeyEventUtil::IsUpperAlphabet(key_event));
  }
}

}  // namespace mozc
