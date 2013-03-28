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

#ifndef MOZC_WIN32_BASE_DELETER_H_
#define MOZC_WIN32_BASE_DELETER_H_

#include <windows.h>

#include "base/port.h"
#include "base/scoped_ptr.h"

namespace mozc {
namespace commands {
class Output;
}  // namespace commands

namespace win32 {

struct InputState;
class VKBackBasedDeleterQueue;
class Win32KeyboardInterface;

// This class implements the *deletion_range* mechanism of the Mozc protocol.
// When the client receives an output which contains *deletion_range*,
// a certain number of keydown/up pairs of VK_BACK will be sent to the
// application before the output is applied by the IME module, like MS-IME
// and ATOK do.
// Here is how this works.
//  1. IME DLL receives an output which contains *deletion_range*.
//  2. IME DLL enqueues the output so that it will be applied after the
//     application deletes the characters to be deleted.
//  3. IME DLL generates ([required deletion count] + 1) keydown/up pairs
//     of VK_BACK.
//  4. As for ([required deletion count]) keydown/up pairs of VK_BACK will be
//     handled by the application to delete [required deletion count] of
//     characters.
//  5. The last keydown/up pair of VK_BACK will be consumed by the IME module
//     and never be sent to the application.  With these key events, the IME
//     module can interrupt just after character delete events.
// - VK_BACK down  | Delivered to the application to delete a character.
// - VK_BACK up    | Delivered to the application to do nothing.
// - VK_BACK down  | Delivered to the application to delete a character.
// - VK_BACK up    | Delivered to the application to do nothing.
// - ...
// - VK_BACK down  | Consumed by the IME to start any pending action.
// - VK_BACK up    | Consumed by the IME to call EndDeletion.
class VKBackBasedDeleter {
 public:
  enum DeletionWaitState {
    // The deleter is waiting for the first test-key-down of VK_BACK.
    WAIT_INITIAL_VK_BACK_TESTDOWN = 0,
    // The deleter is waiting for the test-key-down of VK_BACK.
    WAIT_VK_BACK_TESTDOWN,
    // The deleter is waiting for the test-key-up of VK_BACK.
    WAIT_VK_BACK_TESTUP,
    // The deleter is waiting for the key-down of VK_BACK.
    WAIT_VK_BACK_DOWN,
    // The deleter is waiting for the key-up of VK_BACK.
    WAIT_VK_BACK_UP,
    DELETTION_WAIT_STATE_LAST = 0xffffffff,
  };

  // Return code to represent the expected action of the IME DLL.
  enum ClientAction {
    // IME DLL must behave as if there is no VKBackBasedDeleter.
    // This action can be used for both ImeProcessKey (test-key-[down/up]) and
    // ImeToAsciiEx (key-[down/up]).
    DO_DEFAULT_ACTION = 0,
    // IME DLL must call VKBackBasedDeleter::EndDeletion then behave as if
    // there is no VKBackBasedDeleter.
    // This action can be used for both ImeProcessKey (test-key-[down/up]) and
    // ImeToAsciiEx (key-[down/up]).
    CALL_END_DELETION_THEN_DO_DEFAULT_ACTION,
    // IME DLL must pass this key event to the application.
    // This action can be used for ImeProcessKey (test-key-[down/up]) only.
    SEND_KEY_TO_APPLICATION,
    // IME DLL must not pass this key event to the application nor the server.
    // This action can be used for ImeProcessKey (test-key-[down/up]) only.
    CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER,
    // IME DLL must not pass this key event to the application nor the server.
    // IME DLL must call VKBackBasedDeleter::RestoreModifiers method.
    // This action can be used for both ImeProcessKey (test-key-[down/up]) and
    // ImeToAsciiEx (key-[down/up]).
    CALL_END_DELETION_BUT_NEVER_SEND_TO_SERVER,
    // IME DLL must use the pending output and |ime_state| as if the server
    // responded these data against the current key event.
    // This action can be used for ImeToAsciiEx (key-down) only.
    APPLY_PENDING_STATUS,
    DELETION_ACTION_LAST = 0xffffffff,
  };

  VKBackBasedDeleter();
  ~VKBackBasedDeleter();

  // For unit test only.
  // You can hook relevant Win32 API calls in this class for unit tests.
  // This class takes the ownership of |keyboard_mock| so the caller
  // must not free it.
  explicit VKBackBasedDeleter(Win32KeyboardInterface *keyboard_mock);

  // Initializes the deleter.
  void BeginDeletion(int deletion_count,
                     const mozc::commands::Output &output,
                     const InputState &ime_state);

  // Uninitializes the deleter.  You must call this method whenever OnKeyEvent
  // returns CALL_END_DELETION_THEN_DO_DEFAULT_ACTION or
  // CALL_END_DELETION_BUT_NEVER_SEND_TO_SERVER.
  void EndDeletion();

  // Returns the expected action of the IME DLL against the given key event.
  ClientAction OnKeyEvent(UINT vk, bool is_keydown, bool is_testkey);

  // Returns true is the deleter is waiting for any specific key event.
  bool IsDeletionOngoing() const;

  const mozc::commands::Output &pending_output() const;
  const InputState &pending_ime_state() const;

 private:
  void UnsetModifiers();

  scoped_ptr<VKBackBasedDeleterQueue> wait_queue_;
  scoped_ptr<Win32KeyboardInterface> keyboard_;
  scoped_ptr<InputState> pending_ime_state_;
  scoped_ptr<mozc::commands::Output> pending_output_;

  DISALLOW_COPY_AND_ASSIGN(VKBackBasedDeleter);
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_DELETER_H_
