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

#ifndef MOZC_PREDICTION_DECODER_UTIL_H_
#define MOZC_PREDICTION_DECODER_UTIL_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "prediction/result.h"
#include "request/conversion_request.h"

namespace mozc::prediction {

inline constexpr size_t kSuggestionMaxResultsSize = 256;
inline constexpr size_t kPredictionMaxResultsSize = 100000;

// Returns candidate cutoff threshold depending on the request type.
size_t GetCandidateCutoffThreshold(ConversionRequest::RequestType request_type);

// Returns true if `token` is a noisy number token for the given input `key`
// with `original_key_len`.
bool IsNoisyNumberToken(absl::string_view key, size_t original_key_len,
                        const dictionary::Token& token);

// RAII class to adjust the result size to be `cutoff_threshold`.
class ResultsSizeAdjuster {
 public:
  ResultsSizeAdjuster(const ConversionRequest& request,
                      std::vector<Result>* results);

  ~ResultsSizeAdjuster() { AdjustSize(); }

  void AdjustSize() const;

  // Returns the span of newly added results.
  absl::Span<Result> GetAddedResults() const {
    return absl::Span<Result>(*results_).subspan(prev_size_);
  }

  size_t cutoff_threshold() const { return cutoff_threshold_; }

 private:
  const size_t cutoff_threshold_ = 0;
  std::vector<Result>* results_ = nullptr;
  const size_t prev_size_ = 0;
};

// Common callback for predictive dictionary lookup.
class PredictiveLookupCallback
    : public dictionary::DictionaryInterface::Callback {
 public:
  PredictiveLookupCallback(PredictionTypes types, size_t limit,
                           size_t original_key_len,
                           const absl::btree_set<std::string>& subsequent_chars,
                           int zip_code_id, int unknown_id,
                           std::vector<Result>* results);

  PredictiveLookupCallback(PredictionTypes types, size_t limit,
                           size_t original_key_len, int zip_code_id,
                           int unknown_id, std::vector<Result>* results);

  PredictiveLookupCallback(const PredictiveLookupCallback&) = delete;
  PredictiveLookupCallback& operator=(const PredictiveLookupCallback&) = delete;

  virtual void RewriteResult(Result& result) const {}

  ResultType OnKey(absl::string_view key) override;

  ResultType OnActualKey(absl::string_view key, absl::string_view actual_key,
                         int num_expanded) override;

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const dictionary::Token& token) override;

 protected:
  int32_t penalty_ = 0;
  const PredictionTypes types_;
  const size_t limit_;
  const size_t original_key_len_;
  const absl::btree_set<std::string>* subsequent_chars_ = nullptr;
  const int zip_code_id_;
  const int unknown_id_;
  std::vector<Result>* results_ = nullptr;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_DECODER_UTIL_H_
