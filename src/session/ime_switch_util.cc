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

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "base/config_file_stream.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "session/commands.pb.h"
#include "session/internal/keymap.h"
#include "session/key_event_normalizer.h"
#include "session/key_parser.h"

namespace mozc {
namespace config {
namespace {
const char kModeDirect[] = "Direct";
const char kModeDirectInput[] = "DirectInput";
const char kCommandIMEOn[] = "IMEOn";
const char kCommandHiragana[] = "InputModeHiragana";
const char kCommandFullKatakana[] = "InputModeFullKatakana";
const char kCommandHalfKatakana[] = "InputModeHalfKatakana";
const char kCommandFullAlphanumeric[] = "InputModeFullAlphanumeric";
const char kCommandHalfAlphanumeric[] = "InputModeHalfAlphanumeric";
const char kCommandReconvert[] = "Reconvert";

class ImeSwitchUtilImpl {
 public:
  ImeSwitchUtilImpl() {
    Reload();
  }

  virtual ~ImeSwitchUtilImpl() {}

  bool IsTurnOnInDirectMode(const commands::KeyEvent &key_event) const {
    uint64 key;
    if (!KeyEventNormalizer::ToUint64(key_event, &key)) {
      return false;
    }
    for (size_t i = 0; i < ime_on_keyevents_.size(); ++i) {
      uint64 ime_on_key;
      if (!KeyEventNormalizer::ToUint64(ime_on_keyevents_[i], &ime_on_key)) {
        continue;
      }
      if (key == ime_on_key) {
        return true;
      }
    }
    return false;
  }

  void Reload() {
    ime_on_keyevents_.clear();
    const config::Config::SessionKeymap keymap = GET_CONFIG(session_keymap);
    if (keymap == Config::CUSTOM) {
      const string &custom_keymap_table = GET_CONFIG(custom_keymap_table);
      if (custom_keymap_table.empty()) {
        LOG(WARNING) << "custom_keymap_table is empty. use default setting";
        const char *default_keymapfile =
            keymap::KeyMapManager::GetKeyMapFileName(
                keymap::KeyMapManager::GetDefaultKeyMap());
        ReloadFromFile(default_keymapfile);
        return;
      }
      istringstream ifs(custom_keymap_table);
      ReloadFromStream(&ifs);
      CheckMigration();
      return;
    }
    const char *keymap_file = keymap::KeyMapManager::GetKeyMapFileName(keymap);
    ReloadFromFile(keymap_file);
  }

  void GetTurnOnInDirectModeKeyEventList(
      vector<commands::KeyEvent> *key_events) const {
    *key_events = ime_on_keyevents_;
  }

 private:
  void CheckMigration() {
    // migration code
    // Add hankaku to ime_on_keyevents_ if the settings seem to
    // be from previous version.
    // This code can deleted after IME ON/OFF feature migration.
    // TODO(toshiyuki): remove this after 0.11 Beta.
    const char kKeyOn[] = "On";
    const char kKeyEisu[] = "Eisu";
    const char kKeyHankaku[] = "Hankaku/Zenkaku";
    const char kKeyKanji[] = "Kanji";

    uint64 key_on = 0;
    uint64 key_eisu = 0;
    {
      commands::KeyEvent key_event;
      KeyParser::ParseKey(kKeyOn, &key_event);
      KeyEventNormalizer::ToUint64(key_event, &key_on);
    }
    {
      commands::KeyEvent key_event;
      KeyParser::ParseKey(kKeyEisu, &key_event);
      KeyEventNormalizer::ToUint64(key_event, &key_eisu);
    }
    bool should_be_migrated = true;
    for (size_t i = 0; i < ime_on_keyevents_.size(); ++i) {
      uint64 key = 0;
      KeyEventNormalizer::ToUint64(ime_on_keyevents_[i], &key);
      if (key != key_on && key != key_eisu) {
        should_be_migrated = false;
        break;
      }
    }
    if (should_be_migrated) {
      commands::KeyEvent key_hankaku;
      commands::KeyEvent key_kanji;
      KeyParser::ParseKey(kKeyHankaku, &key_hankaku);
      KeyParser::ParseKey(kKeyKanji, &key_kanji);
      ime_on_keyevents_.push_back(key_hankaku);
      ime_on_keyevents_.push_back(key_kanji);;
    }
  }

  void ReloadFromStream(istream *ifs) {
    string line;
    getline(*ifs, line);  // Skip the first line.
    while (!ifs->eof()) {
      getline(*ifs, line);
      Util::ChopReturns(&line);
      if (line.empty() || line[0] == '#') {
        // empty or comment
        continue;
      }
      vector<string> rules;
      Util::SplitStringUsing(line, "\t", &rules);
      if (rules.size() != 3) {
        LOG(ERROR) << "Invalid format: " << line;
        continue;
      }
      if (!((rules[0] == kModeDirect || rules[0] == kModeDirectInput) &&
            (rules[2] == kCommandIMEOn ||
             rules[2] == kCommandHiragana ||
             rules[2] == kCommandFullKatakana ||
             rules[2] == kCommandHalfKatakana ||
             rules[2] == kCommandFullAlphanumeric ||
             rules[2] == kCommandHalfAlphanumeric ||
             rules[2] == kCommandReconvert))) {
        continue;
      }
      commands::KeyEvent key_event;
      KeyParser::ParseKey(rules[1], &key_event);
      ime_on_keyevents_.push_back(key_event);
    }
  }

  void ReloadFromFile(const string &filename) {
    scoped_ptr<istream> ifs(ConfigFileStream::Open(filename));
    if (ifs.get() == NULL) {
      DLOG(FATAL) << "could not open file: " << filename;
      return;
    }
    return ReloadFromStream(ifs.get());
  }

  // original forms
  vector<commands::KeyEvent> ime_on_keyevents_;
};

}  // namespace

bool ImeSwitchUtil::IsTurnOnInDirectMode(const commands::KeyEvent &key) {
  return Singleton<ImeSwitchUtilImpl>::get()->IsTurnOnInDirectMode(key);
}

void ImeSwitchUtil::GetTurnOnInDirectModeKeyEventList(
    vector<commands::KeyEvent> *key_events) {
  Singleton<ImeSwitchUtilImpl>::get()->
      GetTurnOnInDirectModeKeyEventList(key_events);
}

void ImeSwitchUtil::Reload() {
  Singleton<ImeSwitchUtilImpl>::get()->Reload();
}
}  // namespace config
}  // namespace mozc
