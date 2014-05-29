// Copyright 2010-2014, Google Inc.
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

#include <set>

#include "base/scoped_ptr.h"
#include "testing/base/public/gunit.h"
#include "session/commands.pb.h"
#include "unix/ibus/key_translator.h"

namespace mozc {
namespace ibus {

struct {
  guint ibus_key;
  commands::KeyEvent::SpecialKey mozc_key;
} kSpecialKeys[] = {
  {IBUS_space, commands::KeyEvent::SPACE},
  {IBUS_Return, commands::KeyEvent::ENTER},
  {IBUS_Left, commands::KeyEvent::LEFT},
  {IBUS_Right, commands::KeyEvent::RIGHT},
  {IBUS_Up, commands::KeyEvent::UP},
  {IBUS_Down, commands::KeyEvent::DOWN},
  {IBUS_Escape, commands::KeyEvent::ESCAPE},
  {IBUS_Delete, commands::KeyEvent::DEL},
  {IBUS_BackSpace, commands::KeyEvent::BACKSPACE},
  {IBUS_Insert, commands::KeyEvent::INSERT},
  {IBUS_Henkan, commands::KeyEvent::HENKAN},
  {IBUS_Muhenkan, commands::KeyEvent::MUHENKAN},
  {IBUS_Hiragana, commands::KeyEvent::KANA},
  {IBUS_Katakana, commands::KeyEvent::KATAKANA},
  {IBUS_Hiragana_Katakana, commands::KeyEvent::KANA},
  {IBUS_Eisu_toggle, commands::KeyEvent::EISU},
  {IBUS_Home, commands::KeyEvent::HOME},
  {IBUS_End, commands::KeyEvent::END},
  {IBUS_Tab, commands::KeyEvent::TAB},
  {IBUS_F1, commands::KeyEvent::F1},
  {IBUS_F2, commands::KeyEvent::F2},
  {IBUS_F3, commands::KeyEvent::F3},
  {IBUS_F4, commands::KeyEvent::F4},
  {IBUS_F5, commands::KeyEvent::F5},
  {IBUS_F6, commands::KeyEvent::F6},
  {IBUS_F7, commands::KeyEvent::F7},
  {IBUS_F8, commands::KeyEvent::F8},
  {IBUS_F9, commands::KeyEvent::F9},
  {IBUS_F10, commands::KeyEvent::F10},
  {IBUS_F11, commands::KeyEvent::F11},
  {IBUS_F12, commands::KeyEvent::F12},
  {IBUS_F13, commands::KeyEvent::F13},
  {IBUS_F14, commands::KeyEvent::F14},
  {IBUS_F15, commands::KeyEvent::F15},
  {IBUS_F16, commands::KeyEvent::F16},
  {IBUS_F17, commands::KeyEvent::F17},
  {IBUS_F18, commands::KeyEvent::F18},
  {IBUS_F19, commands::KeyEvent::F19},
  {IBUS_F20, commands::KeyEvent::F20},
  {IBUS_F21, commands::KeyEvent::F21},
  {IBUS_F22, commands::KeyEvent::F22},
  {IBUS_F23, commands::KeyEvent::F23},
  {IBUS_F24, commands::KeyEvent::F24},
  {IBUS_Page_Up, commands::KeyEvent::PAGE_UP},
  {IBUS_Page_Down, commands::KeyEvent::PAGE_DOWN},
  {IBUS_KP_0, commands::KeyEvent::NUMPAD0},
  {IBUS_KP_1, commands::KeyEvent::NUMPAD1},
  {IBUS_KP_2, commands::KeyEvent::NUMPAD2},
  {IBUS_KP_3, commands::KeyEvent::NUMPAD3},
  {IBUS_KP_4, commands::KeyEvent::NUMPAD4},
  {IBUS_KP_5, commands::KeyEvent::NUMPAD5},
  {IBUS_KP_6, commands::KeyEvent::NUMPAD6},
  {IBUS_KP_7, commands::KeyEvent::NUMPAD7},
  {IBUS_KP_8, commands::KeyEvent::NUMPAD8},
  {IBUS_KP_9, commands::KeyEvent::NUMPAD9},
  {IBUS_KP_Equal, commands::KeyEvent::EQUALS},
  {IBUS_KP_Multiply, commands::KeyEvent::MULTIPLY},
  {IBUS_KP_Add, commands::KeyEvent::ADD},
  {IBUS_KP_Separator, commands::KeyEvent::SEPARATOR},
  {IBUS_KP_Subtract, commands::KeyEvent::SUBTRACT},
  {IBUS_KP_Decimal, commands::KeyEvent::DECIMAL},
  {IBUS_KP_Divide, commands::KeyEvent::DIVIDE},
  {IBUS_KP_Space, commands::KeyEvent::SPACE},
  {IBUS_KP_Tab, commands::KeyEvent::TAB},
  {IBUS_KP_Enter, commands::KeyEvent::ENTER},
  {IBUS_KP_Home, commands::KeyEvent::HOME},
  {IBUS_KP_Left, commands::KeyEvent::LEFT},
  {IBUS_KP_Up, commands::KeyEvent::UP},
  {IBUS_KP_Right, commands::KeyEvent::RIGHT},
  {IBUS_KP_Down, commands::KeyEvent::DOWN},
  {IBUS_KP_Page_Up, commands::KeyEvent::PAGE_UP},
  {IBUS_KP_Page_Down, commands::KeyEvent::PAGE_DOWN},
  {IBUS_KP_End, commands::KeyEvent::END},
  {IBUS_KP_Delete, commands::KeyEvent::DEL},
  {IBUS_KP_Insert, commands::KeyEvent::INSERT},
  {IBUS_Caps_Lock, commands::KeyEvent::CAPS_LOCK},
  {IBUS_ISO_Left_Tab, commands::KeyEvent::TAB},
};

// Checks "container" contains "key" value or not
bool IsContained(commands::KeyEvent_ModifierKey key,
                 const ::google::protobuf::RepeatedField<int>& container) {
  for (int i = 0; i< container.size(); ++i) {
    if (container.Get(i) == key) {
      return true;
    }
  }
  return false;
}

class KeyTranslatorTest : public testing::Test {
 protected:
  KeyTranslatorTest() {}
  ~KeyTranslatorTest() {}

