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

#ifndef MOZC_CONVERTER_CONVERTER_H_
#define MOZC_CONVERTER_CONVERTER_H_

#include <memory>
#include <string>

#include "converter/converter_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
//  for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"
#include "absl/strings/string_view.h"

namespace mozc {

class ConversionRequest;
class ImmutableConverterInterface;
class PredictorInterface;
class RewriterInterface;
class Segments;

class ConverterImpl : public ConverterInterface {
 public:
  ConverterImpl();
  ~ConverterImpl() override;

  // Lazily initializes the internal members. Must be called before the use.
  void Init(const dictionary::POSMatcher *pos_matcher,
            const dictionary::SuppressionDictionary *suppression_dictionary,
            std::unique_ptr<PredictorInterface> predictor,
            std::unique_ptr<RewriterInterface> rewriter,
            ImmutableConverterInterface *immutable_converter);

  bool Predict(const ConversionRequest &request, const std::string &key,
               const Segments::RequestType request_type,
               Segments *segments) const;

  bool StartConversionForRequest(const ConversionRequest &request,
                                 Segments *segments) const override;
  bool StartConversion(Segments *segments,
                       const std::string &key) const override;
  bool StartReverseConversion(Segments *segments,
                              const std::string &key) const override;
  bool StartPredictionForRequest(const ConversionRequest &request,
                                 Segments *segments) const override;
  bool StartPrediction(Segments *segments,
                       const std::string &key) const override;
  bool StartSuggestionForRequest(const ConversionRequest &request,
                                 Segments *segments) const override;
  bool StartSuggestion(Segments *segments,
                       const std::string &key) const override;
  bool StartPartialPredictionForRequest(const ConversionRequest &request,
                                        Segments *segments) const override;
  bool StartPartialPrediction(Segments *segments,
                              const std::string &key) const override;
  bool StartPartialSuggestionForRequest(const ConversionRequest &request,
                                        Segments *segments) const override;
  bool StartPartialSuggestion(Segments *segments,
                              const std::string &key) const override;

  bool FinishConversion(const ConversionRequest &request,
                        Segments *segments) const override;
  bool CancelConversion(Segments *segments) const override;
  bool ResetConversion(Segments *segments) const override;
  bool RevertConversion(Segments *segments) const override;
  bool ReconstructHistory(Segments *segments,
                          const std::string &preceding_text) const override;

  bool CommitSegmentValue(Segments *segments, size_t segment_index,
                          int candidate_index) const override;
  bool CommitPartialSuggestionSegmentValue(
      Segments *segments, size_t segment_index, int candidate_index,
      absl::string_view current_segment_key,
      absl::string_view new_segment_key) const override;
  bool FocusSegmentValue(Segments *segments, size_t segment_index,
                         int candidate_index) const override;
  bool FreeSegmentValue(Segments *segments,
                        size_t segment_index) const override;
  bool CommitSegments(
      Segments *segments,
      const std::vector<size_t> &candidate_index) const override;
  bool ResizeSegment(Segments *segments, const ConversionRequest &request,
                     size_t segment_index, int offset_length) const override;
  bool ResizeSegment(Segments *segments, const ConversionRequest &request,
                     size_t start_segment_index, size_t segments_size,
                     const uint8 *new_size_array,
                     size_t array_size) const override;

 private:
  FRIEND_TEST(ConverterTest, CompletePOSIds);
  FRIEND_TEST(ConverterTest, DefaultPredictor);
  FRIEND_TEST(ConverterTest, MaybeSetConsumedKeySizeToSegment);
  FRIEND_TEST(ConverterTest, GetLastConnectivePart);

  // Complete Left id/Right id if they are not defined.
  // Some users don't push conversion button but directly
  // input hiragana sequence only with composition mode. Converter
  // cannot know which POS ids should be used for these directly-
  // input strings. This function estimates IDs from value heuristically.
  void CompletePOSIds(Segment::Candidate *candidate) const;

  bool CommitSegmentValueInternal(Segments *segments, size_t segment_index,
                                  int candidate_index,
                                  Segment::SegmentType segment_type) const;

  // Sets all the candidates' attribute PARTIALLY_KEY_CONSUMED
  // and consumed_key_size if the attribute is not set.
  static void MaybeSetConsumedKeySizeToCandidate(size_t consumed_key_size,
                                                 Segment::Candidate *candidate);

  // Sets all the candidates' attribute PARTIALLY_KEY_CONSUMED
  // and consumed_key_size if the attribute is not set.
  static void MaybeSetConsumedKeySizeToSegment(size_t consumed_key_size,
                                               Segment *segment);

  // Rewrites and applies the suppression dictionary.
  void RewriteAndSuppressCandidates(const ConversionRequest &request,
                                    Segments *segments) const;

  // Limits the number of candidates based on a request.
  // This method doesn't drop meta candidates for T13n conversion.
  void TrimCandidates(const ConversionRequest &request,
                      Segments *segments) const;

  // Commits usage stats for committed text.
  // |begin_segment_index| is a index of whole segments. (history and conversion
  // segments)
  void CommitUsageStats(const Segments *segments, size_t begin_segment_index,
                        size_t segment_length) const;

  // Returns the substring of |str|. This substring consists of similar script
  // type and you can use it as preceding text for conversion.
  bool GetLastConnectivePart(const std::string &preceding_text,
                             std::string *key, std::string *value,
                             uint16 *id) const;

  const dictionary::POSMatcher *pos_matcher_;
  const dictionary::SuppressionDictionary *suppression_dictionary_;
  std::unique_ptr<PredictorInterface> predictor_;
  std::unique_ptr<RewriterInterface> rewriter_;
  const ImmutableConverterInterface *immutable_converter_;
  uint16 general_noun_id_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERTER_H_
