// Copyright 2010, Google Inc.
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

#include "converter/nbest_generator.h"

#include <string>
#include "base/base.h"
#include "converter/candidate_filter.h"
#include "converter/connector_interface.h"
#include "converter/lattice.h"
#include "converter/pos_matcher.h"
#include "converter/segmenter.h"
#include "converter/segments.h"

namespace mozc {

NBestGenerator::NBestGenerator()
    : freelist_(128), filter_(NULL),
      begin_node_(NULL), end_node_(NULL),
      connector_(ConnectorFactory::GetConnector()),
      lattice_(NULL) {}

NBestGenerator::~NBestGenerator() {}

void NBestGenerator::Init(Node *begin_node, Node *end_node,
                          Lattice *lattice) {
  begin_node_ = begin_node;
  end_node_ = end_node;
  lattice_ = lattice;
  Reset();
}

void NBestGenerator::Reset() {
  agenda_.reset(new Agenda);
  filter_.reset(new CandidateFilter);
  freelist_.Free();
  QueueElement *eos = freelist_.Alloc();
  DCHECK(eos);
  eos->node = end_node_;
  eos->next = NULL;
  eos->fx = eos->gx = 0;
  eos->structure_gx = 0;
  agenda_->push(eos);
}

bool NBestGenerator::Next(Segment::Candidate *candidate,
                          const Node **candidate_begin_node,
                          const Node **candidate_end_node) {
  if (lattice_ == NULL || !lattice_->has_lattice()) {
    LOG(ERROR) << "Must create lattice in advance";
    return false;
  }

  const int KMaxTrial = 500;
  int num_trials = 0;
  string key;

  while (!agenda_->empty()) {
    QueueElement *top = agenda_->top();
    DCHECK(top);
    agenda_->pop();
    Node *rnode = top->node;
    DCHECK(rnode);

    if (num_trials++ > KMaxTrial) {   // too many trials
      VLOG(1) << "too many trials";
      return false;
    }

    if (rnode->end_pos == begin_node_->end_pos) {
      for (QueueElement *n = top; n->next != NULL; n = n->next) {
        n->node->next = n->next->node;   // change next & prev
        n->next->node->prev = n->node;
      }

      bool has_constrained_node = false;
      bool is_functional = false;

      *candidate_begin_node = static_cast<const Node *>(rnode->next);

      candidate->value.clear();
      candidate->content_value.clear();
      candidate->content_key.clear();
      candidate->nodes.clear();
      key.clear();
      candidate->is_spelling_correction = false;

      for (const Node *node = *candidate_begin_node; true; node = node->next) {
        DCHECK(node != NULL);
        if (node->begin_pos == end_node_->begin_pos) {
          *candidate_end_node = node;
          break;
        }

        if (node->constrained_prev != NULL ||
            (node->next != NULL &&
             node->next->constrained_prev == node)) {
          has_constrained_node = true;
        }

        if (!is_functional && !POSMatcher::IsFunctional(node->lid)) {
          candidate->content_value += node->value;
          candidate->content_key += node->key;
        } else {
          is_functional = true;
        }

        key += node->key;
        candidate->value += node->value;
        candidate->is_spelling_correction |= node->is_spelling_correction;

        candidate->nodes.push_back(node);
      }

      DCHECK(!candidate->nodes.empty());

      if (candidate->content_value.empty() ||
          candidate->content_key.empty()) {
        candidate->content_value = candidate->value;
        candidate->content_key = key;
      }

      // use remaining route cost as candidate cost
      candidate->cost = top->fx - begin_node_->cost;
      candidate->structure_cost = top->structure_gx;

      candidate->lid = candidate->nodes.front()->lid;
      candidate->rid = candidate->nodes.back()->rid;

      // Skip candidate filter if result has constrained_node.
      // If a node has constrained node, the node is generated by
      //  a) compound node and resegmented via personal name resegmentation
      //  b) compound-based reranking.
      // In general, the cost of constrained node tends to be overestimated.
      // If the top candidate has constrained node, we skip CandidateFilter,
      // meaning that the node is not treated as the top
      // node for CandidateFilter.
      if (has_constrained_node) {
        candidate->learning_type |= Segment::Candidate::CONTEXT_SENSITIVE;
        return true;
      }

      switch (filter_->FilterCandidate(candidate)) {
        case CandidateFilter::GOOD_CANDIDATE:
          return true;
        case CandidateFilter::STOP_ENUMERATION:
          return false;
        case CandidateFilter::BAD_CANDIDATE:
        default:
          break;
          // do nothing
      }
    } else {
      for (Node *lnode = lattice_->end_nodes(rnode->begin_pos);
           lnode != NULL; lnode = lnode->enext) {
        // is_edge is true if current lnode/rnode has same boundary as
        // begin/end node regardless of its value.
        const bool is_edge = (lnode->end_pos == begin_node_->end_pos ||
                              rnode->begin_pos == end_node_->begin_pos);
        const bool is_boundary = (lnode->node_type == Node::HIS_NODE ||
                                  Segmenter::IsBoundary(lnode, rnode));
        const bool is_good_boundary =
            (lnode->node_type == Node::CON_NODE ||
             rnode->node_type == Node::CON_NODE ||
             rnode->is_weak_connected ||
             (is_edge && is_boundary) ||
             (!is_boundary && !is_edge));
        const int bos_trans_cost =
            connector_->GetTransitionCost(0, rnode->lid);  // 0: rid of BOS
        const int cur_trans_cost = GetTransitionCost(lnode, rnode);
        const bool is_connect_bos_cand =
            (lnode->end_pos == begin_node_->end_pos &&
             bos_trans_cost <= cur_trans_cost);
        if (lnode->node_type != Node::UNU_NODE &&
            rnode->node_type != Node::UNU_NODE &&
            (lnode == begin_node_ ||
             lnode->end_pos > begin_node_->end_pos ||
             is_connect_bos_cand) && is_good_boundary) {
          // transition_cost = GetTransitionCost(lnode, rnode);
          int transition_cost = cur_trans_cost;
          int cost = transition_cost + rnode->wcost;
          if (rnode->is_weak_connected) {
            cost *= 2;
            transition_cost *= 2;
          }
          QueueElement *n = freelist_.Alloc();
          DCHECK(n);
          n->node = lnode;
          n->gx = cost + top->gx;
          // structure_gx does not include word cost.
          n->structure_gx = top->structure_gx + transition_cost;
          // uses cost of lnode as cost estimation.
          n->fx = lnode->cost + cost + top->gx;
          n->next = top;
          agenda_->push(n);
        }
      }
    }
  }

  return false;
}

int NBestGenerator::GetTransitionCost(Node *lnode, Node *rnode) const {
  // Ignores connection cost to adjacent context.
  if (lnode == begin_node_ || rnode == end_node_) {
    return 0;
  }
  const int kInvalidPenaltyCost = 100000;
  if (rnode->constrained_prev != NULL && lnode != rnode->constrained_prev) {
    return kInvalidPenaltyCost;
  }
  return connector_->GetTransitionCost(lnode->rid, rnode->lid);
}
}  // namespace mozc