  virtual void SetUp() {
    translator_.reset(new KeyTranslator);
  }

  scoped_ptr<KeyTranslator> translator_;
};

TEST_F(KeyTranslatorTest, TranslateAscii) {
  commands::KeyEvent out;
  // ' ' (0x20) is treated as a special key by Mozc.
  EXPECT_TRUE(translator_->Translate(0x20, 0, 0, config::Config::ROMAN, true,
                                     &out));
  EXPECT_FALSE(out.has_key_code());
  EXPECT_TRUE(out.has_special_key());
  EXPECT_EQ(0, out.modifier_keys_size());

  for (char c = 0x21; c < 0x7f; ++c) {
    EXPECT_TRUE(translator_->Translate(c, 0, 0, config::Config::ROMAN, true,
                                       &out));
    EXPECT_TRUE(out.has_key_code());
    EXPECT_FALSE(out.has_special_key());
    EXPECT_EQ(c, out.key_code());
    EXPECT_EQ(0, out.modifier_keys_size());
  }
}

TEST_F(KeyTranslatorTest, TranslateSpecial) {
  commands::KeyEvent out;
  for (int i = 0; i < arraysize(kSpecialKeys); ++i) {
    EXPECT_TRUE(translator_->Translate(kSpecialKeys[i].ibus_key, 0, 0,
                                       config::Config::ROMAN, true, &out));
    EXPECT_FALSE(out.has_key_code());
    EXPECT_TRUE(out.has_special_key());
    EXPECT_EQ(kSpecialKeys[i].mozc_key, out.special_key());
    EXPECT_EQ(0, out.modifier_keys_size());
  }

  // Check Hiragana_Katakana local hack. The detail is described in
  // key_translator.cc file.
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana_Katakana, 0, 0,
                                     config::Config::ROMAN, true, &out));
  EXPECT_FALSE(out.has_key_code());
  ASSERT_TRUE(out.has_special_key());
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  EXPECT_EQ(0, out.modifier_keys_size());
}

