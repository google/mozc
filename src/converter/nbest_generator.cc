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

#include "converter/nbest_generator.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "converter/candidate_filter.h"
#include "converter/connector.h"
#include "converter/lattice.h"
#include "converter/node.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"

using mozc::dictionary::POSMatcher;
using mozc::dictionary::SuppressionDictionary;

namespace mozc {
namespace {

const int kFreeListSize = 512;
const int kCostDiff = 3453;  // log prob of 1/1000

}  // namespace

using converter::CandidateFilter;

struct NBestGenerator::QueueElement {
  const Node *node;
  const QueueElement *next;
  int32 fx;  // f(x) = h(x) + g(x): cost function for A* search
  int32 gx;  // g(x)
  // transition cost part of g(x).
  // Do not take the transition costs to edge nodes.
  int32 structure_gx;
  int32 w_gx;
};

const NBestGenerator::QueueElement *NBestGenerator::CreateNewElement(
    const Node *node, const QueueElement *next, int32 fx, int32 gx,
    int32 structure_gx, int32 w_gx) {
  QueueElement *elm = freelist_.Alloc();
  DCHECK(elm);
  elm->node = node;
  elm->next = next;
  elm->fx = fx;
  elm->gx = gx;
  elm->structure_gx = structure_gx;
  elm->w_gx = w_gx;
  return elm;
}

struct NBestGenerator::QueueElementComparator {
  bool operator()(const NBestGenerator::QueueElement *q1,
                  const NBestGenerator::QueueElement *q2) const {
    return (q1->fx > q2->fx);
  }
};

inline void NBestGenerator::Agenda::Push(
    const NBestGenerator::QueueElement *element) {
  priority_queue_.push_back(element);
  std::push_heap(priority_queue_.begin(), priority_queue_.end(),
                 QueueElementComparator());
}

inline void NBestGenerator::Agenda::Pop() {
  DCHECK(!priority_queue_.empty());
  std::pop_heap(priority_queue_.begin(), priority_queue_.end(),
                QueueElementComparator());
  priority_queue_.pop_back();
}

NBestGenerator::NBestGenerator(const SuppressionDictionary *suppression_dic,
                               const Segmenter *segmenter,
                               const Connector *connector,
                               const POSMatcher *pos_matcher,
                               const Lattice *lattice,
                               const SuggestionFilter *suggestion_filter,
                               bool apply_suggestion_filter_for_exact_match)
    : suppression_dictionary_(suppression_dic),
      segmenter_(segmenter),
      connector_(connector),
      pos_matcher_(pos_matcher),
      lattice_(lattice),
      begin_node_(nullptr),
      end_node_(nullptr),
      freelist_(kFreeListSize),
      filter_(new CandidateFilter(suppression_dic, pos_matcher,
                                  suggestion_filter,
                                  apply_suggestion_filter_for_exact_match)),
      viterbi_result_checked_(false),
      check_mode_(STRICT),
      boundary_checker_(nullptr) {
  DCHECK(suppression_dictionary_);
  DCHECK(segmenter);
  DCHECK(connector);
  if (lattice_ == nullptr || !lattice_->has_lattice()) {
    LOG(ERROR) << "lattice is not available";
    return;
  }

  agenda_.Reserve(kFreeListSize);
}

NBestGenerator::~NBestGenerator() {}

void NBestGenerator::Reset(const Node *begin_node, const Node *end_node,
                           const BoundaryCheckMode mode) {
  agenda_.Clear();
  freelist_.Free();
  filter_->Reset();
  viterbi_result_checked_ = false;
  check_mode_ = mode;

  begin_node_ = begin_node;
  end_node_ = end_node;

  for (Node *node = lattice_->begin_nodes(end_node_->begin_pos);
       node != nullptr; node = node->bnext) {
    if (node == end_node_ || (node->lid != end_node_->lid &&
                              node->cost - end_node_->cost <= kCostDiff &&
                              node->prev != end_node_->prev)) {
      // Push "EOS" nodes.
      agenda_.Push(CreateNewElement(node, nullptr, node->cost, 0, 0, 0));
    }
  }

  switch (check_mode_) {
    case STRICT:
      boundary_checker_ = &NBestGenerator::CheckStrict;
      break;
    case ONLY_MID:
      boundary_checker_ = &NBestGenerator::CheckOnlyMid;
      break;
    case ONLY_EDGE:
      boundary_checker_ = &NBestGenerator::CheckOnlyEdge;
      break;
    default:
      LOG(ERROR) << "Invalid check mode";
      break;
  }
}

void NBestGenerator::MakeCandidate(
    Segment::Candidate *candidate, int32 cost, int32 structure_cost,
    int32 wcost, const std::vector<const Node *> &nodes) const {
  CHECK(!nodes.empty());

  candidate->Init();
  candidate->lid = nodes.front()->lid;
  candidate->rid = nodes.back()->rid;
  candidate->cost = cost;
  candidate->structure_cost = structure_cost;
  candidate->wcost = wcost;

  bool is_functional = false;
  for (size_t i = 0; i < nodes.size(); ++i) {
    const Node *node = nodes[i];
    DCHECK(node != nullptr);
    if (!is_functional && !pos_matcher_->IsFunctional(node->lid)) {
      candidate->content_value += node->value;
      candidate->content_key += node->key;
    } else {
      is_functional = true;
    }
    candidate->key += node->key;
    candidate->value += node->value;

    if (node->constrained_prev != nullptr ||
        (node->next != nullptr && node->next->constrained_prev == node)) {
      // If result has constrained_node, set CONTEXT_SENSITIVE.
      // If a node has constrained node, the node is generated by
      //  a) compound node and resegmented via personal name resegmentation
      //  b) compound-based reranking.
      candidate->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    }
    if (node->attributes & Node::SPELLING_CORRECTION) {
      candidate->attributes |= Segment::Candidate::SPELLING_CORRECTION;
    }
    if (node->attributes & Node::NO_VARIANTS_EXPANSION) {
      candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    }
    if (node->attributes & Node::USER_DICTIONARY) {
      candidate->attributes |= Segment::Candidate::USER_DICTIONARY;
    }
    if (node->attributes & Node::SUFFIX_DICTIONARY) {
      candidate->attributes |= Segment::Candidate::SUFFIX_DICTIONARY;
    }
  }

  if (candidate->content_value.empty() || candidate->content_key.empty()) {
    candidate->content_value = candidate->value;
    candidate->content_key = candidate->key;
  }

  candidate->inner_segment_boundary.clear();
  if (check_mode_ == ONLY_EDGE) {
    // For realtime conversion.  Set inner segment boundary for user history
    // prediction from realtime conversion result.
    size_t key_len = nodes[0]->key.size(), value_len = nodes[0]->value.size();
    size_t content_key_len = key_len, content_value_len = value_len;
    bool is_content_boundary = false;
    if (pos_matcher_->IsFunctional(nodes[0]->rid)) {
      is_content_boundary = true;
      content_key_len = 0;
      content_value_len = 0;
    }
    for (size_t i = 1; i < nodes.size(); ++i) {
      const Node *lnode = nodes[i - 1];
      const Node *rnode = nodes[i];
      const bool kMultipleSegments = false;
      if (segmenter_->IsBoundary(*lnode, *rnode, kMultipleSegments)) {
        candidate->PushBackInnerSegmentBoundary(
            key_len, value_len, content_key_len, content_value_len);
        key_len = 0;
        value_len = 0;
        content_key_len = 0;
        content_value_len = 0;
        is_content_boundary = false;
      }
      key_len += rnode->key.size();
      value_len += rnode->value.size();
      if (is_content_boundary) {
        continue;
      }
      // Set boundary only after content nouns or pronouns.  For example,
      // "走った" is formed as
      //     "走っ" (content word) + "た" (functional).
      // Since the content word is incomplete, we don't want to learn "走っ".
      if ((pos_matcher_->IsContentNoun(lnode->rid) ||
           pos_matcher_->IsPronoun(lnode->rid)) &&
          pos_matcher_->IsFunctional(rnode->lid)) {
        is_content_boundary = true;
      } else {
        content_key_len += rnode->key.size();
        content_value_len += rnode->value.size();
      }
    }
    candidate->PushBackInnerSegmentBoundary(key_len, value_len, content_key_len,
                                            content_value_len);
  }
}

bool NBestGenerator::Next(const std::string &original_key,
                          Segment::Candidate *candidate,
                          Segments::RequestType request_type) {
  DCHECK(begin_node_);
  DCHECK(end_node_);

  DCHECK(candidate);
  if (lattice_ == nullptr || !lattice_->has_lattice()) {
    LOG(ERROR) << "Must create lattice in advance";
    return false;
  }

  // |cost| and |structure_cost| are calculated as follows:
  //
  // Example:
  // |left_node| => |node1| => |node2| => |node3| => |right_node|.
  // |node1| .. |node3| consists of a candidate.
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
  if (!viterbi_result_checked_) {
    // Use CandidateFilter so that filter is initialized with the
    // Viterbi-best path.
    switch (InsertTopResult(original_key, candidate, request_type)) {
      case CandidateFilter::GOOD_CANDIDATE:
        return true;
      case CandidateFilter::STOP_ENUMERATION:
        return false;
        // Viterbi best result was tried to be inserted but reverted.
      case CandidateFilter::BAD_CANDIDATE:
      default:
        // do nothing
        break;
    }
  }

  const int KMaxTrial = 500;
  int num_trials = 0;

  while (!agenda_.IsEmpty()) {
    const QueueElement *top = agenda_.Top();
    DCHECK(top);
    agenda_.Pop();
    const Node *rnode = top->node;
    CHECK(rnode);

    if (num_trials++ > KMaxTrial) {  // too many trials
      VLOG(2) << "too many trials: " << num_trials;
      return false;
    }

    // reached to the goal.
    if (rnode->end_pos == begin_node_->end_pos) {
      nodes_.clear();
      for (const QueueElement *elm = top->next; elm->next != nullptr;
           elm = elm->next) {
        nodes_.push_back(elm->node);
      }
      CHECK(!nodes_.empty());

      MakeCandidate(candidate, top->gx, top->structure_gx, top->w_gx, nodes_);
      const int filter_result = filter_->FilterCandidate(
          original_key, candidate, nodes_, request_type);
      nodes_.clear();

      switch (filter_result) {
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
      const QueueElement *best_left_elm = nullptr;
      const bool is_right_edge = rnode->begin_pos == end_node_->begin_pos;
      const bool is_left_edge = rnode->begin_pos == begin_node_->end_pos;
      DCHECK(!(is_right_edge && is_left_edge));

      // is_edge is true if current lnode/rnode has same boundary as
      // begin/end node regardless of its value.
      const bool is_edge = (is_right_edge || is_left_edge);

      for (Node *lnode = lattice_->end_nodes(rnode->begin_pos);
           lnode != nullptr; lnode = lnode->enext) {
        // is_invalid_position is true if the lnode's location is invalid
        //  1.   |<-- begin_node_-->|
        //                    |<--lnode-->|  <== overlapped.
        //
        //  2.   |<-- begin_node_-->|
        //         |<--lnode-->|    <== exceeds begin_node.
        // This case can't be happened because the |rnode| is always at just
        // right of the |lnode|. By avoiding case1, this can't be happen.
        //  2'.  |<-- begin_node_-->|
        //         |<--lnode-->||<--rnode-->|
        const bool is_valid_position =
            !((lnode->begin_pos < begin_node_->end_pos &&
               begin_node_->end_pos < lnode->end_pos));
        if (!is_valid_position) {
          continue;
        }

        // If left_node is left edge, there is a cost-based constraint.
        const bool is_valid_cost =
            (lnode->cost - begin_node_->cost) <= kCostDiff;
        if (is_left_edge && !is_valid_cost) {
          continue;
        }

        // We can omit the search for the node which has the
        // same rid with |begin_node_| because:
        //  1. |begin_node_| is the part of the best route.
        //  2. The cost diff of 'LEFT_EDGE' is decided only by
        //     transition_cost for lnode.
        // Actually, checking for each rid once is enough.
        const bool can_omit_search =
            lnode->rid == begin_node_->rid && lnode != begin_node_;
        if (is_left_edge && can_omit_search) {
          continue;
        }

        DCHECK(this->boundary_checker_ != nullptr);
        BoundaryCheckResult boundary_result =
            (this->*boundary_checker_)(lnode, rnode, is_edge);
        if (boundary_result == INVALID) {
          continue;
        }

        // We can expand candidates from |rnode| to |lnode|.
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
          cost_diff = transition_cost + rnode->wcost +
                      (lnode->cost - begin_node_->cost);
          structure_cost_diff = 0;
          wcost_diff = rnode->wcost;
        } else {
          // use rnode->wcost.
          cost_diff = transition_cost + rnode->wcost;
          structure_cost_diff = transition_cost;
          wcost_diff = transition_cost + rnode->wcost;
        }

        if (boundary_result == VALID_WEAK_CONNECTED) {
          const int kWeakConnectedPenalty = 3453;  // log prob of 1/1000
          cost_diff += kWeakConnectedPenalty;
          structure_cost_diff += kWeakConnectedPenalty / 2;
          wcost_diff += kWeakConnectedPenalty / 2;
        }

        const int32 gx = cost_diff + top->gx;
        // |lnode->cost| is heuristics function of A* search, h(x).
        // After Viterbi search, we already know an exact value of h(x).
        const int32 fx = lnode->cost + gx;
        const int32 structure_gx = structure_cost_diff + top->structure_gx;
        const int32 w_gx = wcost_diff + top->w_gx;
        if (is_left_edge) {
          // We only need to only 1 left node here.
          // Even if expand all left nodes, all the |value| part should
          // be identical. Here, we simply use the best left edge node.
          // This hack reduces the number of redundant calls of pop().
          if (best_left_elm == nullptr || best_left_elm->fx > fx) {
            best_left_elm =
                CreateNewElement(lnode, top, fx, gx, structure_gx, w_gx);
          }
        } else {
          agenda_.Push(
              CreateNewElement(lnode, top, fx, gx, structure_gx, w_gx));
        }
      }

      if (best_left_elm != nullptr) {
        agenda_.Push(best_left_elm);
      }
    }
  }

  return false;
}

NBestGenerator::BoundaryCheckResult NBestGenerator::CheckOnlyMid(
    const Node *lnode, const Node *rnode, bool is_edge) const {
  // Special case, no boundary check
  if (rnode->node_type == Node::CON_NODE ||
      lnode->node_type == Node::CON_NODE) {
    return VALID;
  }

  // is_boundary is true if there is a grammar-based boundary
  // between lnode and rnode
  const bool is_boundary = (lnode->node_type == Node::HIS_NODE ||
                            segmenter_->IsBoundary(*lnode, *rnode, false));
  if (!is_edge && is_boundary) {
    // There is a boundary within the segment.
    return INVALID;
  }

  if (is_edge && !is_boundary) {
    // Here is not the boundary gramatically, but segmented by
    // other reason.
    return VALID_WEAK_CONNECTED;
  }

  return VALID;
}

NBestGenerator::BoundaryCheckResult NBestGenerator::CheckOnlyEdge(
    const Node *lnode, const Node *rnode, bool is_edge) const {
  // Special case, no boundary check
  if (rnode->node_type == Node::CON_NODE ||
      lnode->node_type == Node::CON_NODE) {
    return VALID;
  }

  // is_boundary is true if there is a grammar-based boundary
  // between lnode and rnode
  const bool is_boundary = (lnode->node_type == Node::HIS_NODE ||
                            segmenter_->IsBoundary(*lnode, *rnode, true));
  if (is_edge != is_boundary) {
    // on the edge, have a boudnary.
    // not on the edge, not the case.
    return INVALID;
  } else {
    return VALID;
  }
}

NBestGenerator::BoundaryCheckResult NBestGenerator::CheckStrict(
    const Node *lnode, const Node *rnode, bool is_edge) const {
  // Special case, no boundary check
  if (rnode->node_type == Node::CON_NODE ||
      lnode->node_type == Node::CON_NODE) {
    return VALID;
  }

  // is_boundary is true if there is a grammar-based boundary
  // between lnode and rnode
  const bool is_boundary = (lnode->node_type == Node::HIS_NODE ||
                            segmenter_->IsBoundary(*lnode, *rnode, false));

  if (is_edge != is_boundary) {
    // on the edge, have a boudnary.
    // not on the edge, not the case.
    return INVALID;
  } else {
    return VALID;
  }
}

int NBestGenerator::InsertTopResult(const std::string &original_key,
                                    Segment::Candidate *candidate,
                                    Segments::RequestType request_type) {
  nodes_.clear();
  int total_wcost = 0;
  for (const Node *node = begin_node_->next; node != end_node_;
       node = node->next) {
    nodes_.push_back(node);
    if (node != begin_node_->next) {
      total_wcost += node->wcost;
    }
  }
  DCHECK(!nodes_.empty());

  const int cost = end_node_->cost - begin_node_->cost - end_node_->wcost;
  const int structure_cost =
      end_node_->prev->cost - begin_node_->next->cost - total_wcost;
  const int wcost = end_node_->prev->cost - begin_node_->next->cost +
                    begin_node_->next->wcost;

  MakeCandidate(candidate, cost, structure_cost, wcost, nodes_);

  if (request_type == Segments::SUGGESTION) {
    candidate->attributes |= Segment::Candidate::REALTIME_CONVERSION;
  }

  viterbi_result_checked_ = true;
  const int result =
      filter_->FilterCandidate(original_key, candidate, nodes_, request_type);
  nodes_.clear();
  return result;
}

int NBestGenerator::GetTransitionCost(const Node *lnode,
                                      const Node *rnode) const {
  const int kInvalidPenaltyCost = 100000;
  if (rnode->constrained_prev != nullptr && lnode != rnode->constrained_prev) {
    return kInvalidPenaltyCost;
  }
  return connector_->GetTransitionCost(lnode->rid, rnode->lid);
}

}  // namespace mozc
