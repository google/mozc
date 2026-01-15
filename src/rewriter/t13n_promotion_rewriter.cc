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

#include "rewriter/t13n_promotion_rewriter.h"

#include <algorithm>
#include <cstddef>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/rewriter_util.h"
#include "transliteration/transliteration.h"

namespace mozc {

namespace {

// The insertion offset for T13N candidates.
// Only one of Latin T13n candidates (width, case variants for Latin character
// keys) and katakana T13n candidates (Katakana variants for other keys) will
// be promoted.
constexpr size_t kLatinT13nOffset = 3;

bool IsLatinInputMode(const ConversionRequest& request) {
  return request.composer().GetInputMode() == transliteration::HALF_ASCII ||
         request.composer().GetInputMode() == transliteration::FULL_ASCII;
}

bool MaybeInsertLatinT13n(Segment* segment) {
  if (segment->meta_candidates_size() <=
      transliteration::FULL_ASCII_CAPITALIZED) {
    return false;
  }

  const size_t insert_pos =
      RewriterUtil::CalculateInsertPosition(*segment, kLatinT13nOffset);

  absl::flat_hash_set<absl::string_view> seen;
  for (size_t i = 0; i < insert_pos; ++i) {
    seen.insert(segment->candidate(i).value);
  }

  static constexpr transliteration::TransliterationType kLatinT13nTypes[] = {
      transliteration::HALF_ASCII,
      transliteration::HALF_ASCII_UPPER,
      transliteration::HALF_ASCII_LOWER,
      transliteration::HALF_ASCII_CAPITALIZED,
      transliteration::FULL_ASCII,
      transliteration::FULL_ASCII_UPPER,
      transliteration::FULL_ASCII_LOWER,
      transliteration::FULL_ASCII_CAPITALIZED,
  };

  size_t pos = insert_pos;
  for (const auto t13n_type : kLatinT13nTypes) {
    const converter::Candidate& t13n_candidate =
        segment->meta_candidate(t13n_type);
    auto [it, inserted] = seen.insert(t13n_candidate.value);
    if (!inserted) {
      continue;
    }
    *(segment->insert_candidate(pos)) = t13n_candidate;
    ++pos;
  }
  return pos != insert_pos;
}

// Inserts or promote Katakana candidate at `insert_pos`.
// promotes Katakana candidate if `segment` already contains Katakana.
// Katakana candidate is searched from `start_offset`.
// When no Katakana is found, `katakana_candidate` is inserted.
void InsertKatakana(int start_offset, int insert_pos,
                    const converter::Candidate& katakana_candidate,
                    Segment* segment) {
  int katakana_index = -1;
  for (int i = start_offset; i < segment->candidates_size(); ++i) {
    if (segment->candidate(i).value == katakana_candidate.value) {
      katakana_index = i;
      break;
    }
  }

  if (katakana_index >= 0) {
    segment->move_candidate(katakana_index, insert_pos);
  } else {
    *(segment->insert_candidate(insert_pos)) = katakana_candidate;
  }
}

bool MaybePromoteKatakanaWithStaticOffset(
    const commands::DecoderExperimentParams& params,
    const converter::Candidate& katakana_candidate, Segment* segment) {
  if (params.katakana_promotion_offset() < 0) {
    return false;
  }

  const int katakana_t13n_offset = params.katakana_promotion_offset();

  // Katakana candidate already appears at lower than katakana_t13n_offset.
  for (size_t i = 0;
       i < std::min<int>(segment->candidates_size(), katakana_t13n_offset);
       ++i) {
    // No need to promote or insert.
    if (segment->candidate(i).value == katakana_candidate.value) {
      return false;
    }
  }

  const size_t insert_pos =
      RewriterUtil::CalculateInsertPosition(*segment, katakana_t13n_offset);

  InsertKatakana(katakana_t13n_offset, insert_pos, katakana_candidate, segment);

  return true;
}

bool MaybePromoteKatakana(const ConversionRequest& request, Segment* segment) {
  if (segment->meta_candidates_size() <= transliteration::FULL_KATAKANA) {
    return false;
  }

  const auto& params = request.request().decoder_experiment_params();
  const converter::Candidate& katakana_candidate =
      segment->meta_candidate(transliteration::FULL_KATAKANA);

  return MaybePromoteKatakanaWithStaticOffset(params, katakana_candidate,
                                              segment);
}

bool MaybePromoteT13n(const ConversionRequest& request, Segment* segment) {
  if (IsLatinInputMode(request) || Util::IsAscii(segment->key())) {
    return MaybeInsertLatinT13n(segment);
  }
  return MaybePromoteKatakana(request, segment);
}

}  // namespace

T13nPromotionRewriter::T13nPromotionRewriter() = default;

T13nPromotionRewriter::~T13nPromotionRewriter() = default;

int T13nPromotionRewriter::capability(const ConversionRequest& request) const {
  if (request.request().mixed_conversion()) {  // For mobile
    return RewriterInterface::ALL;
  } else {
    return RewriterInterface::NOT_AVAILABLE;
  }
}

bool T13nPromotionRewriter::Rewrite(const ConversionRequest& request,
                                    Segments* segments) const {
  bool modified = false;
  for (Segment& segment : segments->conversion_segments()) {
    modified |= MaybePromoteT13n(request, &segment);
  }
  return modified;
}

}  // namespace mozc
