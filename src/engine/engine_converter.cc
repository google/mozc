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

// A class handling the converter on the session layer.

#include "engine/engine_converter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "converter/candidate.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "engine/candidate_list.h"
#include "engine/engine_converter_interface.h"
#include "engine/engine_output.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace engine {
namespace {

using ::mozc::commands::Request;
using ::mozc::config::Config;

absl::string_view GetCandidateShortcuts(
    config::Config::SelectionShortcut selection_shortcut) {
  // Keyboard shortcut for candidates.
  constexpr absl::string_view kShortcut123456789 = "123456789";
  constexpr absl::string_view kShortcutASDFGHJKL = "asdfghjkl";
  constexpr absl::string_view kNoShortcut = "";

  absl::string_view shortcut = kNoShortcut;
  switch (selection_shortcut) {
    case config::Config::SHORTCUT_123456789:
      shortcut = kShortcut123456789;
      break;
    case config::Config::SHORTCUT_ASDFGHJKL:
      shortcut = kShortcutASDFGHJKL;
      break;
    case config::Config::NO_SHORTCUT:
      break;
    default:
      LOG(WARNING) << "Unknown shortcuts type: " << selection_shortcut;
      break;
  }
  return shortcut;
}

// Calculate cursor offset for committed text.
int32_t CalculateCursorOffset(absl::string_view committed_text) {
  // If committed_text is a bracket pair, set the cursor in the middle.
  return Util::IsBracketPairText(committed_text) ? -1 : 0;
}
}  // namespace

EngineConverter::EngineConverter(
    std::shared_ptr<const ConverterInterface> converter)
    : EngineConverter(std::move(converter), composer::GetSharedDefaultRequest(),
                      config::ConfigHandler::GetSharedDefaultConfig()) {}

EngineConverter::EngineConverter(
    std::shared_ptr<const ConverterInterface> converter,
    std::shared_ptr<const commands::Request> request,
    std::shared_ptr<const Config> config)
    : EngineConverterInterface(),
      converter_(std::move(converter)),
      segments_(),
      incognito_segments_(),
      segment_index_(0),
      result_(),
      candidate_list_(true),
      request_(std::move(request)),
      state_(COMPOSITION),
      request_type_(ConversionRequest::CONVERSION),
      client_revision_(0),
      candidate_list_visible_(false) {
  DCHECK(request_);
  DCHECK(converter_);
  DCHECK(config);
  conversion_preferences_.use_history = true;
  conversion_preferences_.request_suggestion = true;
  candidate_list_.set_page_size(request_->candidate_page_size());
  SetConfig(std::move(config));
}

bool EngineConverter::CheckState(
    EngineConverterInterface::States states) const {
  return ((state_ & states) != NO_STATE);
}

bool EngineConverter::IsActive() const {
  return CheckState(SUGGESTION | PREDICTION | CONVERSION);
}

const ConversionPreferences &EngineConverter::conversion_preferences() const {
  return conversion_preferences_;
}

bool EngineConverter::Convert(const composer::Composer &composer) {
  return ConvertWithPreferences(composer, conversion_preferences_);
}

bool EngineConverter::ConvertWithPreferences(
    const composer::Composer &composer,
    const ConversionPreferences &preferences) {
  DCHECK(CheckState(COMPOSITION | SUGGESTION | CONVERSION));

  DCHECK(request_);
  DCHECK(config_);
  ConversionRequest::Options options;
  options.enable_user_history_for_conversion = preferences.use_history;
  SetRequestType(ConversionRequest::CONVERSION, options);
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetComposer(composer)
          .SetRequestView(*request_)
          .SetConfigView(*config_)
          .SetOptions(std::move(options))
          .Build();

  if (!converter_->StartConversion(conversion_request, &segments_)) {
    LOG(WARNING) << "StartConversion() failed";
    ResetState();
    return false;
  }

  segment_index_ = 0;
  state_ = CONVERSION;
  candidate_list_visible_ = false;
  UpdateCandidateList();
  InitializeSelectedCandidateIndices();
  return true;
}

bool EngineConverter::GetReadingText(absl::string_view source_text,
                                     std::string *reading) {
  DCHECK(reading);
  reading->clear();
  Segments reverse_segments;
  // TODO(team): Replace with StartReverseConversionForRequest()
  // once it is implemented.
  if (!converter_->StartReverseConversion(&reverse_segments, source_text)) {
    return false;
  }
  if (reverse_segments.segments_size() == 0) {
    LOG(WARNING) << "no segments from reverse conversion";
    return false;
  }
  for (const Segment &segment : reverse_segments) {
    if (segment.candidates_size() == 0) {
      LOG(WARNING) << "got an empty segment from reverse conversion";
      return false;
    }
    reading->append(segment.candidate(0).value);
  }
  return true;
}

namespace {
Attributes GetT13nAttributes(const transliteration::TransliterationType type) {
  Attributes attributes = NO_ATTRIBUTES;
  switch (type) {
    case transliteration::HIRAGANA:  // "ひらがな"
      attributes = HIRAGANA;
      break;
    case transliteration::FULL_KATAKANA:  // "カタカナ"
      attributes = (FULL_WIDTH | KATAKANA);
      break;
    case transliteration::HALF_ASCII:  // "ascII"
      attributes = (HALF_WIDTH | ASCII | ASIS);
      break;
    case transliteration::HALF_ASCII_UPPER:  // "ASCII"
      attributes = (HALF_WIDTH | ASCII | UPPER);
      break;
    case transliteration::HALF_ASCII_LOWER:  // "ascii"
      attributes = (HALF_WIDTH | ASCII | LOWER);
      break;
    case transliteration::HALF_ASCII_CAPITALIZED:  // "Ascii"
      attributes = (HALF_WIDTH | ASCII | CAPITALIZED);
      break;
    case transliteration::FULL_ASCII:  // "ａｓｃＩＩ"
      attributes = (FULL_WIDTH | ASCII | ASIS);
      break;
    case transliteration::FULL_ASCII_UPPER:  // "ＡＳＣＩＩ"
      attributes = (FULL_WIDTH | ASCII | UPPER);
      break;
    case transliteration::FULL_ASCII_LOWER:  // "ａｓｃｉｉ"
      attributes = (FULL_WIDTH | ASCII | LOWER);
      break;
    case transliteration::FULL_ASCII_CAPITALIZED:  // "Ａｓｃｉｉ"
      attributes = (FULL_WIDTH | ASCII | CAPITALIZED);
      break;
    case transliteration::HALF_KATAKANA:  // "ｶﾀｶﾅ"
      attributes = (HALF_WIDTH | KATAKANA);
      break;
    default:
      LOG(ERROR) << "Unknown type: " << type;
      break;
  }
  return attributes;
}

// Cycles ASCII (= Alphanumeric) cases to ASIS → UPPER → LOWER → CAPITALIZED.
// example:
//   "moZc": moZc (ASIS) → MOZC (UPPER) → mozc (LOWER) → Mozc (CAPITALIZED) →
//           moZc (ASIS) → ...
//
// If UPPER, LOWER, or CAPITALIZED is the same as ASIS, skip it and cycle to the
// next case.
// example:
//   "mozc": mozc (ASIS | LOWER) → MOZC (UPPER) → Mozc (CAPITALIZED) →
//           mozc (ASIS | LOWER) →
//   "MOZC": MOZC (ASIS | UPPER) → mozc (LOWER) → Mozc (CAPITALIZED) →
//           MOZC (ASIS | UPPER) →
//   "m": m (ASIS | LOWER) → M (UPPER | CAPACALIZED) → m (ASIS | LOWER) →
//   "M": M (ASIS | UPPER | CAPACALIZED) → m (LOWER) →
//        M (ASIS | UPPER | CAPACALIZED) →
void CycleAlphaCase(Attributes query_attr, CandidateList &candidate_list) {
  Attributes current_attr =
      candidate_list.GetDeepestFocusedCandidate().attributes();

  // If the current case is same as the user typed, move to the next case.
  if (current_attr & ASIS) {
    // The next case is basically UPPER.
    // However, if the ASIS is also UPPER, skip it and move to the LOWER case.
    query_attr |= ((current_attr & UPPER) ? LOWER : UPPER);
    candidate_list.MoveNextAttributes(query_attr);
    return;
  }

  // Move to the next case. If the next case is also ASIS, skip it as it's
  // already cycled before.
  // Try up to 3 times as there are 4 cases and avoid infinite loop.
  const Attributes base_query_attr = query_attr;
  for (int i = 0; i < 3; ++i) {
    // Set query_attr to the next case and move it.
    if (current_attr & UPPER) {
      query_attr = base_query_attr | LOWER;
    } else if (current_attr & LOWER) {
      query_attr = base_query_attr | CAPITALIZED;
    } else if (current_attr & CAPITALIZED) {
      query_attr = base_query_attr | ASIS;
    } else {  // nothing.
      query_attr = base_query_attr | UPPER;
    }
    candidate_list.MoveNextAttributes(query_attr);

    // If the next case is intentional ASIS, no need to skip it.
    if (query_attr & ASIS) {
      break;
    }

    const Attributes new_attr =
        candidate_list.GetDeepestFocusedCandidate().attributes();

    // If the next case is not ASIS, no need to skip it.
    if (!(new_attr & ASIS)) {
      break;
    }

    // This checks an edge case. Even if the next case is also ASIS,
    // but the next case is only available case, we should not skip it.
    // If all possible attributes are covered by the current and next cases,
    // it means the next case is only available case.
    const Attributes sum_attr = new_attr | current_attr;
    if ((sum_attr & ASIS) && (sum_attr & UPPER) && (sum_attr & LOWER) &&
        (sum_attr & CAPITALIZED)) {
      break;
    }

    // The new case also contains ASIS, skip it and get the next case.
    current_attr = new_attr;
  }
}
}  // namespace

