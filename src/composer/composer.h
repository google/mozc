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

// Interactive composer from a Roman string to a Hiragana string

#ifndef MOZC_COMPOSER_COMPOSER_H_
#define MOZC_COMPOSER_COMPOSER_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/port.h"
#include "base/protobuf/repeated_field.h"
#include "composer/composition_interface.h"
#include "composer/internal/transliterators.h"
#include "composer/internal/typing_corrector.h"
#include "composer/type_corrected_query.h"
#include "protocol/commands.pb.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace composer {

class Table;

typedef commands::KeyEvent::ProbableKeyEvent ProbableKeyEvent;
typedef mozc::protobuf::RepeatedPtrField<ProbableKeyEvent> ProbableKeyEvents;

class Composer {
 public:
  // Pseudo commands in composer.
  enum InternalCommand {
    REWIND,
    STOP_KEY_TOGGLING,
  };

  Composer();
  Composer(const Table *table, const commands::Request *request,
           const config::Config *config);
  virtual ~Composer();

  // Reset all composing data except table.
  void Reset();

  // Reset input mode.  When the current input mode is
  // HalfAlphanumeric by pressing shifted alphabet, this function
  // revert the input mode from HalfAlphanumeric to the previous input
  // mode.
  void ResetInputMode();

  // Reload the configuration.
  void ReloadConfig();

  // Check the preedit string is empty or not.
  bool Empty() const;

  void SetTable(const Table *table);

  void SetRequest(const commands::Request *request);
  void SetConfig(const config::Config *config);

  void SetInputMode(transliteration::TransliterationType mode);
  void SetTemporaryInputMode(transliteration::TransliterationType mode);
  void SetInputFieldType(commands::Context::InputFieldType type);
  commands::Context::InputFieldType GetInputFieldType() const;

  // Update the input mode considering the input modes of the
  // surrounding characters.
  // If the input mode should not be changed based on the surrounding text,
  // do not call this method (e.g. CursorToEnd, CursorToBeginning).
  void UpdateInputMode();

  transliteration::TransliterationType GetInputMode() const;
  transliteration::TransliterationType GetComebackInputMode() const;
  void ToggleInputMode();

  transliteration::TransliterationType GetOutputMode() const;
  void SetOutputMode(transliteration::TransliterationType mode);

  // Returns preedit strings
  void GetPreedit(std::string *left, std::string *focused,
                  std::string *right) const;

  // Returns a preedit string with user's preferences.
  void GetStringForPreedit(std::string *output) const;

  // Returns a submit string with user's preferences.  The difference
  // from the preedit string is the handling of the last 'n'.
  void GetStringForSubmission(std::string *output) const;

  // Returns a conversion query normalized ascii characters in half width
  void GetQueryForConversion(std::string *output) const;

  // Returns a prediction query trimmed the tail alphabet characters.
  void GetQueryForPrediction(std::string *output) const;

  // Returns a expanded prediction query.
  void GetQueriesForPrediction(std::string *base,
                               std::set<std::string> *expanded) const;

  // Returns a type-corrected prediction queries.
  void GetTypeCorrectedQueriesForPrediction(
      std::vector<TypeCorrectedQuery> *queries) const;

  size_t GetLength() const;
  size_t GetCursor() const;
  void EditErase();

  // Deletes a character at specified position.
  void DeleteAt(size_t pos);
  // Delete multiple characters beginning at specified position.
  void DeleteRange(size_t pos, size_t length);

  void InsertCharacter(const std::string &input);

  // Set preedit text to composer.
  //
  // If you want to set preedit text for testing
  // (to convert from HIRAGANA string rather than key input),
  // you should use SetPreeditTextForTestOnly().
  // With the current implementation, prediction queries can be transliterated
  // and you will not be able to get right candidates.
  void InsertCharacterPreedit(const std::string &input);

  // TEST ONLY: Set preedit text to composer.
  //
  // The |input| will be used in as-is form for GetStringForPreedit()
  // and GetStringForSubmission().
  // For GetQueryForConversion() and GetQueryForPrediction(), |input| will be
  // used as normalized ascii characters in half width.
  //
  // For example, when the |input| will be set as "mo", suggestion will be
  // triggered by "mo", rather than "も", or "ｍｏ", etc.
  //
  // If the input is ascii characters, input mode will be set as HALF_ASCII.
  // This is useful to test the behavior of alphabet keyboard.
  void SetPreeditTextForTestOnly(const std::string &input);

  bool InsertCharacterKeyAndPreedit(const std::string &key,
                                    const std::string &preedit);
  void InsertCharacterForProbableKeyEvents(
      const std::string &input, const ProbableKeyEvents &probable_key_events);
  void InsertCharacterPreeditForProbableKeyEvents(
      const std::string &input, const ProbableKeyEvents &probable_key_events);
  void InsertCharacterKeyAndPreeditForProbableKeyEvents(
      const std::string &key, const std::string &preedit,
      const ProbableKeyEvents &probable_key_events);
  bool InsertCharacterKeyEvent(const commands::KeyEvent &key);
  void InsertCommandCharacter(const InternalCommand internal_command);
  void Delete();
  void Backspace();

