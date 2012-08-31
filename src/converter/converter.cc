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

#include "converter/converter.h"

#include <algorithm>
#include <climits>
#include <string>
#include <utility>
#include <vector>

#include "base/base.h"
#include "base/logging.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/connector_interface.h"
#include "converter/conversion_request.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segments.h"
#include "converter/user_data_manager_interface.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/predictor_interface.h"
#include "rewriter/rewriter_interface.h"
#include "session/request_handler.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace {

const size_t kErrorIndex = static_cast<size_t>(-1);

class UserDataManagerImpl : public UserDataManagerInterface {
 public:
  explicit UserDataManagerImpl(PredictorInterface *predictor,
                               RewriterInterface *rewriter)
      : predictor_(predictor), rewriter_(rewriter) {}
  ~UserDataManagerImpl();

  virtual bool Sync();
  virtual bool Reload();
  virtual bool ClearUserHistory();
  virtual bool ClearUserPrediction();
  virtual bool ClearUnusedUserPrediction();

 private:
  PredictorInterface *predictor_;
  RewriterInterface *rewriter_;
};

size_t GetSegmentIndex(const Segments *segments,
                       size_t segment_index) {
  const size_t history_segments_size = segments->history_segments_size();
  const size_t result = history_segments_size + segment_index;
  if (result >= segments->segments_size()) {
    return kErrorIndex;
  }
  return result;
}

void SetKey(Segments *segments, const string &key) {
  segments->set_max_history_segments_size(4);
  segments->clear_conversion_segments();
  segments->clear_revert_entries();

  mozc::Segment *seg = segments->add_segment();
  DCHECK(seg);

  seg->Clear();
  seg->set_key(key);
  seg->set_segment_type(mozc::Segment::FREE);

  VLOG(2) << segments->DebugString();
}

bool IsMobile() {
  return GET_REQUEST(zero_query_suggestion) &&
      GET_REQUEST(mixed_conversion);
}

bool IsValidSegments(const Segments &segments) {
  // All segments should have candidate
  for (size_t i = 0; i < segments.segments_size(); ++i) {
    if (segments.segment(i).candidates_size() != 0) {
      continue;
    }
    // On mobile, we don't distinguish candidates and meta candidates
    // So it's ok if we have meta candidates even if we don't have candidates
    // TODO(team): we may remove mobile check if other platforms accept
    // meta candidate only segemnt
    if (IsMobile() && segments.segment(i).meta_candidates_size() != 0) {
      continue;
    }
    return false;
  }
  return true;
}
}  // namespace

ConverterImpl::ConverterImpl() : pos_matcher_(NULL),
                                 pos_group_(NULL),
                                 rewriter_(NULL),
                                 user_data_manager_(NULL),
                                 immutable_converter_(NULL),
                                 general_noun_id_(kuint16max) {
}

ConverterImpl::~ConverterImpl() {}

void ConverterImpl::Init(const POSMatcher *pos_matcher,
                         const PosGroup *pos_group,
                         PredictorInterface *predictor,
                         RewriterInterface *rewriter,
                         ImmutableConverterInterface *immutable_converter) {
  // Initializes in order of declaration.
  pos_matcher_ = pos_matcher;
  pos_group_ = pos_group;
  predictor_.reset(predictor);
  rewriter_.reset(rewriter);
  user_data_manager_.reset(new UserDataManagerImpl(predictor, rewriter));
  immutable_converter_ = immutable_converter;
  general_noun_id_ = pos_matcher_->GetGeneralNounId();
}

bool ConverterImpl::SetupHistorySegmentsFromPrecedingText(
    const string &preceding_text, Segments *segments) const {
  if (!StartReverseConversion(segments, preceding_text)) {
    LOG(WARNING) << "Reverse conversion failed for " << preceding_text;
    return false;
  }
  // Below we change the resulting segments to history segments. We should
  // guarantee that the total segments size doesn't exceed the maximum size of
  // history segments, by erasing leading segments as necessary.
  if (segments->segments_size() > segments->max_history_segments_size()) {
    segments->erase_segments(
        0, segments->segments_size() - segments->max_history_segments_size());
  }
  for (int i = 0; i < segments->segments_size(); ++i) {
    Segment *segment = segments->mutable_segment(i);
    DCHECK(segment);
    if (segment->candidates_size() == 0) {
      LOG(WARNING) << "Empty segment was returned by reverse conversion";
      return false;
    }
    // Need to swap key and value as the reverse conversion stores reading
    // (usual key for conversion) to value.
    for (int j = 0; j < segment->candidates_size(); ++j) {
      Segment::Candidate *candidate = segment->mutable_candidate(j);
      swap(candidate->key, candidate->value);
      swap(candidate->content_key, candidate->content_value);
    }
    segment->set_segment_type(Segment::HISTORY);
    segment->set_key(segment->candidate(0).key);
  }
  return true;
}

