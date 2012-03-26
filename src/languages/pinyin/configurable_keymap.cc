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

#include "languages/pinyin/configurable_keymap.h"

#include <cctype>

#include "base/base.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "session/key_event_util.h"

namespace mozc {
using commands::KeyEvent;

namespace pinyin {
namespace keymap {

namespace {
const char kEnglishModeSpecialKey = 'v';
}  // namespace

bool ConfigurableKeymap::GetCommand(const KeyEvent &key_event,
                                    ConverterState state,
                                    KeyCommand *key_command) {
  DCHECK(key_command);
  DCHECK(!KeyEventUtil::IsNumpadKey(key_event));

  const config::PinyinConfig &pinyin_config = GET_CONFIG(pinyin_config);
  const uint32 modifiers = KeyEventUtil::GetModifiers(key_event);
  DCHECK(!KeyEventUtil::HasCaps(modifiers));

  // TODO(hsumita): Refactoring these codes. (There are many nested block)

  if (state == INACTIVE) {
    if (!pinyin_config.double_pinyin()) {
      if (key_event.has_key_code() && modifiers == 0 &&
          key_event.key_code() == kEnglishModeSpecialKey) {
        *key_command = TURN_ON_ENGLISH_MODE;
        return true;
      }
    }
  } else {  // state == ACTIVE
    if (pinyin_config.select_with_shift()) {
      if (!key_event.has_key_code() && !key_event.has_special_key() &&
          KeyEventUtil::IsShift(modifiers)) {
        if (modifiers & KeyEvent::LEFT_SHIFT) {
          *key_command = SELECT_SECOND_CANDIDATE;
          return true;
        }
        if (modifiers & KeyEvent::RIGHT_SHIFT) {
          *key_command = SELECT_THIRD_CANDIDATE;
          return true;
        }
      }
    }

    if (pinyin_config.paging_with_minus_equal()) {
      if (key_event.has_key_code() && modifiers == 0) {
        const char key_code = key_event.key_code();
        if (key_code == '-') {
          *key_command = FOCUS_CANDIDATE_PREV_PAGE;
          return true;
        }
        if (key_code == '=') {
          *key_command = FOCUS_CANDIDATE_NEXT_PAGE;
          return true;
        }
      }
    }

    if (pinyin_config.paging_with_comma_period()) {
      if (key_event.has_key_code() && modifiers == 0) {
        const char key_code = key_event.key_code();
        if (key_code == ',') {
          *key_command = FOCUS_CANDIDATE_PREV_PAGE;
          return true;
        }
        if (key_code == '.') {
          *key_command = FOCUS_CANDIDATE_NEXT_PAGE;
          return true;
        }
      }
    }

    if (pinyin_config.auto_commit()) {
      if (key_event.has_key_code() && ispunct(key_event.key_code()) &&
          modifiers == 0) {
        *key_command = AUTO_COMMIT;
        return true;
      }
    }
  }

  return false;
}

}  // namespace keymap
}  // namespace pinyin
}  // namespace mozc
