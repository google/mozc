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

const int kFreeListSize = 512;
const int kCostDiff     = 3453;

NBestGenerator::NBestGenerator()
    : freelist_(kFreeListSize), filter_(NULL),
      begin_node_(NULL), end_node_(NULL),
      connector_(ConnectorFactory::GetConnector()),
      lattice_(NULL),
      viterbi_result_inserted_(false) {}

NBestGenerator::~NBestGenerator() {}

void NBestGenerator::Init(const Node *begin_node, const Node *end_node,
                          const Lattice *lattice) {
  Reset();

  begin_node_ = begin_node;
  end_node_ = end_node;
  lattice_ = lattice;

  if (lattice_ == NULL || !lattice_->has_lattice()) {
    LOG(ERROR) << "lattice is not available";
    return;
  }

  for (Node *node = lattice_->begin_nodes(end_node_->begin_pos);
       node != NULL; node = node->bnext) {
    if (node == end_node_ ||
        (node->lid != end_node_->lid &&
         node->cost - end_node_->cost <= kCostDiff &&
         node->prev != end_node_->prev)) {
      QueueElement *eos = freelist_.Alloc();
      DCHECK(eos);
      eos->node = node;
      eos->next = NULL;
      eos->fx = node->cost;
      eos->gx = 0;
      eos->structure_gx = 0;
      eos->w_gx = 0;
      agenda_->push(eos);
    }
  }
}

void NBestGenerator::Reset() {
  agenda_.reset(new Agenda);
  filter_.reset(new CandidateFilter);
  freelist_.Free();
  viterbi_result_inserted_ = false;
}

void NBestGenerator::MakeCandidate(Segment::Candidate *candidate,
                                   int32 cost, int32 structure_cost,
                                   int32 wcost,
                                   const vector<const Node *> nodes) const {
  CHECK(!nodes.empty());

  bool has_constrained_node = false;
  bool is_functional = false;

  candidate->key.clear();
  candidate->value.clear();
  candidate->content_value.clear();
  candidate->content_key.clear();
  candidate->is_spelling_correction = false;
  candidate->can_expand_alternative = true;
  candidate->lid = nodes.front()->lid;
  candidate->rid = nodes.back()->rid;
  candidate->cost = cost;
  candidate->structure_cost = structure_cost;
  candidate->wcost = wcost;

  for (size_t i = 0; i < nodes.size(); ++i) {
    const Node *node = nodes[i];
    DCHECK(node != NULL);
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

    candidate->key += node->key;
    candidate->value += node->value;
    candidate->is_spelling_correction |= node->is_spelling_correction;
    if (node->normalization_type == Node::NO_NORMALIZATION) {
      candidate->can_expand_alternative = false;
    }
  }

  if (candidate->content_value.empty() ||
      candidate->content_key.empty()) {
    candidate->content_value = candidate->value;
    candidate->content_key = candidate->key;
  }

  // If result has constrained_node, set CONTEXT_SENSITIVE.
  // If a node has constrained node, the node is generated by
  //  a) compound node and resegmented via personal name resegmentation
  //  b) compound-based reranking.
  if (has_constrained_node) {
    candidate->learning_type |= Segment::Candidate::CONTEXT_SENSITIVE;
  }
}

