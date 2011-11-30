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

// Keymap utils of Mozc interface.

#include <sstream>
#include <vector>
#include "base/config_file_stream.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "session/internal/keymap.h"
#include "session/internal/keymap-inl.h"
#include "session/internal/keymap_factory.h"
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
  EXPECT_TRUE(errors.empty());

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

  // Nothing happen
  const string &keymap_table_new = GET_CONFIG(custom_keymap_table);
  EXPECT_EQ(keymap_table, keymap_table_new);
}

TEST(KeyMap, GetName) {
  KeyMapManager manager;
  {
    // Direct
    string name;
    EXPECT_TRUE(manager.GetNameFromCommandDirect(DirectInputState::IME_ON,
                                                 &name));
    EXPECT_EQ("IMEOn", name);
    EXPECT_TRUE(manager.GetNameFromCommandDirect(DirectInputState::RECONVERT,
                                                 &name));
    EXPECT_EQ("Reconvert", name);
  }
  {
    // Precomposition
    string name;
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::IME_OFF, &name));
    EXPECT_EQ("IMEOff", name);
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::IME_ON, &name));
    EXPECT_EQ("IMEOn", name);
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::INSERT_CHARACTER, &name));
    EXPECT_EQ("InsertCharacter", name);
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::RECONVERT, &name));
    EXPECT_EQ("Reconvert", name);
  }
  {
    // Composition
    string name;
    EXPECT_TRUE(manager.GetNameFromCommandComposition(
        CompositionState::IME_OFF, &name));
    EXPECT_EQ("IMEOff", name);
    EXPECT_TRUE(manager.GetNameFromCommandComposition(
        CompositionState::IME_ON, &name));
    EXPECT_EQ("IMEOn", name);
    EXPECT_TRUE(manager.GetNameFromCommandComposition(
        CompositionState::INSERT_CHARACTER, &name));
    EXPECT_EQ("InsertCharacter", name);
  }
  {
    // Conversion
    string name;
    EXPECT_TRUE(manager.GetNameFromCommandConversion(
        ConversionState::IME_OFF, &name));
    EXPECT_EQ("IMEOff", name);
    EXPECT_TRUE(manager.GetNameFromCommandConversion(
        ConversionState::IME_ON, &name));
    EXPECT_EQ("IMEOn", name);
    EXPECT_TRUE(manager.GetNameFromCommandConversion(
        ConversionState::INSERT_CHARACTER, &name));
    EXPECT_EQ("InsertCharacter", name);
  }
}

