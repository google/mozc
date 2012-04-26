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

#include "languages/pinyin/keymap.h"

#include <string>

#include "base/port.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "session/key_parser.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace pinyin {
namespace keymap {

namespace {
testing::AssertionResult CheckKeyCommand(const char *expected_command_expr,
                                         const char *keymap_expr,
                                         const char *keys_expr,
                                         KeyCommand expected_command,
                                         const KeymapInterface *keymap,
                                         const string &keys) {
  commands::KeyEvent key_event;
  if (!KeyParser::ParseKey(keys, &key_event)) {
    return testing::AssertionFailure() <<
        Util::StringPrintf("Failed to parse keys.\n"
                           "keys: %s (%s)",
                           keys.c_str(), keys_expr);
  }

  KeyCommand key_command;
  // It is enough that we test ACTIVE state only because
  // - PinyinKeymap is simply calls DefaultKeymap and ConfigurableKeymap and
  //   has no complex logic.
  // - Other keymaps doesn't consider any state.
  if (!keymap->GetCommand(key_event, ACTIVE, &key_command)) {
    return testing::AssertionFailure() <<
        Util::StringPrintf("Failed to get command.\n"
                           "keys: %s (%s)", keys.c_str(), keys_expr);
  }

  if (expected_command != key_command) {
    return testing::AssertionFailure() <<
        Util::StringPrintf("KeyCommand is not valid.\n"
                           "Expected: %d\n"
                           "Actual: %d",
                           expected_command, key_command);
  }

  return testing::AssertionSuccess();
}

#define EXPECT_KEY_COMMAND(expected_command, keymap, keys) \
  EXPECT_PRED_FORMAT3(CheckKeyCommand, expected_command, keymap, keys)
}  // namespace

class KeymapTest : public testing::Test {
 protected:
  virtual void SetUp() {
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config.mutable_pinyin_config()->set_paging_with_minus_equal(true);
    config.mutable_pinyin_config()->set_double_pinyin(false);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }
};

TEST_F(KeymapTest, PinyinKeymap) {
  const KeymapInterface *keymap = KeymapFactory::GetKeymap(PINYIN);

  {
    SCOPED_TRACE("Configurable keymap");
    EXPECT_KEY_COMMAND(FOCUS_CANDIDATE_PREV_PAGE, keymap, "-");
  }

  {
    SCOPED_TRACE("Default keymap");
    EXPECT_KEY_COMMAND(INSERT, keymap, "a");
  }
}

namespace {
const struct DirectKeymapTestData {
  string keys;
  KeyCommand command;
} kDirectKeymapTestData[] = {
  { "-",            INSERT },
  { "a",            INSERT },
  { "!",            INSERT },
  { "0",            INSERT },
  { "A",            INSERT },
  // "Shift + printable key" should not be sent from mozc_engine.
  // Shift key with a printable key is removed.
  { "Ctrl a",       DO_NOTHING_WITHOUT_CONSUME },
  { "Left",         DO_NOTHING_WITHOUT_CONSUME },
  { "Ctrl Left",    DO_NOTHING_WITHOUT_CONSUME },
  { "Home",         DO_NOTHING_WITHOUT_CONSUME },
  { "Ctrl",         DO_NOTHING_WITHOUT_CONSUME },
  { "Enter",        DO_NOTHING_WITHOUT_CONSUME },
  { "Shift",        TOGGLE_DIRECT_MODE },
  { "v",            INSERT },
  { "`",            INSERT },
  { "Ctrl Shift f", TOGGLE_SIMPLIFIED_CHINESE_MODE },
};
}  // namespace

TEST_F(KeymapTest, DirectKeymap) {
  const KeymapInterface *keymap = KeymapFactory::GetKeymap(DIRECT);

  for (size_t i = 0; i < ARRAYSIZE(kDirectKeymapTestData); ++i) {
    const DirectKeymapTestData &data = kDirectKeymapTestData[i];
    SCOPED_TRACE(data.keys);
    EXPECT_KEY_COMMAND(data.command, keymap, data.keys);
  }
}

namespace {
const struct EnglishKeymapTestData {
  string keys;
  KeyCommand command;
} kEnglishKeymapTestData[] = {
  { "-",            FOCUS_CANDIDATE_PREV_PAGE },
  { "a",            INSERT },
  { "!",            DO_NOTHING_WITH_CONSUME },
  { "A",            INSERT },
  // "Shift + printable key" should not be sent from mozc_engine.
  // Shift key with a printable key is removed.
  { "Ctrl a",       DO_NOTHING_WITH_CONSUME },
  { "Left",         FOCUS_CANDIDATE_TOP },
  { "Right",        FOCUS_CANDIDATE_TOP },
  { "Ctrl Left",    FOCUS_CANDIDATE_TOP },
  { "Ctrl Right",   FOCUS_CANDIDATE_TOP },
  { "Home",         FOCUS_CANDIDATE_TOP },
  { "End",          FOCUS_CANDIDATE_TOP },
  { "SHIFT",        DO_NOTHING_WITHOUT_CONSUME },
  { "Ctrl BS",      DO_NOTHING_WITHOUT_CONSUME },
  { "Ctrl Delete",  DO_NOTHING_WITHOUT_CONSUME },
  { "v",            INSERT },
  { "`",            DO_NOTHING_WITH_CONSUME },
  { "Ctrl Shift f", TOGGLE_SIMPLIFIED_CHINESE_MODE },
};
}  // namespace

TEST_F(KeymapTest, EnglishKeymap) {
  const KeymapInterface *keymap = KeymapFactory::GetKeymap(ENGLISH);

  for (size_t i = 0; i < ARRAYSIZE(kEnglishKeymapTestData); ++i) {
    const EnglishKeymapTestData &data = kEnglishKeymapTestData[i];
    SCOPED_TRACE(data.keys);
    EXPECT_KEY_COMMAND(data.command, keymap, data.keys);
  }
}

namespace {
const struct PunctuationKeymapTestData {
  string keys;
  KeyCommand command;
} kPunctuationKeymapTestData[] = {
  { "-",            INSERT },
  { "a",            INSERT },
  { "!",            INSERT },
  { "0",            INSERT },
  { "A",            INSERT },
  // "Shift + printable key" should not be sent from mozc_engine.
  // Shift key with a printable key is removed.
  { "Ctrl a",       DO_NOTHING_WITH_CONSUME },
  { "Left",         MOVE_CURSOR_LEFT },
  { "SHIFT",        DO_NOTHING_WITHOUT_CONSUME },
  { "ENTER",        COMMIT_PREEDIT },
  { "v",            INSERT },
  { "`",            INSERT },
  { "Ctrl Shift f", TOGGLE_SIMPLIFIED_CHINESE_MODE },
};
}  // namespace

TEST_F(KeymapTest, PunctuationKeymap) {
  const KeymapInterface *keymap = KeymapFactory::GetKeymap(PUNCTUATION);

  for (size_t i = 0; i < ARRAYSIZE(kPunctuationKeymapTestData); ++i) {
    const PunctuationKeymapTestData &data = kPunctuationKeymapTestData[i];
    SCOPED_TRACE(data.keys);
    EXPECT_KEY_COMMAND(data.command, keymap, data.keys);
  }
}

}  // namespace keymap
}  // namespace pinyin
}  // namespace mozc