TEST_F(KeyTranslatorTest, TranslateSingleModifierMasks) {
  commands::KeyEvent out;

  // CTRL modifier
  // C-F1
  EXPECT_TRUE(translator_->Translate(IBUS_F1, 0, IBUS_CONTROL_MASK,
                                     config::Config::ROMAN, true, &out));
  ASSERT_EQ(1, out.modifier_keys_size());

  // C-a
  EXPECT_TRUE(translator_->Translate(IBUS_A, 'a', IBUS_CONTROL_MASK,
                                     config::Config::ROMAN, true, &out));
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::CTRL, out.modifier_keys(0));

  // SHIFT modifier
  // S-F1
  EXPECT_TRUE(translator_->Translate(IBUS_F1, 0, IBUS_SHIFT_MASK,
                                     config::Config::ROMAN, true, &out));
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::SHIFT, out.modifier_keys(0));

  // S-a
  EXPECT_TRUE(translator_->Translate(IBUS_A, 'a', IBUS_SHIFT_MASK,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(0, out.modifier_keys_size());

  // S-0
  // TODO(nona): Resolve Shift-0 problem (b/4338394)
  // We have to check the behavior of Shift-0, because most of japanese
  // keyboard are not assigned Shift-0 character. So that, we expect the client
  // send keycode='0'(\x30) with shift modifier, but currently only send
  // keycode='0'. There are few difficulties because the mapping of Shift-0 are
  // controled xkb in X11, but the way to get the mapping is unclear.

  // ALT modifier
  // M-F1
  EXPECT_TRUE(translator_->Translate(IBUS_F1, 0, IBUS_MOD1_MASK,
                                     config::Config::ROMAN, true, &out));
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::ALT, out.modifier_keys(0));

  // M-a
  EXPECT_TRUE(translator_->Translate(IBUS_A, 'a', IBUS_MOD1_MASK,
                                     config::Config::ROMAN, true, &out));
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::ALT, out.modifier_keys(0));
}

TEST_F(KeyTranslatorTest, TranslateMultipleModifierMasks) {
  commands::KeyEvent out;
  guint modifier;

  // CTRL + SHIFT modifier
  // C-S-F1 (CTRL + SHIFT + SpecialKey)
  modifier = IBUS_CONTROL_MASK | IBUS_SHIFT_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_F1, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::SHIFT, out.modifier_keys());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());

  // C-S-a (CTRL + SHIFT + OtherKey)
  modifier = IBUS_CONTROL_MASK | IBUS_SHIFT_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_A, 'a', modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::SHIFT, out.modifier_keys());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());

  // CTRL + ALT modifier
  // C-M-F1 (CTRL + ALT + SpecialKey)
  modifier = IBUS_CONTROL_MASK | IBUS_MOD1_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_F1, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());

  // C-M-a (CTRL + ALT + OtherKey)
  modifier = IBUS_CONTROL_MASK | IBUS_MOD1_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_A, 'a', modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());

  // SHIFT + ALT modifier
  // S-M-F1 (CTRL + ALT + SpecialKey)
  modifier = IBUS_SHIFT_MASK | IBUS_MOD1_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_F1, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());

  // S-M-a (CTRL + ALT + OtherKey)
  modifier = IBUS_SHIFT_MASK | IBUS_MOD1_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_A, 'a', modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());

  // CTRL + SHIFT + ALT modifier
  // C-S-M-F1 (CTRL + SHIFT + ALT + SpecialKey)
  modifier = IBUS_SHIFT_MASK | IBUS_MOD1_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_F1, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(3, out.modifier_keys_size());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());
  IsContained(commands::KeyEvent::SHIFT, out.modifier_keys());

  // C-S-M-a (CTRL + SHFIT + ALT + OtherKey)
  modifier = IBUS_SHIFT_MASK | IBUS_MOD1_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_A, 'a', modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(3, out.modifier_keys_size());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());
  IsContained(commands::KeyEvent::SHIFT, out.modifier_keys());
}

