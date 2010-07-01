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

#ifndef MOZC_UNIX_SCIM_SCIM_KEY_TRANSLATOR_H_
#define MOZC_UNIX_SCIM_SCIM_KEY_TRANSLATOR_H_

#define Uses_SCIM_FRONTEND_MODULE

#include <scim.h>

#include <map>
#include <set>
#include <string>

#include "base/base.h"  // for DISALLOW_COPY_AND_ASSIGN.
#include "session/commands.pb.h"

namespace mozc_unix_scim {

// This class is responsible for converting scim::KeyEvent object (defined in
// /usr/include/scim-1.0/scim_event.h) to IPC input for mozc_server.
class ScimKeyTranslator {
 public:
  ScimKeyTranslator();
  ~ScimKeyTranslator();

  // Converts scim_key into Mozc key code and stores them on out_translated.
  // scim_key must satisfy the following precondition: CanConvert(scim_key)
  void Translate(const scim::KeyEvent &key,
                 mozc::config::Config::PreeditMethod method,
                 mozc::commands::KeyEvent *out_event) const;

  // Converts 'left click on a candidate window' into Mozc message.
  // unique_id: Unique identifier of the clicked candidate.
  void TranslateClick(int32 unique_id,
                      mozc::commands::SessionCommand *out_command) const;

  // Returns true iff scim_key can be converted to mozc::commands::Input.
  // Note: some keys and key events, such as 'key released', 'modifier key
  // pressed', or 'special key that Mozc doesn't know pressed' cannot be
  // converted to mozc::commands::Input.
  bool CanConvert(const scim::KeyEvent &scim_key) const;

 private:
  // Returns true iff scim_key is modifier key such as SHIFT, ALT, or CAPSLOCK.
  bool IsModifierKey(const scim::KeyEvent &scim_key) const;

  // Returns true iff scim_key is special key such as ENTER, ESC, or PAGE_UP.
  bool IsSpecialKey(const scim::KeyEvent &scim_key,
                    mozc::commands::KeyEvent::SpecialKey *out) const;

  // Returns true iff scim_key is special key that can be converted to ASCII.
  // For example, scim::SCIM_KEY_KP_0 (numeric keypad zero) in SCIM can be
  // treated as ASCII code '0' in Mozc.
  bool IsSpecialAscii(const scim::KeyEvent &key, uint32 *out) const;

  // Returns true iff scim_key is key with a kana assigned.
  bool IsKanaAvailable(const scim::KeyEvent &key, string *out) const;

  // Returns true iff scim_key is ASCII such as '0', 'A', or '!'.
  static bool IsAscii(const scim::KeyEvent &scim_key);

  // Returns true iff kana_map_jp_ is to be used.
  static bool IsJapaneseLayout(uint16 layout);

  // Initializes private fields.
  void InitializeKeyMaps();

  map<uint32, mozc::commands::KeyEvent::SpecialKey> special_key_map_;
  set<uint32> modifier_keys_;
  map<uint32, uint32> special_ascii_map_;
  map<uint32, const char *> kana_map_jp_;
  map<uint32, const char *> kana_map_us_;

  DISALLOW_COPY_AND_ASSIGN(ScimKeyTranslator);
};

}  // namespace mozc_unix_scim

#endif  // MOZC_UNIX_SCIM_SCIM_KEY_TRANSLATOR_H_
