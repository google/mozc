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

#include "win32/base/deleter.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "protocol/commands.pb.h"
#include "testing/gunit.h"
#include "win32/base/input_state.h"
#include "win32/base/keyboard.h"

namespace mozc {
namespace win32 {

namespace {

constexpr BYTE kPressed = 0x80;
constexpr uint64_t kOutputId = 0x12345678;

class KeyboardMock : public Win32KeyboardInterface {
 public:
  KeyboardMock() = default;

  bool IsKanaLocked(const KeyboardStatus &keyboard_state) override {
    return keyboard_state.IsPressed(VK_KANA);
  }

  bool SetKeyboardState(const KeyboardStatus &keyboard_state) override {
    key_state_ = keyboard_state;
    return true;
  }

  bool GetKeyboardState(KeyboardStatus *keyboard_state) override {
    *keyboard_state = key_state_;
    return true;
  }

  bool AsyncIsKeyPressed(int virtual_key) override {
    return async_key_state_.IsPressed(virtual_key);
  }

  int ToUnicode(UINT virtual_key, UINT scan_code, const BYTE *key_state,
                LPWSTR unicode_buffer, int unicode_buffer_num_elements,
                UINT flags) override {
    // We use a mock class in case the Japanese keyboard layout is not
    // available on this system.  This emulator class should work well in most
    // cases.  It returns an unicode character (if any) as if Japanese keyboard
    // layout was currently active.
    return JapaneseKeyboardLayoutEmulator::ToUnicode(
        virtual_key, scan_code, key_state, unicode_buffer,
        unicode_buffer_num_elements, flags);
  }

  UINT SendInput(std::vector<INPUT> inputs) override {
    last_send_input_data_ = std::move(inputs);
    return last_send_input_data_.size();
  }

  const KeyboardStatus &key_state() const { return key_state_; }

  void set_key_state(const KeyboardStatus &key_state) {
    key_state_ = key_state;
  }

  const KeyboardStatus &async_key_state() const { return async_key_state_; }

  void set_async_key_state(const KeyboardStatus &async_key_state) {
    async_key_state_ = async_key_state;
  }

  const std::vector<INPUT> &last_send_input_data() const {
    return last_send_input_data_;
  }

  void clear_last_send_input_data() { last_send_input_data_.clear(); }