TEST(KeyMap, AdditionalKeymapsForChromeOS) {
  // Additional MSIME key maps for Chrome OS
  KeyMapManager *manager =
      KeyMapFactory::GetKeyMapManager(config::Config::MSIME);

  // Precomposition state
  PrecompositionState::Commands fund_command;
  commands::KeyEvent key_event;
  KeyParser::ParseKey("Ctrl Eisu", &key_event);
  EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &fund_command));
  EXPECT_EQ(PrecompositionState::TOGGLE_ALPHANUMERIC_MODE, fund_command);

  // Composition state
  CompositionState::Commands composition_command;
  KeyParser::ParseKey("Ctrl 0", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_HALF_ALPHANUMERIC,
            composition_command);

  KeyParser::ParseKey("Ctrl 2", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_WITHOUT_HISTORY, composition_command);

  KeyParser::ParseKey("Ctrl 6", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_HIRAGANA, composition_command);

  KeyParser::ParseKey("Ctrl 7", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_FULL_KATAKANA, composition_command);

  KeyParser::ParseKey("Ctrl 8", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_HALF_WIDTH, composition_command);

  KeyParser::ParseKey("Ctrl 9", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_FULL_ALPHANUMERIC,
            composition_command);

  KeyParser::ParseKey("Ctrl Eisu", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::TOGGLE_ALPHANUMERIC_MODE, composition_command);

  // Conversion state
  ConversionState::Commands conv_command;
  KeyParser::ParseKey("Ctrl 0", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_TO_HALF_ALPHANUMERIC, conv_command);

  KeyParser::ParseKey("Ctrl 6", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_TO_HIRAGANA, conv_command);

  KeyParser::ParseKey("Ctrl 7", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_TO_FULL_KATAKANA, conv_command);

  KeyParser::ParseKey("Ctrl 8", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_TO_HALF_WIDTH, conv_command);

  KeyParser::ParseKey("Ctrl 9", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_TO_FULL_ALPHANUMERIC, conv_command);

  KeyParser::ParseKey("Ctrl Eisu", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::TOGGLE_ALPHANUMERIC_MODE, conv_command);


  // Additional Kotoeri key maps for Chrome OS
  manager = KeyMapFactory::GetKeyMapManager(config::Config::KOTOERI);

  // Precomposition state
  KeyParser::ParseKey("Ctrl Eisu", &key_event);
  EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &fund_command));
  EXPECT_EQ(PrecompositionState::TOGGLE_ALPHANUMERIC_MODE, fund_command);

  // Composition state
  KeyParser::ParseKey("Ctrl 0", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::TRANSLATE_HALF_ASCII, composition_command);

  KeyParser::ParseKey("Ctrl 2", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_WITHOUT_HISTORY, composition_command);

  KeyParser::ParseKey("Ctrl 6", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::DISPLAY_AS_HIRAGANA, composition_command);

  KeyParser::ParseKey("Ctrl 7", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::DISPLAY_AS_FULL_KATAKANA, composition_command);

  KeyParser::ParseKey("Ctrl 8", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::TRANSLATE_HALF_WIDTH, composition_command);

  KeyParser::ParseKey("Ctrl 9", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::TRANSLATE_FULL_ASCII, composition_command);

  KeyParser::ParseKey("Ctrl Eisu", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::TOGGLE_ALPHANUMERIC_MODE, composition_command);

  KeyParser::ParseKey("Ctrl Option 1", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::DISPLAY_AS_HIRAGANA, composition_command);

  KeyParser::ParseKey("Ctrl Option 2", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::DISPLAY_AS_FULL_KATAKANA, composition_command);

  KeyParser::ParseKey("Ctrl Option 3", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::TRANSLATE_FULL_ASCII, composition_command);

  KeyParser::ParseKey("Ctrl Option 4", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::TRANSLATE_HALF_WIDTH, composition_command);

  KeyParser::ParseKey("Ctrl Option 5", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::TRANSLATE_HALF_ASCII, composition_command);

  // Conversion state
  KeyParser::ParseKey("Ctrl 0", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::TRANSLATE_HALF_ASCII, conv_command);

  KeyParser::ParseKey("Ctrl 6", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::DISPLAY_AS_HIRAGANA, conv_command);

  KeyParser::ParseKey("Ctrl 7", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::DISPLAY_AS_FULL_KATAKANA, conv_command);

  KeyParser::ParseKey("Ctrl 8", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::TRANSLATE_HALF_WIDTH, conv_command);

  KeyParser::ParseKey("Ctrl 9", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::TRANSLATE_FULL_ASCII, conv_command);

  KeyParser::ParseKey("Ctrl Option 1", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::DISPLAY_AS_HIRAGANA, conv_command);

  KeyParser::ParseKey("Ctrl Option 2", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::DISPLAY_AS_FULL_KATAKANA, conv_command);

  KeyParser::ParseKey("Ctrl Option 3", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::TRANSLATE_FULL_ASCII, conv_command);

  KeyParser::ParseKey("Ctrl Option 4", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::TRANSLATE_HALF_WIDTH, conv_command);

  KeyParser::ParseKey("Ctrl Option 5", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::TRANSLATE_HALF_ASCII, conv_command);


  // Additional ATOK key maps for Chrome OS
  manager = KeyMapFactory::GetKeyMapManager(config::Config::ATOK);

  // Precomposition state
  KeyParser::ParseKey("Ctrl 0", &key_event);
  EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &fund_command));
  EXPECT_EQ(PrecompositionState::TOGGLE_ALPHANUMERIC_MODE, fund_command);

  KeyParser::ParseKey("Ctrl Eisu", &key_event);
  EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &fund_command));
  EXPECT_EQ(PrecompositionState::TOGGLE_ALPHANUMERIC_MODE, fund_command);

  // Composition state
  KeyParser::ParseKey("Ctrl 0", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_HALF_ALPHANUMERIC,
            composition_command);

  KeyParser::ParseKey("Ctrl 2", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_WITHOUT_HISTORY, composition_command);

  KeyParser::ParseKey("Ctrl 6", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_HIRAGANA, composition_command);

  KeyParser::ParseKey("Ctrl 7", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_FULL_KATAKANA, composition_command);

  KeyParser::ParseKey("Ctrl 8", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_HALF_WIDTH, composition_command);

  KeyParser::ParseKey("Ctrl 9", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_FULL_ALPHANUMERIC,
            composition_command);

  KeyParser::ParseKey("Ctrl Eisu", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::TOGGLE_ALPHANUMERIC_MODE, composition_command);

  // Conversion state
  KeyParser::ParseKey("Ctrl 0", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_TO_HALF_ALPHANUMERIC, conv_command);

  KeyParser::ParseKey("Ctrl 6", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_TO_HIRAGANA, conv_command);

  KeyParser::ParseKey("Ctrl 7", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_TO_FULL_KATAKANA, conv_command);

  KeyParser::ParseKey("Ctrl 8", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_TO_HALF_WIDTH, conv_command);

  KeyParser::ParseKey("Ctrl 9", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_TO_FULL_ALPHANUMERIC, conv_command);

  KeyParser::ParseKey("Ctrl Eisu", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::TOGGLE_ALPHANUMERIC_MODE, conv_command);

  KeyParser::ParseKey("Shift Down", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_NEXT_PAGE, conv_command);

  KeyParser::ParseKey("Shift Up", &key_event);
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::CONVERT_PREV_PAGE, conv_command);
}