bool EngineConverter::ConvertToTransliteration(
    const composer::Composer &composer,
    const transliteration::TransliterationType type) {
  DCHECK(CheckState(COMPOSITION | SUGGESTION | PREDICTION | CONVERSION));
  if (CheckState(PREDICTION)) {
    // TODO(komatsu): A better way is to transliterate the key of the
    // focused candidate.  However it takes a long time.
    Cancel();
    DCHECK(CheckState(COMPOSITION));
  }

  Attributes query_attr =
      (GetT13nAttributes(type) &
       (HALF_WIDTH | FULL_WIDTH | ASCII | HIRAGANA | KATAKANA));

  if (CheckState(COMPOSITION | SUGGESTION)) {
    if (!Convert(composer)) {
      LOG(ERROR) << "Conversion failed";
      return false;
    }

    // TODO(komatsu): This is a workaround to transliterate the whole
    // preedit as a single segment.  We should modify
    // converter/converter.cc to enable to accept mozc::Segment::FIXED
    // from the session layer.
    if (segment_index_ + 1 != segments_.conversion_segments_size()) {
      size_t offset = 0;
      for (const Segment &segment :
           segments_.conversion_segments().drop(segment_index_ + 1)) {
        offset += segment.key_len();
      }
      ResizeSegmentWidth(composer, offset);
    }

    DCHECK(CheckState(CONVERSION));

    // The initial transliteration to ASCII is always as-is case.
    // e.g. もZc → moZc
    if (query_attr & ASCII) {
      query_attr |= ASIS;
    }
    candidate_list_.MoveToAttributes(query_attr);
  } else {
    DCHECK(CheckState(CONVERSION));
    Attributes current_attr =
        candidate_list_.GetDeepestFocusedCandidate().attributes();
    const Attributes common_attr = current_attr & query_attr;

    // Transliterations among half-width and full-width will keep the case.
    // e.g. Mozc → Ｍｏｚｃ
    if ((common_attr & ASCII) &&
        ((((query_attr & HALF_WIDTH) && (current_attr & FULL_WIDTH))) ||
         (((query_attr & FULL_WIDTH) && (current_attr & HALF_WIDTH))))) {
      query_attr |= (current_attr & (UPPER | LOWER | CAPITALIZED | ASIS));
    }

    if ((common_attr & ASCII) &&
        ((common_attr & HALF_WIDTH) || (common_attr & FULL_WIDTH))) {
      CycleAlphaCase(query_attr, candidate_list_);
    } else {
      candidate_list_.MoveNextAttributes(query_attr);
    }
  }
  candidate_list_visible_ = false;
  // Treat as top conversion candidate on usage stats.
  selected_candidate_indices_[segment_index_] = 0;
  SegmentFocus();
  return true;
}

bool EngineConverter::ConvertToHalfWidth(const composer::Composer &composer) {
  DCHECK(CheckState(COMPOSITION | SUGGESTION | PREDICTION | CONVERSION));
  if (CheckState(PREDICTION)) {
    // TODO(komatsu): A better way is to transliterate the key of the
    // focused candidate.  However it takes a long time.
    Cancel();
    DCHECK(CheckState(COMPOSITION));
  }

  std::string composition;
  if (CheckState(COMPOSITION | SUGGESTION)) {
    composition = composer.GetStringForPreedit();
  } else {
    composition = GetSelectedCandidate(segment_index_).value;
  }

  // TODO(komatsu): make a function to return a logical sum of ScriptType.
  // If composition_ is "あｂｃ", it should be treated as Katakana.
  if (Util::ContainsScriptType(composition, Util::KATAKANA) ||
      Util::ContainsScriptType(composition, Util::HIRAGANA) ||
      Util::ContainsScriptType(composition, Util::KANJI) ||
      Util::IsKanaSymbolContained(composition)) {
    return ConvertToTransliteration(composer, transliteration::HALF_KATAKANA);
  } else {
    return ConvertToTransliteration(composer, transliteration::HALF_ASCII);
  }
}

bool EngineConverter::SwitchKanaType(const composer::Composer &composer) {
  DCHECK(CheckState(COMPOSITION | SUGGESTION | PREDICTION | CONVERSION));
  if (CheckState(PREDICTION)) {
    // TODO(komatsu): A better way is to transliterate the key of the
    // focused candidate.  However it takes a long time.
    Cancel();
    DCHECK(CheckState(COMPOSITION));
  }

  Attributes attributes = NO_ATTRIBUTES;
  if (CheckState(COMPOSITION | SUGGESTION)) {
    if (!Convert(composer)) {
      LOG(ERROR) << "Conversion failed";
      return false;
    }

    // TODO(komatsu): This is a workaround to transliterate the whole
    // preedit as a single segment.  We should modify
    // converter/converter.cc to enable to accept mozc::Segment::FIXED
    // from the session layer.
    if (segments_.conversion_segments_size() != 1) {
      uint8_t offset = 0;
      for (size_t i = 0; i < segments_.conversion_segments_size(); ++i) {
        offset += segments_.conversion_segment(i).key_len();
      }
      DCHECK(request_);
      DCHECK(config_);
      const ConversionRequest conversion_request =
          ConversionRequestBuilder()
              .SetComposer(composer)
              .SetRequestView(*request_)
              .SetConfigView(*config_)
              .Build();
      if (!converter_->ResizeSegments(&segments_, conversion_request, 0,
                                      {offset})) {
        LOG(WARNING) << "ResizeSegment failed for segments.";
        DLOG(WARNING) << segments_.DebugString();
      }
      UpdateCandidateList();
    }

    attributes = (FULL_WIDTH | KATAKANA);
  } else {
    const Attributes current_attributes =
        candidate_list_.GetDeepestFocusedCandidate().attributes();
    // "漢字" -> "かんじ" -> "カンジ" -> "ｶﾝｼﾞ" -> "かんじ" -> ...
    if (current_attributes & HIRAGANA) {
      attributes = (FULL_WIDTH | KATAKANA);
    } else if ((current_attributes & KATAKANA) &&
               (current_attributes & FULL_WIDTH)) {
      attributes = (HALF_WIDTH | KATAKANA);
    } else {
      attributes = HIRAGANA;
    }
  }

  DCHECK(CheckState(CONVERSION));
  candidate_list_.MoveNextAttributes(attributes);
  candidate_list_visible_ = false;
  // Treat as top conversion candidate on usage stats.
  selected_candidate_indices_[segment_index_] = 0;
  SegmentFocus();
  return true;
}