 private:
  KeyboardStatus key_state_;
  KeyboardStatus async_key_state_;
  std::vector<INPUT> last_send_input_data_;
};

}  // namespace

TEST(VKBackBasedDeleterTest, OnKeyEventTestWhenNoDeletionIsOngoing) {
  KeyboardMock *keyboard_mock = new KeyboardMock();
  VKBackBasedDeleter deleter(keyboard_mock);
  commands::Output output;
  output.set_id(kOutputId);

  EXPECT_FALSE(deleter.IsDeletionOngoing());
  EXPECT_EQ(keyboard_mock->last_send_input_data().size(), 0);

  // OnKeyEvent never crashes even when there is no ongoing session.
  EXPECT_EQ(ClientAction::DO_DEFAULT_ACTION,
            deleter.OnKeyEvent(VK_BACK, true, true));
}

TEST(VKBackBasedDeleterTest, BeginDeletionTest_DeletionCountZero) {
  KeyboardMock *keyboard_mock = new KeyboardMock();
  VKBackBasedDeleter deleter(keyboard_mock);
  commands::Output output;
  output.set_id(kOutputId);

  InputState ime_state;

  // If the deletion count is zero, no deletion operation is started.
  deleter.BeginDeletion(0, output, ime_state);
  EXPECT_FALSE(deleter.IsDeletionOngoing());
  EXPECT_EQ(keyboard_mock->last_send_input_data().size(), 0);
}

TEST(VKBackBasedDeleterTest, NormalSequence) {
  constexpr UINT kLastKey = 'A';

  KeyboardMock *keyboard_mock = new KeyboardMock();

  // VKBackBasedDeleter must clear any modifier before calling SendInput.
  // To check this functionality, set VK_CONTROL bit into the mock.
  // See b/3419452 for detailed information.
  {
    KeyboardStatus keyboard_state;
    keyboard_state.SetState(VK_CONTROL, kPressed);
    keyboard_mock->set_key_state(keyboard_state);
  }
  {
    KeyboardStatus async_keyboard_state;
    async_keyboard_state.SetState(VK_CONTROL, kPressed);
    keyboard_mock->set_async_key_state(async_keyboard_state);
  }

  VKBackBasedDeleter deleter(keyboard_mock);
  commands::Output output;
  output.set_id(kOutputId);

  InputState ime_state;
  ime_state.last_down_key = VirtualKey::FromVirtualKey(kLastKey);

  // Delete proceeding 3 characters.
  deleter.BeginDeletion(3, output, ime_state);
  EXPECT_FALSE(keyboard_mock->key_state().IsPressed(VK_CONTROL));
  EXPECT_TRUE(keyboard_mock->async_key_state().IsPressed(VK_CONTROL));
  EXPECT_TRUE(deleter.IsDeletionOngoing());

  // Expect three pairs of VK_BACK [down/up] for deleting proceeding characters
  // and one pair of VK_BACK [down/up] as a sentinel key event where pending
  // output and ime state wiil be applied.
  const std::vector<INPUT> inputs = keyboard_mock->last_send_input_data();
  EXPECT_EQ(inputs.size(), 8);
  EXPECT_EQ(inputs[0].ki.wVk, VK_BACK);
  EXPECT_EQ(inputs[1].ki.wVk, VK_BACK);
  EXPECT_EQ(inputs[2].ki.wVk, VK_BACK);
  EXPECT_EQ(inputs[3].ki.wVk, VK_BACK);
  EXPECT_EQ(inputs[4].ki.wVk, VK_BACK);
  EXPECT_EQ(inputs[5].ki.wVk, VK_BACK);
  EXPECT_EQ(inputs[6].ki.wVk, VK_BACK);
  EXPECT_EQ(inputs[7].ki.wVk, VK_BACK);
  EXPECT_EQ(inputs[0].ki.dwFlags, 0);
  EXPECT_EQ(inputs[1].ki.dwFlags, KEYEVENTF_KEYUP);
  EXPECT_EQ(inputs[2].ki.dwFlags, 0);
  EXPECT_EQ(inputs[3].ki.dwFlags, KEYEVENTF_KEYUP);
  EXPECT_EQ(inputs[4].ki.dwFlags, 0);
  EXPECT_EQ(inputs[5].ki.dwFlags, KEYEVENTF_KEYUP);
  EXPECT_EQ(inputs[6].ki.dwFlags, 0);
  EXPECT_EQ(inputs[7].ki.dwFlags, KEYEVENTF_KEYUP);

  // Initially, the deleter is waiting for the first VK_BACK test-key-down.
  EXPECT_EQ(deleter.OnKeyEvent(VK_TAB, true, true),
            ClientAction::DO_DEFAULT_ACTION);
  EXPECT_EQ(deleter.OnKeyEvent(VK_TAB, false, true),
            ClientAction::DO_DEFAULT_ACTION);
  EXPECT_EQ(deleter.OnKeyEvent('X', true, true),
            ClientAction::DO_DEFAULT_ACTION);
  EXPECT_EQ(deleter.OnKeyEvent('X', false, true),
            ClientAction::DO_DEFAULT_ACTION);

  // The first pair of test-key-down/up.
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, true, true),
            ClientAction::SEND_KEY_TO_APPLICATION);
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, false, true),
            ClientAction::SEND_KEY_TO_APPLICATION);

  // The second pair of test-key-down/up.
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, true, true),
            ClientAction::SEND_KEY_TO_APPLICATION);
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, false, true),
            ClientAction::SEND_KEY_TO_APPLICATION);

  // The third pair of test-key-down/up.
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, true, true),
            ClientAction::SEND_KEY_TO_APPLICATION);
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, false, true),
            ClientAction::SEND_KEY_TO_APPLICATION);

  // The last key-down will not be sent to the application.
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, true, true),
            ClientAction::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, true, false),
            ClientAction::APPLY_PENDING_STATUS);

  // Check the pending output and state.
  EXPECT_EQ(deleter.pending_output().id(), kOutputId);
  EXPECT_EQ(deleter.pending_ime_state().last_down_key.virtual_key(), kLastKey);

  // The last key-up will not be sent to the application.
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, false, true),
            ClientAction::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, false, false),
            ClientAction::CALL_END_DELETION_BUT_NEVER_SEND_TO_SERVER);

  // Make sure the status of modifier keys have not changed.
  EXPECT_FALSE(keyboard_mock->key_state().IsPressed(VK_CONTROL));
  EXPECT_TRUE(keyboard_mock->async_key_state().IsPressed(VK_CONTROL));

  // The caller must call EndDeletion when
  // CALL_END_DELETION_BUT_NEVER_SEND_TO_SERVER is returned.
  deleter.EndDeletion();

  // After EndDeletion, the modifier state should be restored based on the
  // async key state.
  EXPECT_TRUE(keyboard_mock->key_state().IsPressed(VK_CONTROL));
  EXPECT_TRUE(keyboard_mock->async_key_state().IsPressed(VK_CONTROL));

  // Internal status must be reset by VKBackBasedDeleter::Reset().
  EXPECT_FALSE(deleter.IsDeletionOngoing());
}

