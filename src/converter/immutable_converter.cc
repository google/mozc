// Copyright 2010-2012, Google Inc.
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

#include "converter/immutable_converter.h"

#include <algorithm>
#include <climits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/base.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/connector_interface.h"
#include "converter/key_corrector.h"
#include "converter/lattice.h"
#include "converter/nbest_generator.h"
#include "converter/segmenter.h"
#include "converter/segmenter_interface.h"
#include "converter/segments.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suppression_dictionary.h"

DECLARE_bool(disable_lattice_cache);
DEFINE_bool(disable_predictive_realtime_conversion,
            false,
            "disable predictive realtime conversion");

namespace mozc {
namespace {

const size_t kMaxSegmentsSize                   = 256;
const size_t kMaxCharLength                     = 1024;
const size_t kMaxCharLengthForReverseConversion = 600;  // 200 chars in UTF8
const int    kMaxCost                           = 32767;
const int    kMinCost                           = -32767;
const int    kDefaultNumberCost                 = 3000;

void InsertCorrectedNodes(size_t pos, const string &key,
                          const KeyCorrector *key_corrector,
                          const DictionaryInterface *dictionary,
                          Lattice *lattice) {
  if (key_corrector == NULL) {
    return;
  }
  size_t length = 0;
  const char *str = key_corrector->GetCorrectedPrefix(pos, &length);
  if (str == NULL || length == 0) {
    return;
  }

  Node *rnode = dictionary->LookupPrefix(str, length,
                                         lattice->node_allocator());
  Node *head = NULL;
  Node *prev = NULL;
  for (Node *node = rnode; node != NULL; node = node->bnext) {
    const size_t offset =
        key_corrector->GetOriginalOffset(pos, node->key.size());
    if (!KeyCorrector::IsValidPosition(offset) || offset == 0) {
      continue;
    }
    node->key = key.substr(pos, offset);
    node->wcost += KeyCorrector::GetCorrectedCostPenalty(node->key);
    if (head == NULL) {
      head = node;
    }
    if (prev != NULL) {
      prev->bnext = node;
    }
    prev = node;
  }

  if (prev != NULL) {
    prev->bnext = NULL;
  }

  if (head != NULL) {
    lattice->Insert(pos, head);
  }
}

void MakeGroup(const Segments *segments, vector<uint16> *group) {
  group->clear();
  for (size_t i = 0; i < segments->segments_size(); ++i) {
    for (size_t j = 0; j < segments->segment(i).key().size(); ++j) {
      group->push_back(static_cast<uint16>(i));
    }
  }
  group->push_back(static_cast<uint16>(segments->segments_size() - 1));
}

bool IsNumber(const char c) {
  return c >= '0' && c <= '9';
}

void DecomposeNumberAndSuffix(const string &input,
                              string *number, string *suffix) {
  const char *begin = input.data();
  const char *end = input.data() + input.size();
  size_t pos = 0;
  while (begin < end) {
    if (IsNumber(*begin)) {
      ++pos;
      ++begin;
      continue;
    }
    break;
  }
  *number = input.substr(0, pos);
  *suffix = input.substr(pos, input.size() - pos);
}

void DecomposePrefixAndNumber(const string &input,
                              string *prefix, string *number) {
  const char *begin = input.data();
  const char *end = input.data() + input.size() - 1;
  size_t pos = input.size();
  while (begin <= end) {
    if (IsNumber(*end)) {
      --pos;
      --end;
      continue;
    }
    break;
  }
  *prefix = input.substr(0, pos);
  *number = input.substr(pos, input.size() - pos);
}

void NormalizeHistorySegments(Segments *segments) {
  for (size_t i = 0; i < segments->history_segments_size(); ++i) {
    Segment *segment = segments->mutable_history_segment(i);
    if (segment == NULL || segment->candidates_size() == 0) {
      continue;
    }

    string key;
    Segment::Candidate *c = segment->mutable_candidate(0);
    const string value = c->value;
    const string content_value = c->content_value;
    const string content_key = c->content_key;
    Util::FullWidthAsciiToHalfWidthAscii(segment->key(), &key);
    Util::FullWidthAsciiToHalfWidthAscii(value, &c->value);
    Util::FullWidthAsciiToHalfWidthAscii(content_value, &c->content_value);
    Util::FullWidthAsciiToHalfWidthAscii(content_key, &c->content_key);
    c->key = key;
    segment->set_key(key);

    // Ad-hoc rewrite for Numbers
    // Since number candidate is generative, i.e, any number can be
    // written by users, we normalize the value here. normalzied number
    // is used for the ranking tweaking based on history
    if (key.size() > 1 &&
        key == c->value &&
        key == c->content_value &&
        key == c->key &&
        key == c->content_key &&
        Util::GetScriptType(key) == Util::NUMBER &&
        IsNumber(key[key.size() - 1])) {
      key = key.substr(key.size() - 1, 1);  // use the last digit only
      segment->set_key(key);
      c->value = key;
      c->content_value = key;
      c->content_key = key;
    }
  }
}

Lattice *GetLattice(Segments *segments, bool is_prediction) {
  Lattice *lattice = segments->mutable_cached_lattice();

  const size_t history_segments_size = segments->history_segments_size();
  string conversion_key = "";
  for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
    conversion_key.append(segments->segment(i).key());
  }

  if (!is_prediction
      || FLAGS_disable_lattice_cache
      || Util::CharsLen(conversion_key) <= 1) {
    // Do not cache if conversion is not prediction, or disable_lattice_cache
    // flag is used.  In addition, if a user input the key right after the
    // finish of conversion, reset the lattice to erase old nodes.
    lattice->Clear();
  }

  return lattice;
}

}  // namespace

ConnectionTypeHandler::ConnectionTypeHandler(
    const vector<uint16> &group, const Segments *segments,
    const SegmenterInterface *segmenter) :
    group_(group), segments_(segments), segmenter_(segmenter),
    is_prediction_(
        segments_->request_type() == Segments::PREDICTION ||
        segments_->request_type() == Segments::SUGGESTION ||
        segments_->request_type() == Segments::PARTIAL_PREDICTION ||
        segments_->request_type() == Segments::PARTIAL_SUGGESTION) {}

void ConnectionTypeHandler::SetRNode(const Node *rnode) {
  rnode_ = rnode;
  // |rnode| does not fit the segment boundary
  if (rnode_->node_type != Node::EOS_NODE &&
      group_[rnode_->begin_pos] != group_[rnode_->end_pos - 1]) {
    ret_ = RETURN_NOT_CONNECTED;
    return;
  }

  // BOS/EOS node
  if (rnode_->node_type == Node::EOS_NODE ||
      rnode_->node_type == Node::HIS_NODE) {
    ret_ = RETURN_CONNECTED_IF_LNODE_IS_VALID;
    return;
  }

  ret_ = DEFAULT;
  rtype_ = segments_->segment(group_[rnode_->begin_pos]).segment_type();
}