TEST_F(KeyTranslatorTest, HiraganaKatakanaHandlingWithSingleModifierTest) {
  // Check Hiragana_Katakana local hack. The detail is described in
  // key_translator.cc file.
  commands::KeyEvent out;

  // S-Hiragana_Katakana
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana_Katakana, 0, IBUS_SHIFT_MASK,
                                     config::Config::ROMAN, true, &out));
  EXPECT_FALSE(out.has_key_code());
  ASSERT_TRUE(out.has_special_key());
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  EXPECT_EQ(0, out.modifier_keys_size());

  // C-Hiragana_Katakana
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana_Katakana, 0,
                                     IBUS_CONTROL_MASK, config::Config::ROMAN,
                                     true, &out));
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::CTRL, out.modifier_keys(0));

  // M-Hiragana_Katakana
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana_Katakana, 0, IBUS_MOD1_MASK,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::ALT, out.modifier_keys(0));

  // Hiragana_Katakana handling should have no effect into Hiragana key or
  // Katakana key.
  // S-Hiragana
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana, 0, IBUS_SHIFT_MASK,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  EXPECT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::SHIFT, out.modifier_keys(0));

  // C-Hiragana
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana, 0, IBUS_CONTROL_MASK,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::CTRL, out.modifier_keys(0));

  // M-Hiragana
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana, 0, IBUS_MOD1_MASK,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::ALT, out.modifier_keys(0));

  // S-Katakana
  EXPECT_TRUE(translator_->Translate(IBUS_Katakana, 0, IBUS_SHIFT_MASK,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  EXPECT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::SHIFT, out.modifier_keys(0));

  // C-Katakana
  EXPECT_TRUE(translator_->Translate(IBUS_Katakana, 0, IBUS_CONTROL_MASK,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::CTRL, out.modifier_keys(0));

  // M-Katakana
  EXPECT_TRUE(translator_->Translate(IBUS_Katakana, 0, IBUS_MOD1_MASK,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::ALT, out.modifier_keys(0));
}

TEST_F(KeyTranslatorTest, HiraganaKatakanaHandlingWithMultipleModifiersTest) {
  // Check Hiragana_Katakana local hack. The detail is described in
  // key_translator.cc file.
  commands::KeyEvent out;
  guint modifier;

  // C-S-Hiragana_Katakana
  modifier = IBUS_SHIFT_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana_Katakana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::CTRL, out.modifier_keys(0));

  // M-S-Hiragana_Katakana
  modifier = IBUS_SHIFT_MASK | IBUS_MOD1_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana_Katakana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::ALT, out.modifier_keys(0));

  // C-M-Hiragana_Katakana
  modifier = IBUS_MOD1_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana_Katakana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());

  // C-S-M-Hiragana_Katakana
  modifier = IBUS_SHIFT_MASK | IBUS_MOD1_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana_Katakana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  EXPECT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());

  // Hiragana_Katakana handling should have no effect into Hiragana key or
  // Katakana key.
  // C-S-Hiragana
  modifier = IBUS_SHIFT_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(commands::KeyEvent::SHIFT, out.modifier_keys());

  // M-S-Hiragana
  modifier = IBUS_SHIFT_MASK | IBUS_MOD1_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());
  IsContained(commands::KeyEvent::SHIFT, out.modifier_keys());

  // C-M-Hiragana
  modifier = IBUS_MOD1_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());

  // C-S-M-Hiragana
  modifier = IBUS_SHIFT_MASK | IBUS_MOD1_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Hiragana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KANA, out.special_key());
  EXPECT_EQ(3, out.modifier_keys_size());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());
  IsContained(commands::KeyEvent::SHIFT, out.modifier_keys());

  // C-S-Katakana
  modifier = IBUS_SHIFT_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Katakana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(commands::KeyEvent::SHIFT, out.modifier_keys());

  // M-S-Katakana
  modifier = IBUS_SHIFT_MASK | IBUS_MOD1_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Katakana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::SHIFT, out.modifier_keys());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());

  // C-M-Katakana
  modifier = IBUS_MOD1_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Katakana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  ASSERT_EQ(2, out.modifier_keys_size());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());

  // C-S-M-Katakana
  modifier = IBUS_SHIFT_MASK | IBUS_MOD1_MASK | IBUS_CONTROL_MASK;
  EXPECT_TRUE(translator_->Translate(IBUS_Katakana, 0, modifier,
                                     config::Config::ROMAN, true, &out));
  EXPECT_EQ(commands::KeyEvent::KATAKANA, out.special_key());
  EXPECT_EQ(3, out.modifier_keys_size());
  IsContained(commands::KeyEvent::CTRL, out.modifier_keys());
  IsContained(commands::KeyEvent::SHIFT, out.modifier_keys());
  IsContained(commands::KeyEvent::ALT, out.modifier_keys());
}

