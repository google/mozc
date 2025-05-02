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

#ifndef MOZC_REWRITER_TRANSLITERATION_REWRITER_H_
#define MOZC_REWRITER_TRANSLITERATION_REWRITER_H_

#include <cstdint>
#include <string>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class TransliterationRewriter : public RewriterInterface {
 public:
  explicit TransliterationRewriter(const dictionary::PosMatcher &pos_matcher);
  TransliterationRewriter(const TransliterationRewriter &) = delete;
  TransliterationRewriter &operator=(const TransliterationRewriter &) = delete;
  ~TransliterationRewriter() override = default;

  int capability(const ConversionRequest &request) const override;

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

 private:
  void InitT13nCandidate(absl::string_view key, absl::string_view value,
                         uint16_t lid, uint16_t rid,
                         Segment::Candidate *cand) const;
  // Sets transliteration values into segment.  If t13ns is invalid,
  // false is returned.
  bool SetTransliterations(absl::Span<const std::string> t13ns,
                           absl::string_view key, Segment *segment) const;
  bool FillT13nsFromComposer(const ConversionRequest &request,
                             Segments *segments) const;
  bool FillT13nsFromKey(Segments *segments) const;
  bool AddRawNumberT13nCandidates(const ConversionRequest &request,
                                  Segments *segments) const;

  const uint16_t unknown_id_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_TRANSLITERATION_REWRITER_H_
