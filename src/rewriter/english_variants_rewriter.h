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

#ifndef MOZC_REWRITER_ENGLISH_VARIANTS_REWRITER_H_
#define MOZC_REWRITER_ENGLISH_VARIANTS_REWRITER_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
class ConversionRequest;

class EnglishVariantsRewriter : public RewriterInterface {
 public:
  explicit EnglishVariantsRewriter(dictionary::PosMatcher pos_matcher)
      : pos_matcher_(pos_matcher) {}
  ~EnglishVariantsRewriter() override = default;

  int capability(const ConversionRequest &request) const override;

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

 private:
  friend class EnglishVariantsRewriterTestPeer;

  bool IsT13NCandidate(Segment::Candidate *candidate) const;
  bool IsEnglishCandidate(Segment::Candidate *candidate) const;
  bool ExpandEnglishVariants(absl::string_view input,
                             std::vector<std::string> *variants) const;
  bool ExpandSpacePrefixedVariants(absl::string_view input,
                                   std::vector<std::string> *variants) const;
  bool ExpandEnglishVariantsWithSegment(bool need_space_prefix,
                                        Segment *seg) const;

  const dictionary::PosMatcher pos_matcher_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_ENGLISH_VARIANTS_REWRITER_H_
