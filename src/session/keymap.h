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

#ifndef MOZC_SESSION_KEYMAP_H_
#define MOZC_SESSION_KEYMAP_H_

#include <istream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "base/protobuf/repeated_field.h"
#include "composer/key_event_util.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace keymap {

struct DirectInputState {
  enum Commands {
    NONE = 0,
    IME_ON,
    // Switch input mode.
    COMPOSITION_MODE_HIRAGANA,
    COMPOSITION_MODE_FULL_KATAKANA,
    COMPOSITION_MODE_HALF_KATAKANA,
    COMPOSITION_MODE_FULL_ALPHANUMERIC,
    COMPOSITION_MODE_HALF_ALPHANUMERIC,
    RECONVERT,
  };
};

struct PrecompositionState {
  enum Commands {
    NONE = 0,
    IME_OFF,
    IME_ON,
    INSERT_CHARACTER,  // Move to Composition status.
    INSERT_SPACE,      // To handle spaces.
    // to handle shift+spaces (useally toggle half/full with)
    INSERT_ALTERNATE_SPACE,
    INSERT_HALF_SPACE,         // Input half-width space
    INSERT_FULL_SPACE,         // Input full-width space
    TOGGLE_ALPHANUMERIC_MODE,  // toggle AlphaNumeric and Hiragana mode.
    // Switch input mode.
    COMPOSITION_MODE_HIRAGANA,
    COMPOSITION_MODE_FULL_KATAKANA,
    COMPOSITION_MODE_HALF_KATAKANA,
    COMPOSITION_MODE_FULL_ALPHANUMERIC,
    COMPOSITION_MODE_HALF_ALPHANUMERIC,
    COMPOSITION_MODE_SWITCH_KANA_TYPE,  // rotate input mode
    LAUNCH_CONFIG_DIALOG,
    LAUNCH_DICTIONARY_TOOL,
    LAUNCH_WORD_REGISTER_DIALOG,
    REVERT,  // revert last operation (preedit still remains)
    UNDO,    // undo last operation (preedit is restored)
    RECONVERT,

    // For ZeroQuerySuggestion
    CANCEL,                   // Back to Composition status.
    CANCEL_AND_IME_OFF,       // Cancel composition and turn off IME
    COMMIT_FIRST_SUGGESTION,  // ATOK's Shift-Enter style
    PREDICT_AND_CONVERT,
  };
};

struct CompositionState {
  enum Commands {
    NONE = 0,
    IME_OFF,
    IME_ON,
    INSERT_CHARACTER,
    DEL,  // DELETE cannot be used on Windows.  It is defined as a macro.
    BACKSPACE,
    INSERT_SPACE,  // To handle spaces.
    // to handle shift+spaces (useally toggle half/full with)
    INSERT_ALTERNATE_SPACE,
    INSERT_HALF_SPACE,   // Input half-width space
    INSERT_FULL_SPACE,   // Input full-width space
    CANCEL,              // Move to Precomposition status.
    CANCEL_AND_IME_OFF,  // Cancel composition and turn off IME
    UNDO,
    MOVE_CURSOR_LEFT,
    MOVE_CURSOR_RIGHT,
    MOVE_CURSOR_TO_BEGINNING,
    MOVE_MOVE_CURSOR_TO_END,
    COMMIT,                   // Move to Precomposition status.
    COMMIT_FIRST_SUGGESTION,  // ATOK's Shift-Enter style
    CONVERT,                  // Move to Conversion status.
    CONVERT_WITHOUT_HISTORY,  // Move to Conversion status.
    PREDICT_AND_CONVERT,
    // Switching to ConversionState
    CONVERT_TO_HIRAGANA,       // F6
    CONVERT_TO_FULL_KATAKANA,  // F7
    CONVERT_TO_HALF_KATAKANA,
    CONVERT_TO_HALF_WIDTH,         // F8
    CONVERT_TO_FULL_ALPHANUMERIC,  // F9
    CONVERT_TO_HALF_ALPHANUMERIC,  // F10
    SWITCH_KANA_TYPE,              // Muhenkan
    // Rmaining CompositionState
    DISPLAY_AS_HIRAGANA,       // F6
    DISPLAY_AS_FULL_KATAKANA,  // F7
    DISPLAY_AS_HALF_KATAKANA,
    TRANSLATE_HALF_WIDTH,      // F8
    TRANSLATE_FULL_ASCII,      // F9
    TRANSLATE_HALF_ASCII,      // F10
    TOGGLE_ALPHANUMERIC_MODE,  // toggle AlphaNumeric and Hiragana mode.
    // Switch input mode.
    COMPOSITION_MODE_HIRAGANA,
    COMPOSITION_MODE_FULL_KATAKANA,
    COMPOSITION_MODE_HALF_KATAKANA,
    COMPOSITION_MODE_FULL_ALPHANUMERIC,
    COMPOSITION_MODE_HALF_ALPHANUMERIC,
  };
};

