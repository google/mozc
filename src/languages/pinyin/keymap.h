// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_LANGUAGES_PINYIN_KEYMAP_H_
#define MOZC_LANGUAGES_PINYIN_KEYMAP_H_

#include <map>

#include "base/port.h"
#include "languages/pinyin/pinyin_constant.h"

namespace mozc {
namespace commands {
class KeyEvent;
}  // namespace commands

namespace pinyin {
namespace keymap {

class KeymapInterface {
 public:
  virtual ~KeymapInterface() {}
  // Parses key_event and gets command. This method does not handle CapsLock or
  // Numlock keys. These keys should be removed before calling this method.
  virtual bool GetCommand(const commands::KeyEvent &key_event,
                          ConverterState state,
                          KeyCommand *key_command) const = 0;
};

class KeymapFactory {
 public:
  static const KeymapInterface *GetKeymap(KeymapMode mode);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(KeymapFactory);
};

}  // namespace keymap
}  // namespace pinyin
}  // namespace mozc

#endif  // MOZC_LANGUAGES_PINYIN_KEYMAP_H_
