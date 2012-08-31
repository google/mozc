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

// Interactive composer from a Roman string to a Hiragana string

#include "composer/composer.h"

#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "composer/internal/composition.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/mode_switching_handler.h"
#include "composer/internal/transliterators_ja.h"
#include "composer/table.h"
#include "config/character_form_manager.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "session/key_event_util.h"
#include "session/request_handler.h"

namespace mozc {
namespace composer {

using ::mozc::config::CharacterFormManager;

namespace {

const TransliteratorInterface *GetTransliterator(
    const transliteration::TransliterationType comp_mode) {
  const TransliteratorInterface *kNullTransliterator = NULL;
  switch (comp_mode) {
    case transliteration::HALF_ASCII:
    case transliteration::HALF_ASCII_UPPER:
    case transliteration::HALF_ASCII_LOWER:
    case transliteration::HALF_ASCII_CAPITALIZED:
      VLOG(2) << "GetTransliterator: GetHalfAsciiTransliterator";
      return TransliteratorsJa::GetHalfAsciiTransliterator();

    case transliteration::FULL_ASCII:
    case transliteration::FULL_ASCII_UPPER:
    case transliteration::FULL_ASCII_LOWER:
    case transliteration::FULL_ASCII_CAPITALIZED:
      VLOG(2) << "GetTransliterator: GetFullAsciiTransliterator";
      return TransliteratorsJa::GetFullAsciiTransliterator();

    case transliteration::HALF_KATAKANA:
      VLOG(2) << "GetTransliterator: GetHalfKatakanaTransliterator";
      return TransliteratorsJa::GetHalfKatakanaTransliterator();

    case transliteration::FULL_KATAKANA:
      VLOG(2) << "GetTransliterator: GetFullKatakanaTransliterator";
      return TransliteratorsJa::GetFullKatakanaTransliterator();

    case transliteration::HIRAGANA:
      VLOG(2) << "GetTransliterator: kNullTransliterator";
      return TransliteratorsJa::GetHiraganaTransliterator();

    default:
      VLOG(2) << "GetTransliterator: kNullTransliterator";
      LOG(ERROR) << "Unknown TransliterationType: " << comp_mode;
      return kNullTransliterator;
  }
  VLOG(2) << "GetTransliterator: kNullTransliterator ";
  return kNullTransliterator;  // Just in case
}

transliteration::TransliterationType GetTransliterationType(
    const TransliteratorInterface *transliterator,
    const transliteration::TransliterationType default_type) {
  if (transliterator == TransliteratorsJa::GetHiraganaTransliterator()) {
    return transliteration::HIRAGANA;
  }
  if (transliterator == TransliteratorsJa::GetHalfAsciiTransliterator()) {
    return transliteration::HALF_ASCII;
  }
  if (transliterator == TransliteratorsJa::GetFullAsciiTransliterator()) {
    return transliteration::FULL_ASCII;
  }
  if (transliterator == TransliteratorsJa::GetFullKatakanaTransliterator()) {
    return transliteration::FULL_KATAKANA;
  }
  if (transliterator == TransliteratorsJa::GetHalfKatakanaTransliterator()) {
    return transliteration::HALF_KATAKANA;
  }
  return default_type;
}

void Transliterate(const transliteration::TransliterationType mode,
                   const string &input,
                   string *output) {
  // When the mode is HALF_KATAKANA, Full width ASCII is also
  // transformed.
  if (mode == transliteration::HALF_KATAKANA) {
    string tmp_input;
    Util::HiraganaToKatakana(input, &tmp_input);
    Util::FullWidthToHalfWidth(tmp_input, output);
    return;
  }

  switch (mode) {
    case transliteration::HALF_ASCII:
      Util::FullWidthAsciiToHalfWidthAscii(input, output);
      break;
    case transliteration::HALF_ASCII_UPPER:
      Util::FullWidthAsciiToHalfWidthAscii(input, output);
      Util::UpperString(output);
      break;
    case transliteration::HALF_ASCII_LOWER:
      Util::FullWidthAsciiToHalfWidthAscii(input, output);
      Util::LowerString(output);
      break;
    case transliteration::HALF_ASCII_CAPITALIZED:
      Util::FullWidthAsciiToHalfWidthAscii(input, output);
      Util::CapitalizeString(output);
      break;

    case transliteration::FULL_ASCII:
      Util::HalfWidthAsciiToFullWidthAscii(input, output);
      break;
    case transliteration::FULL_ASCII_UPPER:
      Util::HalfWidthAsciiToFullWidthAscii(input, output);
      Util::UpperString(output);
      break;
    case transliteration::FULL_ASCII_LOWER:
      Util::HalfWidthAsciiToFullWidthAscii(input, output);
      Util::LowerString(output);
      break;
    case transliteration::FULL_ASCII_CAPITALIZED:
      Util::HalfWidthAsciiToFullWidthAscii(input, output);
      Util::CapitalizeString(output);
      break;

    case transliteration::FULL_KATAKANA:
      Util::HiraganaToKatakana(input, output);
      break;
    case transliteration::HIRAGANA:
      *output = input;
      break;
    default:
      LOG(ERROR) << "Unknown TransliterationType: " << mode;
      *output = input;
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

}  // namespace

static const size_t kMaxPreeditLength = 256;
Composer::Composer(const Table *table, const commands::Request &request)
    : position_(0),
      is_new_input_(true),
      input_mode_(transliteration::HIRAGANA),
      output_mode_(transliteration::HIRAGANA),
      comeback_input_mode_(transliteration::HIRAGANA),
      input_field_type_(commands::SessionCommand::NORMAL),
      shifted_sequence_count_(0),
      composition_(new Composition(table)),
      max_length_(kMaxPreeditLength) {
  SetInputMode(transliteration::HIRAGANA);
  Reset();
  request_.CopyFrom(request);
}

Composer::~Composer() {}

void Composer::Reset() {
  EditErase();
  ResetInputMode();
  source_text_.assign("");
}

void Composer::ResetInputMode() {
  SetInputMode(comeback_input_mode_);
}

void Composer::ReloadConfig() {
  // Do nothing at this moment.
}

bool Composer::Empty() const {
  return (GetLength() == 0);
}

void Composer::SetTable(const Table *table) {
  composition_->SetTable(table);
}

void Composer::SetRequest(const commands::Request &request) {
  request_.CopyFrom(request);
}

void Composer::SetInputMode(transliteration::TransliterationType mode) {
  comeback_input_mode_ = mode;
  input_mode_ = mode;
  shifted_sequence_count_ = 0;
  is_new_input_ = true;
  composition_->SetInputMode(GetTransliterator(mode));
}

void Composer::SetTemporaryInputMode(
    transliteration::TransliterationType mode) {
  // Set comeback_input_mode_ to revert back the current input mode.
  comeback_input_mode_ = input_mode_;
  input_mode_ = mode;
  shifted_sequence_count_ = 0;
  is_new_input_ = true;
  composition_->SetInputMode(GetTransliterator(mode));
}

void Composer::UpdateInputMode() {
  if (position_ != 0 &&
      request_.update_input_mode_from_surrounding_text()) {
    const TransliteratorInterface *current_t12r =
        composition_->GetTransliterator(position_);
    if (position_ == composition_->GetLength() ||
        current_t12r == composition_->GetTransliterator(position_ + 1)) {
      // - The cursor is at the tail of composition.
      //   Use last character's transliterator as the input mode.
      // - If the current cursor is between the same character type like
      //   "A|B" and "あ|い", the input mode follows the character type.
      input_mode_ = GetTransliterationType(current_t12r, comeback_input_mode_);
      shifted_sequence_count_ = 0;
      is_new_input_ = true;
      composition_->SetInputMode(GetTransliterator(input_mode_));
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
  composition_->SetTransliterator(
      0, composition_->GetLength(), GetTransliterator(mode));
  position_ = composition_->GetLength();
}

void Composer::ApplyTemporaryInputMode(const string &input, bool caps_locked) {
  DCHECK(!input.empty());

  const config::Config::ShiftKeyModeSwitch switch_mode =
      GET_CONFIG(shift_key_mode_switch);

  // Input is NOT an ASCII code.
  if (Util::OneCharLen(input.c_str()) != 1) {
    SetInputMode(comeback_input_mode_);
    return;
  }

  // Input is an ASCII code.
  // we use first character to determin temporary input mode.
  const char key = input[0];
  const bool alpha_with_shift =
      (!caps_locked && ('A' <= key && key <= 'Z')) ||
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

void Composer::InsertCharacter(const string &key) {
  if (!EnableInsert()) {
    return;
  }
  CompositionInput input;
  input.set_raw(key);
  input.set_is_new_input(is_new_input_);
  position_ = composition_->InsertInput(position_, input);
  is_new_input_ = false;
}

void Composer::InsertCommandCharacter(const InternalCommand internal_command) {
  switch (internal_command) {
    case REWIND:
      InsertCharacter(Table::ParseSpecialKey("{<}"));
      break;
    default:
      LOG(ERROR) << "Unkown command : " << internal_command;
  }
}

void Composer::InsertCharacterPreedit(const string &input) {
  InsertCharacterKeyAndPreedit(input, input);
}

void Composer::InsertCharacterKeyAndPreedit(const string &key,
                                            const string &preedit) {
  if (!EnableInsert()) {
    return;
  }
  CompositionInput input;
  input.set_raw(key);
  input.set_conversion(preedit);
  input.set_is_new_input(is_new_input_);
  position_ = composition_->InsertInput(position_, input);
  is_new_input_ = false;
}

void Composer::InsertCharacterPreeditAt(size_t pos, const string &input) {
  InsertCharacterKeyAndPreeditAt(pos, input, input);
}

void Composer::InsertCharacterKeyAndPreeditAt(size_t pos,
                                              const string &key,
                                              const string &preedit) {
  if (!EnableInsert()) {
    return;
  }
  const size_t position_before_insertion = position_;
  const size_t length_before_insertion = composition_->GetLength();
  const size_t insertion_length = Util::CharsLen(preedit);

  composition_->SetInputMode(
      Transliterators::GetConversionStringSelector());

  CompositionInput input;
  input.set_raw(key);
  input.set_conversion(preedit);
  input.set_is_new_input(true);
  composition_->InsertInput(pos, input);

  DCHECK_EQ(insertion_length,
            composition_->GetLength() - length_before_insertion);

  composition_->SetInputMode(GetTransliterator(input_mode_));

  position_ = position_before_insertion;
  if (position_before_insertion >= pos) {
    position_ += insertion_length;
  }
  is_new_input_ = false;
}

bool Composer::InsertCharacterKeyEvent(const commands::KeyEvent &key) {
  if (!EnableInsert()) {
    return false;
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

  // Fill input representing user's raw input.
  string input;
  if (key.has_key_code()) {
    Util::UCS4ToUTF8(key.key_code(), &input);
  } else if (key.has_key_string()) {
    input = key.key_string();
  } else {
    LOG(WARNING) << "input is empty";
    return false;
  }

  if (key.has_key_string()) {
    if (key.input_style() == commands::KeyEvent::AS_IS ||
        key.input_style() == commands::KeyEvent::DIRECT_INPUT) {
      composition_->SetInputMode(
          Transliterators::GetConversionStringSelector());
      InsertCharacterKeyAndPreedit(input, key.key_string());
      SetInputMode(comeback_input_mode_);
    } else {
      // Kana input usually has key_string.  Note that, the existence of
      // key_string never determine if the input mode is Kana or Romaji.
      InsertCharacterKeyAndPreedit(input, key.key_string());
    }
  } else {
    // Romaji input usually does not has key_string.  Note that, the
    // existence of key_string never determines if the input mode is
    // Kana or Romaji.
    const uint32 modifiers = KeyEventUtil::GetModifiers(key);
    ApplyTemporaryInputMode(input, KeyEventUtil::HasCaps(modifiers));
    InsertCharacter(input);
  }

  if (comeback_input_mode_ == input_mode_) {
    AutoSwitchMode();
  }
  return true;
}

void Composer::DeleteAt(size_t pos) {
  composition_->DeleteAt(pos);
  // Adjust cursor position for composition mode.
  if (position_ > pos) {
    position_--;
  }
  // We do not call UpdateInputMode() here.
  // 1. In composition mode, UpdateInputMode finalizes pending chunk.
  // 2. In conversion mode, InputMode needs not to change.
}

void Composer::Delete() {
  position_ = composition_->DeleteAt(position_);
  UpdateInputMode();
}

void Composer::DeleteRange(size_t pos, size_t length) {
  for (int i = 0; i < length; ++i) {
    DeleteAt(pos);
  }
}

void Composer::EditErase() {
  composition_->Erase();
  position_ = 0;
  SetInputMode(comeback_input_mode_);
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
  position_ = composition_->DeleteAt(position_);
}

void Composer::MoveCursorLeft() {
  if (position_ > 0) {
    --position_;
  }
  UpdateInputMode();
}

void Composer::MoveCursorRight() {
  if (position_ < composition_->GetLength()) {
    ++position_;
  }
  UpdateInputMode();
}

void Composer::MoveCursorToBeginning() {
  position_ = 0;
  SetInputMode(comeback_input_mode_);
}

void Composer::MoveCursorToEnd() {
  position_ = composition_->GetLength();
  // Behavior between MoveCursorToEnd and MoveCursorToRight is different.
  // MoveCursorToEnd always makes current input mode default.
  SetInputMode(comeback_input_mode_);
}

void Composer::MoveCursorTo(uint32 new_position) {
  if (new_position <= composition_->GetLength()) {
    position_ = new_position;
    UpdateInputMode();
  }
}

void Composer::GetPreedit(string *left, string *focused, string *right) const {
  DCHECK(left);
  DCHECK(focused);
  DCHECK(right);
  composition_->GetPreedit(position_, left, focused, right);

  // TODO(komatsu): This function can be obsolete.
  string preedit = *left + *focused + *right;
  if (TransformCharactersForNumbers(&preedit)) {
    const size_t left_size = Util::CharsLen(*left);
    const size_t focused_size = Util::CharsLen(*focused);
    *left = Util::SubString(preedit, 0, left_size);
    *focused = Util::SubString(preedit, left_size, focused_size);
    *right = Util::SubString(preedit, left_size + focused_size, string::npos);
  }
}

void Composer::GetStringForPreedit(string *output) const {
  composition_->GetString(output);
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
  const commands::SessionCommand::InputFieldType field_type =
      GetInputFieldType();
  if (field_type == commands::SessionCommand::NUMBER ||
      field_type == commands::SessionCommand::PASSWORD ||
      field_type == commands::SessionCommand::TEL) {
    const string tmp = *output;
    Util::FullWidthAsciiToHalfWidthAscii(tmp, output);
  }
}

void Composer::GetStringForSubmission(string *output) const {
  // TODO(komatsu): We should make sure if we can integrate this
  // function to GetStringForPreedit after a while.
  GetStringForPreedit(output);
}

void Composer::GetQueryForConversion(string *output) const {
  string base_output;
  composition_->GetStringWithTrimMode(FIX, &base_output);
  TransformCharactersForNumbers(&base_output);
  Util::FullWidthAsciiToHalfWidthAscii(base_output, output);
}

namespace {
// Determine which query is suitable for a prediction query and return
// its pointer.
// Exmaple:
// = Romanji Input =
// ("もz", "も") -> "も"  // a part of romanji should be trimed.
// ("もzky", "もz") -> "もzky"  // a user might intentionally typed them.
// ("z", "") -> "z"      // ditto.
// = Kana Input =
// ("か", "") -> "か"  // a part of kana (it can be "が") should not be trimed.
string *GetBaseQueryForPrediction(string *asis_query,
                                  string *trimed_query) {
  // If the sizes are equal, there is no matter.
  if (asis_query->size() == trimed_query->size()) {
    return asis_query;
  }

  // Get the different part between asis_query and trimed_query.  For
  // example, "ky" is the different part where asis_query is "もzky"
  // and trimed_query is "もz".
  DCHECK_GT(asis_query->size(), trimed_query->size());
  const string asis_tail = asis_query->substr(trimed_query->size());
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
  const string trimed_tail = Util::SubString(*trimed_query,
                                             Util::CharsLen(*trimed_query) - 1,
                                             string::npos);
  DCHECK(!trimed_tail.empty());
  const Util::ScriptType trimed_tail_type = Util::GetScriptType(trimed_tail);
  if (trimed_tail_type == Util::ALPHABET) {
    return asis_query;
  } else {
    return trimed_query;
  }
}
}  // anonymous namespace

void Composer::GetQueryForPrediction(string *output) const {
  string asis_query;
  composition_->GetStringWithTrimMode(ASIS, &asis_query);

  switch (input_mode_) {
    case transliteration::HALF_ASCII: {
      output->assign(asis_query);
      return;
    }
    case transliteration::FULL_ASCII: {
      Util::FullWidthAsciiToHalfWidthAscii(asis_query, output);
      return;
    }
    default: {}
  }

  string trimed_query;
  composition_->GetStringWithTrimMode(TRIM, &trimed_query);

  // NOTE(komatsu): This is a hack to go around the difference
  // expectation between Romanji-Input and Kana-Input.  "かn" in
  // Romaji-Input should be "か" while "あか" in Kana-Input should be
  // "あか", although "かn" and "あか" have the same properties.  An
  // ideal solution is to expand the ambguity and pass all of them to
  // the converter. (e.g. "かn" -> ["かな",..."かの", "かん", ...] /
  // "あか" -> ["あか", "あが"])
  string *base_query = GetBaseQueryForPrediction(&asis_query, &trimed_query);
  TransformCharactersForNumbers(base_query);
  Util::FullWidthAsciiToHalfWidthAscii(*base_query, output);
}

void Composer::GetQueriesForPrediction(
    string *base, set<string> *expanded) const {
  DCHECK(base);
  DCHECK(expanded);
  DCHECK(composition_.get());
  // In case of the Latin input modes, we don't perform expansion.
  switch (input_mode_) {
    case transliteration::HALF_ASCII:
    case transliteration::FULL_ASCII: {
      GetQueryForPrediction(base);
      expanded->clear();
      return;
    }
    default: {}
  }
  composition_->GetExpandedStrings(base, expanded);
}

size_t Composer::GetLength() const {
  return composition_->GetLength();
}

size_t Composer::GetCursor() const {
  return position_;
}

void Composer::GetTransliterations(
    transliteration::Transliterations *t13ns) const {
  GetSubTransliterations(0, GetLength(), t13ns);
}

void Composer::GetSubTransliteration(
    const transliteration::TransliterationType type,
    const size_t position,
    const size_t size,
    string *transliteration) const {
  const TransliteratorInterface *t12r = GetTransliterator(type);
  const TransliteratorInterface *kNullT12r = NULL;

  string full_base;
  composition_->GetStringWithTransliterator(t12r, &full_base);

  const size_t t13n_start =
    composition_->ConvertPosition(position, kNullT12r, t12r);
  const size_t t13n_end =
    composition_->ConvertPosition(position + size, kNullT12r, t12r);
  const size_t t13n_size = t13n_end - t13n_start;

  const string sub_base = Util::SubString(full_base, t13n_start, t13n_size);
  transliteration->clear();
  Transliterate(type, sub_base, transliteration);
}

void Composer::GetSubTransliterations(
    const size_t position,
    const size_t size,
    transliteration::Transliterations *transliterations) const {
  string t13n;
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
  if (!GET_CONFIG(use_auto_ime_turn_off)) {
    return;
  }

  // AutoSwitchMode is only available on Roma input
  if (GET_CONFIG(preedit_method) != config::Config::ROMAN) {
    return;
  }

  string key;
  // Key should be in half-width alphanumeric.
  composition_->GetStringWithTransliterator(
      GetTransliterator(transliteration::HALF_ASCII), &key);

  ModeSwitchingHandler::ModeSwitching display_mode =
      ModeSwitchingHandler::NO_CHANGE;
  ModeSwitchingHandler::ModeSwitching input_mode =
      ModeSwitchingHandler::NO_CHANGE;
  if (!ModeSwitchingHandler::GetModeSwitchingHandler()->GetModeSwitchingRule(
          key, &display_mode, &input_mode)) {
    // If the key is not a pattern of mode switch rule, the procedure
    // stops here.
    return;
  }

  // |display_mode| affects the existing composition the user typed.
  switch (display_mode) {
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
      LOG(ERROR) << "Unkown value: " << display_mode;
      break;
  }

  // |input_mode| affects the current input mode used for the user's
  // new typing.
  switch (input_mode) {
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
      LOG(ERROR) << "Unkown value: " << display_mode;
      break;
  }
}

bool Composer::ShouldCommit() const {
  return composition_->ShouldCommit();
}

bool Composer::ShouldCommitHead(size_t *length_to_commit) const {
  size_t max_remaining_composition_length;
  switch (GetInputFieldType()) {
    case commands::SessionCommand::PASSWORD:
      max_remaining_composition_length = 1;
      break;
    case commands::SessionCommand::TEL:
    case commands::SessionCommand::NUMBER:
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
  OTHERS,
};

bool IsAlphabetOrNumber(const Script script) {
  return (script == ALPHABET) || (script == NUMBER);
}

static const char *kNumberSymbols[] = {
  "+", "*", "/", "=", "(", ")", "<", ">",
  // "＋", "＊", "／", "＝",
  "\xEF\xBC\x8B", "\xEF\xBC\x8A", "\xEF\xBC\x8F", "\xEF\xBC\x9D",
  // "（", "）", "＜", "＞",
  "\xEF\xBC\x88", "\xEF\xBC\x89", "\xEF\xBC\x9C", "\xEF\xBC\x9E",
};
}  // anonymous namespace

// static
bool Composer::TransformCharactersForNumbers(string *query) {
  if (query == NULL) {
    LOG(ERROR) << "query is NULL";
    return false;
  }

  // Create a vector of scripts of query characters to avoid
  // processing query string many times.
  const size_t chars_len = Util::CharsLen(*query);
  vector<Script> char_scripts;
  // flags to determine whether continue to the next step.
  bool has_symbols = false;
  bool has_alphanumerics = false;
  for (size_t i = 0; i < chars_len; ++i) {
    Script script = OTHERS;
    const string one_char = Util::SubString(*query, i, 1);
    if (one_char == "\xE3\x83\xBC") {  // "ー"
      has_symbols = true;
      script = JA_HYPHEN;
    } else if (one_char == "\xE3\x80\x81") {  // "、"
      has_symbols = true;
      script = JA_COMMA;
    } else if (one_char == "\xE3\x80\x82") {  // "。"
      has_symbols = true;
      script = JA_PERIOD;
    } else if (Util::IsScriptType(one_char, Util::NUMBER)) {
      has_alphanumerics = true;
      script = NUMBER;
    } else if (Util::IsScriptType(one_char, Util::ALPHABET)) {
      has_alphanumerics = true;
      script = ALPHABET;
    } else {
      for (size_t j = 0; j < arraysize(kNumberSymbols); ++j) {
        if (one_char == kNumberSymbols[j]) {
          script = ALPHABET;
          break;
        }
      }
    }
    char_scripts.push_back(script);
  }
  DCHECK_EQ(chars_len, char_scripts.size());
  if (!has_alphanumerics || !has_symbols) {
    VLOG(1) << "The query contains neither alphanumeric nor symbol.";
    return false;
  }

  string transformed_query;
  bool transformed = false;
  for (size_t i = 0; i < chars_len; ++i) {
    const Script script = char_scripts[i];
    if (script == OTHERS || IsAlphabetOrNumber(script)) {
      // Append one character.
      transformed_query.append(Util::SubString(*query, i, 1));
      continue;
    }

    // JA_HYPHEN(s) "ー" is/are transformed to "−" if:
    // (i) query has one and only one leading JA_HYPHEN followed by a number,
    // (ii) JA_HYPHEN(s) follow(s) after an alphanumeric (ex. 0-, 0----, etc).
    // Note that rule (i) implies that if query starts with more than
    // one JA_HYPHENs, those JA_HYPHENs are not transformed.
    if (script == JA_HYPHEN) {
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
        string append_char;
        CharacterFormManager::GetCharacterFormManager()->
            ConvertPreeditString("\xE2\x88\x92", &append_char);  // "−"
        transformed_query.append(append_char);
        transformed = true;
      } else {
        // Append one character.
        transformed_query.append(Util::SubString(*query, i, 1));
      }
      continue;
    }

    // "、" should be "，" if the previous character and the next
    // character are both alphanumerics.
    if (script == JA_COMMA) {
      // Previous char should exist and be a number.
      const bool lhs_check = (i > 0 && IsAlphabetOrNumber(char_scripts[i - 1]));
      // JA_COMMA should be transformed to COMMA.
      if (lhs_check) {
        string append_char;
        CharacterFormManager::GetCharacterFormManager()->
            ConvertPreeditString("\xEF\xBC\x8C", &append_char);  // "，"
        transformed_query.append(append_char);
        transformed = true;
      } else {
        // Append one character.
        transformed_query.append(Util::SubString(*query, i, 1));
      }
      continue;
    }

    // "。" should be "．" if the previous character and the next
    // character are both alphanumerics.
    if (script == JA_PERIOD) {
      // Previous char should exist and be a number.
      const bool lhs_check = (i > 0 && IsAlphabetOrNumber(char_scripts[i - 1]));
      // JA_PRERIOD should be transformed to PRERIOD.
      if (lhs_check) {
        string append_char;
        CharacterFormManager::GetCharacterFormManager()->
            ConvertPreeditString("\xEF\xBC\x8E", &append_char);  // "．"
        transformed_query.append(append_char);
        transformed = true;
      } else {
        // Append one character.
        transformed_query.append(Util::SubString(*query, i, 1));
      }
      continue;
    }
    DLOG(FATAL) << "Should not come here.";
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

void Composer::SetNewInput() {
  is_new_input_ = true;
}

void Composer::CopyFrom(const Composer &src) {
  Reset();

  input_mode_ = src.input_mode_;
  comeback_input_mode_ = src.comeback_input_mode_;
  output_mode_ = src.output_mode_;
  input_field_type_ = src.input_field_type_;

  position_ = src.position_;
  is_new_input_ = src.is_new_input_;
  shifted_sequence_count_ = src.shifted_sequence_count_;
  source_text_.assign(src.source_text_);
  max_length_ = src.max_length_;

  composition_.reset(src.composition_->Clone());

  request_.CopyFrom(src.request_);
}

bool Composer::is_new_input() const {
  return is_new_input_;
}

size_t Composer::shifted_sequence_count() const {
  return shifted_sequence_count_;
}

const string &Composer::source_text() const {
  return source_text_;
}
string *Composer::mutable_source_text() {
  return &source_text_;
}
void Composer::set_source_text(const string &source_text) {
  source_text_.assign(source_text);
}

size_t Composer::max_length() const {
  return max_length_;
}
void Composer::set_max_length(size_t length) {
  max_length_ = length;
}

void Composer::SetInputFieldType(
    commands::SessionCommand::InputFieldType type) {
  input_field_type_ = type;
}

commands::SessionCommand::InputFieldType Composer::GetInputFieldType() const {
  return input_field_type_;
}
}  // namespace composer
}  // namespace mozc
