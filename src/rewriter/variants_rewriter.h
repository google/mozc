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
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/util.h"
#include "converter/candidate.h"
#include "converter/inner_segment.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class VariantsRewriter : public RewriterInterface {
 public:
  // Annotation constants.
#ifdef __ANDROID__
  static constexpr absl::string_view kHiragana = "";
  static constexpr absl::string_view kKatakana = "";
  static constexpr absl::string_view kNumber = "";
  static constexpr absl::string_view kAlphabet = "";
  static constexpr absl::string_view kKanji = "";
#else   // __ANDROID__
  static constexpr absl::string_view kHiragana = "ひらがな";
  static constexpr absl::string_view kKatakana = "カタカナ";
  static constexpr absl::string_view kNumber = "数字";
  static constexpr absl::string_view kAlphabet = "アルファベット";
  static constexpr absl::string_view kKanji = "漢字";
#endif  // __ANDROID__
  static constexpr absl::string_view kFullWidth = "[全]";
  static constexpr absl::string_view kHalfWidth = "[半]";
  static constexpr absl::string_view kDidYouMean = "<もしかして>";
  static constexpr absl::string_view kYenKigou = "円記号";

  explicit VariantsRewriter(dictionary::PosMatcher pos_matcher)
      : pos_matcher_(pos_matcher) {}

  int capability(const ConversionRequest& request) const override;
  bool Rewrite(const ConversionRequest& request,
               Segments* segments) const override;
  void Finish(const ConversionRequest& request,
              const Segments& segments) override;
  void Clear() override;

  // Used by UserSegmentHistoryRewriter.
  // TODO(noriyukit): I'd be better to prepare some utility for rewriters.
  static void SetDescriptionForCandidate(dictionary::PosMatcher pos_matcher,
                                         converter::Candidate* candidate);
  static void SetDescriptionForTransliteration(
      dictionary::PosMatcher pos_matcher, converter::Candidate* candidate);
  static void SetDescriptionForPrediction(dictionary::PosMatcher pos_matcher,
                                          converter::Candidate* candidate);

  // Returns form types for given two pair of strings.
  // This function tries to find the difference between
  // |input1| and |input2| and find the place where the script
  // form (halfwidth/fullwidth) is different. This function returns
  // a pair of forms if input1 or input2 needs to have full/half width
  // annotation.
  //
  // Example:
  //  input1="ABCぐーぐる input2="ＡＢＣ"
  //  form1=Half form2=Full
  //
  // If input1 and input2 have mixed form types and the result
  // is ambiguous, this function returns {FULL_HALF_WIDTH, FULL_HALF_WIDTH}.
  //
  // Ambiguous case:
  //  input1="ABC１２３" input2="ＡＢＣ123"
  //  return {FULL_HALF_WIDTH, FULL_HALF_WIDTH}.
  static std::pair<Util::FormType, Util::FormType> GetFormTypesFromStringPair(
      absl::string_view input1, absl::string_view input2);

 private:
  // 1) Full width / half width description
  // 2) CharForm (hiragana/katakana) description
  // 3) Zipcode description (XXX-XXXX)
  //     * note that this overrides other descriptions
  enum DescriptionType {
    FULL_HALF_WIDTH = 1,  // automatically detect full/haflwidth.
    DEPRECATED_FULL_HALF_WIDTH_WITH_UNKNOWN = 2,  // Deprecated.
    // Set half/full width for symbols.
    // This flag must be used together with FULL_HALF_WIDTH.
    // If WITH_UNKNOWN is specified, assign FULL/HALF width annotation
    // more aggressively.
    HALF_WIDTH = 4,       // set half width description if applicable.
                          // otherwise, no width description is set.
    FULL_WIDTH = 8,       // set full width description if applicable.
                          // otherwise, no width description is set.
    CHARACTER_FORM = 16,  // Hiragana/Katakana..etc
    DEPRECATED_PLATFORM_DEPENDENT_CHARACTER = 32,  // Deprecated. "機種依存文字"
    ZIPCODE = 64,
    SPELLING_CORRECTION = 128
  };

  enum RewriteType {
    EXPAND_VARIANT = 0,  // Expand variants
    SELECT_VARIANT = 1,  // Select preferred form
  };

  static std::string GetDescription(dictionary::PosMatcher pos_matcher,
                                    int description_type,
                                    const converter::Candidate& candidate);
  static absl::string_view GetPrefix(int description_type,
                                     const converter::Candidate& candidate);
  static void SetDescription(dictionary::PosMatcher pos_matcher,
                             int description_type,
                             converter::Candidate* candidate);
  bool RewriteSegment(RewriteType type, Segment* seg) const;

  // Generates values for primary and secondary candidates.
  //
  // There are two orthogonal categories for two candidates.
  //
  // * {primary, secondary}: The primary candidate ranked higher than secondary.
  //   If the user has configured half-width value as primary, full-width value
  //   will be secondary.
  // * {original, alternative}: Original is the candidate that already exists in
  //   the segment before this rewriter. The original candidate is used as the
  //   input to this rewriter. Alternative is the candidate that is generated
  //   from the original candidate.
  //
  // original can be either primary or secondary. alternative will be the
  // opposite of original.
  //
  // Returns true if at least one of the values is modified.
  bool GenerateAlternatives(
      const converter::Candidate& original, std::string* primary_value,
      std::string* secondary_value, std::string* primary_content_value,
      std::string* secondary_content_value,
      converter::InnerSegmentBoundary* primary_inner_segment_boundary,
      converter::InnerSegmentBoundary* secondary_inner_segment_boundary) const;

  // Returns an alternative candidate and information for the base candidate.
  struct AlternativeCandidateResult {
    bool is_original_candidate_primary;
    int original_candidate_description_type;
    std::unique_ptr<converter::Candidate> alternative_candidate = nullptr;
  };
  AlternativeCandidateResult CreateAlternativeCandidate(
      const converter::Candidate& original_candidate) const;

  const dictionary::PosMatcher pos_matcher_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_VARIANTS_REWRITER_H_
