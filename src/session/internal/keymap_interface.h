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

// Keymap utils of Mozc interface.

#ifndef MOZC_SESSION_INTERNAL_KEYMAP_INTERFACE_H_
#define MOZC_SESSION_INTERNAL_KEYMAP_INTERFACE_H_

#include <map>
#include "base/base.h"

namespace mozc {
namespace commands {
class KeyEvent;
}

namespace keymap {

struct DirectInputState {
enum Commands {
  NONE = 0,
  IME_ON,
  // Space will not be sent to server.
  // If Config::space_character_form is FULL_WIDTH,
  // space should be full_width even in direct mode
  INSERT_SPACE,  // To handle spaces.
  // to handle shift+spaces (useally toggle half/full width)
  INSERT_ALTERNATE_SPACE,
  // Switch input mode.
  INPUT_MODE_HIRAGANA,
  INPUT_MODE_FULL_KATAKANA,
  INPUT_MODE_HALF_KATAKANA,
  INPUT_MODE_FULL_ALPHANUMERIC,
  INPUT_MODE_HALF_ALPHANUMERIC,
  RECONVERT,
};
};

struct PrecompositionState {
enum Commands {
  NONE = 0,
  IME_OFF,
  IME_ON,
  INSERT_CHARACTER,  // Move to Composition status.
  INSERT_SPACE,   // To handle spaces.
  // to handle shift+spaces (useally toggle half/full with)
  INSERT_ALTERNATE_SPACE,
  INSERT_HALF_SPACE,  // Input half-width space
  INSERT_FULL_SPACE,  // Input full-width space
  TOGGLE_ALPHANUMERIC_MODE,  // toggle AlphaNumeric and Hiragana mode.
  // Switch input mode.
  INPUT_MODE_HIRAGANA,
  INPUT_MODE_FULL_KATAKANA,
  INPUT_MODE_HALF_KATAKANA,
  INPUT_MODE_FULL_ALPHANUMERIC,
  INPUT_MODE_HALF_ALPHANUMERIC,
  INPUT_MODE_SWITCH_KANA_TYPE,  // rotate input mode
  LAUNCH_CONFIG_DIALOG,
  LAUNCH_DICTIONARY_TOOL,
  LAUNCH_WORD_REGISTER_DIALOG,
  REVERT,  // revert last operation (preedit still remains)
  UNDO,    // undo last operation (preedit is restored)
  ABORT,   // Abort the server.  The process is killed.
  RECONVERT,

