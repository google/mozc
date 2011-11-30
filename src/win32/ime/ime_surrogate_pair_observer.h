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

#ifndef MOZC_WIN32_IME_IME_SURROGATE_PAIR_OBSERVER_H_
#define MOZC_WIN32_IME_IME_SURROGATE_PAIR_OBSERVER_H_

#include <windows.h>

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "win32/ime/ime_keyboard.h"

namespace mozc {
namespace commands {
class Output;
}  // namespace commands

namespace win32 {

class SurrogatePairObserver {
 public:
  // Return code which represents the expected action of the IME DLL.
  enum ClientActionType {
    // This key event is not a VK_PACKET-related event.
    // The the caller must do the default action.
    DO_DEFAULT_ACTION = 0,
    // This key event is a VK_PACKET-related event.
    // The caller replace the VirtualKey instance with new one which conatins
    // returned ucs4 character code.
    // Then the caller must proceed to do the default action.
    DO_DEFAULT_ACTION_WITH_RETURNED_UCS4,
    // This key event must be consumed silently. In other words, the caller
    // must not send this event to the Mozc server.
    CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER,
    CLIENT_ACTION_LAST = 0xffffffff,
  };

  struct ClientAction {
    inline ClientAction(ClientActionType type, char32 ucs4)
         : type(type), ucs4(ucs4) {}
    const ClientActionType type;
    const char32 ucs4;
  };

  SurrogatePairObserver();

  ClientAction OnTestKeyEvent(const VirtualKey &virtual_key, bool is_keydown);
  ClientAction OnKeyEvent(const VirtualKey &virtual_key, bool is_keydown);

 private:
  enum ObservationState {
    INITIAL_STATE = 0,
    WAIT_FOR_SURROGATE_HIGH_UP,
    WAIT_FOR_SURROGATE_LOW_DOWM,
    WAIT_FOR_SURROGATE_LOW_UP,
    OBSERVATION_STATE_LAST = 0xffffffff,
  };

  // Returns the expected action of the IME DLL against the given key event.
  ClientAction OnKeyEvent(const VirtualKey &virtual_key, bool is_keydown,
                          bool is_testkey);

  ObservationState state_;
  wchar_t surrogate_high_;
  wchar_t surrogate_low_;

  DISALLOW_COPY_AND_ASSIGN(SurrogatePairObserver);
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_IME_IME_SURROGATE_PAIR_OBSERVER_H_
