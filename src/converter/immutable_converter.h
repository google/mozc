// Copyright 2010-2013, Google Inc.
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

#include <string>
#include <vector>
#include "base/base.h"
#include "converter/connector_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/lattice.h"
#include "converter/nbest_generator.h"
#include "converter/segments.h"
//  for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {

class DictionaryInterface;
class ImmutableConverterInterface;
class POSMatcher;
class PosGroup;
class SegmenterInterface;
class SuggestionFilter;

class ImmutableConverterImpl : public ImmutableConverterInterface {
 public:
  ImmutableConverterImpl(const DictionaryInterface *dictionary,
                         const DictionaryInterface *suffix_dictionary,
                         const SuppressionDictionary *suppression_dictionary,
                         const ConnectorInterface *connector,
                         const SegmenterInterface *segmenter,
                         const POSMatcher *pos_matcher,
                         const PosGroup *pos_group,
                         const SuggestionFilter *suggestion_filter);
  virtual ~ImmutableConverterImpl() {}

  virtual bool ConvertForRequest(
      const ConversionRequest &request, Segments *segments) const;

 private:
  FRIEND_TEST(ImmutableConverterTest, DummyCandidatesCost);
  FRIEND_TEST(ImmutableConverterTest, DummyCandidatesInnerSegmentBoundary);
  FRIEND_TEST(ImmutableConverterTest, PredictiveNodesOnlyForConversionKey);
  FRIEND_TEST(ImmutableConverterTest, AddPredictiveNodes);
  FRIEND_TEST(ImmutableConverterTest, NotConnectedTest);
  friend class NBestGeneratorTest;
  FRIEND_TEST(NBestGeneratorTest, MultiSegmentConnectionTest);
  FRIEND_TEST(NBestGeneratorTest, SingleSegmentConnectionTest);

  enum InsertCandidatesType {
    MULTI_SEGMENTS,  // Normal conversion ("私の|名前は|中野です")
    SINGLE_SEGMENT,  // Realtime conversion ("私の名前は中野です")
    ONLY_FIRST_SEGMENT,  // Insert only first segment ("私の")
  };

  void ExpandCandidates(const string &original_key,
                        NBestGenerator *nbest, Segment *segment,
                        Segments::RequestType request_type,
                        size_t expand_size) const;
  void InsertDummyCandidates(Segment *segment, size_t expand_size) const;
  Node *Lookup(const int begin_pos, const int end_pos,
               const ConversionRequest &request,
               bool is_reverse,
               bool is_prediction,
               Lattice *lattice) const;
  Node *AddCharacterTypeBasedNodes(const char *begin, const char *end,
                                   Lattice *lattice, Node *nodes) const;

  void Resegment(const Segments &segments,
                 const string &history_key, const string &conversion_key,
                 Lattice *lattice) const;

  void ApplyResegmentRules(size_t pos, Lattice *lattice) const;
  // Returns true resegmentation happened
  bool ResegmentArabicNumberAndSuffix(size_t pos, Lattice *lattice) const;
  bool ResegmentPrefixAndArabicNumber(size_t pos, Lattice *lattice) const;
  bool ResegmentPersonalName(size_t pos, Lattice *lattice) const;

  bool MakeLattice(const ConversionRequest &request,
                   Segments *segments, Lattice *lattice) const;
  bool MakeLatticeNodesForHistorySegments(
      const Segments &segments, const ConversionRequest &request,
      Lattice *lattice) const;
  void MakeLatticeNodesForConversionSegments(
      const Segments &segments, const ConversionRequest &request,
      const string &history_key, Lattice *lattice) const;
  void MakeLatticeNodesForPredictiveNodes(
      const Segments &segments, const ConversionRequest &request,
      Lattice *lattice) const;
  void AddPredictiveNodes(const size_t &len,
                          const size_t &pos,
                          Lattice *lattice,
                          Node *result_node) const;
  // Fixes for "好む" vs "この|無", "大|代" vs "代々" preferences.
  // If the last node ends with "prefix", give an extra
  // wcost penalty. In this case  "無" doesn't tend to appear at
  // user input.
  void ApplyPrefixSuffixPenalty(const string &conversion_key,
                                Lattice *lattice) const;

  bool Viterbi(const Segments &segments, Lattice *lattice) const;

  bool PredictionViterbi(const Segments &segments, Lattice *lattice) const;
  void PredictionViterbiInternal(
      int calc_begin_pos, int calc_end_pos, Lattice *lattice) const;

  // TODO(toshiyuki): Change parameter order for mutable |segments|.

  // Inserts first segment from conversion result to candidates.
  // Costs will be modified using the existing candidates.
  void InsertFirstSegmentToCandidates(Segments *segments,
                                      const Lattice &lattice,
                                      const vector<uint16> &group,
                                      size_t max_candidates_size) const;

  void InsertCandidates(Segments *segments,
                        const Lattice &lattice,
                        const vector<uint16> &group,
                        size_t max_candidates_size,
                        InsertCandidatesType type) const;

  // Helper function for InsertCandidates().
  // Returns true if |node| is valid node for segment end.
  bool IsSegmentEndNode(const Segments &segments,
                        const Node *node,
                        const vector<uint16> &group,
                        bool is_single_segment) const;

  // Helper function for InsertCandidates().
  // Returns the segment for inserting candidates.
  Segment *GetInsertTargetSegment(const Lattice &lattice,
                                  const vector<uint16> &group,
                                  InsertCandidatesType type,
                                  size_t begin_pos,
                                  const Node *node,
                                  Segments *segments) const;

  bool MakeSegments(const ConversionRequest &request,
                    const Lattice &lattice,
                    const vector<uint16> &group,
                    Segments *segments) const;

  void MakeGroup(const Segments &segments, vector<uint16> *group) const;

  inline int GetCost(const Node *lnode, const Node *rnode) const {
    const int kInvalidPenaltyCost = 100000;
    if (rnode->constrained_prev != NULL && lnode != rnode->constrained_prev) {
      return kInvalidPenaltyCost;
    }
    return connector_->GetTransitionCost(lnode->rid, rnode->lid) + rnode->wcost;
  }

  const DictionaryInterface *dictionary_;
  const DictionaryInterface *suffix_dictionary_;
  const SuppressionDictionary *suppression_dictionary_;
  const ConnectorInterface *connector_;
  const SegmenterInterface *segmenter_;
  const POSMatcher *pos_matcher_;
  const PosGroup *pos_group_;
  const SuggestionFilter *suggestion_filter_;

  // Cache for POS ids.
  const uint16 first_name_id_;
  const uint16 last_name_id_;
  const uint16 number_id_;
  const uint16 unknown_id_;

  // Cache for transition cost.
  const int32 last_to_first_name_transition_cost_;

  DISALLOW_COPY_AND_ASSIGN(ImmutableConverterImpl);
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_IMMUTABLE_CONVERTER_H_
