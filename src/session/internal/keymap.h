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

#ifndef MOZC_SESSION_INTERNAL_KEYMAP_H_
#define MOZC_SESSION_INTERNAL_KEYMAP_H_

#include <istream>  // NOLINT
#include <map>
#include <set>
#include <string>
#include <vector>

#include "composer/key_event_util.h"
#include "protocol/config.pb.h"
#include "session/internal/keymap_interface.h"
#include "testing/base/public/gunit_prod.h"
#include "absl/container/btree_set.h"

namespace mozc {

namespace commands {
class KeyEvent;
}

namespace keymap {

template <typename T>
class KeyMap : public KeyMapInterface<typename T::Commands> {
 public:
  typedef typename T::Commands CommandsType;

  bool GetCommand(const commands::KeyEvent &key_event,
                  CommandsType *command) const override;
  bool AddRule(const commands::KeyEvent &key_event,
               CommandsType command) override;
  void Clear();

 private:
  typedef std::map<KeyInformation, CommandsType> KeyToCommandMap;
  KeyToCommandMap keymap_;
};

// A manager of key mapping rule for a SessionKeymap.
// When running as a decoder, an instance is always tied to
// an immutable `SessionKeymap`, which is set by the constructors.
class KeyMapManager {
 public:
  // Default ctor for GUI config editor. keymap is NONE.
  KeyMapManager();
  // Decoder should explicitly set the keymap.
  explicit KeyMapManager(config::Config::SessionKeymap keymap);
  ~KeyMapManager();

  config::Config::SessionKeymap GetKeymap() const { return keymap_; }

  // Reloads the key map by using given configuration.
  // `keymap_` is immutable so `config.session_keymap` must be identical to it.
  // Right now `config.custom_keymap_table` is the only reloaded content.
  bool ReloadConfig(const config::Config &config);

  bool GetCommandDirect(const commands::KeyEvent &key_event,
                        DirectInputState::Commands *command) const;

  bool GetCommandPrecomposition(const commands::KeyEvent &key_event,
                                PrecompositionState::Commands *command) const;

  bool GetCommandComposition(const commands::KeyEvent &key_event,
                             CompositionState::Commands *command) const;

  bool GetCommandConversion(const commands::KeyEvent &key_event,
                            ConversionState::Commands *command) const;

  bool GetCommandZeroQuerySuggestion(
      const commands::KeyEvent &key_event,
      PrecompositionState::Commands *command) const;

  bool GetCommandSuggestion(const commands::KeyEvent &key_event,
                            CompositionState::Commands *command) const;

  bool GetCommandPrediction(const commands::KeyEvent &key_event,
                            ConversionState::Commands *command) const;

  bool GetNameFromCommandDirect(DirectInputState::Commands command,
                                std::string *name) const;
  bool GetNameFromCommandPrecomposition(PrecompositionState::Commands command,
                                        std::string *name) const;
  bool GetNameFromCommandComposition(CompositionState::Commands command,
                                     std::string *name) const;
  bool GetNameFromCommandConversion(ConversionState::Commands command,
                                    std::string *name) const;

  // Get command names
  void GetAvailableCommandNameDirect(
      absl::btree_set<std::string> *command_names) const;
  void GetAvailableCommandNamePrecomposition(
      absl::btree_set<std::string> *command_names) const;
  void GetAvailableCommandNameComposition(
      absl::btree_set<std::string> *command_names) const;
  void GetAvailableCommandNameConversion(
      absl::btree_set<std::string> *command_names) const;
  void GetAvailableCommandNameZeroQuerySuggestion(
      absl::btree_set<std::string> *command_names) const;
  void GetAvailableCommandNameSuggestion(
      absl::btree_set<std::string> *command_names) const;
  void GetAvailableCommandNamePrediction(
      absl::btree_set<std::string> *command_names) const;

  // Return the file name bound with the keymap enum.
  static const char *GetKeyMapFileName(config::Config::SessionKeymap keymap);

 private:
  friend class KeyMapTest;
  FRIEND_TEST(KeyMapTest, AddRule);
  FRIEND_TEST(KeyMapTest, DefaultKeyBindings);
  FRIEND_TEST(KeyMapTest, LoadStreamWithErrors);
  FRIEND_TEST(KeyMapTest, AddCommand);
  FRIEND_TEST(KeyMapTest, ZeroQuerySuggestion);

  void Reset();
  void InitCommandData();
  bool Initialize();

  bool LoadFile(const char *filename);
  bool LoadStream(std::istream *ifs);
  bool LoadStreamWithErrors(std::istream *ifs,
                            std::vector<std::string> *errors);

  // Add a command bound with state and key_event.
  bool AddCommand(const std::string &state_name,
                  const std::string &key_event_name,
                  const std::string &command_name);

  bool ParseCommandDirect(const std::string &command_string,
                          DirectInputState::Commands *command) const;
  bool ParseCommandPrecomposition(const std::string &command_string,
                                  PrecompositionState::Commands *command) const;
  bool ParseCommandComposition(const std::string &command_string,
                               CompositionState::Commands *command) const;
  bool ParseCommandConversion(const std::string &command_string,
                              ConversionState::Commands *command) const;
  void RegisterDirectCommand(const std::string &command_string,
                             DirectInputState::Commands command);
  void RegisterPrecompositionCommand(const std::string &command_string,
                                     PrecompositionState::Commands command);
  void RegisterCompositionCommand(const std::string &command_string,
                                  CompositionState::Commands command);
  void RegisterConversionCommand(const std::string &command_string,
                                 ConversionState::Commands command);

  static const bool kInputModeXCommandSupported;

  // The SessionKeymap which this instance represents.
  const config::Config::SessionKeymap keymap_;
  std::map<std::string, DirectInputState::Commands> command_direct_map_;
  std::map<std::string, PrecompositionState::Commands>
      command_precomposition_map_;
  std::map<std::string, CompositionState::Commands> command_composition_map_;
  std::map<std::string, ConversionState::Commands> command_conversion_map_;

  std::map<DirectInputState::Commands, std::string> reverse_command_direct_map_;
  std::map<PrecompositionState::Commands, std::string>
      reverse_command_precomposition_map_;
  std::map<CompositionState::Commands, std::string>
      reverse_command_composition_map_;
  std::map<ConversionState::Commands, std::string>
      reverse_command_conversion_map_;

  // Status should be out of keymap.
  keymap::KeyMap<keymap::DirectInputState> keymap_direct_;
  keymap::KeyMap<keymap::PrecompositionState> keymap_precomposition_;
  keymap::KeyMap<keymap::CompositionState> keymap_composition_;
  keymap::KeyMap<keymap::ConversionState> keymap_conversion_;

  // enabled only if zero query suggestion is shown. Otherwise, inherit from
  // keymap_precomposition
  keymap::KeyMap<keymap::PrecompositionState> keymap_zero_query_suggestion_;

  // enabled only if suggestion is shown. Otherwise, inherit from
  // keymap_composition
  keymap::KeyMap<keymap::CompositionState> keymap_suggestion_;

  // enabled only if prediction is shown. Otherwise, inherit from
  // keymap_conversion
  keymap::KeyMap<keymap::ConversionState> keymap_prediction_;

  DISALLOW_COPY_AND_ASSIGN(KeyMapManager);
};

}  // namespace keymap
}  // namespace mozc

#endif  // MOZC_SESSION_INTERNAL_KEYMAP_H_