TEST(KeyMap, ShiftTabToConvertPrev) {
  // http://b/2973471
  // Shift+TAB does not work on a suggestion window

  commands::KeyEvent key_event;
  ConversionState::Commands conv_command;

  {  // MSIME
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::MSIME);
    KeyParser::ParseKey("Shift Tab", &key_event);
    EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::CONVERT_PREV, conv_command);
  }

  {  // Kotoeri
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::KOTOERI);
    KeyParser::ParseKey("Shift Tab", &key_event);
    EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::CONVERT_PREV, conv_command);
  }

  {  // ATOK
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::ATOK);
    KeyParser::ParseKey("Shift Tab", &key_event);
    EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::CONVERT_PREV, conv_command);
  }
}

TEST(KeyMap, LaunchToolTest) {
  config::Config config;
  commands::KeyEvent key_event;
  PrecompositionState::Commands conv_command;

  {  // ATOK
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::ATOK);

    KeyParser::ParseKey("Ctrl F7", &key_event);
    EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &conv_command));
    EXPECT_EQ(PrecompositionState::LAUNCH_WORD_REGISTER_DIALOG, conv_command);

    KeyParser::ParseKey("Ctrl F12", &key_event);
    EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &conv_command));
    EXPECT_EQ(PrecompositionState::LAUNCH_CONFIG_DIALOG, conv_command);
  }

  // http://b/3432829
  // MS-IME does not have the key-binding "Ctrl F7" in precomposition mode.
  {
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::MSIME);

    KeyParser::ParseKey("Ctrl F7", &key_event);
    EXPECT_FALSE(manager->GetCommandPrecomposition(key_event, &conv_command));
  }
}

TEST(KeyMap, Undo) {
  PrecompositionState::Commands command;
  commands::KeyEvent key_event;

  {  // ATOK
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::ATOK);
    KeyParser::ParseKey("Ctrl Backspace", &key_event);
    EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &command));
    EXPECT_EQ(PrecompositionState::UNDO, command);
  }
  {  // MSIME
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::MSIME);

    KeyParser::ParseKey("Ctrl Backspace", &key_event);
    EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &command));
    EXPECT_EQ(PrecompositionState::UNDO, command);
  }
  {  // KOTOERI
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::KOTOERI);

    KeyParser::ParseKey("Ctrl Backspace", &key_event);
    EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &command));
    EXPECT_EQ(PrecompositionState::UNDO, command);
  }
}

TEST(KeyMap, Reconvert) {
  DirectInputState::Commands direct_command;
  PrecompositionState::Commands precomposition_command;
  commands::KeyEvent key_event;

  {  // ATOK
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::ATOK);

    KeyParser::ParseKey("Shift Henkan", &key_event);
    EXPECT_TRUE(manager->GetCommandDirect(key_event, &direct_command));
    EXPECT_EQ(DirectInputState::RECONVERT, direct_command);
    EXPECT_TRUE(manager->GetCommandPrecomposition(
        key_event, &precomposition_command));
    EXPECT_EQ(PrecompositionState::RECONVERT, precomposition_command);
  }
  {  // MSIME
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::MSIME);

    KeyParser::ParseKey("Henkan", &key_event);
    EXPECT_TRUE(manager->GetCommandDirect(key_event, &direct_command));
    EXPECT_EQ(DirectInputState::RECONVERT, direct_command);
    EXPECT_TRUE(manager->GetCommandPrecomposition(
        key_event, &precomposition_command));
    EXPECT_EQ(PrecompositionState::RECONVERT, precomposition_command);
  }
  {  // KOTOERI
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::KOTOERI);

    KeyParser::ParseKey("Ctrl Shift r", &key_event);
    EXPECT_TRUE(manager->GetCommandDirect(key_event, &direct_command));
    EXPECT_EQ(DirectInputState::RECONVERT, direct_command);
    EXPECT_TRUE(manager->GetCommandPrecomposition(
        key_event, &precomposition_command));
    EXPECT_EQ(PrecompositionState::RECONVERT, precomposition_command);
  }
}