ConnectionTypeHandler::ConnectionType
ConnectionTypeHandler::GetConnectionType(Node *lnode) const {
  switch (ret_) {
    case RETURN_NOT_CONNECTED: {
      return NOT_CONNECTED;
    }
    case RETURN_CONNECTED_IF_LNODE_IS_VALID: {
      if (lnode->node_type != Node::BOS_NODE &&
          group_[lnode->begin_pos] != group_[lnode->end_pos - 1]) {
        return NOT_CONNECTED;
      } else {
        return CONNECTED;
      }
    }
    case DEFAULT: {
      if (lnode->node_type != Node::BOS_NODE &&
          group_[lnode->begin_pos] != group_[lnode->end_pos - 1]) {
        return NOT_CONNECTED;
      }
      if (lnode->node_type == Node::BOS_NODE ||
          lnode->node_type == Node::HIS_NODE) {
        return CONNECTED;
      }
      if (rtype_ == Segment::FREE) {
        const Segment::SegmentType ltype =
            segments_->segment(group_[lnode->begin_pos]).segment_type();
        if (ltype == Segment::FREE) {
          return CONNECTED;
        }
      }
      const bool is_rule_boundary = segmenter_->IsBoundary(
          lnode, rnode_, is_prediction_);
      const bool is_constraint_boundary =
          group_[lnode->begin_pos] != group_[rnode_->begin_pos];
      if (is_constraint_boundary && !is_rule_boundary) {
        return WEAK_CONNECTED;
      }
      return CONNECTED;
    }
    default: {
      LOG(FATAL) << "Should not come here.";
      return NOT_CONNECTED;
    }
  }
}

ConnectionTypeHandler::ConnectionType
ConnectionTypeHandler::GetConnectionTypeForUnittest(
    const Node *lnode, const Node *rnode, const vector<uint16> &group,
    const Segments *segments, const SegmenterInterface *segmenter) {
  // |lnode| or |rnode| does not fit the segment boundary
  if ((rnode->node_type != Node::EOS_NODE &&
       group[rnode->begin_pos] != group[rnode->end_pos - 1]) ||
      (lnode->node_type != Node::BOS_NODE &&
       group[lnode->begin_pos] != group[lnode->end_pos - 1])) {
    return NOT_CONNECTED;
  }

  // BOS/EOS nodes
  if (lnode->node_type == Node::BOS_NODE ||
      rnode->node_type == Node::EOS_NODE ||
      lnode->node_type == Node::HIS_NODE ||
      rnode->node_type == Node::HIS_NODE) {
    return CONNECTED;
  }
  // lnode and rnode are both FREE Node
  const Segment::SegmentType ltype =
      segments->segment(group[lnode->begin_pos]).segment_type();
  const Segment::SegmentType rtype =
      segments->segment(group[rnode->begin_pos]).segment_type();
  if (ltype == Segment::FREE && rtype == Segment::FREE) {
    return CONNECTED;
  }

  const bool is_prediction =
      (segments->request_type() == Segments::PREDICTION ||
       segments->request_type() == Segments::SUGGESTION ||
       segments->request_type() == Segments::PARTIAL_PREDICTION ||
       segments->request_type() == Segments::PARTIAL_SUGGESTION);

  const bool is_rule_boundary = segmenter->IsBoundary(lnode, rnode,
                                                      is_prediction);
  const bool is_constraint_boundary =
      group[lnode->begin_pos] != group[rnode->begin_pos];

  if (is_constraint_boundary && !is_rule_boundary) {
    return WEAK_CONNECTED;
  }

  return CONNECTED;
}

// Constructor for keeping compatibility
// TODO(team): Change code to use another constractor and

// specify implementations.
ImmutableConverterImpl::ImmutableConverterImpl()
    : dictionary_(DictionaryFactory::GetDictionary()),
      suffix_dictionary_(SuffixDictionaryFactory::GetSuffixDictionary()),
      suppression_dictionary_(Singleton<SuppressionDictionary>::get()),
      connector_(ConnectorFactory::GetConnector()),
      segmenter_(Singleton<Segmenter>::get()),
      pos_matcher_(UserPosManager::GetUserPosManager()->GetPOSMatcher()),
      pos_group_(UserPosManager::GetUserPosManager()->GetPosGroup()),
      first_name_id_(pos_matcher_->GetFirstNameId()),
      last_name_id_(pos_matcher_->GetLastNameId()),
      number_id_(pos_matcher_->GetNumberId()),
      unknown_id_(pos_matcher_->GetUnknownId()),
      last_to_first_name_transition_cost_(
          connector_->GetTransitionCost(last_name_id_, first_name_id_)) {
  DCHECK(dictionary_);
  DCHECK(suffix_dictionary_);
  DCHECK(connector_);
  DCHECK(segmenter_);
  DCHECK(pos_matcher_);
  DCHECK(pos_group_);
}

ImmutableConverterImpl::ImmutableConverterImpl(
    const DictionaryInterface *dictionary,
    const DictionaryInterface *suffix_dictionary,
    const SuppressionDictionary *suppression_dictionary,
    const ConnectorInterface *connector,
    const SegmenterInterface *segmenter,
    const POSMatcher *pos_matcher,
    const PosGroup *pos_group)
    : dictionary_(dictionary),
      suffix_dictionary_(suffix_dictionary),
      suppression_dictionary_(suppression_dictionary),
      connector_(connector),
      segmenter_(segmenter),
      pos_matcher_(pos_matcher),
      pos_group_(pos_group),
      first_name_id_(pos_matcher_->GetFirstNameId()),
      last_name_id_(pos_matcher_->GetLastNameId()),
      number_id_(pos_matcher_->GetNumberId()),
      unknown_id_(pos_matcher_->GetUnknownId()),
      last_to_first_name_transition_cost_(
          connector_->GetTransitionCost(last_name_id_, first_name_id_)) {
  DCHECK(dictionary_);
  DCHECK(suffix_dictionary_);
  DCHECK(suppression_dictionary_);
  DCHECK(connector_);
  DCHECK(segmenter_);
  DCHECK(pos_matcher_);
  DCHECK(pos_group_);
}

void ImmutableConverterImpl::ExpandCandidates(
    NBestGenerator *nbest, Segment *segment,
    Segments::RequestType request_type, size_t expand_size) const {
  DCHECK(nbest);
  DCHECK(segment);
  CHECK_GT(expand_size, 0);

  while (segment->candidates_size() < expand_size) {
    Segment::Candidate *candidate = segment->push_back_candidate();
    DCHECK(candidate);
    candidate->Init();

    // if NBestGenerator::Next() returns NULL,
    // no more entries are generated.
    if (!nbest->Next(candidate, request_type)) {
      segment->pop_back_candidate();
      break;
    }
  }
}

