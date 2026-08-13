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

#ifndef MOZC_PREDICTION_ENGLISH_DECODER_H_
#define MOZC_PREDICTION_ENGLISH_DECODER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/strings/string_view.h"
#include "dictionary/dictionary_interface.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "request/conversion_request.h"

namespace mozc::prediction {

// Decoder specialized for English predictive candidate generation
// (dictionary lookup, case preservation/conversion, full-width/half-width
// ASCII support, raw input, and supplemental model).
//
// Concrete Conversion Examples:
//  1. Half-width ASCII input with case preservation:
//     - Key: "conv" -> Candidates: "converge", "converged", "convergent"
//     - Key: "CONV" -> Candidates: "CONVERGE", "CONVERGED", "CONVERGENT"
//     - Key: "Conv" -> Candidates: "Converge", "Converged", "Convergent"
//  2. Full-width ASCII input:
//     - Key: "ｃｏｎｖ" -> Candidates: "ｃｏｎｖｅｒｇｅ", "ｃｏｎｖｅｒｇｅｄ"
//     - Key: "ＣＯＮＶ" -> Candidates: "ＣＯＮＶＥＲＧＥ", "ＣＯＮＶＥＲＧＥＤ"
//     - Key: "Ｃｏｎｖ" -> Candidates: "Ｃｏｎｖｅｒｇｅ", "Ｃｏｎｖｅｒｇｅｄ"
//  3. Raw alphabet input from Roman/Kana composer:
//     - Raw keystroke sequence: "google" (even in kana preedit mode) ->
//     Candidate: "Google"
class EnglishDecoder {
 public:
  explicit EnglishDecoder(const engine::Modules& modules);

  // Decodes English predictive results using query key.
  std::vector<Result> Decode(const ConversionRequest& request) const;

  // Decodes English predictive results using raw input string from composer.
  std::vector<Result> DecodeUsingRawInput(
      const ConversionRequest& request) const;

 private:
  // Performs a custom look up for English words where case-conversion might be
  // applied to lookup key and/or output results.
  void GetPredictiveResultsForEnglishKey(const ConversionRequest& request,
                                         absl::string_view request_key,
                                         size_t lookup_limit,
                                         std::vector<Result>* results) const;

  const engine::Modules& modules_;
  const dictionary::DictionaryInterface& dictionary_;
  uint16_t zip_code_id_ = 0;
  uint16_t unknown_id_ = 0;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_ENGLISH_DECODER_H_
