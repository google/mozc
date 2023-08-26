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

#include "win32/base/surrogate_pair_observer.h"

#include "testing/googletest.h"
#include "testing/gunit.h"
#include "win32/base/keyboard.h"

namespace mozc {
namespace win32 {

TEST(ImeSurrogatePairObserverTest, UCS2Test) {
  const wchar_t kHiraganaA = L'\u3042';
  const VirtualKey vk_a = VirtualKey::FromCombinedVirtualKey(
      (static_cast<DWORD>(kHiraganaA) << 16) | VK_PACKET);

  SurrogatePairObserver observer;

  // test key down
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_a, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4);
    EXPECT_EQ(action.ucs4, kHiraganaA);
  }

  // key down
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_a, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4);
    EXPECT_EQ(action.ucs4, kHiraganaA);
  }
}

TEST(ImeSurrogatePairObserverTest, BasicSurrogatePairTest) {
  // "𠮟"
  const wchar_t kHighSurrogate = static_cast<wchar_t>(0xD842);
  const wchar_t kLowSurrogate = static_cast<wchar_t>(0xDF9F);
  const char32_t kUcs4 = 0x20B9F;

  const VirtualKey vk_high = VirtualKey::FromCombinedVirtualKey(
      (static_cast<DWORD>(kHighSurrogate) << 16) | VK_PACKET);
  const VirtualKey vk_low = VirtualKey::FromCombinedVirtualKey(
      (static_cast<DWORD>(kLowSurrogate) << 16) | VK_PACKET);

  SurrogatePairObserver observer;

  // test key down (high surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_high, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // key down (high surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_high, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // test key down (low surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_low, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4);
    EXPECT_EQ(action.ucs4, kUcs4);
  }

  // key down (low surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_low, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4);
    EXPECT_EQ(action.ucs4, kUcs4);
  }
}

TEST(ImeSurrogatePairObserverTest, BasicSurrogatePairTestWithKeyUp) {
  // "𠮟"
  const wchar_t kHighSurrogate = static_cast<wchar_t>(0xD842);
  const wchar_t kLowSurrogate = static_cast<wchar_t>(0xDF9F);
  const char32_t kUcs4 = 0x20B9F;

  const VirtualKey vk_high = VirtualKey::FromCombinedVirtualKey(
      (static_cast<DWORD>(kHighSurrogate) << 16) | VK_PACKET);
  const VirtualKey vk_low = VirtualKey::FromCombinedVirtualKey(
      (static_cast<DWORD>(kLowSurrogate) << 16) | VK_PACKET);

  SurrogatePairObserver observer;

  // test key down (high surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_high, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // key down (high surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_high, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // test key up (high surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_high, false);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // key up (high surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_high, false);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // test key down (low surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_low, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4);
    EXPECT_EQ(action.ucs4, kUcs4);
  }

  // key down (low surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_low, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4);
    EXPECT_EQ(action.ucs4, kUcs4);
  }

  // test key up (low surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_low, false);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4);
    EXPECT_EQ(action.ucs4, kUcs4);
  }

  // key up (low surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_low, false);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4);
    EXPECT_EQ(action.ucs4, kUcs4);
  }
}

TEST(ImeSurrogatePairObserverTest, IrregularOrderSurrogatePairTest) {
  // "𠮟"
  const wchar_t kHighSurrogate = static_cast<wchar_t>(0xD842);
  const wchar_t kLowSurrogate = static_cast<wchar_t>(0xDF9F);
  const char32_t kUcs4 = 0x20B9F;

  const VirtualKey vk_high = VirtualKey::FromCombinedVirtualKey(
      (static_cast<DWORD>(kHighSurrogate) << 16) | VK_PACKET);
  const VirtualKey vk_low = VirtualKey::FromCombinedVirtualKey(
      (static_cast<DWORD>(kLowSurrogate) << 16) | VK_PACKET);

  const wchar_t kHiraganaA = L'\u3042';
  const VirtualKey vk_a = VirtualKey::FromCombinedVirtualKey(
      (static_cast<DWORD>(kHiraganaA) << 16) | VK_PACKET);

  SurrogatePairObserver observer;

  // test key down (high surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_high, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // key down (high surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_high, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // test key up (high surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_high, false);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // key up (high surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_high, false);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // test key down "あ"
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_a, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4);
    EXPECT_EQ(action.ucs4, kHiraganaA);
  }

  // key down "あ"
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_a, true);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::DO_DEFAULT_ACTION_WITH_RETURNED_UCS4);
    EXPECT_EQ(action.ucs4, kHiraganaA);
  }

  // test key up (low surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnTestKeyEvent(vk_low, false);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }

  // key up (low surrogate)
  {
    SurrogatePairObserver::ClientAction action =
        observer.OnKeyEvent(vk_low, false);
    EXPECT_EQ(action.type,
              SurrogatePairObserver::CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER);
    EXPECT_EQ(action.ucs4, 0);
  }
}

}  // namespace win32
}  // namespace mozc