bool EngineConverter::Suggest(const composer::Composer &composer,
                              const commands::Context &context) {
  return SuggestWithPreferences(composer, context, conversion_preferences_);
}

bool EngineConverter::SuggestWithPreferences(
    const composer::Composer &composer, const commands::Context &context,
    const ConversionPreferences &preferences) {
  DCHECK(CheckState(COMPOSITION | SUGGESTION));
  candidate_list_visible_ = false;

  // Normalize the current state by resetting the previous state.
  ResetState();

  // If we are on a password field, suppress suggestion.
  if (!preferences.request_suggestion ||
      composer.GetInputFieldType() == commands::Context::PASSWORD) {
    return false;
  }

  // Initialize the conversion request and segments for suggestion.
  ConversionRequest::Options options;
  options.enable_user_history_for_conversion = preferences.use_history;
  segments_.clear_conversion_segments();

  const size_t cursor = composer.GetCursor();

  // We have four (2x2) conditions for
  // (use_prediction_candidate, use_partial_composition):
  // - (false, false): Original suggestion behavior on desktop.
  // - (false, true): Never happens.
  // - (true, false): Mobile suggestion with richer candidates through
  //                  prediction API.
  // - (true, true): Mobile suggestion with richer candidates through
  //                  prediction API, using partial composition text.
  const bool use_prediction_candidate = request_->mixed_conversion();
  const bool use_partial_composition =
      (cursor != composer.GetLength() && cursor != 0 &&
       request_->mixed_conversion());

  // Setup request based on the above two flags.
  options.use_actual_converter_for_realtime_conversion = true;
  if (use_partial_composition) {
    // Auto partial suggestion should be activated only when we use all the
    // composition.
    // Note: For now, use_partial_composition is only for mobile typing.
    SetRequestType(ConversionRequest::PARTIAL_PREDICTION, options);
  } else {
    options.create_partial_candidates = request_->auto_partial_suggestion();
    if (use_prediction_candidate) {
      SetRequestType(ConversionRequest::PREDICTION, options);
    } else {
      SetRequestType(ConversionRequest::SUGGESTION, options);
    }
  }

  DCHECK(config_);
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetComposer(composer)
          .SetRequestView(*request_)
          .SetContextView(context)
          .SetConfigView(*config_)
          .SetOptions(std::move(options))
          .Build();

  // Start actual suggestion/prediction.
  bool result = converter_->StartPrediction(conversion_request, &segments_);
  if (!result) {
    MOZC_VLOG(1)
        << "Start(Partial?)(Suggestion|Prediction)ForRequest() returns no "
           "suggestions.";
    // Clear segments and keep the context
    converter_->CancelConversion(&segments_);
    return false;
  }

  // Fill incognito candidates if required.
  // The candidates are always from suggestion API
  // as richer results are not needed.
  if (request_->fill_incognito_candidate_words()) {
    ConversionRequest::Options incognito_options = conversion_request.options();
    incognito_options.enable_user_history_for_conversion = false;
    incognito_options.request_type = use_partial_composition
                                         ? ConversionRequest::PARTIAL_SUGGESTION
                                         : ConversionRequest::SUGGESTION;
    incognito_options.incognito_mode = true;
    const ConversionRequest incognito_conversion_request =
        ConversionRequestBuilder()
            .SetConversionRequestView(conversion_request)
            .SetConfigView(*config_)
            .SetOptions(std::move(incognito_options))
            .Build();
    incognito_segments_.Clear();
    result = converter_->StartPrediction(incognito_conversion_request,
                                         &incognito_segments_);
    if (!result) {
      MOZC_VLOG(1)
          << "Start(Partial?)SuggestionForRequest() for incognito request "
             "returned no suggestions.";
      // TODO(noriyukit): Check if fall through here is ok.
    }
  }
  DCHECK_EQ(segments_.conversion_segments_size(), 1);

  // Copy current suggestions so that we can merge
  // prediction/suggestions later
  previous_suggestions_ = segments_.conversion_segment(0);

  // Overwrite the request type to SUGGESTION.
  // Without this logic, a candidate gets focused that is unexpected behavior.
  request_type_ = ConversionRequest::SUGGESTION;

  // TODO(komatsu): the next line can be deleted.
  segment_index_ = 0;
  state_ = SUGGESTION;
  UpdateCandidateList();
  candidate_list_visible_ = true;
  InitializeSelectedCandidateIndices();
  return true;
}

bool EngineConverter::Predict(const composer::Composer &composer) {
  return PredictWithPreferences(composer, conversion_preferences_);
}

bool EngineConverter::IsEmptySegment(const Segment &segment) const {
  return ((segment.candidates_size() == 0) &&
          (segment.meta_candidates_size() == 0));
}

bool EngineConverter::PredictWithPreferences(
    const composer::Composer &composer,
    const ConversionPreferences &preferences) {
  // TODO(komatsu): DCHECK should be
  // DCHECK(CheckState(COMPOSITION | SUGGESTION | PREDICTION));
  DCHECK(CheckState(COMPOSITION | SUGGESTION | CONVERSION | PREDICTION));
  ResetResult();

  // Initialize the segments and conversion_request for prediction
  ConversionRequest::Options options;
  options.enable_user_history_for_conversion = preferences.use_history;
  DCHECK(request_);
  DCHECK(config_);
  SetRequestType(ConversionRequest::PREDICTION, options);
  options.use_actual_converter_for_realtime_conversion = true;
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetComposer(composer)
          .SetRequestView(*request_)
          .SetConfigView(*config_)
          .SetOptions(std::move(options))
          .Build();

  const bool predict_first =
      !CheckState(PREDICTION) && IsEmptySegment(previous_suggestions_);

  const bool predict_expand =
      (CheckState(PREDICTION) && !IsEmptySegment(previous_suggestions_) &&
       candidate_list_.size() > 0 && candidate_list_.focused() &&
       candidate_list_.focused_index() == candidate_list_.last_index());

  segments_.clear_conversion_segments();

  if (predict_expand || predict_first) {
    const bool result = converter_->StartPredictionWithPreviousSuggestion(
        conversion_request, previous_suggestions_, &segments_);
    if (!result && predict_first) {
      // Returns false if we failed at the first prediction.
      // If predict_expand is true, it means we have prevous_suggestions_.
      // So we can use it as the result of this prediction.
      ResetState();
      return false;
    }
  } else {
    converter_->PrependCandidates(conversion_request, previous_suggestions_,
                                  &segments_);
  }

  segment_index_ = 0;
  state_ = PREDICTION;
  UpdateCandidateList();
  candidate_list_visible_ = true;
  InitializeSelectedCandidateIndices();
  return true;
}

void EngineConverter::MaybeExpandPrediction(
    const composer::Composer &composer) {
  DCHECK(CheckState(PREDICTION | CONVERSION));

  // Expand the current suggestions and fill with Prediction results.
  if (!CheckState(PREDICTION) || IsEmptySegment(previous_suggestions_) ||
      !candidate_list_.focused() ||
      candidate_list_.focused_index() != candidate_list_.last_index()) {
    return;
  }

  DCHECK(CheckState(PREDICTION));
  ResetResult();

  const size_t previous_index = candidate_list_.focused_index();
  if (!PredictWithPreferences(composer, conversion_preferences_)) {
    return;
  }

  DCHECK_LT(previous_index, candidate_list_.size());
  candidate_list_.MoveToId(candidate_list_.candidate(previous_index).id());
  UpdateSelectedCandidateIndex();
}

