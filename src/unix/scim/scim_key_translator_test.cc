// Copyright 2010, Google Inc.
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

#include "testing/base/public/gunit.h"

#include "base/port.h"
#include "unix/scim/scim_key_translator.h"

using mozc_unix_scim::ScimKeyTranslator;
using scim::KeyEvent;

namespace {

const char kAscii[] = {
  '!', 'a', 'A', '0', '@', ';', ',', '\\', '/', '~'
};

struct KanaMap {
  uint32 code;
  std::string no_shift, shift;
};

const KanaMap kKanaMapJP[] = {
  // Selected
  { 'a' , "\xe3\x81\xa1", "\xe3\x81\xa1" },  // "ち", "ち"
  { 'A' , "\xe3\x81\xa1", "\xe3\x81\xa1" },  // "ち", "ち"
  { 'z' , "\xe3\x81\xa4", "\xe3\x81\xa3" },  // "つ", "っ"
  { 'Z' , "\xe3\x81\xa4", "\xe3\x81\xa3" },  // "つ", "っ"
  { '0' , "\xe3\x82\x8f", "\xe3\x82\x92" },  // "わ", "を"
  { '/' , "\xe3\x82\x81", "\xe3\x83\xbb" },  // "め", "・"
  { '?' , "\xe3\x82\x81", "\xe3\x83\xbb" },  // "め", "・"
  { '=' , "\xe3\x81\xbb", "\xe3\x81\xbb" },  // "ほ", "ほ"
  { '~' , "\xe3\x81\xb8", "\xe3\x82\x92" },  // "へ", "を"
  { '|' , "\xe3\x83\xbc", "\xe3\x83\xbc" },  // "ー", "ー"
  { '_' , "\xe3\x82\x8d", "\xe3\x82\x8d" },  // "ろ", "ろ"
};

const KanaMap kKanaMapUS[] = {
  // Selected
  { 'a' , "\xe3\x81\xa1", "\xe3\x81\xa1" },  // "ち", "ち"
  { 'A' , "\xe3\x81\xa1", "\xe3\x81\xa1" },  // "ち", "ち"
  { 'z' , "\xe3\x81\xa4", "\xe3\x81\xa3" },  // "つ", "っ"
  { 'Z' , "\xe3\x81\xa4", "\xe3\x81\xa3" },  // "つ", "っ"
  { '0' , "\xe3\x82\x8f", "\xe3\x82\x92" },  // "わ", "を"
  { '/' , "\xe3\x82\x81", "\xe3\x83\xbb" },  // "め", "・"
  { '?' , "\xe3\x82\x81", "\xe3\x83\xbb" },  // "め", "・"
  { '=' , "\xe3\x81\xb8", "\xe3\x81\xb8" },  // "へ", "へ"
  { '~' , "\xe3\x82\x8d", "\xe3\x82\x8d" },  // "ろ", "ろ"
  { '|' , "\xe3\x82\x80", "\xe3\x80\x8d" },  // "む", "」"
  { '_' , "\xe3\x81\xbb", "\xe3\x83\xbc" },  // "ほ", "ー"
};

const scim::KeyCode kSpecial[] = {
  scim::SCIM_KEY_F1,
  scim::SCIM_KEY_F12,
  scim::SCIM_KEY_F13,
  scim::SCIM_KEY_F24,
  scim::SCIM_KEY_Page_Up,
  scim::SCIM_KEY_Page_Down,
  scim::SCIM_KEY_Return,
  scim::SCIM_KEY_Tab,
  scim::SCIM_KEY_BackSpace,
  scim::SCIM_KEY_Escape,
};

const mozc::commands::KeyEvent::SpecialKey kMappedSpecial[] = {
  mozc::commands::KeyEvent::F1,
  mozc::commands::KeyEvent::F12,
  mozc::commands::KeyEvent::F13,
  mozc::commands::KeyEvent::F24,
  mozc::commands::KeyEvent::PAGE_UP,
  mozc::commands::KeyEvent::PAGE_DOWN,
  mozc::commands::KeyEvent::ENTER,
  mozc::commands::KeyEvent::TAB,
  mozc::commands::KeyEvent::BACKSPACE,
  mozc::commands::KeyEvent::ESCAPE,
};

const mozc::config::Config::PreeditMethod kMethod[] = {
  mozc::config::Config::ROMAN,
  mozc::config::Config::KANA,
};

const uint16 kLayoutJP[] = {
  scim::SCIM_KEYBOARD_Unknown,
  scim::SCIM_KEYBOARD_Japanese,
};

const uint16 kLayoutUS[] = {
  scim::SCIM_KEYBOARD_US,
};

}  // namespace

