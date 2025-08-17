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

// Keymap utils of Mozc interface.

#include "session/keymap.h"

#include <istream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "base/config_file_stream.h"
#include "composer/key_parser.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "testing/test_peer.h"

namespace mozc {
namespace keymap {
namespace {
config::Config GetDefaultConfig(config::Config::SessionKeymap keymap) {
  config::Config config;
  config.set_session_keymap(keymap);
  return config;
}
}  // namespace

class KeyMapManagerTestPeer : public testing::TestPeer<KeyMapManager> {
 public:
  explicit KeyMapManagerTestPeer(KeyMapManager& manager)
      : testing::TestPeer<KeyMapManager>(manager) {}

  PEER_METHOD(AddCommand);
  PEER_METHOD(LoadStream);
  PEER_METHOD(LoadStreamWithErrors);
};

class KeyMapTest : public testing::TestWithTempUserProfile {
 protected:
  bool isInputModeXCommandSupported() const {
    return KeyMapManager::kInputModeXCommandSupported;
  }
};

TEST_F(KeyMapTest, AddRule) {
  KeyMap<PrecompositionState> keymap;
  commands::KeyEvent key_event;
  // 'a'
  key_event.set_key_code(97);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 0 is treated as null input
  key_event.set_key_code(0);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 1 to 31 should be rejected.
  key_event.set_key_code(1);
  EXPECT_FALSE(
      keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));
  key_event.set_key_code(31);
  EXPECT_FALSE(
      keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 32 (space) is also invalid input.
  key_event.set_key_code(32);
  EXPECT_FALSE(
      keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 33 (!) is a valid input.
  key_event.set_key_code(33);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 8bit char is a valid input.
  key_event.set_key_code(127);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));
  key_event.set_key_code(255);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));
}

TEST_F(KeyMapTest, GetCommand) {
  {
    KeyMap<PrecompositionState> keymap;
    commands::KeyEvent init_key_event;
    init_key_event.set_key_code(97);
    EXPECT_TRUE(
        keymap.AddRule(init_key_event, PrecompositionState::INSERT_CHARACTER));

    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(command, PrecompositionState::INSERT_CHARACTER);

    key_event.Clear();
    key_event.set_key_code(98);
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }
  {
    KeyMap<PrecompositionState> keymap;
    commands::KeyEvent init_key_event;
    init_key_event.set_special_key(commands::KeyEvent::TEXT_INPUT);
    EXPECT_TRUE(
        keymap.AddRule(init_key_event, PrecompositionState::INSERT_CHARACTER));

    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_string("hoge");
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(command, PrecompositionState::INSERT_CHARACTER);
  }
  {
    KeyMap<CompositionState> keymap;
    commands::KeyEvent init_key_event;
    init_key_event.set_key_code(97);
    EXPECT_TRUE(
        keymap.AddRule(init_key_event, CompositionState::INSERT_CHARACTER));

    commands::KeyEvent key_event;
    CompositionState::Commands command;
    key_event.set_key_code(97);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(command, CompositionState::INSERT_CHARACTER);

    key_event.Clear();
    key_event.set_key_code(98);
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }
  {
    KeyMap<CompositionState> keymap;
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
    EXPECT_EQ(command, CompositionState::COMMIT);

    // CTRL-ALT-DELETE
    key_event.Clear();
    key_event.set_special_key(commands::KeyEvent::DEL);
    key_event.add_modifier_keys(commands::KeyEvent::CTRL);
    key_event.add_modifier_keys(commands::KeyEvent::ALT);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(command, CompositionState::IME_OFF);
  }
}

TEST_F(KeyMapTest, GetCommandForKeyString) {
  KeyMap<PrecompositionState> keymap;

  // When a key event is not registered, GetCommand should return false.
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }

  // When a key event is not registered, GetCommand should return false even if
  // the key event has |key_string|. See also b/9684668
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    key_event.set_key_string("a");
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }

  // Special case for b/4170089. VK_PACKET on Windows will be encoded as
  // {
  //   key_code: (empty)
  //   key_string: (the Unicode string to be input)
  // }
  // We always treat such key events as INSERT_CHARACTER command.
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_string("a");
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }

  // After adding the rule of TEXT_INPUT -> INSERT_CHARACTER, the above cases
  // should return INSERT_CHARACTER.
  commands::KeyEvent text_input_key_event;
  text_input_key_event.set_special_key(commands::KeyEvent::TEXT_INPUT);
  keymap.AddRule(text_input_key_event, PrecompositionState::INSERT_CHARACTER);

  // key_code = 97, key_string = empty
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(command, PrecompositionState::INSERT_CHARACTER);
  }

  // key_code = 97, key_string = "a"
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    key_event.set_key_string("a");
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(command, PrecompositionState::INSERT_CHARACTER);
  }

  // key_code = empty, key_string = "a"
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_string("a");
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(command, PrecompositionState::INSERT_CHARACTER);
  }
}