  // void Undo();
  void MoveCursorLeft();
  void MoveCursorRight();
  void MoveCursorToBeginning();
  void MoveCursorToEnd();
  void MoveCursorTo(uint32 new_position);

  // Returns raw input from a user.
  // The main purpose is Transliteration.
  void GetRawString(std::string *raw_string) const;

  // Returns substring of raw input.  The position and size is based on the
  // composed string.  For example, when [さ|sa][し|shi][み|mi] is the
  // composition, GetRawSubString(0, 2) returns "sashi".
  void GetRawSubString(const size_t position, const size_t size,
                       std::string *raw_sub_string) const;

  // Generate transliterations.
  void GetTransliterations(transliteration::Transliterations *t13ns) const;

  // Generate substrings of specified transliteration.
  void GetSubTransliteration(const transliteration::TransliterationType type,
                             const size_t position, const size_t size,
                             std::string *transliteration) const;

  // Generate substrings of transliterations.
  void GetSubTransliterations(size_t position, size_t size,
                              transliteration::Transliterations *t13ns) const;

  // Check if the preedit is can be modified.
  bool EnableInsert() const;

  // Automatically switch the composition mode according to the current
  // status and user's settings.
  void AutoSwitchMode();

  // Return true if the composition is adviced to be committed immediately.
  bool ShouldCommit() const;

  // Returns true if characters at the head of the preedit should be committed
  // immediately.
  // This is used for implementing password input mode in Android.
  // We cannot use direct input mode because it cannot deal with toggle input.
  // In password mode, first character in composition should be committed
  // when another letter is generated in composition.
  bool ShouldCommitHead(size_t *length_to_commit) const;

  // Transform characters for preferred number format.  If any
  // characters are transformed true is returned.
  // For example, if the query is "ー１、０００。５", it should be
  // transformed to "−１，０００．５".  and true is returned.
  static bool TransformCharactersForNumbers(std::string *query);

  // Set new input flag.
  // By calling this method, next inserted character will introduce
  // new chunk if the character has NewChunk attribute.
  void SetNewInput();

  void CopyFrom(const Composer &src);

  // Returns true when the current character at cursor position is toggleable.
  bool IsToggleable() const;

  bool is_new_input() const;
  size_t shifted_sequence_count() const;
  const std::string &source_text() const;
  std::string *mutable_source_text();
  void set_source_text(const std::string &source_text);
  size_t max_length() const;
  void set_max_length(size_t length);

  void set_composition(std::unique_ptr<CompositionInterface> composition) {
    composition_ = std::move(composition);
  }

 private:
  FRIEND_TEST(ComposerTest, ApplyTemporaryInputMode);

  bool InsertCharacterInternal(const std::string &input);
  bool InsertCharacterKeyAndPreeditInternal(const std::string &key,
                                            const std::string &preedit);

  // Change input mode temporarily accoding to the current context and
  // the given input character.
  // This function have a bug when key has characters input with Preedit.
  // Expected behavior: InsertPreedit("A") + InsertKey("a") -> "Aあ"
  // Actual behavior:   InsertPreedit("A") + InsertKey("a") -> "Aa"
  void ApplyTemporaryInputMode(const std::string &key, bool caps_locked);

  // Generate transliterated substrings.
  void GetTransliteratedText(Transliterators::Transliterator transliterator,
                             const size_t position, const size_t size,
                             std::string *output) const;

  size_t position_;
  // Whether the next insertion is the beginning of typing after an
  // editing command like SetInputMode or not.  Some conversion rules
  // refer this state.  Assuming the input events are
  // "abc<left-cursor>d", when "a" or "d" is typed, this value should
  // be true.  When "b" or "c" is typed, the value should be false.
  bool is_new_input_;
  transliteration::TransliterationType input_mode_;
  transliteration::TransliterationType output_mode_;
  // On reset, comeback_input_mode_ is used as the input mode.
  transliteration::TransliterationType comeback_input_mode_;
  // Type of the input field to input texts.
  commands::Context::InputFieldType input_field_type_;

  size_t shifted_sequence_count_;
  std::unique_ptr<CompositionInterface> composition_;

  TypingCorrector typing_corrector_;

  // The original text for the composition.  The value is usually
  // empty, and used for reverse conversion.
  std::string source_text_;

  size_t max_length_;

  const commands::Request *request_;
  const config::Config *config_;

  DISALLOW_COPY_AND_ASSIGN(Composer);
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_COMPOSER_H_
