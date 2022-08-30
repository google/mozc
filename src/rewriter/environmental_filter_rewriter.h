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

// This rewriter is used for environment specific filtering.
// There are roughly three major roles of this rewriter.
//
// 1. Normalization
// There were characters which should be rewriten in some platforms. For
// example, in Windows environment, U+FF0D is preferred than U+2212 for the
// glyph of 'full-width minus', due to historical reason. This rewriter rewrites
// candidate containing U+2212 if the environment is Windows.
//
// 2. Validation
// This rewriter checks validity as UTF-8 string for each candidate. If
// unacceptable candidates were to be found, this rewriter removes such
// candidates.
//
// 3. Unavailable glyph removal
// There are some glyphs that can be in candidates but not always available
// among environments. For example, newer emojis tend to be unavailable in old
// OSes. In order to prevend such glyphs appearing as candidates, this rewriter
// removes candidates containing unavailable glyphs. Information about font
// availability in environments are sent by clients.
#ifndef MOZC_REWRITER_ENVIRONMENTAL_FILTER_REWRITER_H_
#define MOZC_REWRITER_ENVIRONMENTAL_FILTER_REWRITER_H_

#include "base/text_normalizer.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class ConversionRequest;
class Segments;

class EnvironmentalFilterRewriter : public RewriterInterface {
 public:
  EnvironmentalFilterRewriter() = default;
  ~EnvironmentalFilterRewriter() override = default;

  int capability(const ConversionRequest &request) const override;

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;
  void SetNormalizationFlag(TextNormalizer::Flag flag) { flag_ = flag; }

 private:
  // Controls the normalization behavior.
  TextNormalizer::Flag flag_ = TextNormalizer::kDefault;
};
}  // namespace mozc
#endif  // MOZC_REWRITER_ENVIRONMENTAL_FILTER_REWRITER_H_