TEST(ScimKeyTranslatorTest, TestCanConvert_Release) {
  ScimKeyTranslator translator;
  // We don't handle any event that have KEY_ReleaseMask mask.
  KeyEvent key(scim::SCIM_KEY_Return, scim::SCIM_KEY_ReleaseMask, 0);
  EXPECT_FALSE(translator.CanConvert(key));
}

TEST(ScimKeyTranslatorTest, TestCanConvert_Modifier) {
  ScimKeyTranslator translator;
  // We don't handle modifier-only input.
  {
    KeyEvent key(scim::SCIM_KEY_Alt_L, 0, 0);
    EXPECT_FALSE(translator.CanConvert(key));
  }
  {
    KeyEvent key(scim::SCIM_KEY_Control_L, 0, 0);
    EXPECT_FALSE(translator.CanConvert(key));
  }
  {
    KeyEvent key(scim::SCIM_KEY_Super_R, 0, 0);
    EXPECT_FALSE(translator.CanConvert(key));
  }
}

TEST(ScimKeyTranslatorTest, TestCanConvert_UnknownSpecial) {
  ScimKeyTranslator translator;
  // F25 is one of a special (i.e. non-ASCII) key which Mozc doesn't know.
  {
    KeyEvent key(scim::SCIM_KEY_F25, 0, 0);
    EXPECT_FALSE(translator.CanConvert(key));
  }
  // Mozc doesn't know LF as well.
  {
    KeyEvent key(scim::SCIM_KEY_Linefeed, 0, 0);
    EXPECT_FALSE(translator.CanConvert(key));
  }
}

TEST(ScimKeyTranslatorTest, TestCanConvert_KnownSpecial) {
  ScimKeyTranslator translator;
  // Mozc knows F1 to 12, PageUp, PageDown, etc.
  for (int i = 0; i < arraysize(kSpecial); ++i) {
    KeyEvent key(kSpecial[i], 0, 0);
    EXPECT_TRUE(translator.CanConvert(key)) << i;
  }
}

TEST(ScimKeyTranslatorTest, TestCanConvert_NumericKeypad) {
  ScimKeyTranslator translator;
  // Mozc knows Keypad (10-key).
  {
    KeyEvent key(scim::SCIM_KEY_KP_Space, 0, 0);
    EXPECT_TRUE(translator.CanConvert(key));
  }
  {
    KeyEvent key(scim::SCIM_KEY_KP_Delete, 0, 0);
    EXPECT_TRUE(translator.CanConvert(key));
  }
  {
    KeyEvent key(scim::SCIM_KEY_KP_Equal, 0, 0);
    EXPECT_TRUE(translator.CanConvert(key));
  }
  {
    KeyEvent key(scim::SCIM_KEY_KP_9, 0, 0);
    EXPECT_TRUE(translator.CanConvert(key));
  }
}

TEST(ScimKeyTranslatorTest, TestCanConvert_Ascii) {
  ScimKeyTranslator translator;
  for (int i = 0; i < arraysize(kAscii); ++i) {
    KeyEvent key(kAscii[i], 0, 0);
    // Note: The scim::KeyEvent::KeyEvent(const String &str) constructor does
    // not accept symbols like "!".
    EXPECT_TRUE(translator.CanConvert(key)) << i;
  }
}

