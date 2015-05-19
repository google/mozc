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

#include "languages/pinyin/keymap.h"

#include <cctype>

#include "base/port.h"
#include "base/singleton.h"
#include "languages/pinyin/configurable_keymap.h"
#include "languages/pinyin/default_keymap.h"
#include "session/commands.pb.h"
#include "session/key_event_util.h"

namespace mozc {
namespace pinyin {
namespace keymap {

// TODO(hsumita): Investigates the behavior of "@" key when converter is active.

////////////////////////////////////////////////////////////
// Pinyin
////////////////////////////////////////////////////////////

class PinyinKeymapImpl : public KeymapInterface {
 public:
  PinyinKeymapImpl() {}
  virtual ~PinyinKeymapImpl() {}

  bool GetCommand(const commands::KeyEvent &key_event,
                  ConverterState state, KeyCommand *key_command) const {
    DCHECK(key_command);
    DCHECK(!KeyEventUtil::HasCaps(KeyEventUtil::GetModifiers(key_event)));
    DCHECK(!KeyEventUtil::IsNumpadKey(key_event));

    if (ConfigurableKeymap::GetCommand(key_event, state, key_command) ||
        DefaultKeymap::GetCommand(key_event, state, key_command)) {
      return true;
    }

    LOG(ERROR) << "Should not reach here.";
    *key_command = DO_NOTHING_WITHOUT_CONSUME;
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PinyinKeymapImpl);
};

////////////////////////////////////////////////////////////
// Direct
////////////////////////////////////////////////////////////

class DirectKeymapImpl : public KeymapInterface {
 public:
  DirectKeymapImpl() {}
  virtual ~DirectKeymapImpl() {}

  bool GetCommand(const commands::KeyEvent &key_event,
                  ConverterState state, KeyCommand *key_command) const {
    DCHECK(key_command);
    DCHECK(!KeyEventUtil::HasCaps(KeyEventUtil::GetModifiers(key_event)));
    DCHECK(!KeyEventUtil::IsNumpadKey(key_event));

    const uint32 modifiers = KeyEventUtil::GetModifiers(key_event);

    if (KeyEventUtil::IsCtrlShift(modifiers) && key_event.has_key_code() &&
        (key_event.key_code() == 'f' || key_event.key_code() == 'F')) {
      *key_command = TOGGLE_SIMPLIFIED_CHINESE_MODE;
      return true;
    }

    if (KeyEventUtil::HasAlt(modifiers) || KeyEventUtil::HasCtrl(modifiers)) {
      *key_command = DO_NOTHING_WITHOUT_CONSUME;
      return true;
    }

    if (key_event.has_key_code()) {
      *key_command = INSERT;
      return true;
    }

    if (KeyEventUtil::IsShift(modifiers) && !key_event.has_special_key()) {
      *key_command = TOGGLE_DIRECT_MODE;
      return true;
    }

    *key_command = DO_NOTHING_WITHOUT_CONSUME;
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DirectKeymapImpl);
};

////////////////////////////////////////////////////////////
// English
////////////////////////////////////////////////////////////

class EnglishKeymapImpl : public KeymapInterface {
 public:
  EnglishKeymapImpl() {}
  virtual ~EnglishKeymapImpl() {}

  bool GetCommand(const commands::KeyEvent &key_event,
                  ConverterState state, KeyCommand *key_command) const {
    DCHECK(key_command);
    DCHECK(!KeyEventUtil::HasCaps(KeyEventUtil::GetModifiers(key_event)));
    DCHECK(!KeyEventUtil::IsNumpadKey(key_event));

    const uint32 modifiers = KeyEventUtil::GetModifiers(key_event);
    if (!KeyEventUtil::HasCtrl(modifiers) && !KeyEventUtil::HasAlt(modifiers) &&
        key_event.has_key_code() && isalpha(key_event.key_code())) {
      *key_command = INSERT;
      return true;
    }

    if (!ConfigurableKeymap::GetCommand(key_event, state, key_command) &&
        !DefaultKeymap::GetCommand(key_event, state, key_command)) {
      LOG(ERROR) << "Should not reach here.";
      *key_command = DO_NOTHING_WITHOUT_CONSUME;
      return false;
    }

    *key_command = GetOverridedCommand(*key_command);
    return true;
  }