TEST_F(KeyTranslatorTest, TranslateUnknow) {
  commands::KeyEvent out;
  EXPECT_FALSE(translator_->Translate(IBUS_VoidSymbol, 0, 0,
                                      config::Config::ROMAN, true, &out));

  // Mozc does not support F25 - F35.
  EXPECT_FALSE(translator_->Translate(IBUS_F25, 0, 0, config::Config::ROMAN,
                                      true, &out));
  EXPECT_FALSE(translator_->Translate(IBUS_F26, 0, 0, config::Config::ROMAN,
                                      true, &out));
  EXPECT_FALSE(translator_->Translate(IBUS_F27, 0, 0, config::Config::ROMAN,
                                      true, &out));
  EXPECT_FALSE(translator_->Translate(IBUS_F28, 0, 0, config::Config::ROMAN,
                                      true, &out));
  EXPECT_FALSE(translator_->Translate(IBUS_F29, 0, 0, config::Config::ROMAN,
                                      true, &out));
  EXPECT_FALSE(translator_->Translate(IBUS_F30, 0, 0, config::Config::ROMAN,
                                      true, &out));
  EXPECT_FALSE(translator_->Translate(IBUS_F31, 0, 0, config::Config::ROMAN,
                                      true, &out));
  EXPECT_FALSE(translator_->Translate(IBUS_F32, 0, 0, config::Config::ROMAN,
                                      true, &out));
  EXPECT_FALSE(translator_->Translate(IBUS_F33, 0, 0, config::Config::ROMAN,
                                      true, &out));
  EXPECT_FALSE(translator_->Translate(IBUS_F34, 0, 0, config::Config::ROMAN,
                                      true, &out));
  EXPECT_FALSE(translator_->Translate(IBUS_F35, 0, 0, config::Config::ROMAN,
                                      true, &out));
}

TEST_F(KeyTranslatorTest, TranslateModiferOnly) {
  commands::KeyEvent out;

  // Just tap left_shift key
  EXPECT_TRUE(translator_->Translate(
      IBUS_Shift_L, 0, 0, config::Config::ROMAN, true, &out));
  EXPECT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::LEFT_SHIFT, out.modifier_keys(0));

  // Just tap right_shift key
  out.Clear();
  EXPECT_TRUE(translator_->Translate(
      IBUS_Shift_R, 0, 0, config::Config::ROMAN, true, &out));
  EXPECT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::RIGHT_SHIFT, out.modifier_keys(0));

  // Just tap left_ctrl key
  out.Clear();
  EXPECT_TRUE(translator_->Translate(
      IBUS_Control_L, 0, 0, config::Config::ROMAN, true, &out));
  EXPECT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::LEFT_CTRL, out.modifier_keys(0));

  // Just tap right_ctrl key
  out.Clear();
  EXPECT_TRUE(translator_->Translate(
      IBUS_Control_R, 0, 0, config::Config::ROMAN, true, &out));
  EXPECT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::RIGHT_CTRL, out.modifier_keys(0));

  // Just tap left_alt key
  out.Clear();
  EXPECT_TRUE(translator_->Translate(
      IBUS_Alt_L, 0, 0, config::Config::ROMAN, true, &out));
  EXPECT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::LEFT_ALT, out.modifier_keys(0));

  // Just tap right_alt key
  out.Clear();
  EXPECT_TRUE(translator_->Translate(
      IBUS_Alt_R, 0, 0, config::Config::ROMAN, true, &out));
  EXPECT_EQ(1, out.modifier_keys_size());
  EXPECT_EQ(commands::KeyEvent::RIGHT_ALT, out.modifier_keys(0));
}

}  // namespace ibus
}  // namespace mozc
