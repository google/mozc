// Copyright 2010-2020, Google Inc.
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

#include "rewriter/katakana_promotion_rewriter.h"

#include "base/util.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "transliteration/transliteration.h"

namespace mozc {

namespace {

bool MaybePromoteKatakana(Segment *segment) {
  if (segment->meta_candidates_size() <= transliteration::FULL_KATAKANA) {
    return false;
  }

  const Segment::Candidate &katakana_candidate =
      segment->meta_candidate(transliteration::FULL_KATAKANA);
  const std::string &katakana_value = katakana_candidate.value;
  if (!Util::IsScriptType(katakana_value, Util::KATAKANA)) {
    return false;
  }

  constexpr size_t kMaxRankForKatakana = 5;
  for (size_t i = 0;
       i < std::min(segment->candidates_size(), kMaxRankForKatakana); ++i) {
    if (segment->candidate(i).value == katakana_value) {
      // No need to promote or insert.
      return false;
    }
  }

  Segment::Candidate insert_candidate = katakana_candidate;
  size_t index = kMaxRankForKatakana;
  for (; index < segment->candidates_size(); ++index) {
    if (segment->candidate(index).value == katakana_value) {
      break;
    }
  }

  const size_t insert_pos =
      std::min(kMaxRankForKatakana, segment->candidates_size());
  if (index < segment->candidates_size()) {
    const Segment::Candidate insert_candidate = segment->candidate(index);
    *(segment->insert_candidate(insert_pos)) = insert_candidate;
  } else {
    *(segment->insert_candidate(insert_pos)) = katakana_candidate;
  }

  return true;
}

}  // namespace

KatakanaPromotionRewriter::KatakanaPromotionRewriter() = default;

KatakanaPromotionRewriter::~KatakanaPromotionRewriter() = default;

int KatakanaPromotionRewriter::capability(
    const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  } else {
    // Desktop version has a keybind to select katakana.
    return RewriterInterface::NOT_AVAILABLE;
  }
}

bool KatakanaPromotionRewriter::Rewrite(const ConversionRequest &request,
                                        Segments *segments) const {
  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    modified |= MaybePromoteKatakana(segments->mutable_conversion_segment(i));
  }
  return modified;
}

}  // namespace mozc
