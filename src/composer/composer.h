// Copyright 2010, Google Inc.
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

#include <set>
#include <string>
#include <vector>

#include "base/base.h"
#include "transliteration/transliteration.h"
#include "composer/composition_interface.h"

namespace mozc {
namespace commands {
class Context;
class KeyEvent;
}  // namespace commands

namespace composer {

class Table;

class Composer {
 public:
  explicit Composer(CompositionInterface *composition);
  virtual ~Composer();

  // Reset all composing data except table.
  void Reset();

  // Reset input mode.  When the current input mode is
  // HalfAlphanumeric by pressing capital alphabet, this function
  // revert the input mode from HalfAlphanumeric to the previous input
  // mode.
  void ResetInputMode();

  // Reload the configuration.
  void ReloadConfig();

  // Check the preedit string is empty or not.
  bool Empty() const;

  void SetTable(const Table *table);

  // Add a conversion rule to the table.
  void AddRule(const string &input,
               const string &output,
               const string &pending);

  void SetInputMode(transliteration::TransliterationType mode);
  void SetTemporaryInputMode(transliteration::TransliterationType mode);

  // Update the input mode considering the input modes of the
  // surrounding characters.
  void UpdateInputMode();

  transliteration::TransliterationType GetInputMode() const;
  void ToggleInputMode();

  transliteration::TransliterationType GetOutputMode() const;
  void SetOutputMode(transliteration::TransliterationType mode);

  // Return preedit strings
  void GetPreedit(string *left, string *focused, string *right) const;

  // Return a preedit string with user's preferences.
  void GetStringForPreedit(string *output) const;

  // Return a submit string with user's preferences.  The difference
  // from the preedit string is the handling of the last 'n'.
  void GetStringForSubmission(string *output) const;

  // Return a conversion query normalized ascii characters in half width
  void GetQueryForConversion(string *output) const;

  // Return a prediction query trimmed the tail alphabet characters.
  void GetQueryForPrediction(string *output) const;

  size_t GetLength() const;
  size_t GetCursor() const;
  void EditErase();

  void InsertCharacterAt(size_t pos, const string &input);
  void DeleteAt(size_t pos);
  void BackspaceAt(size_t pos);
  void InsertCharacter(const string &input);
  void InsertCharacterPreedit(const string &input);
  void InsertCharacterKeyAndPreedit(const string &key, const string &preedit);
  void InsertCharacterPreeditAt(size_t pos, const string &input);
  void InsertCharacterKeyAndPreeditAt(size_t pos,
                                      const string &key,
                                      const string &preedit);
  bool InsertCharacterKeyEvent(const commands::KeyEvent &key);
  void Delete();
  void Backspace();

  // void Undo();
  void MoveCursorLeft();
  void MoveCursorRight();
  void MoveCursorToBeginning();
  void MoveCursorToEnd();

  // Generate transliterations.
  void GetTransliterations(transliteration::Transliterations *t13ns) const;

  // Generate substrings of specified transliteration.
  void GetSubTransliteration(const transliteration::TransliterationType type,
                             const size_t position,
                             const size_t size,
                             string *transliteration) const;

  // Generate substrings of transliterations.
  void GetSubTransliterations(size_t position,
                              size_t size,
                              transliteration::Transliterations *t13ns) const;

  // Check if the preedit is can be modified.
  bool EnableInsert() const;
  void set_max_length(size_t length);
  size_t max_length() const;

  // Automatically switch the composition mode according to the current
  // status and user's settings.
  void AutoSwitchMode();

  // Insert preceding text if |context| meets some condition.  It returns the
  // number of characters at the end of context.preceding_text() which are
  // involved to composition and must be deleted from preceding text.
  // NOTE: Do not call this function if client cannot delete preceding text.
  size_t InsertPrecedingText(const commands::Context &context);

  static Composer *Create(const Table *table);
  // Transform characters for preferred number format.  If any
  // characters are transformed true is returned.
  // For example, if the query is "ー１、０００。５", it should be
  // transformed to "−１，０００．５".  and true is returned.
  static bool TransformCharactersForNumbers(string *query);

 private:
  size_t position_;
  transliteration::TransliterationType input_mode_;
  transliteration::TransliterationType output_mode_;
  // On reset, comeback_input_mode_ is used as the input mode.
  transliteration::TransliterationType comeback_input_mode_;
  size_t capital_sequence_count_;
  scoped_ptr<CompositionInterface> composition_;

  size_t max_length_;

  DISALLOW_COPY_AND_ASSIGN(Composer);
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_COMPOSER_H_
