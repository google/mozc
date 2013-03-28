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

// A class handling the converter on the session layer.

#include "session/session_converter.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/base.h"
#include "base/logging.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "composer/composer.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/converter_util.h"
#include "converter/segments.h"
#include "session/commands.pb.h"
#include "session/internal/candidate_list.h"
#include "session/internal/session_output.h"
#include "session/session_usage_stats_util.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"

using mozc::usage_stats::UsageStats;

#ifdef OS_ANDROID
const bool kDefaultUseActualConverterForRealtimeConversion = false;
#else
const bool kDefaultUseActualConverterForRealtimeConversion = true;
#endif  // OS_ANDROID

DEFINE_bool(use_actual_converter_for_realtime_conversion,
            kDefaultUseActualConverterForRealtimeConversion,
            "If true, use the actual (non-immutable) converter for real "
            "time conversion.");

namespace mozc {
namespace session {

namespace {

using mozc::commands::Request;
using mozc::config::Config;
using mozc::config::ConfigHandler;

const size_t kDefaultMaxHistorySize = 3;

void SetPresentationMode(bool enabled) {
  Config config;
  ConfigHandler::GetConfig(&config);
  config.set_presentation_mode(enabled);
  ConfigHandler::SetConfig(config);
}

void SetIncognitoMode(bool enabled) {
  Config config;
  ConfigHandler::GetConfig(&config);
  config.set_incognito_mode(enabled);
  ConfigHandler::SetConfig(config);
}

}  // namespace

const size_t SessionConverter::kConsumedAllCharacters =
    numeric_limits<size_t>::max();

SessionConverter::SessionConverter(const ConverterInterface *converter,
                                   const Request *request)
    : SessionConverterInterface(),
      state_(COMPOSITION),
      converter_(converter),
      segments_(new Segments),
      segment_index_(0),
      result_(new commands::Result),
      candidate_list_(new CandidateList(true)),
      candidate_list_visible_(false),
      request_(request) {
  conversion_preferences_.use_history = true;
  conversion_preferences_.max_history_size = kDefaultMaxHistorySize;
  conversion_preferences_.request_suggestion = true;
  operation_preferences_.use_cascading_window = true;
  operation_preferences_.candidate_shortcuts.clear();
}

SessionConverter::~SessionConverter() {}

void SessionConverter::SetOperationPreferences(
    const OperationPreferences &preferences) {
  operation_preferences_.use_cascading_window =
      preferences.use_cascading_window;
  operation_preferences_.candidate_shortcuts =
      preferences.candidate_shortcuts;
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

  segments_->set_request_type(Segments::CONVERSION);
  SetConversionPreferences(preferences, segments_.get());

  const ConversionRequest conversion_request(&composer, request_);
  if (!converter_->StartConversionForRequest(conversion_request,
                                             segments_.get())) {
    LOG(WARNING) << "StartConversionForRequest() failed";
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

bool SessionConverter::GetReadingText(const string &source_text,
                                      string *reading) {
  DCHECK(reading);
  reading->clear();
  Segments reverse_segments;
  if (!converter_->StartReverseConversion(&reverse_segments, source_text)) {
    return false;
  }
  if (reverse_segments.segments_size() == 0) {
    LOG(WARNING) << "no segments from reverse conversion";
    return false;
  }
  for (size_t i = 0; i < reverse_segments.segments_size(); ++i) {
    const mozc::Segment &segment = reverse_segments.segment(i);
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
    if (segments_->conversion_segments_size() != 1) {
      string composition;
      GetPreedit(0, segments_->conversion_segments_size(), &composition);
      ResizeSegmentWidth(composer, Util::CharsLen(composition));
    }

    DCHECK(CheckState(CONVERSION));
    candidate_list_->MoveToAttributes(query_attr);
  } else {
    DCHECK(CheckState(CONVERSION));
    const Attributes current_attr =
        candidate_list_->GetDeepestFocusedCandidate().attributes();

    if ((query_attr & current_attr & ASCII) &&
        ((((query_attr & HALF_WIDTH) && (current_attr & FULL_WIDTH))) ||
         (((query_attr & FULL_WIDTH) && (current_attr & HALF_WIDTH))))) {
      query_attr |= (current_attr & (UPPER | LOWER | CAPITALIZED));
    }

    candidate_list_->MoveNextAttributes(query_attr);
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

  string composition;
  if (CheckState(COMPOSITION | SUGGESTION)) {
    if (!Convert(composer)) {
      LOG(ERROR) << "Conversion failed";
      return false;
    }
    GetPreedit(0, segments_->conversion_segments_size(), &composition);
    // TODO(komatsu): This is a workaround to transliterate the whole
    // preedit as a single segment.  We should modify
    // converter/converter.cc to enable to accept mozc::Segment::FIXED
    // from the session layer.
    if (segments_->conversion_segments_size() != 1) {
      ResizeSegmentWidth(composer, Util::CharsLen(composition));
    }
  } else {
    composition = GetSelectedCandidate(segment_index_).value;
  }

  DCHECK(CheckState(CONVERSION));
  Attributes attributes = HALF_WIDTH;
  // TODO(komatsu): make a function to return a logical sum of ScriptType.
  // If composition_ is "あｂｃ", it should be treated as Katakana.
  if (Util::ContainsScriptType(composition, Util::KATAKANA) ||
      Util::ContainsScriptType(composition, Util::HIRAGANA) ||
      Util::ContainsScriptType(composition, Util::KANJI) ||
      Util::IsKanaSymbolContained(composition)
  ) {
    attributes |= KATAKANA;
  } else {
    attributes |= ASCII;
    attributes |= (candidate_list_->GetDeepestFocusedCandidate().attributes() &
                   (UPPER | LOWER | CAPITALIZED));
  }
  candidate_list_->MoveNextAttributes(attributes);
  candidate_list_visible_ = false;
  // Treat as top conversion candidate on usage stats.
  selected_candidate_indices_[segment_index_] = 0;
  SegmentFocus();
  return true;
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
    if (segments_->conversion_segments_size() != 1) {
      string composition;
      GetPreedit(0, segments_->conversion_segments_size(), &composition);
      const ConversionRequest conversion_request(&composer, request_);
      converter_->ResizeSegment(segments_.get(),
                                conversion_request,
                                0, Util::CharsLen(composition));
      UpdateCandidateList();
    }

    attributes = (FULL_WIDTH | KATAKANA);
  } else {
    const Attributes current_attributes =
        candidate_list_->GetDeepestFocusedCandidate().attributes();
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
  candidate_list_->MoveNextAttributes(attributes);
  candidate_list_visible_ = false;
  // Treat as top conversion candidate on usage stats.
  selected_candidate_indices_[segment_index_] = 0;
  SegmentFocus();
  return true;
}

namespace {

// Prepend the candidates to the first conversion segment.
void PrependCandidates(const Segment &previous_segment,
                       const string &preedit,
                       Segments *segments) {
  DCHECK(segments);

  // TODO(taku) want to have a method in converter to make an empty segment
  if (segments->conversion_segments_size() == 0) {
    segments->clear_conversion_segments();
    Segment *segment = segments->add_segment();
    segment->Clear();
    segment->set_key(preedit);
  }

  DCHECK_EQ(1, segments->conversion_segments_size());
  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);

  const size_t cands_size = previous_segment.candidates_size();
  for (size_t i = 0; i < cands_size; ++i) {
    Segment::Candidate *candidate = segment->push_front_candidate();
    candidate->CopyFrom(previous_segment.candidate(cands_size - i - 1));
  }
  *(segment->mutable_meta_candidates()) = previous_segment.meta_candidates();
}
}  // namespace


bool SessionConverter::Suggest(const composer::Composer &composer) {
  return SuggestWithPreferences(composer, conversion_preferences_);
}

bool SessionConverter::SuggestWithPreferences(
    const composer::Composer &composer,
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

  // Initialize the segments for suggestion.
  SetConversionPreferences(preferences, segments_.get());

  ConversionRequest conversion_request(&composer, request_);

  const size_t cursor = composer.GetCursor();
  if (cursor == composer.GetLength() || cursor == 0 ||
      !request_->mixed_conversion()) {
    conversion_request.set_create_partial_candidates(
        request_->auto_partial_suggestion());
    conversion_request.set_use_actual_converter_for_realtime_conversion(
        FLAGS_use_actual_converter_for_realtime_conversion);
    if (!converter_->StartSuggestionForRequest(conversion_request,
                                               segments_.get())) {
      // TODO(komatsu): Because suggestion is a prefix search, once
      // StartSuggestion returns false, this GetSuggestion always
      // returns false.  Refactor it.
      VLOG(1) << "StartSuggestionForRequest() returns no suggestions.";
      // Clear segments and keep the context
      converter_->CancelConversion(segments_.get());
      return false;
    }
  } else {
    // create_partial_candidates is false because auto partial suggestion
    // should be activated only when the cursor is at the tail or head from
    // the view point of UX.
    // use_actual_converter_for_realtime_conversion is also false because of
    // implementation reason. If the flag is true, all the composition
    // characters will be used in the below process, which conflicts
    // with *partial* prediction.
    if (!converter_->StartPartialSuggestionForRequest(conversion_request,
                                                      segments_.get())) {
      VLOG(1) << "StartPartialSuggestionForRequest() returns no suggestions.";
      // Clear segments and keep the context
      converter_->CancelConversion(segments_.get());
      return false;
    }
  }
  DCHECK_EQ(1, segments_->conversion_segments_size());

  // Copy current suggestions so that we can merge
  // prediction/suggestions later
  previous_suggestions_.CopyFrom(segments_->conversion_segment(0));

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

  // Initialize the segments for prediction
  segments_->set_request_type(Segments::PREDICTION);
  SetConversionPreferences(preferences, segments_.get());

  const bool predict_first =
      !CheckState(PREDICTION) && IsEmptySegment(previous_suggestions_);

  const bool predict_expand =
      (CheckState(PREDICTION) &&
       !IsEmptySegment(previous_suggestions_) &&
       candidate_list_->size() > 0 &&
       candidate_list_->focused() &&
       candidate_list_->focused_index() == candidate_list_->last_index());

  segments_->clear_conversion_segments();

  if (predict_expand || predict_first) {
    ConversionRequest conversion_request(&composer, request_);
    conversion_request.set_use_actual_converter_for_realtime_conversion(
        FLAGS_use_actual_converter_for_realtime_conversion);
    if (!converter_->StartPredictionForRequest(conversion_request,
                                               segments_.get())) {
      LOG(WARNING) << "StartPredictionForRequest() failed";

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
  string preedit;
  composer.GetQueryForPrediction(&preedit);
  PrependCandidates(previous_suggestions_, preedit, segments_.get());

  segment_index_ = 0;
  state_ = PREDICTION;
  UpdateCandidateList();
  candidate_list_visible_ = true;
  InitializeSelectedCandidateIndices();

  return true;
}

bool SessionConverter::ExpandSuggestion(const composer::Composer &composer) {
  return ExpandSuggestionWithPreferences(composer, conversion_preferences_);
}

bool SessionConverter::ExpandSuggestionWithPreferences(
    const composer::Composer &composer,
    const ConversionPreferences &preferences) {
  DCHECK(CheckState(COMPOSITION | SUGGESTION | PREDICTION));
  if (CheckState(COMPOSITION)) {
    // Client can send EXPAND_SUGGESTION command when on composition mode.
    // In such case we do nothing.
    VLOG(1) << "ExpandSuggestion does nothing on composition mode.";
    return false;
  }

  ResetResult();

  // Expand suggestion.
  // Current implementation is hacky.
  // We want prediction candidates,
  // but want to set candidates' category SUGGESTION.
  // TODO(matsuzakit or yamaguchi): Refactor following lines,
  //     after implemention of partial conversion.

  // Initialize the segments for prediction.
  SetConversionPreferences(preferences, segments_.get());

  string preedit;
  composer.GetQueryForPrediction(&preedit);

  // We do not need "segments_->clear_conversion_segments()".
  // Without this statement we can add additional candidates into
  // existing segments.

  ConversionRequest conversion_request(&composer, request_);

  const size_t cursor = composer.GetCursor();
  if (cursor == composer.GetLength() || cursor == 0 ||
      !request_->mixed_conversion()) {
    conversion_request.set_create_partial_candidates(
        request_->auto_partial_suggestion());
    conversion_request.set_use_actual_converter_for_realtime_conversion(
        FLAGS_use_actual_converter_for_realtime_conversion);
    // This is abuse of StartPrediction().
    // TODO(matsuzakit or yamaguchi): Add ExpandSuggestion method
    //    to Converter class.
    if (!converter_->StartPredictionForRequest(conversion_request,
                                               segments_.get())) {
      LOG(WARNING) << "StartPredictionForRequest() failed";
    }
  } else {
    // c.f. SuggestWithPreferences for ConversionRequest flags.
    if (!converter_->StartPartialPredictionForRequest(conversion_request,
                                                      segments_.get())) {
      VLOG(1) << "StartPartialPredictionForRequest() returns no suggestions.";
      // Clear segments and keep the context
      converter_->CancelConversion(segments_.get());
      return false;
    }
  }
  // Overwrite the request type to SUGGESTION.
  // Without this logic, a candidate gets focused that is unexpected behavior.
  segments_->set_request_type(Segments::SUGGESTION);

  // Merge suggestions and predictions.
  PrependCandidates(previous_suggestions_, preedit, segments_.get());

  segment_index_ = 0;
  // Call AppendCandidateList instead of UpdateCandidateList because
  // we want to keep existing candidates.
  // As a result, ExpandSuggestionWithPreferences adds expanded suggestion
  // candidates at the tail of existing candidates.
  AppendCandidateList();
  candidate_list_visible_ = true;
  return true;
}

void SessionConverter::MaybeExpandPrediction(
    const composer::Composer &composer) {
  DCHECK(CheckState(PREDICTION | CONVERSION));

  // Expand the current suggestions and fill with Prediction results.
  if (!CheckState(PREDICTION) ||
      IsEmptySegment(previous_suggestions_) ||
      !candidate_list_->focused() ||
      candidate_list_->focused_index() != candidate_list_->last_index()) {
    return;
  }

  DCHECK(CheckState(PREDICTION));
  ResetResult();

  const size_t previous_index = candidate_list_->focused_index();
  if (!PredictWithPreferences(composer, conversion_preferences_)) {
    // TODO(komatsu): Consider the case when PredictWithPreferences fails.
    return;
  }

  if (previous_index < candidate_list_->size()) {
    candidate_list_->MoveToId(candidate_list_->candidate(previous_index).id());
    UpdateSelectedCandidateIndex();
  } else {
    // Ideally this should not happen.
  }
}

void SessionConverter::Cancel() {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  // Clear segments and keep the context
  converter_->CancelConversion(segments_.get());
  ResetState();
}

void SessionConverter::Reset() {
  DCHECK(CheckState(COMPOSITION | SUGGESTION | PREDICTION | CONVERSION));

  // Even if composition mode, call ResetConversion
  // in order to clear history segments.
  converter_->ResetConversion(segments_.get());

  if (CheckState(COMPOSITION)) {
    return;
  }

  ResetResult();
  // Reset segments and context
  ResetState();
}

void SessionConverter::Commit(const commands::Context &context) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  if (!UpdateResult(0, segments_->conversion_segments_size(), NULL)) {
    Cancel();
    ResetState();
    return;
  }

  for (size_t i = 0; i < segments_->conversion_segments_size(); ++i) {
    converter_->CommitSegmentValue(segments_.get(),
                                   i,
                                   GetCandidateIndexForConverter(i));
  }
  CommitUsageStats(state_, context);
  converter_->FinishConversion(segments_.get());
  ResetState();
}

bool SessionConverter::CommitSuggestionInternal(
    const composer::Composer &composer,
    const commands::Context &context,
    size_t *consumed_key_size) {
  DCHECK(consumed_key_size);
  DCHECK(CheckState(SUGGESTION));
  ResetResult();
  string preedit;
  composer.GetStringForPreedit(&preedit);

  if (!UpdateResult(0, segments_->conversion_segments_size(),
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
    converter_->CommitPartialSuggestionSegmentValue(
        segments_.get(),
        0,
        GetCandidateIndexForConverter(0),
        Util::SubString(preedit, 0, *consumed_key_size),
        Util::SubString(preedit,
                        *consumed_key_size,
                        preedit_length - *consumed_key_size));
    CommitUsageStats(SessionConverterInterface::SUGGESTION, context);
    InitializeSelectedCandidateIndices();
    // One or more segments must exist because new segment is inserted
    // just after the commited segment.
    DCHECK_GT(segments_->conversion_segments_size(), 0);
  } else {
    // Not partial suggestion so let's reset the state.
    converter_->CommitSegmentValue(segments_.get(),
                                   0,
                                   GetCandidateIndexForConverter(0));
    CommitUsageStats(SessionConverterInterface::SUGGESTION, context);
    converter_->FinishConversion(segments_.get());
    DCHECK_EQ(0, segments_->conversion_segments_size());
    ResetState();
  }
  return true;
}

bool SessionConverter::CommitSuggestionByIndex(
    const size_t index,
    const composer::Composer &composer,
    const commands::Context &context,
    size_t *consumed_key_size) {
  DCHECK(CheckState(SUGGESTION));
  if (index >= candidate_list_->size()) {
    LOG(ERROR) << "index is out of the range: " << index;
    return false;
  }
  candidate_list_->MoveToPageIndex(index);
  UpdateSelectedCandidateIndex();
  return CommitSuggestionInternal(composer, context, consumed_key_size);
}

bool SessionConverter::CommitSuggestionById(
    const int id,
    const composer::Composer &composer,
    const commands::Context &context,
    size_t *consumed_key_size) {
  DCHECK(CheckState(SUGGESTION));
  if (!candidate_list_->MoveToId(id)) {
    // Don't use CandidateMoveToId() method, which overwrites candidates.
    // This is harmful for EXPAND_SUGGESTION session command.
    LOG(ERROR) << "No id found";
    return false;
  }
  UpdateSelectedCandidateIndex();
  return CommitSuggestionInternal(composer, context, consumed_key_size);
}

void SessionConverter::CommitFirstSegment(const commands::Context &context,
                                          size_t *consumed_key_size) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();
  candidate_list_visible_ = false;
  *consumed_key_size = 0;

  // If the number of segments is one, just call Commit.
  if (segments_->conversion_segments_size() == 1) {
    Commit(context);
    return;
  }

  // Store the first conversion segment to the result.
  if (!UpdateResult(0, 1, NULL)) {
    // If the selected candidate of the first segment has the command
    // attribute, Cancel is performed instead of Commit.
    Cancel();
    ResetState();
    return;
  }

  // Get the first conversion segment and the selected candidate.
  Segment *first_segment = segments_->mutable_conversion_segment(0);
  if (first_segment == NULL) {
    LOG(ERROR) << "There is no segment.";
    return;
  }

  // Set the size of 1st segment's key.
  // The caller will remove corresponding characters from the composer.
  *consumed_key_size = Util::CharsLen(first_segment->key());

  // Adjust the segment_index, since the first segment disappeared.
  if (segment_index_ > 0) {
    --segment_index_;
  }

  // Commit the first conversion segment only.
  CommitUsageStatsWithSegmentsSize(state_, context, 1);
  converter_->CommitFirstSegment(segments_.get(),
                                 candidate_list_->focused_id());
  UpdateCandidateList();
}

void SessionConverter::CommitPreedit(const composer::Composer &composer,
                                     const commands::Context &context) {
  string key, preedit, normalized_preedit;
  composer.GetQueryForConversion(&key);
  composer.GetStringForSubmission(&preedit);
  TextNormalizer::NormalizePreeditText(preedit, &normalized_preedit);
  SessionOutput::FillPreeditResult(preedit, result_.get());

  ConverterUtil::InitSegmentsFromString(key, normalized_preedit,
                                        segments_.get());

  CommitUsageStats(SessionConverterInterface::COMPOSITION, context);
  converter_->FinishConversion(segments_.get());
  ResetState();
}

void SessionConverter::CommitHead(
    size_t count, const composer::Composer &composer,
    size_t *consumed_key_size) {
  string preedit;
  composer.GetStringForSubmission(&preedit);
  if (count > preedit.length()) {
    *consumed_key_size = preedit.length();
  } else {
    *consumed_key_size = count;
  }
  preedit = Util::SubString(preedit, 0, *consumed_key_size);
  string composition;
  TextNormalizer::NormalizePreeditText(preedit, &composition);
  SessionOutput::FillPreeditResult(composition, result_.get());
}

void SessionConverter::Revert() {
  converter_->RevertConversion(segments_.get());
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
  if (segment_index_ + 1 >= segments_->conversion_segments_size()) {
    // If |segment_index_| is at the tail of the segments,
    // focus on the head.
    SegmentFocusLeftEdge();
  } else {
    SegmentFocusInternal(segment_index_ + 1);
  }
}

void SessionConverter::SegmentFocusLast() {
  const size_t r_edge = segments_->conversion_segments_size() - 1;
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

void SessionConverter::SegmentFocusLeftEdge() {
  SegmentFocusInternal(0);
}

void SessionConverter::ResizeSegmentWidth(const composer::Composer &composer,
                                          int delta) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  candidate_list_visible_ = false;
  if (CheckState(PREDICTION)) {
    return;  // Do nothing.
  }
  ResetResult();

  const ConversionRequest conversion_request(&composer, request_);
  if (!converter_->ResizeSegment(segments_.get(),
                                 conversion_request,
                                 segment_index_, delta)) {
    return;
  }

  UpdateCandidateList();
  // Clears selected index of a focused segment and trailing segments.
  // TODO(hsumita): Keep the indices if the segment type is FIXED_VALUE.
  selected_candidate_indices_.resize(segments_->conversion_segments_size());
  fill(selected_candidate_indices_.begin() + segment_index_ + 1,
       selected_candidate_indices_.end(), 0);
  UpdateSelectedCandidateIndex();
}

void SessionConverter::SegmentWidthExpand(const composer::Composer &composer) {
  ResizeSegmentWidth(composer, 1);
}

void SessionConverter::SegmentWidthShrink(const composer::Composer &composer) {
  ResizeSegmentWidth(composer, -1);
}

const Segment::Candidate *
SessionConverter::GetSelectedCandidateOfFocusedSegment() const {
  if (!candidate_list_->focused()) {
    return NULL;
  }
  const Candidate &cand = candidate_list_->focused_candidate();
  const Segment &seg = segments_->conversion_segment(segment_index_);
  return &seg.candidate(cand.id());
}

void SessionConverter::CandidateNext(const composer::Composer &composer) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  MaybeExpandPrediction(composer);
  candidate_list_->MoveNext();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void SessionConverter::CandidateNextPage() {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_->MoveNextPage();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void SessionConverter::CandidatePrev() {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_->MovePrev();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void SessionConverter::CandidatePrevPage() {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_->MovePrevPage();
  candidate_list_visible_ = true;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void SessionConverter::CandidateMoveToId(
    const int id, const composer::Composer &composer) {
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

  candidate_list_->MoveToId(id);
  candidate_list_visible_ = false;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

void SessionConverter::CandidateMoveToPageIndex(const size_t index) {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  ResetResult();

  candidate_list_->MoveToPageIndex(index);
  candidate_list_visible_ = false;
  UpdateSelectedCandidateIndex();
  SegmentFocus();
}

bool SessionConverter::CandidateMoveToShortcut(const char shortcut) {
  DCHECK(CheckState(PREDICTION | CONVERSION));

  if (!candidate_list_visible_) {
    VLOG(1) << "Candidate list is not displayed.";
    return false;
  }

  const string &shortcuts = operation_preferences_.candidate_shortcuts;
  if (shortcuts.empty()) {
    VLOG(1) << "No shortcuts";
    return false;
  }

  // Check if the input character is in the shortcut.
  // TODO(komatsu): Support non ASCII characters such as Unicode and
  // special keys.
  const string::size_type index = shortcuts.find(shortcut);
  if (index == string::npos) {
    VLOG(1) << "shortcut is not a member of shortcuts.";
    return false;
  }

  if (!candidate_list_->MoveToPageIndex(index)) {
    VLOG(1) << "shortcut is out of the range.";
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

void SessionConverter::PopOutput(
    const composer::Composer &composer, commands::Output *output) {
  FillOutput(composer, output);
  ResetResult();
}

void SessionConverter::FillOutput(
    const composer::Composer &composer, commands::Output *output) const {
  if (output == NULL) {
    LOG(ERROR) << "output is NULL.";
    return;
  }
  if (result_->has_value()) {
    FillResult(output->mutable_result());
  }
  if (CheckState(COMPOSITION)) {
    if (!composer.Empty()) {
      session::SessionOutput::FillPreedit(composer,
                                          output->mutable_preedit());
    }
  }
  if (!IsActive()) {
    return;
  }

  // Composition on Suggestion
  if (CheckState(SUGGESTION)) {
    // When the suggestion comes from zero query suggestion, the
    // composer is empty.  In that case, preedit is not rendered.
    if (!composer.Empty()) {
      session::SessionOutput::FillPreedit(composer,
                                          output->mutable_preedit());
    }
  } else if (CheckState(PREDICTION | CONVERSION)) {
    // Conversion on Prediction or Conversion
    FillConversion(output->mutable_preedit());
  }
  // Candidate list
  if (CheckState(SUGGESTION | PREDICTION | CONVERSION) &&
      candidate_list_visible_) {
    FillCandidates(output->mutable_candidates());
  }
#ifndef __native_client__
  // All candidate words
  // In NaCl, we don't use the all candidate word data.
  if (CheckState(SUGGESTION | PREDICTION | CONVERSION)) {
    FillAllCandidateWords(output->mutable_all_candidate_words());
  }
#endif  // __native_client__

  // Propagate config information to renderer.
  PropagateConfigToRenderer(output);
}

void SessionConverter::FillContext(commands::Context *context) const {
  if (context->has_preceding_text()) {
    // Client has set the information of surrounding text, so do nothing.
    return;
  }
  if (segments_->history_segments_size() == 0) {
    return;
  }

  // Set preceding text using history segments.
  string *preceding_text = context->mutable_preceding_text();
  for (size_t i = 0; i < segments_->history_segments_size(); ++i) {
    *preceding_text += segments_->history_segment(i).candidate(0).value;
  }
}

void SessionConverter::RemoveTailOfHistorySegments(size_t num_of_characters) {
  segments_->RemoveTailOfHistorySegments(num_of_characters);
}

// static
void SessionConverter::SetConversionPreferences(
    const ConversionPreferences &preferences,
    Segments *segments) {
  segments->set_user_history_enabled(preferences.use_history);
  segments->set_max_history_segments_size(preferences.max_history_size);
}

SessionConverter* SessionConverter::Clone() const {
  SessionConverter *session_converter =
      new SessionConverter(converter_, request_);

  // Copy the members in order of their declarations.
  session_converter->state_ = state_;
  // TODO(team): copy of |converter_| member.
  // We cannot copy the member converter_ from SessionConverterInterface becase
  // it doesn't (and shouldn't) define a method like GetConverter(). At the
  // moment it's ok because the current design guarantees that the converter is
  // singleton. However, we should refactor such bad design; see also the
  // comment right above.
  session_converter->segments_->CopyFrom(*segments_);
  session_converter->segment_index_ = segment_index_;
  session_converter->previous_suggestions_.CopyFrom(previous_suggestions_);
  session_converter->conversion_preferences_ = conversion_preferences();
  session_converter->operation_preferences_ = operation_preferences_;
  session_converter->result_->CopyFrom(*result_);

  if (session_converter->CheckState(SUGGESTION | PREDICTION | CONVERSION)) {
    session_converter->UpdateCandidateList();
    session_converter->candidate_list_->MoveToId(candidate_list_->focused_id());
    session_converter->SetCandidateListVisible(candidate_list_visible_);
  }

  session_converter->request_ = request_;
  session_converter->selected_candidate_indices_ = selected_candidate_indices_;

  return session_converter;
}

void SessionConverter::ResetResult() {
  result_->Clear();
}

void SessionConverter::ResetState() {
  state_ = COMPOSITION;
  segment_index_ = 0;
  previous_suggestions_.clear();
  candidate_list_visible_ = false;
  candidate_list_->Clear();
  selected_candidate_indices_.clear();
}

void SessionConverter::SegmentFocus() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  converter_->FocusSegmentValue(segments_.get(),
                                segment_index_,
                                GetCandidateIndexForConverter(segment_index_));
}

void SessionConverter::SegmentFix() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  converter_->CommitSegmentValue(segments_.get(),
                                 segment_index_,
                                 GetCandidateIndexForConverter(segment_index_));
}

void SessionConverter::GetPreedit(const size_t index,
                                  const size_t size,
                                  string *preedit) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  DCHECK(index + size <= segments_->conversion_segments_size());
  DCHECK(preedit);

  preedit->clear();
  for (size_t i = index; i < size; ++i) {
    if (CheckState(CONVERSION)) {
      // In conversion mode, all the key of candidates is same.
      preedit->append(segments_->conversion_segment(i).key());
    } else {
      DCHECK(CheckState(SUGGESTION | PREDICTION));
      // In suggestion or prediction modes, each key may have
      // different keys, so content_key is used although it is
      // possibly droped the conjugational word (ex. the content_key
      // of "はしる" is "はし").
      preedit->append(GetSelectedCandidate(i).content_key);
    }
  }
}

void SessionConverter::GetConversion(const size_t index,
                                     const size_t size,
                                     string *conversion) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  DCHECK(index + size <= segments_->conversion_segments_size());
  DCHECK(conversion);

  conversion->clear();
  for (size_t i = index; i < size; ++i) {
    conversion->append(GetSelectedCandidateValue(i));
  }
}

size_t SessionConverter::GetConsumedPreeditSize(const size_t index,
                                                const size_t size) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  DCHECK(index + size <= segments_->conversion_segments_size());

  if (CheckState(SUGGESTION | PREDICTION)) {
    DCHECK_EQ(1, size);
    const Segment &segment = segments_->conversion_segment(0);
    const int id = GetCandidateIndexForConverter(0);
    const Segment::Candidate &candidate = segment.candidate(id);
    return (candidate.attributes & Segment::Candidate::PARTIALLY_KEY_CONSUMED)
               ? candidate.consumed_key_size : kConsumedAllCharacters;
  }

  DCHECK(CheckState(CONVERSION));
  size_t result = 0;
  for (size_t i = index; i < size; ++i) {
    const int id = GetCandidateIndexForConverter(i);
    const Segment::Candidate &candidate =
        segments_->conversion_segment(i).candidate(id);
    DCHECK(!(candidate.attributes &
             Segment::Candidate::PARTIALLY_KEY_CONSUMED));
    result += Util::CharsLen(segments_->conversion_segment(i).key());
  }
  return result;
}

bool SessionConverter::MaybePerformCommandCandidate(
    const size_t index,
    const size_t size) const {
  // If a candidate has the command attribute, Cancel is performed
  // instead of Commit after executing the specified action.
  for (size_t i = index; i < size; ++i) {
    const int id = GetCandidateIndexForConverter(i);
    const Segment::Candidate &candidate =
        segments_->conversion_segment(i).candidate(id);
    if (candidate.attributes & Segment::Candidate::COMMAND_CANDIDATE) {
      switch (candidate.command) {
        case Segment::Candidate::DEFAULT_COMMAND:
          // Do nothing
          break;
        case Segment::Candidate::ENABLE_INCOGNITO_MODE:
          SetIncognitoMode(true);
          break;
        case Segment::Candidate::DISABLE_INCOGNITO_MODE:
          SetIncognitoMode(false);
          break;
        case Segment::Candidate::ENABLE_PRESENTATION_MODE:
          SetPresentationMode(true);
          break;
        case Segment::Candidate::DISABLE_PRESENTATION_MODE:
          SetPresentationMode(false);
          break;
        default:
          LOG(WARNING) << "Unkown command: " << candidate.command;
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

  string preedit, conversion;
  GetPreedit(index, size, &preedit);
  GetConversion(index, size, &conversion);
  if (consumed_key_size) {
    *consumed_key_size = GetConsumedPreeditSize(index, size);
  }
  SessionOutput::FillConversionResult(preedit, conversion, result_.get());
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
  const bool add_meta_candidates = (candidate_list_->size() == 0);

  const Segment &segment = segments_->conversion_segment(segment_index_);
  for (size_t i = candidate_list_->next_available_id();
       i < segment.candidates_size();
       ++i) {
    candidate_list_->AddCandidate(i, segment.candidate(i).value);
    // if candidate has spelling correction attribute,
    // always display the candidate to let user know the
    // miss spelled candidate.
    if (i < 10 &&
        (segment.candidate(i).attributes &
         Segment::Candidate::SPELLING_CORRECTION)) {
      candidate_list_visible_ = true;
    }
  }

  const bool focused = (
      segments_->request_type() != Segments::SUGGESTION &&
      segments_->request_type() != Segments::PARTIAL_SUGGESTION &&
      segments_->request_type() != Segments::PARTIAL_PREDICTION);
  candidate_list_->set_focused(focused);

  if (segment.meta_candidates_size() == 0) {
    // For suggestion mode, it is natural that T13N is not initialized.
    if (CheckState(SUGGESTION)) {
      return;
    }
    // For other modes, records |segment| just in case.
    VLOG(1) << "T13N is not initialized: " << segment.key();
    return;
  }

  if (!add_meta_candidates) {
    return;
  }

  // Set transliteration candidates
  CandidateList *transliterations;
  if (operation_preferences_.use_cascading_window) {
    const bool kNoRotate = false;
    transliterations = candidate_list_->AllocateSubCandidateList(kNoRotate);
    transliterations->set_focused(true);

    const char kT13nLabel[] =
      // "そのほかの文字種";
      "\xe3\x81\x9d\xe3\x81\xae\xe3\x81\xbb\xe3\x81\x8b\xe3\x81\xae"
      "\xe6\x96\x87\xe5\xad\x97\xe7\xa8\xae";
    transliterations->set_name(kT13nLabel);
  } else {
    transliterations = candidate_list_.get();
  }

  // Add transliterations.
  for (size_t i = 0; i < transliteration::NUM_T13N_TYPES; ++i) {
    const transliteration::TransliterationType type =
      transliteration::TransliterationTypeArray[i];
    transliterations->AddCandidateWithAttributes(
        GetT13nId(type),
        segment.meta_candidate(i).value,
        GetT13nAttributes(type));
  }
}

void SessionConverter::UpdateCandidateList() {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  candidate_list_->Clear();
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
  return candidate_list_->focused_id();
}

string SessionConverter::GetSelectedCandidateValue(
    const size_t segment_index) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  const int id = GetCandidateIndexForConverter(segment_index);
  const Segment::Candidate &candidate =
      segments_->conversion_segment(segment_index).candidate(id);
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
  return segments_->conversion_segment(segment_index).candidate(id);
}

void SessionConverter::FillConversion(commands::Preedit *preedit) const {
  DCHECK(CheckState(PREDICTION | CONVERSION));
  SessionOutput::FillConversion(*segments_,
                                segment_index_,
                                candidate_list_->focused_id(),
                                preedit);
}

void SessionConverter::FillResult(commands::Result *result) const {
  result->CopyFrom(*result_);
}

void SessionConverter::FillCandidates(commands::Candidates *candidates) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  if (!candidate_list_visible_) {
    return;
  }

  // The position to display the candidate window.
  size_t position = 0;
  string conversion;
  for (size_t i = 0; i < segment_index_; ++i) {
    position += Util::CharsLen(GetSelectedCandidate(i).value);
  }

  const Segment &segment = segments_->conversion_segment(segment_index_);
  SessionOutput::FillCandidates(
      segment, *candidate_list_, position, candidates);

  // Shortcut keys
  if (CheckState(PREDICTION | CONVERSION)) {
    SessionOutput::FillShortcuts(operation_preferences_.candidate_shortcuts,
                                 candidates);
  }

  // Store category
  switch (segments_->request_type()) {
    case Segments::CONVERSION:
      candidates->set_category(commands::CONVERSION);
      break;
    case Segments::PREDICTION:
      candidates->set_category(commands::PREDICTION);
      break;
    case Segments::SUGGESTION:
      candidates->set_category(commands::SUGGESTION);
      break;
    case Segments::PARTIAL_PREDICTION:
      // Not PREDICTION because we do not want to get focused candidate.
      candidates->set_category(commands::SUGGESTION);
      break;
    case Segments::PARTIAL_SUGGESTION:
      candidates->set_category(commands::SUGGESTION);
      break;
    default:
      LOG(WARNING) << "Unknown request type: " << segments_->request_type();
      candidates->set_category(commands::CONVERSION);
      break;
  }

  if (candidates->has_usages()) {
    candidates->mutable_usages()->set_category(commands::USAGE);
  }
  if (candidates->has_subcandidates()) {
    // TODO(komatsu): Subcandidate is not always for transliterations.
    // The category of the subcandidates should be checked.
    candidates->mutable_subcandidates()->set_category(
        commands::TRANSLITERATION);
  }

  // Store display type
  candidates->set_display_type(commands::MAIN);
  if (candidates->has_usages()) {
    candidates->mutable_usages()->set_display_type(commands::CASCADE);
  }
  if (candidates->has_subcandidates()) {
    // TODO(komatsu): Subcandidate is not always for transliterations.
    // The category of the subcandidates should be checked.
    candidates->mutable_subcandidates()->set_display_type(commands::CASCADE);
  }

  // Store footer.
  SessionOutput::FillFooter(candidates->category(), candidates);
}


void SessionConverter::FillAllCandidateWords(
    commands::CandidateList *candidates) const {
  DCHECK(CheckState(SUGGESTION | PREDICTION | CONVERSION));
  commands::Category category;
  switch (segments_->request_type()) {
    case Segments::CONVERSION:
      category = commands::CONVERSION;
      break;
    case Segments::PREDICTION:
      category = commands::PREDICTION;
      break;
    case Segments::SUGGESTION:
      category = commands::SUGGESTION;
      break;
    case Segments::PARTIAL_PREDICTION:
      // Not PREDICTION because we do not want to get focused candidate.
      category = commands::SUGGESTION;
      break;
    case Segments::PARTIAL_SUGGESTION:
      category = commands::SUGGESTION;
      break;
    default:
      LOG(WARNING) << "Unkown request type: " << segments_->request_type();
      category = commands::CONVERSION;
      break;
  }

  const Segment &segment = segments_->conversion_segment(segment_index_);
  SessionOutput::FillAllCandidateWords(
      segment, *candidate_list_, category, candidates);
}

void SessionConverter::PropagateConfigToRenderer(
    commands::Output *output) const {
  DCHECK(output);
  if (!candidate_list_visible_ || !CheckState(PREDICTION | CONVERSION)) {
    return;
  }
  const config::Config &config = config::ConfigHandler::GetConfig();
  if (config.has_information_list_config() &&
      config.information_list_config().use_web_usage_dictionary() &&
      config.information_list_config().web_service_entries_size() > 0) {
    output->mutable_config()->mutable_information_list_config()->CopyFrom(
        config.information_list_config());
  }
}

void SessionConverter::SetRequest(const commands::Request *request) {
  request_ = request;
}

void SessionConverter::UpdateSelectedCandidateIndex() {
  int index;
  const Candidate &focused_candidate = candidate_list_->focused_candidate();
  if (focused_candidate.IsSubcandidateList()) {
    const int t13n_index =
        focused_candidate.subcandidate_list().focused_index();
    index = -1 - t13n_index;
  } else {
    // TODO(hsumita): Use id instead of focused index.
    index = candidate_list_->focused_index();
  }
  selected_candidate_indices_[segment_index_] = index;
}

void SessionConverter::InitializeSelectedCandidateIndices() {
  selected_candidate_indices_.clear();
  selected_candidate_indices_.resize(segments_->conversion_segments_size());
}

void SessionConverter::UpdateCandidateStats(const string &base_name,
                                            int32 index) {
  string prefix;
  if (index < 0) {
    prefix = "TransliterationCandidates";
    index = -1 - index;
  } else {
    prefix = base_name + "Candidates";
  }

  if (index <= 9) {
    const string stats_name = prefix + NumberUtil::SimpleItoa(index);
    UsageStats::IncrementCount(stats_name);
  } else {
    const string stats_name = prefix + "GE10";
    UsageStats::IncrementCount(stats_name);
  }
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
      commit_segment_size = segments_->conversion_segments_size();
      break;
    default:
      LOG(DFATAL) << "Unexpected state: " << commit_state;
  }
  CommitUsageStatsWithSegmentsSize(commit_state, context, commit_segment_size);
}

void SessionConverter::CommitUsageStatsWithSegmentsSize(
    SessionConverterInterface::State commit_state,
    const commands::Context &context,
    size_t commit_segments_size) {
  CHECK_LE(commit_segments_size, selected_candidate_indices_.size());

  string stats_str;
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
        UpdateCandidateStats(stats_str,
                             selected_candidate_indices_[i]);
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
      UsageStats::IncrementCount(
          "CommitFrom" + stats_str + "InGoogleSearchBox");
    }
  }

  const vector<int>::iterator it = selected_candidate_indices_.begin();
  selected_candidate_indices_.erase(it, it + commit_segments_size);
}

}  // namespace session
}  // namespace mozc
