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

#include "composer/composer.h"

#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/clock.h"
#include "base/japanese_util.h"
#include "base/logging.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "composer/internal/composition.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/mode_switching_handler.h"
#include "composer/internal/transliterators.h"
#include "composer/internal/typing_corrector.h"
#include "composer/key_event_util.h"
#include "composer/table.h"
#include "composer/type_corrected_query.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "transliteration/transliteration.h"
#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

// Use flags instead of constant for performance evaluation.
ABSL_FLAG(uint64_t, max_typing_correction_query_candidates, 40,
          "Maximum # of typing correction query temporary candidates.");
ABSL_FLAG(uint64_t, max_typing_correction_query_results, 8,
          "Maximum # of typing correction query results.");

namespace mozc {
namespace composer {
namespace {

using config::CharacterFormManager;
using strings::OneCharLen;

Transliterators::Transliterator GetTransliterator(
    transliteration::TransliterationType comp_mode) {
  switch (comp_mode) {
    case transliteration::HALF_ASCII:
    case transliteration::HALF_ASCII_UPPER:
    case transliteration::HALF_ASCII_LOWER:
    case transliteration::HALF_ASCII_CAPITALIZED:
      return Transliterators::HALF_ASCII;

    case transliteration::FULL_ASCII:
    case transliteration::FULL_ASCII_UPPER:
    case transliteration::FULL_ASCII_LOWER:
    case transliteration::FULL_ASCII_CAPITALIZED:
      return Transliterators::FULL_ASCII;

    case transliteration::HALF_KATAKANA:
      return Transliterators::HALF_KATAKANA;

    case transliteration::FULL_KATAKANA:
      return Transliterators::FULL_KATAKANA;

    case transliteration::HIRAGANA:
      return Transliterators::HIRAGANA;

    default:
      LOG(ERROR) << "Unknown TransliterationType: " << comp_mode;
      return Transliterators::CONVERSION_STRING;
  }
}

transliteration::TransliterationType GetTransliterationType(
    Transliterators::Transliterator transliterator,
    const transliteration::TransliterationType default_type) {
  if (transliterator == Transliterators::HIRAGANA) {
    return transliteration::HIRAGANA;
  }
  if (transliterator == Transliterators::HALF_ASCII) {
    return transliteration::HALF_ASCII;
  }
  if (transliterator == Transliterators::FULL_ASCII) {
    return transliteration::FULL_ASCII;
  }
  if (transliterator == Transliterators::FULL_KATAKANA) {
    return transliteration::FULL_KATAKANA;
  }
  if (transliterator == Transliterators::HALF_KATAKANA) {
    return transliteration::HALF_KATAKANA;
  }
  return default_type;
}

void Transliterate(const transliteration::TransliterationType mode,
                   const absl::string_view input, std::string *output) {
  // When the mode is HALF_KATAKANA, Full width ASCII is also
  // transformed.
  if (mode == transliteration::HALF_KATAKANA) {
    std::string tmp_input;
    japanese_util::HiraganaToKatakana(input, &tmp_input);
    japanese_util::FullWidthToHalfWidth(tmp_input, output);
    return;
  }

  switch (mode) {
    case transliteration::HALF_ASCII:
      japanese_util::FullWidthAsciiToHalfWidthAscii(input, output);
      break;
    case transliteration::HALF_ASCII_UPPER:
      japanese_util::FullWidthAsciiToHalfWidthAscii(input, output);
      Util::UpperString(output);
      break;
    case transliteration::HALF_ASCII_LOWER:
      japanese_util::FullWidthAsciiToHalfWidthAscii(input, output);
      Util::LowerString(output);
      break;
    case transliteration::HALF_ASCII_CAPITALIZED:
      japanese_util::FullWidthAsciiToHalfWidthAscii(input, output);
      Util::CapitalizeString(output);
      break;

    case transliteration::FULL_ASCII:
      japanese_util::HalfWidthAsciiToFullWidthAscii(input, output);
      break;
    case transliteration::FULL_ASCII_UPPER:
      japanese_util::HalfWidthAsciiToFullWidthAscii(input, output);
      Util::UpperString(output);
      break;
    case transliteration::FULL_ASCII_LOWER:
      japanese_util::HalfWidthAsciiToFullWidthAscii(input, output);
      Util::LowerString(output);
      break;
    case transliteration::FULL_ASCII_CAPITALIZED:
      japanese_util::HalfWidthAsciiToFullWidthAscii(input, output);
      Util::CapitalizeString(output);
      break;

    case transliteration::FULL_KATAKANA:
      japanese_util::HiraganaToKatakana(input, output);
      break;
    case transliteration::HIRAGANA:
      *output = std::string(input);
      break;
    default:
      LOG(ERROR) << "Unknown TransliterationType: " << mode;
      *output = std::string(input);
      break;
  }
}

transliteration::TransliterationType GetTransliterationTypeFromCompositionMode(
    const commands::CompositionMode mode) {
  switch (mode) {
    case commands::HIRAGANA:
      return transliteration::HIRAGANA;
    case commands::FULL_KATAKANA:
      return transliteration::FULL_KATAKANA;
    case commands::HALF_ASCII:
      return transliteration::HALF_ASCII;
    case commands::FULL_ASCII:
      return transliteration::FULL_ASCII;
    case commands::HALF_KATAKANA:
      return transliteration::HALF_KATAKANA;
    default:
      // commands::DIRECT or invalid mode.
      LOG(ERROR) << "Invalid CompositionMode: " << mode;
      return transliteration::HIRAGANA;
  }
}

// A map used in `GetQueriesForPrediction()`. The key is a modified Hiragana
// and the values are its related Hiragana characters that can be cycled by
// hitting the modifier key. For instance, there's a modification cycle
// つ -> っ -> づ -> つ. For this cycle, the multimap contains:
//   っ: [つ, づ]
//   づ: [つ, っ]
// If the composition ends with a key in this map, its corresponding values
// are removed from the expansion produced by `GetQueryForPrediction()`,
// whereby we can suppress prediction from unmodified key when one modified a
// character explicitly (e.g., we don't want to suggest words starting with
// "さ" when one typed "ざ" with modified key).
using ModifierRemovalMap =
    std::unordered_multimap<absl::string_view, absl::string_view,
                            absl::Hash<absl::string_view>>;

const ModifierRemovalMap *GetModifierRemovalMap() {
  static const ModifierRemovalMap *const removal_map = new ModifierRemovalMap{
      {"ぁ", "あ"}, {"ぃ", "い"}, {"ぅ", "う"}, {"ぅ", "ゔ"}, {"ゔ", "う"},
      {"ゔ", "ぅ"}, {"ぇ", "え"}, {"ぉ", "お"}, {"が", "か"}, {"ぎ", "き"},
      {"ぐ", "く"}, {"げ", "け"}, {"ご", "こ"}, {"ざ", "さ"}, {"じ", "し"},
      {"ず", "す"}, {"ぜ", "せ"}, {"ぞ", "そ"}, {"だ", "た"}, {"ぢ", "ち"},
      {"づ", "つ"}, {"づ", "っ"}, {"っ", "つ"}, {"っ", "づ"}, {"で", "て"},
      {"ど", "と"}, {"ば", "は"}, {"ば", "ぱ"}, {"ぱ", "は"}, {"ぱ", "ば"},
      {"び", "ひ"}, {"び", "ぴ"}, {"ぴ", "ひ"}, {"ぴ", "び"}, {"ぶ", "ふ"},
      {"ぶ", "ぷ"}, {"ぷ", "ふ"}, {"ぷ", "ぶ"}, {"べ", "へ"}, {"べ", "ぺ"},
      {"ぺ", "へ"}, {"ぺ", "べ"}, {"ぼ", "ほ"}, {"ぼ", "ぽ"}, {"ぽ", "ほ"},
      {"ぽ", "ぼ"}, {"ゃ", "や"}, {"ゅ", "ゆ"}, {"ょ", "よ"}, {"ゎ", "わ"},
  };
  return removal_map;
}

void RemoveExpandedCharsForModifier(absl::string_view asis,
                                    absl::string_view base,
                                    std::set<std::string> *expanded) {
  // The following check is needed for mitigating crashes like the one which
  // occurred in b/277163340 in production.
  if (asis.size() < base.size()) {
    LOG(DFATAL) << "asis.size() is smaller than base.size().";
    return;
  }

  const absl::string_view trailing(asis.substr(base.size()));
  for (auto [iter, end] = GetModifierRemovalMap()->equal_range(trailing);
       iter != end; ++iter) {
    expanded->erase(std::string(iter->second));
  }
}

constexpr size_t kMaxPreeditLength = 256;

}  // namespace

Composer::Composer()
    : Composer(&Table::GetDefaultTable(),
               &commands::Request::default_instance(),
               &config::ConfigHandler::DefaultConfig()) {}

Composer::Composer(const Table *table, const commands::Request *request,
                   const config::Config *config)
    : position_(0),
      input_mode_(transliteration::HIRAGANA),
      output_mode_(transliteration::HIRAGANA),
      comeback_input_mode_(transliteration::HIRAGANA),
      input_field_type_(commands::Context::NORMAL),
      shifted_sequence_count_(0),
      composition_(table),
      typing_corrector_(
          request, table,
          absl::GetFlag(FLAGS_max_typing_correction_query_candidates),
          absl::GetFlag(FLAGS_max_typing_correction_query_results)),
      max_length_(kMaxPreeditLength),
      request_(request),
      config_(config),
      table_(table),
      is_new_input_(true) {
  SetInputMode(transliteration::HIRAGANA);
  if (config_ == nullptr) {
    config_ = &config::ConfigHandler::DefaultConfig();
  }
  typing_corrector_.SetConfig(config_);
  Reset();
}

Composer::Composer(const Composer &) = default;
Composer &Composer::operator=(const Composer &) = default;

Composer::~Composer() = default;

void Composer::Reset() {
  EditErase();
  ResetInputMode();
  SetOutputMode(transliteration::HIRAGANA);
  source_text_.assign("");
  typing_corrector_.Reset();
  timeout_threshold_msec_ = config_->composing_timeout_threshold_msec();
}

void Composer::ResetInputMode() { SetInputMode(comeback_input_mode_); }

void Composer::ReloadConfig() {
  // Do nothing at this moment.
}

bool Composer::Empty() const { return (GetLength() == 0); }

void Composer::SetTable(const Table *table) {
  table_ = table;
  composition_.SetTable(table);
  typing_corrector_.SetTable(table);
}

void Composer::SetRequest(const commands::Request *request) {
  typing_corrector_.SetRequest(request);
  request_ = request;
}

void Composer::SetConfig(const config::Config *config) {
  config_ = config;
  typing_corrector_.SetConfig(config);
}

void Composer::SetInputMode(transliteration::TransliterationType mode) {
  comeback_input_mode_ = mode;
  input_mode_ = mode;
  shifted_sequence_count_ = 0;
  is_new_input_ = true;
  composition_.SetInputMode(GetTransliterator(mode));
}

void Composer::SetTemporaryInputMode(
    transliteration::TransliterationType mode) {
  // Set comeback_input_mode_ to revert back the current input mode.
  comeback_input_mode_ = input_mode_;
  input_mode_ = mode;
  shifted_sequence_count_ = 0;
  is_new_input_ = true;
  composition_.SetInputMode(GetTransliterator(mode));
}

void Composer::UpdateInputMode() {
  if (position_ != 0 && request_->update_input_mode_from_surrounding_text()) {
    const Transliterators::Transliterator current_t12r =
        composition_.GetTransliterator(position_);
    if (position_ == composition_.GetLength() ||
        current_t12r == composition_.GetTransliterator(position_ + 1)) {
      // - The cursor is at the tail of composition.
      //   Use last character's transliterator as the input mode.
      // - If the current cursor is between the same character type like
      //   "A|B" and "あ|い", the input mode follows the character type.
      input_mode_ = GetTransliterationType(current_t12r, comeback_input_mode_);
      shifted_sequence_count_ = 0;
      is_new_input_ = true;
      composition_.SetInputMode(GetTransliterator(input_mode_));
      return;
    }
  }

  // Set the default input mode.
  SetInputMode(comeback_input_mode_);
}

transliteration::TransliterationType Composer::GetInputMode() const {
  return input_mode_;
}

transliteration::TransliterationType Composer::GetComebackInputMode() const {
  return comeback_input_mode_;
}

void Composer::ToggleInputMode() {
  if (input_mode_ == transliteration::HIRAGANA) {
    // TODO(komatsu): Refer user's perference.
    SetInputMode(transliteration::HALF_ASCII);
  } else {
    SetInputMode(transliteration::HIRAGANA);
  }
}

transliteration::TransliterationType Composer::GetOutputMode() const {
  return output_mode_;
}

void Composer::SetOutputMode(transliteration::TransliterationType mode) {
  output_mode_ = mode;
  composition_.SetTransliterator(0, composition_.GetLength(),
                                 GetTransliterator(mode));
  position_ = composition_.GetLength();
}

void Composer::ApplyTemporaryInputMode(const absl::string_view input,
                                       bool caps_locked) {
  DCHECK(!input.empty());

  const config::Config::ShiftKeyModeSwitch switch_mode =
      config_->shift_key_mode_switch();

  // When input is not an ASCII code, reset the input mode to the one before
  // temporary input mode.
  if (OneCharLen(input.front()) != 1) {
    // Call SetInputMode() only when the current input mode is temporary, which
    // is detected by the if-condition below.  Without this check,
    // SetInputMode() is called always for multi-byte charactesrs.  This causes
    // a bug that multi-byte characters is inserted to a new chunk because
    // |is_new_input_| is set to true in SetInputMode(); see b/31444698.
    if (comeback_input_mode_ != input_mode_) {
      SetInputMode(comeback_input_mode_);
    }
    return;
  }

  // Input is an ASCII code.
  // we use first character to determine temporary input mode.
  const char key = input[0];
  const bool alpha_with_shift = (!caps_locked && ('A' <= key && key <= 'Z')) ||
                                (caps_locked && ('a' <= key && key <= 'z'));
  const bool alpha_without_shift =
      (caps_locked && ('A' <= key && key <= 'Z')) ||
      (!caps_locked && ('a' <= key && key <= 'z'));

  if (alpha_with_shift) {
    if (switch_mode == config::Config::ASCII_INPUT_MODE) {
      if (input_mode_ == transliteration::HALF_ASCII ||
          input_mode_ == transliteration::FULL_ASCII) {
        // Do nothing.
      } else {
        SetTemporaryInputMode(transliteration::HALF_ASCII);
      }
    } else if (switch_mode == config::Config::KATAKANA_INPUT_MODE) {
      if (input_mode_ == transliteration::HIRAGANA) {
        SetTemporaryInputMode(transliteration::FULL_KATAKANA);
      } else {
        // Do nothing.
      }
    }
    ++shifted_sequence_count_;
  } else if (alpha_without_shift) {
    // When shifted input continues, the next lower input is the end
    // of temporary half-width Ascii input.
    if (shifted_sequence_count_ > 1 &&
        switch_mode == config::Config::ASCII_INPUT_MODE) {
      SetInputMode(comeback_input_mode_);
    }
    if (switch_mode == config::Config::KATAKANA_INPUT_MODE) {
      SetInputMode(comeback_input_mode_);
    }
    shifted_sequence_count_ = 0;
  } else {
    // If the key is not an alphabet, reset shifted_sequence_count_
    // because "Continuous shifted input" feature should be reset
    // when the input meets non-alphabet character.
    shifted_sequence_count_ = 0;
  }
}

bool Composer::ProcessCompositionInput(const CompositionInput &input) {
  if (!EnableInsert()) {
    return false;
  }

  position_ = composition_.InsertInput(position_, input);
  is_new_input_ = false;
  typing_corrector_.InsertCharacter(input);
  return true;
}

void Composer::InsertCharacter(const absl::string_view key) {
  CompositionInput input;
  input.InitFromRaw(key, is_new_input_);
  ProcessCompositionInput(input);
}

void Composer::InsertCommandCharacter(const InternalCommand internal_command) {
  CompositionInput input;
  switch (internal_command) {
    case REWIND:
      input.InitFromRaw(table_->ParseSpecialKey("{<}"), is_new_input_);
      ProcessCompositionInput(input);
      break;
    case STOP_KEY_TOGGLING:
      input.InitFromRaw(table_->ParseSpecialKey("{!}"), is_new_input_);
      ProcessCompositionInput(input);
      break;
    default:
      LOG(ERROR) << "Unknown command : " << internal_command;
  }
}

void Composer::InsertCharacterPreedit(const absl::string_view input) {
  size_t begin = 0;
  const size_t end = input.size();
  while (begin < end) {
    const size_t mblen = OneCharLen(input[begin]);
    const absl::string_view character(input.substr(begin, mblen));
    if (!InsertCharacterKeyAndPreedit(character, character)) {
      return;
    }
    begin += mblen;
  }
  DCHECK_EQ(begin, end);
}

// Note: This method is only for test.
void Composer::SetPreeditTextForTestOnly(const absl::string_view input) {
  composition_.SetInputMode(Transliterators::RAW_STRING);
  size_t begin = 0;
  const size_t end = input.size();
  while (begin < end) {
    const size_t mblen = OneCharLen(input[begin]);
    CompositionInput composition_input;
    composition_input.set_raw(input.substr(begin, mblen));
    composition_input.set_is_new_input(is_new_input_);
    position_ = composition_.InsertInput(position_, composition_input);
    is_new_input_ = false;
    begin += mblen;
  }
  DCHECK_EQ(begin, end);

  std::string lower_input(input);
  Util::LowerString(&lower_input);
  if (Util::IsLowerAscii(lower_input)) {
    // Fake input mode.
    // This is useful to test the behavior of alphabet keyboard.
    SetTemporaryInputMode(transliteration::HALF_ASCII);
  }
}

bool Composer::InsertCharacterKeyAndPreedit(const absl::string_view key,
                                            const absl::string_view preedit) {
  CompositionInput input;
  input.InitFromRawAndConv(key, preedit, is_new_input_);
  return ProcessCompositionInput(input);
}

bool Composer::InsertCharacterKeyEvent(const commands::KeyEvent &key) {
  if (!EnableInsert()) {
    return false;
  }

  // Check timeout.
  // If the duration from the previous input is over than the threshold,
  // A STOP_KEY_TOGGLING command is sent before the key input.
  if (timeout_threshold_msec_ > 0) {
    int64_t current_msec = key.has_timestamp_msec()
                               ? key.timestamp_msec()
                               : absl::ToUnixMillis(Clock::GetAbslTime());
    if (timestamp_msec_ > 0 &&
        current_msec - timestamp_msec_ >= timeout_threshold_msec_) {
      InsertCommandCharacter(STOP_KEY_TOGGLING);
    }
    timestamp_msec_ = current_msec;
  }

  if (key.has_mode()) {
    const transliteration::TransliterationType new_input_mode =
        GetTransliterationTypeFromCompositionMode(key.mode());
    if (new_input_mode != input_mode_) {
      // Only when the new input mode is different from the current
      // input mode, SetInputMode is called.  Otherwise the value of
      // comeback_input_mode_ is lost.
      SetInputMode(new_input_mode);
    }
  }

  // If only SHIFT is pressed, this is used to revert back to the
  // previous input mode.
  if (!key.has_key_code()) {
    for (size_t i = 0; key.modifier_keys_size(); ++i) {
      if (key.modifier_keys(i) == commands::KeyEvent::SHIFT) {
        // TODO(komatsu): Enable to customize the behavior.
        SetInputMode(comeback_input_mode_);
        return true;
      }
    }
  }

  CompositionInput input;
  if (!input.Init(*table_, key, is_new_input_)) {
    return false;
  }

  if (key.has_key_string()) {
    if (key.input_style() == commands::KeyEvent::AS_IS ||
        key.input_style() == commands::KeyEvent::DIRECT_INPUT) {
      composition_.SetInputMode(Transliterators::CONVERSION_STRING);
      // Disable typing correction mainly for Android. b/258369101
      typing_corrector_.Invalidate();
      ProcessCompositionInput(input);
      SetInputMode(comeback_input_mode_);
    } else {
      // Kana input usually has key_string.  Note that, the existence of
      // key_string never determine if the input mode is Kana or Romaji.
      ProcessCompositionInput(input);
    }
  } else {
    // Romaji input usually does not has key_string.  Note that, the
    // existence of key_string never determines if the input mode is
    // Kana or Romaji.
    const uint32_t modifiers = KeyEventUtil::GetModifiers(key);
    ApplyTemporaryInputMode(input.raw(), KeyEventUtil::HasCaps(modifiers));
    ProcessCompositionInput(input);
  }

  if (comeback_input_mode_ == input_mode_) {
    AutoSwitchMode();
  }
  return true;
}

void Composer::DeleteAt(size_t pos) {
  composition_.DeleteAt(pos);
  // Adjust cursor position for composition mode.
  if (position_ > pos) {
    position_--;
  }
  // We do not call UpdateInputMode() here.
  // 1. In composition mode, UpdateInputMode finalizes pending chunk.
  // 2. In conversion mode, InputMode needs not to change.
  typing_corrector_.Invalidate();
}

void Composer::Delete() {
  position_ = composition_.DeleteAt(position_);
  UpdateInputMode();

  typing_corrector_.Invalidate();
}

void Composer::DeleteRange(size_t pos, size_t length) {
  for (int i = 0; i < length && pos < composition_.GetLength(); ++i) {
    DeleteAt(pos);
  }
  typing_corrector_.Invalidate();
}

void Composer::EditErase() {
  composition_.Erase();
  position_ = 0;
  SetInputMode(comeback_input_mode_);
  typing_corrector_.Reset();
}

void Composer::Backspace() {
  if (position_ == 0) {
    return;
  }

  // In the view point of updating input mode,
  // backspace is special case because new input mode is based on both
  // new current character and *character to be deleted*.

  // At first, move to left.
  // Now the cursor is between 'new current character'
  // and 'character to be deleted'.
  --position_;

  // Update input mode based on both 'new current character' and
  // 'character to be deleted'.
  UpdateInputMode();

  // Delete 'character to be deleted'
  position_ = composition_.DeleteAt(position_);

  typing_corrector_.Invalidate();
}

void Composer::MoveCursorLeft() {
  if (position_ > 0) {
    --position_;
  }
  UpdateInputMode();

  typing_corrector_.Invalidate();
}

void Composer::MoveCursorRight() {
  if (position_ < composition_.GetLength()) {
    ++position_;
  }
  UpdateInputMode();

  typing_corrector_.Invalidate();
}

void Composer::MoveCursorToBeginning() {
  position_ = 0;
  SetInputMode(comeback_input_mode_);

  typing_corrector_.Invalidate();
}

void Composer::MoveCursorToEnd() {
  position_ = composition_.GetLength();
  // Behavior between MoveCursorToEnd and MoveCursorToRight is different.
  // MoveCursorToEnd always makes current input mode default.
  SetInputMode(comeback_input_mode_);

  typing_corrector_.Invalidate();
}

void Composer::MoveCursorTo(uint32_t new_position) {
  if (new_position <= composition_.GetLength()) {
    position_ = new_position;
    UpdateInputMode();
  }
  typing_corrector_.Invalidate();
}

void Composer::GetPreedit(std::string *left, std::string *focused,
                          std::string *right) const {
  DCHECK(left);
  DCHECK(focused);
  DCHECK(right);
  composition_.GetPreedit(position_, left, focused, right);

  // TODO(komatsu): This function can be obsolete.
  std::string preedit = *left + *focused + *right;
  if (TransformCharactersForNumbers(&preedit)) {
    const size_t left_size = Util::CharsLen(*left);
    const size_t focused_size = Util::CharsLen(*focused);
    Util::Utf8SubString(preedit, 0, left_size, left);
    Util::Utf8SubString(preedit, left_size, focused_size, focused);
    Util::Utf8SubString(preedit, left_size + focused_size, std::string::npos,
                        right);
  }
}

void Composer::GetStringForPreedit(std::string *output) const {
  composition_.GetString(output);
  TransformCharactersForNumbers(output);
  // If the input field type needs half ascii characters,
  // perform conversion here.
  // Note that this purpose is also achieved by the client by setting
  // input type as "half ascii".
  // But the architecture of Mozc expects the server to handle such character
  // width management.
  // In addition, we also think about PASSWORD field type.
  // we can prepare NUMBER and TEL keyboard layout, which has
  // "half ascii" composition mode. This works.
  // But we will not have PASSWORD only keyboard. We will share the basic
  // keyboard on usual and password mode
  // so such hacky code cannot be applicable.
  // TODO(matsuzakit): Move this logic to another appopriate location.
  // SetOutputMode() is not currently applicable but ideally it is
  // better location than here.
  const commands::Context::InputFieldType field_type = GetInputFieldType();
  if (field_type == commands::Context::NUMBER ||
      field_type == commands::Context::PASSWORD ||
      field_type == commands::Context::TEL) {
    const std::string tmp = *output;
    japanese_util::FullWidthAsciiToHalfWidthAscii(tmp, output);
  }
}

void Composer::GetStringForSubmission(std::string *output) const {
  // TODO(komatsu): We should make sure if we can integrate this
  // function to GetStringForPreedit after a while.
  GetStringForPreedit(output);
}

void Composer::GetQueryForConversion(std::string *output) const {
  std::string base_output;
  composition_.GetStringWithTrimMode(FIX, &base_output);
  TransformCharactersForNumbers(&base_output);
  japanese_util::FullWidthAsciiToHalfWidthAscii(base_output, output);
}

namespace {
// Determine which query is suitable for a prediction query and return
// its pointer.
// Example:
// = Romanji Input =
// ("もz", "も") -> "も"  // a part of romanji should be trimed.
// ("もzky", "もz") -> "もzky"  // a user might intentionally typed them.
// ("z", "") -> "z"      // ditto.
// = Kana Input =
// ("か", "") -> "か"  // a part of kana (it can be "が") should not be trimed.
std::string *GetBaseQueryForPrediction(std::string *asis_query,
                                       std::string *trimed_query) {
  // If the sizes are equal, there is no matter.
  if (asis_query->size() == trimed_query->size()) {
    return asis_query;
  }

  // Get the different part between asis_query and trimed_query.  For
  // example, "ky" is the different part where asis_query is "もzky"
  // and trimed_query is "もz".
  DCHECK_GT(asis_query->size(), trimed_query->size());
  const std::string asis_tail(*asis_query, trimed_query->size());
  DCHECK(!asis_tail.empty());

  // If the different part is not an alphabet, asis_query is used.
  // This check is mainly used for Kana Input.
  const Util::ScriptType asis_tail_type = Util::GetScriptType(asis_tail);
  if (asis_tail_type != Util::ALPHABET) {
    return asis_query;
  }

  // If the trimed_query is empty and asis_query is alphabet, an asis
  // string is used because the query may be typed intentionally.
  if (trimed_query->empty()) {  // alphabet???
    const Util::ScriptType asis_type = Util::GetScriptType(*asis_query);
    if (asis_type == Util::ALPHABET) {
      return asis_query;
    } else {
      return trimed_query;
    }
  }

  // Now there are two patterns: ("もzk", "もz") and ("もずk", "もず").
  // We assume "もzk" is user's intentional query, but "もずk" is not.
  // So our results are:
  // ("もzk", "もz") => "もzk" and ("もずk", "もず") => "もず".
  const absl::string_view trimed_tail = Util::Utf8SubString(
      *trimed_query, Util::CharsLen(*trimed_query) - 1, std::string::npos);
  DCHECK(!trimed_tail.empty());
  const Util::ScriptType trimed_tail_type = Util::GetScriptType(trimed_tail);
  if (trimed_tail_type == Util::ALPHABET) {
    return asis_query;
  } else {
    return trimed_query;
  }
}
}  // namespace

void Composer::GetQueryForPrediction(std::string *output) const {
  std::string asis_query;
  composition_.GetStringWithTrimMode(ASIS, &asis_query);

  switch (input_mode_) {
    case transliteration::HALF_ASCII: {
      output->assign(asis_query);
      return;
    }
    case transliteration::FULL_ASCII: {
      japanese_util::FullWidthAsciiToHalfWidthAscii(asis_query, output);
      return;
    }
    default: {
    }
  }

  std::string trimed_query;
  composition_.GetStringWithTrimMode(TRIM, &trimed_query);

  // NOTE(komatsu): This is a hack to go around the difference
  // expectation between Romanji-Input and Kana-Input.  "かn" in
  // Romaji-Input should be "か" while "あか" in Kana-Input should be
  // "あか", although "かn" and "あか" have the same properties.  An
  // ideal solution is to expand the ambguity and pass all of them to
  // the converter. (e.g. "かn" -> ["かな",..."かの", "かん", ...] /
  // "あか" -> ["あか", "あが"])
  std::string *base_query =
      GetBaseQueryForPrediction(&asis_query, &trimed_query);
  TransformCharactersForNumbers(base_query);
  japanese_util::FullWidthAsciiToHalfWidthAscii(*base_query, output);
}

void Composer::GetQueriesForPrediction(std::string *base,
                                       std::set<std::string> *expanded) const {
  DCHECK(base);
  DCHECK(expanded);
  // In case of the Latin input modes, we don't perform expansion.
  switch (input_mode_) {
    case transliteration::HALF_ASCII:
    case transliteration::FULL_ASCII: {
      GetQueryForPrediction(base);
      expanded->clear();
      return;
    }
    default: {
    }
  }
  std::string base_query;
  composition_.GetExpandedStrings(&base_query, expanded);
  // The above `GetExpandedStrings` generates expansion for modifier key as
  // well, e.g., if the composition is "ざ", `expanded` contains "さ" too.
  // However, "ざ" is usually composed by explicitly hitting the modifier key.
  // So we don't want to generate prediction from "さ" in this case. The
  // following code removes such unnecessary expansion.
  std::string asis;
  composition_.GetStringWithTrimMode(ASIS, &asis);
  RemoveExpandedCharsForModifier(asis, base_query, expanded);

  japanese_util::FullWidthAsciiToHalfWidthAscii(base_query, base);
}

void Composer::GetTypeCorrectedQueriesForPrediction(
    std::vector<TypeCorrectedQuery> *queries) const {
  typing_corrector_.GetQueriesForPrediction(queries);
  for (auto &query : *queries) {
    RemoveExpandedCharsForModifier(query.asis, query.base, &query.expanded);
  }
}

size_t Composer::GetLength() const { return composition_.GetLength(); }

size_t Composer::GetCursor() const { return position_; }

void Composer::GetTransliteratedText(Transliterators::Transliterator t12r,
                                     const size_t position, const size_t size,
                                     std::string *result) const {
  DCHECK(result);
  std::string full_base;
  composition_.GetStringWithTransliterator(t12r, &full_base);

  const size_t t13n_start =
      composition_.ConvertPosition(position, Transliterators::LOCAL, t12r);
  const size_t t13n_end = composition_.ConvertPosition(
      position + size, Transliterators::LOCAL, t12r);
  const size_t t13n_size = t13n_end - t13n_start;

  Util::Utf8SubString(full_base, t13n_start, t13n_size, result);
}

void Composer::GetRawString(std::string *raw_string) const {
  GetRawSubString(0, GetLength(), raw_string);
}

void Composer::GetRawSubString(const size_t position, const size_t size,
                               std::string *raw_sub_string) const {
  DCHECK(raw_sub_string);
  GetTransliteratedText(Transliterators::RAW_STRING, position, size,
                        raw_sub_string);
}

void Composer::GetTransliterations(
    transliteration::Transliterations *t13ns) const {
  GetSubTransliterations(0, GetLength(), t13ns);
}

void Composer::GetSubTransliteration(
    const transliteration::TransliterationType type, const size_t position,
    const size_t size, std::string *transliteration) const {
  const Transliterators::Transliterator t12r = GetTransliterator(type);
  std::string result;
  GetTransliteratedText(t12r, position, size, &result);
  transliteration->clear();
  Transliterate(type, result, transliteration);
}

void Composer::GetSubTransliterations(
    const size_t position, const size_t size,
    transliteration::Transliterations *transliterations) const {
  std::string t13n;
  for (size_t i = 0; i < transliteration::NUM_T13N_TYPES; ++i) {
    const transliteration::TransliterationType t13n_type =
        transliteration::TransliterationTypeArray[i];
    GetSubTransliteration(t13n_type, position, size, &t13n);
    transliterations->push_back(t13n);
  }
}

bool Composer::EnableInsert() const {
  if (GetLength() >= max_length_) {
    // do not accept long chars to prevent DOS attack.
    LOG(WARNING) << "The length is too long.";
    return false;
  }
  return true;
}

void Composer::AutoSwitchMode() {
  if (!config_->use_auto_ime_turn_off()) {
    return;
  }

  // AutoSwitchMode is only available on Roma input
  if (config_->preedit_method() != config::Config::ROMAN) {
    return;
  }

  std::string key;
  // Key should be in half-width alphanumeric.
  composition_.GetStringWithTransliterator(
      GetTransliterator(transliteration::HALF_ASCII), &key);

  const ModeSwitchingHandler::Rule mode_switching =
      ModeSwitchingHandler::GetModeSwitchingHandler()->GetModeSwitchingRule(
          key);

  // |display_mode| affects the existing composition the user typed.
  switch (mode_switching.display_mode) {
    case ModeSwitchingHandler::NO_CHANGE:
      // Do nothing.
      break;
    case ModeSwitchingHandler::REVERT_TO_PREVIOUS_MODE:
      // Invalid value for display_mode
      LOG(ERROR) << "REVERT_TO_PREVIOUS_MODE is an invalid value "
                 << "for display_mode.";
      break;
    case ModeSwitchingHandler::PREFERRED_ALPHANUMERIC:
      if (input_mode_ == transliteration::FULL_ASCII) {
        SetOutputMode(transliteration::FULL_ASCII);
      } else {
        SetOutputMode(transliteration::HALF_ASCII);
      }
      break;
    case ModeSwitchingHandler::HALF_ALPHANUMERIC:
      SetOutputMode(transliteration::HALF_ASCII);
      break;
    case ModeSwitchingHandler::FULL_ALPHANUMERIC:
      SetOutputMode(transliteration::FULL_ASCII);
      break;
    default:
      LOG(ERROR) << "Unknown value: " << mode_switching.display_mode;
      break;
  }

  // |input_mode| affects the current input mode used for the user's
  // new typing.
  switch (mode_switching.input_mode) {
    case ModeSwitchingHandler::NO_CHANGE:
      // Do nothing.
      break;
    case ModeSwitchingHandler::REVERT_TO_PREVIOUS_MODE:
      SetInputMode(comeback_input_mode_);
      break;
    case ModeSwitchingHandler::PREFERRED_ALPHANUMERIC:
      if (input_mode_ != transliteration::HALF_ASCII &&
          input_mode_ != transliteration::FULL_ASCII) {
        SetTemporaryInputMode(transliteration::HALF_ASCII);
      }
      break;
    case ModeSwitchingHandler::HALF_ALPHANUMERIC:
      if (input_mode_ != transliteration::HALF_ASCII) {
        SetTemporaryInputMode(transliteration::HALF_ASCII);
      }
      break;
    case ModeSwitchingHandler::FULL_ALPHANUMERIC:
      if (input_mode_ != transliteration::FULL_ASCII) {
        SetTemporaryInputMode(transliteration::FULL_ASCII);
      }
      break;
    default:
      LOG(ERROR) << "Unknown value: " << mode_switching.input_mode;
      break;
  }
}

bool Composer::ShouldCommit() const { return composition_.ShouldCommit(); }

bool Composer::ShouldCommitHead(size_t *length_to_commit) const {
  size_t max_remaining_composition_length;
  switch (GetInputFieldType()) {
    case commands::Context::PASSWORD:
      max_remaining_composition_length = 1;
      break;
    case commands::Context::TEL:
    case commands::Context::NUMBER:
      max_remaining_composition_length = 0;
      break;
    default:
      // No need to commit. Return here.
      return false;
  }
  if (GetLength() > max_remaining_composition_length) {
    *length_to_commit = GetLength() - max_remaining_composition_length;
    return true;
  }
  return false;
}

namespace {
enum Script {
  ALPHABET,   // alphabet characters or symbols
  NUMBER,     // 0 - 9, "０" - "９"
  JA_HYPHEN,  // "ー"
  JA_COMMA,   // "、"
  JA_PERIOD,  // "。"
  OTHER,
};

bool IsAlphabetOrNumber(const Script script) {
  return (script == ALPHABET) || (script == NUMBER);
}
}  // namespace

// static
bool Composer::TransformCharactersForNumbers(std::string *query) {
  if (query == nullptr) {
    LOG(ERROR) << "query is nullptr";
    return false;
  }

  // Create a vector of scripts of query characters to avoid
  // processing query string many times.
  const size_t chars_len = Util::CharsLen(*query);
  std::vector<Script> char_scripts;
  char_scripts.reserve(chars_len);

  // flags to determine whether continue to the next step.
  bool has_symbols = false;
  bool has_alphanumerics = false;
  for (ConstChar32Iterator iter(*query); !iter.Done(); iter.Next()) {
    const char32_t one_char = iter.Get();
    switch (one_char) {
      case 0x30FC:  // "ー"
        has_symbols = true;
        char_scripts.push_back(JA_HYPHEN);
        break;
      case 0x3001:  // "、"
        has_symbols = true;
        char_scripts.push_back(JA_COMMA);
        break;
      case 0x3002:  // "。"
        has_symbols = true;
        char_scripts.push_back(JA_PERIOD);
        break;
      case '+':
      case '*':
      case '/':
      case '=':
      case '(':
      case ')':
      case '<':
      case '>':
      case 0xFF0B:  // "＋"
      case 0xFF0A:  // "＊"
      case 0xFF0F:  // "／"
      case 0xFF1D:  // "＝"
      case 0xFF08:  // "（"
      case 0xFF09:  // "）"
      case 0xFF1C:  // "＜"
      case 0xFF1E:  // "＞"
        char_scripts.push_back(ALPHABET);
        break;
      default: {
        Util::ScriptType script_type = Util::GetScriptType(one_char);
        if (script_type == Util::NUMBER) {
          has_alphanumerics = true;
          char_scripts.push_back(NUMBER);
        } else if (script_type == Util::ALPHABET) {
          has_alphanumerics = true;
          char_scripts.push_back(ALPHABET);
        } else {
          char_scripts.push_back(OTHER);
        }
      }
    }
  }

  DCHECK_EQ(chars_len, char_scripts.size());
  if (!has_alphanumerics || !has_symbols) {
    VLOG(1) << "The query contains neither alphanumeric nor symbol.";
    return false;
  }

  std::string transformed_query;
  bool transformed = false;
  size_t i = 0;
  std::string append_char;
  for (ConstChar32Iterator iter(*query); !iter.Done(); iter.Next(), ++i) {
    append_char.clear();
    switch (char_scripts[i]) {
      case JA_HYPHEN: {
        // JA_HYPHEN(s) "ー" is/are transformed to "−" if:
        // (i) query has one and only one leading JA_HYPHEN followed by a
        //     number,
        // (ii) JA_HYPHEN(s) follow(s) after an alphanumeric (ex. 0-, 0----,
        //     etc).
        // Note that rule (i) implies that if query starts with more than
        // one JA_HYPHENs, those JA_HYPHENs are not transformed.
        bool check = false;
        if (i == 0 && chars_len > 1) {
          check = (char_scripts[1] == NUMBER);
        } else {
          for (size_t j = i; j > 0; --j) {
            if (char_scripts[j - 1] == JA_HYPHEN) {
              continue;
            }
            check = IsAlphabetOrNumber(char_scripts[j - 1]);
            break;
          }
        }

        // JA_HYPHEN should be transformed to MINUS.
        if (check) {
          CharacterFormManager::GetCharacterFormManager()->ConvertPreeditString(
              "−",  // U+2212
              &append_char);
          DCHECK(!append_char.empty());
        }
        break;
      }

      case JA_COMMA: {
        // "、" should be "，" if the previous character is alphanumerics.
        // character are both alphanumerics.
        // Previous char should exist and be a number.
        const bool lhs_check =
            (i > 0 && IsAlphabetOrNumber(char_scripts[i - 1]));
        // JA_COMMA should be transformed to COMMA.
        if (lhs_check) {
          CharacterFormManager::GetCharacterFormManager()->ConvertPreeditString(
              "，", &append_char);
          DCHECK(!append_char.empty());
        }
        break;
      }

      case JA_PERIOD: {
        // "。" should be "．" if the previous character and the next
        // character are both alphanumerics.
        // Previous char should exist and be a number.
        const bool lhs_check =
            (i > 0 && IsAlphabetOrNumber(char_scripts[i - 1]));
        // JA_PRERIOD should be transformed to PRERIOD.
        if (lhs_check) {
          CharacterFormManager::GetCharacterFormManager()->ConvertPreeditString(
              "．", &append_char);
          DCHECK(!append_char.empty());
        }
        break;
      }

      default: {
        // Do nothing.
      }
    }

    if (append_char.empty()) {
      // Append one character.
      Util::Ucs4ToUtf8Append(iter.Get(), &transformed_query);
    } else {
      // Append the transformed character.
      transformed_query.append(append_char);
      transformed = true;
    }
  }
  if (!transformed) {
    return false;
  }

  // It is possible that the query's size in byte differs from the
  // orig_query's size in byte.
  DCHECK_EQ(Util::CharsLen(*query), Util::CharsLen(transformed_query));
  *query = transformed_query;
  return true;
}

void Composer::SetNewInput() { is_new_input_ = true; }

bool Composer::IsToggleable() const {
  // When |is_new_input_| is true, a new chunk is always created and, hence, key
  // toggling never happens regardless of the composition state.
  return !is_new_input_ && composition_.IsToggleable(position_);
}

bool Composer::is_new_input() const { return is_new_input_; }

size_t Composer::shifted_sequence_count() const {
  return shifted_sequence_count_;
}

const std::string &Composer::source_text() const { return source_text_; }
std::string *Composer::mutable_source_text() { return &source_text_; }
void Composer::set_source_text(const absl::string_view source_text) {
  source_text_ = std::string(source_text);
}

size_t Composer::max_length() const { return max_length_; }
void Composer::set_max_length(size_t length) { max_length_ = length; }

int Composer::timeout_threshold_msec() const { return timeout_threshold_msec_; }

void Composer::set_timeout_threshold_msec(int threshold_msec) {
  timeout_threshold_msec_ = threshold_msec;
}

void Composer::SetInputFieldType(commands::Context::InputFieldType type) {
  input_field_type_ = type;
}

commands::Context::InputFieldType Composer::GetInputFieldType() const {
  return input_field_type_;
}
}  // namespace composer
}  // namespace mozc
