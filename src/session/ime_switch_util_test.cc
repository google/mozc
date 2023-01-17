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

#include "session/ime_switch_util.h"

#include <string>

#include "base/system_util.h"
#include "composer/key_parser.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/internal/keymap.h"
#include "testing/googletest.h"
#include "testing/gunit.h"

namespace mozc {
namespace config {

TEST(ImeSwitchUtilTest, PresetTest) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  config.set_session_keymap(Config::ATOK);
  ImeSwitchUtil::ReloadConfig(config);
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    // Reconcersion
    commands::KeyEvent key;
    KeyParser::ParseKey("Shift HENKAN", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }

  config.set_session_keymap(Config::MSIME);
  ImeSwitchUtil::ReloadConfig(config);
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }

  config.set_session_keymap(Config::KOTOERI);
  ImeSwitchUtil::ReloadConfig(config);
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    // Reconcersion
    commands::KeyEvent key;
    KeyParser::ParseKey("Ctrl Shift r", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
}

TEST(ImeSwitchUtilTest, DefaultTest) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  config.set_session_keymap(Config::NONE);
  ImeSwitchUtil::ReloadConfig(config);
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    // HENKAN key in MSIME is TurnOn key while it's not in KOTOERI.
    if (ConfigHandler::GetDefaultKeyMap() == config::Config::MSIME) {
      EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
    } else {
      EXPECT_FALSE(ImeSwitchUtil::IsDirectModeCommand(key));
    }
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    if (ConfigHandler::GetDefaultKeyMap() == config::Config::MSIME) {
      EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
    } else {
      EXPECT_FALSE(ImeSwitchUtil::IsDirectModeCommand(key));
    }
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ON", &key);
    key.set_special_key(commands::KeyEvent::ON);
    if (ConfigHandler::GetDefaultKeyMap() == config::Config::CHROMEOS) {
      EXPECT_FALSE(ImeSwitchUtil::IsDirectModeCommand(key));
    } else {
      EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
    }
  }
}

TEST(ImeSwitchUtilTest, CustomTest) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);

  const std::string custom_keymap_table =
      "status\tkey\tcommand\n"
      "DirectInput\tHenkan\tIMEOn\n"
      "DirectInput\tCtrl j\tIMEOn\n"
      "DirectInput\tCtrl k\tIMEOff\n"
      "DirectInput\tCtrl l\tLaunchWordRegisterDialog\n"
      "Precomposition\tCtrl m\tIMEOn\n";

  config.set_session_keymap(Config::CUSTOM);
  config.set_custom_keymap_table(custom_keymap_table);
  ImeSwitchUtil::ReloadConfig(config);
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("HENKAN", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("EISU", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl j", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl k", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl l", &key);
    EXPECT_TRUE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
  {
    commands::KeyEvent key;
    KeyParser::ParseKey("ctrl m", &key);
    EXPECT_FALSE(ImeSwitchUtil::IsDirectModeCommand(key));
  }
}

}  // namespace config
}  // namespace mozc
