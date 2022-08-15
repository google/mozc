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

#include "rewriter/normalization_rewriter.h"

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "converter/segments.h"
#include "request/conversion_request.h"

namespace mozc {
namespace {

bool CheckCodePointsAcceptable(const std::string value) {
  for (ConstChar32Iterator iter(value); !iter.Done(); iter.Next()) {
    if (!Util::IsAcceptableCharacterAsCandidate(iter.Get())) {
      // remove candidate here
      return false;
      break;
    }
  }
  return true;
}

bool NormalizeCandidate(Segment::Candidate *candidate,
                        TextNormalizer::Flag flag) {
  DCHECK(candidate);
  if (candidate->attributes & Segment::Candidate::USER_DICTIONARY) {
    return false;
  }

  const std::string value =
      TextNormalizer::NormalizeTextWithFlag(candidate->value, flag);
  const std::string content_value =
      TextNormalizer::NormalizeTextWithFlag(candidate->content_value, flag);

  if (content_value == candidate->content_value && value == candidate->value) {
    // No update.
    return false;
  }

  candidate->value = std::move(value);
  candidate->content_value = std::move(content_value);
  // Clear the description which might be wrong.
  candidate->description.clear();

  return true;
}
}  // namespace

int NormalizationRewriter::capability(const ConversionRequest &request) const {
  return RewriterInterface::ALL;
}

bool NormalizationRewriter::Rewrite(const ConversionRequest &request,
                                    Segments *segments) const {
  DCHECK(segments);

  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    DCHECK(segment);

    // Meta candidate
    for (size_t j = 0; j < segment->meta_candidates_size(); ++j) {
      Segment::Candidate *candidate =
          segment->mutable_candidate(-static_cast<int>(j) - 1);
      DCHECK(candidate);
      modified |= NormalizeCandidate(candidate, flag_);
    }

    // Regular candidate.
    const size_t candidates_size = segment->candidates_size();
    for (size_t j = 0; j < candidates_size; ++j) {
      const size_t reversed_j = candidates_size - j - 1;
      Segment::Candidate *candidate = segment->mutable_candidate(reversed_j);
      DCHECK(candidate);

      // Character Normalization
      modified |= NormalizeCandidate(candidate, flag_);

      // Check acceptability of code points as a candidate.
      if (!CheckCodePointsAcceptable(candidate->value)) {
        segment->erase_candidate(reversed_j);
        modified = true;
      }
    }
  }

  return modified;
}
}  // namespace mozc
