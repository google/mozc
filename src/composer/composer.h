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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/btree_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "composer/composition.h"
#include "composer/composition_input.h"
#include "composer/table.h"
#include "composer/transliterators.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/friend_test.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace composer {

std::shared_ptr<const commands::Request> GetSharedDefaultRequest();

// ComposerData is a data structure that represents the current state of the
// composer. It is used by Converter, Predictor and Rewriters as a const
// object.
class ComposerData {
 public:
  ComposerData() = delete;
  ComposerData(Composition composition, size_t position,
               transliteration::TransliterationType input_mode,
               commands::Context::InputFieldType input_field_type,
               std::string source_text,
               std::vector<commands::SessionCommand::CompositionEvent>
                   compositions_for_handwriting);

  // Copyable and movable.
  ComposerData(const ComposerData &other) = default;
  ComposerData &operator=(const ComposerData &) = default;
  ComposerData(ComposerData &&other) = default;
  ComposerData &operator=(ComposerData &&) = default;

  transliteration::TransliterationType GetInputMode() const;

  // Returns a preedit string with user's preferences.
  std::string GetStringForPreedit() const;

  // Returns a conversion query normalized ascii characters in half width
  std::string GetQueryForConversion() const;

  // Returns a prediction query trimmed the tail alphabet characters.
  std::string GetQueryForPrediction() const;

  // Returns a expanded prediction query.
  std::pair<std::string, absl::btree_set<std::string>> GetQueriesForPrediction()
      const;

  // Returns a string to be used for type correction.
  std::string GetStringForTypeCorrection() const;

  size_t GetLength() const;
  size_t GetCursor() const;

  absl::Span<const commands::SessionCommand::CompositionEvent>
  GetHandwritingCompositions() const;

  // Returns raw input from a user.
  // The main purpose is Transliteration.
  std::string GetRawString() const;

  // Returns substring of raw input.  The position and size is based on the
  // composed string.  For example, when [さ|sa][し|shi][み|mi] is the
  // composition, GetRawSubString(0, 2) returns "sashi".
  std::string GetRawSubString(size_t position, size_t size) const;

  // Generate transliterations.
  void GetTransliterations(transliteration::Transliterations *t13ns) const;

  // Generate substrings of transliterations.
  void GetSubTransliterations(size_t position, size_t size,
                              transliteration::Transliterations *t13ns) const;

  absl::string_view source_text() const { return source_text_; }

 private:
  // Composition copied from the Composer as a snapshot.
  Composition composition_;

  // Cursor position.
  size_t position_ = 0;

  // Input mode of IME (e.g. HIRAGANA, HALF_ASCII, etc.)
  transliteration::TransliterationType input_mode_ = transliteration::HIRAGANA;

  // Type of the input field to input texts.
  commands::Context::InputFieldType input_field_type_ =
      commands::Context::NORMAL;

  // The original text for the composition.
  // The value is usually empty, and used for reverse conversion.
  std::string source_text_;

  // Example:
  //   {{"かん字", 0.99}, {"かlv字", 0.01}}
  // Please refer to commands.proto
  std::vector<commands::SessionCommand::CompositionEvent>
      compositions_for_handwriting_;
};

// Composer is a class that manages the composing text. It provides methods to
// edit the text by users. Composer creates ComposerData as the snapshot of
// the current state of the composer.
class Composer final {
 public:
  // Pseudo commands in composer.
  enum InternalCommand {
    REWIND,
    STOP_KEY_TOGGLING,
  };

  Composer();
  Composer(std::shared_ptr<const Table> table,
           std::shared_ptr<const commands::Request> request,
           std::shared_ptr<const config::Config> config);

  // This constructor is for testing.
  ABSL_DEPRECATED("Use the constructor with Table")
  Composer(std::shared_ptr<const commands::Request> request,
           std::shared_ptr<const config::Config> config);

  // Copies `request` and `config`.

  ABSL_DEPRECATED("Use the constructor with Table")
  Composer(commands::Request request, config::Config config);
  Composer(std::shared_ptr<const Table> table, commands::Request request,
           config::Config config);

  // Copyable and movable.
  Composer(const Composer &) = default;
  Composer &operator=(const Composer &) = default;
  Composer(Composer &&) = default;
  Composer &operator=(Composer &&) = default;

  // Returns a reference of empty ComposerData used for placeholder.
  static const ComposerData &EmptyComposerData();

  // Return a ComposerData with the current state of the composer.
  ComposerData CreateComposerData() const;

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

  void SetTable(std::shared_ptr<const Table> table);

  void SetRequest(std::shared_ptr<const commands::Request> request);
  void SetConfig(std::shared_ptr<const config::Config> config);

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
  std::string GetStringForPreedit() const;

  // Returns a submit string with user's preferences.  The difference
  // from the preedit string is the handling of the last 'n'.
  std::string GetStringForSubmission() const;

  // Returns a conversion query normalized ascii characters in half width
  std::string GetQueryForConversion() const;

  // Returns a prediction query trimmed the tail alphabet characters.
  std::string GetQueryForPrediction() const;

  // Returns a expanded prediction query.
  std::pair<std::string, absl::btree_set<std::string>> GetQueriesForPrediction()
      const;

  // Returns a string to be used for type correction.
  std::string GetStringForTypeCorrection() const;

  size_t GetLength() const;
  size_t GetCursor() const;
  void EditErase();

  // Deletes a character at specified position.
  void DeleteAt(size_t pos);
  // Delete multiple characters beginning at specified position.
  void DeleteRange(size_t pos, size_t length);

  void InsertCharacter(std::string key);