TEST(ScimKeyTranslatorTest, TestCanConvert_ModifierAscii) {
  ScimKeyTranslator translator;
  for (int i = 0; i < arraysize(kAscii); ++i) {
    KeyEvent key(kAscii[i], scim::SCIM_KEY_ShiftMask, 0);
    EXPECT_TRUE(translator.CanConvert(key)) << i;
  }
  for (int i = 0; i < arraysize(kAscii); ++i) {
    KeyEvent key(kAscii[i], scim::SCIM_KEY_ControlMask, 0);
    EXPECT_TRUE(translator.CanConvert(key)) << i;
  }
  for (int i = 0; i < arraysize(kAscii); ++i) {
    KeyEvent key(
        kAscii[i], scim::SCIM_KEY_ControlMask | scim::SCIM_KEY_ShiftMask, 0);
    EXPECT_TRUE(translator.CanConvert(key)) << i;
  }
}

TEST(ScimKeyTranslatorTest, TestCanConvert_ModifierSpecial) {
  ScimKeyTranslator translator;
  for (int i = 0; i < arraysize(kSpecial); ++i) {
    KeyEvent key(kSpecial[i], scim::SCIM_KEY_ShiftMask, 0);
    EXPECT_TRUE(translator.CanConvert(key)) << i;
  }
  for (int i = 0; i < arraysize(kSpecial); ++i) {
    KeyEvent key(kSpecial[i], scim::SCIM_KEY_ControlMask, 0);
    EXPECT_TRUE(translator.CanConvert(key)) << i;
  }
  for (int i = 0; i < arraysize(kSpecial); ++i) {
    KeyEvent key(
        kSpecial[i], scim::SCIM_KEY_ControlMask | scim::SCIM_KEY_ShiftMask, 0);
    EXPECT_TRUE(translator.CanConvert(key)) << i;
  }
}

#if defined(DEBUG)
TEST(ScimKeyTranslatorTest, TestTranslate_UnknownSpecial) {
  ScimKeyTranslator translator;
  mozc::commands::KeyEvent out;
  for (int i = 0; i < arraysize(kMethod); ++i) {
    {
      KeyEvent key(scim::SCIM_KEY_F25, 0, 0);
      // precondition violation.
      EXPECT_DEATH(
          translator.Translate(key, kMethod[i], &out), "");
    }
    {
      KeyEvent key(scim::SCIM_KEY_Linefeed, 0, 0);
      // ditto.
      EXPECT_DEATH(
          translator.Translate(key, kMethod[i], &out), "");
    }
  }
  // We can safely use EXPECT_DEATH since the test is single threaded.
}
#endif