void ImmutableConverterImpl::InsertDummyCandidates(Segment *segment,
                                                   size_t expand_size) const {
  const Segment::Candidate *top_candidate =
      segment->candidates_size() == 0 ? NULL :
      segment->mutable_candidate(0);
  const Segment::Candidate *last_candidate =
      segment->candidates_size() == 0 ? NULL :
      segment->mutable_candidate(segment->candidates_size() - 1);

  // Insert a dummy candiate whose content_value is katakana.
  // If functional_key() is empty, no need to make a dummy candidate.
  if (segment->candidates_size() > 0 &&
      segment->candidates_size() < expand_size &&
      !segment->candidate(0).functional_key().empty() &&
      Util::GetScriptType(segment->candidate(0).content_key) ==
      Util::HIRAGANA) {
    // Use last_candidate as a refernce of cost.
    // Use top_candidate as a refarence of lid/rid and key/value.
    DCHECK(top_candidate);
    DCHECK(last_candidate);
    Segment::Candidate *new_candidate = segment->add_candidate();
    DCHECK(new_candidate);

    string katakana_value;
    Util::HiraganaToKatakana(segment->candidate(0).content_key,
                             &katakana_value);

    new_candidate->CopyFrom(*top_candidate);
    new_candidate->value = katakana_value + top_candidate->functional_value();
    new_candidate->content_value = katakana_value;
    new_candidate->cost = last_candidate->cost + 1;
    new_candidate->wcost = last_candidate->wcost + 1;
    new_candidate->structure_cost = last_candidate->structure_cost + 1;
    new_candidate->attributes = 0;
    last_candidate = new_candidate;
  }

  // Insert a dummy hiragana candidate.
  if (segment->candidates_size() == 0 ||
      (segment->candidates_size() < expand_size &&
       Util::GetScriptType(segment->key()) == Util::HIRAGANA)) {
    Segment::Candidate *new_candidate = segment->add_candidate();
    DCHECK(new_candidate);

    if (last_candidate != NULL) {
      new_candidate->CopyFrom(*last_candidate);
    } else {
      new_candidate->Init();
    }
    new_candidate->key = segment->key();
    new_candidate->value = segment->key();
    new_candidate->content_key = segment->key();
    new_candidate->content_value = segment->key();
    if (last_candidate != NULL) {
      new_candidate->cost = last_candidate->cost + 1;
      new_candidate->wcost = last_candidate->wcost + 1;
      new_candidate->structure_cost = last_candidate->structure_cost + 1;
    }
    new_candidate->attributes = 0;
    last_candidate = new_candidate;
    // One character hiragana/katakana will cause side effect.
    // Type "し" and choose "シ". After that, "しました" will become "シました".
    if (Util::CharsLen(new_candidate->key) <= 1) {
      new_candidate->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    }
  }

  // Insert a dummy katakana candidate.
  string katakana_value;
  Util::HiraganaToKatakana(segment->key(), &katakana_value);
  if (segment->candidates_size() > 0 &&
      segment->candidates_size() < expand_size &&
      Util::GetScriptType(katakana_value) == Util::KATAKANA) {
    Segment::Candidate *new_candidate = segment->add_candidate();
    DCHECK(new_candidate);
    DCHECK(last_candidate);
    new_candidate->Init();
    new_candidate->key = segment->key();
    new_candidate->value = katakana_value;
    new_candidate->content_key = segment->key();
    new_candidate->content_value = katakana_value;
    new_candidate->cost = last_candidate->cost + 1;
    new_candidate->wcost = last_candidate->wcost + 1;
    new_candidate->structure_cost = last_candidate->structure_cost + 1;
    new_candidate->lid = last_candidate->lid;
    new_candidate->rid = last_candidate->rid;
    if (Util::CharsLen(new_candidate->key) <= 1) {
      new_candidate->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    }
  }

  DCHECK_GT(segment->candidates_size(), 0);
}

void ImmutableConverterImpl::ApplyResegmentRules(
    size_t pos, Lattice *lattice) const {
  if (ResegmentArabicNumberAndSuffix(pos, lattice)) {
    VLOG(1) << "ResegmentArabicNumberAndSuffix returned true";
    return;
  }

  if (ResegmentPrefixAndArabicNumber(pos, lattice)) {
    VLOG(1) << "ResegmentArabicNumberAndSuffix returned true";
    return;
  }

  if (ResegmentPersonalName(pos, lattice)) {
    VLOG(1) << "ResegmentPersonalName returned true";
    return;
  }
}

// Currently, only arabic_number + suffix patterns are resegmented
// TODO(taku): consider kanji number into consideration
bool ImmutableConverterImpl::ResegmentArabicNumberAndSuffix(
    size_t pos, Lattice *lattice) const {
  const Node *bnode = lattice->begin_nodes(pos);
  if (bnode == NULL) {
    VLOG(1) << "bnode is NULL";
    return false;
  }

  bool modified = false;

  for (const Node *compound_node = bnode;
       compound_node != NULL; compound_node = compound_node->bnext) {
    if (!compound_node->value.empty() && !compound_node->key.empty() &&
        pos_matcher_->IsNumber(compound_node->lid) &&
        !pos_matcher_->IsNumber(compound_node->rid) &&
        IsNumber(compound_node->value[0]) && IsNumber(compound_node->key[0])) {
      string number_value, number_key;
      string suffix_value, suffix_key;
      DecomposeNumberAndSuffix(compound_node->value,
                               &number_value, &suffix_value);
      DecomposeNumberAndSuffix(compound_node->key,
                               &number_key, &suffix_key);

      if (suffix_value.empty() || suffix_key.empty()) {
        continue;
      }

      // not compatibile
      if (number_value != number_key) {
        LOG(WARNING) << "Incompatible key/value number pair";
        continue;
      }

      // do -1 so that resegmented nodes are boosted
      // over compound node.
      const int32 wcost = max(compound_node->wcost / 2 - 1, 0);

      Node *number_node = lattice->NewNode();
      CHECK(number_node);
      number_node->key = number_key;
      number_node->value = number_value;
      number_node->lid = compound_node->lid;
      number_node->rid = 0;   // 0 to 0 transition cost is 0
      number_node->wcost = wcost;
      number_node->node_type = Node::NOR_NODE;
      number_node->bnext = NULL;

      // insert number into the lattice
      lattice->Insert(pos, number_node);

      Node *suffix_node = lattice->NewNode();
      CHECK(suffix_node);
      suffix_node->key = suffix_key;
      suffix_node->value = suffix_value;
      suffix_node->lid = 0;
      suffix_node->rid = compound_node->rid;
      suffix_node->wcost = wcost;
      suffix_node->node_type = Node::NOR_NODE;
      suffix_node->bnext = NULL;

      suffix_node->constrained_prev = number_node;

      // insert suffix into the lattice
      lattice->Insert(pos + number_node->key.size(), suffix_node);
      VLOG(1) << "Resegmented: " << compound_node->value << " "
              << number_node->value << " " << suffix_node->value;

      modified = true;
    }
  }

  return modified;
}

bool ImmutableConverterImpl::ResegmentPrefixAndArabicNumber(
    size_t pos, Lattice *lattice) const {
  const Node *bnode = lattice->begin_nodes(pos);
  if (bnode == NULL) {
    VLOG(1) << "bnode is NULL";
    return false;
  }

  bool modified = false;

  for (const Node *compound_node = bnode;
       compound_node != NULL; compound_node = compound_node->bnext) {
    // Unlike ResegmentArabicNumberAndSuffix, we don't
    // check POS as words ending with Arabic numbers are pretty rare.
    if (compound_node->value.size() > 1 && compound_node->key.size() > 1 &&
        !IsNumber(compound_node->value[0]) &&
        !IsNumber(compound_node->key[0]) &&
        IsNumber(compound_node->value[compound_node->value.size() - 1]) &&
        IsNumber(compound_node->key[compound_node->key.size() - 1])) {
      string number_value, number_key;
      string prefix_value, prefix_key;
      DecomposePrefixAndNumber(compound_node->value,
                               &prefix_value, &number_value);
      DecomposePrefixAndNumber(compound_node->key,
                               &prefix_key, &number_key);

      if (prefix_value.empty() || prefix_key.empty()) {
        continue;
      }

      // not compatibile
      if (number_value != number_key) {
        LOG(WARNING) << "Incompatible key/value number pair";
        continue;
      }

      // do -1 so that resegmented nodes are boosted
      // over compound node.
      const int32 wcost = max(compound_node->wcost / 2 - 1, 0);

      Node *prefix_node = lattice->NewNode();
      CHECK(prefix_node);
      prefix_node->key = prefix_key;
      prefix_node->value = prefix_value;
      prefix_node->lid = compound_node->lid;
      prefix_node->rid = 0;   // 0 to 0 transition cost is 0
      prefix_node->wcost = wcost;
      prefix_node->node_type = Node::NOR_NODE;
      prefix_node->bnext = NULL;

      // insert number into the lattice
      lattice->Insert(pos, prefix_node);

      Node *number_node = lattice->NewNode();
      CHECK(number_node);
      number_node->key = number_key;
      number_node->value = number_value;
      number_node->lid = 0;
      number_node->rid = compound_node->rid;
      number_node->wcost = wcost;
      number_node->node_type = Node::NOR_NODE;
      number_node->bnext = NULL;

      number_node->constrained_prev = prefix_node;

      // insert number into the lattice
      lattice->Insert(pos + prefix_node->key.size(), number_node);
      VLOG(1) << "Resegmented: " << compound_node->value << " "
              << prefix_node->value << " " << number_node->value;

      modified = true;
    }
  }

  return modified;
}