  // Set preedit text to composer.
  //
  // If you want to set preedit text for testing
  // (to convert from HIRAGANA string rather than key input),
  // you should use SetPreeditTextForTestOnly().
  // With the current implementation, prediction queries can be transliterated
  // and you will not be able to get right candidates.
  void InsertCharacterPreedit(absl::string_view input);

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
  void SetPreeditTextForTestOnly(absl::string_view input);

  // Set compositions from handwriting recognition results.
  // The composition may contain Kana-Kanji mixed string. (ex. "かん字")
  // Handwriting engine can generate multiple candidates.
  void SetCompositionsForHandwriting(
      absl::Span<const commands::SessionCommand::CompositionEvent *const>
          compositions);
  absl::Span<const commands::SessionCommand::CompositionEvent>
  GetHandwritingCompositions() const;

  bool InsertCharacterKeyAndPreedit(absl::string_view key,
                                    absl::string_view preedit);
  bool InsertCharacterKeyEvent(const commands::KeyEvent &key);
  void InsertCommandCharacter(InternalCommand internal_command);
  void Delete();
  void Backspace();

  // void Undo();
  void MoveCursorLeft();
  void MoveCursorRight();
  void MoveCursorToBeginning();
  void MoveCursorToEnd();
  void MoveCursorTo(uint32_t new_position);

  // Returns raw input from a user.
  // The main purpose is Transliteration.
  std::string GetRawString() const;

  // Returns substring of raw input.  The position and size is based on the
  // composed string.  For example, when [さ|sa][し|shi][み|mi] is the
  // composition, GetRawSubString(0, 2) returns "sashi".
  std::string GetRawSubString(size_t position, size_t size) const;

  // Generate transliterations.
  void GetTransliterations(transliteration::Transliterations *t13ns) const;

  // Generate substrings of specified transliteration.
  std::string GetSubTransliteration(transliteration::TransliterationType type,
                                    size_t position, size_t size) const;

  // Generate substrings of transliterations.
  void GetSubTransliterations(size_t position, size_t size,
                              transliteration::Transliterations *t13ns) const;

  // Check if the preedit is can be modified.
  bool EnableInsert() const;

  // Automatically switch the composition mode according to the current
  // status and user's settings.
  void AutoSwitchMode();

  // Return true if the composition is advised to be committed immediately.
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

  // Returns true when the current character at cursor position is toggleable.
  bool IsToggleable() const;

  bool is_new_input() const { return is_new_input_; }
  size_t shifted_sequence_count() const { return shifted_sequence_count_; }
  absl::string_view source_text() const { return source_text_; }
  std::string *mutable_source_text() { return &source_text_; }
  void set_source_text(absl::string_view source_text);
  size_t max_length() const { return max_length_; }
  void set_max_length(size_t length) { max_length_ = length; }

  int timeout_threshold_msec() const { return timeout_threshold_msec_; }
  void set_timeout_threshold_msec(int threshold_msec) {
    timeout_threshold_msec_ = threshold_msec;
  }

 private:
  FRIEND_TEST(ComposerTest, ApplyTemporaryInputMode);

  bool ProcessCompositionInput(CompositionInput input);

  // Change input mode temporarily according to the current context and
  // the given input character.
  // This function have a bug when key has characters input with Preedit.
  // Expected behavior: InsertPreedit("A") + InsertKey("a") -> "Aあ"
  // Actual behavior:   InsertPreedit("A") + InsertKey("a") -> "Aa"
  void ApplyTemporaryInputMode(absl::string_view input, bool caps_locked);

  // Generate transliterated substrings.
  std::string GetTransliteratedText(Transliterators::Transliterator t12r,
                                    size_t position, size_t size) const;

  size_t position_;
  transliteration::TransliterationType input_mode_;
  transliteration::TransliterationType output_mode_;
  // On reset, comeback_input_mode_ is used as the input mode.
  transliteration::TransliterationType comeback_input_mode_;
  // Type of the input field to input texts.
  commands::Context::InputFieldType input_field_type_;

  size_t shifted_sequence_count_;

  // The original text for the composition.  The value is usually
  // empty, and used for reverse conversion.
  std::string source_text_;

  size_t max_length_;

  std::shared_ptr<const commands::Request> request_;
  std::shared_ptr<const config::Config> config_;

  // Though we would like to avoid shared object, Table is not copyable so
  // there is no other way to share them at this moment. The internal data
  // of the Table is managed by std::unique_ptr, which makes simple copying
  // impossible. Furthermore, copying the table every time would result in
  // a significant performance degradation. Style guide says that we prefer
  // to use std::shared_ptr for shared object.
  std::shared_ptr<const Table> table_;

  Composition composition_;

  // Timestamp of last modified.
  int64_t timestamp_msec_ = 0;

  // If the duration between key inputs is more than timeout_threadhols_msec_,
  // the STOP_KEY_TOGGLING event is sent before the next key input.
  // If the value is 0, STOP_KEY_TOGGLING is not sent.
  int timeout_threshold_msec_;

  // Whether the next insertion is the beginning of typing after an
  // editing command like SetInputMode or not.  Some conversion rules
  // refer this state.  Assuming the input events are
  // "abc<left-cursor>d", when "a" or "d" is typed, this value should
  // be true.  When "b" or "c" is typed, the value should be false.
  bool is_new_input_;

  // Example:
  //   {{"かん字", 0.99}, {"かlv字", 0.01}}
  // Please refer to commands.proto
  std::vector<commands::SessionCommand::CompositionEvent>
      compositions_for_handwriting_;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_COMPOSER_H_
