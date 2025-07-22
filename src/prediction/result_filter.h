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

#ifndef MOZC_PREDICTION_RESULT_FILTER_H_
#define MOZC_PREDICTION_RESULT_FILTER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "converter/connector.h"
#include "dictionary/pos_matcher.h"
#include "prediction/result.h"
#include "prediction/suggestion_filter.h"
#include "request/conversion_request.h"

namespace mozc::prediction::filter {

class ResultFilter {
 public:
  ResultFilter(
      const ConversionRequest &request, dictionary::PosMatcher pos_matcher,
      const Connector &connector ABSL_ATTRIBUTE_LIFETIME_BOUND,
      const SuggestionFilter &suggestion_filter ABSL_ATTRIBUTE_LIFETIME_BOUND);

  bool ShouldRemove(const Result &result, int added_num);

 private:
  absl::string_view request_key_;
  std::string history_key_;
  std::string history_value_;
  const size_t request_key_len_;
  const dictionary::PosMatcher pos_matcher_;
  const Connector &connector_;
  const SuggestionFilter &suggestion_filter_;
  const bool is_mixed_conversion_;
  const bool auto_partial_suggestion_;
  const bool include_exact_key_;
  const bool is_handwriting_;
  const int suffix_nwp_transition_cost_threshold_;
  const int history_rid_ = 0;

  int suffix_count_ = 0;
  int predictive_count_ = 0;
  int realtime_count_ = 0;
  int prefix_tc_count_ = 0;
  int tc_count_ = 0;

  // Seen set for dup value check.
  absl::flat_hash_set<std::string> seen_;
};

// Returns the position of misspelled character position.
//
// Example:
// key: "れみおめろん"
// value: "レミオロメン"
// returns 3
//
// Example:
// key: "ろっぽんぎ"
// value: "六本木"
// returns 5 (charslen("ろっぽんぎ"))
size_t GetMissSpelledPosition(absl::string_view key, absl::string_view value);

// Removes redundant result from `results` based on the following algorithm.
// 1) Take the Result with minimum cost.
// 2) Remove results which is "redundant" (defined by MaybeRedundant in
//    result_filter.cc), from remaining results.
// 3) Repeat 1) and 2) five times.
// Note: to reduce the number of memory allocation, we swap out the
//   "redundant" results to the end of the `results` vector.
// TODO(taku): Better to pass the function object to define the redundant
// condition of two results.
void RemoveRedundantResults(std::vector<Result> *results);

}  // namespace mozc::prediction::filter
#endif  // MOZC_PREDICTION_RESULT_FILTER_H_
