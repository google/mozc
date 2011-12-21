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

#ifndef MOZC_CONVERTER_NBEST_GENERATOR_H_
#define MOZC_CONVERTER_NBEST_GENERATOR_H_

#include <queue>
#include <set>
#include <vector>
#include <string>
#include "base/base.h"
#include "base/freelist.h"
#include "converter/node.h"
#include "converter/segments.h"

namespace mozc {

class CandidateFilter;
class ConnectorInterface;
class Lattice;
class SegmenterInterface;
struct Node;

// TODO(toshiyuki): write unittest for NBestGenerator.
//                  we can unittest this by making Mocks
//                  for Connector and Lattice
class NBestGenerator {
 public:
  // constractor for compatibility
  NBestGenerator();
  explicit NBestGenerator(const SegmenterInterface *segmenter);
  virtual ~NBestGenerator();

  // set starting Node and ending Node --
  // Try to enumurate N-best results between begin_node and end_node.
  void  Init(const Node *begin_node, const Node *end_node,
             const Lattice *lattice, bool is_prediction);

  // reset internal priority queue. Reuse begin_node and eos_node
  void Reset();

  // Iterator:
  // Can obtain N-best results by calling Next() in sequence.
  // candidate_begin/end_node:
  //  nodes for generated candidate in Next().
  //  they could be different from begin/end_node_, because Next() generates
  //  candidates which have same boundaries with begin/end_node_ rather than
  //  have exactly same nodes with begin/end_node_.
  //  note that candidate_begin_node is inclusive
  //  and candidate_end_node is exclusive.
  bool Next(Segment::Candidate *candidate,
            Segments::RequestType request_type);

 private:
  void MakeCandidate(Segment::Candidate *candidate,
                     int cost,
                     int structure_cost,
                     int w_cost,
                     const vector<const Node *> nodes) const;

  int GetTransitionCost(const Node *lnode, const Node *rnode) const;

  struct QueueElement {
    const Node *node;
    const QueueElement *next;
    int32 fx;  // f(x) = h(x) + g(x): cost function for A* search
    int32 gx;  // g(x)
    // transition cost part of g(x).
    // Do not take the transition costs to edge nodes.
    int32 structure_gx;
    int32 w_gx;
  };

  class QueueElementComp {
   public:
    const bool operator()(const QueueElement *q1,
                          const QueueElement *q2) const {
      return (q1->fx > q2->fx);
    }
  };

  typedef priority_queue<const QueueElement *, vector<const QueueElement *>,
                         QueueElementComp> Agenda;

  scoped_ptr<Agenda> agenda_;
  FreeList<QueueElement> freelist_;
  scoped_ptr<CandidateFilter> filter_;
  const Node *begin_node_;
  const Node *end_node_;
  const ConnectorInterface *connector_;
  const SegmenterInterface *segmenter_;
  const Lattice *lattice_;
  bool viterbi_result_checked_;
  bool is_prediction_;

  DISALLOW_COPY_AND_ASSIGN(NBestGenerator);
};
}

#endif  // MOZC_CONVERTER_NBEST_GENERATOR_H_