void EngineConverter::Cancel() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  ResetResult();

  // Clear segments and keep the context
  converter_->CancelConversion(&segments_);
  ResetState();
}

void EngineConverter::Reset() {
  DCHECK(CheckState(COMPOSITION | SUGGESTION | PREDICTION | CONVERSION));

  // Even if composition mode, call ResetConversion
  // in order to clear history segments.
  converter_->ResetConversion(&segments_);

  if (CheckState(COMPOSITION)) {
    return;
  }

  ResetResult();
  // Reset segments (and its internal context)
  ResetState();
}

void EngineConverter::Commit(const composer::Composer &composer,
                             const commands::Context &context) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  if (!UpdateResult(0, segments_.conversion_segments_size(), nullptr)) {
    Cancel();
    ResetState();
    return;
  }

  for (size_t i = 0; i < segments_.conversion_segments_size(); ++i) {
    if (!converter_->CommitSegmentValue(&segments_, i,
                                        GetCandidateIndexForConverter(i))) {
      LOG(WARNING) << "Failed to commit segment " << i;
    }
  }
  CommitSegmentsSize(state_, context);
  DCHECK(request_);
  DCHECK(config_);
  const ConversionRequest conversion_request = ConversionRequestBuilder()
                                                   .SetComposer(composer)
                                                   .SetRequestView(*request_)
                                                   .SetContextView(context)
                                                   .SetConfigView(*config_)
                                                   .Build();
  converter_->FinishConversion(conversion_request, &segments_);
  ResetState();
}

bool EngineConverter::CommitSuggestionInternal(
    const composer::Composer &composer, const commands::Context &context,
    size_t *consumed_key_size) {
  DCHECK(consumed_key_size);
  DCHECK(CheckState(SUGGESTION));
  ResetResult();
  const std::string preedit = composer.GetStringForPreedit();

  if (!UpdateResult(0, segments_.conversion_segments_size(),
                    consumed_key_size)) {
    // Do not need to call Cancel like Commit because the current
    // state is SUGGESTION.
    ResetState();
    return false;
  }

  const size_t preedit_length = Util::CharsLen(preedit);

  // TODO(horo): When we will support hardware keyboard and introduce
  // shift+enter keymap in Android, this if condition may be insufficient.
  if (request_->zero_query_suggestion() &&
      *consumed_key_size < composer.GetLength()) {
    // A candidate was chosen from partial suggestion.
    if (!converter_->CommitPartialSuggestionSegmentValue(
            &segments_, 0, GetCandidateIndexForConverter(0),
            Util::Utf8SubString(preedit, 0, *consumed_key_size),
            Util::Utf8SubString(preedit, *consumed_key_size,
                                preedit_length - *consumed_key_size))) {
      LOG(WARNING) << "CommitPartialSuggestionSegmentValue failed";
      return false;
    }
    CommitSegmentsSize(EngineConverterInterface::SUGGESTION, context);
    InitializeSelectedCandidateIndices();
    // One or more segments must exist because new segment is inserted
    // just after the committed segment.
    DCHECK_GT(segments_.conversion_segments_size(), 0);
  } else {
    // Not partial suggestion so let's reset the state.
    if (!converter_->CommitSegmentValue(&segments_, 0,
                                        GetCandidateIndexForConverter(0))) {
      LOG(WARNING) << "CommitSegmentValue failed";
      return false;
    }
    CommitSegmentsSize(EngineConverterInterface::SUGGESTION, context);
    DCHECK(config_);
    const ConversionRequest conversion_request = ConversionRequestBuilder()
                                                     .SetComposer(composer)
                                                     .SetRequestView(*request_)
                                                     .SetContextView(context)
                                                     .SetConfigView(*config_)
                                                     .Build();
    converter_->FinishConversion(conversion_request, &segments_);
    DCHECK_EQ(0, segments_.conversion_segments_size());
    ResetState();
  }
  return true;
}

bool EngineConverter::CommitSuggestionByIndex(
    const size_t index, const composer::Composer &composer,
    const commands::Context &context, size_t *consumed_key_size) {
  DCHECK(CheckState(SUGGESTION));
  if (index >= candidate_list_.size()) {
    LOG(ERROR) << "index is out of the range: " << index;
    return false;
  }
  candidate_list_.MoveToPageIndex(index);
  UpdateSelectedCandidateIndex();
  return CommitSuggestionInternal(composer, context, consumed_key_size);
}

bool EngineConverter::CommitSuggestionById(const int id,
                                           const composer::Composer &composer,
                                           const commands::Context &context,
                                           size_t *consumed_key_size) {
  DCHECK(CheckState(SUGGESTION));
  if (!candidate_list_.MoveToId(id)) {
    // Don't use CandidateMoveToId() method, which overwrites candidates.
    // This is harmful for EXPAND_SUGGESTION session command.
    LOG(ERROR) << "No id found";
    return false;
  }
  UpdateSelectedCandidateIndex();
  return CommitSuggestionInternal(composer, context, consumed_key_size);
}

void EngineConverter::CommitHeadToFocusedSegments(
    const composer::Composer &composer, const commands::Context &context,
    size_t *consumed_key_size) {
  CommitSegmentsInternal(composer, context, segment_index_ + 1,
                         consumed_key_size);
}

void EngineConverter::CommitFirstSegment(const composer::Composer &composer,
                                         const commands::Context &context,
                                         size_t *consumed_key_size) {
  CommitSegmentsInternal(composer, context, 1, consumed_key_size);
}

void EngineConverter::CommitSegmentsInternal(const composer::Composer &composer,
                                             const commands::Context &context,
                                             size_t segments_to_commit,
                                             size_t *consumed_key_size) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  DCHECK(segments_.conversion_segments_size() >= segments_to_commit);
  ResetResult();
  candidate_list_visible_ = false;
  *consumed_key_size = 0;

  // If commit all segments, just call Commit.
  if (segments_.conversion_segments_size() <= segments_to_commit) {
    Commit(composer, context);
    return;
  }

  // Store the first conversion segment to the result.
  if (!UpdateResult(0, segments_to_commit, nullptr)) {
    // If the selected candidate of the first segment has the command
    // attribute, Cancel is performed instead of Commit.
    Cancel();
    ResetState();
    return;
  }

  std::vector<size_t> candidate_ids;
  for (size_t i = 0; i < segments_to_commit; ++i) {
    // Get the i-th (0 origin) conversion segment and the selected candidate.
    const Segment &segment = segments_.conversion_segment(i);

    // Accumulate the size of i-th segment's key.
    // The caller will remove corresponding characters from the composer.
    *consumed_key_size += segment.key_len();

    // Collect candidate's id for each segment.
    candidate_ids.push_back(GetCandidateIndexForConverter(i));
  }
  if (!converter_->CommitSegments(&segments_, candidate_ids)) {
    LOG(WARNING) << "CommitSegments failed";
  }

  // Commit the [0, segments_to_commit - 1] conversion segment.
  CommitSegmentsSize(segments_to_commit);

  // Adjust the segment_index, since the [0, segment_to_commit - 1] segments
  // disappeared.
  // Note that segment_index_ is unsigned.
  segment_index_ = segment_index_ > segments_to_commit
                       ? segment_index_ - segments_to_commit
                       : 0;
  UpdateCandidateList();
}

