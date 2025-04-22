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

#include <tuple>

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "base/absl_nullability.h"
#include "base/strings/unicode.h"
#include "composer/query.h"
#include "converter/segments.h"
#include "dictionary/dictionary_token.h"
#include "prediction/zero_query_dict.h"

namespace mozc {
namespace prediction {
namespace result_internal {

bool ValueLess(absl::string_view lhs, absl::string_view rhs) {
  // This loop processes lhs and rhs in one pass.
  while (!lhs.empty() && !rhs.empty()) {
    // Read each Unicode character.
    absl::string_view lhs_char, rhs_char;
    std::tie(lhs_char, lhs) = strings::FrontChar(lhs);
    std::tie(rhs_char, rhs) = strings::FrontChar(rhs);
    if (lhs_char != rhs_char) {
      // If they're different, count the number of characters first.
      auto lhs_it = lhs.begin(), rhs_it = rhs.begin();
      while (lhs_it != lhs.end() && rhs_it != rhs.end()) {
        lhs_it += strings::OneCharLen(*lhs_it);
        rhs_it += strings::OneCharLen(*rhs_it);
      }
      if (lhs_it == lhs.end() && rhs_it == rhs.end()) {
        // Same lengths, use the codepoint order.
        return lhs_char < rhs_char;
      } else {
        // Different lengths. Check if lhs is shorter.
        return lhs_it == lhs.end();
      }
    }
  }
  // Here, the numbers of chars in lhs and rhs are different.
  // Return true if the remaining lhs is shorter than the remaining lhs.
  return lhs.size() < rhs.size();
}

}  // namespace result_internal

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

void PopulateTypeCorrectedQuery(
    const composer::TypeCorrectedQuery &typing_corrected_result,
    Result *absl_nonnull result) {
  if (typing_corrected_result.type & composer::TypeCorrectedQuery::CORRECTION) {
    result->types |= TYPING_CORRECTION;
  }
  if (typing_corrected_result.type & composer::TypeCorrectedQuery::COMPLETION) {
    result->types |= TYPING_COMPLETION;
  }
  result->typing_correction_score = typing_corrected_result.score;
  // bias = hyp_score - base_score, so larger is better.
  // bias is computed in log10 domain, so we need to use the different
  // scale factor. 500 * log(10) = ~1150.
  const int adjustment = -1150 * typing_corrected_result.bias;
  result->typing_correction_adjustment = adjustment;
  result->wcost += adjustment;
}

}  // namespace prediction
}  // namespace mozc