bool ImmutableConverterImpl::ResegmentPersonalName(
    size_t pos, Lattice *lattice) const {
  const Node *bnode = lattice->begin_nodes(pos);
  if (bnode == NULL) {
    VLOG(1) << "bnode is NULL";
    return false;
  }

  bool modified = false;

  // find a combination of last_name and first_name, e.g. "田中麗奈".
  for (const Node *compound_node = bnode;
       compound_node != NULL; compound_node = compound_node->bnext) {
    // left word is last name and right word is first name
    if (compound_node->lid != last_name_id_ ||
        compound_node->rid != first_name_id_) {
      continue;
    }

    const size_t len = Util::CharsLen(compound_node->value);

    // Don't resegment one-word last_name/first_name like 林健,
    // as it would deliver side effect.
    if (len <= 2) {
      continue;
    }

    // Don't resegment if the value is katakana
    if (Util::GetScriptType(compound_node->value) == Util::KATAKANA) {
      continue;
    }

    // Do constrained Viterbi search inside the compound "田中麗奈".
    // Constraints:
    // 1. Concats of last_name and first_name should be "田中麗奈"
    // 2. consisting of two words (last_name and first_name)
    // 3. Segment-boundary exist between the two words.
    // 4.a Either (a) POS of lnode is last_name or (b) POS of rnode is fist_name
    //     (len >= 4)
    // 4.b Both (a) POS of lnode is last_name and (b) POS of rnode is fist_name
    //     (len == 3)
    const Node *best_last_name_node = NULL;
    const Node *best_first_name_node = NULL;
    int best_cost = 0x7FFFFFFF;
    for (const Node *lnode = bnode; lnode != NULL; lnode = lnode->bnext) {
      // lnode(last_name) is a prefix of compound, Constraint 1.
      if (compound_node->value.size() > lnode->value.size() &&
          compound_node->key.size() > lnode->key.size() &&
          Util::StartsWith(compound_node->value, lnode->value)) {
        // rnode(first_name) is a suffix of compound, Constraint 1.
        for (const Node *rnode = lattice->begin_nodes(pos + lnode->key.size());
             rnode != NULL; rnode = rnode->bnext) {
          if ((lnode->value.size() + rnode->value.size())
              == compound_node->value.size() &&
              (lnode->value + rnode->value) == compound_node->value &&
              segmenter_->IsBoundary(lnode, rnode, false)) {   // Constraint 3.
            const int32 cost = lnode->wcost + GetCost(lnode, rnode);
            if (cost < best_cost) {   // choose the smallest ones
              best_last_name_node = lnode;
              best_first_name_node = rnode;
              best_cost = cost;
            }
          }
        }
      }
    }

    // No valid first/last names are found
    if (best_first_name_node == NULL || best_last_name_node == NULL) {
      continue;
    }

    // Constraint 4.a
    if (len >= 4 &&
        (best_last_name_node->lid != last_name_id_ &&
         best_first_name_node->rid != first_name_id_)) {
      continue;
    }

    // Constraint 4.b
    if (len == 3 &&
        (best_last_name_node->lid != last_name_id_ ||
         best_first_name_node->rid != first_name_id_)) {
      continue;
    }

    // Insert LastName and FirstName as independent nodes.
    // Duplications will be removed in nbest enumerations.
    // word costs are calculated from compound node by assuming that
    // transition cost is 0.
    //
    // last_name_cost + transition_cost + first_name_cost == compound_cost
    // last_name_cost == first_name_cost
    // i.e,
    // last_name_cost = first_name_cost =
    // (compound_cost - transition_cost) / 2;
    const int32 wcost = (compound_node->wcost -
                         last_to_first_name_transition_cost_) / 2;

    Node *last_name_node = lattice->NewNode();
    CHECK(last_name_node);
    last_name_node->key = best_last_name_node->key;
    last_name_node->value = best_last_name_node->value;
    last_name_node->lid = compound_node->lid;
    last_name_node->rid = last_name_id_;
    last_name_node->wcost = wcost;
    last_name_node->node_type = Node::NOR_NODE;
    last_name_node->bnext = NULL;

    // insert last_name into the lattice
    lattice->Insert(pos, last_name_node);

    Node *first_name_node = lattice->NewNode();
    CHECK(first_name_node);
    first_name_node->key = best_first_name_node->key;
    first_name_node->value = best_first_name_node->value;
    first_name_node->lid = first_name_id_;
    first_name_node->rid = compound_node->rid;
    first_name_node->wcost = wcost;
    first_name_node->node_type = Node::NOR_NODE;
    first_name_node->bnext = NULL;

    first_name_node->constrained_prev = last_name_node;

    // insert first_name into the lattice
    lattice->Insert(pos + last_name_node->key.size(), first_name_node);

    VLOG(2) << "Resegmented: " << compound_node->value << " "
            << last_name_node->value << " " << first_name_node->value;

    modified = true;
  }

  return modified;
}

Node *ImmutableConverterImpl::Lookup(const int begin_pos,
                                     const int end_pos,
                                     bool is_reverse,
                                     bool is_prediction,
                                     Lattice *lattice) const {
  CHECK_LE(begin_pos, end_pos);
  const char *begin = lattice->key().data() + begin_pos;
  const char *end = lattice->key().data() + end_pos;
  const size_t len = end_pos - begin_pos;

  lattice->node_allocator()->set_max_nodes_size(8192);
  Node *result_node = NULL;
  if (is_reverse) {
    result_node = dictionary_->LookupReverse(begin, len,
                                             lattice->node_allocator());
  } else {
    if (is_prediction && !FLAGS_disable_lattice_cache) {
      DictionaryInterface::Limit limit;
      limit.key_len_lower_limit = lattice->cache_info(begin_pos) + 1;

      result_node =
          dictionary_->LookupPrefixWithLimit(
              begin, len, limit, lattice->node_allocator());

      // add ENABLE_CACHE attribute and set raw_wcost
      for (Node *node = result_node; node != NULL; node = node->bnext) {
        node->attributes |= Node::ENABLE_CACHE;
        node->raw_wcost = node->wcost;
      }

      lattice->SetCacheInfo(begin_pos, len);
    } else {
      // when cache feature is not used, look up normally
      result_node = dictionary_->LookupPrefix(
          begin, len, lattice->node_allocator());
    }
  }
  return AddCharacterTypeBasedNodes(begin, end, lattice, result_node);
}

