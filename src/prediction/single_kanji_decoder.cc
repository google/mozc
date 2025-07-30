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

#include "prediction/single_kanji_decoder.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/strings/assign.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/candidate.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_util.h"

namespace mozc::prediction {

namespace {

bool UseSvs(const ConversionRequest &request) {
  return request.request()
             .decoder_experiment_params()
             .variation_character_types() &
         commands::DecoderExperimentParams::SVS_JAPANESE;
}

void StripLastChar(std::string *key) {
  const size_t key_len = Util::CharsLen(*key);
  if (key_len <= 1) {
    *key = "";
    return;
  }
  Util::Utf8SubString(*key, 0, key_len - 1, key);
}

}  // namespace

SingleKanjiDecoder::SingleKanjiDecoder(
    const dictionary::PosMatcher &pos_matcher,
    const dictionary::SingleKanjiDictionary &single_kanji_dictionary)
    : single_kanji_dictionary_(single_kanji_dictionary),
      general_symbol_id_(pos_matcher.GetGeneralSymbolId()) {}

SingleKanjiDecoder::~SingleKanjiDecoder() = default;

std::vector<Result> SingleKanjiDecoder::Decode(
    const ConversionRequest &request) const {
  std::vector<Result> results;
  constexpr int kMinSingleKanjiSize = 5;

  const bool use_svs = UseSvs(request);

  const std::string original_request_key =
      request.composer().GetQueryForPrediction();
  int offset = 0;
  for (std::string key = original_request_key; !key.empty();
       StripLastChar(&key)) {
    if (!request_util::IsAutoPartialSuggestionEnabled(request) &&
        key != original_request_key) {
      // Do not include partial results
      break;
    }
    const std::vector<std::string> kanji_list =
        single_kanji_dictionary_.LookupKanjiEntries(key, use_svs);
    if (kanji_list.empty()) {
      continue;
    }
    AppendResults(key, original_request_key, kanji_list, offset, &results);
    // Make sure that single kanji entries for shorter key should be
    // ranked lower than the entries for longer key.
    constexpr int kShorterKeyOffst = 3450;  // 500 * log(1000)
    offset += kShorterKeyOffst;
    if (results.size() > kMinSingleKanjiSize) {
      break;
    }
  }
  return results;
}

void SingleKanjiDecoder::AppendResults(absl::string_view kanji_key,
                                       absl::string_view original_request_key,
                                       absl::Span<const std::string> kanji_list,
                                       const int offset,
                                       std::vector<Result> *results) const {
  for (const std::string &kanji : kanji_list) {
    Result result;
    // Set the wcost to keep the `kanji_list` order.
    result.wcost = offset + results->size();
    result.types = SINGLE_KANJI;
    strings::Assign(result.key, kanji_key);
    result.value = kanji;
    result.lid = general_symbol_id_;
    result.rid = general_symbol_id_;
    if (kanji_key.size() < original_request_key.size()) {
      result.candidate_attributes =
          converter::Candidate::PARTIALLY_KEY_CONSUMED;
      result.consumed_key_size = Util::CharsLen(kanji_key);
    }

    results->emplace_back(std::move(result));
  }
}

}  // namespace mozc::prediction
