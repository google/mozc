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

#include "win32/base/keyboard.h"

#include <bit>
#include <cstddef>

#include "testing/gunit.h"

namespace mozc {
namespace win32 {
namespace {
constexpr BYTE kPressed = 0x80;
constexpr BYTE kToggled = 0x01;
}  // namespace

class ImeKeyboardTest : public testing::Test {
 protected:
  ImeKeyboardTest() : japanese_keyboard_layout_(nullptr) {}

  virtual void SetUp() {
    const HKL hkl = ::LoadKeyboardLayoutW(L"00000411", KLF_ACTIVATE);

    // In 32-bit Windows, |hkl| is like 04110411.
    // In 64-bit Windows, |hkl| is like 0000000004110411.
    const ptrdiff_t dword_hkl = std::bit_cast<ptrdiff_t>(hkl);
    constexpr ptrdiff_t kExpectedHKL = 0x04110411;
    if (dword_hkl != kExpectedHKL) {
      // seems to fall back to non-Japanese keyboard layout.
      return;
    }

    japanese_keyboard_layout_ = hkl;
  }

  virtual void TearDown() {
    if (japanese_keyboard_layout_ != nullptr) {
      ::UnloadKeyboardLayout(japanese_keyboard_layout_);
    }
    japanese_keyboard_layout_ = nullptr;
  }

  bool japanese_keyboard_layout_available() const {
    const bool available = (japanese_keyboard_layout_ != nullptr);
    return available;
  }

  HKL japanese_keyboard_layout() const { return japanese_keyboard_layout_; }

 private:
  HKL japanese_keyboard_layout_;
};

TEST_F(ImeKeyboardTest, CheckQKeyWithKanaLock) {
  if (!japanese_keyboard_layout_available()) {
    // We cannot continue this test because Japanese keyboard layout is not
    // available on this system.
    return;
  }

  BYTE keyboard_state[256] = {};
  keyboard_state[VK_KANA] = kPressed;

  wchar_t expected_buffer[16] = {};
  const int expected_length =
      ::ToUnicodeEx('Q', 0, keyboard_state, expected_buffer,
                    std::size(expected_buffer), 0, japanese_keyboard_layout());

  wchar_t actual_buffer[16] = {};
  const int actual_length = JapaneseKeyboardLayoutEmulator::ToUnicode(
      'Q', 0, keyboard_state, actual_buffer, std::size(actual_buffer), 0);

  EXPECT_EQ(actual_length, expected_length);
  EXPECT_EQ(actual_length, 1);

  EXPECT_EQ(actual_buffer[0], expected_buffer[0]);
  EXPECT_EQ(actual_buffer[0], 0xff80);
}

TEST_F(ImeKeyboardTest, CheckQKeyWithoutCapsLock) {
  if (!japanese_keyboard_layout_available()) {
    // We cannot continue this test because Japanese keyboard layout is not
    // available on this system.
    return;
  }

  BYTE keyboard_state[256] = {};

  wchar_t expected_buffer[16] = {};
  const int expected_length =
      ::ToUnicodeEx('Q', 0, keyboard_state, expected_buffer,
                    std::size(expected_buffer), 0, japanese_keyboard_layout());

  wchar_t actual_buffer[16] = {};
  const int actual_length = JapaneseKeyboardLayoutEmulator::ToUnicode(
      'Q', 0, keyboard_state, actual_buffer, std::size(actual_buffer), 0);

  EXPECT_EQ(actual_length, expected_length);
  EXPECT_EQ(actual_length, 1);

  EXPECT_EQ(actual_buffer[0], expected_buffer[0]);
  EXPECT_EQ(actual_buffer[0], 'q');
}

TEST_F(ImeKeyboardTest, CheckQKeyWithCapsLock) {
  if (!japanese_keyboard_layout_available()) {
    // We cannot continue this test because Japanese keyboard layout is not
    // available on this system.
    return;
  }

  BYTE keyboard_state[256] = {};
  keyboard_state[VK_CAPITAL] = kToggled;

  wchar_t expected_buffer[16] = {};
  const int expected_length =
      ::ToUnicodeEx('Q', 0, keyboard_state, expected_buffer,
                    std::size(expected_buffer), 0, japanese_keyboard_layout());

  wchar_t actual_buffer[16] = {};
  const int actual_length = JapaneseKeyboardLayoutEmulator::ToUnicode(
      'Q', 0, keyboard_state, actual_buffer, std::size(actual_buffer), 0);

  EXPECT_EQ(actual_length, expected_length);
  EXPECT_EQ(actual_length, 1);

  EXPECT_EQ(actual_buffer[0], expected_buffer[0]);
  EXPECT_EQ(actual_buffer[0], 'Q');
}

TEST_F(ImeKeyboardTest, CheckQKeyWithShiftCapsLock) {
  if (!japanese_keyboard_layout_available()) {
    // We cannot continue this test because Japanese keyboard layout is not
    // available on this system.
    return;
  }

  BYTE keyboard_state[256] = {};
  keyboard_state[VK_SHIFT] = kPressed;
  keyboard_state[VK_CAPITAL] = kToggled;

  wchar_t expected_buffer[16] = {};
  const int expected_length =
      ::ToUnicodeEx('Q', 0, keyboard_state, expected_buffer,
                    std::size(expected_buffer), 0, japanese_keyboard_layout());

  wchar_t actual_buffer[16] = {};
  const int actual_length = JapaneseKeyboardLayoutEmulator::ToUnicode(
      'Q', 0, keyboard_state, actual_buffer, std::size(actual_buffer), 0);

  EXPECT_EQ(actual_length, expected_length);
  EXPECT_EQ(actual_length, 1);

  EXPECT_EQ(actual_buffer[0], expected_buffer[0]);
  EXPECT_EQ(actual_buffer[0], 'q');
}

TEST_F(ImeKeyboardTest, CheckQKeyWithShiftCtrlCapsLock) {
  if (!japanese_keyboard_layout_available()) {
    // We cannot continue this test because Japanese keyboard layout is not
    // available on this system.
    return;
  }

  BYTE keyboard_state[256] = {};
  keyboard_state[VK_SHIFT] = kPressed;
  keyboard_state[VK_CONTROL] = kPressed;
  keyboard_state[VK_CAPITAL] = kToggled;

  wchar_t expected_buffer[16] = {};
  const int expected_length =
      ::ToUnicodeEx('Q', 0, keyboard_state, expected_buffer,
                    std::size(expected_buffer), 0, japanese_keyboard_layout());

  wchar_t actual_buffer[16] = {};
  const int actual_length = JapaneseKeyboardLayoutEmulator::ToUnicode(
      'Q', 0, keyboard_state, actual_buffer, std::size(actual_buffer), 0);

  EXPECT_EQ(actual_length, expected_length);
  EXPECT_EQ(actual_length, 1);

  EXPECT_EQ(actual_buffer[0], expected_buffer[0]);
}

}  // namespace win32
}  // namespace mozc