TEST(VKBackBasedDeleterTest, BeginDeletion_InsuccessfulCase) {
  constexpr UINT kLastKey = 'A';

  KeyboardMock *keyboard_mock = new KeyboardMock();

  // VKBackBasedDeleter must clear any modifier before calling SendInput.
  // To check this functionality, set VK_CONTROL bit into the mock.
  // See b/3419452 for detailed information.
  {
    KeyboardStatus keyboard_state;
    keyboard_state.SetState(VK_CONTROL, kPressed);
    keyboard_mock->set_key_state(keyboard_state);
  }
  {
    KeyboardStatus async_keyboard_state;
    async_keyboard_state.SetState(VK_CONTROL, kPressed);
    keyboard_mock->set_async_key_state(async_keyboard_state);
  }

  VKBackBasedDeleter deleter(keyboard_mock);

  commands::Output output;
  output.set_id(kOutputId);

  InputState ime_state;
  ime_state.last_down_key = VirtualKey::FromVirtualKey(kLastKey);

  // Delete proceeding 3 characters.
  deleter.BeginDeletion(3, output, ime_state);
  EXPECT_FALSE(keyboard_mock->key_state().IsPressed(VK_CONTROL));
  EXPECT_TRUE(keyboard_mock->async_key_state().IsPressed(VK_CONTROL));

  EXPECT_TRUE(deleter.IsDeletionOngoing());

  // The first pair of test-key-down/up.
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, true, true),
            ClientAction::SEND_KEY_TO_APPLICATION);
  EXPECT_EQ(deleter.OnKeyEvent(VK_BACK, false, true),
            ClientAction::SEND_KEY_TO_APPLICATION);

  // If an unexpected key is passed, the deletion sequence must be terminated.
  EXPECT_EQ(deleter.OnKeyEvent(VK_TAB, true, true),
            ClientAction::CALL_END_DELETION_THEN_DO_DEFAULT_ACTION);

  // Make sure the status of modifier keys have not changed.
  EXPECT_FALSE(keyboard_mock->key_state().IsPressed(VK_CONTROL));
  EXPECT_TRUE(keyboard_mock->async_key_state().IsPressed(VK_CONTROL));

  // The caller must call EndDeletion when
  // CALL_END_DELETION_THEN_DO_DEFAULT_ACTION is returned.
  deleter.EndDeletion();

  // After EndDeletion, the modifier state should be restored based on the
  // async key state.
  EXPECT_TRUE(keyboard_mock->key_state().IsPressed(VK_CONTROL));
  EXPECT_TRUE(keyboard_mock->async_key_state().IsPressed(VK_CONTROL));

  // Internal status must be reset by VKBackBasedDeleter::Reset().
  EXPECT_FALSE(deleter.IsDeletionOngoing());
}

}  // namespace win32
}  // namespace mozc
