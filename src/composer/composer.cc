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
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/btree_set.h"
#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "base/clock.h"
#include "base/japanese_util.h"
#include "base/strings/assign.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/internal/composition.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/mode_switching_handler.h"
#include "composer/internal/transliterators.h"
#include "composer/key_event_util.h"
#include "composer/table.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace composer {
namespace {

using ::mozc::config::CharacterFormManager;
using ::mozc::strings::OneCharLen;

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

std::string Transliterate(const transliteration::TransliterationType mode,
                          const absl::string_view input) {
  // When the mode is HALF_KATAKANA, Full width ASCII is also
  // transformed.
  if (mode == transliteration::HALF_KATAKANA) {
    const std::string katakana = japanese_util::HiraganaToKatakana(input);
    return japanese_util::FullWidthToHalfWidth(katakana);
  }

  switch (mode) {
    case transliteration::HALF_ASCII:
      return japanese_util::FullWidthAsciiToHalfWidthAscii(input);
    case transliteration::HALF_ASCII_UPPER: {
      std::string output = japanese_util::FullWidthAsciiToHalfWidthAscii(input);
      Util::UpperString(&output);
      return output;
    }
    case transliteration::HALF_ASCII_LOWER: {
      std::string output = japanese_util::FullWidthAsciiToHalfWidthAscii(input);
      Util::LowerString(&output);
      return output;
    }
    case transliteration::HALF_ASCII_CAPITALIZED: {
      std::string output = japanese_util::FullWidthAsciiToHalfWidthAscii(input);
      Util::CapitalizeString(&output);
      return output;
    }
    case transliteration::FULL_ASCII:
      return japanese_util::HalfWidthAsciiToFullWidthAscii(input);
    case transliteration::FULL_ASCII_UPPER: {
      std::string output = japanese_util::HalfWidthAsciiToFullWidthAscii(input);
      Util::UpperString(&output);
      return output;
    }
    case transliteration::FULL_ASCII_LOWER: {
      std::string output = japanese_util::HalfWidthAsciiToFullWidthAscii(input);
      Util::LowerString(&output);
      return output;
    }
    case transliteration::FULL_ASCII_CAPITALIZED: {
      std::string output = japanese_util::HalfWidthAsciiToFullWidthAscii(input);
      Util::CapitalizeString(&output);
      return output;
    }
    case transliteration::FULL_KATAKANA:
      return japanese_util::HiraganaToKatakana(input);
    case transliteration::HIRAGANA:
      return std::string{input};
    default:
      LOG(ERROR) << "Unknown TransliterationType: " << mode;
      return std::string{input};
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
                                    absl::btree_set<std::string> *expanded) {
  if (!absl::StartsWith(asis, base)) {
    LOG(DFATAL) << "base is not a prefix of asis.";
    return;
  }

  const absl::string_view trailing(asis.substr(base.size()));
  for (auto [iter, end] = GetModifierRemovalMap()->equal_range(trailing);
       iter != end; ++iter) {
    expanded->erase(std::string(iter->second));
  }
}

constexpr size_t kMaxPreeditLength = 256;

namespace common {
std::string GetStringForPreedit(
    const Composition &composition,
    const commands::Context::InputFieldType input_field_type) {
  std::string output = composition.GetString();
  Composer::TransformCharactersForNumbers(&output);
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
  if (input_field_type == commands::Context::NUMBER ||
      input_field_type == commands::Context::PASSWORD ||
      input_field_type == commands::Context::TEL) {
    output = japanese_util::FullWidthAsciiToHalfWidthAscii(output);
  }
  return output;
}

std::string GetQueryForConversion(const Composition &composition) {
  std::string base_output = composition.GetStringWithTrimMode(FIX);
  Composer::TransformCharactersForNumbers(&base_output);
  return japanese_util::FullWidthAsciiToHalfWidthAscii(base_output);
}

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

std::string GetQueryForPrediction(
    const Composition &composition,
    const transliteration::TransliterationType input_mode) {
  std::string asis_query = composition.GetStringWithTrimMode(ASIS);

  switch (input_mode) {
    case transliteration::HALF_ASCII: {
      return asis_query;
    }
    case transliteration::FULL_ASCII: {
      return japanese_util::FullWidthAsciiToHalfWidthAscii(asis_query);
    }
    default: {
    }
  }

  std::string trimed_query = composition.GetStringWithTrimMode(TRIM);

  // NOTE(komatsu): This is a hack to go around the difference
  // expectation between Romanji-Input and Kana-Input.  "かn" in
  // Romaji-Input should be "か" while "あか" in Kana-Input should be
  // "あか", although "かn" and "あか" have the same properties.  An
  // ideal solution is to expand the ambguity and pass all of them to
  // the converter. (e.g. "かn" -> ["かな",..."かの", "かん", ...] /
  // "あか" -> ["あか", "あが"])
  std::string *base_query =
      GetBaseQueryForPrediction(&asis_query, &trimed_query);
  Composer::TransformCharactersForNumbers(base_query);
  return japanese_util::FullWidthAsciiToHalfWidthAscii(*base_query);
}

std::pair<std::string, absl::btree_set<std::string>> GetQueriesForPrediction(
    const Composition &composition,
    const transliteration::TransliterationType input_mode) {
  // In case of the Latin input modes, we don't perform expansion.
  switch (input_mode) {
    case transliteration::HALF_ASCII:
    case transliteration::FULL_ASCII: {
      return std::make_pair(GetQueryForPrediction(composition, input_mode),
                            absl::btree_set<std::string>());
    }
    default: {
    }
  }

  // auto = std::pair<std::string, absl::btree_set<std::string>>
  auto[base_query, expanded] = composition.GetExpandedStrings();

  // The above `GetExpandedStrings` generates expansion for modifier key as
  // well, e.g., if the composition is "ざ", `expanded` contains "さ" too.
  // However, "ざ" is usually composed by explicitly hitting the modifier key.
  // So we don't want to generate prediction from "さ" in this case. The
  // following code removes such unnecessary expansion.
  const std::string asis = composition.GetStringWithTrimMode(ASIS);
  RemoveExpandedCharsForModifier(asis, base_query, &expanded);

  return std::make_pair(
    japanese_util::FullWidthAsciiToHalfWidthAscii(base_query), expanded);
}

std::string GetStringForTypeCorrection(const Composition &composition) {
  return composition.GetStringWithTrimMode(ASIS);
}

std::string GetTransliteratedText(const Composition &composition,
                                  Transliterators::Transliterator t12r,
                                  const size_t position, const size_t size) {
  const std::string full_base = composition.GetStringWithTransliterator(t12r);

  const size_t t13n_start =
      composition.ConvertPosition(position, Transliterators::LOCAL, t12r);
  const size_t t13n_end = composition.ConvertPosition(
      position + size, Transliterators::LOCAL, t12r);
  const size_t t13n_size = t13n_end - t13n_start;

  return std::string{Util::Utf8SubString(full_base, t13n_start, t13n_size)};
}

std::string GetRawSubString(const Composition &composition,
                            const size_t position, const size_t size) {
  return GetTransliteratedText(composition, Transliterators::RAW_STRING,
                               position, size);
}

std::string GetRawString(const Composition &composition) {
  return GetRawSubString(composition, 0, composition.GetLength());
}

std::string GetSubTransliteration(
    const Composition &composition,
    const transliteration::TransliterationType type, const size_t position,
    const size_t size) {
  const Transliterators::Transliterator t12r = GetTransliterator(type);
  const std::string result =
      GetTransliteratedText(composition, t12r, position, size);
  return Transliterate(type, result);
}

void GetSubTransliterations(
    const Composition &composition, const size_t position, const size_t size,
    transliteration::Transliterations *transliterations) {
  for (size_t i = 0; i < transliteration::NUM_T13N_TYPES; ++i) {
    const transliteration::TransliterationType t13n_type =
        transliteration::TransliterationTypeArray[i];
    const std::string t13n =
        GetSubTransliteration(composition, t13n_type, position, size);
    transliterations->push_back(t13n);
  }
}

void GetTransliterations(const Composition &composition,
                         transliteration::Transliterations *t13ns) {
  GetSubTransliterations(composition, 0, composition.GetLength(), t13ns);
}
}  // namespace common
}  // namespace

// ComposerData

ComposerData::ComposerData(
    Composition composition, size_t position,
    transliteration::TransliterationType input_mode,
    commands::Context::InputFieldType input_field_type,
    std::string source_text,
    std::vector<commands::SessionCommand::CompositionEvent>
        compositions_for_handwriting)
    : composition_(composition),
      position_(position),
      input_mode_(input_mode),
      input_field_type_(input_field_type),
      source_text_(source_text),
      compositions_for_handwriting_(compositions_for_handwriting) {}

ComposerData::ComposerData(const ComposerData &other)
    : composition_(other.composition_),
      position_(other.position_),
      input_mode_(other.input_mode_),
      input_field_type_(other.input_field_type_),
      source_text_(other.source_text_),
      compositions_for_handwriting_(other.compositions_for_handwriting_) {}

ComposerData &ComposerData::operator=(const ComposerData &other) {
  if (this != &other) {
    composition_ = other.composition_;
    position_ = other.position_;
    input_mode_ = other.input_mode_;
    input_field_type_ = other.input_field_type_;
    source_text_ = other.source_text_;
    compositions_for_handwriting_ = other.compositions_for_handwriting_;
  }
  return *this;
}

ComposerData::ComposerData(ComposerData &&other) noexcept
    : composition_(std::move(other.composition_)),
      position_(other.position_),
      input_mode_(other.input_mode_),
      input_field_type_(other.input_field_type_),
      source_text_(std::move(other.source_text_)),
      compositions_for_handwriting_(
          std::move(other.compositions_for_handwriting_)) {}

ComposerData &ComposerData::operator=(ComposerData &&other) noexcept {
  if (this != &other) {
    composition_ = std::move(other.composition_);
    position_ = other.position_;
    input_mode_ = other.input_mode_;
    input_field_type_ = other.input_field_type_;
    source_text_ = std::move(other.source_text_);
    compositions_for_handwriting_ =
        std::move(other.compositions_for_handwriting_);
  }
  return *this;
}

transliteration::TransliterationType ComposerData::GetInputMode() const {
  return input_mode_;
}

absl::Span<const commands::SessionCommand::CompositionEvent>
ComposerData::GetHandwritingCompositions() const {
  return compositions_for_handwriting_;
}

std::string ComposerData::GetStringForPreedit() const {
  return common::GetStringForPreedit(composition_, input_field_type_);
}

std::string ComposerData::GetQueryForConversion() const {
  return common::GetQueryForConversion(composition_);
}

std::string ComposerData::GetQueryForPrediction() const {
  return common::GetQueryForPrediction(composition_, input_mode_);
}

std::pair<std::string, absl::btree_set<std::string>>
ComposerData::GetQueriesForPrediction() const {
  return common::GetQueriesForPrediction(composition_, input_mode_);
}

std::string ComposerData::GetStringForTypeCorrection() const {
  return common::GetStringForTypeCorrection(composition_);
}

size_t ComposerData::GetLength() const { return composition_.GetLength(); }

size_t ComposerData::GetCursor() const { return position_; }

std::string ComposerData::GetRawString() const {
  return common::GetRawString(composition_);
}

std::string ComposerData::GetRawSubString(const size_t position,
                                      const size_t size) const {
  return common::GetRawSubString(composition_, position, size);
}

void ComposerData::GetTransliterations(
    transliteration::Transliterations *t13ns) const {
  common::GetTransliterations(composition_, t13ns);
}

void ComposerData::GetSubTransliterations(
    const size_t position, const size_t size,
    transliteration::Transliterations *t13ns) const {
  common::GetSubTransliterations(composition_, position, size, t13ns);
}

// Composer

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
      max_length_(kMaxPreeditLength),
      request_(request),
      config_(config),
      table_(table),
      is_new_input_(true) {
  SetInputMode(transliteration::HIRAGANA);
  if (config_ == nullptr) {
    config_ = &config::ConfigHandler::DefaultConfig();
  }
  Reset();
}

// static
ComposerData Composer::CreateEmptyComposerData() {
  static const absl::NoDestructor<Table> table;
  static const absl::NoDestructor<Composition> composition(table.get());
  return ComposerData(*composition, 0, transliteration::HIRAGANA,
                      commands::Context::NORMAL, "", {});
}

ComposerData Composer::CreateComposerData() const {
  return ComposerData(composition_, position_, input_mode_, input_field_type_,
                      source_text_, compositions_for_handwriting_);
}

void Composer::Reset() {
  EditErase();
  ResetInputMode();
  SetOutputMode(transliteration::HIRAGANA);
  source_text_.clear();
  timeout_threshold_msec_ = config_->composing_timeout_threshold_msec();
  compositions_for_handwriting_.clear();
}

void Composer::ResetInputMode() { SetInputMode(comeback_input_mode_); }

void Composer::ReloadConfig() {
  // Do nothing at this moment.
}

bool Composer::Empty() const { return (GetLength() == 0); }

void Composer::SetTable(const Table *table) {
  table_ = table;
  composition_.SetTable(table);
}

void Composer::SetRequest(const commands::Request *request) {
  request_ = request;
}

void Composer::SetConfig(const config::Config *config) { config_ = config; }

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
    // TODO(komatsu): Refer user's preference.
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

bool Composer::ProcessCompositionInput(CompositionInput input) {
  if (!EnableInsert()) {
    return false;
  }

  position_ = composition_.InsertInput(position_, std::move(input));
  is_new_input_ = false;
  return true;
}

void Composer::InsertCharacter(std::string key) {
  CompositionInput input;
  input.InitFromRaw(std::move(key), is_new_input_);
  ProcessCompositionInput(std::move(input));
}

void Composer::InsertCommandCharacter(const InternalCommand internal_command) {
  CompositionInput input;
  switch (internal_command) {
    case REWIND:
      input.InitFromRaw(table_->ParseSpecialKey("{<}"), is_new_input_);
      ProcessCompositionInput(std::move(input));
      break;
    case STOP_KEY_TOGGLING:
      input.InitFromRaw(table_->ParseSpecialKey("{!}"), is_new_input_);
      ProcessCompositionInput(std::move(input));
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

  const Utf8AsChars input_chars(input);
  for (const absl::string_view c : input_chars) {
    CompositionInput composition_input;
    composition_input.set_raw(c);
    composition_input.set_is_new_input(is_new_input_);
    position_ =
        composition_.InsertInput(position_, std::move(composition_input));
    is_new_input_ = false;
  }

  std::string lower_input(input);
  Util::LowerString(&lower_input);
  if (Util::IsLowerAscii(lower_input)) {
    // Fake input mode.
    // This is useful to test the behavior of alphabet keyboard.
    SetTemporaryInputMode(transliteration::HALF_ASCII);
  }
}

void Composer::SetCompositionsForHandwriting(
    absl::Span<const commands::SessionCommand::CompositionEvent *const>
        compositions) {
  Reset();
  compositions_for_handwriting_.clear();
  for (const auto &elm : compositions) {
    compositions_for_handwriting_.push_back(*elm);
  }

  if (compositions_for_handwriting_.empty()) {
    return;
  }
  composition_.SetInputMode(Transliterators::RAW_STRING);

  const Utf8AsChars input_chars(
      compositions_for_handwriting_.front().composition_string());
  for (const absl::string_view c : input_chars) {
    CompositionInput composition_input;
    composition_input.set_raw(c);
    composition_input.set_is_new_input(is_new_input_);
    position_ =
        composition_.InsertInput(position_, std::move(composition_input));
    is_new_input_ = false;
  }
}

absl::Span<const commands::SessionCommand::CompositionEvent>
Composer::GetHandwritingCompositions() const {
  return compositions_for_handwriting_;
}

bool Composer::InsertCharacterKeyAndPreedit(const absl::string_view key,
                                            const absl::string_view preedit) {
  CompositionInput input;
  input.InitFromRawAndConv(std::string(key), std::string(preedit),
                           is_new_input_);
  return ProcessCompositionInput(std::move(input));
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

  if (!input.conversion().empty()) {
    if (input.is_asis()) {
      composition_.SetInputMode(Transliterators::CONVERSION_STRING);
      // Disable typing correction mainly for Android. b/258369101
      ProcessCompositionInput(std::move(input));
      SetInputMode(comeback_input_mode_);
    } else {
      // Kana input usually has conversion. Note that, the existence of
      // key_string never determine if the input mode is Kana or Romaji.
      ProcessCompositionInput(std::move(input));
    }
  } else {
    // Romaji input usually does not has conversion. Note that, the
    // existence of key_string never determines if the input mode is
    // Kana or Romaji.
    const uint32_t modifiers = KeyEventUtil::GetModifiers(key);
    ApplyTemporaryInputMode(input.raw(), KeyEventUtil::HasCaps(modifiers));
    ProcessCompositionInput(std::move(input));
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
}

void Composer::Delete() {
  position_ = composition_.DeleteAt(position_);
  UpdateInputMode();
}

void Composer::DeleteRange(size_t pos, size_t length) {
  for (int i = 0; i < length && pos < composition_.GetLength(); ++i) {
    DeleteAt(pos);
  }
}

void Composer::EditErase() {
  composition_.Erase();
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
  position_ = composition_.DeleteAt(position_);
}

void Composer::MoveCursorLeft() {
  if (position_ > 0) {
    --position_;
  }
  UpdateInputMode();
}

void Composer::MoveCursorRight() {
  if (position_ < composition_.GetLength()) {
    ++position_;
  }
  UpdateInputMode();
}

void Composer::MoveCursorToBeginning() {
  position_ = 0;
  SetInputMode(comeback_input_mode_);
}

void Composer::MoveCursorToEnd() {
  position_ = composition_.GetLength();
  // Behavior between MoveCursorToEnd and MoveCursorToRight is different.
  // MoveCursorToEnd always makes current input mode default.
  SetInputMode(comeback_input_mode_);
}

void Composer::MoveCursorTo(uint32_t new_position) {
  if (new_position <= composition_.GetLength()) {
    position_ = new_position;
    UpdateInputMode();
  }
}

void Composer::GetPreedit(std::string *left, std::string *focused,
                          std::string *right) const {
  DCHECK(left);
  DCHECK(focused);
  DCHECK(right);
  composition_.GetPreedit(position_, left, focused, right);

  // TODO(komatsu): This function can be obsolete.
  std::string preedit = absl::StrCat(*left, *focused, *right);
  if (TransformCharactersForNumbers(&preedit)) {
    const size_t left_size = Util::CharsLen(*left);
    const size_t focused_size = Util::CharsLen(*focused);
    Util::Utf8SubString(preedit, 0, left_size, left);
    Util::Utf8SubString(preedit, left_size, focused_size, focused);
    Util::Utf8SubString(preedit, left_size + focused_size, std::string::npos,
                        right);
  }
}

std::string Composer::GetStringForPreedit() const {
  return common::GetStringForPreedit(composition_, input_field_type_);
}

std::string Composer::GetStringForSubmission() const {
  // TODO(komatsu): We should make sure if we can integrate this
  // function to GetStringForPreedit after a while.
  return GetStringForPreedit();
}

std::string Composer::GetQueryForConversion() const {
  return common::GetQueryForConversion(composition_);
}

std::string Composer::GetQueryForPrediction() const {
  return common::GetQueryForPrediction(composition_, input_mode_);
}

std::pair<std::string, absl::btree_set<std::string>>
Composer::GetQueriesForPrediction() const {
  return common::GetQueriesForPrediction(composition_, input_mode_);
}

std::string Composer::GetStringForTypeCorrection() const {
  return common::GetStringForTypeCorrection(composition_);
}

size_t Composer::GetLength() const { return composition_.GetLength(); }

size_t Composer::GetCursor() const { return position_; }

std::string Composer::GetTransliteratedText(
    Transliterators::Transliterator t12r, const size_t position,
    const size_t size) const {
  return common::GetTransliteratedText(composition_, t12r, position, size);
}

std::string Composer::GetRawString() const {
  return common::GetRawString(composition_);
}

std::string Composer::GetRawSubString(const size_t position,
                                      const size_t size) const {
  return common::GetRawSubString(composition_, position, size);
}

void Composer::GetTransliterations(
    transliteration::Transliterations *t13ns) const {
  common::GetTransliterations(composition_, t13ns);
}

std::string Composer::GetSubTransliteration(
    const transliteration::TransliterationType type, const size_t position,
    const size_t size) const {
  return common::GetSubTransliteration(composition_, type, position, size);
}

void Composer::GetSubTransliterations(
    const size_t position, const size_t size,
    transliteration::Transliterations *t13ns) const {
  common::GetSubTransliterations(composition_, position, size, t13ns);
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

  // Key should be in half-width alphanumeric.
  const std::string key = composition_.GetStringWithTransliterator(
      GetTransliterator(transliteration::HALF_ASCII));

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
    MOZC_VLOG(1) << "The query contains neither alphanumeric nor symbol.";
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
        // JA_PERIOD should be transformed to PERIOD.
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
      Util::CodepointToUtf8Append(iter.Get(), &transformed_query);
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
  *query = std::move(transformed_query);
  return true;
}

void Composer::SetNewInput() { is_new_input_ = true; }

bool Composer::IsToggleable() const {
  // When |is_new_input_| is true, a new chunk is always created and, hence, key
  // toggling never happens regardless of the composition state.
  return !is_new_input_ && composition_.IsToggleable(position_);
}

void Composer::set_source_text(const absl::string_view source_text) {
  strings::Assign(source_text_, source_text);
}

void Composer::SetInputFieldType(commands::Context::InputFieldType type) {
  input_field_type_ = type;
}

commands::Context::InputFieldType Composer::GetInputFieldType() const {
  return input_field_type_;
}

}  // namespace composer
}  // namespace mozc
