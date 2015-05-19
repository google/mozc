// Copyright 2010-2014, Google Inc.
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

#include "session/key_info_util.h"

#include <sstream>
#include <vector>

#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "session/commands.pb.h"
#include "session/internal/keymap.h"
#include "session/key_parser.h"

namespace mozc {
using commands::KeyEvent;

namespace {

using config::Config;

vector<KeyInformation> ExtractSortedDirectModeKeysFromStream(istream *ifs) {
  const char kModeDirect[] = "Direct";
  const char kModeDirectInput[] = "DirectInput";

  vector<KeyInformation> result;

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
    if (!(rules[0] == kModeDirect || rules[0] == kModeDirectInput)) {
      continue;
    }
    commands::KeyEvent key_event;
    if (KeyParser::ParseKey(rules[1], &key_event)) {
      KeyInformation info;
      if (KeyEventUtil::GetKeyInformation(key_event, &info)) {
        result.push_back(info);
      }
    }
  }

  sort(result.begin(), result.end());
  return result;
}

vector<KeyInformation> ExtractSortedDirectModeKeysFromFile(
      const string &filename) {
  scoped_ptr<istream> ifs(ConfigFileStream::LegacyOpen(filename));
  if (ifs.get() == NULL) {
    DLOG(FATAL) << "could not open file: " << filename;
    return vector<KeyInformation>();
  }
  return ExtractSortedDirectModeKeysFromStream(ifs.get());
}

}  // namespace

vector<KeyInformation> KeyInfoUtil::ExtractSortedDirectModeKeys(
    const config::Config &config) {
  const config::Config::SessionKeymap &keymap = config.session_keymap();
  if (keymap == Config::CUSTOM) {
    const string &custom_keymap_table = config.custom_keymap_table();
    if (custom_keymap_table.empty()) {
      LOG(WARNING) << "custom_keymap_table is empty. use default setting";
      const char *default_keymapfile = keymap::KeyMapManager::GetKeyMapFileName(
          keymap::KeyMapManager::GetDefaultKeyMap());
      return ExtractSortedDirectModeKeysFromFile(default_keymapfile);
    }
    istringstream ifs(custom_keymap_table);
    return ExtractSortedDirectModeKeysFromStream(&ifs);
  }
  const char *keymap_file = keymap::KeyMapManager::GetKeyMapFileName(keymap);
  return ExtractSortedDirectModeKeysFromFile(keymap_file);
}

bool KeyInfoUtil::ContainsKey(const vector<KeyInformation> &sorted_keys,
                              const commands::KeyEvent &key_event) {
  KeyInformation key_info;
  if (!KeyEventUtil::GetKeyInformation(key_event, &key_info)) {
    return false;
  }
  return binary_search(sorted_keys.begin(), sorted_keys.end(), key_info);
}

}  // namespace mozc
