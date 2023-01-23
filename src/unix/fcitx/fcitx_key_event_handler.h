// Copyright 2010-2012, Google Inc.
// Copyright 2012~2013, Weng Xuetian <wengxt@gmail.com>
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

#ifndef MOZC_UNIX_FCITX_KEY_EVENT_HANDLER_H_
#define MOZC_UNIX_FCITX_KEY_EVENT_HANDLER_H_

#include <set>
#include <memory>
#include <cstdint>

#include "base/port.h"
#include "protocol/config.pb.h"
#include "protocol/commands.pb.h"
#include "unix/fcitx/fcitx_key_translator.h"

namespace mozc {
namespace fcitx {

class KeyEventHandler {
 public:
  KeyEventHandler();
  KeyEventHandler(const KeyEventHandler &) = delete;

  // Converts a key event came from fcitx to commands::KeyEvent. This is a
  // stateful method. It stores modifier keys states since ibus doesn't send
  // an enough information about the modifier keys.
  bool GetKeyEvent(FcitxKeySym keyval, uint32_t keycode, uint32_t modifiers,
                   config::Config::PreeditMethod preedit_method,
                   bool layout_is_jp, bool is_key_up, commands::KeyEvent *key);

  // Clears states.
  void Clear();

 private:

  // Manages modifier keys. Returns false if it should not be sent to server.
  bool ProcessModifiers(bool is_key_up, uint32_t keyval,
                        commands::KeyEvent *key_event);

  std::unique_ptr<KeyTranslator> key_translator_;
  // Non modifier key is pressed or not after all keys are released.
  bool is_non_modifier_key_pressed_;
  // Currently pressed modifier keys.  It is set of keyval.
  std::set<uint32_t> currently_pressed_modifiers_;
  // Pending modifier keys.
  std::set<commands::KeyEvent::ModifierKey> modifiers_to_be_sent_;
};

}  // namespace fcitx
}  // namespace mozc

#endif  // MOZC_UNIX_FCITX_KEY_EVENT_HANDLER_H_
