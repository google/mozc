// Copyright 2010-2011, Google Inc.
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

#include "converter/converter_interface.h"

#include <string>
#include <utility>
#include <vector>
#include "base/base.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/util.h"
#include "composer/composer.h"
#include "transliteration/transliteration.h"
#include "converter/focus_candidate_handler.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "prediction/predictor_interface.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

size_t kErrorIndex = static_cast<size_t>(-1);

class ConverterImpl : public ConverterInterface {
 public:
  ConverterImpl();
  virtual ~ConverterImpl();

  bool StartConversion(Segments *segments,
                       const string &key) const;
  bool StartConversionWithComposer(Segments *segments,
                                   const composer::Composer *composer) const;
  bool StartReverseConversion(Segments *segments,
                              const string &key) const;
  bool StartPrediction(Segments *segments,
                       const string &key) const;
  bool StartSuggestion(Segments *segments,
                       const string &key) const;
  bool FinishConversion(Segments *segments) const;
  bool CancelConversion(Segments *segments) const;
  bool ResetConversion(Segments *segments) const;
  bool RevertConversion(Segments *segments) const;
  bool CommitSegmentValue(Segments *segments,
                          size_t segment_index,
                          int    candidate_index) const;
  bool FocusSegmentValue(Segments *segments,
                         size_t segment_index,
                         int candidate_index) const;
  bool FreeSegmentValue(Segments *segments,
                        size_t segment_index) const;
  bool SubmitFirstSegment(Segments *segments,
                          size_t candidate_index) const;
  bool ResizeSegment(Segments *segments,
                     size_t segment_index,
                     int offset_length) const;
  bool ResizeSegment(Segments *segments,
                     size_t start_segment_index,
                     size_t segments_size,
                     const uint8 *new_size_array,
                     size_t array_size) const;
  bool Sync() const;
  bool Reload() const;
  bool ClearUserHistory() const;
  bool ClearUserPrediction() const;
  bool ClearUnusedUserPrediction() const;
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
  segments->clear_lattice();
  segments->clear_revert_entries();
  segments->set_composer(NULL);

  mozc::Segment *seg = segments->add_segment();
  DCHECK(seg);

  seg->Clear();
  seg->set_key(key);
  seg->set_segment_type(mozc::Segment::FREE);

  VLOG(2) << segments->DebugString();
}

ConverterInterface *g_converter = NULL;
}  // namespace

ConverterInterface *ConverterFactory::GetConverter() {
  if (g_converter == NULL) {
    return Singleton<ConverterImpl>::get();
  } else {
    return g_converter;
  }
}

void ConverterFactory::SetConverter(ConverterInterface *converter) {
  g_converter = converter;
}

ConverterImpl::ConverterImpl() {}
ConverterImpl::~ConverterImpl() {}

bool ConverterImpl::StartConversion(Segments *segments,
                                    const string &key) const {
  SetKey(segments, key);
  segments->set_request_type(Segments::CONVERSION);
  if (ImmutableConverterFactory::GetImmutableConverter()->Convert(segments)) {
    RewriterFactory::GetRewriter()->Rewrite(segments);
    return true;
  }
  return false;
}

bool ConverterImpl::StartConversionWithComposer(
    Segments *segments, const composer::Composer *composer) const {
  string conversion_key;
  if (composer != NULL) {
    composer->GetQueryForConversion(&conversion_key);
  }
  SetKey(segments, conversion_key);
  segments->set_composer(composer);
  segments->set_request_type(Segments::CONVERSION);
  if (ImmutableConverterFactory::GetImmutableConverter()->Convert(segments)) {
    RewriterFactory::GetRewriter()->Rewrite(segments);
    return true;
  }
  return false;
}

