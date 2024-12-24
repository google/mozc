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

#ifndef MOZC_REWRITER_COLLOCATION_REWRITER_H_
#define MOZC_REWRITER_COLLOCATION_REWRITER_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "storage/existence_filter.h"

namespace mozc {
namespace collocation_rewriter_internal {

class CollocationFilter {
 public:
  explicit CollocationFilter(storage::ExistenceFilter filter)
      : filter_(std::move(filter)) {}

  static absl::StatusOr<CollocationFilter> Create(
      absl::Span<const uint32_t> data);

  bool Exists(absl::string_view left, absl::string_view right) const;

 private:
  storage::ExistenceFilter filter_;
};

class SuppressionFilter {
 public:
  explicit SuppressionFilter(storage::ExistenceFilter filter)
      : filter_(std::move(filter)) {}

  static absl::StatusOr<SuppressionFilter> Create(
      absl::Span<const uint32_t> data);

  bool Exists(const Segment::Candidate &cand) const;

 private:
  storage::ExistenceFilter filter_;
};

}  // namespace collocation_rewriter_internal

class CollocationRewriter : public RewriterInterface {
 public:
  CollocationRewriter(
      dictionary::PosMatcher pos_matcher,
      collocation_rewriter_internal::CollocationFilter collocation_filter,
      collocation_rewriter_internal::SuppressionFilter suppression_filter)
      : pos_matcher_(std::move(pos_matcher)),
        first_name_id_(pos_matcher_.GetFirstNameId()),
        last_name_id_(pos_matcher_.GetLastNameId()),
        collocation_filter_(std::move(collocation_filter)),
        suppression_filter_(std::move(suppression_filter)) {}

  static std::unique_ptr<CollocationRewriter> Create(
      const DataManager &data_manager);

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

 private:
  bool IsName(const Segment::Candidate &cand) const;
  bool RewriteFromPrevSegment(const Segment::Candidate &prev_cand,
                              Segment *seg) const;
  bool RewriteUsingNextSegment(Segment *next_seg, Segment *seg) const;
  bool RewriteCollocation(Segments *segments) const;

  const dictionary::PosMatcher pos_matcher_;
  const uint16_t first_name_id_;
  const uint16_t last_name_id_;

  // Used to test if pairs of strings are in collocation data. Since it's a
  // bloom filter, non-collocation words are sometimes mistakenly boosted,
  // although the possibility is very low (0.001% by default).
  collocation_rewriter_internal::CollocationFilter collocation_filter_;

  // Used to test if pairs of content key and value are "ateji". Since it's a
  // bloom filter, non-ateji words are sometimes mistakenly classified as ateji,
  // resulting in passing on the right collocations, though the possibility is
  // very low (0.001% by default).
  collocation_rewriter_internal::SuppressionFilter suppression_filter_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_COLLOCATION_REWRITER_H_