Node *ImmutableConverterImpl::AddCharacterTypeBasedNodes(
    const char *begin, const char *end, Lattice *lattice, Node *nodes) const {

  size_t mblen = 0;
  const char32 ucs4 = Util::UTF8ToUCS4(begin, end, &mblen);

  const Util::ScriptType first_script_type = Util::GetScriptType(ucs4);
  const Util::FormType first_form_type = Util::GetFormType(ucs4);

  // Add 1 character node. It can be either UnknownId or NumberId.
  {
    Node *new_node = lattice->NewNode();
    CHECK(new_node);
    if (first_script_type == Util::NUMBER) {
      new_node->lid = number_id_;
      new_node->rid = number_id_;
    } else {
      new_node->lid = unknown_id_;
      new_node->rid = unknown_id_;
    }

    new_node->wcost = kMaxCost;
    new_node->value.assign(begin, mblen);
    new_node->key.assign(begin, mblen);
    new_node->node_type = Node::NOR_NODE;
    new_node->bnext = nodes;
    nodes = new_node;
  }  // scope out |new_node|

  if (first_script_type == Util::NUMBER) {
    nodes->wcost = kDefaultNumberCost;
    return nodes;
  }

  if (first_script_type != Util::ALPHABET &&
      first_script_type != Util::KATAKANA) {
    return nodes;
  }

  // group by same char type
  int num_char = 1;
  const char *p = begin + mblen;
  while (p < end) {
    const char32 next_ucs4 = Util::UTF8ToUCS4(p, end, &mblen);
    if (first_script_type != Util::GetScriptType(next_ucs4) ||
        first_form_type != Util::GetFormType(next_ucs4)) {
      break;
    }
    p += mblen;
    ++num_char;
  }

  if (num_char > 1) {
    mblen = static_cast<uint32>(p - begin);
    Node *new_node = lattice->NewNode();
    CHECK(new_node);
    if (first_script_type == Util::NUMBER) {
      new_node->lid = number_id_;
      new_node->rid = number_id_;
    } else {
      new_node->lid = unknown_id_;
      new_node->rid = unknown_id_;
    }
    new_node->wcost = kMaxCost / 2;
    new_node->value.assign(begin, mblen);
    new_node->key.assign(begin, mblen);
    new_node->node_type = Node::NOR_NODE;
    new_node->bnext = nodes;
    nodes = new_node;
  }

  return nodes;
}

bool ImmutableConverterImpl::Viterbi(Segments *segments,
                                     const Lattice &lattice,
                                     const vector<uint16> &group) const {
  DCHECK(segments);

  // log prob of 1/1000
  const int kWeakConnectedPeanlty = 3453;

  // Reasonably big cost. Cannot use INT_MAX a new cost will be calculated
  // based on kVeryBigCost.
  const int kVeryBigCost = (INT_MAX >> 2);

  const string &key = lattice.key();

  ConnectionTypeHandler connection_type_handler(group, segments, segmenter_);
  for (size_t pos = 0; pos <= key.size(); ++pos) {
    for (Node *rnode = lattice.begin_nodes(pos);
         rnode != NULL; rnode = rnode->bnext) {
      int best_cost = INT_MAX;
      Node *best_node = NULL;
      connection_type_handler.SetRNode(rnode);
      for (Node *lnode = lattice.end_nodes(pos);
           lnode != NULL; lnode = lnode->enext) {
        int cost = 0;
        switch (connection_type_handler.GetConnectionType(lnode)) {
          case ConnectionTypeHandler::CONNECTED:
            cost = lnode->cost + GetCost(lnode, rnode);
            break;
          case ConnectionTypeHandler::WEAK_CONNECTED:
            // word boundary with WEAK_CONNECTED is created as follows
            // - [ABCD] becomes one segment with converter,
            //   where A, B, C and D a word
            // - User changed the boundary into ABC|D
            // - The boundary between C and D is WEAK_CONNECTED.
            // Here, we simply demote the transition probability of
            // WEAK_CONNECTED.
            // Issue is how strongly we should demote?
            // - If converter strongly obeys user-preference and demotes
            //      the probability aggressively, the word D will disappear,
            //      since C->D transition gets rarer.
            // - If converter ignores user-preference, it's also annoying, as
            //    the result will be unchanged even after changing the boundary.
            cost = lnode->cost + GetCost(lnode, rnode) + kWeakConnectedPeanlty;
            rnode->attributes |= Node::WEAK_CONNECTED;
            break;
          case ConnectionTypeHandler::NOT_CONNECTED:
            cost = kVeryBigCost;
            break;
          default:
            break;
        }

        if (cost < best_cost) {
          best_node = lnode;
          best_cost = cost;
        }
      }
      rnode->prev = best_node;
      rnode->cost = best_cost;
    }
  }

  Node *node = lattice.eos_nodes();
  CHECK(node->bnext == NULL);
  Node *prev = NULL;
  while (node->prev != NULL) {
    prev = node->prev;
    prev->next = node;
    node = prev;
  }

  if (lattice.bos_nodes() != prev) {
    LOG(WARNING) << "cannot make lattice";
    return false;
  }

  return true;
}

// faster Viterbi algorithm for prediction
//
// Run simple Viterbi algorithm with contracting the same lid and rid.
// Because the original Viterbi has speciall nodes, we should take it
// consideration.
// 1. CONNECTED nodes: are normal nodes.
// 2. WEAK_CONNECTED nodes: don't occur in prediction, so we do not have to
//    consider about them.
// 3. NOT_CONNECTED nodes: occur when they are between history nodes and
//    normal nodes.
// For NOT_CONNECTED nodes, we should run Viterbi for history nodes first,
// and do it for normal nodes second. The function "PredictionViterbiSub"
// runs Viterbi for positions between calc_begin_pos and calc_end_pos,
// inclusive.
//
// We cannot apply this function in suggestion because in suggestion there are
// WEAK_CONNECTED nodes and this function is not designed for them.
//
// TODO(toshiyuki): We may be able to use faster viterbi for
// conversion/suggestion if we use richer info as contraction group.

bool ImmutableConverterImpl::PredictionViterbi(Segments *segments,
                                               const Lattice &lattice) const {
  const size_t &key_length = lattice.key().size();
  const size_t history_segments_size = segments->history_segments_size();
  size_t history_length = 0;
  for (size_t i = 0; i < history_segments_size; ++i) {
    history_length += segments->segment(i).key().size();
  }
  PredictionViterbiSub(segments, lattice, 0, history_length);
  PredictionViterbiSub(segments, lattice, history_length, key_length);

  Node *node = lattice.eos_nodes();
  CHECK(node->bnext == NULL);
  Node *prev = NULL;
  while (node->prev != NULL) {
    prev = node->prev;
    prev->next = node;
    node = prev;
  }

  if (lattice.bos_nodes() != prev) {
    LOG(WARNING) << "cannot make lattice";
    return false;
  }

  return true;
}

void ImmutableConverterImpl::PredictionViterbiSub(Segments *segments,
                                                  const Lattice &lattice,
                                                  int calc_begin_pos,
                                                  int calc_end_pos) const {
  CHECK_LE(calc_begin_pos, calc_end_pos);
  for (size_t pos = calc_begin_pos; pos <= calc_end_pos; ++pos) {
    // Mapping from lnode's rid to (cost, Node) of best way/cost
    map<int, pair<int, Node*> > lbest;

    for (Node *lnode = lattice.end_nodes(pos);
         lnode != NULL; lnode = lnode->enext) {
      const int rid = lnode->rid;
      map<int, pair<int, Node*> >::iterator it_best = lbest.find(rid);
      if (it_best == lbest.end()) {
        lbest[rid] = pair<int, Node*>(INT_MAX, static_cast<Node*>(NULL));
        it_best = lbest.find(rid);
      }
      if (lnode->cost < it_best->second.first) {
        it_best->second.first = lnode->cost;
        it_best->second.second = lnode;
      }
    }

    map<int, int> rbest_cost;
    for (Node *rnode = lattice.begin_nodes(pos);
         rnode != NULL; rnode = rnode->bnext) {
      if (rnode->end_pos > calc_end_pos) {
        continue;
      }
      rbest_cost[rnode->lid] = INT_MAX;
    }

    map<int, Node*> rbest_prev_node;
    for (map<int, pair<int, Node*> >::iterator lt = lbest.begin();
         lt != lbest.end(); ++lt) {
      for (map<int, int>::iterator rt = rbest_cost.begin();
           rt != rbest_cost.end(); ++rt) {
        const int cost = lt->second.first +
            connector_->GetTransitionCost(lt->first, rt->first);
        if (cost < rt->second) {
          rbest_prev_node[rt->first] = lt->second.second;
          rt->second = cost;
        }
      }
    }

    for (Node *rnode = lattice.begin_nodes(pos);
         rnode != NULL; rnode = rnode->bnext) {
      if (rnode->end_pos > calc_end_pos) {
        continue;
      }
      const int lid = rnode->lid;
      if (rbest_prev_node[lid] == NULL) {
        continue;
      }
      rnode->prev = rbest_prev_node[lid];
      rnode->cost = rbest_cost[lid] + rnode->wcost;
    }
  }
}

