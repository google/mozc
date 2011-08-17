// Copyright 2010-2011, Google Inc.
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

#include "session/ime_switch_util.h"

#include <string>

#include "base/util.h"
#include "config/config_handler.h"
#include "session/key_event_normalizer.h"
#include "session/key_parser.h"
#include "session/internal/keymap.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/googletest.h"

namespace mozc {
namespace config {

class ImeSwitchUtilTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ImeSwitchUtil::Reload();
    Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }
};

TEST_F(ImeSwitchUtilTest, PresetTest) {
  Config config;
  ConfigHandler::GetConfig(&config);
  config.set_session_keymap(Config::ATOK);
  ConfigHandler::SetConfig(config);
  ImeSwitchUtil::Reload();
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    // Reconcersion
    commands::KeyEvent key;
    KeyParser::ParseKey("Shift HENKAN", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }

  config.set_session_keymap(Config::MSIME);
  ConfigHandler::SetConfig(config);
  ImeSwitchUtil::Reload();
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }

  config.set_session_keymap(Config::KOTOERI);
  ConfigHandler::SetConfig(config);
  ImeSwitchUtil::Reload();
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    // Reconcersion
    commands::KeyEvent key;
    KeyParser::ParseKey("Ctrl Shift r", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
}

TEST_F(ImeSwitchUtilTest, DefaultTest) {
  Config config;
  ConfigHandler::GetConfig(&config);
  config.set_session_keymap(Config::NONE);
  ConfigHandler::SetConfig(config);
  ImeSwitchUtil::Reload();
  // MSIME for windows, KOTOERI for others
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    // HENKAN key in MSIME is TurnOn key while it's not in KOTOERI.
    if (keymap::KeyMapManager::GetDefaultKeyMap() == config::Config::MSIME) {
      EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
    } else {
      EXPECT_FALSE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
    }
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    if (keymap::KeyMapManager::GetDefaultKeyMap() == config::Config::MSIME) {
      EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
    } else {
      EXPECT_FALSE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
    }
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    key.set_special_key(commands::KeyEvent::ON);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
}

TEST_F(ImeSwitchUtilTest, CustomTest) {
  Config config;
  ConfigHandler::GetConfig(&config);

  const string custom_keymap_table =
      "status\tkey\tcommand\n"
      "DirectInput\tCtrl j\tIMEOn\n"
      "DirectInput\tHenkan\tIMEOn\n"
      "DirectInput\tCtrl k\tIMEOff\n"
      "Precomposition\tCtrl l\tIMEOn\n";

  config.set_session_keymap(Config::CUSTOM);
  config.set_custom_keymap_table(custom_keymap_table);
  ConfigHandler::SetConfig(config);
  ImeSwitchUtil::Reload();
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl j", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl k", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl l", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsTurnOnInDirectMode(key));
  }
}

namespace {
  bool IsIncludedKeyEvent(const commands::KeyEvent &key_event,
                          const vector<commands::KeyEvent> &key_event_list) {
    uint64 key;
    if (!KeyEventNormalizer::ToUint64(key_event, &key)) {
      return false;
    }
    for (size_t i = 0; i < key_event_list.size(); ++i) {
      uint64 listed_key;
      if (!KeyEventNormalizer::ToUint64(key_event_list[i], &listed_key)) {
        return false;
      }
      if (key == listed_key) {
        return true;
      }
    }
    return false;
  }
}

TEST_F(ImeSwitchUtilTest, GetKeyEventListTest) {
  Config config;
  ConfigHandler::GetConfig(&config);

  const string custom_keymap_table =
      "status\tkey\tcommand\n"
      "DirectInput\tCtrl j\tIMEOn\n"
      "DirectInput\tHenkan\tIMEOn\n"
      "DirectInput\tCtrl k\tIMEOff\n"
      "Precomposition\tCtrl l\tIMEOn\n";

  config.set_session_keymap(Config::CUSTOM);
  config.set_custom_keymap_table(custom_keymap_table);
  ConfigHandler::SetConfig(config);
  ImeSwitchUtil::Reload();
  vector<commands::KeyEvent> key_event_list;
  ImeSwitchUtil::GetTurnOnInDirectModeKeyEventList(&key_event_list);
  EXPECT_EQ(2, key_event_list.size());
  {
    commands::KeyEvent key_event;
    KeyParser::ParseKey("HENKAN", &key_event);
    EXPECT_TRUE(IsIncludedKeyEvent(key_event, key_event_list));
  }
  {
    commands::KeyEvent key_event;
    KeyParser::ParseKey("EISU", &key_event);
    EXPECT_FALSE(IsIncludedKeyEvent(key_event, key_event_list));
  }
  {
    commands::KeyEvent key_event;
    KeyParser::ParseKey("ctrl j", &key_event);
    EXPECT_TRUE(IsIncludedKeyEvent(key_event, key_event_list));
  }
  {
    commands::KeyEvent key_event;
    KeyParser::ParseKey("ctrl k", &key_event);
    EXPECT_FALSE(IsIncludedKeyEvent(key_event, key_event_list));
  }
  {
    commands::KeyEvent key_event;
    KeyParser::ParseKey("ctrl l", &key_event);
    EXPECT_FALSE(IsIncludedKeyEvent(key_event, key_event_list));
  }
}

TEST_F(ImeSwitchUtilTest, MigrationTest) {
  Config config;
  ConfigHandler::GetConfig(&config);

  const string custom_keymap_table =
      "status\tkey\tcommand\n"
      "DirectInput\tON\tIMEOn\n";
  config.set_session_keymap(Config::CUSTOM);
  config.set_custom_keymap_table(custom_keymap_table);
  ConfigHandler::SetConfig(config);
  ImeSwitchUtil::Reload();
  vector<commands::KeyEvent> key_event_list;
  ImeSwitchUtil::GetTurnOnInDirectModeKeyEventList(&key_event_list);
  EXPECT_EQ(3, key_event_list.size());
  {
    commands::KeyEvent key_event;
    KeyParser::ParseKey("ON", &key_event);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key_event));
    EXPECT_TRUE(IsIncludedKeyEvent(key_event, key_event_list));
  }
  {
    commands::KeyEvent key_event;
    KeyParser::ParseKey("Hankaku/Zenkaku", &key_event);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key_event));
    EXPECT_TRUE(IsIncludedKeyEvent(key_event, key_event_list));
  }
  {
    commands::KeyEvent key_event;
    KeyParser::ParseKey("Kanji", &key_event);
    EXPECT_TRUE(ImeSwitchUtil::IsTurnOnInDirectMode(key_event));
    EXPECT_TRUE(IsIncludedKeyEvent(key_event, key_event_list));
  }
}
}  // namespace config
}  // namespace mozc