 private:
  // Override some commands which is differ from Pinyin keymap. Some commands
  // does not make sense on this mode, so we replace these.
  // To keep codes simple, and avoid leeks of conversion, we directly execute
  // conversion from KeyCommand to KeyCommand instead of conversion from
  // KeyEvent to KeyCommand.
  KeyCommand GetOverridedCommand(KeyCommand key_command) const {
    switch (key_command) {
      case AUTO_COMMIT:               return DO_NOTHING_WITH_CONSUME;
      case MOVE_CURSOR_LEFT:          return FOCUS_CANDIDATE_TOP;
      case MOVE_CURSOR_RIGHT:         return FOCUS_CANDIDATE_TOP;
      case MOVE_CURSOR_LEFT_BY_WORD:  return FOCUS_CANDIDATE_TOP;
      case MOVE_CURSOR_RIGHT_BY_WORD: return FOCUS_CANDIDATE_TOP;
      case MOVE_CURSOR_TO_BEGINNING:  return FOCUS_CANDIDATE_TOP;
      case MOVE_CURSOR_TO_END:        return FOCUS_CANDIDATE_TOP;
      case REMOVE_WORD_BEFORE:        return DO_NOTHING_WITHOUT_CONSUME;
      case REMOVE_WORD_AFTER:         return DO_NOTHING_WITHOUT_CONSUME;
      case TOGGLE_DIRECT_MODE:        return DO_NOTHING_WITHOUT_CONSUME;
      default:                        return key_command;
    }
  }

  DISALLOW_COPY_AND_ASSIGN(EnglishKeymapImpl);
};

////////////////////////////////////////////////////////////
// Punctuation
////////////////////////////////////////////////////////////

class PunctuationKeymapImpl : public KeymapInterface {
 public:
  PunctuationKeymapImpl() {}
  virtual ~PunctuationKeymapImpl() {}

  bool GetCommand(const commands::KeyEvent &key_event,
                  ConverterState state, KeyCommand *key_command) const {
    DCHECK(key_command);
    DCHECK(!KeyEventUtil::HasCaps(KeyEventUtil::GetModifiers(key_event)));
    DCHECK(!KeyEventUtil::IsNumpadKey(key_event));

    const uint32 modifiers = KeyEventUtil::GetModifiers(key_event);
    if (!KeyEventUtil::HasAlt(modifiers) && !KeyEventUtil::HasCtrl(modifiers) &&
        key_event.has_key_code()) {
      *key_command = INSERT;
      return true;
    }

    if (!ConfigurableKeymap::GetCommand(key_event, state, key_command) &&
        !DefaultKeymap::GetCommand(key_event, state, key_command)) {
      LOG(ERROR) << "Should not reach here.";
      *key_command = DO_NOTHING_WITHOUT_CONSUME;
      return false;
    }

    *key_command = GetOverridedCommand(*key_command);
    return true;
  }

 private:
  // Override some commands which is differ from Pinyin keymap. Some commands
  // does not make sense on this mode, so we replace these.
  // To keep codes simple, and avoid leeks of conversion, we directly execute
  // conversion from KeyCommand to KeyCommand instead of conversion from
  // KeyEvent to KeyCommand.
  KeyCommand GetOverridedCommand(KeyCommand key_command) const {
    switch (key_command) {
      case AUTO_COMMIT:               return INSERT;
      case COMMIT:                    return COMMIT_PREEDIT;
      case TOGGLE_DIRECT_MODE:        return DO_NOTHING_WITHOUT_CONSUME;
      case TURN_ON_PUNCTUATION_MODE:  return INSERT;
      default:                        return key_command;
    }
  }

  map<KeyCommand, KeyCommand> key_command_map_;

  DISALLOW_COPY_AND_ASSIGN(PunctuationKeymapImpl);
};

////////////////////////////////////////////////////////////
// Keymap Factory
////////////////////////////////////////////////////////////

const KeymapInterface *KeymapFactory::GetKeymap(KeymapMode mode) {
  switch (mode) {
    case PINYIN:
      return Singleton<PinyinKeymapImpl>::get();
    case DIRECT:
      return Singleton<DirectKeymapImpl>::get();
    case ENGLISH:
      return Singleton<EnglishKeymapImpl>::get();
    case PUNCTUATION:
      return Singleton<PunctuationKeymapImpl>::get();
    default:
      LOG(ERROR) << "Should not reach here";
      return Singleton<PinyinKeymapImpl>::get();
  }
}

}  // namespace keymap
}  // namespace pinyin
}  // namespace mozc