// Add predictive nodes from conversion key.
void ImmutableConverterImpl::MakeLatticeNodesForPredictiveNodes(
    Lattice *lattice, Segments *segments) const {
  const string &key = lattice->key();
  string conversion_key;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    conversion_key += segments->conversion_segment(i).key();
  }
  DCHECK_NE(string::npos, key.find(conversion_key));
  vector<string> conversion_key_chars;
  Util::SplitStringToUtf8Chars(conversion_key, &conversion_key_chars);

  // do nothing if the conversion key is short
  const size_t kKeyMinLength = 7;
  if (conversion_key_chars.size() < kKeyMinLength) {
    return;
  }

  // predictive search from suffix dictionary
  // (search words with between 1 and 6 characters)
  {
    const size_t kMaxSuffixLookupKey = 6;
    const size_t max_sufffix_len =
        min(kMaxSuffixLookupKey, conversion_key_chars.size());
    size_t pos = key.size();
    for (size_t suffix_len = 1; suffix_len <= max_sufffix_len; ++suffix_len) {
      pos -= conversion_key_chars[
          conversion_key_chars.size() - suffix_len].size();
      DCHECK_GE(key.size(), pos);

      const size_t str_len = key.size() - pos;
      Node *result_node = suffix_dictionary_->LookupPredictive(
          key.data() + pos, str_len, lattice->node_allocator());
      AddPredictiveNodes(suffix_len, pos, lattice, result_node);
    }
  }

  // predictive search from system dictionary
  // (search words with between 5 and 8 characters)
  {
    const size_t kMinSystemLookupKey = 5;
    const size_t kMaxSystemLookupKey = 8;
    const size_t max_suffix_len =
        min(kMaxSystemLookupKey, conversion_key_chars.size());
    size_t pos = key.size();
    for (size_t suffix_len = 1; suffix_len <= max_suffix_len; ++suffix_len) {
      pos -= conversion_key_chars[
          conversion_key_chars.size() - suffix_len].size();
      DCHECK_GE(key.size(), pos);

      if (suffix_len < kMinSystemLookupKey) {
        // Just update |pos|.
        continue;
      }

      const size_t str_len = key.size() - pos;
      Node *result_node = dictionary_->LookupPredictive(
          key.data() + pos, str_len, lattice->node_allocator());
      AddPredictiveNodes(suffix_len, pos, lattice, result_node);
    }
  }
}

void ImmutableConverterImpl::AddPredictiveNodes(const size_t &len,
                                                const size_t &pos,
                                                Lattice *lattice,
                                                Node *result_node) const {
  if (result_node == NULL) {
    return;
  }

  // add penalty cost to each nodes
  for (Node* rnode = result_node; rnode != NULL; rnode = rnode->bnext) {
    // add penalty to predictive nodes
    const int kPredictiveNodeDefaultPenalty = 900;  // =~ -500 * log(1/6)
    int additional_cost = kPredictiveNodeDefaultPenalty;

    // bonus for suffix word
    if (pos_matcher_->IsSuffixWord(rnode->rid)
        && pos_matcher_->IsSuffixWord(rnode->lid)) {
      const int kSuffixWordBonus = 700;
      additional_cost -= kSuffixWordBonus;
    }

    // penalty for unique noun word
    if (pos_matcher_->IsUniqueNoun(rnode->rid)
        || pos_matcher_->IsUniqueNoun(rnode->lid)) {
      const int kUniqueNounPenalty = 500;
      additional_cost += kUniqueNounPenalty;
    }

    // penalty for number
    if (pos_matcher_->IsNumber(rnode->rid)
        || pos_matcher_->IsNumber(rnode->lid)) {
      const int kNumberPenalty = 4000;
      additional_cost += kNumberPenalty;
    }

    rnode->wcost += additional_cost;
  }

  // insert nodes
  lattice->Insert(pos, result_node);
}

bool ImmutableConverterImpl::MakeLattice(Lattice *lattice,
                                         Segments *segments) const {
  if (segments == NULL) {
    LOG(ERROR) << "Segments is NULL";
    return false;
  }

  if (lattice == NULL) {
    LOG(ERROR) << "Lattice is NULL";
    return false;
  }

  if (segments->segments_size() >= kMaxSegmentsSize) {
    LOG(WARNING) << "too many segments";
    return false;
  }

  NormalizeHistorySegments(segments);

  const bool is_reverse =
      (segments->request_type() == Segments::REVERSE_CONVERSION);

  const bool is_prediction =
      (segments->request_type() == Segments::SUGGESTION ||
       segments->request_type() == Segments::PREDICTION);

  // In suggestion mode, ImmutableConverter will not accept multiple-segments.
  // The result always consists of one segment.
  if ((is_reverse || is_prediction) &&
      (segments->conversion_segments_size() != 1 ||
       segments->conversion_segment(0).segment_type() != Segment::FREE)) {
    LOG(WARNING) << "ImmutableConverter doesn't support constrained requests";
    return false;
  }

  string history_key, conversion_key;
  const size_t history_segments_size = segments->history_segments_size();
  for (size_t i = 0; i < history_segments_size; ++i) {
    DCHECK(!segments->segment(i).key().empty());
    history_key.append(segments->segment(i).key());
  }

  for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
    DCHECK(!segments->segment(i).key().empty());
    conversion_key.append(segments->segment(i).key());
  }

  const size_t max_char_len =
      is_reverse ? kMaxCharLengthForReverseConversion : kMaxCharLength;
  if (history_key.size() + conversion_key.size() >= max_char_len) {
    LOG(WARNING) << "too long input chars";
    return false;
  }

  if (conversion_key.empty()) {
    LOG(WARNING) << "key is empty";
    return false;
  }

  const string key = history_key + conversion_key;
  lattice->UpdateKey(key);
  lattice->ResetNodeCost();

  if (is_reverse) {
    // Reverse lookup for each prefix string in key is slow with current
    // implementation, so run it for them at once and cache the result.
    dictionary_->PopulateReverseLookupCache(
        key.c_str(), key.size(), lattice->node_allocator());
  }

  bool is_valid_lattice = true;
  // Perform the main part of lattice construction.
  if (!MakeLatticeNodesForHistorySegments(lattice, segments) ||
      lattice->end_nodes(history_key.size()) == NULL) {
    is_valid_lattice = false;
  }

  // Can not apply key corrector to invalid lattice.
  if (is_valid_lattice) {
    MakeLatticeNodesForConversionSegments(history_key, segments, lattice);
  }

  if (is_reverse) {
    // No reverse look up will happen afterwards.
    dictionary_->ClearReverseLookupCache(
        lattice->node_allocator());
  }

  // Predictive real time conversion
  if (is_prediction && !FLAGS_disable_predictive_realtime_conversion) {
    MakeLatticeNodesForPredictiveNodes(lattice, segments);
  }

  if (!is_valid_lattice) {
    // Safely bail out, since reverse look up cache was released already.
    return false;
  }

  if (lattice->end_nodes(key.size()) == NULL) {
    LOG(WARNING) << "cannot build lattice from input";
    return false;
  }

  ApplyPrefixSuffixPenalty(conversion_key, lattice);

  // Re-segment personal-names, numbers ...etc
  const bool is_conversion =
      (segments->request_type() == Segments::CONVERSION);
  if (is_conversion) {
    Resegment(history_key, conversion_key, segments, lattice);
  }

  return true;
}

