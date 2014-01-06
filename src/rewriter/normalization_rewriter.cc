// Copyright 2010-2014, Google Inc.
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
#include <vector>

#include "base/logging.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"

namespace mozc {
namespace {

enum CandidateType {
  CANDIDATE,
  TRANSLITERATION
};

bool NormalizeCandidate(Segment::Candidate *candidate,
                        CandidateType type) {
  DCHECK(candidate);
  if (candidate->attributes & Segment::Candidate::USER_DICTIONARY) {
    return false;
  }

  string value, content_value;
  switch (type) {
    case CANDIDATE:
      TextNormalizer::NormalizeCandidateText(candidate->value, &value);
      TextNormalizer::NormalizeCandidateText(candidate->content_value,
                                             &content_value);
      break;
    case TRANSLITERATION:
      TextNormalizer::NormalizeTransliterationText(candidate->value, &value);
      TextNormalizer::NormalizeTransliterationText(candidate->content_value,
                                                   &content_value);
      break;
    default:
      LOG(ERROR) << "unkown type";
      return false;
  }

  const bool modified = (content_value != candidate->content_value ||
                         value != candidate->value);
  candidate->value = value;
  candidate->content_value = content_value;

  return modified;
}
}  // namespace

NormalizationRewriter::NormalizationRewriter() {}

NormalizationRewriter::~NormalizationRewriter() {}

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
          segment->mutable_candidate(-static_cast<int>(j)-1);
      DCHECK(candidate);
      modified |= NormalizeCandidate(candidate, TRANSLITERATION);
    }

    // Regular candidate.
    for (size_t j = 0; j < segment->candidates_size(); ++j) {
      Segment::Candidate *candidate = segment->mutable_candidate(j);
      DCHECK(candidate);
      modified |= NormalizeCandidate(candidate, CANDIDATE);
    }
  }

  return modified;
}
}  // namespace mozc