bool NBestGenerator::Next(Segment::Candidate *candidate) {
  if (lattice_ == NULL || !lattice_->has_lattice()) {
    LOG(ERROR) << "Must create lattice in advance";
    return false;
  }

  // |cost| and |structure_cost| are calculated as follows:
  //
  // Example:
  // |left_node| => |node1| => |node2| => |node3| => |right_node|.
  // |node1| .. |node2| consists of a candidate.
  //
  // cost = (left_node->cost - begin_node_->cost) +
  //        trans(left_node, node1) + node1->wcost +
  //        trans(node1,     node2) + node2->wcost +
  //        trans(node2,     node3) + node3->wcost +
  //        trans(node3, rigt_node) +
  //        (right_node->cost - end_node_->cost)
  // structure_cost = trans(node1, node2) + trans(node2, node3);
  // wcost = node1->wcost +
  //        trans(node1,     node2) + node2->wcost +
  //        trans(node2,     node3) + node3->wcost
  //
  // Here (left_node->cost - begin_node_->cost) and
  //      (right_node->cost - end_node->cost) act as an approximation
  // of marginalized costs of the candidate |node1| .. |node3|.
  // "marginalized cost" means that how likely the left_node or right_node
  // are selected by taking the all paths encoded in the lattice.
  // These approximated costs are exactly 0 when taking Viterbi-best
  // path.

  // Insert Viterbi best result here to make sure that
  // the top result is Viterbi best result.
  if (!viterbi_result_inserted_) {
    vector<const Node *> nodes;
    int total_wcost = 0;
    for (const Node *node = begin_node_->next;
         node != end_node_; node = node->next) {
      nodes.push_back(node);
      if (node != begin_node_->next) {
        total_wcost += node->wcost;
      }
    }
    DCHECK(!nodes.empty());

    const int cost = end_node_->cost -
        begin_node_->cost - end_node_->wcost;
    const int structure_cost = end_node_->prev->cost -
        begin_node_->next->cost - total_wcost;
    const int wcost = end_node_->prev->cost -
        begin_node_->next->cost + begin_node_->next->wcost;

    MakeCandidate(candidate, cost, structure_cost, wcost, nodes);

    // User CandiadteFilter so that filter is initialized with the
    // Viterbi-best path.
    filter_->FilterCandidate(candidate, nodes);
    viterbi_result_inserted_ = true;

    return true;
  }

  const int KMaxTrial = 500;
  int num_trials = 0;

  while (!agenda_->empty()) {
    const QueueElement *top = agenda_->top();
    DCHECK(top);
    agenda_->pop();
    const Node *rnode = top->node;
    CHECK(rnode);

    if (num_trials++ > KMaxTrial) {   // too many trials
      VLOG(2) <<  "too many trials: " << num_trials;
      return false;
    }

    // reached to the goal.
    if (rnode->end_pos == begin_node_->end_pos) {
      vector<const Node *> nodes;
      for (const QueueElement *elm = top->next;
           elm->next != NULL; elm = elm->next) {
        nodes.push_back(elm->node);
      }
      CHECK(!nodes.empty());

      MakeCandidate(candidate, top->gx, top->structure_gx, top->w_gx, nodes);

      switch (filter_->FilterCandidate(candidate, nodes)) {
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
      const QueueElement *best_left_elm = NULL;
      const bool is_right_edge = rnode->begin_pos == end_node_->begin_pos;
      const bool is_left_edge = rnode->begin_pos == begin_node_->end_pos;

      for (Node *lnode = lattice_->end_nodes(rnode->begin_pos);
           lnode != NULL; lnode = lnode->enext) {
        // is_edge is true if current lnode/rnode has same boundary as
        // begin/end node regardless of its value.
        DCHECK(!(is_right_edge && is_left_edge));

        const bool is_edge = (is_right_edge || is_left_edge);

        // is_boundary is true if there is a grammer-based boundary
        // between lnode and rnode
        const bool is_boundary = (lnode->node_type == Node::HIS_NODE ||
                                  Segmenter::IsBoundary(lnode, rnode));

        // is_valid_boudnary is true if the word connection from
        // lnode to rnode has a gramatically correct relation.
        const bool is_valid_boundary =
            (lnode->node_type == Node::CON_NODE ||
             rnode->node_type == Node::CON_NODE ||
             rnode->is_weak_connected ||
             (is_edge && is_boundary) ||   // on the edge, have a boudnary.
             (!is_boundary && !is_edge));  // not on the edge, not the case.

        // is_valid_cost is true if the left node is valid
        // in terms of cost. if left_node is left edge, there
        // is a cost-based constraint.
        const bool is_valid_cost =
            (!is_left_edge ||
             (is_left_edge && (begin_node_->cost - lnode->cost) <= kCostDiff));

        // is_invalid_position is true if the lnode's location is invalid
        //  1.   |<-- begin_node_-->|
        //           |<--lnode-->|    <== exceeds begin_node.
        //  2.   |<-- begin_node_-->|
        //                      |<--lnode-->|  <== overlapped.
        const bool is_valid_position =
             !(lnode->end_pos < begin_node_->end_pos ||     // case(1)
               (lnode->begin_pos < begin_node_->end_pos &&  // case(2)
                begin_node_->end_pos < lnode->end_pos));

        // can_expand_more is true if we can expand candidates from
        // |rnode| to |lnode|.
        const bool can_expand_more  =
            (is_valid_boundary && is_valid_cost && is_valid_position &&
             lnode->node_type != Node::UNU_NODE &&
             rnode->node_type != Node::UNU_NODE);

        if (can_expand_more) {
          const int transition_cost = GetTransitionCost(lnode, rnode);

          // How likely the costs get increased after expanding rnode.
          int cost_diff = 0;
          int structure_cost_diff = 0;
          int wcost_diff = 0;

          if (is_right_edge) {
            // use |rnode->cost - end_node_->cost| is an approximation
            // of marginalized word cost.
            cost_diff = transition_cost + (rnode->cost - end_node_->cost);
            structure_cost_diff = 0;
            wcost_diff = 0;
          } else if (is_left_edge) {
            // use |lnode->cost - begin_node_->cost| is an approximation
            // of marginalized word cost.
            cost_diff = (lnode->cost - begin_node_->cost) +
                transition_cost + rnode->wcost;
            structure_cost_diff = 0;
            wcost_diff = rnode->wcost;
          } else {
            // use rnode->wcost.
            cost_diff = transition_cost + rnode->wcost;
            structure_cost_diff = transition_cost;
            wcost_diff = transition_cost + rnode->wcost;
          }

          if (rnode->is_weak_connected) {
            const int kWeakConnectedPenalty = 3453;   // log prob of 1/1000
            cost_diff += kWeakConnectedPenalty;
            structure_cost_diff += kWeakConnectedPenalty / 2;
            wcost_diff += kWeakConnectedPenalty / 2;
          }

          QueueElement *elm = freelist_.Alloc();
          DCHECK(elm);

          elm->node = lnode;
          elm->gx = cost_diff + top->gx;
          elm->structure_gx = structure_cost_diff + top->structure_gx;
          elm->w_gx = wcost_diff + top->w_gx;

          // |lnode->cost| is heuristics function of A* search, h(x).
          // After Viterbi search, we already know an exact value of h(x).
          elm->fx = lnode->cost + elm->gx;
          elm->next = top;

          if (is_left_edge) {
            // We only need to only 1 left node here.
            // Even if expand all left nodes, all the |value| part should
            // be identical. Here, we simply user the best left edge node.
            // This hack reduces the number of redundant calls of pop().
            if (best_left_elm == NULL || best_left_elm->fx > elm->fx) {
              best_left_elm = elm;
            }
          } else {
            agenda_->push(elm);
          }
        }
      }

      if (best_left_elm != NULL) {
        agenda_->push(best_left_elm);
      }
    }
  }

  return false;
}

int NBestGenerator::GetTransitionCost(const Node *lnode,
                                      const Node *rnode) const {
  const int kInvalidPenaltyCost = 100000;
  if (rnode->constrained_prev != NULL && lnode != rnode->constrained_prev) {
    return kInvalidPenaltyCost;
  }
  return connector_->GetTransitionCost(lnode->rid, rnode->lid);
}
}  // namespace mozc
