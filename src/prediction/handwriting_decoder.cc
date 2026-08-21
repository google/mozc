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

#include "prediction/handwriting_decoder.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "converter/attribute.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "engine/modules.h"
#include "prediction/decoder_util.h"
#include "prediction/realtime_decoder.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_util.h"

namespace mozc::prediction {
namespace {

using ::mozc::converter::Attribute;
using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::Token;

}  // namespace

HandwritingDecoder::HandwritingDecoder(const engine::Modules& modules,
                                       const RealtimeDecoder& realtime_decoder)
    : dictionary_(modules.GetDictionary()),
      realtime_decoder_(realtime_decoder) {}

std::vector<Result> HandwritingDecoder::Decode(
    const ConversionRequest& request) const {
  if (!request_util::IsHandwriting(request)) {
    return {};
  }
  if (request.request_type() != ConversionRequest::PREDICTION &&
      request.request_type() != ConversionRequest::SUGGESTION) {
    return {};
  }

  std::vector<Result> results;
  const ResultsSizeAdjuster adjuster(request, &results);

  const commands::DecoderExperimentParams& param =
      request.request().decoder_experiment_params();
  const int handwriting_cost_offset =
      param.handwriting_conversion_candidate_cost_offset();

  int processed_count = 0;
  const int size_to_process = param.max_composition_event_to_process();
  absl::Span<const commands::SessionCommand::CompositionEvent>
      composition_events = request.composer().GetHandwritingCompositions();
  for (size_t i = 0; i < composition_events.size(); ++i) {
    const commands::SessionCommand::CompositionEvent& elm =
        composition_events[i];
    if (elm.probability() <= 0.0) {
      continue;
    }
    const int recognition_cost = -500.0 * log(elm.probability());
    constexpr int kAsisCostOffset = 3453;  // 500 * log(1000) = ~3453
    Result asis_result = {
        .key = elm.composition_string(),
        .value = elm.composition_string(),
        .attributes =
            (Attribute::UNIGRAM | Attribute::NO_VARIANTS_EXPANSION |
             Attribute::NO_EXTRA_DESCRIPTION | Attribute::NO_MODIFICATION),
        // Set small cost for the top recognition result.
        .wcost = (i == 0) ? 0 : kAsisCostOffset + recognition_cost,
    };

    const std::optional<HandwritingQueryInfo> query_info =
        processed_count < size_to_process
            ? GenerateQueryForHandwriting(request, elm)
            : std::nullopt;
    if (query_info.has_value()) {
      ++processed_count;

      dictionary::InlineCallback cb;
      cb.OnToken([&](absl::string_view key, absl::string_view actual_key,
                     const Token& token) {
        using enum DictionaryInterface::Callback::ResultType;
        const int penalty = handwriting_cost_offset + recognition_cost;
        size_t next_pos = 0;
        for (absl::string_view constraint : query_info->constraints) {
          const size_t pos = token.value.find(constraint, next_pos);
          if (pos == std::string::npos) {
            return TRAVERSE_CONTINUE;
          }
          next_pos = pos + 1;
        }
        Result result;
        result.InitializeByTokenAndTypes(token, UNIGRAM);
        result.wcost += penalty;
        results.emplace_back(std::move(result));
        return (results.size() < adjuster.cutoff_threshold())
                   ? TRAVERSE_CONTINUE
                   : TRAVERSE_DONE;
      });

      dictionary_.LookupExact(query_info->query, request.options(), &cb);

      // Rewrite key with the look-up query.
      asis_result.key = query_info->query;
    }
    results.emplace_back(std::move(asis_result));
  }

  return results;
}

std::optional<HandwritingDecoder::HandwritingQueryInfo>
HandwritingDecoder::GenerateQueryForHandwriting(
    const ConversionRequest& request,
    const commands::SessionCommand::CompositionEvent& composition_event) const {
  if (composition_event.probability() < 0.0001) {
    // Skip generating the query info for unconfident composition,
    // since running reverse conversion is slow.
    return std::nullopt;
  }
  if (absl::StrContains(composition_event.composition_string(), " ")) {
    // Skip providing converted candidates for queries including white space.
    return std::nullopt;
  }
  if (!Util::ContainsScriptType(composition_event.composition_string(),
                                Util::HIRAGANA)) {
    // Skip providing converted candidates for queries not including Hiragana.
    return std::nullopt;
  }

  const ConversionRequest request_for_realtime =
      ConversionRequestBuilder()
          .SetConversionRequestView(request)
          .SetRequestType(ConversionRequest::REVERSE_CONVERSION)
          .SetKey(composition_event.composition_string())
          .Build();

  HandwritingQueryInfo info;
  std::vector<Result> results =
      realtime_decoder_.ReverseDecode(request_for_realtime);
  if (results.empty()) return info;

  Result& result = results.front();
  info.query = std::move(result.value);

  // b/324976556:
  // We have to use the segment key instead of the candidate key.
  // candidate key does not always match segment key for T13N chars.
  std::string utf8_str;
  const Utf8AsChars original_chars(result.key);
  for (const absl::string_view c : original_chars) {
    if (Util::GetScriptType(c) != Util::HIRAGANA) {
      absl::StrAppend(&utf8_str, c);
    } else if (!utf8_str.empty()) {
      info.constraints.emplace_back(utf8_str);
      utf8_str.clear();
    }
  }
  if (!utf8_str.empty()) {
    info.constraints.emplace_back(utf8_str);
  }

  return info;
}

}  // namespace mozc::prediction