bool ConverterImpl::StartConversionForRequest(const ConversionRequest &request,
                                              Segments *segments) const {
  if (!request.has_composer()) {
    LOG(ERROR) << "Request doesn't have composer";
    return false;
  }

  // Existing history segments are used prior to preceding text.
  if (segments->history_segments_size() == 0 &&
      !request.preceding_text().empty()) {
    if (!SetupHistorySegmentsFromPrecedingText(request.preceding_text(),
                                               segments)) {
      LOG(WARNING) << "Failed to utilize preceding text: "
                   << request.preceding_text();
      segments->Clear();  // fall back to a normal conversion
    }
  }
  string conversion_key;
  request.composer().GetQueryForConversion(&conversion_key);
  SetKey(segments, conversion_key);
  segments->set_request_type(Segments::CONVERSION);
  if (!immutable_converter_->Convert(segments)) {
    return false;
  }
  rewriter_->Rewrite(request, segments);
  return IsValidSegments(*segments);
}

bool ConverterImpl::StartConversion(Segments *segments,
                                    const string &key) const {
  SetKey(segments, key);
  segments->set_request_type(Segments::CONVERSION);
  if (!immutable_converter_->Convert(segments)) {
    return false;
  }
  rewriter_->Rewrite(ConversionRequest(), segments);
  return IsValidSegments(*segments);
}

bool ConverterImpl::StartReverseConversion(Segments *segments,
                                           const string &key) const {
  segments->Clear();
  SetKey(segments, key);
  segments->set_request_type(Segments::REVERSE_CONVERSION);
  if (!immutable_converter_->Convert(segments)) {
    return false;
  }
  if (segments->segments_size() == 0) {
    LOG(WARNING) << "no segments from reverse conversion";
    return false;
  }
  for (int i = 0; i < segments->segments_size(); ++i) {
    const mozc::Segment &seg = segments->segment(i);
    if (seg.candidates_size() == 0 ||
        seg.candidate(0).value.empty()) {
      segments->Clear();
      LOG(WARNING) << "got an empty segment from reverse conversion";
      return false;
    }
  }
  return true;
}

// TODO(noriyukit): |key| can be a member of ConversionRequest.
bool ConverterImpl::Predict(const ConversionRequest &request,
                            const string &key,
                            const Segments::RequestType request_type,
                            Segments *segments) const {
  const Segments::RequestType original_request_type = segments->request_type();
  if ((original_request_type != Segments::PREDICTION &&
       original_request_type != Segments::PARTIAL_PREDICTION) ||
      segments->conversion_segments_size() == 0 ||
      segments->conversion_segment(0).key() != key) {
    // - If the original request is not prediction relating one
    //   (e.g. conversion), invoke SetKey because current segments has
    //   data for conversion, not prediction.
    // - If the segment size is 0, invoke SetKey because the segments is not
    //   correctly prepared.
    // - If the key of the segments differs from the input key,
    //   invoke SetKey because current segments should be completely reset.
    // - Otherwise keep current key and candidates.
    //
    // This SetKey omitting is for mobile predictor.
    // On normal inputting, we are showing suggestion results. When users
    // push expansion button, we will add prediction results just after the
    // suggestion results. For this, we don't reset segments for prediction.
    // However, we don't have to do so for suggestion. Here, we are deciding
    // whether the input key is changed or not by using segment key. This is not
    // perfect because for roman input, conversion key is not updated by
    // imcomplete input, for example, conversion key is "あ" for the input "a",
    // and will still be "あ" for the input "ak". For avoiding mis-reset of
    // the results, we will reset always for suggestion request type.
    SetKey(segments, key);
  }
  DCHECK_EQ(1, segments->conversion_segments_size());
  DCHECK_EQ(key, segments->conversion_segment(0).key());

  segments->set_request_type(request_type);
  if (!predictor_->PredictForRequest(request, segments)) {
    return false;
  }
  rewriter_->Rewrite(request, segments);
  return IsValidSegments(*segments);
}