struct ConversionState {
  enum Commands {
    NONE = 0,
    IME_OFF,
    IME_ON,
    INSERT_CHARACTER,  // Submit and Move to Composition status.
    INSERT_SPACE,      // To handle spaces.
    // to handle shift+spaces (useally toggle half/full with)
    INSERT_ALTERNATE_SPACE,
    INSERT_HALF_SPACE,   // Input half-width space
    INSERT_FULL_SPACE,   // Input full-width space
    CANCEL,              // Back to Composition status.
    CANCEL_AND_IME_OFF,  // Cancel composition and turn off IME
    UNDO,
    SEGMENT_FOCUS_LEFT,
    SEGMENT_FOCUS_RIGHT,
    SEGMENT_FOCUS_FIRST,
    SEGMENT_FOCUS_LAST,
    SEGMENT_WIDTH_EXPAND,
    SEGMENT_WIDTH_SHRINK,
    CONVERT_NEXT,
    CONVERT_PREV,
    CONVERT_NEXT_PAGE,
    CONVERT_PREV_PAGE,
    PREDICT_AND_CONVERT,
    COMMIT,          // Move to Precomposition status.
    COMMIT_SEGMENT,  // Down on the ATOK style.
    // CONVERT_TO and TRANSLATE are same behavior on ConversionState.
    CONVERT_TO_HIRAGANA,       // F6
    CONVERT_TO_FULL_KATAKANA,  // F7
    CONVERT_TO_HALF_KATAKANA,
    CONVERT_TO_HALF_WIDTH,         // F8
    CONVERT_TO_FULL_ALPHANUMERIC,  // F9
    CONVERT_TO_HALF_ALPHANUMERIC,  // F10
    SWITCH_KANA_TYPE,              // Muhenkan
    DISPLAY_AS_HIRAGANA,           // F6
    DISPLAY_AS_FULL_KATAKANA,      // F7
    DISPLAY_AS_HALF_KATAKANA,
    TRANSLATE_HALF_WIDTH,      // F8
    TRANSLATE_FULL_ASCII,      // F9
    TRANSLATE_HALF_ASCII,      // F10
    TOGGLE_ALPHANUMERIC_MODE,  // toggle AlphaNumeric and Hiragana mode.
    // Switch input mode.
    COMPOSITION_MODE_HIRAGANA,
    COMPOSITION_MODE_FULL_KATAKANA,
    COMPOSITION_MODE_HALF_KATAKANA,
    COMPOSITION_MODE_FULL_ALPHANUMERIC,
    COMPOSITION_MODE_HALF_ALPHANUMERIC,
    DELETE_SELECTED_CANDIDATE,
    REPORT_BUG,
  };
};

template <typename T>
class KeyMap {
 public:
  using CommandsType = typename T::Commands;

  bool GetCommand(const commands::KeyEvent& key_event,
                  CommandsType* command) const;
  bool AddRule(const commands::KeyEvent& key_event, CommandsType command);
  void Clear();

