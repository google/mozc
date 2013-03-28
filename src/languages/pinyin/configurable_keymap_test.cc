// Copyright 2010-2013, Google Inc.
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

#include "languages/pinyin/configurable_keymap.h"

#include <string>

#include "base/system_util.h"
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

namespace  {
bool GetKeyCommand(const string &key_string, ConverterState state,
                   KeyCommand *key_command) {
  commands::KeyEvent key_event;
  EXPECT_TRUE(KeyParser::ParseKey(key_string, &key_event));
  return ConfigurableKeymap::GetCommand(key_event, state, key_command);
}
}  // namespace

class ConfigurableKeymapTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }
};

TEST_F(ConfigurableKeymapTest, DoublePinyin) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);

  KeyCommand key_command;

  {
    config.mutable_pinyin_config()->set_double_pinyin(true);
    config::ConfigHandler::SetConfig(config);

    ASSERT_FALSE(GetKeyCommand("v", ACTIVE, &key_command));
    ASSERT_FALSE(GetKeyCommand("v", INACTIVE, &key_command));
  }

  {
    config.mutable_pinyin_config()->set_double_pinyin(false);
    config::ConfigHandler::SetConfig(config);

    ASSERT_FALSE(GetKeyCommand("v", ACTIVE, &key_command));
    ASSERT_TRUE(GetKeyCommand("v", INACTIVE, &key_command));
    EXPECT_EQ(TURN_ON_ENGLISH_MODE, key_command);
  }
}

TEST_F(ConfigurableKeymapTest, SelectWithShift) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);

  {
    config.mutable_pinyin_config()->set_select_with_shift(true);
    config::ConfigHandler::SetConfig(config);

    KeyCommand key_command;

    ASSERT_TRUE(GetKeyCommand("LeftShift", ACTIVE, &key_command));
    EXPECT_EQ(SELECT_SECOND_CANDIDATE, key_command);
    ASSERT_TRUE(GetKeyCommand("RightShift", ACTIVE, &key_command));
    EXPECT_EQ(SELECT_THIRD_CANDIDATE, key_command);

    EXPECT_FALSE(GetKeyCommand("Alt", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt LeftShift", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl LeftShift", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl LeftShift", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("LeftShift", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt LeftShift", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl LeftShift", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl LeftShift", INACTIVE, &key_command));

    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("LeftShift BS", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("LeftShift BS", INACTIVE, &key_command));
  }

  {
    config.mutable_pinyin_config()->set_select_with_shift(false);
    config::ConfigHandler::SetConfig(config);

    KeyCommand key_command;

    EXPECT_FALSE(GetKeyCommand("Alt", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Shift", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift", ACTIVE, &key_command));

    EXPECT_FALSE(GetKeyCommand("Alt", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Shift", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift", INACTIVE, &key_command));

    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("LeftShift BS", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("LeftShift BS", INACTIVE, &key_command));
  }
}

TEST_F(ConfigurableKeymapTest, PagingWithMinusEqual) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);

  {
    config.mutable_pinyin_config()->set_paging_with_minus_equal(true);
    config::ConfigHandler::SetConfig(config);

    KeyCommand key_command;

    ASSERT_TRUE(GetKeyCommand("-", ACTIVE, &key_command));
    EXPECT_EQ(FOCUS_CANDIDATE_PREV_PAGE, key_command);

    ASSERT_TRUE(GetKeyCommand("=", ACTIVE, &key_command));
    EXPECT_EQ(FOCUS_CANDIDATE_NEXT_PAGE, key_command);

    EXPECT_FALSE(GetKeyCommand("Alt =", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl =", ACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl =", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift =", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift =", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift =", ACTIVE, &key_command));

    EXPECT_FALSE(GetKeyCommand("Alt =", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl =", INACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl =", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift =", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift =", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift =", INACTIVE, &key_command));
  }

  {
    config.mutable_pinyin_config()->set_paging_with_minus_equal(false);
    config::ConfigHandler::SetConfig(config);

    KeyCommand key_command;

    EXPECT_FALSE(GetKeyCommand("Alt =", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl =", ACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl =", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift =", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift =", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift =", ACTIVE, &key_command));

    EXPECT_FALSE(GetKeyCommand("Alt =", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl =", INACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl =", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift =", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift =", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift =", INACTIVE, &key_command));
  }
}

TEST_F(ConfigurableKeymapTest, PagingWithCommaPeriod) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);

  {
    config.mutable_pinyin_config()->set_paging_with_comma_period(true);
    config::ConfigHandler::SetConfig(config);

    KeyCommand key_command;

    ASSERT_TRUE(GetKeyCommand(",", ACTIVE, &key_command));
    EXPECT_EQ(FOCUS_CANDIDATE_PREV_PAGE, key_command);

    ASSERT_TRUE(GetKeyCommand(".", ACTIVE, &key_command));
    EXPECT_EQ(FOCUS_CANDIDATE_NEXT_PAGE, key_command);

    EXPECT_FALSE(GetKeyCommand("Alt .", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl .", ACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl .", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift .", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift .", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift .", ACTIVE, &key_command));

    EXPECT_FALSE(GetKeyCommand("Alt .", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl .", INACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl .", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift .", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift .", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift .", INACTIVE, &key_command));
  }

  {
    config.mutable_pinyin_config()->set_paging_with_comma_period(false);
    config::ConfigHandler::SetConfig(config);

    KeyCommand key_command;

    EXPECT_FALSE(GetKeyCommand("Alt .", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl .", ACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl .", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift .", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift .", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift .", ACTIVE, &key_command));

    EXPECT_FALSE(GetKeyCommand("Alt .", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl .", INACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl .", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift .", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift .", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift .", INACTIVE, &key_command));
  }
}

TEST_F(ConfigurableKeymapTest, AutoCommit) {
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);

  {
    config.mutable_pinyin_config()->set_auto_commit(true);
    config::ConfigHandler::SetConfig(config);

    KeyCommand key_command;

    ASSERT_TRUE(GetKeyCommand("!", ACTIVE, &key_command));
    EXPECT_EQ(AUTO_COMMIT, key_command);

    EXPECT_FALSE(GetKeyCommand("Alt !", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl !", ACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl !", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift !", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift !", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift !", ACTIVE, &key_command));

    ASSERT_FALSE(GetKeyCommand("!", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt !", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl !", INACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl !", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift !", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift !", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift !", INACTIVE, &key_command));

    ASSERT_FALSE(GetKeyCommand("a", ACTIVE, &key_command));
    ASSERT_FALSE(GetKeyCommand("a", INACTIVE, &key_command));
  }

  {
    config.mutable_pinyin_config()->set_auto_commit(false);
    config::ConfigHandler::SetConfig(config);

    KeyCommand key_command;

    ASSERT_FALSE(GetKeyCommand("!", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt !", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl !", ACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl !", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift !", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift !", ACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift !", ACTIVE, &key_command));

    ASSERT_FALSE(GetKeyCommand("!", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt !", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl !", INACTIVE, &key_command));
    // "Shift + printable key" should not be sent from mozc_engine.
    // Shift key with a printable key is removed.
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl !", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Shift !", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Ctrl Shift !", INACTIVE, &key_command));
    EXPECT_FALSE(GetKeyCommand("Alt Ctrl Shift !", INACTIVE, &key_command));

    ASSERT_FALSE(GetKeyCommand("a", ACTIVE, &key_command));
    ASSERT_FALSE(GetKeyCommand("a", INACTIVE, &key_command));
  }
}

}  // namespace keymap
}  // namespace pinyin
}  // namespace mozc
