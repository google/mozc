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

#include "languages/pinyin/default_keymap.h"

#include <cctype>

#include "base/base.h"
#include "base/logging.h"
#include "session/commands.pb.h"
#include "session/key_event_util.h"

namespace mozc {
namespace pinyin {
namespace keymap {

namespace {
const char kPunctuationModeSpecialKey = '`';
}  // namespace

// TODO(hsumita): Investigates the behavior of "@" key when converter is active.

bool DefaultKeymap::GetCommand(const commands::KeyEvent &key_event,
                               ConverterState state,
                               KeyCommand *key_command) {
  DCHECK(key_command);
  DCHECK(!KeyEventUtil::HasCaps(KeyEventUtil::GetModifiers(key_event)));
  DCHECK(!KeyEventUtil::IsNumpadKey(key_event));

  if (key_event.has_key_code()) {
    *key_command = ProcessKeyCode(key_event, state);
    return true;
  }
  if (key_event.has_special_key()) {
    *key_command = ProcessSpecialKey(key_event, state);
    return true;
  }
  if (key_event.modifier_keys_size() != 0) {
    *key_command = ProcessModifierKey(key_event, state);
    return true;
  }

  LOG(ERROR) << "There is no key_code, modifier_key or special_key";
  return false;
}

KeyCommand DefaultKeymap::ProcessKeyCode(const commands::KeyEvent &key_event,
                                         ConverterState state) {
  DCHECK(key_event.has_key_code());
  DCHECK(!key_event.has_special_key());

  const uint32 modifiers = KeyEventUtil::GetModifiers(key_event);
  const char key_code = key_event.key_code();

  if (KeyEventUtil::IsCtrlShift(modifiers) &&
      (key_code == 'f' || key_code == 'F')) {
    return TOGGLE_SIMPLIFIED_CHINESE_MODE;
  }

  if (state == INACTIVE) {
    if (ispunct(key_event.key_code()) && modifiers == 0) {
      if (key_event.key_code() == kPunctuationModeSpecialKey) {
        return TURN_ON_PUNCTUATION_MODE;
      } else {
        return INSERT_PUNCTUATION;
      }
    }
    if (KeyEventUtil::HasAlt(modifiers) || KeyEventUtil::HasCtrl(modifiers)) {
      return DO_NOTHING_WITHOUT_CONSUME;
    }
    return INSERT;
  }
  DCHECK_EQ(ACTIVE, state);

  if (KeyEventUtil::HasAlt(modifiers)) {
    return DO_NOTHING_WITH_CONSUME;
  }

  if (isalpha(key_code)) {
    if (KeyEventUtil::HasCtrl(modifiers)) {
      return DO_NOTHING_WITH_CONSUME;
    }
    if (KeyEventUtil::IsUpperAlphabet(key_event)) {
      return DO_NOTHING_WITH_CONSUME;
    }
    return INSERT;
  }

  if (isdigit(key_code)) {
    if (KeyEventUtil::HasShift(modifiers)) {
      return DO_NOTHING_WITH_CONSUME;
    }
    if (KeyEventUtil::IsCtrl(modifiers)) {
      return CLEAR_CANDIDATE_FROM_HISTORY;
    }
    return SELECT_CANDIDATE;
  }

  return DO_NOTHING_WITH_CONSUME;
}

KeyCommand DefaultKeymap::ProcessSpecialKey(
    const commands::KeyEvent &key_event, ConverterState state) {
  DCHECK(key_event.has_special_key());
  DCHECK(!key_event.has_key_code());

  const commands::KeyEvent::SpecialKey special_key = key_event.special_key();

  if (state == INACTIVE) {
    return DO_NOTHING_WITHOUT_CONSUME;
  }
  DCHECK_EQ(ACTIVE, state);

  // Shift key is always ignored with special key.

  const uint32 modifiers = KeyEventUtil::GetModifiers(key_event);
  const bool has_alt = KeyEventUtil::HasAlt(modifiers);
  const bool has_ctrl = KeyEventUtil::HasCtrl(modifiers);

  if (has_alt && has_ctrl) {
    switch (special_key) {
      case commands::KeyEvent::UP:
        return MOVE_CURSOR_TO_BEGINNING;
      case commands::KeyEvent::DOWN:
        return MOVE_CURSOR_TO_END;
      default:
        return DO_NOTHING_WITH_CONSUME;
    }
  } else if (has_alt) {
    switch (special_key) {
      case commands::KeyEvent::UP:
        return FOCUS_CANDIDATE_PREV_PAGE;
      case commands::KeyEvent::DOWN:
        return FOCUS_CANDIDATE_NEXT_PAGE;
      default:
        return DO_NOTHING_WITH_CONSUME;
    }
  } else if (has_ctrl) {
    switch (special_key) {
      case commands::KeyEvent::RIGHT:
        return MOVE_CURSOR_RIGHT_BY_WORD;
      case commands::KeyEvent::LEFT:
        return MOVE_CURSOR_LEFT_BY_WORD;
      case commands::KeyEvent::BACKSPACE:
        return REMOVE_WORD_BEFORE;
      case commands::KeyEvent::DEL:
        return REMOVE_WORD_AFTER;
      default:
        return DO_NOTHING_WITH_CONSUME;
    }
  } else {
    switch (special_key) {
      case commands::KeyEvent::ENTER:
        return COMMIT;
      case commands::KeyEvent::SPACE:
        return SELECT_FOCUSED_CANDIDATE;
      case commands::KeyEvent::UP:
        return FOCUS_CANDIDATE_PREV;
      case commands::KeyEvent::DOWN:
        return FOCUS_CANDIDATE_NEXT;
      case commands::KeyEvent::RIGHT:
        return MOVE_CURSOR_RIGHT;
      case commands::KeyEvent::LEFT:
        return MOVE_CURSOR_LEFT;
      case commands::KeyEvent::PAGE_UP:
        return FOCUS_CANDIDATE_PREV_PAGE;
      case commands::KeyEvent::PAGE_DOWN:
        return FOCUS_CANDIDATE_NEXT_PAGE;
      case commands::KeyEvent::HOME:
        return MOVE_CURSOR_TO_BEGINNING;
      case commands::KeyEvent::END:
        return MOVE_CURSOR_TO_END;
      case commands::KeyEvent::BACKSPACE:
        return REMOVE_CHAR_BEFORE;
      case commands::KeyEvent::DEL:
        return REMOVE_CHAR_AFTER;
      case commands::KeyEvent::ESCAPE:
        return CLEAR;
      case commands::KeyEvent::TAB:
        return FOCUS_CANDIDATE_NEXT_PAGE;
      default:
        return DO_NOTHING_WITH_CONSUME;
    }
  }

  LOG(ERROR) << "We should NOT process here.";
  return DO_NOTHING_WITH_CONSUME;
}

KeyCommand DefaultKeymap::ProcessModifierKey(
    const commands::KeyEvent &key_event, ConverterState state) {
  DCHECK_NE(0, key_event.modifier_keys_size());
  DCHECK(!key_event.has_special_key());
  DCHECK(!key_event.has_key_code());

  const uint32 modifiers = KeyEventUtil::GetModifiers(key_event);

  if (KeyEventUtil::IsShift(modifiers)) {
    return TOGGLE_DIRECT_MODE;
  }

  return DO_NOTHING_WITHOUT_CONSUME;
}

}  // namespace keymap
}  // namespace pinyin
}  // namespace mozc
