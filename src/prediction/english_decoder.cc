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

#include "prediction/english_decoder.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "base/japanese_util.h"
#include "base/number_util.h"
#include "base/util.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "engine/modules.h"
#include "prediction/decoder_util.h"
#include "prediction/result.h"
#include "request/conversion_request.h"
#include "transliteration/transliteration.h"

namespace mozc::prediction {


EnglishDecoder::EnglishDecoder(const engine::Modules& modules)
    : modules_(modules),
      dictionary_(modules.GetDictionary()),
      zip_code_id_(modules.GetPosMatcher().GetZipcodeId()),
      unknown_id_(modules.GetPosMatcher().GetUnknownId()) {}

std::vector<Result> EnglishDecoder::Decode(
    const ConversionRequest& request) const {
  if (request.request_type() == ConversionRequest::CONVERSION ||
      request.key().empty()) {
    return {};
  }
  std::vector<Result> results;
  const size_t cutoff = GetCandidateCutoffThreshold(request.request_type());
  GetPredictiveResultsForEnglishKey(request, request.key(), cutoff, &results);
  modules_.GetSupplementalModel().DecodeEnglish(request, results);
  return results;
}

std::vector<Result> EnglishDecoder::DecodeUsingRawInput(
    const ConversionRequest& request) const {
  if (request.request_type() == ConversionRequest::CONVERSION ||
      request.composer().GetRawString().empty()) {
    return {};
  }
  std::vector<Result> results;
  const size_t cutoff = GetCandidateCutoffThreshold(request.request_type());
  GetPredictiveResultsForEnglishKey(request, request.composer().GetRawString(),
                                    cutoff, &results);
  modules_.GetSupplementalModel().DecodeEnglish(request, results);
  return results;
}

void EnglishDecoder::GetPredictiveResultsForEnglishKey(
    const ConversionRequest& request, const absl::string_view request_key,
    size_t lookup_limit, std::vector<Result>* results) const {
  DCHECK(results);
  const size_t prev_results_size = results->size();
  if (Util::IsUpperAscii(request_key)) {
    // For upper case key, look up its lower case version and then transform
    // the results to upper case.
    std::string key(request_key);
    Util::LowerString(&key);
    PredictiveLookupCallback callback(ENGLISH, lookup_limit, key.size(),
                                      zip_code_id_, unknown_id_, results);
    dictionary_.LookupPredictive(key, request.options(), &callback);
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      Util::UpperString(&(*results)[i].value);
    }
  } else if (Util::IsCapitalizedAscii(request_key)) {
    // For capitalized key, look up its lower case version and then transform
    // the results to capital.
    std::string key(request_key);
    Util::LowerString(&key);
    PredictiveLookupCallback callback(ENGLISH, lookup_limit, key.size(),
                                      zip_code_id_, unknown_id_, results);
    dictionary_.LookupPredictive(key, request.options(), &callback);
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      Util::CapitalizeString(&(*results)[i].value);
    }
  } else {
    // For other cases (lower and as-is), just look up directly.
    PredictiveLookupCallback callback(ENGLISH, lookup_limit, request_key.size(),
                                      zip_code_id_, unknown_id_, results);
    dictionary_.LookupPredictive(request_key, request.options(), &callback);
  }
  // If input mode is FULL_ASCII, then convert the results to full-width.
  if (request.composer().GetInputMode() == transliteration::FULL_ASCII) {
    std::string tmp;
    for (size_t i = prev_results_size; i < results->size(); ++i) {
      tmp.assign((*results)[i].value);
      (*results)[i].value = japanese_util::HalfWidthAsciiToFullWidthAscii(tmp);
    }
  }
}

}  // namespace mozc::prediction