 private:
  using KeyToCommandMap = absl::flat_hash_map<KeyInformation, CommandsType>;
  KeyToCommandMap keymap_;
};

// A manager of key mapping rule for a Config.
// The instance is created based on a Config through the constructor,
// and the instance is immutable (If the config is updated after the instance
// creation, this instance is unchanged).
class KeyMapManager {
 public:
  // Default ctor. keymap is NONE.
  KeyMapManager();
  explicit KeyMapManager(const config::Config& config);
  KeyMapManager(const KeyMapManager&) = delete;
  KeyMapManager& operator=(const KeyMapManager&) = delete;
  ~KeyMapManager() = default;

  bool GetCommandDirect(const commands::KeyEvent& key_event,
                        DirectInputState::Commands* command) const;

  bool GetCommandPrecomposition(const commands::KeyEvent& key_event,
                                PrecompositionState::Commands* command) const;

  bool GetCommandComposition(const commands::KeyEvent& key_event,
                             CompositionState::Commands* command) const;

  bool GetCommandConversion(const commands::KeyEvent& key_event,
                            ConversionState::Commands* command) const;

  bool GetCommandZeroQuerySuggestion(
      const commands::KeyEvent& key_event,
      PrecompositionState::Commands* command) const;

  bool GetCommandSuggestion(const commands::KeyEvent& key_event,
                            CompositionState::Commands* command) const;

  bool GetCommandPrediction(const commands::KeyEvent& key_event,
                            ConversionState::Commands* command) const;

  bool GetNameFromCommandDirect(DirectInputState::Commands command,
                                std::string* name) const;
  bool GetNameFromCommandPrecomposition(PrecompositionState::Commands command,
                                        std::string* name) const;
  bool GetNameFromCommandComposition(CompositionState::Commands command,
                                     std::string* name) const;
  bool GetNameFromCommandConversion(ConversionState::Commands command,
                                    std::string* name) const;

  // Append command names to command_names.
  void AppendAvailableCommandNameDirect(
      absl::flat_hash_set<std::string>& command_names) const;
  void AppendAvailableCommandNamePrecomposition(
      absl::flat_hash_set<std::string>& command_names) const;
  void AppendAvailableCommandNameComposition(
      absl::flat_hash_set<std::string>& command_names) const;
  void AppendAvailableCommandNameConversion(
      absl::flat_hash_set<std::string>& command_names) const;
  void AppendAvailableCommandNameZeroQuerySuggestion(
      absl::flat_hash_set<std::string>& command_names) const;
  void AppendAvailableCommandNameSuggestion(
      absl::flat_hash_set<std::string>& command_names) const;
  void AppendAvailableCommandNamePrediction(
      absl::flat_hash_set<std::string>& command_names) const;

  // Return the file name bound with the keymap enum.
  static const char* GetKeyMapFileName(config::Config::SessionKeymap keymap);

  // Returns true if both `new_config` and `old_config`
  // can use the same KeyMapManager.
  static bool IsSameKeyMapManagerApplicable(const config::Config& old_config,
                                            const config::Config& new_config);

 private:
  friend class KeyMapTest;
  friend class KeyMapManagerTestPeer;

  void Reset();
  void InitCommandData();
  bool Initialize();

  bool LoadFile(const char* filename);
  bool LoadStream(std::istream* ifs);
  bool LoadStreamWithErrors(std::istream* ifs,
                            std::vector<std::string>* errors);
  // Applies "primary" SessionKeymap, like MSIME or KOTOERI.
  bool ApplyPrimarySessionKeymap(config::Config::SessionKeymap keymap,
                                 const std::string& custom_keymap_table);
  // Applies "overlay" SessionKeymaps, like OVERLAY_HENKAN_TO_IME_ON.
  void ApplyOverlaySessionKeymap(
      const ::mozc::protobuf::RepeatedField<int>& overlay_keymaps);

  // Add a command bound with state and key_event.
  bool AddCommand(const std::string& state_name,
                  const std::string& key_event_name,
                  const std::string& command_name);