void EngineConverter::CommitPreedit(const composer::Composer &composer,
                                    const commands::Context &context) {
  const std::string key = composer.GetQueryForConversion();
  const std::string preedit = composer.GetStringForSubmission();
  std::string normalized_preedit = TextNormalizer::NormalizeText(preedit);
  output::FillPreeditResult(preedit, &result_);

  // Add ResultToken
  commands::ResultToken *token = result_.add_tokens();
  token->set_key(preedit);
  token->set_value(preedit);

  // Cursor offset needs to be calculated based on normalized text.
  output::FillCursorOffsetResult(CalculateCursorOffset(normalized_preedit),
                                 &result_);
  segments_.InitForCommit(key, normalized_preedit);
  CommitSegmentsSize(EngineConverterInterface::COMPOSITION, context);
  DCHECK(request_);
  DCHECK(config_);
  // the request mode is CONVERSION, as the user experience
  // is similar to conversion. UserHistoryPredictor distinguishes
  // CONVERSION from SUGGESTION now.
  ConversionRequest::Options options;
  SetRequestType(ConversionRequest::CONVERSION, options);
  const ConversionRequest conversion_request =
      ConversionRequestBuilder()
          .SetComposer(composer)
          .SetRequestView(*request_)
          .SetContextView(context)
          .SetConfigView(*config_)
          .SetOptions(std::move(options))
          .Build();
  converter_->FinishConversion(conversion_request, &segments_);
  ResetState();
}

void EngineConverter::CommitHead(size_t count,
                                 const composer::Composer &composer,
                                 size_t *consumed_key_size) {
  std::string preedit = composer.GetStringForSubmission();
  if (count > preedit.length()) {
    *consumed_key_size = preedit.length();
  } else {
    *consumed_key_size = count;
  }
  Util::Utf8SubString(preedit, 0, *consumed_key_size, &preedit);
  const std::string composition = TextNormalizer::NormalizeText(preedit);
  output::FillPreeditResult(composition, &result_);
  output::FillCursorOffsetResult(CalculateCursorOffset(composition), &result_);
}

void EngineConverter::Revert() { converter_->RevertConversion(&segments_); }

bool EngineConverter::DeleteCandidateFromHistory(std::optional<int> id) {
  if (id == std::nullopt) {
    if (!candidate_list_.focused()) {
      return false;
    }
    const Candidate &cand = candidate_list_.focused_candidate();
    id = cand.id();
  } else {
    if (segment_index_ >= segments_.conversion_segments_size()) {
      return false;
    }
    const Segment &segment = segments_.conversion_segment(segment_index_);
    if (!segment.is_valid_index(*id)) {
      return false;
    }
  }
  DCHECK(id.has_value());
  return converter_->DeleteCandidateFromHistory(
      segments_, segments_.history_segments_size() + segment_index_, *id);
}

void EngineConverter::SegmentFocusInternal(size_t index) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  candidate_list_visible_ = false;
  if (CheckState(PREDICTION)) {
    return;  // Do nothing.
  }
  ResetResult();

  if (segment_index_ == index) {
    return;
  }

  SegmentFix();
  segment_index_ = index;
  UpdateCandidateList();
}

void EngineConverter::SegmentFocusRight() {
  if (segment_index_ + 1 >= segments_.conversion_segments_size()) {
    // If |segment_index_| is at the tail of the segments,
    // focus on the head.
    SegmentFocusLeftEdge();
  } else {
    SegmentFocusInternal(segment_index_ + 1);
  }
}

void EngineConverter::SegmentFocusLast() {
  const size_t r_edge = segments_.conversion_segments_size() - 1;
  SegmentFocusInternal(r_edge);
}

void EngineConverter::SegmentFocusLeft() {
  if (segment_index_ <= 0) {
    // If |segment_index_| is at the head of the segments,
    // focus on the tail.
    SegmentFocusLast();
  } else {
    SegmentFocusInternal(segment_index_ - 1);
  }
}

void EngineConverter::SegmentFocusLeftEdge() { SegmentFocusInternal(0); }

void EngineConverter::ResizeSegmentWidth(const composer::Composer &composer,
                                         int delta) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  candidate_list_visible_ = false;
  if (CheckState(PREDICTION)) {
    return;  // Do nothing.
  }
  ResetResult();

  DCHECK(request_);
  DCHECK(config_);
  const ConversionRequest conversion_request = ConversionRequestBuilder()
                                                   .SetComposer(composer)
                                                   .SetRequestView(*request_)
                                                   .SetConfigView(*config_)
                                                   .Build();
  if (!converter_->ResizeSegment(&segments_, conversion_request, segment_index_,
                                 delta)) {
    return;
  }

  UpdateCandidateList();
  // Clears selected index of a focused segment and trailing segments.
  // TODO(hsumita): Keep the indices if the segment type is FIXED_VALUE.
  selected_candidate_indices_.resize(segments_.conversion_segments_size());
  std::fill(selected_candidate_indices_.begin() + segment_index_ + 1,
            selected_candidate_indices_.end(), 0);
  UpdateSelectedCandidateIndex();
}

void EngineConverter::SegmentWidthExpand(const composer::Composer &composer) {
  ResizeSegmentWidth(composer, 1);
}

void EngineConverter::SegmentWidthShrink(const composer::Composer &composer) {
  ResizeSegmentWidth(composer, -1);
}

