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

#ifndef MOZC_UNIX_IBUS_KEY_TRANSLATOR_H_
#define MOZC_UNIX_IBUS_KEY_TRANSLATOR_H_

#include <map>

#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "unix/ibus/ibus_header.h"

namespace mozc {
namespace ibus {

// This class is responsible for converting key code sent from ibus-daemon
// (defined in /usr/include/ibus-1.0/ibuskeysyms.h) to a KeyEvent object for
// the input of session_interface.
class KeyTranslator {
 public:
  KeyTranslator() = default;
  KeyTranslator(const KeyTranslator &) = delete;
  KeyTranslator &operator=(const KeyTranslator &) = delete;
  virtual ~KeyTranslator() = default;

  // Converts ibus keycode to Mozc key code and stores them on |out_event|.
  // Returns true if ibus keycode is successfully converted to Mozc key code.
  bool Translate(uint keyval, uint keycode, uint modifiers,
                 config::Config::PreeditMethod method, bool layout_is_jp,
                 commands::KeyEvent *out_event) const;

 private:
  // Returns true iff |keyval| is a key with a kana assigned.
  bool IsKanaAvailable(uint keyval, uint keycode, uint modifiers,
                       bool layout_is_jp, std::string *out) const;

  // Returns true iff key is ASCII such as '0', 'A', or '!'.
  static bool IsAscii(uint keyval, uint keycode, uint modifiers);

  // Returns true iff key is printable.
  static bool IsPrintable(uint keyval, uint keycode, uint modifiers);

  // Returns true iff key is HiraganaKatakana with shift modifier.
  static bool IsHiraganaKatakanaKeyWithShift(uint keyval, uint keycode,
                                             uint modifiers);
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_KEY_TRANSLATOR_H_
