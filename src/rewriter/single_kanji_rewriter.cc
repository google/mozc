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

#include "rewriter/single_kanji_rewriter.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/strings/assign.h"
#include "base/vlog.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/serialized_dictionary.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/rewriter_util.h"

using mozc::dictionary::PosMatcher;

namespace mozc {

namespace {

void InsertNounPrefix(const PosMatcher& pos_matcher, Segment* segment,
                      SerializedDictionary::iterator begin,
                      SerializedDictionary::iterator end) {
  DCHECK(begin != end);

  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return;
  }

  if (segment->segment_type() == Segment::FIXED_VALUE) {
    return;
  }

  absl::string_view candidate_key =
      ((!segment->key().empty()) ? segment->key() : segment->candidate(0).key);
  for (auto iter = begin; iter != end; ++iter) {
    // Note:
    // The entry "cost" of noun_prefix_dictionary is "0" or "1".
    // Please refer to: mozc/rewriter/gen_single_kanji_noun_prefix_data.cc
    const int insert_pos = RewriterUtil::CalculateInsertPosition(
        *segment,
        static_cast<int>(iter.cost() + (segment->candidate(0).attributes &
                                        converter::Attribute::CONTEXT_SENSITIVE)
                             ? 1
                             : 0));
    converter::Candidate* c = segment->insert_candidate(insert_pos);
    c->lid = pos_matcher.GetNounPrefixId();
    c->rid = pos_matcher.GetNounPrefixId();
    c->cost = 5000;
    strings::Assign(c->content_value, iter.value());
    c->key = candidate_key;
    c->content_key = candidate_key;
    strings::Assign(c->value, iter.value());
    c->attributes |= converter::Attribute::CONTEXT_SENSITIVE;
    c->attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;
  }
}

}  // namespace

SingleKanjiRewriter::SingleKanjiRewriter(
    const dictionary::PosMatcher& pos_matcher,
    const dictionary::SingleKanjiDictionary& single_kanji_dictionary)
    : pos_matcher_(pos_matcher),
      single_kanji_dictionary_(single_kanji_dictionary) {}

SingleKanjiRewriter::~SingleKanjiRewriter() = default;

int SingleKanjiRewriter::capability(const ConversionRequest& request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool SingleKanjiRewriter::Rewrite(const ConversionRequest& request,
                                  Segments* segments) const {
  if (!request.config().use_single_kanji_conversion()) {
    MOZC_VLOG(2) << "no use_single_kanji_conversion";
    return false;
  }
  if (request.request().mixed_conversion() &&
      request.request_type() != ConversionRequest::CONVERSION) {
    MOZC_VLOG(2) << "single kanji prediction is enabled";
    // Single kanji entries are populated in the predictor when mixed conversion
    // mode, so we only sets the description of single kanji.
    for (Segment& segment : segments->conversion_segments()) {
      AddDescriptionForExistingCandidates(&segment);
    }
    return true;
  }

  bool modified = false;
  const Segments::range conversion_segments = segments->conversion_segments();
  const size_t segments_size = conversion_segments.size();
  const bool is_single_segment = (segments_size == 1);
  const bool use_svs = (request.request()
                            .decoder_experiment_params()
                            .variation_character_types() &
                        commands::DecoderExperimentParams::SVS_JAPANESE);
  for (Segment& segment : conversion_segments) {
    AddDescriptionForExistingCandidates(&segment);

    const std::vector<std::string> kanji_list =
        single_kanji_dictionary_.LookupKanjiEntries(segment.key(), use_svs);
    if (kanji_list.empty()) {
      continue;
    }
    modified |=
        InsertCandidate(is_single_segment, pos_matcher_.GetGeneralSymbolId(),
                        kanji_list, &segment);
  }

  // Tweak for noun prefix.
  // TODO(team): Ideally, this issue can be fixed via the language model
  // and dictionary generation.
  for (size_t i = 0; i < segments_size; ++i) {
    Segment& segment = conversion_segments[i];
    if (segment.candidates_size() == 0) {
      continue;
    }

    if (i + 1 < segments_size) {
      const converter::Candidate& right_candidate =
          conversion_segments[i + 1].candidate(0);
      // right segment must be a noun.
      if (!pos_matcher_.IsContentNoun(right_candidate.lid)) {
        continue;
      }
    } else if (segments_size != 1) {  // also apply if segments_size == 1.
      continue;
    }

    absl::string_view key = segment.key();
    const auto range = single_kanji_dictionary_.LookupNounPrefixEntries(key);
    if (range.first == range.second) {
      continue;
    }
    InsertNounPrefix(pos_matcher_, &segment, range.first, range.second);
    // Ignore the next noun content word.
    ++i;
    modified = true;
  }

  return modified;
}

// Add single kanji variants description to existing candidates,
// because if we have candidates with same value, the lower ranked candidate
// will be removed.
void SingleKanjiRewriter::AddDescriptionForExistingCandidates(
    Segment* segment) const {
  DCHECK(segment);
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    converter::Candidate* cand = segment->mutable_candidate(i);
    if (!cand->description.empty()) {
      continue;
    }
    single_kanji_dictionary_.GenerateDescription(cand->value,
                                                 &cand->description);
  }
}

// Insert SingleKanji into segment.
bool SingleKanjiRewriter::InsertCandidate(
    bool is_single_segment, uint16_t single_kanji_id,
    absl::Span<const std::string> kanji_list, Segment* segment) const {
  DCHECK(segment);
  DCHECK(!kanji_list.empty());
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return false;
  }

  absl::string_view candidate_key =
      ((!segment->key().empty()) ? segment->key() : segment->candidate(0).key);

  // Adding 8000 to the single kanji cost
  // Note that this cost does not make no effect.
  // Here we set the cost just in case.
  constexpr int kOffsetCost = 8000;

  // Append single-kanji
  for (size_t i = 0; i < kanji_list.size(); ++i) {
    converter::Candidate* c = segment->push_back_candidate();
    FillCandidate(candidate_key, kanji_list[i], kOffsetCost + i,
                  single_kanji_id, c);
  }
  return true;
}

void SingleKanjiRewriter::FillCandidate(const absl::string_view key,
                                        const absl::string_view value,
                                        const int cost,
                                        const uint16_t single_kanji_id,
                                        converter::Candidate* cand) const {
  cand->lid = single_kanji_id;
  cand->rid = single_kanji_id;
  cand->cost = cost;
  strings::Assign(cand->content_key, key);
  strings::Assign(cand->content_value, value);
  strings::Assign(cand->key, key);
  strings::Assign(cand->value, value);
  cand->attributes |= converter::Attribute::CONTEXT_SENSITIVE;
  cand->attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;
  single_kanji_dictionary_.GenerateDescription(value, &cand->description);
}
}  // namespace mozc