void EngineConverter::CandidateNext(const composer::Composer &composer) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  MaybeExpandPrediction(composer);
  candidate_list_.MoveNext();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void EngineConverter::CandidateNextPage() {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_.MoveNextPage();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void EngineConverter::CandidatePrev() {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_.MovePrev();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void EngineConverter::CandidatePrevPage() {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_.MovePrevPage();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void EngineConverter::CandidateMoveToId(const int id,
                                        const composer::Composer &composer) {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  ResetResult();

  if (CheckState(SUGGESTION)) {
    // This method makes a candidate focused but SUGGESTION state cannot
    // have focused candidate.
    // To solve this conflict we call Predict() method to transit to
    // PREDICTION state, on which existence of focused candidate is acceptable.
    Predict(composer);
  }
  DCHECK(CheckState(PREDICTION | CONVERSION));

  candidate_list_.MoveToId(id);
  candidate_list_visible_ = false;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void EngineConverter::CandidateMoveToPageIndex(const size_t index) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_.MoveToPageIndex(index);
  candidate_list_visible_ = false;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

bool EngineConverter::CandidateMoveToShortcut(const char shortcut) {
  DCHECK(CheckState(PREDICTION | CONVERSION));

  if (!candidate_list_visible_) {
    MOZC_VLOG(1) << "Candidate list is not displayed.";
    return false;
  }

  const absl::string_view shortcuts(GetCandidateShortcuts(selection_shortcut_));
  if (shortcuts.empty()) {
    MOZC_VLOG(1) << "No shortcuts";
    return false;
  }

  // Check if the input character is in the shortcut.
  // TODO(komatsu): Support non ASCII characters such as Unicode and
  // special keys.
  const absl::string_view::size_type index = shortcuts.find(shortcut);
  if (index == absl::string_view::npos) {
    MOZC_VLOG(1) << "shortcut is not a member of shortcuts.";
    return false;
  }

  if (!candidate_list_.MoveToPageIndex(index)) {
    MOZC_VLOG(1) << "shortcut is out of the range.";
    return false;
  }
  UpdateSelectedCandidateIndex();
  ResetResult();
  SegmentFocus();
  return true;
}

void EngineConverter::SetCandidateListVisible(bool visible) {
  candidate_list_visible_ = visible;
}

void EngineConverter::PopOutput(const composer::Composer &composer,
                                commands::Output *output) {
  FillOutput(composer, output);
  updated_command_ = converter::Candidate::DEFAULT_COMMAND;
  ResetResult();
}

namespace {
void MaybeFillConfig(converter::Candidate::Command command,
                     const config::Config &base_config,
                     commands::Output *output) {
  if (command == converter::Candidate::DEFAULT_COMMAND) {
    return;
  }

  *output->mutable_config() = base_config;
  switch (command) {
    case converter::Candidate::ENABLE_INCOGNITO_MODE:
      output->mutable_config()->set_incognito_mode(true);
      break;
    case converter::Candidate::DISABLE_INCOGNITO_MODE:
      output->mutable_config()->set_incognito_mode(false);
      break;
    case converter::Candidate::ENABLE_PRESENTATION_MODE:
      output->mutable_config()->set_presentation_mode(true);
      break;
    case converter::Candidate::DISABLE_PRESENTATION_MODE:
      output->mutable_config()->set_presentation_mode(false);
      break;
    default:
      LOG(WARNING) << "Unknown command: " << command;
      break;
  }
}
}  // namespace

void EngineConverter::FillPreedit(const composer::Composer &composer,
                                  commands::Preedit *preedit) const {
  output::FillPreedit(composer, preedit);
}

void EngineConverter::FillOutput(const composer::Composer &composer,
                                 commands::Output *output) const {
  if (!output) {
    LOG(ERROR) << "output is nullptr.";
    return;
  }
  if (result_.has_value()) {
    FillResult(output->mutable_result());
  }
  if (CheckState(COMPOSITION)) {
    if (!composer.Empty()) {
      output::FillPreedit(composer, output->mutable_preedit());
    }
  }

  MaybeFillConfig(updated_command_, *config_, output);

  if (!IsActive()) {
    return;
  }

  // Composition on Suggestion
  if (CheckState(SUGGESTION)) {
    // When the suggestion comes from zero query suggestion, the
    // composer is empty.  In that case, preedit is not rendered.
    if (!composer.Empty()) {
      output::FillPreedit(composer, output->mutable_preedit());
    }
  } else if (CheckState(PREDICTION | CONVERSION)) {
    // Conversion on Prediction or Conversion
    FillConversion(output->mutable_preedit());
  }
  // Candidate list
  if (CheckState(SUGGESTION | PREDICTION | CONVERSION) &&
      candidate_list_visible_) {
    FillCandidateWindow(output->mutable_candidate_window());
  }

  // All candidate words
  if (CheckState(SUGGESTION | PREDICTION | CONVERSION)) {
    FillAllCandidateWords(output->mutable_all_candidate_words());
    if (request_->fill_incognito_candidate_words()) {
      FillIncognitoCandidateWords(output->mutable_incognito_candidate_words());
    }
  }

  // For debug. Removed candidate words through the conversion process.
  if (CheckState(SUGGESTION | PREDICTION | CONVERSION)) {
    output::FillRemovedCandidates(
        segments_.conversion_segment(segment_index_),
        output->mutable_removed_candidate_words_for_debug());
  }
}

EngineConverter *EngineConverter::Clone() const {
  EngineConverter *engine_converter =
      new EngineConverter(converter_, request_, config_);
  *engine_converter = *this;

  if (engine_converter->CheckState(SUGGESTION | PREDICTION | CONVERSION)) {
    // UpdateCandidateList() is not simple setter and it uses some members.
    engine_converter->UpdateCandidateList();
    engine_converter->candidate_list_.MoveToId(candidate_list_.focused_id());
    engine_converter->SetCandidateListVisible(candidate_list_visible_);
  }

  return engine_converter;
}

void EngineConverter::ResetResult() { result_.Clear(); }

void EngineConverter::ResetState() {
  state_ = COMPOSITION;
  segment_index_ = 0;
  previous_suggestions_.clear();
  candidate_list_visible_ = false;
  candidate_list_.Clear();
  selected_candidate_indices_.clear();
  incognito_segments_.Clear();
}

void EngineConverter::SegmentFocus() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  if (!converter_->FocusSegmentValue(
          &segments_, segment_index_,
          GetCandidateIndexForConverter(segment_index_))) {
    LOG(ERROR) << "FocusSegmentValue failed";
  }
}

void EngineConverter::SegmentFix() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  if (!converter_->CommitSegmentValue(
          &segments_, segment_index_,
          GetCandidateIndexForConverter(segment_index_))) {
    LOG(WARNING) << "CommitSegmentValue failed";
  }
}

void EngineConverter::GetPreedit(const size_t index, const size_t size,
                                 std::string *preedit) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  DCHECK(index + size <= segments_.conversion_segments_size());
  DCHECK(preedit);

  preedit->clear();
  for (size_t i = index; i < size; ++i) {
    if (CheckState(CONVERSION)) {
      // In conversion mode, all the key of candidates is same.
      preedit->append(segments_.conversion_segment(i).key());
    } else {
      DCHECK(CheckState(SUGGESTION | PREDICTION));
      // In suggestion or prediction modes, each key may have
      // different keys, so content_key is used although it is
      // possibly dropped the conjugational word (ex. the content_key
      // of "はしる" is "はし").
      preedit->append(GetSelectedCandidate(i).content_key);
    }
  }
}

void EngineConverter::GetConversion(const size_t index, const size_t size,
                                    std::string *conversion) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  DCHECK(index + size <= segments_.conversion_segments_size());
  DCHECK(conversion);

  conversion->clear();
  for (size_t i = index; i < size; ++i) {
    conversion->append(GetSelectedCandidateValue(i));
  }
}

void EngineConverter::UpdateResultTokens(const size_t index,
                                         const size_t size) {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  DCHECK(index + size <= segments_.conversion_segments_size());

  auto add_tokens = [this](absl::string_view content_key,
                           absl::string_view content_value,
                           absl::string_view functional_key,
                           absl::string_view functional_value) {
    commands::ResultToken *token1 = result_.add_tokens();
    token1->set_key(content_key);
    token1->set_value(content_value);
    if (!functional_key.empty() || !functional_value.empty()) {
      commands::ResultToken *token2 = result_.add_tokens();
      token2->set_key(functional_key);
      token2->set_value(functional_value);
    }
  };

  for (size_t i = index; i < size; ++i) {
    const int cand_idx = GetCandidateIndexForConverter(i);
    const converter::Candidate &candidate =
        segments_.conversion_segment(i).candidate(cand_idx);
    const int first_token_idx = result_.tokens_size();

    if (converter::Candidate::InnerSegmentIterator it(&candidate); !it.Done()) {
      // If the candidate has inner segments, fill them to the result tokens.
      for (; !it.Done(); it.Next()) {
        add_tokens(it.GetContentKey(), it.GetContentValue(),
                   it.GetFunctionalKey(), it.GetFunctionalValue());
      }
    } else {
      add_tokens(candidate.content_key, candidate.content_value,
                 candidate.functional_key(), candidate.functional_value());
    }
    // Set lid and rid to the first and last tokens respectively.
    // Other lids and rids are filled with the default POS (i.e. -1 as unknown).
    const int last_token_idx = result_.tokens_size() - 1;
    DCHECK_GE(last_token_idx, first_token_idx);
    result_.mutable_tokens(first_token_idx)->set_lid(candidate.lid);
    result_.mutable_tokens(last_token_idx)->set_rid(candidate.rid);
  }
}

size_t EngineConverter::GetConsumedPreeditSize(const size_t index,
                                               const size_t size) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  DCHECK(index + size <= segments_.conversion_segments_size());

  if (CheckState(SUGGESTION | PREDICTION)) {
    DCHECK_EQ(1, size);
    const Segment &segment = segments_.conversion_segment(0);
    const int id = GetCandidateIndexForConverter(0);
    const converter::Candidate &candidate = segment.candidate(id);
    return (candidate.attributes & converter::Candidate::PARTIALLY_KEY_CONSUMED)
               ? candidate.consumed_key_size
               : kConsumedAllCharacters;
  }

  DCHECK(CheckState(CONVERSION));
  size_t result = 0;
  for (size_t i = index; i < size; ++i) {
    const int id = GetCandidateIndexForConverter(i);
    const converter::Candidate &candidate =
        segments_.conversion_segment(i).candidate(id);
    DCHECK(
        !(candidate.attributes & converter::Candidate::PARTIALLY_KEY_CONSUMED));
    result += segments_.conversion_segment(i).key_len();
  }
  return result;
}