bool ConverterImpl::StartPredictionForRequest(const ConversionRequest &request,
                                              Segments *segments) const {
  if (!request.has_composer()) {
    LOG(ERROR) << "Composer is NULL";
    return false;
  }

  // Existing history segments are used prior to preceding text.
  if (segments->history_segments_size() == 0 &&
      !request.preceding_text().empty()) {
    if (!SetupHistorySegmentsFromPrecedingText(request.preceding_text(),
                                               segments)) {
      LOG(WARNING) << "Failed to utilize preceding text: "
                   << request.preceding_text();
      segments->Clear();  // fall back to a normal prediction
    }
  }
  string prediction_key;
  request.composer().GetQueryForPrediction(&prediction_key);
  return Predict(request, prediction_key, Segments::PREDICTION, segments);
}

bool ConverterImpl::StartPrediction(Segments *segments,
                                    const string &key) const {
  ConversionRequest default_request;
  return Predict(default_request, key, Segments::PREDICTION, segments);
}

bool ConverterImpl::StartSuggestion(Segments *segments,
                                    const string &key) const {
  ConversionRequest default_request;
  return Predict(default_request, key, Segments::SUGGESTION, segments);
}

bool ConverterImpl::StartSuggestionForRequest(const ConversionRequest &request,
                                              Segments *segments) const {
  DCHECK(request.has_composer());
  string prediction_key;
  request.composer().GetQueryForPrediction(&prediction_key);
  return Predict(request, prediction_key, Segments::SUGGESTION, segments);
}

bool ConverterImpl::StartPartialSuggestion(Segments *segments,
                                           const string &key) const {
  ConversionRequest default_request;
  return Predict(default_request, key, Segments::PARTIAL_SUGGESTION, segments);
}

bool ConverterImpl::StartPartialSuggestionForRequest(
    const ConversionRequest &request, Segments *segments) const {
  DCHECK(request.has_composer());
  const size_t cursor = request.composer().GetCursor();
  if (cursor == 0 || cursor == request.composer().GetLength()) {
    return StartSuggestionForRequest(request, segments);
  }

  string conversion_key;
  request.composer().GetQueryForConversion(&conversion_key);
  conversion_key = Util::SubString(conversion_key, 0, cursor);
  return Predict(request, conversion_key,
                 Segments::PARTIAL_SUGGESTION, segments);
}

bool ConverterImpl::StartPartialPrediction(Segments *segments,
                                           const string &key) const {
  ConversionRequest default_request;
  return Predict(default_request, key, Segments::PARTIAL_PREDICTION, segments);
}

bool ConverterImpl::StartPartialPredictionForRequest(
    const ConversionRequest &request, Segments *segments) const {
  DCHECK(request.has_composer());
  const size_t cursor = request.composer().GetCursor();
  if (cursor == 0 || cursor == request.composer().GetLength()) {
    return StartPredictionForRequest(request, segments);
  }

  string conversion_key;
  request.composer().GetQueryForConversion(&conversion_key);
  conversion_key = Util::SubString(conversion_key, 0, cursor);

  return Predict(request, conversion_key,
                 Segments::PARTIAL_PREDICTION, segments);
}

bool ConverterImpl::FinishConversion(Segments *segments) const {
  for (int i = 0; i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    // revert SUBMITTED segments to FIXED_VALUE
    // SUBMITTED segments are created by "submit first segment" operation
    // (ctrl+N for ATOK keymap).
    // To learn the conversion result, we should change the segment types
    // to FIXED_VALUE.
    if (seg->segment_type() == Segment::SUBMITTED) {
      seg->set_segment_type(Segment::FIXED_VALUE);
    }
    if (seg->candidates_size() > 0) {
      CompletePOSIds(seg->mutable_candidate(0));
    }
  }

  segments->clear_revert_entries();
  if (segments->request_type() == Segments::CONVERSION) {
    rewriter_->Finish(segments);
  }
  predictor_->Finish(segments);

  // Remove the front segments except for some segments which will be
  // used as history segments.
  const int start_index = max(
      0,
      static_cast<int>(segments->segments_size()
          - segments->max_history_segments_size()));
  for (int i = 0; i < start_index; ++i) {
    segments->pop_front_segment();
  }

  // Remaining segments are used as history segments.
  for (size_t i = 0; i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    seg->set_segment_type(Segment::HISTORY);
  }

  return true;
}

