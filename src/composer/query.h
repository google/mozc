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

#ifndef MOZC_COMPOSER_QUERY_H_
#define MOZC_COMPOSER_QUERY_H_

#include <cstdint>
#include <string>

namespace mozc::composer {

struct TypeCorrectedQuery {
  // Bit field of correction type.
  enum CorrectionType {
    NO_CORRECTION = 0,
    CORRECTION = 1,                    // Normal typing correction.
    COMPLETION = 2,                    // complete the rest of words/phrases.
    KANA_MODIFIER_INSENTIVE_ONLY = 4,  // Pure katukou conversion.
  };

  std::string correction;

  uint8_t type = NO_CORRECTION;

  // `score` is the score diff against identity score.
  // score = hyp_score - identity_score.
  // `score` can be used to determine the triggering condition.
  float score = 0.0;

  // `bias` is the score diff against the base score.
  // bias = hyp_score - base_score.
  // `bias` is used to calculate the penalty/bonus of the correction cost.
  // base_score is usually the same as the identity_score, but  We consider that
  // pure kana modifier insensitive correction is not an actual typing
  // correction. So when the top is a pure kana modifier insensitive correction,
  // uses the top score as the base score.
  float bias = 0.0;
};

}  // namespace mozc::composer

#endif  // MOZC_COMPOSER_QUERY_H_