bool EngineConverter::MaybePerformCommandCandidate(const size_t index,
                                                   const size_t size) {
  // If a candidate has the command attribute, Cancel is performed
  // instead of Commit after executing the specified action.
  for (size_t i = index; i < size; ++i) {
    const int id = GetCandidateIndexForConverter(i);
    const converter::Candidate &candidate =
        segments_.conversion_segment(i).candidate(id);
    if (candidate.attributes & converter::Candidate::COMMAND_CANDIDATE) {
      switch (candidate.command) {
        case converter::Candidate::DEFAULT_COMMAND:
          // Do nothing
          break;
        case converter::Candidate::ENABLE_INCOGNITO_MODE:
        case converter::Candidate::DISABLE_INCOGNITO_MODE:
        case converter::Candidate::ENABLE_PRESENTATION_MODE:
        case converter::Candidate::DISABLE_PRESENTATION_MODE:
          updated_command_ = candidate.command;
          break;
        default:
          LOG(WARNING) << "Unknown command: " << candidate.command;
          break;
      }
      return true;
    }
  }
  return false;
}

bool EngineConverter::UpdateResult(size_t index, size_t size,
                                   size_t *consumed_key_size) {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));

  // If command candidate is performed, result is not updated and
  // returns false.
  if (MaybePerformCommandCandidate(index, size)) {
    return false;
  }

  std::string preedit, conversion;
  GetPreedit(index, size, &preedit);
  GetConversion(index, size, &conversion);
  if (consumed_key_size) {
    *consumed_key_size = GetConsumedPreeditSize(index, size);
  }
  output::FillConversionResult(preedit, conversion, &result_);
  output::FillCursorOffsetResult(CalculateCursorOffset(conversion), &result_);
  UpdateResultTokens(index, size);
  return true;
}

namespace {
// Convert transliteration::TransliterationType to id used in the
// converter.  The id number are negative values, and 0 of
// transliteration::TransliterationType is bound for -1 of the id.
int GetT13nId(const transliteration::TransliterationType type) {
  return -(type + 1);
}
}  // namespace

void EngineConverter::AppendCandidateList() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));

  // Meta candidates are added iff |candidate_list_| is empty.
  // This is because if |candidate_list_| is not empty we cannot decide
  // where to add meta candidates, especially use_cascading_window flag
  // is true (If there are two or more sub candidate lists, and existent
  // meta candidates are not located in the same list (distributed over
  // some lists), the most appropriate location to be added new meta candidates
  // cannot be decided).
  const bool add_meta_candidates = (candidate_list_.size() == 0);

  DCHECK_LT(segment_index_, segments_.conversion_segments_size());
  const Segment &segment = segments_.conversion_segment(segment_index_);

  auto get_candidate_dedup_key =
      [](const converter::Candidate &c) -> const std::string & {
    return c.value;
  };

  for (size_t i = candidate_list_.next_available_id();
       i < segment.candidates_size(); ++i) {
    const converter::Candidate &c = segment.candidate(i);
    candidate_list_.AddCandidate(i, get_candidate_dedup_key(c));
    // if candidate has spelling correction attribute,
    // always display the candidate to let user know the
    // miss spelled candidate.
    if (i < 10 && (segment.candidate(i).attributes &
                   converter::Candidate::SPELLING_CORRECTION)) {
      candidate_list_visible_ = true;
    }
  }

  const bool focused =
      (request_type_ != ConversionRequest::SUGGESTION &&
       request_type_ != ConversionRequest::PARTIAL_SUGGESTION &&
       request_type_ != ConversionRequest::PARTIAL_PREDICTION);
  candidate_list_.set_focused(focused);

  if (segment.meta_candidates_size() == 0) {
    // For suggestion mode, it is natural that T13N is not initialized.
    if (CheckState(SUGGESTION)) {
      return;
    }
    // For other modes, records |segment| just in case.
    MOZC_VLOG(1) << "T13N is not initialized: " << segment.key();
    return;
  }

  if (!add_meta_candidates) {
    return;
  }

  // Set transliteration candidates
  CandidateList *transliterations;
  if (use_cascading_window_) {
    transliterations = candidate_list_.AddSubCandidateList();
    transliterations->set_rotate(false);
    transliterations->set_focused(true);

    constexpr char kT13nLabel[] = "そのほかの文字種";
    transliterations->set_name(kT13nLabel);
  } else {
    transliterations = &candidate_list_;
  }

  // Add transliterations.
  for (size_t i = 0; i < transliteration::NUM_T13N_TYPES; ++i) {
    const transliteration::TransliterationType type =
        transliteration::TransliterationTypeArray[i];
    transliterations->AddCandidateWithAttributes(
        GetT13nId(type), get_candidate_dedup_key(segment.meta_candidate(i)),
        GetT13nAttributes(type));
  }
}

void EngineConverter::UpdateCandidateList() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  candidate_list_.Clear();
  AppendCandidateList();
}

int EngineConverter::GetCandidateIndexForConverter(
    const size_t segment_index) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  // If segment_index does not point to the focused segment, the value
  // should be always zero.
  if (segment_index != segment_index_) {
    return 0;
  }
  return candidate_list_.focused_id();
}

std::string EngineConverter::GetSelectedCandidateValue(
    const size_t segment_index) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  const int id = GetCandidateIndexForConverter(segment_index);
  const converter::Candidate &candidate =
      segments_.conversion_segment(segment_index).candidate(id);
  if (candidate.attributes & converter::Candidate::COMMAND_CANDIDATE) {
    // Return an empty string, however this path should not be reached.
    return "";
  }
  return candidate.value;
}

const converter::Candidate &EngineConverter::GetSelectedCandidate(
    const size_t segment_index) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  const int id = GetCandidateIndexForConverter(segment_index);
  return segments_.conversion_segment(segment_index).candidate(id);
}

void EngineConverter::FillConversion(commands::Preedit *preedit) const {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  output::FillConversion(segments_, segment_index_,
                         candidate_list_.focused_id(), preedit);
}

void EngineConverter::FillResult(commands::Result *result) const {
  *result = result_;
}

void EngineConverter::FillCandidateWindow(
    commands::CandidateWindow *candidate_window) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  if (!candidate_list_visible_) {
    return;
  }

  // The position to display the candidate window.
  size_t position = 0;
  std::string conversion;
  for (size_t i = 0; i < segment_index_; ++i) {
    position += Util::CharsLen(GetSelectedCandidate(i).value);
  }

  // Temporarily added to see if this condition is really satisfied in the
  // real world or not.
#ifdef CHANNEL_DEV
  CHECK_LT(0, segments_.conversion_segments_size());
