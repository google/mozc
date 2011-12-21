// Copyright 2010-2011, Google Inc.
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

#include "converter/immutable_converter_interface.h"

#include <string>
#include <vector>

#include "base/base.h"
#include "converter/connector_interface.h"
#include "converter/lattice.h"
#include "converter/nbest_generator.h"
#include "converter/segments.h"
//  for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {

class DictionaryInterface;
class ImmutableConverterInterface;
class SegmenterInterface;

class ImmutableConverterImpl: public ImmutableConverterInterface {
 public:
  ImmutableConverterImpl();
  explicit ImmutableConverterImpl(const SegmenterInterface *segmenter);
  virtual ~ImmutableConverterImpl() {}

  virtual bool Convert(Segments *segments) const;

 private:
  FRIEND_TEST(ImmutableConverterTest, DummyCandidatesCost);
  FRIEND_TEST(ImmutableConverterTest, PredictiveNodesOnlyForConversionKey);
  FRIEND_TEST(ImmutableConverterTest, AddPredictiveNodes);
  FRIEND_TEST(ImmutableConverterTest, PromoteUserDictionaryCandidate);

  void ExpandCandidates(NBestGenerator *nbest, Segment *segment,
                        Segments::RequestType request_type,
                        size_t expand_size) const;
  void PromoteUserDictionaryCandidate(Segment *segment) const;
  void InsertDummyCandidates(Segment *segment, size_t expand_size) const;
  Node *Lookup(const int begin_pos, const int end_pos,
               bool is_reverse,
               bool is_prediction,
               Lattice *lattice) const;
  Node *AddCharacterTypeBasedNodes(const char *begin, const char *end,
                                   Lattice *lattice, Node *nodes) const;

  void Resegment(const string &history_key,
                 const string &conversion_key,
                 Segments *segments, Lattice *lattice) const;

  void ApplyResegmentRules(size_t pos, Lattice *lattice) const;
  // return true resegmentation happened
  bool ResegmentArabicNumberAndSuffix(size_t pos, Lattice *lattice) const;
  bool ResegmentPrefixAndArabicNumber(size_t pos, Lattice *lattice) const;
  bool ResegmentPersonalName(size_t pos, Lattice *lattice) const;

  bool MakeLattice(Lattice *lattice, Segments *segments) const;
  bool MakeLatticeNodesForHistorySegments(Lattice *lattice,
                                          Segments *segments) const;
  void MakeLatticeNodesForConversionSegments(
      const string &history_key,
      Segments *segments, Lattice *lattice) const;
  void MakeLatticeNodesForPredictiveNodes(Lattice *lattice,
                                          Segments *segments) const;
  void AddPredictiveNodes(const size_t &len,
                          const size_t &pos,
                          Lattice *lattice,
                          Node *result_node) const;
  // Fix for "好む" vs "この|無", "大|代" vs "代々" preferences.
  // If the last node ends with "prefix", give an extra
  // wcost penalty. In this case  "無" doesn't tend to appear at
  // user input.
  void ApplyPrefixSuffixPenalty(const string &conversion_key,
                                Lattice *lattice) const;

  bool Viterbi(Segments *segments,
               const Lattice &lattice,
               const vector<uint16> &group) const;

  int GetConnectionType(const Node *lnode,
                        const Node *rnode,
                        const vector<uint16> &group,
                        const Segments *segments) const;


  bool PredictionViterbi(Segments *segments,
                         const Lattice &lattice) const;

  void PredictionViterbiSub(Segments *segments,
                            const Lattice &lattice,
                            int calc_begin_pos,
                            int calc_end_pos) const;

  bool MakeSegments(Segments *segments,
                    const Lattice &lattice,
                    const vector<uint16> &group) const;

  inline int GetCost(const Node *lnode, const Node *rnode) const {
    const int kInvalidPenaltyCost = 100000;
    if (rnode->constrained_prev != NULL && lnode != rnode->constrained_prev) {
      return kInvalidPenaltyCost;
    }
    return connector_->GetTransitionCost(lnode->rid, rnode->lid) + rnode->wcost;
  }

  ConnectorInterface *connector_;
  DictionaryInterface *dictionary_;
  const SegmenterInterface *segmenter_;

  int32 last_to_first_name_transition_cost_;
  DISALLOW_COPY_AND_ASSIGN(ImmutableConverterImpl);
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_IMMUTABLE_CONVERTER_H_
