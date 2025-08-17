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

#include <algorithm>
#include <istream>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_split.h"
#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/util.h"
#include "composer/key_parser.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"

namespace mozc {
namespace keymap {
namespace {
static constexpr char kMSIMEKeyMapFile[] = "system://ms-ime.tsv";
static constexpr char kATOKKeyMapFile[] = "system://atok.tsv";
static constexpr char kKotoeriKeyMapFile[] = "system://kotoeri.tsv";
// keymap.tsv is a write-only file for debugging purposes.
static constexpr char kCustomKeyMapFile[] = "user://keymap.tsv";
static constexpr char kMobileKeyMapFile[] = "system://mobile.tsv";
static constexpr char kChromeOsKeyMapFile[] = "system://chromeos.tsv";
static constexpr char kOverlayHenkanMuhenkanToImeOnOffKeyMapFile[] =
    "system://overlay_henkan_muhenkan_to_ime_on_off.tsv";
}  // namespace

// static
bool KeyMapManager::IsSameKeyMapManagerApplicable(
    const config::Config& old_config, const config::Config& new_config) {
  if (&old_config == &new_config) {
    return true;
  }
  if (old_config.session_keymap() != new_config.session_keymap()) {
    return false;
  }
  if (!std::equal(old_config.overlay_keymaps().begin(),
                  old_config.overlay_keymaps().end(),
                  new_config.overlay_keymaps().begin(),
                  new_config.overlay_keymaps().end())) {
    return false;
  }
  if (old_config.session_keymap() == config::Config::CUSTOM &&
      old_config.custom_keymap_table() != new_config.custom_keymap_table()) {
    return false;
  }
  return true;
}

KeyMapManager::KeyMapManager() {
  InitCommandData();
  ApplyPrimarySessionKeymap(config::ConfigHandler::GetDefaultKeyMap(), "");
  // No overlay keymap is set.
}

KeyMapManager::KeyMapManager(const config::Config& config) {
  InitCommandData();
  ApplyPrimarySessionKeymap(config.session_keymap(),
                            config.custom_keymap_table());
  ApplyOverlaySessionKeymap(config.overlay_keymaps());
}

void KeyMapManager::Reset() {
  keymap_direct_.Clear();
  keymap_precomposition_.Clear();
  keymap_composition_.Clear();
  keymap_conversion_.Clear();
  keymap_zero_query_suggestion_.Clear();
  keymap_suggestion_.Clear();
  keymap_prediction_.Clear();
}

bool KeyMapManager::ApplyPrimarySessionKeymap(
    const config::Config::SessionKeymap keymap,
    const std::string& custom_keymap_table) {
  const char* keymap_file = GetKeyMapFileName(keymap);
  if ((keymap == config::Config::CUSTOM && custom_keymap_table.empty()) ||
      keymap_file == nullptr) {
    // Exceptional case; fallback to default key map.
    LOG(WARNING) << "custom_keymap_table is empty. use default setting";
    return LoadFile(
        GetKeyMapFileName(config::ConfigHandler::GetDefaultKeyMap()));
  } else if (keymap != config::Config::CUSTOM) {
    // For non-custom keymap, load and apply an embedded keymap tsv file.
    return LoadFile(keymap_file);
  } else {
    // For custom keymap, apply keymap in the config message.
#ifndef NDEBUG
    // make a copy of keymap file just for debugging
    const char* keymap_file = GetKeyMapFileName(keymap);
    const std::string filename = ConfigFileStream::GetFileName(keymap_file);
    OutputFileStream ofs(filename);
    if (ofs) {
      ofs << "# This is a copy of keymap table for debugging." << std::endl;
      ofs << "# Nothing happens when you edit this file manually." << std::endl;
      ofs << custom_keymap_table;
    }
#endif  // NDEBUG

    std::istringstream ifs(custom_keymap_table);
    return LoadStream(&ifs);
  }
}

void KeyMapManager::ApplyOverlaySessionKeymap(
    const ::mozc::protobuf::RepeatedField<int>& overlay_keymaps) {
  for (const auto& overlay : overlay_keymaps) {
    const char* overlay_keymap_file =
        GetKeyMapFileName(static_cast<config::Config::SessionKeymap>(overlay));
    DLOG(INFO) << "Overlay keymap " << overlay_keymap_file;
    if (overlay_keymap_file != nullptr) {
      LoadFile(overlay_keymap_file);
    }
  }
}

// static
const char* KeyMapManager::GetKeyMapFileName(
    const config::Config::SessionKeymap keymap) {
  switch (keymap) {
    case config::Config::ATOK:
      return kATOKKeyMapFile;
    case config::Config::MOBILE:
      return kMobileKeyMapFile;
    case config::Config::MSIME:
      return kMSIMEKeyMapFile;
    case config::Config::KOTOERI:
      return kKotoeriKeyMapFile;
    case config::Config::CHROMEOS:
      return kChromeOsKeyMapFile;
    case config::Config::CUSTOM:
      return kCustomKeyMapFile;
    case config::Config::OVERLAY_HENKAN_MUHENKAN_TO_IME_ON_OFF:
      return kOverlayHenkanMuhenkanToImeOnOffKeyMapFile;
    case config::Config::OVERLAY_FOR_TEST:
    case config::Config::NONE:
    default:
      // should not appear here.
      LOG(ERROR) << "Keymap type: " << keymap
                 << " appeared at key map initialization.";
      const config::Config::SessionKeymap default_keymap =
          config::ConfigHandler::GetDefaultKeyMap();
      DCHECK(default_keymap == config::Config::ATOK ||
             default_keymap == config::Config::MOBILE ||
             default_keymap == config::Config::MSIME ||
             default_keymap == config::Config::KOTOERI ||
             default_keymap == config::Config::CHROMEOS ||
             default_keymap == config::Config::CUSTOM);
      // should never make loop.
      return GetKeyMapFileName(default_keymap);
  }
}

bool KeyMapManager::LoadFile(const char* filename) {
  std::unique_ptr<std::istream> ifs(ConfigFileStream::LegacyOpen(filename));
  if (ifs == nullptr) {
    LOG(WARNING) << "cannot load keymap table: " << filename;
    return false;
  }
  return LoadStream(ifs.get());
}

bool KeyMapManager::LoadStream(std::istream* ifs) {
  std::vector<std::string> errors;
  return LoadStreamWithErrors(ifs, &errors);
}

bool KeyMapManager::LoadStreamWithErrors(std::istream* ifs,
                                         std::vector<std::string>* errors) {
  std::string line;
  std::getline(*ifs, line);  // Skip the first line.
  while (!ifs->eof()) {
    std::getline(*ifs, line);
    Util::ChopReturns(&line);

    if (line.empty() || line[0] == '#') {  // Skip empty or comment line.
      continue;
    }

    std::vector<std::string> rules =
        absl::StrSplit(line, '\t', absl::SkipEmpty());
    if (rules.size() != 3) {
      LOG(ERROR) << "Invalid format: " << line;
      continue;
    }

    if (!AddCommand(rules[0], rules[1], rules[2])) {
      errors->push_back(line);
      LOG(ERROR) << "Unknown command: " << line;
    }
  }

  commands::KeyEvent key_event;
  KeyParser::ParseKey("TextInput", &key_event);
  keymap_precomposition_.AddRule(key_event,
                                 PrecompositionState::INSERT_CHARACTER);
  keymap_composition_.AddRule(key_event, CompositionState::INSERT_CHARACTER);
  keymap_conversion_.AddRule(key_event, ConversionState::INSERT_CHARACTER);

  key_event.Clear();
  KeyParser::ParseKey("Shift", &key_event);
  keymap_composition_.AddRule(key_event, CompositionState::INSERT_CHARACTER);
  return true;
}

bool KeyMapManager::AddCommand(const std::string& state_name,
                               const std::string& key_event_name,
                               const std::string& command_name) {
#ifdef NDEBUG  // means RELEASE BUILD
  // On the release build, we do not support the ReportBug
  // commands.  Note, true is returned as the arguments are
  // interpreted properly.
  if (command_name == "ReportBug") {
    return true;
  }
#endif  // NDEBUG

  commands::KeyEvent key_event;
  if (!KeyParser::ParseKey(key_event_name, &key_event)) {
    return false;
  }

  if (state_name == "DirectInput" || state_name == "Direct") {
    DirectInputState::Commands command;
    if (!ParseCommandDirect(command_name, &command)) {
      return false;
    }

    keymap_direct_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "Precomposition") {
    PrecompositionState::Commands command;
    if (!ParseCommandPrecomposition(command_name, &command)) {
      return false;
    }

    keymap_precomposition_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "Composition") {
    CompositionState::Commands command;
    if (!ParseCommandComposition(command_name, &command)) {
      return false;
    }

    keymap_composition_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "Conversion") {
    ConversionState::Commands command;
    if (!ParseCommandConversion(command_name, &command)) {
      return false;
    }

    keymap_conversion_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "ZeroQuerySuggestion") {
    PrecompositionState::Commands command;
    if (!ParseCommandPrecomposition(command_name, &command)) {
      return false;
    }

    keymap_zero_query_suggestion_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "Suggestion") {
    CompositionState::Commands command;
    if (!ParseCommandComposition(command_name, &command)) {
      return false;
    }

    keymap_suggestion_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "Prediction") {
    ConversionState::Commands command;
    if (!ParseCommandConversion(command_name, &command)) {
      return false;
    }

    keymap_prediction_.AddRule(key_event, command);
    return true;
  }

  return false;
}

namespace {
template <typename T>
bool GetNameInternal(
    const absl::flat_hash_map<T, std::string>& reverse_command_map, T command,
    std::string* name) {
  DCHECK(name);
  const auto iter = reverse_command_map.find(command);
  if (iter == reverse_command_map.end()) {
    return false;
  } else {
    *name = iter->second;
    return true;
  }
}
}  // namespace

bool KeyMapManager::GetNameFromCommandDirect(DirectInputState::Commands command,
                                             std::string* name) const {
  return GetNameInternal<DirectInputState::Commands>(
      reverse_command_direct_map_, command, name);
}

bool KeyMapManager::GetNameFromCommandPrecomposition(
    PrecompositionState::Commands command, std::string* name) const {
  return GetNameInternal<PrecompositionState::Commands>(
      reverse_command_precomposition_map_, command, name);
}

bool KeyMapManager::GetNameFromCommandComposition(
    CompositionState::Commands command, std::string* name) const {
  return GetNameInternal<CompositionState::Commands>(
      reverse_command_composition_map_, command, name);
}

bool KeyMapManager::GetNameFromCommandConversion(
    ConversionState::Commands command, std::string* name) const {
  return GetNameInternal<ConversionState::Commands>(
      reverse_command_conversion_map_, command, name);
}

void KeyMapManager::RegisterDirectCommand(const std::string& command_string,
                                          DirectInputState::Commands command) {
  command_direct_map_[command_string] = command;
  reverse_command_direct_map_[command] = command_string;
}

void KeyMapManager::RegisterPrecompositionCommand(
    const std::string& command_string, PrecompositionState::Commands command) {
  command_precomposition_map_[command_string] = command;
  reverse_command_precomposition_map_[command] = command_string;
}

void KeyMapManager::RegisterCompositionCommand(
    const std::string& command_string, CompositionState::Commands command) {
  command_composition_map_[command_string] = command;
  reverse_command_composition_map_[command] = command_string;
}

void KeyMapManager::RegisterConversionCommand(
    const std::string& command_string, ConversionState::Commands command) {
  command_conversion_map_[command_string] = command;
  reverse_command_conversion_map_[command] = command_string;
}

void KeyMapManager::InitCommandData() {
  RegisterDirectCommand("IMEOn", DirectInputState::IME_ON);
  if (kInputModeXCommandSupported) {
    RegisterDirectCommand("InputModeHiragana",
                          DirectInputState::INPUT_MODE_HIRAGANA);
    RegisterDirectCommand("InputModeFullKatakana",
                          DirectInputState::INPUT_MODE_FULL_KATAKANA);
    RegisterDirectCommand("InputModeHalfKatakana",
                          DirectInputState::INPUT_MODE_HALF_KATAKANA);
    RegisterDirectCommand("InputModeFullAlphanumeric",
                          DirectInputState::INPUT_MODE_FULL_ALPHANUMERIC);
    RegisterDirectCommand("InputModeHalfAlphanumeric",
                          DirectInputState::INPUT_MODE_HALF_ALPHANUMERIC);
  } else {
    RegisterDirectCommand("InputModeHiragana", DirectInputState::NONE);
    RegisterDirectCommand("InputModeFullKatakana", DirectInputState::NONE);
    RegisterDirectCommand("InputModeHalfKatakana", DirectInputState::NONE);
    RegisterDirectCommand("InputModeFullAlphanumeric", DirectInputState::NONE);
    RegisterDirectCommand("InputModeHalfAlphanumeric", DirectInputState::NONE);
  }
  RegisterDirectCommand("Reconvert", DirectInputState::RECONVERT);

  // Precomposition
  RegisterPrecompositionCommand("IMEOff", PrecompositionState::IME_OFF);
  RegisterPrecompositionCommand("IMEOn", PrecompositionState::IME_ON);
  RegisterPrecompositionCommand("InsertCharacter",
                                PrecompositionState::INSERT_CHARACTER);
  RegisterPrecompositionCommand("InsertSpace",
                                PrecompositionState::INSERT_SPACE);
  RegisterPrecompositionCommand("InsertAlternateSpace",
                                PrecompositionState::INSERT_ALTERNATE_SPACE);
  RegisterPrecompositionCommand("InsertHalfSpace",
                                PrecompositionState::INSERT_HALF_SPACE);
  RegisterPrecompositionCommand("InsertFullSpace",
                                PrecompositionState::INSERT_FULL_SPACE);
  RegisterPrecompositionCommand("ToggleAlphanumericMode",
                                PrecompositionState::TOGGLE_ALPHANUMERIC_MODE);
  if (kInputModeXCommandSupported) {
    RegisterPrecompositionCommand("InputModeHiragana",
                                  PrecompositionState::INPUT_MODE_HIRAGANA);
    RegisterPrecompositionCommand(
        "InputModeFullKatakana", PrecompositionState::INPUT_MODE_FULL_KATAKANA);
    RegisterPrecompositionCommand(
        "InputModeHalfKatakana", PrecompositionState::INPUT_MODE_HALF_KATAKANA);
    RegisterPrecompositionCommand(
        "InputModeFullAlphanumeric",
        PrecompositionState::INPUT_MODE_FULL_ALPHANUMERIC);
    RegisterPrecompositionCommand(
        "InputModeHalfAlphanumeric",
        PrecompositionState::INPUT_MODE_HALF_ALPHANUMERIC);
    RegisterPrecompositionCommand(
        "InputModeSwitchKanaType",
        PrecompositionState::INPUT_MODE_SWITCH_KANA_TYPE);
  } else {
    RegisterPrecompositionCommand("InputModeHiragana",
                                  PrecompositionState::NONE);
    RegisterPrecompositionCommand("InputModeFullKatakana",
                                  PrecompositionState::NONE);
    RegisterPrecompositionCommand("InputModeHalfKatakana",
                                  PrecompositionState::NONE);
    RegisterPrecompositionCommand("InputModeFullAlphanumeric",
                                  PrecompositionState::NONE);
    RegisterPrecompositionCommand("InputModeHalfAlphanumeric",
                                  PrecompositionState::NONE);
    RegisterPrecompositionCommand("InputModeSwitchKanaType",
                                  PrecompositionState::NONE);
  }

  RegisterPrecompositionCommand("LaunchConfigDialog",
                                PrecompositionState::LAUNCH_CONFIG_DIALOG);
  RegisterPrecompositionCommand("LaunchDictionaryTool",
                                PrecompositionState::LAUNCH_DICTIONARY_TOOL);
  RegisterPrecompositionCommand(
      "LaunchWordRegisterDialog",
      PrecompositionState::LAUNCH_WORD_REGISTER_DIALOG);

  RegisterPrecompositionCommand("Revert", PrecompositionState::REVERT);
  RegisterPrecompositionCommand("Undo", PrecompositionState::UNDO);
  RegisterPrecompositionCommand("Reconvert", PrecompositionState::RECONVERT);

  RegisterPrecompositionCommand("Cancel", PrecompositionState::CANCEL);
  RegisterPrecompositionCommand("CancelAndIMEOff",
                                PrecompositionState::CANCEL_AND_IME_OFF);
  RegisterPrecompositionCommand("CommitFirstSuggestion",
                                PrecompositionState::COMMIT_FIRST_SUGGESTION);
  RegisterPrecompositionCommand("PredictAndConvert",
                                PrecompositionState::PREDICT_AND_CONVERT);

  // Composition
  RegisterCompositionCommand("IMEOff", CompositionState::IME_OFF);
  RegisterCompositionCommand("IMEOn", CompositionState::IME_ON);
  RegisterCompositionCommand("InsertCharacter",
                             CompositionState::INSERT_CHARACTER);
  RegisterCompositionCommand("Delete", CompositionState::DEL);
  RegisterCompositionCommand("Backspace", CompositionState::BACKSPACE);
  RegisterCompositionCommand("InsertSpace", CompositionState::INSERT_SPACE);
  RegisterCompositionCommand("InsertAlternateSpace",
                             CompositionState::INSERT_ALTERNATE_SPACE);
  RegisterCompositionCommand("InsertHalfSpace",
                             CompositionState::INSERT_HALF_SPACE);
  RegisterCompositionCommand("InsertFullSpace",
                             CompositionState::INSERT_FULL_SPACE);
  RegisterCompositionCommand("Cancel", CompositionState::CANCEL);
  RegisterCompositionCommand("CancelAndIMEOff",
                             CompositionState::CANCEL_AND_IME_OFF);
  RegisterCompositionCommand("Undo", CompositionState::UNDO);
  RegisterCompositionCommand("MoveCursorLeft",
                             CompositionState::MOVE_CURSOR_LEFT);
  RegisterCompositionCommand("MoveCursorRight",
                             CompositionState::MOVE_CURSOR_RIGHT);
  RegisterCompositionCommand("MoveCursorToBeginning",
                             CompositionState::MOVE_CURSOR_TO_BEGINNING);
  RegisterCompositionCommand("MoveCursorToEnd",
                             CompositionState::MOVE_MOVE_CURSOR_TO_END);
  RegisterCompositionCommand("Commit", CompositionState::COMMIT);
  RegisterCompositionCommand("CommitFirstSuggestion",
                             CompositionState::COMMIT_FIRST_SUGGESTION);
  RegisterCompositionCommand("Convert", CompositionState::CONVERT);
  RegisterCompositionCommand("ConvertWithoutHistory",
                             CompositionState::CONVERT_WITHOUT_HISTORY);
  RegisterCompositionCommand("PredictAndConvert",
                             CompositionState::PREDICT_AND_CONVERT);
  RegisterCompositionCommand("ConvertToHiragana",
                             CompositionState::CONVERT_TO_HIRAGANA);
  RegisterCompositionCommand("ConvertToFullKatakana",
                             CompositionState::CONVERT_TO_FULL_KATAKANA);
  RegisterCompositionCommand("ConvertToHalfKatakana",
                             CompositionState::CONVERT_TO_HALF_KATAKANA);
  RegisterCompositionCommand("ConvertToHalfWidth",
                             CompositionState::CONVERT_TO_HALF_WIDTH);
  RegisterCompositionCommand("ConvertToFullAlphanumeric",
                             CompositionState::CONVERT_TO_FULL_ALPHANUMERIC);
  RegisterCompositionCommand("ConvertToHalfAlphanumeric",
                             CompositionState::CONVERT_TO_HALF_ALPHANUMERIC);
  RegisterCompositionCommand("SwitchKanaType",
                             CompositionState::SWITCH_KANA_TYPE);
  RegisterCompositionCommand("DisplayAsHiragana",
                             CompositionState::DISPLAY_AS_HIRAGANA);
  RegisterCompositionCommand("DisplayAsFullKatakana",
                             CompositionState::DISPLAY_AS_FULL_KATAKANA);
  RegisterCompositionCommand("DisplayAsHalfKatakana",
                             CompositionState::DISPLAY_AS_HALF_KATAKANA);
  RegisterCompositionCommand("DisplayAsHalfWidth",
                             CompositionState::TRANSLATE_HALF_WIDTH);
  RegisterCompositionCommand("DisplayAsFullAlphanumeric",
                             CompositionState::TRANSLATE_FULL_ASCII);
  RegisterCompositionCommand("DisplayAsHalfAlphanumeric",
                             CompositionState::TRANSLATE_HALF_ASCII);
  RegisterCompositionCommand("ToggleAlphanumericMode",
                             CompositionState::TOGGLE_ALPHANUMERIC_MODE);
  if (kInputModeXCommandSupported) {
    RegisterCompositionCommand("InputModeHiragana",
                               CompositionState::INPUT_MODE_HIRAGANA);
    RegisterCompositionCommand("InputModeFullKatakana",
                               CompositionState::INPUT_MODE_FULL_KATAKANA);
    RegisterCompositionCommand("InputModeHalfKatakana",
                               CompositionState::INPUT_MODE_HALF_KATAKANA);
    RegisterCompositionCommand("InputModeFullAlphanumeric",
                               CompositionState::INPUT_MODE_FULL_ALPHANUMERIC);
    RegisterCompositionCommand("InputModeHalfAlphanumeric",
                               CompositionState::INPUT_MODE_HALF_ALPHANUMERIC);
  } else {
    RegisterCompositionCommand("InputModeHiragana", CompositionState::NONE);
    RegisterCompositionCommand("InputModeFullKatakana", CompositionState::NONE);
    RegisterCompositionCommand("InputModeHalfKatakana", CompositionState::NONE);
    RegisterCompositionCommand("InputModeFullAlphanumeric",
                               CompositionState::NONE);
    RegisterCompositionCommand("InputModeHalfAlphanumeric",
                               CompositionState::NONE);
  }

  // Conversion
  RegisterConversionCommand("IMEOff", ConversionState::IME_OFF);
  RegisterConversionCommand("IMEOn", ConversionState::IME_ON);
  RegisterConversionCommand("InsertCharacter",
                            ConversionState::INSERT_CHARACTER);
  RegisterConversionCommand("InsertSpace", ConversionState::INSERT_SPACE);
  RegisterConversionCommand("InsertAlternateSpace",
                            ConversionState::INSERT_ALTERNATE_SPACE);
  RegisterConversionCommand("InsertHalfSpace",
                            ConversionState::INSERT_HALF_SPACE);
  RegisterConversionCommand("InsertFullSpace",
                            ConversionState::INSERT_FULL_SPACE);
  RegisterConversionCommand("Cancel", ConversionState::CANCEL);
  RegisterConversionCommand("CancelAndIMEOff",
                            ConversionState::CANCEL_AND_IME_OFF);
  RegisterConversionCommand("Undo", ConversionState::UNDO);
  RegisterConversionCommand("SegmentFocusLeft",
                            ConversionState::SEGMENT_FOCUS_LEFT);
  RegisterConversionCommand("SegmentFocusRight",
                            ConversionState::SEGMENT_FOCUS_RIGHT);
  RegisterConversionCommand("SegmentFocusFirst",
                            ConversionState::SEGMENT_FOCUS_FIRST);
  RegisterConversionCommand("SegmentFocusLast",
                            ConversionState::SEGMENT_FOCUS_LAST);
  RegisterConversionCommand("SegmentWidthExpand",
                            ConversionState::SEGMENT_WIDTH_EXPAND);
  RegisterConversionCommand("SegmentWidthShrink",
                            ConversionState::SEGMENT_WIDTH_SHRINK);
  RegisterConversionCommand("ConvertNext", ConversionState::CONVERT_NEXT);
  RegisterConversionCommand("ConvertPrev", ConversionState::CONVERT_PREV);
  RegisterConversionCommand("ConvertNextPage",
                            ConversionState::CONVERT_NEXT_PAGE);
  RegisterConversionCommand("ConvertPrevPage",
                            ConversionState::CONVERT_PREV_PAGE);
  RegisterConversionCommand("PredictAndConvert",
                            ConversionState::PREDICT_AND_CONVERT);
  RegisterConversionCommand("Commit", ConversionState::COMMIT);
  RegisterConversionCommand("CommitOnlyFirstSegment",
                            ConversionState::COMMIT_SEGMENT);
  RegisterConversionCommand("ConvertToHiragana",
                            ConversionState::CONVERT_TO_HIRAGANA);
  RegisterConversionCommand("ConvertToFullKatakana",
                            ConversionState::CONVERT_TO_FULL_KATAKANA);
  RegisterConversionCommand("ConvertToHalfKatakana",
                            ConversionState::CONVERT_TO_HALF_KATAKANA);
  RegisterConversionCommand("ConvertToHalfWidth",
                            ConversionState::CONVERT_TO_HALF_WIDTH);
  RegisterConversionCommand("ConvertToFullAlphanumeric",
                            ConversionState::CONVERT_TO_FULL_ALPHANUMERIC);
  RegisterConversionCommand("ConvertToHalfAlphanumeric",
                            ConversionState::CONVERT_TO_HALF_ALPHANUMERIC);
  RegisterConversionCommand("SwitchKanaType",
                            ConversionState::SWITCH_KANA_TYPE);
  RegisterConversionCommand("ToggleAlphanumericMode",
                            ConversionState::TOGGLE_ALPHANUMERIC_MODE);
  RegisterConversionCommand("DisplayAsHiragana",
                            ConversionState::DISPLAY_AS_HIRAGANA);
  RegisterConversionCommand("DisplayAsFullKatakana",
                            ConversionState::DISPLAY_AS_FULL_KATAKANA);
  RegisterConversionCommand("DisplayAsHalfKatakana",
                            ConversionState::DISPLAY_AS_HALF_KATAKANA);
  RegisterConversionCommand("DisplayAsHalfWidth",
                            ConversionState::TRANSLATE_HALF_WIDTH);
  RegisterConversionCommand("DisplayAsFullAlphanumeric",
                            ConversionState::TRANSLATE_FULL_ASCII);
  RegisterConversionCommand("DisplayAsHalfAlphanumeric",
                            ConversionState::TRANSLATE_HALF_ASCII);
  RegisterConversionCommand("DeleteSelectedCandidate",
                            ConversionState::DELETE_SELECTED_CANDIDATE);
  if (kInputModeXCommandSupported) {
    RegisterConversionCommand("InputModeHiragana",
                              ConversionState::INPUT_MODE_HIRAGANA);
    RegisterConversionCommand("InputModeFullKatakana",
                              ConversionState::INPUT_MODE_FULL_KATAKANA);
    RegisterConversionCommand("InputModeHalfKatakana",
                              ConversionState::INPUT_MODE_HALF_KATAKANA);
    RegisterConversionCommand("InputModeFullAlphanumeric",
                              ConversionState::INPUT_MODE_FULL_ALPHANUMERIC);
    RegisterConversionCommand("InputModeHalfAlphanumeric",
                              ConversionState::INPUT_MODE_HALF_ALPHANUMERIC);
  } else {
    RegisterConversionCommand("InputModeHiragana", ConversionState::NONE);
    RegisterConversionCommand("InputModeFullKatakana", ConversionState::NONE);
    RegisterConversionCommand("InputModeHalfKatakana", ConversionState::NONE);
    RegisterConversionCommand("InputModeFullAlphanumeric",
                              ConversionState::NONE);
    RegisterConversionCommand("InputModeHalfAlphanumeric",
                              ConversionState::NONE);
  }
#ifndef NDEBUG  // means NOT RELEASE build
  RegisterConversionCommand("ReportBug", ConversionState::REPORT_BUG);
#endif  // NDEBUG
}

bool KeyMapManager::GetCommandDirect(
    const commands::KeyEvent& key_event,
    DirectInputState::Commands* command) const {
  return keymap_direct_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandPrecomposition(
    const commands::KeyEvent& key_event,
    PrecompositionState::Commands* command) const {
  return keymap_precomposition_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandComposition(
    const commands::KeyEvent& key_event,
    CompositionState::Commands* command) const {
  return keymap_composition_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandZeroQuerySuggestion(
    const commands::KeyEvent& key_event,
    PrecompositionState::Commands* command) const {
  // try zero query suggestion rule first
  if (keymap_zero_query_suggestion_.GetCommand(key_event, command)) {
    return true;
  }
  // use precomposition rule
  return keymap_precomposition_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandSuggestion(
    const commands::KeyEvent& key_event,
    CompositionState::Commands* command) const {
  // try suggestion rule first
  if (keymap_suggestion_.GetCommand(key_event, command)) {
    return true;
  }
  // use composition rule
  return keymap_composition_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandConversion(
    const commands::KeyEvent& key_event,
    ConversionState::Commands* command) const {
  return keymap_conversion_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandPrediction(
    const commands::KeyEvent& key_event,
    ConversionState::Commands* command) const {
  // try prediction rule first
  if (keymap_prediction_.GetCommand(key_event, command)) {
    return true;
  }
  // use conversion rule
  return keymap_conversion_.GetCommand(key_event, command);
}

bool KeyMapManager::ParseCommandDirect(
    const std::string& command_string,
    DirectInputState::Commands* command) const {
  const auto it = command_direct_map_.find(command_string);
  if (it == command_direct_map_.end()) {
    return false;
  }
  *command = it->second;
  return true;
}

// This should be in KeyMap instead of KeyMapManager.
bool KeyMapManager::ParseCommandPrecomposition(
    const std::string& command_string,
    PrecompositionState::Commands* command) const {
  const auto it = command_precomposition_map_.find(command_string);
  if (it == command_precomposition_map_.end()) {
    return false;
  }
  *command = it->second;
  return true;
}

bool KeyMapManager::ParseCommandComposition(
    const std::string& command_string,
    CompositionState::Commands* command) const {
  const auto it = command_composition_map_.find(command_string);
  if (it == command_composition_map_.end()) {
    return false;
  }
  *command = it->second;
  return true;
}

bool KeyMapManager::ParseCommandConversion(
    const std::string& command_string,
    ConversionState::Commands* command) const {
  const auto it = command_conversion_map_.find(command_string);
  if (it == command_conversion_map_.end()) {
    return false;
  }
  *command = it->second;
  return true;
}

void KeyMapManager::AppendAvailableCommandNameDirect(
    absl::flat_hash_set<std::string>& command_names) const {
  for (const auto& [key, value] : command_direct_map_) {
    command_names.insert(key);
  }
}

void KeyMapManager::AppendAvailableCommandNamePrecomposition(
    absl::flat_hash_set<std::string>& command_names) const {
  for (const auto& [key, value] : command_precomposition_map_) {
    command_names.insert(key);
  }
}

void KeyMapManager::AppendAvailableCommandNameComposition(
    absl::flat_hash_set<std::string>& command_names) const {
  for (const auto& [key, value] : command_composition_map_) {
    command_names.insert(key);
  }
}

void KeyMapManager::AppendAvailableCommandNameConversion(
    absl::flat_hash_set<std::string>& command_names) const {
  for (const auto& [key, value] : command_conversion_map_) {
    command_names.insert(key);
  }
}

void KeyMapManager::AppendAvailableCommandNameZeroQuerySuggestion(
    absl::flat_hash_set<std::string>& command_names) const {
  AppendAvailableCommandNamePrecomposition(command_names);
}

void KeyMapManager::AppendAvailableCommandNameSuggestion(
    absl::flat_hash_set<std::string>& command_names) const {
  AppendAvailableCommandNameComposition(command_names);
}

void KeyMapManager::AppendAvailableCommandNamePrediction(
    absl::flat_hash_set<std::string>& command_names) const {
  AppendAvailableCommandNameConversion(command_names);
}

}  // namespace keymap
}  // namespace mozc