TEST_F(KeyMapTest, GetCommandKeyStub) {
  KeyMap<PrecompositionState> keymap;
  commands::KeyEvent init_key_event;
  init_key_event.set_special_key(commands::KeyEvent::TEXT_INPUT);
  EXPECT_TRUE(
      keymap.AddRule(init_key_event, PrecompositionState::INSERT_CHARACTER));

  PrecompositionState::Commands command;
  commands::KeyEvent key_event;
  key_event.set_key_code(97);
  EXPECT_TRUE(keymap.GetCommand(key_event, &command));
  EXPECT_EQ(command, PrecompositionState::INSERT_CHARACTER);
}

TEST_F(KeyMapTest, GetCommand_overlay) {
  config::Config config;
  config.set_session_keymap(config::Config::MSIME);
  config.add_overlay_keymaps(
      config::Config::OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF);
  KeyMapManager manager(config);
  {
    commands::KeyEvent key_event;
    key_event.set_special_key(commands::KeyEvent::HENKAN);
    DirectInputState::Commands command;
    EXPECT_TRUE(manager.GetCommandDirect(key_event, &command));
    // MSIME defines HENKAN as Reconvert, but the overlay defines it as IME_ON.
    EXPECT_EQ(command, DirectInputState::Commands::IME_ON);
  }
}