bool ConverterImpl::CancelConversion(Segments *segments) const {
  segments->clear_conversion_segments();
  return true;
}

bool ConverterImpl::ResetConversion(Segments *segments) const {
  segments->Clear();
  return true;
}

bool ConverterImpl::RevertConversion(Segments *segments) const {
  if (segments->revert_entries_size() == 0) {
    return true;
  }
  predictor_->Revert(segments);
  segments->clear_revert_entries();
  return true;
}

bool ConverterImpl::CommitSegmentValueInternal(
    Segments *segments, size_t segment_index, int candidate_index,
    Segment::SegmentType segment_type) const {
  segment_index = GetSegmentIndex(segments, segment_index);
  if (segment_index == kErrorIndex) {
    return false;
  }

  Segment *segment = segments->mutable_segment(segment_index);
  const int values_size = static_cast<int>(segment->candidates_size());
  if (candidate_index < -transliteration::NUM_T13N_TYPES ||
      candidate_index >= values_size) {
    return false;
  }

  segment->set_segment_type(segment_type);
  segment->move_candidate(candidate_index, 0);

  if (candidate_index != 0) {
    segment->mutable_candidate(0)->attributes |= Segment::Candidate::RERANKED;
  }

  return true;
}

bool ConverterImpl::CommitSegmentValue(Segments *segments, size_t segment_index,
                                       int candidate_index) const {
  return CommitSegmentValueInternal(segments, segment_index, candidate_index,
                                    Segment::FIXED_VALUE);
}

bool ConverterImpl::CommitPartialSuggestionSegmentValue(
    Segments *segments, size_t segment_index, int candidate_index,
    const string &current_segment_key, const string &new_segment_key) const {
  DCHECK_GT(segments->conversion_segments_size(), 0);

  size_t raw_segment_index = GetSegmentIndex(segments, segment_index);
  if (!CommitSegmentValueInternal(segments, segment_index, candidate_index,
                                  Segment::SUBMITTED)) {
    return false;
  }
  Segment *segment = segments->mutable_segment(raw_segment_index);
  segment->set_key(current_segment_key);
  Segment *new_segment = segments->insert_segment(raw_segment_index + 1);
  new_segment->set_key(new_segment_key);
  DCHECK_GT(segments->conversion_segments_size(), 0);

  return true;
}

bool ConverterImpl::FocusSegmentValue(Segments *segments,
                                      size_t segment_index,
                                      int    candidate_index) const {
  segment_index = GetSegmentIndex(segments, segment_index);
  if (segment_index == kErrorIndex) {
    return false;
  }

  return rewriter_->Focus(segments, segment_index, candidate_index);
}

bool ConverterImpl::FreeSegmentValue(Segments *segments,
                                     size_t segment_index) const {
  segment_index = GetSegmentIndex(segments, segment_index);
  if (segment_index == kErrorIndex) {
    return false;
  }

  Segment *segment = segments->mutable_segment(segment_index);
  segment->set_segment_type(Segment::FREE);

  if (segments->request_type() != Segments::CONVERSION) {
    return false;
  }

  return immutable_converter_->Convert(segments);
}

bool ConverterImpl::SubmitFirstSegment(Segments *segments,
                                       size_t candidate_index) const {
  return CommitSegmentValueInternal(segments, 0, candidate_index,
                                    Segment::SUBMITTED);
}

