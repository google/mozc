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

#ifndef MOZC_REWRITER_NUMBER_REWRITER_H_
#define MOZC_REWRITER_NUMBER_REWRITER_H_

#include <vector>

#include "absl/types/span.h"
#include "base/container/serialized_string_array.h"
#include "base/number_util.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

// A rewriter to expand number styles (NumberUtil::NumberString::Style)
class NumberRewriter : public RewriterInterface {
 public:
  explicit NumberRewriter(const DataManager *data_manager);
  NumberRewriter(const NumberRewriter &) = delete;
  NumberRewriter &operator=(const NumberRewriter &) = delete;
  ~NumberRewriter() override;

  int capability(const ConversionRequest &request) const override;

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

  void Finish(const ConversionRequest &request, Segments *segments) override;

 private:
  bool RewriteOneSegment(const ConversionRequest &request, Segment *segment,
                         Segments *segments) const;
  void RememberNumberStyle(const Segment::Candidate &candidate);
  std::vector<Segment::Candidate> GenerateCandidatesToInsert(
      const Segment::Candidate &arabic_candidate,
      absl::Span<const NumberUtil::NumberString> numbers,
      bool should_rerank) const;
  bool ShouldRerankCandidates(const ConversionRequest &request,
                              const Segments &segments) const;
  void RerankCandidates(std::vector<Segment::Candidate> &candidates) const;

  SerializedStringArray suffix_array_;
  const dictionary::PosMatcher pos_matcher_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_NUMBER_REWRITER_H_