  bool ParseCommandDirect(const std::string& command_string,
                          DirectInputState::Commands* command) const;
  bool ParseCommandPrecomposition(const std::string& command_string,
                                  PrecompositionState::Commands* command) const;
  bool ParseCommandComposition(const std::string& command_string,
                               CompositionState::Commands* command) const;
  bool ParseCommandConversion(const std::string& command_string,
                              ConversionState::Commands* command) const;
  void RegisterDirectCommand(const std::string& command_string,
                             DirectInputState::Commands command);
  void RegisterPrecompositionCommand(const std::string& command_string,
                                     PrecompositionState::Commands command);
  void RegisterCompositionCommand(const std::string& command_string,
                                  CompositionState::Commands command);
  void RegisterConversionCommand(const std::string& command_string,
                                 ConversionState::Commands command);

#if defined(__APPLE__)
  static constexpr bool kCompositionModeXCommandSupported = false;
#else   // __APPLE__
  static constexpr bool kCompositionModeXCommandSupported = true;
#endif  // __APPLE__

  absl::flat_hash_map<std::string, DirectInputState::Commands>
      command_direct_map_;
  absl::flat_hash_map<std::string, PrecompositionState::Commands>
      command_precomposition_map_;
  absl::flat_hash_map<std::string, CompositionState::Commands>
      command_composition_map_;
  absl::flat_hash_map<std::string, ConversionState::Commands>
      command_conversion_map_;

  absl::flat_hash_map<DirectInputState::Commands, std::string>
      reverse_command_direct_map_;
  absl::flat_hash_map<PrecompositionState::Commands, std::string>
      reverse_command_precomposition_map_;
  absl::flat_hash_map<CompositionState::Commands, std::string>
      reverse_command_composition_map_;
  absl::flat_hash_map<ConversionState::Commands, std::string>
      reverse_command_conversion_map_;

  // Status should be out of keymap.
  KeyMap<DirectInputState> keymap_direct_;
  KeyMap<PrecompositionState> keymap_precomposition_;
  KeyMap<CompositionState> keymap_composition_;
  KeyMap<ConversionState> keymap_conversion_;

  // enabled only if zero query suggestion is shown. Otherwise, inherit from
  // keymap_precomposition
  KeyMap<PrecompositionState> keymap_zero_query_suggestion_;

  // enabled only if suggestion is shown. Otherwise, inherit from
  // keymap_composition
  KeyMap<CompositionState> keymap_suggestion_;

  // enabled only if prediction is shown. Otherwise, inherit from
  // keymap_conversion
  KeyMap<ConversionState> keymap_prediction_;
};

template <typename T>
bool KeyMap<T>::GetCommand(const commands::KeyEvent& key_event,
                           CommandsType* command) const {
  // Shortcut keys should be available as if CapsLock was not enabled like
  // other IMEs such as MS-IME or ATOK. b/5627459
  commands::KeyEvent normalized_key_event;
  KeyEventUtil::NormalizeModifiers(key_event, &normalized_key_event);
  KeyInformation key;
  if (!KeyEventUtil::GetKeyInformation(normalized_key_event, &key)) {
    return false;
  }

  if (const auto it = keymap_.find(key); it != keymap_.end()) {
    *command = it->second;
    return true;
  }

  if (KeyEventUtil::MaybeGetKeyStub(normalized_key_event, &key)) {
    const auto it = keymap_.find(key);
    if (it != keymap_.end()) {
      *command = it->second;
      return true;
    }
  }

  return false;
}

template <typename T>
bool KeyMap<T>::AddRule(const commands::KeyEvent& key_event,
                        CommandsType command) {
  KeyInformation key;
  if (!KeyEventUtil::GetKeyInformation(key_event, &key)) {
    return false;
  }

  keymap_[key] = command;
  return true;
}

template <typename T>
void KeyMap<T>::Clear() {
  keymap_.clear();
}

}  // namespace keymap
}  // namespace mozc

#endif  // MOZC_SESSION_KEYMAP_H_