bool ConverterImpl::StartReverseConversion(Segments *segments,
                                           const string &key) const {
  segments->Clear();
  SetKey(segments, key);
  segments->set_request_type(Segments::REVERSE_CONVERSION);
  if (!ImmutableConverterFactory::GetImmutableConverter()->Convert(segments)) {
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

bool ConverterImpl::StartPrediction(Segments *segments,
                                    const string &key) const {
  SetKey(segments, key);
  segments->set_request_type(Segments::PREDICTION);
  return PredictorFactory::GetPredictor()->Predict(segments);
}

bool ConverterImpl::StartSuggestion(Segments *segments,
                                    const string &key) const {
  SetKey(segments, key);
  segments->set_request_type(Segments::SUGGESTION);
  return PredictorFactory::GetPredictor()->Predict(segments);
}

bool ConverterImpl::FinishConversion(Segments *segments) const {
  // revert SUBMITTED segments to FIXED_VALUE
  for (int i = 0; i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    if (seg->segment_type() == Segment::SUBMITTED) {
      seg->set_segment_type(Segment::FIXED_VALUE);
    }
  }

  segments->clear_revert_entries();
  if (segments->request_type() == Segments::CONVERSION) {
    RewriterFactory::GetRewriter()->Finish(segments);
  }
  PredictorFactory::GetPredictor()->Finish(segments);

  segments->clear_lattice();

  const int start_index = max(0,
                              static_cast<int>(
                                  segments->segments_size() -
                                  segments->max_history_segments_size()));
  for (int i = 0; i < start_index; ++i) {
    segments->pop_front_segment();
  }

  for (size_t i = 0; i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    seg->set_segment_type(Segment::HISTORY);
  }

  return true;
}

bool ConverterImpl::CancelConversion(Segments *segments) const {
  segments->clear_lattice();
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
  PredictorFactory::GetPredictor()->Revert(segments);
  segments->clear_revert_entries();
  return true;
}

bool ConverterImpl::CommitSegmentValue(Segments *segments,
                                       size_t segment_index,
                                       int candidate_index) const {
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

  segment->set_segment_type(Segment::FIXED_VALUE);
  segment->move_candidate(candidate_index, 0);

  if (candidate_index != 0) {
    segment->mutable_candidate(0)->attributes
        |= Segment::Candidate::RERANKED;
  }

  return true;
}

bool ConverterImpl::FocusSegmentValue(Segments *segments,
                                      size_t segment_index,
                                      int    candidate_index) const {
  segment_index = GetSegmentIndex(segments, segment_index);
  if (segment_index == kErrorIndex) {
    return false;
  }

  return FocusCandidateHandler::FocusSegmentValue(segments,
                                                  segment_index,
                                                  candidate_index);
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

  return ImmutableConverterFactory::GetImmutableConverter()->Convert(segments);
}

bool ConverterImpl::SubmitFirstSegment(Segments *segments,
                                       size_t candidate_index) const {
  if (!CommitSegmentValue(segments, 0, candidate_index)) {
    return false;
  }

  const size_t segment_index = GetSegmentIndex(segments, 0);
  Segment *segment = segments->mutable_segment(segment_index);
  segment->set_segment_type(Segment::SUBMITTED);
  return true;
}

bool ConverterImpl::ResizeSegment(Segments *segments,
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
  string tmp;

  if (offset_length > 0) {
    int length = offset_length;
    string last_key;
    string new_key = cur_segment_key;
    size_t last_clen = 0;
    while (segment_index + 1 < segments->segments_size()) {
      last_key = segments->segment(segment_index + 1).key();
      segments->erase_segment(segment_index + 1);
      last_clen = Util::CharsLen(last_key.c_str(), last_key.size());
      length -= static_cast<int>(last_clen);
      if (length <= 0) {
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

  if (!ImmutableConverterFactory::GetImmutableConverter()->Convert(segments)) {
    return false;
  }

  RewriterFactory::GetRewriter()->Rewrite(segments);

  return true;
}

bool ConverterImpl::ResizeSegment(Segments *segments,
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

  if (!ImmutableConverterFactory::GetImmutableConverter()->Convert(segments)) {
    return false;
  }

  RewriterFactory::GetRewriter()->Rewrite(segments);

  return true;
}

bool ConverterImpl::Sync() const {
  return (RewriterFactory::GetRewriter()->Sync() &&
          PredictorFactory::GetPredictor()->Sync() &&
          DictionaryFactory::GetDictionary()->Sync());
}

bool ConverterImpl::Reload() const {
  return (RewriterFactory::GetRewriter()->Reload() &&
          PredictorFactory::GetPredictor()->Reload() &&
          DictionaryFactory::GetDictionary()->Reload());
}

bool ConverterImpl::ClearUserHistory() const {
  RewriterFactory::GetRewriter()->Clear();
  return true;
}

bool ConverterImpl::ClearUserPrediction() const {
  PredictorFactory::GetPredictor()->ClearAllHistory();
  return true;
}

bool ConverterImpl::ClearUnusedUserPrediction() const {
  PredictorFactory::GetPredictor()->ClearUnusedHistory();
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
