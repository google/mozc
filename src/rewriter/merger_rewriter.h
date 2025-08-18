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

#ifndef MOZC_REWRITER_MERGER_REWRITER_H_
#define MOZC_REWRITER_MERGER_REWRITER_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class MergerRewriter : public RewriterInterface {
 public:
  MergerRewriter() = default;
  ~MergerRewriter() override = default;

  MergerRewriter(const MergerRewriter&) = delete;
  MergerRewriter& operator=(const MergerRewriter&) = delete;

  void AddRewriter(std::unique_ptr<RewriterInterface> rewriter) {
    DCHECK(rewriter);
    rewriters_.push_back(std::move(rewriter));
  }

  std::optional<ResizeSegmentsRequest> CheckResizeSegmentsRequest(
      const ConversionRequest& request, const Segments& segments) const {
    if (segments.resized()) {
      return std::nullopt;
    }

    for (const std::unique_ptr<RewriterInterface>& rewriter : rewriters_) {
      std::optional<ResizeSegmentsRequest> resize_request =
          rewriter->CheckResizeSegmentsRequest(request, segments);
      if (resize_request.has_value()) {
        return resize_request;
      }
    }
    return std::nullopt;
  }

  bool Rewrite(const ConversionRequest& request,
               Segments* segments) const override {
    if (segments == nullptr) {
      return false;
    }

    const CapabilityType capability_type = [&request]() {
      switch (request.request_type()) {
        case ConversionRequest::CONVERSION:
          return RewriterInterface::CONVERSION;
        case ConversionRequest::PREDICTION:
        case ConversionRequest::PARTIAL_PREDICTION:
          return RewriterInterface::PREDICTION;
        case ConversionRequest::SUGGESTION:
        case ConversionRequest::PARTIAL_SUGGESTION:
          return RewriterInterface::SUGGESTION;
        case ConversionRequest::REVERSE_CONVERSION:
        default:
          return RewriterInterface::NOT_AVAILABLE;
      }
    }();

    bool is_updated = false;
    for (const std::unique_ptr<RewriterInterface>& rewriter : rewriters_) {
      if (rewriter->capability(request) & capability_type) {
        is_updated |= rewriter->Rewrite(request, segments);
      }
    }

    if (request.request_type() == ConversionRequest::SUGGESTION &&
        segments->conversion_segments_size() == 1 &&
        !request.request().mixed_conversion()) {
      const size_t max_suggestions = request.config().suggestions_size();
      Segment* segment = segments->mutable_conversion_segment(0);
      const size_t candidate_size = segment->candidates_size();
      if (candidate_size > max_suggestions) {
        segment->erase_candidates(max_suggestions,
                                  candidate_size - max_suggestions);
      }
    }
    return is_updated;
  }

  // This method is mainly called when user puts SPACE key
  // and changes the focused candidate.
  // In this method, Converter will find bracketing matching.
  // e.g., when user selects "「",  corresponding closing bracket "」"
  // is chosen in the preedit.
  bool Focus(Segments* segments, size_t segment_index,
             int candidate_index) const override {
    bool result = false;
    for (const std::unique_ptr<RewriterInterface>& rewriter : rewriters_) {
      result |= rewriter->Focus(segments, segment_index, candidate_index);
    }
    return result;
  }

  // Hook(s) for all mutable operations
  void Finish(const ConversionRequest& request,
              const Segments& segments) override {
    for (const std::unique_ptr<RewriterInterface>& rewriter : rewriters_) {
      rewriter->Finish(request, segments);
    }
  }

  void Revert(const Segments& segments) override {
    for (const std::unique_ptr<RewriterInterface>& rewriter : rewriters_) {
      rewriter->Revert(segments);
    }
  }

  bool ClearHistoryEntry(const Segments& segments, size_t segment_index,
                         int candidate_index) override {
    bool result = false;
    for (const std::unique_ptr<RewriterInterface>& rewriter : rewriters_) {
      result |=
          rewriter->ClearHistoryEntry(segments, segment_index, candidate_index);
    }
    return result;
  }

  // Syncs internal data to local file system.
  bool Sync() override {
    bool result = false;
    for (const std::unique_ptr<RewriterInterface>& rewriter : rewriters_) {
      result |= rewriter->Sync();
    }
    return result;
  }

  // Reloads internal data from local file system.
  bool Reload() override {
    bool result = false;
    for (const std::unique_ptr<RewriterInterface>& rewriter : rewriters_) {
      result |= rewriter->Reload();
    }
    return result;
  }

  // Clears internal data
  void Clear() override {
    for (const std::unique_ptr<RewriterInterface>& rewriter : rewriters_) {
      rewriter->Clear();
    }
  }

 private:
  std::vector<std::unique_ptr<RewriterInterface>> rewriters_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_MERGER_REWRITER_H_