  // For ZeroQuerySuggestion
  CANCEL,  // Back to Composition status.
  COMMIT_FIRST_SUGGESTION,   // ATOK's Shift-Enter style
  PREDICT_AND_CONVERT,
};
};

struct CompositionState {
enum Commands {
  NONE = 0,
  IME_OFF,
  IME_ON,
  INSERT_CHARACTER,
  DEL,  // DELETE cannot be used on Windows.  It is defined as a macro.
  BACKSPACE,
  INSERT_HALF_SPACE,  // Input half-width space
  INSERT_FULL_SPACE,  // Input full-width space
  CANCEL,  // Move to Precomposition stauts.
  UNDO,
  MOVE_CURSOR_LEFT,
  MOVE_CURSOR_RIGHT,
  MOVE_CURSOR_TO_BEGINNING,
  MOVE_MOVE_CURSOR_TO_END,
  COMMIT,  // Move to Precomposition status.
  COMMIT_FIRST_SUGGESTION,   // ATOK's Shift-Enter style
  CONVERT,  // Move to Conversion status.
  CONVERT_WITHOUT_HISTORY,  // Move to Conversion status.
  PREDICT_AND_CONVERT,
  // Switching to ConversionState
  CONVERT_TO_HIRAGANA,  // F6
  CONVERT_TO_FULL_KATAKANA,  // F7
  CONVERT_TO_HALF_KATAKANA,
  CONVERT_TO_HALF_WIDTH,  // F8
  CONVERT_TO_FULL_ALPHANUMERIC,  // F9
  CONVERT_TO_HALF_ALPHANUMERIC,  // F10
  SWITCH_KANA_TYPE,  // Muhenkan
  // Rmaining CompositionState
  DISPLAY_AS_HIRAGANA,  // F6
  DISPLAY_AS_FULL_KATAKANA,  // F7
  DISPLAY_AS_HALF_KATAKANA,
  TRANSLATE_HALF_WIDTH,  // F8
  TRANSLATE_FULL_ASCII,  // F9
  TRANSLATE_HALF_ASCII,  // F10
  TOGGLE_ALPHANUMERIC_MODE,  // toggle AlphaNumeric and Hiragana mode.
  // Switch input mode.
  INPUT_MODE_HIRAGANA,
  INPUT_MODE_FULL_KATAKANA,
  INPUT_MODE_HALF_KATAKANA,
  INPUT_MODE_FULL_ALPHANUMERIC,
  INPUT_MODE_HALF_ALPHANUMERIC,
  ABORT,  // Abort the server.  The process is killed.
};
};

struct ConversionState {
enum Commands {
  NONE = 0,
  IME_OFF,
  IME_ON,
  INSERT_CHARACTER,  // Submit and Move to Composition status.
  INSERT_HALF_SPACE,  // Input half-width space
  INSERT_FULL_SPACE,  // Input full-width space
  CANCEL,  // Back to Composition status.
  UNDO,
  SEGMENT_FOCUS_LEFT,
  SEGMENT_FOCUS_RIGHT,
  SEGMENT_FOCUS_FIRST,
  SEGMENT_FOCUS_LAST,
  SEGMENT_WIDTH_EXPAND,
  SEGMENT_WIDTH_SHRINK,
  CONVERT_NEXT,
  CONVERT_PREV,
  CONVERT_NEXT_PAGE,
  CONVERT_PREV_PAGE,
  PREDICT_AND_CONVERT,
  COMMIT,  // Move to Precomposition status.
  COMMIT_SEGMENT,  // Down on the ATOK style.
  // CONVERT_TO and TRANSLATE are same behavior on ConversionState.
  CONVERT_TO_HIRAGANA,  // F6
  CONVERT_TO_FULL_KATAKANA,  // F7
  CONVERT_TO_HALF_KATAKANA,
  CONVERT_TO_HALF_WIDTH,  // F8
  CONVERT_TO_FULL_ALPHANUMERIC,  // F9
  CONVERT_TO_HALF_ALPHANUMERIC,  // F10
  SWITCH_KANA_TYPE,  // Muhenkan
  DISPLAY_AS_HIRAGANA,  // F6
  DISPLAY_AS_FULL_KATAKANA,  // F7
  DISPLAY_AS_HALF_KATAKANA,
  TRANSLATE_HALF_WIDTH,  // F8
  TRANSLATE_FULL_ASCII,  // F9
  TRANSLATE_HALF_ASCII,  // F10
  TOGGLE_ALPHANUMERIC_MODE,  // toggle AlphaNumeric and Hiragana mode.
  // Switch input mode.
  INPUT_MODE_HIRAGANA,
  INPUT_MODE_FULL_KATAKANA,
  INPUT_MODE_HALF_KATAKANA,
  INPUT_MODE_FULL_ALPHANUMERIC,
  INPUT_MODE_HALF_ALPHANUMERIC,
  REPORT_BUG,
  ABORT,  // Abort the server.  The process is killed.
};
};


template<typename T>
class KeyMapInterface {
 public:
  virtual ~KeyMapInterface<T>() {}
  virtual bool GetCommand(const commands::KeyEvent &key_event,
                          T *command) const = 0;

  virtual bool AddRule(const commands::KeyEvent &key_event, T command) = 0;
};

}  // namespace keymap
}  // namespace mozc
#endif  // MOZC_SESSION_INTERNAL_KEYMAP_INTERFACE_H_
