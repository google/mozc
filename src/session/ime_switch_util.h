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

#ifndef MOZC_SESSION_IME_SWITCH_UTIL_H_
#define MOZC_SESSION_IME_SWITCH_UTIL_H_

#include <vector>

#include "base/port.h"
#include "session/commands.pb.h"

namespace mozc {
namespace config {
class ImeSwitchUtil {
 public:
  // Returns true if 'key' is assigned for IMEOn, IMEToggle or input mode change
  // in direct mode.
  // We want to know this configuration before starting mozc server,
  // because we use this config to start mozc server.
  // Please call this fuction only when the server is not runnning.
  static bool IsTurnOnInDirectMode(const commands::KeyEvent &key);

  // Returns the copy of key event list.
  // They are needed for Windows.
  // On Windows, we should register these hotkeys on IME activation.
  static void GetTurnOnInDirectModeKeyEventList(
      vector<commands::KeyEvent> *key_events);

  static void Reload();

 private:
  // Should never be allocated.
  DISALLOW_COPY_AND_ASSIGN(ImeSwitchUtil);
};
}  // namespace config
}  // namespace mozc

#endif  // MOZC_SESSION_IME_SWITCH_UTIL_H_