#endif  // CHANNEL_DEV
  if (segment_index_ >= segments_.conversion_segments_size()) {
    LOG(WARNING) << "Invalid segment_index_: " << segment_index_
                 << ", segments_size: " << segments_.conversion_segments_size();
    return;
  }

  const Segment &segment = segments_.conversion_segment(segment_index_);
  output::FillCandidateWindow(segment, candidate_list_, position,
                              candidate_window);

  // Shortcut keys
  if (CheckState(PREDICTION | CONVERSION)) {
    output::FillShortcuts(GetCandidateShortcuts(selection_shortcut_),
                          candidate_window);
  }

  // Store category
  switch (request_type_) {
    case ConversionRequest::CONVERSION:
      candidate_window->set_category(commands::CONVERSION);
      break;
    case ConversionRequest::PREDICTION:
      candidate_window->set_category(commands::PREDICTION);
      break;
    case ConversionRequest::SUGGESTION:
      candidate_window->set_category(commands::SUGGESTION);
      break;
    case ConversionRequest::PARTIAL_PREDICTION:
      // Not PREDICTION because we do not want to get focused candidate.
      candidate_window->set_category(commands::SUGGESTION);
      break;
    case ConversionRequest::PARTIAL_SUGGESTION:
      candidate_window->set_category(commands::SUGGESTION);
      break;
    default:
      LOG(WARNING) << "Unknown request type: " << request_type_;
      candidate_window->set_category(commands::CONVERSION);
      break;
  }

  if (candidate_window->has_usages()) {
    candidate_window->mutable_usages()->set_category(commands::USAGE);
  }
  if (candidate_window->has_sub_candidate_window()) {
    // TODO(komatsu): Subcandidate is not always for transliterations.
    // The category of the sub candidate window should be checked.
    candidate_window->mutable_sub_candidate_window()->set_category(
        commands::TRANSLITERATION);
  }

  // Store display type
  candidate_window->set_display_type(commands::MAIN);
  if (candidate_window->has_usages()) {
    candidate_window->mutable_usages()->set_display_type(commands::CASCADE);
  }
  if (candidate_window->has_sub_candidate_window()) {
    // TODO(komatsu): Sub candidate window is not always for transliterations.
    // The category of the sub candidate window should be checked.
    candidate_window->mutable_sub_candidate_window()->set_display_type(
        commands::CASCADE);
  }

  // Store footer.
  output::FillFooter(candidate_window->category(), candidate_window);
}

void EngineConverter::FillAllCandidateWords(
    commands::CandidateList *candidates) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  commands::Category category;
  switch (request_type_) {
    case ConversionRequest::CONVERSION:
      category = commands::CONVERSION;
      break;
    case ConversionRequest::PREDICTION:
      category = commands::PREDICTION;
      break;
    case ConversionRequest::SUGGESTION:
      category = commands::SUGGESTION;
      break;
    case ConversionRequest::PARTIAL_PREDICTION:
      // Not PREDICTION because we do not want to get focused candidate.
      category = commands::SUGGESTION;
      break;
    case ConversionRequest::PARTIAL_SUGGESTION:
      category = commands::SUGGESTION;
      break;
    default:
      LOG(WARNING) << "Unknown request type: " << request_type_;
      category = commands::CONVERSION;
      break;
  }

  if (segment_index_ >= segments_.conversion_segments_size()) {
    LOG(WARNING) << "Invalid segment_index_: " << segment_index_
                 << ", segments_size: " << segments_.conversion_segments_size();
    return;
  }
  const Segment &segment = segments_.conversion_segment(segment_index_);
  output::FillAllCandidateWords(segment, candidate_list_, category, candidates);
}

void EngineConverter::FillIncognitoCandidateWords(
    commands::CandidateList *candidates) const {
  const Segment &segment =
      incognito_segments_.conversion_segment(segment_index_);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    commands::CandidateWord *candidate_word_proto =
        candidates->add_candidates();
    const converter::Candidate cand = segment.candidate(i);

    candidate_word_proto->set_id(i);
    candidate_word_proto->set_index(i);
    candidate_word_proto->set_key(cand.key);
    candidate_word_proto->set_value(cand.value);
  }
}

void EngineConverter::SetRequest(
    std::shared_ptr<const commands::Request> request) {
  DCHECK(request);
  request_ = std::move(request);
  candidate_list_.set_page_size(request_->candidate_page_size());
}

void EngineConverter::SetConfig(std::shared_ptr<const config::Config> config) {
  DCHECK(config);
  config_ = std::move(config);
  updated_command_ = converter::Candidate::DEFAULT_COMMAND;
  selection_shortcut_ = config_->selection_shortcut();
  use_cascading_window_ = config_->use_cascading_window();
}

void EngineConverter::OnStartComposition(const commands::Context &context) {
  bool revision_changed = false;
  if (context.has_revision()) {
    revision_changed = (context.revision() != client_revision_);
    client_revision_ = context.revision();
  }
  if (!context.has_preceding_text()) {
    // In this case, reset history segments when the revision is mismatched.
    if (revision_changed) {
      converter_->ResetConversion(&segments_);
    }
    return;
  }

  const std::string &preceding_text = context.preceding_text();
  // If preceding text is empty, it is OK to reset the history segments by
  // calling ResetConversion.
  if (preceding_text.empty()) {
    converter_->ResetConversion(&segments_);
    return;
  }

  // Hereafter, we keep the existing history segments as long as it is
  // consistent with the preceding text even when revision_changed is true.
  std::string history_text;
  for (const Segment &segment : segments_) {
    if (segment.segment_type() != Segment::HISTORY) {
      break;
    }
    if (segment.candidates_size() == 0) {
      break;
    }
    history_text.append(segment.candidate(0).value);
  }

  if (!history_text.empty()) {
    // Compare |preceding_text| with |history_text| to check if the history
    // segments are still valid or not.
    DCHECK(!preceding_text.empty());
    DCHECK(!history_text.empty());
    if (preceding_text.size() > history_text.size()) {
      if (preceding_text.ends_with(history_text)) {
        // History segments seem to be consistent with preceding text.
        return;
      }
    } else {
      if (history_text.ends_with(preceding_text)) {
        // History segments seem to be consistent with preceding text.
        return;
      }
    }
  }

  // Here we reconstruct history segments from |preceding_text| regardless
  // of revision mismatch. If it fails the history segments is cleared anyway.
  if (!converter_->ReconstructHistory(&segments_, preceding_text)) {
    LOG(WARNING) << "ReconstructHistory failed.";
    DLOG(WARNING) << "preceding_text: " << preceding_text
                  << ", segments: " << segments_.DebugString();
  }
}

void EngineConverter::UpdateSelectedCandidateIndex() {
  int index;
  const Candidate &focused_candidate = candidate_list_.focused_candidate();
  if (focused_candidate.HasSubcandidateList()) {
    const int t13n_index =
        focused_candidate.subcandidate_list().focused_index();
    index = -1 - t13n_index;
  } else {
    // TODO(hsumita): Use id instead of focused index.
    index = candidate_list_.focused_index();
  }
  selected_candidate_indices_[segment_index_] = index;
}

void EngineConverter::InitializeSelectedCandidateIndices() {
  selected_candidate_indices_.clear();
  selected_candidate_indices_.resize(segments_.conversion_segments_size());
}

void EngineConverter::UpdateCandidateStats(absl::string_view base_name,
                                           int32_t index) {
  std::string name;
  if (index < 0) {
    name = "TransliterationCandidates";
    index = -1 - index;
  } else {
    absl::StrAppend(&name, base_name, "Candidates");
  }

  if (index <= 9) {
    absl::StrAppend(&name, index);
  } else {
    name += "GE10";
  }
}

void EngineConverter::CommitSegmentsSize(
    EngineConverterInterface::State commit_state,
    const commands::Context &context) {
  size_t commit_segment_size = 0;
  switch (commit_state) {
    case COMPOSITION:
      commit_segment_size = 0;
      break;
    case SUGGESTION:
    case PREDICTION:
      commit_segment_size = 1;
      break;
    case CONVERSION:
      commit_segment_size = segments_.conversion_segments_size();
      break;
    default:
      LOG(DFATAL) << "Unexpected state: " << commit_state;
  }
  CommitSegmentsSize(commit_segment_size);
}

void EngineConverter::CommitSegmentsSize(size_t commit_segments_size) {
  CHECK_LE(commit_segments_size, selected_candidate_indices_.size());
  const auto it = selected_candidate_indices_.begin();
  selected_candidate_indices_.erase(it, it + commit_segments_size);
}

// Sets request type and update the engine_converter's state
void EngineConverter::SetRequestType(
    ConversionRequest::RequestType request_type,
    ConversionRequest::Options &options) {
  request_type_ = request_type;
  options.request_type = request_type;
}

}  // namespace engine
}  // namespace mozc
