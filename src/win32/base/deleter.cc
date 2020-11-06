// Copyright 2010-2020, Google Inc.
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

#include <deque>
#include <vector>

#include "base/logging.h"
#include "protocol/commands.pb.h"
#include "win32/base/input_state.h"
#include "win32/base/keyboard.h"

namespace mozc {
namespace win32 {

class VKBackBasedDeleterQueue
    : public std::deque<std::pair<VKBackBasedDeleter::DeletionWaitState,
                                  VKBackBasedDeleter::ClientAction>> {};

VKBackBasedDeleter::VKBackBasedDeleter()
    : wait_queue_(new VKBackBasedDeleterQueue),
      keyboard_(Win32KeyboardInterface::CreateDefault()),
      pending_ime_state_(new InputState()),
      pending_output_(new mozc::commands::Output()) {}

VKBackBasedDeleter::VKBackBasedDeleter(Win32KeyboardInterface *keyboard_mock)
    : wait_queue_(new VKBackBasedDeleterQueue),
      keyboard_(keyboard_mock),
      pending_ime_state_(new InputState()),
      pending_output_(new mozc::commands::Output()) {}

VKBackBasedDeleter::~VKBackBasedDeleter() {}

void VKBackBasedDeleter::BeginDeletion(int deletion_count,
                                       const mozc::commands::Output &output,
                                       const InputState &ime_state) {
  std::vector<INPUT> inputs;

  wait_queue_->clear();
  *pending_ime_state_ = InputState();
  pending_output_->Clear();

  if (deletion_count == 0) {
    return;
  }

  *pending_ime_state_ = ime_state;
  pending_output_->CopyFrom(output);

  wait_queue_->push_back(
      std::make_pair(WAIT_INITIAL_VK_BACK_TESTDOWN, SEND_KEY_TO_APPLICATION));
  wait_queue_->push_back(
      std::make_pair(WAIT_VK_BACK_TESTUP, SEND_KEY_TO_APPLICATION));

  for (int i = 1; i < deletion_count; ++i) {
    wait_queue_->push_back(
        std::make_pair(WAIT_VK_BACK_TESTDOWN, SEND_KEY_TO_APPLICATION));
    wait_queue_->push_back(
        std::make_pair(WAIT_VK_BACK_TESTUP, SEND_KEY_TO_APPLICATION));
  }

  wait_queue_->push_back(std::make_pair(WAIT_VK_BACK_TESTDOWN,
                                        CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER));
  wait_queue_->push_back(
      std::make_pair(WAIT_VK_BACK_DOWN, APPLY_PENDING_STATUS));
  wait_queue_->push_back(std::make_pair(WAIT_VK_BACK_TESTUP,
                                        CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER));
  wait_queue_->push_back(std::make_pair(
      WAIT_VK_BACK_UP, CALL_END_DELETION_BUT_NEVER_SEND_TO_SERVER));

  const KEYBDINPUT keyboard_input = {VK_BACK, 0, 0, 0, 0};
  INPUT keydown = {};
  keydown.type = INPUT_KEYBOARD;
  keydown.ki = keyboard_input;

  INPUT keyup = keydown;
  keyup.type = INPUT_KEYBOARD;
  keyup.ki.dwFlags = KEYEVENTF_KEYUP;

  for (size_t i = 0; i < deletion_count; ++i) {
    inputs.push_back(keydown);
    inputs.push_back(keyup);
  }

  inputs.push_back(keydown);  // Sentinel Keydown
  inputs.push_back(keyup);    // Sentinel Keyup

  UnsetModifiers();
  keyboard_->SendInput(inputs);
}

VKBackBasedDeleter::ClientAction VKBackBasedDeleter::OnKeyEvent(
    UINT vk, bool is_keydown, bool is_test_key) {
  // Default action when no auto-deletion is ongoing.
  if (!IsDeletionOngoing()) {
    return DO_DEFAULT_ACTION;
  }

  // Hereafter, auto-deletion is ongoing.
  const std::pair<DeletionWaitState, ClientAction> next = wait_queue_->front();
  if (next.first == WAIT_INITIAL_VK_BACK_TESTDOWN) {
    if ((vk == VK_BACK) && is_keydown && is_test_key) {
      wait_queue_->pop_front();
      return next.second;
    } else {
      // Do not pop front.
      return DO_DEFAULT_ACTION;
    }
  }

  wait_queue_->pop_front();

  bool matched = false;
  switch (next.first) {
    case WAIT_VK_BACK_TESTDOWN:
      matched = ((vk == VK_BACK) && is_keydown && is_test_key);
      break;
    case WAIT_VK_BACK_TESTUP:
      matched = ((vk == VK_BACK) && !is_keydown && is_test_key);
      break;
    case WAIT_VK_BACK_DOWN:
      matched = ((vk == VK_BACK) && is_keydown && !is_test_key);
      break;
    case WAIT_VK_BACK_UP:
      matched = ((vk == VK_BACK) && !is_keydown && !is_test_key);
      break;
    default:
      DLOG(FATAL) << "unexpected state found";
      break;
  }
  if (matched) {
    return next.second;
  }

  return CALL_END_DELETION_THEN_DO_DEFAULT_ACTION;
}

void VKBackBasedDeleter::UnsetModifiers() {
  // Ensure that Shift, Control, and Alt key do not affect generated key
  // events.  See b/3419452 for details.
  // TODO(yukawa): Use 3rd argument of ImeToAsciiEx to obtain the keyboard
  //               state instead of GetKeyboardState API.
  // TODO(yukawa): Update VKBackBasedDeleter to consider this case.
  KeyboardStatus keyboard_state;
  if (!keyboard_->GetKeyboardState(&keyboard_state)) {
    return;
  }

  // If any side-effect is found, we might want to clear only the highest
  // bit.
  const BYTE kUnsetState = 0;
  bool to_be_updated = false;
  if (keyboard_state.IsPressed(VK_SHIFT)) {
    to_be_updated = true;
    keyboard_state.SetState(VK_SHIFT, kUnsetState);
  }
  if (keyboard_state.IsPressed(VK_CONTROL)) {
    to_be_updated = true;
    keyboard_state.SetState(VK_CONTROL, kUnsetState);
  }
  if (keyboard_state.IsPressed(VK_MENU)) {
    to_be_updated = true;
    keyboard_state.SetState(VK_MENU, kUnsetState);
  }
  if (to_be_updated) {
    keyboard_->SetKeyboardState(keyboard_state);
  }
}

void VKBackBasedDeleter::EndDeletion() {
  bool to_be_updated = false;
  KeyboardStatus keyboard_state;
  if (!keyboard_->GetKeyboardState(&keyboard_state)) {
    return;
  }

  const BYTE kPressed = 0x80;
  if (keyboard_->AsyncIsKeyPressed(VK_SHIFT)) {
    to_be_updated = true;
    keyboard_state.SetState(VK_SHIFT, kPressed);
  }
  if (keyboard_->AsyncIsKeyPressed(VK_CONTROL)) {
    to_be_updated = true;
    keyboard_state.SetState(VK_CONTROL, kPressed);
  }
  if (keyboard_->AsyncIsKeyPressed(VK_MENU)) {
    to_be_updated = true;
    keyboard_state.SetState(VK_MENU, kPressed);
  }
  if (to_be_updated) {
    keyboard_->SetKeyboardState(keyboard_state);
  }
  wait_queue_->clear();
}

bool VKBackBasedDeleter::IsDeletionOngoing() const {
  return !wait_queue_->empty();
}

const mozc::commands::Output &VKBackBasedDeleter::pending_output() const {
  return *pending_output_;
}

const InputState &VKBackBasedDeleter::pending_ime_state() const {
  return *pending_ime_state_;
}

}  // namespace win32
}  // namespace mozc
