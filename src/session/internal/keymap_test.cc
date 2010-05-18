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

// Keymap utils of Mozc interface.

#include <sstream>
#include "base/config_file_stream.h"
#include "session/commands.pb.h"
#include "session/config.pb.h"
#include "session/config_handler.h"
#include "session/internal/keymap.h"
#include "session/internal/keymap-inl.h"
#include "session/key_parser.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace keymap {

TEST(KeyMap, AddRule) {
  KeyMap<PrecompositionState::Commands> keymap;
  commands::KeyEvent key_event;
  // 'a'
  key_event.set_key_code(97);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 0 is treated as null input
  key_event.set_key_code(0);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 1 to 31 should be rejected.
  key_event.set_key_code(1);
  EXPECT_FALSE(keymap.AddRule(key_event,
                              PrecompositionState::INSERT_CHARACTER));
  key_event.set_key_code(31);
  EXPECT_FALSE(keymap.AddRule(key_event,
                              PrecompositionState::INSERT_CHARACTER));

  // 32 (space) is also invalid input.
  key_event.set_key_code(32);
  EXPECT_FALSE(keymap.AddRule(key_event,
                              PrecompositionState::INSERT_CHARACTER));

  // 33 (!) is a valid input.
  key_event.set_key_code(33);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 8bit char is a valid input.
  key_event.set_key_code(127);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));
  key_event.set_key_code(255);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));
}

TEST(KeyMap, GetCommand) {
  {
    KeyMap<PrecompositionState::Commands> keymap;
    commands::KeyEvent init_key_event;
    init_key_event.set_key_code(97);
    EXPECT_TRUE(keymap.AddRule(init_key_event,
                               PrecompositionState::INSERT_CHARACTER));

    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(PrecompositionState::INSERT_CHARACTER, command);

    key_event.Clear();
    key_event.set_key_code(98);
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }
  {
    KeyMap<CompositionState::Commands> keymap;
    commands::KeyEvent init_key_event;
    init_key_event.set_key_code(97);
    EXPECT_TRUE(keymap.AddRule(init_key_event,
                               CompositionState::INSERT_CHARACTER));

    commands::KeyEvent key_event;
    CompositionState::Commands command;
    key_event.set_key_code(97);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(CompositionState::INSERT_CHARACTER, command);

    key_event.Clear();
    key_event.set_key_code(98);
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }
  {
    KeyMap<CompositionState::Commands> keymap;
    commands::KeyEvent init_key_event;
    init_key_event.set_special_key(commands::KeyEvent::ENTER);
    EXPECT_TRUE(keymap.AddRule(init_key_event, CompositionState::COMMIT));

    init_key_event.Clear();
    init_key_event.set_special_key(commands::KeyEvent::DEL);
    init_key_event.add_modifier_keys(commands::KeyEvent::CTRL);
    init_key_event.add_modifier_keys(commands::KeyEvent::ALT);
    EXPECT_TRUE(keymap.AddRule(init_key_event, CompositionState::IME_OFF));

    commands::KeyEvent key_event;
    CompositionState::Commands command;

    // ENTER
    key_event.Clear();
    key_event.set_special_key(commands::KeyEvent::ENTER);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(CompositionState::COMMIT, command);

    // CTRL-ALT-DELETE
    key_event.Clear();
    key_event.set_special_key(commands::KeyEvent::DEL);
    key_event.add_modifier_keys(commands::KeyEvent::CTRL);
    key_event.add_modifier_keys(commands::KeyEvent::ALT);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(CompositionState::IME_OFF, command);
  }
}

TEST(KeyMap, GetCommandKeyStub) {
  KeyMap<PrecompositionState::Commands> keymap;
  commands::KeyEvent init_key_event;
  init_key_event.set_special_key(commands::KeyEvent::ASCII);
  EXPECT_TRUE(keymap.AddRule(init_key_event,
                             PrecompositionState::INSERT_CHARACTER));

  PrecompositionState::Commands command;
  commands::KeyEvent key_event;
  key_event.set_key_code(97);
  EXPECT_TRUE(keymap.GetCommand(key_event, &command));
  EXPECT_EQ(PrecompositionState::INSERT_CHARACTER, command);
}

