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

#ifndef MOZC_REWRITER_VARIANTS_REWRITER_H_
#define MOZC_REWRITER_VARIANTS_REWRITER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/port.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/rewriter_interface.h"
#include "absl/strings/string_view.h"

namespace mozc {

class VariantsRewriter : public RewriterInterface {
 public:
  // Annotation constants.
#ifdef OS_ANDROID
  static constexpr absl::string_view kHiragana = "";
  static constexpr absl::string_view kKatakana = "";
  static constexpr absl::string_view kNumber = "";
  static constexpr absl::string_view kAlphabet = "";
  static constexpr absl::string_view kKanji = "";
#else   // OS_ANDROID
  static constexpr absl::string_view kHiragana = "ひらがな";
  static constexpr absl::string_view kKatakana = "カタカナ";
  static constexpr absl::string_view kNumber = "数字";
  static constexpr absl::string_view kAlphabet = "アルファベット";
  static constexpr absl::string_view kKanji = "漢字";
#endif  // OS_ANDROID
  static constexpr absl::string_view kFullWidth = "[全]";
  static constexpr absl::string_view kHalfWidth = "[半]";
  static constexpr absl::string_view kDidYouMean = "<もしかして>";
  static constexpr absl::string_view kYenKigou = "円記号";

  explicit VariantsRewriter(dictionary::POSMatcher pos_matcher);
  ~VariantsRewriter() override;
  int capability(const ConversionRequest &request) const override;
  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;
  void Finish(const ConversionRequest &request, Segments *segments) override;
  void Clear() override;

  // Used by UserSegmentHistoryRewriter.
  // TODO(noriyukit): I'd be better to prepare some utility for rewriters.
  static void SetDescriptionForCandidate(
      const dictionary::POSMatcher &pos_matcher, Segment::Candidate *candidate);
  static void SetDescriptionForTransliteration(
      const dictionary::POSMatcher &pos_matcher, Segment::Candidate *candidate);
  static void SetDescriptionForPrediction(
      const dictionary::POSMatcher &pos_matcher, Segment::Candidate *candidate);

 private:
  // 1) Full width / half width description
  // 2) CharForm (hiragana/katakana) description
  // 3) Zipcode description (XXX-XXXX)
  //     * note that this overrides other descriptions
  enum DescriptionType {
    FULL_HALF_WIDTH = 1,               // automatically detect full/haflwidth.
    FULL_HALF_WIDTH_WITH_UNKNOWN = 2,  // set half/full widith for symbols.
    // This flag must be used together with FULL_HALF_WIDTH.
    // If WITH_UNKNOWN is specified, assign FULL/HALF width annotation
    // more aggressively.
    HALF_WIDTH = 4,       // always set half width description.
    FULL_WIDTH = 8,       // always set full width description.
    CHARACTER_FORM = 16,  // Hiragana/Katakana..etc
    DEPRECATED_PLATFORM_DEPENDENT_CHARACTER = 32,  // Deprecated. "機種依存文字"
    ZIPCODE = 64,
    SPELLING_CORRECTION = 128
  };

  enum RewriteType {
    EXPAND_VARIANT = 0,  // Expand variants
    SELECT_VARIANT = 1,  // Select preferred form
  };

  static void SetDescription(const dictionary::POSMatcher &pos_matcher,
                             int description_type,
                             Segment::Candidate *candidate);
  bool RewriteSegment(RewriteType type, Segment *seg) const;
  bool GenerateAlternatives(
      const Segment::Candidate &original, std::string *default_value,
      std::string *alternative_value, std::string *default_content_value,
      std::string *alternative_content_value,
      std::vector<uint32_t> *default_inner_segment_boundary,
      std::vector<uint32_t> *alternative_inner_segment_boundary) const;

  const dictionary::POSMatcher pos_matcher_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_VARIANTS_REWRITER_H_
