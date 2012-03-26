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
  uint32 keysym;
  const char *kana;
};

const KanaMap kKanaMapJP[] = {
  // Selected
  { 'a' , "\xe3\x81\xa1" },  // "ち"
  { 'A' , "\xe3\x81\xa1" },  // "ち"
  { 'z' , "\xe3\x81\xa4" },  // "つ"
  { 'Z' , "\xe3\x81\xa3" },  // "っ"
  { '0' , "\xe3\x82\x8f" },  // "わ"
  { '/' , "\xe3\x82\x81" },  // "め"
  { '?' , "\xe3\x83\xbb" },  // "・"
  { '=' , "\xe3\x81\xbb" },  // "ほ"
  { '~' , "\xe3\x82\x92" },  // "を"
  { '|' , "\xe3\x83\xbc" },  // "ー"
  { '_' , "\xe3\x82\x8d" },  // "ろ"
};

const KanaMap kKanaMapUS[] = {
  // Selected
  { 'a' , "\xe3\x81\xa1" },  // "ち"
  { 'A' , "\xe3\x81\xa1" },  // "ち"
  { 'z' , "\xe3\x81\xa4" },  // "つ"
  { 'Z' , "\xe3\x81\xa3" },  // "っ"
  { '0' , "\xe3\x82\x8f" },  // "わ"
  { '/' , "\xe3\x82\x81" },  // "め"
  { '?' , "\xe3\x83\xbb" },  // "・"
  { '=' , "\xe3\x81\xb8" },  // "へ"
  { '~' , "\xe3\x82\x8d" },  // "ろ"
  { '|' , "\xe3\x80\x8d" },  // "」"
  { '_' , "\xe3\x83\xbc" },  // "ー"
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

// Checks "container" contains "key" value or not
bool IsContained(mozc::commands::KeyEvent::ModifierKey key,
                 const ::google::protobuf::RepeatedField<int>& container) {
  for (int i = 0; i< container.size(); ++i) {
    if (container.Get(i) == key) {
      return true;
    }
  }
  return false;
}

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
      KeyEvent key(kKanaMapJP[i].keysym, 0, kLayoutJP[j]);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, mozc::config::Config::KANA, &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      ASSERT_TRUE(out_key.has_key_code());
      EXPECT_EQ(kKanaMapJP[i].keysym, out_key.key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      EXPECT_FALSE(out_key.has_special_key());
      EXPECT_TRUE(out_key.has_key_string());
      EXPECT_EQ(kKanaMapJP[i].kana, out_key.key_string());
    }
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_AsciiKanaUS) {
  ScimKeyTranslator translator;
  for (int j = 0; j < arraysize(kLayoutUS); ++j) {
    for (int i = 0; i < arraysize(kKanaMapUS); ++i) {
      KeyEvent key(kKanaMapUS[i].keysym, 0, kLayoutUS[j]);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, mozc::config::Config::KANA, &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      ASSERT_TRUE(out_key.has_key_code());
      EXPECT_EQ(kKanaMapUS[i].keysym, out_key.key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      EXPECT_FALSE(out_key.has_special_key());
      EXPECT_TRUE(out_key.has_key_string());
      EXPECT_EQ(kKanaMapUS[i].kana, out_key.key_string());
    }
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_AsciiKanaShiftJP) {
  ScimKeyTranslator translator;
  for (int j = 0; j < arraysize(kLayoutJP); ++j) {
    for (int i = 0; i < arraysize(kKanaMapJP); ++i) {
      KeyEvent key(
          kKanaMapJP[i].keysym, scim::SCIM_KEY_ShiftMask, kLayoutJP[j]);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, mozc::config::Config::KANA, &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      ASSERT_TRUE(out_key.has_key_code());
      EXPECT_EQ(kKanaMapJP[i].keysym, out_key.key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      EXPECT_FALSE(out_key.has_special_key());
      EXPECT_TRUE(out_key.has_key_string());
      EXPECT_EQ(kKanaMapJP[i].kana, out_key.key_string());
    }
  }
}

TEST(ScimKeyTranslatorTest, TestTranslate_AsciiKanaShiftUS) {
  ScimKeyTranslator translator;
  for (int j = 0; j < arraysize(kLayoutUS); ++j) {
    for (int i = 0; i < arraysize(kKanaMapUS); ++i) {
      KeyEvent key(
          kKanaMapUS[i].keysym, scim::SCIM_KEY_ShiftMask, kLayoutUS[j]);
      mozc::commands::KeyEvent out_key;
      translator.Translate(key, mozc::config::Config::KANA, &out_key);
      EXPECT_TRUE(out_key.IsInitialized());
      ASSERT_TRUE(out_key.has_key_code());
      EXPECT_EQ(kKanaMapUS[i].keysym, out_key.key_code());
      EXPECT_EQ(0, out_key.modifier_keys_size());
      EXPECT_FALSE(out_key.has_special_key());
      EXPECT_TRUE(out_key.has_key_string());
      EXPECT_EQ(kKanaMapUS[i].kana, out_key.key_string());
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

TEST(ScimKeyTranslatorTest, HiraganaKatakanaHandlingWithSingleModifier) {
  using scim::SCIM_KEY_AltMask;
  using scim::SCIM_KEY_ControlMask;
  using scim::SCIM_KEY_Hiragana;
  using scim::SCIM_KEY_Hiragana_Katakana;
  using scim::SCIM_KEY_Katakana;
  using scim::SCIM_KEY_ShiftMask;

  ScimKeyTranslator translator;

  // Check Hiragana_Katakana local hack. The detail is described in
  // scim_key_translator.cc file.
  mozc::commands::KeyEvent out;

  // S-Hiragana_Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana_Katakana, SCIM_KEY_ShiftMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_FALSE(out.has_key_code());
  ASSERT_TRUE(out.has_special_key());
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  EXPECT_EQ(0, out.modifier_keys_size());

  // C-Hiragana_Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana_Katakana, SCIM_KEY_ControlMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(mozc::commands::KeyEvent::CTRL, out.modifier_keys(0));

  // A-Hiragana_Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana_Katakana, SCIM_KEY_AltMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(mozc::commands::KeyEvent::ALT, out.modifier_keys(0));

  // Hiragana_Katakana handling should have no effect into Hiragana key or
  // Katakana key.
  // S-Hiragana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana, SCIM_KEY_ShiftMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KANA, out.special_key());
  EXPECT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(mozc::commands::KeyEvent::SHIFT, out.modifier_keys(0));

  // C-Hiragana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana, SCIM_KEY_ControlMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(mozc::commands::KeyEvent::CTRL, out.modifier_keys(0));

  // A-Hiragana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana, SCIM_KEY_AltMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(mozc::commands::KeyEvent::ALT, out.modifier_keys(0));

  // S-Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Katakana, SCIM_KEY_ShiftMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  EXPECT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(mozc::commands::KeyEvent::SHIFT, out.modifier_keys(0));

  // C-Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Katakana, SCIM_KEY_ControlMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(mozc::commands::KeyEvent::CTRL, out.modifier_keys(0));

  // A-Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Katakana, SCIM_KEY_AltMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(mozc::commands::KeyEvent::ALT, out.modifier_keys(0));
}

TEST(ScimKeyTranslatorTest, HiraganaKatakanaHandlingWithMultipleModifiers) {
  using scim::SCIM_KEY_AltMask;
  using scim::SCIM_KEY_ControlMask;
  using scim::SCIM_KEY_Hiragana;
  using scim::SCIM_KEY_Hiragana_Katakana;
  using scim::SCIM_KEY_Katakana;
  using scim::SCIM_KEY_ShiftMask;

  ScimKeyTranslator translator;

  // Check Hiragana_Katakana local hack. The detail is described in
  // scim_key_translator.cc file.
  mozc::commands::KeyEvent out;

  // C-S-Hiragana_Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana_Katakana,
               SCIM_KEY_ShiftMask | SCIM_KEY_ControlMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(mozc::commands::KeyEvent::CTRL, out.modifier_keys(0));

  // A-S-Hiragana_Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana_Katakana,
               SCIM_KEY_ShiftMask | SCIM_KEY_AltMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(mozc::commands::KeyEvent::ALT, out.modifier_keys(0));

  // C-A-Hiragana_Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana_Katakana,
               SCIM_KEY_ControlMask | SCIM_KEY_AltMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(mozc::commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::ALT, out.modifier_keys());

  // C-S-A-Hiragana_Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana_Katakana,
               SCIM_KEY_ControlMask | SCIM_KEY_ShiftMask | SCIM_KEY_AltMask,
               0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  EXPECT_EQ(2, out.modifier_keys_size());
  IsContained(mozc::commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::ALT, out.modifier_keys());

  // Hiragana_Katakana handling should have no effect into Hiragana key or
  // Katakana key.
  // C-S-Hiragana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana,
               SCIM_KEY_ShiftMask | SCIM_KEY_ControlMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(mozc::commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::SHIFT, out.modifier_keys());

  // S-A-Hiragana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana, SCIM_KEY_ShiftMask | SCIM_KEY_AltMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(mozc::commands::KeyEvent::ALT, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::SHIFT, out.modifier_keys());

  // C-A-Hiragana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana, SCIM_KEY_ControlMask | SCIM_KEY_AltMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(mozc::commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::ALT, out.modifier_keys());

  // C-S-A-Hiragana
  translator.Translate(
      KeyEvent(SCIM_KEY_Hiragana,
               SCIM_KEY_ShiftMask | SCIM_KEY_ControlMask | SCIM_KEY_AltMask,
               0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KANA, out.special_key());
  EXPECT_EQ(3, out.modifier_keys_size());
  IsContained(mozc::commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::ALT, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::SHIFT, out.modifier_keys());

  // C-S-Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Katakana,
               SCIM_KEY_ControlMask | SCIM_KEY_ShiftMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(mozc::commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::SHIFT, out.modifier_keys());

  // A-S-Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Katakana, SCIM_KEY_AltMask | SCIM_KEY_ShiftMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(mozc::commands::KeyEvent::SHIFT, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::ALT, out.modifier_keys());

  // C-A-Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Katakana, SCIM_KEY_ControlMask | SCIM_KEY_AltMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(mozc::commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::ALT, out.modifier_keys());

  // C-S-A-Katakana
  translator.Translate(
      KeyEvent(SCIM_KEY_Katakana,
               SCIM_KEY_ControlMask | SCIM_KEY_ShiftMask | SCIM_KEY_AltMask, 0),
      mozc::config::Config::ROMAN, &out);
  EXPECT_EQ(mozc::commands::KeyEvent::KATAKANA, out.special_key());
  EXPECT_EQ(3, out.modifier_keys_size());
  IsContained(mozc::commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::SHIFT, out.modifier_keys());
  IsContained(mozc::commands::KeyEvent::ALT, out.modifier_keys());
}