TEST(KeyMap, GetKeyMapFileName) {
  EXPECT_STREQ("system://atok.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::ATOK));
  EXPECT_STREQ("system://ms-ime.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::MSIME));
  EXPECT_STREQ("system://kotoeri.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::KOTOERI));
  EXPECT_STREQ("user://keymap.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::CUSTOM));
}

TEST(KeyMap, DefaultKeyBindings) {
  KeyMapManager manager;

  istringstream iss("", istringstream::in);
  EXPECT_TRUE(manager.LoadStream(&iss));

  {  // Check key bindings of ASCII.
    commands::KeyEvent key_event;
    KeyParser::ParseKey("ASCII", &key_event);

    PrecompositionState::Commands fund_command;
    EXPECT_TRUE(manager.GetCommandPrecomposition(key_event, &fund_command));
    EXPECT_EQ(PrecompositionState::INSERT_CHARACTER, fund_command);

    CompositionState::Commands composition_command;
    EXPECT_TRUE(manager.GetCommandComposition(key_event, &composition_command));
    EXPECT_EQ(CompositionState::INSERT_CHARACTER, composition_command);

    ConversionState::Commands conv_command;
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::INSERT_CHARACTER, conv_command);
  }

  {  // Check key bindings of Shift.
    commands::KeyEvent key_event;
    KeyParser::ParseKey("Shift", &key_event);

    PrecompositionState::Commands fund_command;
    EXPECT_FALSE(manager.GetCommandPrecomposition(key_event, &fund_command));

    CompositionState::Commands composition_command;
    EXPECT_TRUE(manager.GetCommandComposition(key_event, &composition_command));
    EXPECT_EQ(CompositionState::INSERT_CHARACTER, composition_command);

    ConversionState::Commands conv_command;
    EXPECT_FALSE(manager.GetCommandConversion(key_event, &conv_command));
  }
}

TEST(KeyMap, LoadStreamWithErrors) {
  KeyMapManager manager;
  vector<string> errors;
  scoped_ptr<istream> is(ConfigFileStream::Open("system://atok.tsv"));
  EXPECT_TRUE(manager.LoadStreamWithErrors(is.get(), &errors));
  EXPECT_TRUE(errors.empty());

  errors.clear();
  is.reset(ConfigFileStream::Open("system://ms-ime.tsv"));
  EXPECT_TRUE(manager.LoadStreamWithErrors(is.get(), &errors));
#ifdef OS_WINDOWS
  // Now we have invalid commands for Mac
  // Input mode change commands are not allowed on Mac.
  EXPECT_TRUE(errors.empty());
#else
  EXPECT_FALSE(errors.empty());
#endif  // OS_WINDOWS

  errors.clear();
  is.reset(ConfigFileStream::Open("system://kotoeri.tsv"));
  EXPECT_TRUE(manager.LoadStreamWithErrors(is.get(), &errors));
  EXPECT_TRUE(errors.empty());
}

TEST(KeyMap, MigrationTest) {
  Util::SetUserProfileDirectory(FLAGS_test_tmpdir);
  config::Config config;
  config.CopyFrom(config::ConfigHandler::GetConfig());

  string test_keymap;
  ostringstream oss(test_keymap);
  oss << endl;  // first line is reverved for comment and will be skipped
  oss << "DirectInput\tON\tIMEOn" << endl;
  oss << "Conversion\tOFF\tIMEOff" << endl;
  oss << "Precomposition\tOFF\tIMEOff" << endl;
  oss << "Composition\tOFF\tIMEOff" << endl;
  config.set_session_keymap(config::Config::CUSTOM);
  config.set_custom_keymap_table(oss.str());

  config::ConfigHandler::SetConfig(config);
  KeyMapManager manager;
  manager.Reload();

  {  // Check key bindings of Hankaku
    commands::KeyEvent key_event;
    KeyParser::ParseKey("Hankaku/Zenkaku", &key_event);

    DirectInputState::Commands direct_command;
    EXPECT_TRUE(manager.GetCommandDirect(key_event, &direct_command));
    EXPECT_EQ(DirectInputState::IME_ON, direct_command);

    PrecompositionState::Commands fund_command;
    EXPECT_TRUE(manager.GetCommandPrecomposition(key_event, &fund_command));
    EXPECT_EQ(PrecompositionState::IME_OFF, fund_command);

    CompositionState::Commands composition_command;
    EXPECT_TRUE(manager.GetCommandComposition(key_event, &composition_command));
    EXPECT_EQ(CompositionState::IME_OFF, composition_command);

    ConversionState::Commands conv_command;
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::IME_OFF, conv_command);
  }

  const string &keymap_table = GET_CONFIG(custom_keymap_table);
  manager.Reload();

  // Nothing happen
  const string &keymap_table_new = GET_CONFIG(custom_keymap_table);
  EXPECT_EQ(keymap_table, keymap_table_new);
}

}  // namespace keymap
}  // namespace mozc
