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

#include "engine/minimal_engine.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/strings/assign.h"
#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "data_manager/data_manager_interface.h"
#include "request/conversion_request.h"

namespace mozc {
namespace {

bool AddAsIsCandidate(const absl::string_view key, Segments *segments) {
  if (key.empty()) {
    return false;
  }
  if (segments == nullptr) {
    return false;
  }
  segments->Clear();
  Segment *segment = segments->add_segment();
  DCHECK(segment);

  Segment::Candidate *candidate = segment->push_back_candidate();
  DCHECK(candidate);
  strings::Assign(candidate->content_key, key);
  strings::Assign(candidate->content_value, key);
  strings::Assign(candidate->key, key);
  strings::Assign(candidate->value, key);
  candidate->lid = 0;
  candidate->rid = 0;
  candidate->wcost = 0;
  candidate->cost = 0;
  candidate->attributes = Segment::Candidate::DEFAULT_ATTRIBUTE;

  return true;
}

bool AddAsIsCandidate(const ConversionRequest &request, Segments *segments) {
  return AddAsIsCandidate(request.key(), segments);
}

class MinimalConverter : public ConverterInterface {
 public:
  MinimalConverter() = default;

  bool StartConversion(const ConversionRequest &request,
                       Segments *segments) const override {
    return AddAsIsCandidate(request, segments);
  }

  bool StartReverseConversion(Segments *segments,
                              const absl::string_view key) const override {
    return false;
  }

  bool StartPrediction(const ConversionRequest &request,
                       Segments *segments) const override {
    return AddAsIsCandidate(request, segments);
  }

  void FinishConversion(const ConversionRequest &request,
                        Segments *segments) const override {}

  void CancelConversion(Segments *segments) const override {}

  void ResetConversion(Segments *segments) const override {}

  void RevertConversion(Segments *segments) const override {}

  bool DeleteCandidateFromHistory(const Segments &segments,
                                  size_t segment_index,
                                  int candidate_index) const override {
    return true;
  }

  bool ReconstructHistory(
      Segments *segments,
      const absl::string_view preceding_text) const override {
    return true;
  }

  bool CommitSegmentValue(Segments *segments, size_t segment_index,
                          int candidate_index) const override {
    return true;
  }

  bool CommitPartialSuggestionSegmentValue(
      Segments *segments, size_t segment_index, int candidate_index,
      absl::string_view current_segment_key,
      absl::string_view new_segment_key) const override {
    return true;
  }

  bool FocusSegmentValue(Segments *segments, size_t segment_index,
                         int candidate_index) const override {
    return true;
  }

  bool CommitSegments(Segments *segments,
                      absl::Span<const size_t> candidate_index) const override {
    return true;
  }

  bool ResizeSegment(Segments *segments, const ConversionRequest &request,
                     size_t segment_index, int offset_length) const override {
    return true;
  }

  bool ResizeSegments(Segments *segments, const ConversionRequest &request,
                      size_t start_segment_index,
                      absl::Span<const uint8_t> new_size_array) const override {
    return true;
  }
};
}  // namespace

MinimalEngine::MinimalEngine()
    : converter_(std::make_unique<MinimalConverter>()),
      data_manager_(std::make_unique<DataManager>()) {}

ConverterInterface *MinimalEngine::GetConverter() const {
  return converter_.get();
}

absl::string_view MinimalEngine::GetPredictorName() const {
  constexpr absl::string_view kPredictorName = "MinimalPredictor";
  return kPredictorName;
}

const DataManagerInterface *MinimalEngine::GetDataManager() const {
  return data_manager_.get();
}

}  // namespace mozc
