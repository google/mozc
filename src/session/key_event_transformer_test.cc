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

#include "session/key_event_transformer.h"

#include <cstdint>
#include <string>

#include "absl/strings/str_cat.h"
#include "base/singleton.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"

namespace mozc {
namespace session {
namespace {

::testing::AssertionResult IsKeyEventTransformerEq(
    const KeyEventTransformer &x, const KeyEventTransformer &y) {
  if (x.table().size() != y.table().size()) {
    return ::testing::AssertionFailure() << "Table size differs";
  }

  for (const auto &[key, event] : x.table()) {
    const auto iter = y.table().find(key);
    if (iter == y.table().end()) {
      return ::testing::AssertionFailure()
             << "Key doesn't exist in RHS: key = " << key;
    }
    if (absl::StrCat(event) != absl::StrCat(iter->second)) {
      return ::testing::AssertionFailure()
             << "Value mismatch for key = " << key;
    }
  }

  if (x.numpad_character_form() != y.numpad_character_form()) {
    return ::testing::AssertionFailure()
           << "numpad_character_form is different: "
           << x.numpad_character_form() << " vs " << y.numpad_character_form();
  }

  return ::testing::AssertionSuccess();
}

void TestNumpadTransformation(commands::KeyEvent::SpecialKey input,
                              uint32_t expected_key_code,
                              const std::string &expected_key_string,
                              commands::KeyEvent::InputStyle expected_style) {
  KeyEventTransformer *table = Singleton<KeyEventTransformer>::get();

  commands::KeyEvent key_event;
  key_event.set_special_key(input);
  EXPECT_TRUE(table->TransformKeyEvent(&key_event));

  ASSERT_TRUE(key_event.has_key_code());
  ASSERT_TRUE(key_event.has_key_string());
  ASSERT_FALSE(key_event.has_special_key());
  EXPECT_EQ(key_event.key_code(), expected_key_code);
  EXPECT_EQ(key_event.key_string(), expected_key_string);
  EXPECT_EQ(key_event.input_style(), expected_style);
}

void TestKanaTransformation(const std::string &key_string,
                            uint32_t expected_key_code,
                            const std::string &expected_key_string) {
  KeyEventTransformer *table = Singleton<KeyEventTransformer>::get();

  commands::KeyEvent key_event;
  key_event.set_key_string(key_string);

  if (key_string == expected_key_string) {
    // Should not transform key_event.
    ASSERT_FALSE(table->TransformKeyEvent(&key_event));
    return;
  }

  ASSERT_TRUE(table->TransformKeyEvent(&key_event));

  ASSERT_TRUE(key_event.has_key_string()) << key_string;
  ASSERT_FALSE(key_event.has_special_key()) << key_string;
  EXPECT_EQ(key_event.key_string(), expected_key_string) << key_string;
  EXPECT_EQ(key_event.key_code(), expected_key_code) << key_string;
}

}  // namespace

TEST(KeyEventTransformerTest, Numpad) {
  KeyEventTransformer *table = Singleton<KeyEventTransformer>::get();

  {  // "Separator to Enter"
    commands::KeyEvent key_event;
    key_event.set_special_key(commands::KeyEvent::SEPARATOR);
    EXPECT_TRUE(table->TransformKeyEvent(&key_event));

    ASSERT_TRUE(key_event.has_special_key());
    ASSERT_FALSE(key_event.has_key_code());
    EXPECT_EQ(key_event.special_key(), commands::KeyEvent::ENTER);
  }

  {  // NUMPAD_INPUT_MODE
    config::Config config;
    config.set_numpad_character_form(config::Config::NUMPAD_INPUT_MODE);
    table->ReloadConfig(config);
    {
      SCOPED_TRACE("NUMPAD_INPUT_MODE: input: 0");
      TestNumpadTransformation(commands::KeyEvent::NUMPAD0, '0', "０",
                               commands::KeyEvent::FOLLOW_MODE);
    }
    {
      SCOPED_TRACE("NUMPAD_INPUT_MODE: input: =");
      TestNumpadTransformation(commands::KeyEvent::EQUALS, '=', "＝",
                               commands::KeyEvent::FOLLOW_MODE);
    }
  }

  {  // NUMPAD_FULL_WIDTH
    config::Config config;
    config.set_numpad_character_form(config::Config::NUMPAD_FULL_WIDTH);
    table->ReloadConfig(config);
    {
      SCOPED_TRACE("NUMPAD_FULL_WIDTH: input: 0");
      TestNumpadTransformation(commands::KeyEvent::NUMPAD0, '0', "０",
                               commands::KeyEvent::AS_IS);
    }
    {
      SCOPED_TRACE("NUMPAD_FULL_WIDTH: input: =");
      TestNumpadTransformation(commands::KeyEvent::EQUALS, '=', "＝",
                               commands::KeyEvent::AS_IS);
    }
  }

  {  // NUMPAD_HALF_WIDTH
    config::Config config;
    config.set_numpad_character_form(config::Config::NUMPAD_HALF_WIDTH);
    table->ReloadConfig(config);
    {
      SCOPED_TRACE("NUMPAD_HALF_WIDTH: input: 0");
      TestNumpadTransformation(commands::KeyEvent::NUMPAD0, '0', "0",
                               commands::KeyEvent::AS_IS);
    }
    {
      SCOPED_TRACE("NUMPAD_HALF_WIDTH: input: =");
      TestNumpadTransformation(commands::KeyEvent::EQUALS, '=', "=",
                               commands::KeyEvent::AS_IS);
    }
  }

  {  // NUMPAD_DIRECT_INPUT
    config::Config config;
    config.set_numpad_character_form(config::Config::NUMPAD_DIRECT_INPUT);
    table->ReloadConfig(config);
    {
      SCOPED_TRACE("NUMPAD_DIRECT_INPUT: input: 0");
      TestNumpadTransformation(commands::KeyEvent::NUMPAD0, '0', "0",
                               commands::KeyEvent::DIRECT_INPUT);
    }
    {
      SCOPED_TRACE("NUMPAD_DIRECT_INPUT: input: =");
      TestNumpadTransformation(commands::KeyEvent::EQUALS, '=', "=",
                               commands::KeyEvent::DIRECT_INPUT);
    }
  }
}

TEST(KeyEventTransformerTest, Kana) {
  KeyEventTransformer *table = Singleton<KeyEventTransformer>::get();

  {  // Punctuation
    const char *kFullKuten = "、";
    const char *kFullTouten = "。";
    const char *kFullComma = "，";
    const char *kFullPeriod = "．";

    {
      SCOPED_TRACE("KUTEN_TOUTEN");
      config::Config config;
      config.set_punctuation_method(config::Config::KUTEN_TOUTEN);
      table->ReloadConfig(config);

      TestKanaTransformation(kFullKuten, ',', kFullKuten);
      TestKanaTransformation(kFullTouten, '.', kFullTouten);
    }
    {
      SCOPED_TRACE("COMMA_PERIOD");
      config::Config config;
      config.set_punctuation_method(config::Config::COMMA_PERIOD);
      table->ReloadConfig(config);

      TestKanaTransformation(kFullKuten, ',', kFullComma);
      TestKanaTransformation(kFullTouten, '.', kFullPeriod);
    }
    {
      SCOPED_TRACE("KUTEN_PERIOD");
      config::Config config;
      config.set_punctuation_method(config::Config::KUTEN_PERIOD);
      table->ReloadConfig(config);

      TestKanaTransformation(kFullKuten, ',', kFullKuten);
      TestKanaTransformation(kFullTouten, '.', kFullPeriod);
    }
    {
      SCOPED_TRACE("COMMA_TOUTEN");
      config::Config config;
      config.set_punctuation_method(config::Config::COMMA_TOUTEN);
      table->ReloadConfig(config);

      TestKanaTransformation(kFullKuten, ',', kFullComma);
      TestKanaTransformation(kFullTouten, '.', kFullTouten);
    }
  }

  {  // Symbol
    const char *kFullLeftSquareBracket = "［";
    const char *kFullRightSquareBracket = "］";
    const char *kFullLeftCornerBracket = "「";
    const char *kFullRightCornerBracket = "」";
    const char *kFullSlash = "／";
    const char *kFullMiddleDot = "・";

    {
      SCOPED_TRACE("CORNER_BRACKET_MIDDLE_DOT");
      config::Config config;
      config.set_symbol_method(config::Config::CORNER_BRACKET_MIDDLE_DOT);
      table->ReloadConfig(config);

      TestKanaTransformation(kFullLeftCornerBracket, '[',
                             kFullLeftCornerBracket);
      TestKanaTransformation(kFullRightCornerBracket, ']',
                             kFullRightCornerBracket);
      TestKanaTransformation(kFullMiddleDot, '/', kFullMiddleDot);
    }
    {
      SCOPED_TRACE("SQUARE_BRACKET_SLASH");
      config::Config config;
      config.set_symbol_method(config::Config::SQUARE_BRACKET_SLASH);
      table->ReloadConfig(config);

      TestKanaTransformation(kFullLeftCornerBracket, '[',
                             kFullLeftSquareBracket);
      TestKanaTransformation(kFullRightCornerBracket, ']',
                             kFullRightSquareBracket);
      TestKanaTransformation(kFullMiddleDot, '/', kFullSlash);
    }
    {
      SCOPED_TRACE("CORNER_BRACKET_SLASH");
      config::Config config;
      config.set_symbol_method(config::Config::CORNER_BRACKET_SLASH);
      table->ReloadConfig(config);

      TestKanaTransformation(kFullLeftCornerBracket, '[',
                             kFullLeftCornerBracket);
      TestKanaTransformation(kFullRightCornerBracket, ']',
                             kFullRightCornerBracket);
      TestKanaTransformation(kFullMiddleDot, '/', kFullSlash);
    }
    {
      SCOPED_TRACE("SQUARE_BRACKET_MIDDLE_DOT");
      config::Config config;
      config.set_symbol_method(config::Config::SQUARE_BRACKET_MIDDLE_DOT);
      table->ReloadConfig(config);

      TestKanaTransformation(kFullLeftCornerBracket, '[',
                             kFullLeftSquareBracket);
      TestKanaTransformation(kFullRightCornerBracket, ']',
                             kFullRightSquareBracket);
      TestKanaTransformation(kFullMiddleDot, '/', kFullMiddleDot);
    }
  }
}

TEST(KeyEventTransformerTest, Copy) {
  KeyEventTransformer x;

  config::Config config;
  config.set_punctuation_method(config::Config::COMMA_PERIOD);
  x.ReloadConfig(config);

  const KeyEventTransformer y(x);
  EXPECT_TRUE(IsKeyEventTransformerEq(x, y));

  KeyEventTransformer z;
  EXPECT_FALSE(IsKeyEventTransformerEq(x, z));
  z = x;
  EXPECT_TRUE(IsKeyEventTransformerEq(x, y));
}

}  // namespace session
}  // namespace mozc