TEST(KeyMap, ReloadWithKeymap) {
  KeyMapManager manager;
  config::Config::SessionKeymap keymap_setting;
  commands::KeyEvent key_event;
  ConversionState::Commands conv_command;

  {  // ATOK
    keymap_setting = config::Config::ATOK;
    manager.ReloadWithKeymap(keymap_setting);
    KeyParser::ParseKey("Right", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::SEGMENT_WIDTH_EXPAND, conv_command);
  }
  {  // MSIME
    keymap_setting = config::Config::MSIME;
    manager.ReloadWithKeymap(keymap_setting);
    KeyParser::ParseKey("Right", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::SEGMENT_FOCUS_RIGHT, conv_command);
  }
}

TEST(KeyMap, AddCommand) {
  KeyMapManager manager;
  commands::KeyEvent key_event;
  const char kKeyEvent[] = "Ctrl Shift Insert";

  KeyParser::ParseKey(kKeyEvent, &key_event);

  {  // Add command
    CompositionState::Commands command;
    EXPECT_FALSE(manager.GetCommandComposition(key_event, &command));

    EXPECT_TRUE(manager.AddCommand("Composition", kKeyEvent, "Cancel"));

    EXPECT_TRUE(manager.GetCommandComposition(key_event, &command));
    EXPECT_EQ(CompositionState::CANCEL, command);
  }

  {  // Error detections
    EXPECT_FALSE(manager.AddCommand("void", kKeyEvent, "Cancel"));
    EXPECT_FALSE(manager.AddCommand("Composition", kKeyEvent, "Unknown"));
    EXPECT_FALSE(manager.AddCommand("Composition", "INVALID", "Cancel"));
  }
}

TEST(KeyMap, ZeroQuerySuggestion) {
  KeyMapManager manager;
  EXPECT_TRUE(manager.AddCommand("ZeroQuerySuggestion",
                                 "ESC", "Cancel"));
  EXPECT_TRUE(manager.AddCommand("ZeroQuerySuggestion",
                                 "Tab", "PredictAndConvert"));
  EXPECT_TRUE(manager.AddCommand("ZeroQuerySuggestion",
                                 "Shift Enter", "CommitFirstSuggestion"));
  // For fallback testing
  EXPECT_TRUE(manager.AddCommand("Precomposition", "Ctrl Backspace", "Revert"));

  commands::KeyEvent key_event;
  PrecompositionState::Commands command;

  KeyParser::ParseKey("ESC", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(PrecompositionState::CANCEL, command);

  KeyParser::ParseKey("Tab", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(PrecompositionState::PREDICT_AND_CONVERT, command);

  KeyParser::ParseKey("Shift Enter", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(PrecompositionState::COMMIT_FIRST_SUGGESTION, command);

  KeyParser::ParseKey("Ctrl Backspace", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(PrecompositionState::REVERT, command);
}

TEST(KeyMap, CapsLock) {
  // MSIME
  KeyMapManager *manager =
      KeyMapFactory::GetKeyMapManager(config::Config::MSIME);
  commands::KeyEvent key_event;
  KeyParser::ParseKey("CAPS a", &key_event);

  ConversionState::Commands conv_command;
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::INSERT_CHARACTER, conv_command);
}

TEST(KeyMap, ShortcutKeysWithCapsLock_Issue5627459) {
  // MSIME
  KeyMapManager *manager =
      KeyMapFactory::GetKeyMapManager(config::Config::MSIME);

  commands::KeyEvent key_event;
  CompositionState::Commands composition_command;

  // "Ctrl CAPS H" means that Ctrl and H key are pressed w/o shift key.
  // See the description in command.proto.
  KeyParser::ParseKey("Ctrl CAPS H", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::BACKSPACE, composition_command);

  // "Ctrl CAPS h" means that Ctrl, Shift and H key are pressed.
  KeyParser::ParseKey("Ctrl CAPS h", &key_event);
  EXPECT_FALSE(manager->GetCommandComposition(key_event, &composition_command));

  // "Ctrl CAPS 0" should be treated "Ctrl 0" as well.
  KeyParser::ParseKey("Ctrl CAPS 0", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::CONVERT_TO_HALF_ALPHANUMERIC,
            composition_command);
}

}  // namespace keymap
}  // namespace mozc
