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

#include "session/session_converter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "session/internal/candidate_list.h"
#include "session/internal/session_output.h"
#include "session/session_converter_interface.h"
#include "session/session_usage_stats_util.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"

namespace mozc {
namespace session {
namespace {

using ::mozc::commands::Request;
using ::mozc::config::Config;
using ::mozc::usage_stats::UsageStats;

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

// Make a segment having one candidate. The value of candidate is the
// same as the preedit.  This function can be used for error handling.
// When the converter fails, we can call this function to make a
// virtual segment.
void InitSegmentsFromString(std::string key, std::string preedit,
                            Segments *segments) {
  segments->clear_conversion_segments();
  Segment *segment = segments->add_segment();
  segment->set_key(key);
  segment->set_segment_type(Segment::FIXED_VALUE);
  Segment::Candidate *c = segment->add_candidate();
  c->value = preedit;
  c->content_value = std::move(preedit);
  c->key = key;
  c->content_key = std::move(key);
}
}  // namespace

SessionConverter::SessionConverter(const ConverterInterface *converter,
                                   const Request *request, const Config *config)
    : SessionConverterInterface(),
      converter_(converter),
      segments_(),
      incognito_segments_(),
      segment_index_(0),
      result_(),
      candidate_list_(true),
      request_(request),
      state_(COMPOSITION),
      request_type_(ConversionRequest::CONVERSION),
      client_revision_(0),
      candidate_list_visible_(false) {
  conversion_preferences_.use_history = true;
  conversion_preferences_.request_suggestion = true;
  candidate_list_.set_page_size(request->candidate_page_size());
  SetConfig(config);
}

bool SessionConverter::CheckState(
    SessionConverterInterface::States states) const {
  return ((state_ & states) != NO_STATE);
}

bool SessionConverter::IsActive() const {
  return CheckState(SUGGESTION | PREDICTION | CONVERSION);
}

const ConversionPreferences &SessionConverter::conversion_preferences() const {
  return conversion_preferences_;
}

bool SessionConverter::Convert(const composer::Composer &composer) {
  return ConvertWithPreferences(composer, conversion_preferences_);
}

bool SessionConverter::ConvertWithPreferences(
    const composer::Composer &composer,
    const ConversionPreferences &preferences) {
  DCHECK(CheckState(COMPOSITION | SUGGESTION | CONVERSION));

  const commands::Context context;
  DCHECK(request_);
  DCHECK(config_);
  ConversionRequest::Options options;
  options.enable_user_history_for_conversion = preferences.use_history;
  SetRequestType(ConversionRequest::CONVERSION, options);
  const ConversionRequest conversion_request(composer, *request_, context,
                                             *config_, std::move(options));

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

bool SessionConverter::GetReadingText(absl::string_view source_text,
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
      attributes = (HALF_WIDTH | ASCII);
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
      attributes = (FULL_WIDTH | ASCII);
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
}  // namespace

bool SessionConverter::ConvertToTransliteration(
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
        offset += Util::CharsLen(segment.key());
      }
      ResizeSegmentWidth(composer, offset);
    }

    DCHECK(CheckState(CONVERSION));
    candidate_list_.MoveToAttributes(query_attr);
  } else {
    DCHECK(CheckState(CONVERSION));
    const Attributes current_attr =
        candidate_list_.GetDeepestFocusedCandidate().attributes();

    if ((query_attr & current_attr & ASCII) &&
        ((((query_attr & HALF_WIDTH) && (current_attr & FULL_WIDTH))) ||
         (((query_attr & FULL_WIDTH) && (current_attr & HALF_WIDTH))))) {
      query_attr |= (current_attr & (UPPER | LOWER | CAPITALIZED));
    }

    candidate_list_.MoveNextAttributes(query_attr);
  }
  candidate_list_visible_ = false;
  // Treat as top conversion candidate on usage stats.
  selected_candidate_indices_[segment_index_] = 0;
  SegmentFocus();
  return true;
}

