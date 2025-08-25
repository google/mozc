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

#ifndef MOZC_CONVERTER_IMMUTABLE_CONVERTER_H_
#define MOZC_CONVERTER_IMMUTABLE_CONVERTER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/connector.h"
#include "converter/immutable_converter_interface.h"
#include "converter/lattice.h"
#include "converter/nbest_generator.h"
#include "converter/node.h"
#include "converter/node_list_builder.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "prediction/suggestion_filter.h"
#include "request/conversion_request.h"

namespace mozc {

class ImmutableConverter : public ImmutableConverterInterface {
 public:
  explicit ImmutableConverter(const engine::Modules& modules);
  ImmutableConverter(const ImmutableConverter&) = delete;
  ImmutableConverter& operator=(const ImmutableConverter&) = delete;
  ~ImmutableConverter() override = default;

  // Accepts the internal lattice structure for debugging.
  [[nodiscard]] bool Convert(const ConversionRequest& request,
                             Segments* segments, Lattice* lattice) const;

  [[nodiscard]] bool Convert(const ConversionRequest& request,
                             Segments* segments) const override;

 private:
  friend class ImmutableConverterTestPeer;

  enum InsertCandidatesType {
    MULTI_SEGMENTS,      // Normal conversion ("私の|名前は|中野です")
    SINGLE_SEGMENT,      // Realtime conversion ("私の名前は中野です")
    ONLY_FIRST_SEGMENT,  // Insert only first segment ("私の")
    // Insert only first segment for n-best path
    // ここでは着物を脱ぐ
    // ここで履物を脱ぐ
    // → ここでは, ここで
    FIRST_INNER_SEGMENT,
  };

  void ExpandCandidates(const ConversionRequest& request,
                        absl::string_view original_key, NBestGenerator* nbest,
                        Segment* segment, size_t expand_size) const;
  void InsertDummyCandidates(Segment* segment, size_t expand_size) const;
  std::vector<Node*> Lookup(int begin_pos, const ConversionRequest& request,
                            bool is_reverse, Lattice* lattice) const;
  void AddCharacterTypeBasedNodes(absl::string_view key_substr,
                                  Lattice* lattice,
                                  BaseNodeListBuilder* builder) const;

  void Resegment(const Segments& segments, absl::string_view history_key,
                 absl::string_view conversion_key, Lattice* lattice) const;

  void ApplyResegmentRules(size_t pos, Lattice* lattice) const;
  // Returns true resegmentation happened
  bool ResegmentArabicNumberAndSuffix(size_t pos, Lattice* lattice) const;
  bool ResegmentPrefixAndArabicNumber(size_t pos, Lattice* lattice) const;
  bool ResegmentPersonalName(size_t pos, Lattice* lattice) const;

  bool MakeLattice(const ConversionRequest& request, Segments* segments,
                   Lattice* lattice) const;
  bool MakeLatticeNodesForHistorySegments(const Segments& segments,
                                          const ConversionRequest& request,
                                          Lattice* lattice) const;
  void MakeLatticeNodesForConversionSegments(const Segments& segments,
                                             const ConversionRequest& request,
                                             absl::string_view history_key,
                                             Lattice* lattice) const;
  // Fixes for "好む" vs "この|無", "大|代" vs "代々" preferences.
  // If the last node ends with "prefix", give an extra
  // wcost penalty. In this case  "無" doesn't tend to appear at
  // user input.
  void ApplyPrefixSuffixPenalty(absl::string_view conversion_key,
                                Lattice* lattice) const;

  bool Viterbi(const Segments& segments, Lattice* lattice) const;

  bool PredictionViterbi(const Segments& segments, Lattice* lattice) const;
  void PredictionViterbiInternal(int calc_begin_pos, int calc_end_pos,
                                 Lattice* lattice) const;

  // TODO(toshiyuki): Change parameter order for mutable |segments|.

  // Inserts first segment from conversion result to candidates.
  // Costs will be modified using the existing candidates.
  void InsertFirstSegmentToCandidates(const ConversionRequest& request,
                                      Segments* segments,
                                      const Lattice& lattice,
                                      absl::Span<const uint16_t> group,
                                      size_t max_candidates_size,
                                      bool allow_exact) const;

  void InsertCandidates(const ConversionRequest& request, Segments* segments,
                        const Lattice& lattice,
                        absl::Span<const uint16_t> group,
                        size_t max_candidates_size,
                        InsertCandidatesType type) const;

  void InsertCandidatesForRealtimeWithCandidateChecker(
      const ConversionRequest& request, const Lattice& lattice,
      absl::Span<const uint16_t> group, Segments* segments) const;

  // Helper function for InsertCandidates().
  // Returns true if |node| is valid node for segment end.
  bool IsSegmentEndNode(const ConversionRequest& request,
                        const Segments& segments, const Node* node,
                        absl::Span<const uint16_t> group,
                        bool is_single_segment) const;

  // Helper function for InsertCandidates().
  // Returns the segment for inserting candidates.
  Segment* GetInsertTargetSegment(const Lattice& lattice,
                                  absl::Span<const uint16_t> group,
                                  InsertCandidatesType type, size_t begin_pos,
                                  const Node* node, Segments* segments) const;

  bool MakeSegments(const ConversionRequest& request, const Lattice& lattice,
                    Segments* segments) const;

  std::vector<uint16_t> MakeGroup(const Segments& segments) const;

  inline int GetCost(const Node* lnode, const Node* rnode) const {
    const int kInvalidPenaltyCost = 100000;
    if (rnode->constrained_prev != nullptr &&
        lnode != rnode->constrained_prev) {
      return kInvalidPenaltyCost;
    }
    return connector_.GetTransitionCost(lnode->rid, rnode->lid) + rnode->wcost;
  }

  void InsertCandidatesForConversion(const ConversionRequest& request,
                                     const Lattice& lattice,
                                     absl::Span<const uint16_t> group,
                                     Segments* segments) const;

  void InsertCandidatesForPrediction(const ConversionRequest& request,
                                     const Lattice& lattice,
                                     absl::Span<const uint16_t> group,
                                     Segments* segments) const;

  const dictionary::DictionaryInterface& dictionary_;
  const dictionary::DictionaryInterface& suffix_dictionary_;
  const dictionary::UserDictionaryInterface& user_dictionary_;
  const Connector& connector_;
  const Segmenter& segmenter_;
  const dictionary::PosMatcher& pos_matcher_;
  const dictionary::PosGroup& pos_group_;
  const SuggestionFilter& suggestion_filter_;

  // Cache for POS ids.
  const uint16_t first_name_id_;
  const uint16_t last_name_id_;
  const uint16_t number_id_;
  const uint16_t unknown_id_;

  // Cache for transition cost.
  const int32_t last_to_first_name_transition_cost_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_IMMUTABLE_CONVERTER_H_