bool ConverterImpl::ResizeSegment(Segments *segments,
                                  const ConversionRequest &request,
                                  size_t segment_index,
                                  int offset_length) const {
  if (segments->request_type() != Segments::CONVERSION) {
    return false;
  }

  // invalid request
  if (offset_length == 0) {
    return false;
  }

  segment_index = GetSegmentIndex(segments, segment_index);
  if (segment_index == kErrorIndex) {
    return false;
  }

  // the last segments cannot become longer
  if (offset_length > 0 && segment_index == segments->segments_size() - 1) {
    return false;
  }

  const Segment &cur_segment = segments->segment(segment_index);
  const size_t cur_length = Util::CharsLen(cur_segment.key());

  // length canont become 0
  if (cur_length + offset_length == 0) {
    return false;
  }

  const string cur_segment_key = cur_segment.key();

  if (offset_length > 0) {
    int length = offset_length;
    string last_key;
    size_t last_clen = 0;
    {
      string new_key = cur_segment_key;
      while (segment_index + 1 < segments->segments_size()) {
        last_key = segments->segment(segment_index + 1).key();
        segments->erase_segment(segment_index + 1);
        last_clen = Util::CharsLen(last_key.c_str(), last_key.size());
        length -= static_cast<int>(last_clen);
        if (length <= 0) {
          string tmp;
          Util::SubString(last_key, 0, length + last_clen, &tmp);
          new_key += tmp;
          break;
        }
        new_key += last_key;
      }

      Segment *segment = segments->mutable_segment(segment_index);
      segment->Clear();
      segment->set_segment_type(Segment::FIXED_BOUNDARY);
      segment->set_key(new_key);
    }  // scope out |segment|, |new_key|

    if (length < 0) {  // remaning part
      Segment *segment = segments->insert_segment(segment_index + 1);
      segment->set_segment_type(Segment::FREE);
      string new_key;
      Util::SubString(last_key,
                      static_cast<size_t>(length + last_clen),
                      static_cast<size_t>(-length), &new_key);
      segment->set_key(new_key);
    }
  } else if (offset_length < 0) {
    if (cur_length + offset_length > 0) {
      Segment *segment1 = segments->mutable_segment(segment_index);
      segment1->Clear();
      segment1->set_segment_type(Segment::FIXED_BOUNDARY);
      string new_key;
      Util::SubString(cur_segment_key,
                      0,
                      cur_length + offset_length,
                      &new_key);
      segment1->set_key(new_key);
    }

    if (segment_index + 1 < segments->segments_size()) {
      Segment *segment2 = segments->mutable_segment(segment_index + 1);
      segment2->set_segment_type(Segment::FREE);
      string tmp;
      Util::SubString(cur_segment_key,
                      max(static_cast<size_t>(0), cur_length + offset_length),
                      cur_length,
                      &tmp);
      tmp += segment2->key();
      segment2->set_key(tmp);
    } else {
      Segment *segment2 = segments->add_segment();
      segment2->set_segment_type(Segment::FREE);
      string new_key;
      Util::SubString(cur_segment_key,
                      max(static_cast<size_t>(0), cur_length + offset_length),
                      cur_length,
                      &new_key);
      segment2->set_key(new_key);
    }
  }

  segments->set_resized(true);

  if (!immutable_converter_->Convert(segments)) {
    return false;
  }

  rewriter_->Rewrite(request, segments);

  return true;
}

bool ConverterImpl::ResizeSegment(Segments *segments,
                                  const ConversionRequest &request,
                                  size_t start_segment_index,
                                  size_t segments_size,
                                  const uint8 *new_size_array,
                                  size_t array_size) const {
  if (segments->request_type() != Segments::CONVERSION) {
    return false;
  }

  const size_t kMaxArraySize = 256;
  start_segment_index = GetSegmentIndex(segments, start_segment_index);
  const size_t end_segment_index = start_segment_index + segments_size;
  if (start_segment_index == kErrorIndex ||
      end_segment_index <= start_segment_index ||
      end_segment_index > segments->segments_size() ||
      array_size > kMaxArraySize) {
    return false;
  }

  string key;
  for (size_t i = start_segment_index; i < end_segment_index; ++i) {
    key += segments->segment(i).key();
  }

  if (key.empty()) {
    return false;
  }

  size_t consumed = 0;
  const size_t key_len = Util::CharsLen(key);
  vector<string> new_keys;
  new_keys.reserve(array_size + 1);

  for (size_t i = 0; i < array_size; ++i) {
    if (new_size_array[i] != 0 && consumed < key_len) {
      new_keys.push_back(Util::SubString(key, consumed, new_size_array[i]));
      consumed += new_size_array[i];
    }
  }
  if (consumed < key_len) {
    new_keys.push_back(Util::SubString(key, consumed, key_len - consumed));
  }

  segments->erase_segments(start_segment_index, segments_size);

  for (size_t i = 0; i < new_keys.size(); ++i) {
    Segment *seg = segments->insert_segment(start_segment_index + i);
    seg->set_segment_type(Segment::FIXED_BOUNDARY);
    seg->set_key(new_keys[i]);
  }

  segments->set_resized(true);

  if (!immutable_converter_->Convert(segments)) {
    return false;
  }

  rewriter_->Rewrite(request, segments);

  return true;
}

