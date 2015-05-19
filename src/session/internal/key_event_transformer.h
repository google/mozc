// Copyright 2010-2012, Google Inc.
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

// Transform key event accoding to config.
// This class is designed for using with Singleton class.

#ifndef MOZC_SESSION_INTERNAL_TRANSFORM_TABLE_H_
#define MOZC_SESSION_INTERNAL_TRANSFORM_TABLE_H_

#include <map>
#include <string>

#include "base/port.h"
#include "base/singleton.h"
#include "config/config.pb.h"

namespace mozc {
namespace commands {
class KeyEvent;
}  // namespace commands

namespace config {
class Config;
}  // namespace config

namespace session {

class KeyEventTransformer {
 public:
  // Update transform table accoding to config.
  void ReloadConfig(const config::Config &config);

  // Transform key event accoding to transform table.
  // We should update table using ReloadConfig() before calling a this function.
  bool TransformKeyEvent(commands::KeyEvent *key_event);

 private:
  friend class Singleton<KeyEventTransformer>;

  KeyEventTransformer();
  virtual ~KeyEventTransformer();

  // Transform the key event base on the rule.  This function is used
  // for special treatment with numpad keys.
  bool TransformKeyEventForNumpad(commands::KeyEvent *key_event);

  // Transform symbols for Kana input.  Character transformation for
  // Romanji input is performed in preedit/table.cc
  bool TransformKeyEventForKana(commands::KeyEvent *key_event);

  typedef map<string, commands::KeyEvent> Table;
  Table table_;
  config::Config::NumpadCharacterForm numpad_character_form_;

  DISALLOW_COPY_AND_ASSIGN(KeyEventTransformer);
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_INTERNAL_TRANSFORM_TABLE_H_
