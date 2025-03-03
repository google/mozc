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

#ifndef MOZC_REWRITER_CALCULATOR_REWRITER_H_
#define MOZC_REWRITER_CALCULATOR_REWRITER_H_

#include <cstddef>
#include <optional>

#include "absl/strings/string_view.h"
#include "converter/segments.h"
#include "rewriter/calculator/calculator.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class CalculatorRewriterTest;
class ConversionRequest;
class ConversionRequest;
class ConverterInterface;
class Segments;

class CalculatorRewriter : public RewriterInterface {
 public:
  friend class CalculatorRewriterTest;

  int capability(const ConversionRequest &request) const override;

  std::optional<ResizeSegmentsRequest> CheckResizeSegmentsRequest(
      const ConversionRequest &request,
      const Segments &segments) const override;

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

 private:
  // Inserts a candidate with the string into the |segment|.
  // Position of insertion is indicated by |insert_pos|. It returns false if
  // insertion is failed.
  bool InsertCandidate(absl::string_view value, size_t insert_pos,
                       Segment *segment) const;

  const Calculator calculator_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_CALCULATOR_REWRITER_H_
