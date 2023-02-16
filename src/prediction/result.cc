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

#include "prediction/result.h"

#include "base/util.h"
#include "converter/segments.h"
#include "prediction/zero_query_dict.h"

namespace mozc {
namespace prediction {

using ::mozc::dictionary::Token;

void Result::InitializeByTokenAndTypes(const Token &token,
                                       PredictionTypes types) {
  SetTypesAndTokenAttributes(types, token.attributes);
  key = token.key;
  value = token.value;
  wcost = token.cost;
  lid = token.lid;
  rid = token.rid;
}

void Result::SetTypesAndTokenAttributes(PredictionTypes prediction_types,
                                        Token::AttributesBitfield token_attr) {
  types = prediction_types;
  candidate_attributes = 0;
  if (types & TYPING_CORRECTION) {
    candidate_attributes |= Segment::Candidate::TYPING_CORRECTION;
  }
  if (types & (REALTIME | REALTIME_TOP)) {
    candidate_attributes |= Segment::Candidate::REALTIME_CONVERSION;
  }
  if (types & REALTIME_TOP) {
    candidate_attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  }
  if (types & PREFIX) {
    candidate_attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
  }
  if (token_attr & Token::SPELLING_CORRECTION) {
    candidate_attributes |= Segment::Candidate::SPELLING_CORRECTION;
  }
  if (token_attr & Token::USER_DICTIONARY) {
    candidate_attributes |= (Segment::Candidate::USER_DICTIONARY |
                             Segment::Candidate::NO_MODIFICATION |
                             Segment::Candidate::NO_VARIANTS_EXPANSION);
  }
}

void Result::SetSourceInfoForZeroQuery(ZeroQueryType type) {
  switch (type) {
    case ZERO_QUERY_NONE:
      source_info |= Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NONE;
      return;
    case ZERO_QUERY_NUMBER_SUFFIX:
      source_info |=
          Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX;
      return;
    case ZERO_QUERY_EMOTICON:
      source_info |=
          Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOTICON;
      return;
    case ZERO_QUERY_EMOJI:
      source_info |= Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOJI;
      return;
    case ZERO_QUERY_BIGRAM:
      source_info |= Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM;
      return;
    case ZERO_QUERY_SUFFIX:
      source_info |= Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX;
      return;
    default:
      LOG(ERROR) << "Should not come here";
      return;
  }
}

bool Result::IsUserDictionaryResult() const {
  return (candidate_attributes & Segment::Candidate::USER_DICTIONARY) != 0;
}

}  // namespace prediction
}  // namespace mozc
