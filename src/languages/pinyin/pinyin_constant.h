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

#ifndef MOZC_LANGUAGES_PINYIN_PINYIN_CONSTANT_H_
#define MOZC_LANGUAGES_PINYIN_PINYIN_CONSTANT_H_

namespace mozc {
namespace pinyin {
enum ConversionMode {
  NONE,         // Initial state.
  PINYIN,       // Parses alphabet as pinyin, and converts it to phrase.
  DIRECT,       // Inputs input characters.
  ENGLISH,      // Suggests English words.
  PUNCTUATION,  // Suggests punctuations.
};

namespace keymap {
// TODO(hsumita): Merge this enum to ConversionMode enum.
enum KeymapMode {
  PINYIN,
  DIRECT,
  ENGLISH,
  PUNCTUATION,
};

enum KeyCommand {
  // When we execute INSERT command, we should determine which we consume the
  // key event or not using a return value of PinyinContext::Insert().
  // Other commands should consume the key event excepted for
  // DO_NOTHING_WITHOUT_CONSUME.
  INSERT,
  INSERT_PUNCTUATION,
  COMMIT,
  COMMIT_PREEDIT,
  CLEAR,
  // Select current candidate and commit. Input key should NOT be consumed.
  // This command is only for Pinyin mode.
  AUTO_COMMIT,

  MOVE_CURSOR_RIGHT,
  MOVE_CURSOR_LEFT,
  MOVE_CURSOR_RIGHT_BY_WORD,
  MOVE_CURSOR_LEFT_BY_WORD,
  MOVE_CURSOR_TO_BEGINNING,
  MOVE_CURSOR_TO_END,

  SELECT_CANDIDATE,
  SELECT_FOCUSED_CANDIDATE,
  SELECT_SECOND_CANDIDATE,
  SELECT_THIRD_CANDIDATE,
  FOCUS_CANDIDATE,
  // Focuses to candidate[0]
  FOCUS_CANDIDATE_TOP,
  FOCUS_CANDIDATE_PREV,
  FOCUS_CANDIDATE_NEXT,
  FOCUS_CANDIDATE_PREV_PAGE,
  FOCUS_CANDIDATE_NEXT_PAGE,
  CLEAR_CANDIDATE_FROM_HISTORY,

  REMOVE_CHAR_BEFORE,
  REMOVE_CHAR_AFTER,
  REMOVE_WORD_BEFORE,
  REMOVE_WORD_AFTER,

  // Commands for session.

  TOGGLE_DIRECT_MODE,
  TURN_ON_ENGLISH_MODE,
  TURN_ON_PUNCTUATION_MODE,
  TOGGLE_SIMPLIFIED_CHINESE_MODE,
  // Does nothing. Key event should be consumed.
  DO_NOTHING_WITH_CONSUME,
  // Does nothing. Key event should NOT be consumed.
  DO_NOTHING_WITHOUT_CONSUME,
};

enum ConverterState {
  INACTIVE,
  ACTIVE,
};
}  // namespace keymap
}  // namespace pinyin
}  // namespace mozc

#endif  // MOZC_LANGUAGES_PINYIN_PINYIN_CONSTANT_H_