TEST(ScimKeyTranslatorTest, TestTranslate_KnownSpecials) {
  ScimKeyTranslator translator;
  // Mozc knows F1 to 12, PageUp, PageDown, etc.
  for (int j = 0; j < arraysize(kMethod); ++j) {
    for (int i = 0; i < arraysize(kSpecial); ++i) {
      KeyEvent key(kSpecial[i], 0, 0);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, kMethod[j], &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      EXPECT_FALSE(out_key.has_key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      ASSERT_TRUE(out_key.has_special_key());
      EXPECT_EQ(kMappedSpecial[i], out_key.special_key());
      EXPECT_FALSE(out_key.has_key_string());
    }
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_NumericKeypad) {
  ScimKeyTranslator translator;
  // Mozc knows Keypad (10-key).
  for (int i = 0; i < arraysize(kMethod); ++i) {
    {
      KeyEvent key(scim::SCIM_KEY_KP_0, 0, 0);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, kMethod[i], &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      EXPECT_FALSE(out_key.has_key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      ASSERT_TRUE(out_key.has_special_key());
      EXPECT_EQ(mozc::commands::KeyEvent::NUMPAD0, out_key.special_key());
      EXPECT_FALSE(out_key.has_key_string());
    }
    {
      KeyEvent key(scim::SCIM_KEY_KP_Divide, 0, 0);  // [/] on Keypad.
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, kMethod[i], &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      EXPECT_FALSE(out_key.has_key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      ASSERT_TRUE(out_key.has_special_key());
      EXPECT_EQ(mozc::commands::KeyEvent::DIVIDE, out_key.special_key());
      EXPECT_FALSE(out_key.has_key_string());
    }
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_AsciiRoman) {
  ScimKeyTranslator translator;
  for (int i = 0; i < arraysize(kAscii); ++i) {
    KeyEvent key(kAscii[i], 0, 0);
    mozc::commands::KeyEvent out_key;
    translator.Translate(key, mozc::config::Config::ROMAN, &out_key);
    EXPECT_TRUE(out_key.IsInitialized());
    ASSERT_TRUE(out_key.has_key_code());
    EXPECT_EQ(kAscii[i], out_key.key_code());
    EXPECT_EQ(0, out_key.modifier_keys_size());
    EXPECT_FALSE(out_key.has_special_key());
    EXPECT_FALSE(out_key.has_key_string());
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_AsciiKanaJP) {
  ScimKeyTranslator translator;
  for (int j = 0; j < arraysize(kLayoutJP); ++j) {
    for (int i = 0; i < arraysize(kKanaMapJP); ++i) {
      KeyEvent key(kKanaMapJP[i].code, 0, kLayoutJP[j]);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, mozc::config::Config::KANA, &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      ASSERT_TRUE(out_key.has_key_code());
      EXPECT_EQ(kKanaMapJP[i].code, out_key.key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      EXPECT_FALSE(out_key.has_special_key());
      EXPECT_TRUE(out_key.has_key_string());
      EXPECT_EQ(kKanaMapJP[i].no_shift, out_key.key_string());
    }
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_AsciiKanaUS) {
  ScimKeyTranslator translator;
  for (int j = 0; j < arraysize(kLayoutUS); ++j) {
    for (int i = 0; i < arraysize(kKanaMapUS); ++i) {
      KeyEvent key(kKanaMapUS[i].code, 0, kLayoutUS[j]);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, mozc::config::Config::KANA, &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      ASSERT_TRUE(out_key.has_key_code());
      EXPECT_EQ(kKanaMapUS[i].code, out_key.key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      EXPECT_FALSE(out_key.has_special_key());
      EXPECT_TRUE(out_key.has_key_string());
      EXPECT_EQ(kKanaMapUS[i].no_shift, out_key.key_string());
    }
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_AsciiKanaShiftJP) {
  ScimKeyTranslator translator;
  for (int j = 0; j < arraysize(kLayoutJP); ++j) {
    for (int i = 0; i < arraysize(kKanaMapJP); ++i) {
      KeyEvent key(kKanaMapJP[i].code, scim::SCIM_KEY_ShiftMask, kLayoutJP[j]);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, mozc::config::Config::KANA, &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      ASSERT_TRUE(out_key.has_key_code());
      EXPECT_EQ(kKanaMapJP[i].code, out_key.key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      EXPECT_FALSE(out_key.has_special_key());
      EXPECT_TRUE(out_key.has_key_string());
      EXPECT_EQ(kKanaMapJP[i].shift, out_key.key_string());
    }
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_AsciiKanaShiftUS) {
  ScimKeyTranslator translator;
  for (int j = 0; j < arraysize(kLayoutUS); ++j) {
    for (int i = 0; i < arraysize(kKanaMapUS); ++i) {
      KeyEvent key(kKanaMapUS[i].code, scim::SCIM_KEY_ShiftMask, kLayoutUS[j]);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, mozc::config::Config::KANA, &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      ASSERT_TRUE(out_key.has_key_code());
      EXPECT_EQ(kKanaMapUS[i].code, out_key.key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      EXPECT_FALSE(out_key.has_special_key());
      EXPECT_TRUE(out_key.has_key_string());
      EXPECT_EQ(kKanaMapUS[i].shift, out_key.key_string());
    }
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_AsciiKanaBackslashJP) {
  ScimKeyTranslator translator;
  scim::KeyboardLayout layout = scim::SCIM_KEYBOARD_Japanese;
  {
    KeyEvent key('\\', 0, layout);
    mozc::commands::KeyEvent out_key;
    translator.Translate(key, mozc::config::Config::KANA, &out_key);
    EXPECT_TRUE(out_key.IsInitialized());
    ASSERT_TRUE(out_key.has_key_code());
    EXPECT_EQ('\\', out_key.key_code());
    EXPECT_EQ(0, out_key.modifier_keys_size());
    EXPECT_FALSE(out_key.has_special_key());
    EXPECT_TRUE(out_key.has_key_string());
    EXPECT_EQ("\xe3\x83\xbc", out_key.key_string());  // "ー"
  }
  {
    KeyEvent key('\\', scim::SCIM_KEY_QuirkKanaRoMask, layout);
    mozc::commands::KeyEvent out_key;
    translator.Translate(key, mozc::config::Config::KANA, &out_key);
    EXPECT_TRUE(out_key.IsInitialized());
    ASSERT_TRUE(out_key.has_key_code());
    EXPECT_EQ('\\', out_key.key_code());
    EXPECT_EQ(0, out_key.modifier_keys_size());
    EXPECT_FALSE(out_key.has_special_key());
    EXPECT_TRUE(out_key.has_key_string());
    EXPECT_EQ("\xe3\x82\x8d", out_key.key_string());  // "ろ"
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_AsciiKanaBackslashUS) {
  ScimKeyTranslator translator;
  scim::KeyboardLayout layout = scim::SCIM_KEYBOARD_US;
  {
    KeyEvent key('\\', 0, layout);
    mozc::commands::KeyEvent out_key;
    translator.Translate(key, mozc::config::Config::KANA, &out_key);
    EXPECT_TRUE(out_key.IsInitialized());
    ASSERT_TRUE(out_key.has_key_code());
    EXPECT_EQ('\\', out_key.key_code());
    EXPECT_EQ(0, out_key.modifier_keys_size());
    EXPECT_FALSE(out_key.has_special_key());
    EXPECT_TRUE(out_key.has_key_string());
    EXPECT_EQ("\xe3\x82\x80", out_key.key_string());  // "む"
  }
  {
    KeyEvent key('\\', scim::SCIM_KEY_QuirkKanaRoMask, layout);
    mozc::commands::KeyEvent out_key;
    translator.Translate(key, mozc::config::Config::KANA, &out_key);
    EXPECT_TRUE(out_key.IsInitialized());
    ASSERT_TRUE(out_key.has_key_code());
    EXPECT_EQ('\\', out_key.key_code());
    EXPECT_EQ(0, out_key.modifier_keys_size());
    EXPECT_FALSE(out_key.has_special_key());
    EXPECT_TRUE(out_key.has_key_string());
    EXPECT_EQ("\xe3\x82\x80", out_key.key_string());  // "む"
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_ModifierAscii) {
  ScimKeyTranslator translator;
  for (int j = 0; j < arraysize(kMethod); ++j) {
    for (int i = 0; i < arraysize(kAscii); ++i) {
      KeyEvent key(kAscii[i],
                   scim::SCIM_KEY_ControlMask | scim::SCIM_KEY_ShiftMask, 0);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, kMethod[j], &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      ASSERT_TRUE(out_key.has_key_code());
      EXPECT_EQ(kAscii[i], out_key.key_code());
      // Users might use SHIFT key to input some symbols. We should be able to
      // convert them but the SHIFT modifier should be omitted from the output.
      // when the key_code() is ascii. See http://b/1456236 for details.
      ASSERT_EQ(1, out_key.modifier_keys_size());  // NOT 2.
      EXPECT_EQ(mozc::commands::KeyEvent::CTRL, out_key.modifier_keys(0));
      EXPECT_FALSE(out_key.has_special_key());
    }
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_ModifierSpecial) {
  ScimKeyTranslator translator;
  for (int j = 0; j < arraysize(kMethod); ++j) {
    for (int i = 0; i < arraysize(kSpecial); ++i) {
      KeyEvent key(kSpecial[i],
                   scim::SCIM_KEY_ControlMask | scim::SCIM_KEY_ShiftMask, 0);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, kMethod[j], &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      EXPECT_FALSE(out_key.has_key_code());
      // The key_code() is NOT ascii. So we should NOT ignore the SHIFT
      // modifier.
      ASSERT_EQ(2, out_key.modifier_keys_size()); // NOT 1.
      EXPECT_EQ(mozc::commands::KeyEvent::CTRL |
                mozc::commands::KeyEvent::SHIFT,
                out_key.modifier_keys(0) | out_key.modifier_keys(1));
      ASSERT_TRUE(out_key.has_special_key());
      EXPECT_EQ(kMappedSpecial[i], out_key.special_key());
    }
  }
}