bool ImmutableConverterImpl::MakeLatticeNodesForHistorySegments(
    Lattice *lattice, Segments *segments) const {
  const bool is_reverse =
     (segments->request_type() == Segments::REVERSE_CONVERSION);
  const size_t history_segments_size = segments->history_segments_size();
  const string &key = lattice->key();

  size_t segments_pos = 0;
  uint16 last_rid = 0;

  for (size_t s = 0; s < history_segments_size; ++s) {
    const Segment &segment = segments->segment(s);
    if (segment.segment_type() != Segment::HISTORY &&
        segment.segment_type() != Segment::SUBMITTED) {
      LOG(WARNING) << "inconsistent history";
      return false;
    }
    if (segment.key().empty()) {
      LOG(WARNING) << "invalid history: key is empty";
      return false;
    }
    const Segment::Candidate &candidate = segment.candidate(0);

    // Add a virtual nodes corresponding to HISTORY segments.
    Node *rnode = lattice->NewNode();
    CHECK(rnode);
    rnode->lid = candidate.lid;
    rnode->rid = candidate.rid;
    rnode->wcost = 0;
    rnode->value = candidate.value;
    rnode->key = segment.key();
    rnode->node_type = Node::HIS_NODE;
    rnode->bnext = NULL;
    lattice->Insert(segments_pos, rnode);

    // For the last history segment,  we also insert a new node having
    // EOS part-of-speech. Viterbi algorithm will find the
    // best path from rnode(context) and rnode2(EOS).
    if (s + 1 == history_segments_size && candidate.rid != 0) {
      Node *rnode2 = lattice->NewNode();
      CHECK(rnode2);
      rnode2->lid = candidate.lid;
      rnode2->rid = 0;   // 0 is BOS/EOS

      // This cost was originally set to 1500.
      // It turned out this penalty was so strong that it caused some
      // undesirable conversions like "の-なまえ" -> "の-な前" etc., so we
      // changed this to 0.
      // Reducing the cost promotes context-unaware conversions, and this may
      // have some unexpected side effects.
      // TODO(team): Figure out a better way to set the cost using
      // boundary.def-like approach.
      rnode2->wcost = 0;
      rnode2->value = candidate.value;
      rnode2->key = segment.key();
      rnode2->node_type = Node::HIS_NODE;
      rnode2->bnext = NULL;
      lattice->Insert(segments_pos, rnode2);
    }

    // Dictionary lookup for the candidates which are
    // overlapping between history and conversion.
    // Check only the last history segment at this moment.
    //
    // Example: history "おいかわ(及川)", conversion: "たくや"
    // Here, try to find "おいかわたくや(及川卓也)" from dictionary
    // and insert "卓也" as a new word node with a modified cost
    if (s + 1 == history_segments_size) {
      const bool is_prediction =
          (segments->request_type() == Segments::SUGGESTION ||
           segments->request_type() == Segments::PREDICTION);
      const Node *node = Lookup(segments_pos, key.size(),
                                is_reverse,
                                is_prediction,
                                lattice);
      for (const Node *compound_node = node; compound_node != NULL;
           compound_node = compound_node->bnext) {
        // No overlapps
        if (compound_node->key.size() <= rnode->key.size() ||
            compound_node->value.size() <= rnode->value.size() ||
            !Util::StartsWith(compound_node->key, rnode->key) ||
            !Util::StartsWith(compound_node->value, rnode->value)) {
          // not a prefix
          continue;
        }

        // Must be in the same POS group.
        // http://b/issue?id=2977618
        if (pos_group_->GetPosGroup(candidate.lid)
            != pos_group_->GetPosGroup(compound_node->lid)) {
          continue;
        }

        // make new virtual node
        Node *new_node = lattice->NewNode();
        CHECK(new_node);

        // get the suffix part ("たくや/卓也")
        new_node->key =
            compound_node->key.substr(rnode->key.size(),
                                      compound_node->key.size() -
                                      rnode->key.size());

        new_node->value =
            compound_node->value.substr(rnode->value.size(),
                                        compound_node->value.size() -
                                        rnode->value.size());

        // rid/lid are derived from the compound.
        // lid is just an approximation
        new_node->rid = compound_node->rid;
        new_node->lid = compound_node->lid;
        new_node->bnext = NULL;
        new_node->node_type = Node::NOR_NODE;

        // New cost recalcuration:
        //
        // compound_node->wcost * (candidate len / compound_node len)
        // - trans(candidate.rid, new_node.lid)
        new_node->wcost =
            compound_node->wcost *
            candidate.value.size() / compound_node->value.size()
            - connector_->GetTransitionCost(candidate.rid, new_node->lid);

        VLOG(2) << " compound_node->lid=" << compound_node->lid
                << " compound_node->rid=" << compound_node->rid
                << " compound_node->wcost=" << compound_node->wcost;
        VLOG(2) << " last_rid=" << last_rid
                << " candidate.lid=" << candidate.lid
                << " candidate.rid=" << candidate.rid
                << " candidate.cost=" << candidate.cost
                << " candidate.wcost=" << candidate.wcost;
        VLOG(2) << " new_node->wcost=" << new_node->wcost;

        new_node->constrained_prev = rnode;

        // Added as new node
        lattice->Insert(segments_pos + rnode->key.size(), new_node);

        VLOG(2) << "Added: " << new_node->key << " " << new_node->value;
      }
    }

    // update segment pos
    segments_pos += rnode->key.size();
    last_rid = rnode->rid;
  }
  return true;
}

void ImmutableConverterImpl::MakeLatticeNodesForConversionSegments(
    const string &history_key,
    Segments *segments, Lattice *lattice) const {
  const string &key = lattice->key();
  const bool is_conversion =
      (segments->request_type() == Segments::CONVERSION);
  // Do not use KeyCorrector if user changes the boundary.
  // http://b/issue?id=2804996
  scoped_ptr<KeyCorrector> key_corrector;
  if (is_conversion && !segments->resized()) {
    KeyCorrector::InputMode mode = KeyCorrector::ROMAN;
    if (GET_CONFIG(preedit_method) != config::Config::ROMAN) {
      mode = KeyCorrector::KANA;
    }
    key_corrector.reset(new KeyCorrector(key, mode, history_key.size()));
  }

  const bool is_reverse =
      (segments->request_type() == Segments::REVERSE_CONVERSION);
  const bool is_prediction =
      (segments->request_type() == Segments::SUGGESTION ||
       segments->request_type() == Segments::PREDICTION);
  for (size_t pos = history_key.size(); pos < key.size(); ++pos) {
    if (lattice->end_nodes(pos) != NULL) {
      Node *rnode = Lookup(pos, key.size(), is_reverse, is_prediction, lattice);
      // If history key is NOT empty and user input seems to starts with
      // a particle ("はにで..."), mark the node as STARTS_WITH_PARTICLE.
      // We change the segment boundary if STARTS_WITH_PARTICLE attribute
      // is assigned.
      if (!history_key.empty() && pos == history_key.size()) {
        for (Node *node = rnode; node != NULL; node = node->bnext) {
          if (pos_matcher_->IsAcceptableParticleAtBeginOfSegment(node->lid) &&
              node->lid == node->rid) {  // not a compound.
            node->attributes |= Node::STARTS_WITH_PARTICLE;
          }
        }
      }
      CHECK(rnode != NULL);
      lattice->Insert(pos, rnode);
      InsertCorrectedNodes(pos, key,
                           key_corrector.get(),
                           dictionary_, lattice);
    }
  }
}

