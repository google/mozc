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

#ifndef MOZC_SESSION_INTERNAL_KEYMAP_H_
#define MOZC_SESSION_INTERNAL_KEYMAP_H_

#include <istream>
#include <map>
#include <set>
#include <string>
#include "config/config.pb.h"
#include "session/internal/keymap_interface.h"

namespace mozc {

namespace commands {
class KeyEvent;
}

namespace keymap {

typedef uint64 Key;  // modifiers (63-32 bits) + keycode (31-0 bits)

// TODO(komatsu): Refactor these functions when refactoring the session class.

// Normalizes given key event for key command looking-up. This function does
// - remove commands::KeyEvent::CAPS from the modifier keys
// - revert the flip of alphabetical key code caused by CapsLock
// so that shortcut keys can be used as if CapsLock was not enabled. b/5627459
void NormalizeKeyEvent(const commands::KeyEvent &key_event,
                       commands::KeyEvent *new_key_event);
bool GetKey(const commands::KeyEvent &key_event, Key *key);
bool MaybeGetKeyStub(const commands::KeyEvent &key_event, Key *key);

template<typename T>
class KeyMap : public KeyMapInterface<T> {
 public:
  bool GetCommand(const commands::KeyEvent &key_event, T *command) const;
  bool AddRule(const commands::KeyEvent &key_event, T command);
  void Clear();

 private:
  map<Key, T> keymap_;
};


class KeyMapManager {
 public:
  KeyMapManager();
  ~KeyMapManager();

  bool Initialize();

  // Reloads the key map by using given configuration.
  bool ReloadWithKeymap(const config::Config::SessionKeymap new_keymap);

  bool LoadFile(const char *filename);
  bool LoadStream(istream *is);
  bool LoadStreamWithErrors(istream *ifs, vector<string> *errors);

  // Add a command bound with state and key_event.
  bool AddCommand(const string &state_name,
                  const string &key_event_name,
                  const string &command_name);

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
                                string *name) const;
  bool GetNameFromCommandPrecomposition(PrecompositionState::Commands command,
                                        string *name) const;
  bool GetNameFromCommandComposition(CompositionState::Commands command,
                                     string *name) const;
  bool GetNameFromCommandConversion(ConversionState::Commands command,
                                    string *name) const;

  // Get command names
  void GetAvailableCommandNameDirect(set<string> *command_names) const;
  void GetAvailableCommandNamePrecomposition(set<string> *command_names) const;
  void GetAvailableCommandNameComposition(set<string> *command_names) const;
  void GetAvailableCommandNameConversion(set<string> *command_names) const;
  void GetAvailableCommandNameZeroQuerySuggestion(
      set<string> *command_names) const;
  void GetAvailableCommandNameSuggestion(set<string> *command_names) const;
  void GetAvailableCommandNamePrediction(set<string> *command_names) const;

  // Return the file name bound with the keymap enum.
  static const char *GetKeyMapFileName(config::Config::SessionKeymap keymap);
  // Return the default keymap.
  static config::Config::SessionKeymap GetDefaultKeyMap();

 private:
  void InitCommandData();

  bool ParseCommandDirect(const string &command_string,
                          DirectInputState::Commands *command) const;
  bool ParseCommandPrecomposition(const string &command_string,
                               PrecompositionState::Commands *command) const;
  bool ParseCommandComposition(const string &command_string,
                           CompositionState::Commands *command) const;
  bool ParseCommandConversion(const string &command_string,
                              ConversionState::Commands *command) const;
  void RegisterDirectCommand(const string &command_string,
                             DirectInputState::Commands command);
  void RegisterPrecompositionCommand(const string &command_string,
                                     PrecompositionState::Commands command);
  void RegisterCompositionCommand(const string &command_string,
                                  CompositionState::Commands command);
  void RegisterConversionCommand(const string &command_string,
                                 ConversionState::Commands command);

  // Migration code for CUSTOM keymap.
  // If custom keymap has no IMEOf/Off key settings, add them and save
  // to custom keymap.
  void CheckIMEOnOffKeymap();
  // This is used for migration.
  // This stores ime on/off related keyevent.
  // If this contains only ON, OFF or EISU, this settings should be
  // migrated.
  set<uint64> ime_on_off_keys_;

  config::Config::SessionKeymap keymap_;
  map<string, DirectInputState::Commands> command_direct_map_;
  map<string, PrecompositionState::Commands> command_precomposition_map_;
  map<string, CompositionState::Commands> command_composition_map_;
  map<string, ConversionState::Commands> command_conversion_map_;

  map<DirectInputState::Commands, string> reverse_command_direct_map_;
  map<PrecompositionState::Commands, string>
      reverse_command_precomposition_map_;
  map<CompositionState::Commands, string> reverse_command_composition_map_;
  map<ConversionState::Commands, string> reverse_command_conversion_map_;

  // Status should be out of keymap.
  keymap::KeyMap<keymap::DirectInputState::Commands> keymap_direct_;
  keymap::KeyMap<keymap::PrecompositionState::Commands> keymap_precomposition_;
  keymap::KeyMap<keymap::CompositionState::Commands> keymap_composition_;
  keymap::KeyMap<keymap::ConversionState::Commands> keymap_conversion_;

  // enabled only if zero query suggestion is shown. Otherwise, inherit from
  // keymap_precomposition
  keymap::KeyMap<keymap::PrecompositionState::Commands>
      keymap_zero_query_suggestion_;

  // enabled only if suggestion is shown. Otherwise, inherit from
  // keymap_composition
  keymap::KeyMap<keymap::CompositionState::Commands> keymap_suggestion_;

  // enabled only if prediction is shown. Otherwise, inherit from
  // keymap_conversion
  keymap::KeyMap<keymap::ConversionState::Commands> keymap_prediction_;

  DISALLOW_COPY_AND_ASSIGN(KeyMapManager);
};

}  // namespace keymap
}  // namespace mozc

#endif  // MOZC_SESSION_INTERNAL_KEYMAP_H_