void ConverterImpl::CompletePOSIds(Segment::Candidate *candidate) const {
  DCHECK(candidate);
  if (candidate->value.empty() || candidate->key.empty()) {
    return;
  }

  if (candidate->lid != 0 && candidate->rid != 0) {
    return;
  }

  // Use general noun,  unknown word ("サ変") tend to produce
  // "する" "して", which are not always acceptable for non-sahen words.
  candidate->lid = general_noun_id_;
  candidate->rid = general_noun_id_;
  const size_t kExpandSizeStart = 5;
  const size_t kExpandSizeDiff = 50;
  const size_t kExpandSizeMax = 80;
  // In almost all cases, user choses the top candidate.
  // In order to reduce the latency, first, expand 5 candidates.
  // If no valid candidates are found within 5 candidates, expand
  // candidates step-by-step.
  for (size_t size = kExpandSizeStart; size < kExpandSizeMax;
       size += kExpandSizeDiff) {
    Segments segments;
    SetKey(&segments, candidate->key);
    // use PREDICTION mode, as the size of segments after
    // PREDICTION mode is always 1, thanks to real time conversion.
    // However, PREDICTION mode produces "predictions", meaning
    // that keys of result candidate are not always the same as
    // query key. It would be nice to have PREDICTION_REALTIME_CONVERSION_ONLY.
    segments.set_request_type(Segments::PREDICTION);
    segments.set_max_prediction_candidates_size(size);
    // In order to complete POSIds, call ImmutableConverter again.
    if (!immutable_converter_->Convert(&segments)) {
      LOG(ERROR) << "ImmutableConverter::Convert() failed";
      return;
    }
    for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
      const Segment::Candidate &ref_candidate =
          segments.segment(0).candidate(i);
      if (ref_candidate.value == candidate->value) {
        candidate->lid = ref_candidate.lid;
        candidate->rid = ref_candidate.rid;
        candidate->cost = ref_candidate.cost;
        candidate->wcost = ref_candidate.wcost;
        candidate->structure_cost = ref_candidate.structure_cost;
        VLOG(1) << "Set LID: " << candidate->lid;
        VLOG(1) << "Set RID: " << candidate->rid;
        return;
      }
    }
  }
  LOG(WARNING) << "Cannot set lid/rid. use default value. "
               << "key: " << candidate->key << ", "
               << "value: " << candidate->value << ", "
               << "lid: " << candidate->lid << ", "
               << "rid: " << candidate->rid;
}

UserDataManagerInterface *ConverterImpl::GetUserDataManager() {
  return user_data_manager_.get();
}

UserDataManagerImpl::~UserDataManagerImpl() {}

bool UserDataManagerImpl::Sync() {
  return (rewriter_->Sync() && predictor_->Sync());
}

bool UserDataManagerImpl::Reload() {
  return (rewriter_->Reload() && predictor_->Reload());
}

bool UserDataManagerImpl::ClearUserHistory() {
  rewriter_->Clear();
  return true;
}

bool UserDataManagerImpl::ClearUserPrediction() {
  predictor_->ClearAllHistory();
  return true;
}

bool UserDataManagerImpl::ClearUnusedUserPrediction() {
  predictor_->ClearUnusedHistory();
  return true;
}

void ConverterUtil::InitSegmentsFromString(const string &key,
                                           const string &preedit,
                                           Segments *segments) {
  segments->clear_conversion_segments();
  // the request mode is CONVERSION, as the user experience
  // is similar to conversion. UserHistryPredictor distinguishes
  // CONVERSION from SUGGESTION now.
  segments->set_request_type(Segments::CONVERSION);
  Segment *segment = segments->add_segment();
  segment->Clear();
  segment->set_key(key);
  segment->set_segment_type(Segment::FIXED_VALUE);
  Segment::Candidate *c = segment->add_candidate();
  c->Init();
  c->value = preedit;
  c->content_value = preedit;
  c->key = key;
  c->content_key = key;
}

}  // namespace mozc