void ImmutableConverterImpl::ApplyPrefixSuffixPenalty(
    const string &conversion_key,
    Lattice *lattice) const {
  const string &key = lattice->key();
  DCHECK_LE(conversion_key.size(), key.size());
  for (Node *node = lattice->begin_nodes(key.size() -
                                         conversion_key.size());
       node != NULL; node = node->bnext) {
    // TODO(taku):
    // We might be able to tweak the penalty according to
    // the size of history segments.
    // If history-segments is non-empty, we can make the
    // penalty smaller so that history context is more likely
    // selected.
    node->wcost += segmenter_->GetPrefixPenalty(node->lid);
  }

  for (Node *node = lattice->end_nodes(key.size());
       node != NULL; node = node->enext) {
    node->wcost += segmenter_->GetSuffixPenalty(node->rid);
  }
}

void ImmutableConverterImpl::Resegment(
    const string &history_key, const string &conversion_key,
    Segments *segments, Lattice *lattice) const {
  for (size_t pos = history_key.size();
       pos < history_key.size() + conversion_key.size(); ++pos) {
    ApplyResegmentRules(pos, lattice);
  }

  // Enable constrained node.
  size_t segments_pos = 0;
  for (size_t s = 0; s < segments->segments_size(); ++s) {
    const Segment &segment = segments->segment(s);
    if (segment.segment_type() == Segment::FIXED_VALUE) {
      const Segment::Candidate &candidate = segment.candidate(0);
      Node *rnode = lattice->NewNode();
      CHECK(rnode);
      rnode->lid       = candidate.lid;
      rnode->rid       = candidate.rid;
      rnode->wcost     = kMinCost;
      rnode->value     = candidate.value;
      rnode->key       = segment.key();
      rnode->node_type = Node::CON_NODE;
      rnode->bnext     = NULL;
      lattice->Insert(segments_pos, rnode);
    }
    segments_pos += segment.key().size();
  }
}

bool ImmutableConverterImpl::MakeSegments(Segments *segments,
                                          const Lattice &lattice,
                                          const vector<uint16> &group) const {
  if (segments == NULL) {
    LOG(WARNING) << "Segments is NULL";
    return false;
  }

  // skip HIS_NODE(s)
  Node *prev = lattice.bos_nodes();
  for (Node *node = lattice.bos_nodes()->next;
       node->next != NULL && node->node_type == Node::HIS_NODE;
       node = node->next) {
    prev = node;
  }

  size_t max_candidates_size = 0;
  bool is_prediction = false;
  switch (segments->request_type()) {
    case Segments::PREDICTION:
    case Segments::SUGGESTION:
    case Segments::PARTIAL_PREDICTION:
    case Segments::PARTIAL_SUGGESTION:
      max_candidates_size = segments->max_prediction_candidates_size();
      is_prediction = true;
      break;
    case Segments::CONVERSION:
      max_candidates_size = segments->max_conversion_candidates_size();
      break;
    case Segments::REVERSE_CONVERSION:
      // Currently, we assume that REVERSE_CONVERSION only
      // requires 1 result.
      // TODO(taku): support to set the size on REVESER_CONVERSION mode.
      max_candidates_size = 1;
      break;
    default:
      break;
  }

  const size_t expand_size =
      max(static_cast<size_t>(1),
          min(static_cast<size_t>(512), max_candidates_size));

  const size_t history_segments_size = segments->history_segments_size();
  const size_t old_segments_size = segments->segments_size();

  string key;
  for (Node *node = prev->next; node->next != NULL; node = node->next) {
    key.append(node->key);
    const Segment &old_segment = segments->segment(group[node->begin_pos]);
    // Condition 1: prev->next is NOT a boundary. Very strong constraint
    if (node->next->node_type != Node::EOS_NODE &&
        old_segment.segment_type() == Segment::FIXED_BOUNDARY &&
        group[node->begin_pos] == group[node->next->begin_pos]) {
      // do nothing
      // Condition 2: prev->next is a boundary. Very strong constraint
    } else if (node->node_type == Node::CON_NODE ||
               (node->next->node_type != Node::EOS_NODE &&
                group[node->begin_pos] != group[node->next->begin_pos]) ||
               segmenter_->IsBoundary(node, node->next, is_prediction)) {
      scoped_ptr<NBestGenerator> nbest(new NBestGenerator(
          suppression_dictionary_, segmenter_, connector_, pos_matcher_,
          prev, node->next, &lattice, is_prediction));
      CHECK(nbest.get());
      Segment *segment = is_prediction ?
          segments->mutable_segment(segments->segments_size() - 1) :
          segments->add_segment();
      CHECK(segment);
      if (!is_prediction) {
        segment->clear_candidates();
        segment->set_key(key);
      }
      ExpandCandidates(nbest.get(), segment, segments->request_type(),
                       expand_size);
      InsertDummyCandidates(segment, expand_size);
      if (node->node_type == Node::CON_NODE) {
        segment->set_segment_type(Segment::FIXED_VALUE);
      } else {
        segment->set_segment_type(Segment::FREE);
      }
      key.clear();
      prev = node;
    }
    // otherwise, not a boundary
  }

  // erase old segments
  if (!is_prediction &&
      old_segments_size - history_segments_size > 0) {
    segments->erase_segments(history_segments_size,
                             old_segments_size -
                             history_segments_size);
  }

  return true;
}

bool ImmutableConverterImpl::Convert(Segments *segments) const {
  const bool is_prediction =
      (segments->request_type() == Segments::PREDICTION ||
       segments->request_type() == Segments::SUGGESTION);

  Lattice *lattice = GetLattice(segments, is_prediction);

  if (!MakeLattice(lattice, segments)) {
    LOG(WARNING) << "could not make lattice";
    return false;
  }

  vector<uint16> group;
  MakeGroup(segments, &group);

  if (is_prediction) {
    if (!PredictionViterbi(segments, *lattice)) {
      LOG(WARNING) << "prediction_viterbi failed";
      return false;
    }
  } else {
    if (!Viterbi(segments, *lattice, group)) {
      LOG(WARNING) << "viterbi failed";
      return false;
    }
  }

  VLOG(2) << lattice->DebugString();

  if (!MakeSegments(segments, *lattice, group)) {
    LOG(WARNING) << "make segments failed";
    return false;
  }

  return true;
}

namespace {
ImmutableConverterInterface *g_immutable_converter = NULL;

}  // namespace

ImmutableConverterInterface *
ImmutableConverterFactory::GetImmutableConverter() {
  if (g_immutable_converter == NULL) {
    return Singleton<ImmutableConverterImpl>::get();
  } else {
    return g_immutable_converter;
  }
}

void ImmutableConverterFactory::SetImmutableConverter(
    ImmutableConverterInterface *immutable_converter) {
  g_immutable_converter = immutable_converter;
}
}  // namespace mozc