bool SessionConverter::ConvertToHalfWidth(const composer::Composer &composer) {
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

bool SessionConverter::SwitchKanaType(const composer::Composer &composer) {
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
      std::string composition;
      GetPreedit(0, segments_.conversion_segments_size(), &composition);
      const commands::Context context;
      DCHECK(request_);
      DCHECK(config_);
      const ConversionRequest conversion_request(composer, *request_, context,
                                                 *config_, {});
      if (!converter_->ResizeSegment(&segments_, conversion_request, 0,
                                     Util::CharsLen(composition))) {
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

namespace {

// Prepend the candidates to the first conversion segment.
void PrependCandidates(const Segment &previous_segment, std::string preedit,
                       Segments *segments) {
  DCHECK(segments);

  // TODO(taku) want to have a method in converter to make an empty segment
  if (segments->conversion_segments_size() == 0) {
    segments->clear_conversion_segments();
    Segment *segment = segments->add_segment();
    segment->set_key(std::move(preedit));
  }

  DCHECK_EQ(1, segments->conversion_segments_size());
  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);

  const size_t cands_size = previous_segment.candidates_size();
  for (size_t i = 0; i < cands_size; ++i) {
    Segment::Candidate *candidate = segment->push_front_candidate();
    *candidate = previous_segment.candidate(cands_size - i - 1);
  }
  segment->mutable_meta_candidates()->assign(
      previous_segment.meta_candidates().begin(),
      previous_segment.meta_candidates().end());
}
}  // namespace

bool SessionConverter::Suggest(const composer::Composer &composer,
                               const commands::Context &context) {
  return SuggestWithPreferences(composer, context, conversion_preferences_);
}

bool SessionConverter::SuggestWithPreferences(
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
  const ConversionRequest conversion_request(composer, *request_, context,
                                             *config_, std::move(options));

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
    const Config incognito_config = CreateIncognitoConfig();
    ConversionRequest::Options incognito_options = conversion_request.options();
    incognito_options.enable_user_history_for_conversion = false;
    incognito_options.request_type = use_partial_composition
                                         ? ConversionRequest::PARTIAL_SUGGESTION
                                         : ConversionRequest::SUGGESTION;
    const ConversionRequest incognito_conversion_request =
        ConversionRequestBuilder()
            .SetConversionRequest(conversion_request)
            .SetConfig(incognito_config)
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

bool SessionConverter::Predict(const composer::Composer &composer) {
  return PredictWithPreferences(composer, conversion_preferences_);
}

bool SessionConverter::IsEmptySegment(const Segment &segment) const {
  return ((segment.candidates_size() == 0) &&
          (segment.meta_candidates_size() == 0));
}

bool SessionConverter::PredictWithPreferences(
    const composer::Composer &composer,
    const ConversionPreferences &preferences) {
  // TODO(komatsu): DCHECK should be
  // DCHECK(CheckState(COMPOSITION | SUGGESTION | PREDICTION));
  DCHECK(CheckState(COMPOSITION | SUGGESTION | CONVERSION | PREDICTION));
  ResetResult();

  // Initialize the segments and conversion_request for prediction
  ConversionRequest::Options options;
  options.enable_user_history_for_conversion = preferences.use_history;
  const commands::Context context;
  DCHECK(request_);
  DCHECK(config_);
  SetRequestType(ConversionRequest::PREDICTION, options);
  options.use_actual_converter_for_realtime_conversion = true;
  const ConversionRequest conversion_request(composer, *request_, context,
                                             *config_, std::move(options));

  const bool predict_first =
      !CheckState(PREDICTION) && IsEmptySegment(previous_suggestions_);

  const bool predict_expand =
      (CheckState(PREDICTION) && !IsEmptySegment(previous_suggestions_) &&
       candidate_list_.size() > 0 && candidate_list_.focused() &&
       candidate_list_.focused_index() == candidate_list_.last_index());

  segments_.clear_conversion_segments();

  if (predict_expand || predict_first) {
    if (!converter_->StartPrediction(conversion_request, &segments_)) {
      LOG(WARNING) << "StartPrediction() failed";
      // TODO(komatsu): Perform refactoring after checking the stability test.
      //
      // If predict_expand is true, it means we have prevous_suggestions_.
      // So we can use it as the result of this prediction.
      if (predict_first) {
        ResetState();
        return false;
      }
    }
  }

  // Merge suggestions and prediction
  std::string preedit = composer.GetStringForPreedit();
  PrependCandidates(previous_suggestions_, std::move(preedit), &segments_);

  segment_index_ = 0;
  state_ = PREDICTION;
  UpdateCandidateList();
  candidate_list_visible_ = true;
  InitializeSelectedCandidateIndices();

  return true;
}

void SessionConverter::MaybeExpandPrediction(
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

void SessionConverter::Cancel() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  ResetResult();

  // Clear segments and keep the context
  converter_->CancelConversion(&segments_);
  ResetState();
}

void SessionConverter::Reset() {
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

void SessionConverter::Commit(const composer::Composer &composer,
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
  CommitUsageStats(state_, context);
  DCHECK(request_);
  DCHECK(config_);
  const ConversionRequest conversion_request(composer, *request_, context,
                                             *config_, {});
  converter_->FinishConversion(conversion_request, &segments_);
  ResetState();
}

bool SessionConverter::CommitSuggestionInternal(
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
    CommitUsageStats(SessionConverterInterface::SUGGESTION, context);
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
    CommitUsageStats(SessionConverterInterface::SUGGESTION, context);
    DCHECK(config_);
    const ConversionRequest conversion_request(composer, *request_, context,
                                               *config_, {});
    converter_->FinishConversion(conversion_request, &segments_);
    DCHECK_EQ(0, segments_.conversion_segments_size());
    ResetState();
  }
  return true;
}

bool SessionConverter::CommitSuggestionByIndex(
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

bool SessionConverter::CommitSuggestionById(const int id,
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

void SessionConverter::CommitHeadToFocusedSegments(
    const composer::Composer &composer, const commands::Context &context,
    size_t *consumed_key_size) {
  CommitSegmentsInternal(composer, context, segment_index_ + 1,
                         consumed_key_size);
}

void SessionConverter::CommitFirstSegment(const composer::Composer &composer,
                                          const commands::Context &context,
                                          size_t *consumed_key_size) {
  CommitSegmentsInternal(composer, context, 1, consumed_key_size);
}

void SessionConverter::CommitSegmentsInternal(
    const composer::Composer &composer, const commands::Context &context,
    size_t segments_to_commit, size_t *consumed_key_size) {
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
    *consumed_key_size += Util::CharsLen(segment.key());

    // Collect candidate's id for each segment.
    candidate_ids.push_back(GetCandidateIndexForConverter(i));
  }
  if (!converter_->CommitSegments(&segments_, candidate_ids)) {
    LOG(WARNING) << "CommitSegments failed";
  }

  // Commit the [0, segments_to_commit - 1] conversion segment.
  CommitUsageStatsWithSegmentsSize(state_, context, segments_to_commit);

  // Adjust the segment_index, since the [0, segment_to_commit - 1] segments
  // disappeared.
  // Note that segment_index_ is unsigned.
  segment_index_ = segment_index_ > segments_to_commit
                       ? segment_index_ - segments_to_commit
                       : 0;
  UpdateCandidateList();
}

void SessionConverter::CommitPreedit(const composer::Composer &composer,
                                     const commands::Context &context) {
  const std::string key = composer.GetQueryForConversion();
  const std::string preedit = composer.GetStringForSubmission();
  std::string normalized_preedit = TextNormalizer::NormalizeText(preedit);
  SessionOutput::FillPreeditResult(preedit, &result_);

  // Add ResultToken
  commands::ResultToken *token = result_.add_tokens();
  token->set_key(preedit);
  token->set_value(preedit);

  // Cursor offset needs to be calculated based on normalized text.
  SessionOutput::FillCursorOffsetResult(
      CalculateCursorOffset(normalized_preedit), &result_);
  InitSegmentsFromString(std::move(key), std::move(normalized_preedit),
                         &segments_);
  CommitUsageStats(SessionConverterInterface::COMPOSITION, context);
  DCHECK(request_);
  DCHECK(config_);
  // the request mode is CONVERSION, as the user experience
  // is similar to conversion. UserHistoryPredictor distinguishes
  // CONVERSION from SUGGESTION now.
  ConversionRequest::Options options;
  SetRequestType(ConversionRequest::CONVERSION, options);
  const ConversionRequest conversion_request(composer, *request_, context,
                                             *config_, std::move(options));
  converter_->FinishConversion(conversion_request, &segments_);
  ResetState();
}

void SessionConverter::CommitHead(size_t count,
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
  SessionOutput::FillPreeditResult(composition, &result_);
  SessionOutput::FillCursorOffsetResult(CalculateCursorOffset(composition),
                                        &result_);
}

void SessionConverter::Revert() { converter_->RevertConversion(&segments_); }

bool SessionConverter::DeleteCandidateFromHistory(std::optional<int> id) {
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

void SessionConverter::SegmentFocusInternal(size_t index) {
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

void SessionConverter::SegmentFocusRight() {
  if (segment_index_ + 1 >= segments_.conversion_segments_size()) {
    // If |segment_index_| is at the tail of the segments,
    // focus on the head.
    SegmentFocusLeftEdge();
  } else {
    SegmentFocusInternal(segment_index_ + 1);
  }
}

void SessionConverter::SegmentFocusLast() {
  const size_t r_edge = segments_.conversion_segments_size() - 1;
  SegmentFocusInternal(r_edge);
}

void SessionConverter::SegmentFocusLeft() {
  if (segment_index_ <= 0) {
    // If |segment_index_| is at the head of the segments,
    // focus on the tail.
    SegmentFocusLast();
  } else {
    SegmentFocusInternal(segment_index_ - 1);
  }
}

void SessionConverter::SegmentFocusLeftEdge() { SegmentFocusInternal(0); }

void SessionConverter::ResizeSegmentWidth(const composer::Composer &composer,
                                          int delta) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  candidate_list_visible_ = false;
  if (CheckState(PREDICTION)) {
    return;  // Do nothing.
  }
  ResetResult();

  const commands::Context context;
  DCHECK(request_);
  DCHECK(config_);
  const ConversionRequest conversion_request(composer, *request_, context,
                                             *config_, {});
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

void SessionConverter::SegmentWidthExpand(const composer::Composer &composer) {
  ResizeSegmentWidth(composer, 1);
}

void SessionConverter::SegmentWidthShrink(const composer::Composer &composer) {
  ResizeSegmentWidth(composer, -1);
}

void SessionConverter::CandidateNext(const composer::Composer &composer) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  MaybeExpandPrediction(composer);
  candidate_list_.MoveNext();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void SessionConverter::CandidateNextPage() {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_.MoveNextPage();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void SessionConverter::CandidatePrev() {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_.MovePrev();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void SessionConverter::CandidatePrevPage() {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_.MovePrevPage();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void SessionConverter::CandidateMoveToId(const int id,
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

void SessionConverter::CandidateMoveToPageIndex(const size_t index) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_.MoveToPageIndex(index);
  candidate_list_visible_ = false;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

bool SessionConverter::CandidateMoveToShortcut(const char shortcut) {
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

void SessionConverter::SetCandidateListVisible(bool visible) {
  candidate_list_visible_ = visible;
}

void SessionConverter::PopOutput(const composer::Composer &composer,
                                 commands::Output *output) {
  FillOutput(composer, output);
  updated_command_ = Segment::Candidate::DEFAULT_COMMAND;
  ResetResult();
}

namespace {
void MaybeFillConfig(Segment::Candidate::Command command,
                     const config::Config &base_config,
                     commands::Output *output) {
  if (command == Segment::Candidate::DEFAULT_COMMAND) {
    return;
  }

  *output->mutable_config() = base_config;
  switch (command) {
    case Segment::Candidate::ENABLE_INCOGNITO_MODE:
      output->mutable_config()->set_incognito_mode(true);
      break;
    case Segment::Candidate::DISABLE_INCOGNITO_MODE:
      output->mutable_config()->set_incognito_mode(false);
      break;
    case Segment::Candidate::ENABLE_PRESENTATION_MODE:
      output->mutable_config()->set_presentation_mode(true);
      break;
    case Segment::Candidate::DISABLE_PRESENTATION_MODE:
      output->mutable_config()->set_presentation_mode(false);
      break;
    default:
      LOG(WARNING) << "Unknown command: " << command;
      break;
  }
}
}  // namespace

void SessionConverter::FillOutput(const composer::Composer &composer,
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
      session::SessionOutput::FillPreedit(composer, output->mutable_preedit());
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
      session::SessionOutput::FillPreedit(composer, output->mutable_preedit());
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
    SessionOutput::FillRemovedCandidates(
        segments_.conversion_segment(segment_index_),
        output->mutable_removed_candidate_words_for_debug());
  }
}

SessionConverter *SessionConverter::Clone() const {
  SessionConverter *session_converter =
      new SessionConverter(converter_, request_, config_);

  // Copy the members in order of their declarations.
  session_converter->state_ = state_;
  // TODO(team): copy of |converter_| member.
  // We cannot copy the member converter_ from SessionConverterInterface because
  // it doesn't (and shouldn't) define a method like GetConverter(). At the
  // moment it's ok because the current design guarantees that the converter is
  // singleton. However, we should refactor such bad design; see also the
  // comment right above.
  session_converter->segments_ = segments_;
  session_converter->incognito_segments_ = incognito_segments_;
  session_converter->segment_index_ = segment_index_;
  session_converter->previous_suggestions_ = previous_suggestions_;
  session_converter->conversion_preferences_ = conversion_preferences();
  session_converter->result_ = result_;
  session_converter->request_ = request_;
  session_converter->config_ = config_;
  session_converter->use_cascading_window_ = use_cascading_window_;
  session_converter->selected_candidate_indices_ = selected_candidate_indices_;
  session_converter->request_type_ = request_type_;

  if (session_converter->CheckState(SUGGESTION | PREDICTION | CONVERSION)) {
    // UpdateCandidateList() is not simple setter and it uses some members.
    session_converter->UpdateCandidateList();
    session_converter->candidate_list_.MoveToId(candidate_list_.focused_id());
    session_converter->SetCandidateListVisible(candidate_list_visible_);
  }

  return session_converter;
}

void SessionConverter::ResetResult() { result_.Clear(); }

void SessionConverter::ResetState() {
  state_ = COMPOSITION;
  segment_index_ = 0;
  previous_suggestions_.clear();
  candidate_list_visible_ = false;
  candidate_list_.Clear();
  selected_candidate_indices_.clear();
  incognito_segments_.Clear();
}

void SessionConverter::SegmentFocus() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  if (!converter_->FocusSegmentValue(
          &segments_, segment_index_,
          GetCandidateIndexForConverter(segment_index_))) {
    LOG(ERROR) << "FocusSegmentValue failed";
  }
}

void SessionConverter::SegmentFix() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  if (!converter_->CommitSegmentValue(
          &segments_, segment_index_,
          GetCandidateIndexForConverter(segment_index_))) {
    LOG(WARNING) << "CommitSegmentValue failed";
  }
}

void SessionConverter::GetPreedit(const size_t index, const size_t size,
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

void SessionConverter::GetConversion(const size_t index, const size_t size,
                                     std::string *conversion) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  DCHECK(index + size <= segments_.conversion_segments_size());
  DCHECK(conversion);

  conversion->clear();
  for (size_t i = index; i < size; ++i) {
    conversion->append(GetSelectedCandidateValue(i));
  }
}

void SessionConverter::UpdateResultTokens(const size_t index,
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
    const Segment::Candidate &candidate =
        segments_.conversion_segment(i).candidate(cand_idx);
    const int first_token_idx = result_.tokens_size();

    if (Segment::Candidate::InnerSegmentIterator it(&candidate); !it.Done()) {
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

size_t SessionConverter::GetConsumedPreeditSize(const size_t index,
                                                const size_t size) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  DCHECK(index + size <= segments_.conversion_segments_size());

  if (CheckState(SUGGESTION | PREDICTION)) {
    DCHECK_EQ(1, size);
    const Segment &segment = segments_.conversion_segment(0);
    const int id = GetCandidateIndexForConverter(0);
    const Segment::Candidate &candidate = segment.candidate(id);
    return (candidate.attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED)
               ? candidate.consumed_key_size
               : kConsumedAllCharacters;
  }

  DCHECK(CheckState(CONVERSION));
  size_t result = 0;
  for (size_t i = index; i < size; ++i) {
    const int id = GetCandidateIndexForConverter(i);
    const Segment::Candidate &candidate =
        segments_.conversion_segment(i).candidate(id);
    DCHECK(
        !(candidate.attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED));
    result += Util::CharsLen(segments_.conversion_segment(i).key());
  }
  return result;
}

bool SessionConverter::MaybePerformCommandCandidate(const size_t index,
                                                    const size_t size) {
  // If a candidate has the command attribute, Cancel is performed
  // instead of Commit after executing the specified action.
  for (size_t i = index; i < size; ++i) {
    const int id = GetCandidateIndexForConverter(i);
    const Segment::Candidate &candidate =
        segments_.conversion_segment(i).candidate(id);
    if (candidate.attributes & Segment::Candidate::COMMAND_CANDIDATE) {
      switch (candidate.command) {
        case Segment::Candidate::DEFAULT_COMMAND:
          // Do nothing
          break;
        case Segment::Candidate::ENABLE_INCOGNITO_MODE:
        case Segment::Candidate::DISABLE_INCOGNITO_MODE:
        case Segment::Candidate::ENABLE_PRESENTATION_MODE:
        case Segment::Candidate::DISABLE_PRESENTATION_MODE:
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

bool SessionConverter::UpdateResult(size_t index, size_t size,
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
  SessionOutput::FillConversionResult(preedit, conversion, &result_);
  SessionOutput::FillCursorOffsetResult(CalculateCursorOffset(conversion),
                                        &result_);
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

void SessionConverter::AppendCandidateList() {
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
      [](const Segment::Candidate &c) -> const std::string & {
    return c.value;
  };

  for (size_t i = candidate_list_.next_available_id();
       i < segment.candidates_size(); ++i) {
    const Segment::Candidate &c = segment.candidate(i);
    candidate_list_.AddCandidate(i, get_candidate_dedup_key(c));
    // if candidate has spelling correction attribute,
    // always display the candidate to let user know the
    // miss spelled candidate.
    if (i < 10 && (segment.candidate(i).attributes &
                   Segment::Candidate::SPELLING_CORRECTION)) {
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
    constexpr bool kNoRotate = false;
    transliterations = candidate_list_.AllocateSubCandidateList(kNoRotate);
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

void SessionConverter::UpdateCandidateList() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  candidate_list_.Clear();
  AppendCandidateList();
}

int SessionConverter::GetCandidateIndexForConverter(
    const size_t segment_index) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  // If segment_index does not point to the focused segment, the value
  // should be always zero.
  if (segment_index != segment_index_) {
    return 0;
  }
  return candidate_list_.focused_id();
}

std::string SessionConverter::GetSelectedCandidateValue(
    const size_t segment_index) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  const int id = GetCandidateIndexForConverter(segment_index);
  const Segment::Candidate &candidate =
      segments_.conversion_segment(segment_index).candidate(id);
  if (candidate.attributes & Segment::Candidate::COMMAND_CANDIDATE) {
    // Return an empty string, however this path should not be reached.
    return "";
  }
  return candidate.value;
}

const Segment::Candidate &SessionConverter::GetSelectedCandidate(
    const size_t segment_index) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  const int id = GetCandidateIndexForConverter(segment_index);
  return segments_.conversion_segment(segment_index).candidate(id);
}

void SessionConverter::FillConversion(commands::Preedit *preedit) const {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  SessionOutput::FillConversion(segments_, segment_index_,
                                candidate_list_.focused_id(), preedit);
}

void SessionConverter::FillResult(commands::Result *result) const {
  *result = result_;
}

void SessionConverter::FillCandidateWindow(
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
  SessionOutput::FillCandidateWindow(segment, candidate_list_, position,
                                     candidate_window);

  // Shortcut keys
  if (CheckState(PREDICTION | CONVERSION)) {
    SessionOutput::FillShortcuts(GetCandidateShortcuts(selection_shortcut_),
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
  SessionOutput::FillFooter(candidate_window->category(), candidate_window);
}

void SessionConverter::FillAllCandidateWords(
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
  SessionOutput::FillAllCandidateWords(segment, candidate_list_, category,
                                       candidates);
}

void SessionConverter::FillIncognitoCandidateWords(
    commands::CandidateList *candidates) const {
  const Segment &segment =
      incognito_segments_.conversion_segment(segment_index_);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    commands::CandidateWord *candidate_word_proto =
        candidates->add_candidates();
    const Segment::Candidate cand = segment.candidate(i);

    candidate_word_proto->set_id(i);
    candidate_word_proto->set_index(i);
    candidate_word_proto->set_key(cand.key);
    candidate_word_proto->set_value(cand.value);
  }
}

void SessionConverter::SetRequest(const commands::Request *request) {
  request_ = request;
  candidate_list_.set_page_size(request->candidate_page_size());
}

void SessionConverter::SetConfig(const config::Config *config) {
  config_ = config;
  updated_command_ = Segment::Candidate::DEFAULT_COMMAND;
  selection_shortcut_ = config->selection_shortcut();
  use_cascading_window_ = config->use_cascading_window();
}

void SessionConverter::OnStartComposition(const commands::Context &context) {
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
      if (absl::EndsWith(preceding_text, history_text)) {
        // History segments seem to be consistent with preceding text.
        return;
      }
    } else {
      if (absl::EndsWith(history_text, preceding_text)) {
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

void SessionConverter::UpdateSelectedCandidateIndex() {
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

void SessionConverter::InitializeSelectedCandidateIndices() {
  selected_candidate_indices_.clear();
  selected_candidate_indices_.resize(segments_.conversion_segments_size());
}

void SessionConverter::UpdateCandidateStats(absl::string_view base_name,
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
  UsageStats::IncrementCount(name);
}

void SessionConverter::CommitUsageStats(
    SessionConverterInterface::State commit_state,
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
  CommitUsageStatsWithSegmentsSize(commit_state, context, commit_segment_size);
}

void SessionConverter::CommitUsageStatsWithSegmentsSize(
    SessionConverterInterface::State commit_state,
    const commands::Context &context, size_t commit_segments_size) {
  CHECK_LE(commit_segments_size, selected_candidate_indices_.size());

  std::string stats_str;
  switch (commit_state) {
    case COMPOSITION:
      stats_str = "Composition";
      break;
    case SUGGESTION:
    case PREDICTION:
      // Suggestion related usage stats are collected as Prediction.
      stats_str = "Prediction";
      UpdateCandidateStats(stats_str, selected_candidate_indices_[0]);
      break;
    case CONVERSION:
      stats_str = "Conversion";
      for (size_t i = 0; i < commit_segments_size; ++i) {
        UpdateCandidateStats(stats_str, selected_candidate_indices_[i]);
      }
      break;
    default:
      LOG(DFATAL) << "Unexpected state: " << commit_state;
      stats_str = "Unknown";
  }

  UsageStats::IncrementCount("Commit");
  UsageStats::IncrementCount("CommitFrom" + stats_str);

  if (stats_str != "Unknown") {
    if (SessionUsageStatsUtil::HasExperimentalFeature(context,
                                                      "chrome_omnibox")) {
      UsageStats::IncrementCount("CommitFrom" + stats_str + "InChromeOmnibox");
    }
    if (SessionUsageStatsUtil::HasExperimentalFeature(context,
                                                      "google_search_box")) {
      UsageStats::IncrementCount("CommitFrom" + stats_str +
                                 "InGoogleSearchBox");
    }
  }

  const std::vector<int>::iterator it = selected_candidate_indices_.begin();
  selected_candidate_indices_.erase(it, it + commit_segments_size);
}

// Sets request type and update the session_converter's state
void SessionConverter::SetRequestType(
    ConversionRequest::RequestType request_type,
    ConversionRequest::Options &options) {
  request_type_ = request_type;
  options.request_type = request_type;
}

Config SessionConverter::CreateIncognitoConfig() {
  Config ret = *config_;
  ret.set_incognito_mode(true);
  return ret;
}

}  // namespace session
}  // namespace mozc
