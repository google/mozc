// Copyright 2010-2012, Google Inc.
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

#include "win32/ime/ime_surrogate_pair_observer.h"

#include "win32/ime/ime_keyboard.h"

namespace mozc {
namespace win32 {
namespace {

// TODO(yukawa): Move this to util.cc.
char32 SurrogatePairToUCS4(wchar_t high, wchar_t low) {
  return (((high - 0xD800) & 0x3FF) << 10) +
         ((low - 0xDC00) & 0x3FF) + 0x10000;
}

}  // namespace

SurrogatePairObserver::SurrogatePairObserver()
    : state_(INITIAL_STATE),
      surrogate_high_(L'\0'),
      surrogate_low_(L'\0') {}

SurrogatePairObserver::ClientAction SurrogatePairObserver::OnTestKeyEvent(
    const VirtualKey &virtual_key, bool is_keydown) {
  return OnKeyEvent(virtual_key, is_keydown, true);
}

SurrogatePairObserver::ClientAction SurrogatePairObserver::OnKeyEvent(
    const VirtualKey &virtual_key, bool is_keydown) {
  return OnKeyEvent(virtual_key, is_keydown, false);
}

SurrogatePairObserver::ClientAction SurrogatePairObserver::OnKeyEvent(
    const VirtualKey &virtual_key, bool is_keydown, bool is_test_key) {
  switch (state_) {
    case INITIAL_STATE: {
      if ((virtual_key.virtual_key() != VK_PACKET)) {
        return ClientAction(DO_DEFAULT_ACTION, 0);
      }
      const wchar_t ucs2 = virtual_key.wide_char();
      if (IS_HIGH_SURROGATE(ucs2)) {
        if (is_keydown) {
          if (!is_test_key) {
            surrogate_high_ = ucs2;
            state_ = WAIT_FOR_SURROGATE_HIGH_UP;
          }
          return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
        }
        // Ignore orhpaned key-up of high surrogate.
        return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
      }
      if (IS_LOW_SURROGATE(ucs2)) {
        // Ignore orhpaned key-up/down of low surrogate.
        return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
      }
      return ClientAction(DO_DEFAULT_ACTION_WITH_RETURNED_UCS4, ucs2);
    }
    case WAIT_FOR_SURROGATE_HIGH_UP: {
      if ((virtual_key.virtual_key() != VK_PACKET)) {
        return ClientAction(DO_DEFAULT_ACTION, 0);
      }
      const wchar_t ucs2 = virtual_key.wide_char();
      if (IS_HIGH_SURROGATE(ucs2)) {
        if (is_keydown) {
          if (!is_test_key) {
            surrogate_high_ = ucs2;
            state_ = WAIT_FOR_SURROGATE_HIGH_UP;
          }
          return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
        }
        if (ucs2 != surrogate_high_) {
          // Ignore orhpaned key-up of high surrogate.
          return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
        }
        if (!is_test_key) {
          state_ = WAIT_FOR_SURROGATE_LOW_DOWM;
        }
        return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
      }
      if (IS_LOW_SURROGATE(ucs2)) {
        if (is_keydown) {
          const char32 ucs4 = SurrogatePairToUCS4(surrogate_high_,
                                                  ucs2);
          if (!is_test_key) {
            surrogate_low_ = ucs2;
            state_ = WAIT_FOR_SURROGATE_LOW_UP;
          }
          return ClientAction(DO_DEFAULT_ACTION_WITH_RETURNED_UCS4, ucs4);
        }
        // Ignore orhpaned key-up of low surrogate.
        return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
      }
      return ClientAction(DO_DEFAULT_ACTION_WITH_RETURNED_UCS4, ucs2);
    }
    case WAIT_FOR_SURROGATE_LOW_DOWM: {
      if ((virtual_key.virtual_key() != VK_PACKET)) {
        return ClientAction(DO_DEFAULT_ACTION, 0);
      }
      const wchar_t ucs2 = virtual_key.wide_char();
      if (IS_HIGH_SURROGATE(ucs2)) {
        if (is_keydown) {
          if (!is_test_key) {
            surrogate_high_ = ucs2;
            state_ = WAIT_FOR_SURROGATE_HIGH_UP;
          }
          return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
        }
        // Ignore orhpaned key-up of high surrogate.
        return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
      }
      if (IS_LOW_SURROGATE(ucs2)) {
        if (is_keydown) {
          const char32 ucs4 = SurrogatePairToUCS4(surrogate_high_,
                                                  ucs2);
          if (!is_test_key) {
            surrogate_low_ = ucs2;
            state_ = WAIT_FOR_SURROGATE_LOW_UP;
          }
          return ClientAction(DO_DEFAULT_ACTION_WITH_RETURNED_UCS4, ucs4);
        }
        // Ignore orhpaned key-up of low surrogate.
        return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
      }
      if (!is_test_key) {
        state_ = INITIAL_STATE;
        surrogate_high_ = L'\0';
        surrogate_low_ = L'\0';
      }
      return ClientAction(DO_DEFAULT_ACTION_WITH_RETURNED_UCS4, ucs2);
    }
    case WAIT_FOR_SURROGATE_LOW_UP: {
      if ((virtual_key.virtual_key() != VK_PACKET)) {
        return ClientAction(DO_DEFAULT_ACTION, 0);
      }
      const wchar_t ucs2 = virtual_key.wide_char();
      if (IS_HIGH_SURROGATE(ucs2)) {
        if (is_keydown) {
          if (!is_test_key) {
            surrogate_high_ = ucs2;
            state_ = WAIT_FOR_SURROGATE_HIGH_UP;
          }
          return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
        }
        // Ignore orhpaned key-up of high surrogate.
        return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
      }
      if (IS_LOW_SURROGATE(ucs2)) {
        if (is_keydown) {
          // Ignore orhpaned key-down of low surrogate.
          return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
        }
        if (surrogate_low_ != ucs2) {
          // Ignore orhpaned key-up of low surrogate.
          return ClientAction(CONSUME_KEY_BUT_NEVER_SEND_TO_SERVER, 0);
        }
        const char32 ucs4 = SurrogatePairToUCS4(surrogate_high_,
                                                surrogate_low_);
        if (!is_test_key) {
          state_ = INITIAL_STATE;
          surrogate_high_ = L'\0';
          surrogate_low_ = L'\0';
        }
        return ClientAction(DO_DEFAULT_ACTION_WITH_RETURNED_UCS4, ucs4);
      }
      if (!is_test_key) {
        state_ = INITIAL_STATE;
        surrogate_high_ = L'\0';
        surrogate_low_ = L'\0';
      }
      return ClientAction(DO_DEFAULT_ACTION_WITH_RETURNED_UCS4, ucs2);
    }
    default:
      DLOG(FATAL) << "must not reach here.";
      return ClientAction(DO_DEFAULT_ACTION, 0);
  }
}

}  // namespace win32
}  // namespace mozc