TEST_F(KeyMapTest, GetKeyMapFileName) {
  EXPECT_STREQ("system://atok.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::ATOK));
  EXPECT_STREQ("system://mobile.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::MOBILE));
  EXPECT_STREQ("system://ms-ime.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::MSIME));
  EXPECT_STREQ("system://kotoeri.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::KOTOERI));
  EXPECT_STREQ("system://chromeos.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::CHROMEOS));
  EXPECT_STREQ("user://keymap.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::CUSTOM));
}

TEST_F(KeyMapTest, DefaultKeyBindings) {
  KeyMapManager manager;

  std::istringstream iss("", std::istringstream::in);
  EXPECT_TRUE(KeyMapManagerTestPeer(manager).LoadStream(&iss));

  {  // Check key bindings of TextInput.
    commands::KeyEvent key_event;
    KeyParser::ParseKey("TextInput", &key_event);

    PrecompositionState::Commands fund_command;
    EXPECT_TRUE(manager.GetCommandPrecomposition(key_event, &fund_command));
    EXPECT_EQ(fund_command, PrecompositionState::INSERT_CHARACTER);

    CompositionState::Commands composition_command;
    EXPECT_TRUE(manager.GetCommandComposition(key_event, &composition_command));
    EXPECT_EQ(composition_command, CompositionState::INSERT_CHARACTER);

    ConversionState::Commands conv_command;
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(conv_command, ConversionState::INSERT_CHARACTER);
  }

  {  // Check key bindings of Shift.
    commands::KeyEvent key_event;
    KeyParser::ParseKey("Shift", &key_event);

    PrecompositionState::Commands fund_command;
    EXPECT_FALSE(manager.GetCommandPrecomposition(key_event, &fund_command));

    CompositionState::Commands composition_command;
    EXPECT_TRUE(manager.GetCommandComposition(key_event, &composition_command));
    EXPECT_EQ(composition_command, CompositionState::INSERT_CHARACTER);

    ConversionState::Commands conv_command;
    EXPECT_FALSE(manager.GetCommandConversion(key_event, &conv_command));
  }
}

TEST_F(KeyMapTest, LoadStreamWithErrors) {
  KeyMapManager manager;
  KeyMapManagerTestPeer manager_peer(manager);

  std::vector<std::string> errors;
  std::unique_ptr<std::istream> is(
      ConfigFileStream::OpenReadText("system://atok.tsv"));
  EXPECT_TRUE(manager_peer.LoadStreamWithErrors(is.get(), &errors));
  EXPECT_TRUE(errors.empty());

  errors.clear();
  is = ConfigFileStream::OpenReadText("system://ms-ime.tsv");
  EXPECT_TRUE(manager_peer.LoadStreamWithErrors(is.get(), &errors));
  EXPECT_TRUE(errors.empty());

  errors.clear();
  is = ConfigFileStream::OpenReadText("system://kotoeri.tsv");
  EXPECT_TRUE(manager_peer.LoadStreamWithErrors(is.get(), &errors));
  EXPECT_TRUE(errors.empty());

  errors.clear();
  is = ConfigFileStream::OpenReadText("system://mobile.tsv");
  EXPECT_TRUE(manager_peer.LoadStreamWithErrors(is.get(), &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(KeyMapTest, GetName) {
  KeyMapManager manager;
  {
    // Direct
    std::string name;
    EXPECT_TRUE(
        manager.GetNameFromCommandDirect(DirectInputState::IME_ON, &name));
    EXPECT_EQ(name, "IMEOn");
    EXPECT_TRUE(
        manager.GetNameFromCommandDirect(DirectInputState::RECONVERT, &name));
    EXPECT_EQ(name, "Reconvert");
  }
  {
    // Precomposition
    std::string name;
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::IME_OFF, &name));
    EXPECT_EQ(name, "IMEOff");
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::IME_ON, &name));
    EXPECT_EQ(name, "IMEOn");
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::INSERT_CHARACTER, &name));
    EXPECT_EQ(name, "InsertCharacter");
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::RECONVERT, &name));
    EXPECT_EQ(name, "Reconvert");
  }
  {
    // Composition
    std::string name;
    EXPECT_TRUE(manager.GetNameFromCommandComposition(CompositionState::IME_OFF,
                                                      &name));
    EXPECT_EQ(name, "IMEOff");
    EXPECT_TRUE(
        manager.GetNameFromCommandComposition(CompositionState::IME_ON, &name));
    EXPECT_EQ(name, "IMEOn");
    EXPECT_TRUE(manager.GetNameFromCommandComposition(
        CompositionState::INSERT_CHARACTER, &name));
    EXPECT_EQ(name, "InsertCharacter");
  }
  {
    // Conversion
    std::string name;
    EXPECT_TRUE(
        manager.GetNameFromCommandConversion(ConversionState::IME_OFF, &name));
    EXPECT_EQ(name, "IMEOff");
    EXPECT_TRUE(
        manager.GetNameFromCommandConversion(ConversionState::IME_ON, &name));
    EXPECT_EQ(name, "IMEOn");
    EXPECT_TRUE(manager.GetNameFromCommandConversion(
        ConversionState::INSERT_CHARACTER, &name));
    EXPECT_EQ(name, "InsertCharacter");
  }
}

TEST_F(KeyMapTest, DirectModeDoesNotSupportInsertSpace) {
  // InsertSpace, InsertAlternateSpace, InsertHalfSpace, and InsertFullSpace
  // are not supported in direct mode.
  KeyMapManager manager;
  absl::flat_hash_set<std::string> names;
  manager.AppendAvailableCommandNameDirect(names);

  // We cannot use EXPECT_EQ because of overload resolution here.
  EXPECT_FALSE(names.contains("InsertSpace"));
  EXPECT_FALSE(names.contains("InsertAlternateSpace"));
  EXPECT_FALSE(names.contains("InsertHalfSpace"));
  EXPECT_FALSE(names.contains("InsertFullSpace"));
}

TEST_F(KeyMapTest, ShiftTabToConvertPrev) {
  // http://b/2973471
  // Shift+TAB does not work on a suggestion window

  commands::KeyEvent key_event;
  ConversionState::Commands conv_command;

  {  // MSIME
    KeyMapManager manager(GetDefaultConfig(config::Config::MSIME));

    KeyParser::ParseKey("Shift Tab", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(conv_command, ConversionState::CONVERT_PREV);
  }

  {  // Kotoeri
    KeyMapManager manager(GetDefaultConfig(config::Config::KOTOERI));
    KeyParser::ParseKey("Shift Tab", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(conv_command, ConversionState::CONVERT_PREV);
  }

  {  // ATOK
    KeyMapManager manager(GetDefaultConfig(config::Config::ATOK));
    KeyParser::ParseKey("Shift Tab", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(conv_command, ConversionState::CONVERT_PREV);
  }
}

TEST_F(KeyMapTest, LaunchToolTest) {
  config::Config config;
  commands::KeyEvent key_event;
  PrecompositionState::Commands conv_command;

  {  // ATOK
    KeyMapManager manager(GetDefaultConfig(config::Config::ATOK));

    KeyParser::ParseKey("Ctrl F7", &key_event);
    EXPECT_TRUE(manager.GetCommandPrecomposition(key_event, &conv_command));
    EXPECT_EQ(conv_command, PrecompositionState::LAUNCH_WORD_REGISTER_DIALOG);

    KeyParser::ParseKey("Ctrl F12", &key_event);
    EXPECT_TRUE(manager.GetCommandPrecomposition(key_event, &conv_command));
    EXPECT_EQ(conv_command, PrecompositionState::LAUNCH_CONFIG_DIALOG);
  }

  // http://b/3432829
  // MS-IME does not have the key-binding "Ctrl F7" in precomposition mode.
  {
    KeyMapManager manager(GetDefaultConfig(config::Config::MSIME));

    KeyParser::ParseKey("Ctrl F7", &key_event);
    EXPECT_FALSE(manager.GetCommandPrecomposition(key_event, &conv_command));
  }
}

TEST_F(KeyMapTest, Undo) {
  PrecompositionState::Commands command;
  commands::KeyEvent key_event;

  {  // ATOK
    KeyMapManager manager(GetDefaultConfig(config::Config::ATOK));

    KeyParser::ParseKey("Ctrl Backspace", &key_event);
    EXPECT_TRUE(manager.GetCommandPrecomposition(key_event, &command));
    EXPECT_EQ(command, PrecompositionState::UNDO);
  }
  {  // MSIME
    KeyMapManager manager(GetDefaultConfig(config::Config::MSIME));

    KeyParser::ParseKey("Ctrl Backspace", &key_event);
    EXPECT_TRUE(manager.GetCommandPrecomposition(key_event, &command));
    EXPECT_EQ(command, PrecompositionState::UNDO);
  }
  {  // KOTOERI
    KeyMapManager manager(GetDefaultConfig(config::Config::KOTOERI));

    KeyParser::ParseKey("Ctrl Backspace", &key_event);
    EXPECT_TRUE(manager.GetCommandPrecomposition(key_event, &command));
    EXPECT_EQ(command, PrecompositionState::UNDO);
  }
}

TEST_F(KeyMapTest, Reconvert) {
  DirectInputState::Commands direct_command;
  PrecompositionState::Commands precomposition_command;
  commands::KeyEvent key_event;

  {  // ATOK
    KeyMapManager manager(GetDefaultConfig(config::Config::ATOK));

    KeyParser::ParseKey("Shift Henkan", &key_event);
    EXPECT_TRUE(manager.GetCommandDirect(key_event, &direct_command));
    EXPECT_EQ(direct_command, DirectInputState::RECONVERT);
    EXPECT_TRUE(
        manager.GetCommandPrecomposition(key_event, &precomposition_command));
    EXPECT_EQ(precomposition_command, PrecompositionState::RECONVERT);
  }
  {  // MSIME
    KeyMapManager manager(GetDefaultConfig(config::Config::MSIME));

    KeyParser::ParseKey("Henkan", &key_event);
    EXPECT_TRUE(manager.GetCommandDirect(key_event, &direct_command));
    EXPECT_EQ(direct_command, DirectInputState::RECONVERT);
    EXPECT_TRUE(
        manager.GetCommandPrecomposition(key_event, &precomposition_command));
    EXPECT_EQ(precomposition_command, PrecompositionState::RECONVERT);
  }
  {  // KOTOERI
    KeyMapManager manager(GetDefaultConfig(config::Config::KOTOERI));

    KeyParser::ParseKey("Ctrl Shift r", &key_event);
    EXPECT_TRUE(manager.GetCommandDirect(key_event, &direct_command));
    EXPECT_EQ(direct_command, DirectInputState::RECONVERT);
    EXPECT_TRUE(
        manager.GetCommandPrecomposition(key_event, &precomposition_command));
    EXPECT_EQ(precomposition_command, PrecompositionState::RECONVERT);
  }
}

TEST_F(KeyMapTest, Initialize) {
  commands::KeyEvent key_event;
  ConversionState::Commands conv_command;

  {  // ATOK
    KeyMapManager manager(GetDefaultConfig(config::Config::ATOK));
    KeyParser::ParseKey("Right", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(conv_command, ConversionState::SEGMENT_WIDTH_EXPAND);
  }
  {  // MSIME
    KeyMapManager manager(GetDefaultConfig(config::Config::MSIME));
    KeyParser::ParseKey("Right", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(conv_command, ConversionState::SEGMENT_FOCUS_RIGHT);
  }
}

TEST_F(KeyMapTest, AddCommand) {
  KeyMapManager manager;
  KeyMapManagerTestPeer manager_peer(manager);
  commands::KeyEvent key_event;
  constexpr char kKeyEvent[] = "Ctrl Shift Insert";

  KeyParser::ParseKey(kKeyEvent, &key_event);

  {  // Add command
    CompositionState::Commands command;
    EXPECT_FALSE(manager.GetCommandComposition(key_event, &command));

    EXPECT_TRUE(manager_peer.AddCommand("Composition", kKeyEvent, "Cancel"));

    EXPECT_TRUE(manager.GetCommandComposition(key_event, &command));
    EXPECT_EQ(command, CompositionState::CANCEL);
  }

  {  // Error detections
    EXPECT_FALSE(manager_peer.AddCommand("void", kKeyEvent, "Cancel"));
    EXPECT_FALSE(manager_peer.AddCommand("Composition", kKeyEvent, "Unknown"));
    EXPECT_FALSE(manager_peer.AddCommand("Composition", "INVALID", "Cancel"));
  }
}

TEST_F(KeyMapTest, ZeroQuerySuggestion) {
  KeyMapManager manager;
  KeyMapManagerTestPeer manager_peer(manager);
  EXPECT_TRUE(manager_peer.AddCommand("ZeroQuerySuggestion", "ESC", "Cancel"));
  EXPECT_TRUE(manager_peer.AddCommand("ZeroQuerySuggestion", "Tab",
                                      "PredictAndConvert"));
  EXPECT_TRUE(manager_peer.AddCommand("ZeroQuerySuggestion", "Shift Enter",
                                      "CommitFirstSuggestion"));
  // For fallback testing
  EXPECT_TRUE(
      manager_peer.AddCommand("Precomposition", "Ctrl Backspace", "Revert"));

  commands::KeyEvent key_event;
  PrecompositionState::Commands command;

  KeyParser::ParseKey("ESC", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(command, PrecompositionState::CANCEL);

  KeyParser::ParseKey("Tab", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(command, PrecompositionState::PREDICT_AND_CONVERT);

  KeyParser::ParseKey("Shift Enter", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(command, PrecompositionState::COMMIT_FIRST_SUGGESTION);

  KeyParser::ParseKey("Ctrl Backspace", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(command, PrecompositionState::REVERT);
}

TEST_F(KeyMapTest, CapsLock) {
  // MSIME
  KeyMapManager manager(GetDefaultConfig(config::Config::MSIME));
  commands::KeyEvent key_event;
  KeyParser::ParseKey("CAPS a", &key_event);

  ConversionState::Commands conv_command;
  EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(conv_command, ConversionState::INSERT_CHARACTER);
}

TEST_F(KeyMapTest, ShortcutKeysWithCapsLockIssue5627459) {
  // MSIME
  KeyMapManager manager(GetDefaultConfig(config::Config::MSIME));

  commands::KeyEvent key_event;
  CompositionState::Commands composition_command;

  // "Ctrl CAPS H" means that Ctrl and H key are pressed w/o shift key.
  // See the description in command.proto.
  KeyParser::ParseKey("Ctrl CAPS H", &key_event);
  EXPECT_TRUE(manager.GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(composition_command, CompositionState::BACKSPACE);

  // "Ctrl CAPS h" means that Ctrl, Shift and H key are pressed.
  KeyParser::ParseKey("Ctrl CAPS h", &key_event);
  EXPECT_FALSE(manager.GetCommandComposition(key_event, &composition_command));
}

// InputModeX is not supported on MacOSX.
TEST_F(KeyMapTest, InputModeChangeIsNotEnabledOnChromeOsIssue13947207) {
  if (!isInputModeXCommandSupported()) {
    return;
  }

  commands::KeyEvent key_event;
  ConversionState::Commands conv_command;

  {  // MSIME
    KeyMapManager manager(GetDefaultConfig(config::Config::MSIME));
    KeyParser::ParseKey("Hiragana", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(conv_command, ConversionState::INPUT_MODE_HIRAGANA);
  }
  {  // CHROMEOS
    KeyMapManager manager(GetDefaultConfig(config::Config::CHROMEOS));
    KeyParser::ParseKey("Hiragana", &key_event);
    EXPECT_FALSE(manager.GetCommandConversion(key_event, &conv_command));
  }
}

TEST_F(KeyMapTest, IsSameKeyMapManagerApplicable) {
  {
    // Current: CHROMEOS + []
    // Sending config: CHROMEOS + []
    // Expectation: True

    // CHROMEOS + []
    config::Config configChromeOs1;
    configChromeOs1.set_session_keymap(config::Config::CHROMEOS);
    config::Config configChromeOs2;
    configChromeOs2.set_session_keymap(config::Config::CHROMEOS);

    EXPECT_TRUE(KeyMapManager::IsSameKeyMapManagerApplicable(configChromeOs1,
                                                             configChromeOs2));
  }
  {
    // Current: CHROMEOS + []
    // Sending config: CHROMEOS + [OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF]
    // Expectation: False

    // CHROMEOS + []
    config::Config configChromeOs;
    configChromeOs.set_session_keymap(config::Config::CHROMEOS);
    // CHROMEOS + [OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF]
    config::Config configChromeOs_henkan;
    configChromeOs_henkan.set_session_keymap(config::Config::CHROMEOS);
    configChromeOs_henkan.add_overlay_keymaps(
        config::Config::OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF);

    EXPECT_FALSE(KeyMapManager::IsSameKeyMapManagerApplicable(
        configChromeOs, configChromeOs_henkan));
  }
  {
    // Current: CHROMEOS + [OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF,
    // OVERLAY_FOR_TEST]
    // Sending config: CHROMEOS + [OVERLAY_FOR_TEST]
    // Expectation: False

    // CHROMEOS + [OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF, OVERLAY_FOR_TEST]
    config::Config configChromeOs_henkan_muhenkan;
    configChromeOs_henkan_muhenkan.set_session_keymap(config::Config::CHROMEOS);
    configChromeOs_henkan_muhenkan.add_overlay_keymaps(
        config::Config::OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF);
    configChromeOs_henkan_muhenkan.add_overlay_keymaps(
        config::Config::OVERLAY_FOR_TEST);
    // CHROMEOS + [OVERLAY_FOR_TEST]
    config::Config configChromeOs_muhenkan;
    configChromeOs_muhenkan.set_session_keymap(config::Config::CHROMEOS);
    configChromeOs_muhenkan.add_overlay_keymaps(
        config::Config::OVERLAY_FOR_TEST);

    EXPECT_FALSE(KeyMapManager::IsSameKeyMapManagerApplicable(
        configChromeOs_henkan_muhenkan, configChromeOs_muhenkan));
  }
  {
    // Current: CHROMEOS + [OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF,
    // OVERLAY_FOR_TEST]
    // Sending config: CHROMEOS +
    // [OVERLAY_FOR_TEST, OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF]
    // Expectation: False (must be ordering aware)

    // CHROMEOS + [OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF, OVERLAY_FOR_TEST]
    config::Config configChromeOs_henkan_muhenkan;
    configChromeOs_henkan_muhenkan.set_session_keymap(config::Config::CHROMEOS);
    configChromeOs_henkan_muhenkan.add_overlay_keymaps(
        config::Config::OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF);
    configChromeOs_henkan_muhenkan.add_overlay_keymaps(
        config::Config::OVERLAY_FOR_TEST);
    // CHROMEOS + [OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF, OVERLAY_FOR_TEST]
    config::Config configChromeOs_muhenkan_henkan;
    configChromeOs_muhenkan_henkan.set_session_keymap(config::Config::CHROMEOS);
    configChromeOs_muhenkan_henkan.add_overlay_keymaps(
        config::Config::OVERLAY_FOR_TEST);
    configChromeOs_muhenkan_henkan.add_overlay_keymaps(
        config::Config::OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF);

    EXPECT_FALSE(KeyMapManager::IsSameKeyMapManagerApplicable(
        configChromeOs_henkan_muhenkan, configChromeOs_muhenkan_henkan));
  }
}

}  // namespace keymap
}  // namespace mozc
