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

#include "converter/immutable_converter_interface.h"

#include <algorithm>
#include <climits>
#include <string>
#include <utility>
#include <vector>

#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/singleton.h"
#include "base/util.h"
#include "converter/connector_interface.h"
#include "converter/key_corrector.h"
#include "converter/lattice.h"
#include "converter/nbest_generator.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "session/config.pb.h"
#include "session/config_handler.h"

namespace mozc {
namespace {

const size_t kMaxSegmentsSize                   = 256;
const size_t kMaxCharLength                     = 1024;
const size_t kMaxCharLengthForReverseConversion = 600;  // 200 chars in UTF8
const int    kMaxCost                           = 32767;
const int    kMinCost                           = -32767;
const int    kDefaultNumberCost                 = 3000;

enum {
  CONNECTED,
  WEAK_CONNECTED,
  NOT_CONNECTED,
};

// TODO(taku): this is a tentative fix.
// including the header file both in rewriter and converter are redudant.
// we'd like to have a common module.
#include "rewriter/user_segment_history_rewriter_rule.h"

// return the POS group of the given candidate
uint16 GetPosGroup(uint16 lid) {
  return kLidGroup[lid];
}

void ExpandCandidates(NBestGenerator *nbest, Segment *segment,
                      Segments::RequestType request_type, size_t expand_size) {
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
    new_candidate->cost = last_candidate->cost;
    new_candidate->wcost = last_candidate->wcost;
    new_candidate->structure_cost = last_candidate->structure_cost;
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
    new_candidate->attributes = 0;
    last_candidate = new_candidate;
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
    new_candidate->cost = last_candidate->cost;
    new_candidate->wcost = last_candidate->wcost;
    new_candidate->structure_cost =
            last_candidate->structure_cost;
    new_candidate->lid = last_candidate->lid;
    new_candidate->rid = last_candidate->rid;
  }

  DCHECK_GT(segment->candidates_size(), 0);
}

void InsertCorrectedNodes(size_t pos, const string &key,
                          const KeyCorrector *key_corrector,
                          DictionaryInterface *dictionary,
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

int GetConnectionType(const Node *lnode,
                      const Node *rnode,
                      const vector<uint16> &group,
                      const Segments *segments) {
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
       segments->request_type() == Segments::SUGGESTION);

  const bool is_rule_boundary = Segmenter::IsBoundary(lnode, rnode,
                                                      is_prediction);
  const bool is_constraint_boundary =
      group[lnode->begin_pos] != group[rnode->begin_pos];

  if (is_constraint_boundary && !is_rule_boundary) {
    return WEAK_CONNECTED;
  }

  return CONNECTED;
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

class ImmutableConverterImpl: public ImmutableConverterInterface {
 public:
  virtual bool Convert(Segments *segments) const;
  ImmutableConverterImpl();
  virtual ~ImmutableConverterImpl() {}

 private:
  Node *Lookup(const char *begin, const char *end,
               bool is_reverse,
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
  // Fix for "好む" vs "この|無", "大|代" vs "代々" preferences.
  // If the last node ends with "prefix", give an extra
  // wcost penalty. In this case  "無" doesn't tend to appear at
  // user input.
  void ApplyPrefixSuffixPenalty(const string &conversion_key,
                                Lattice *lattice) const;

  bool Viterbi(Segments *segments,
               const Lattice &lattice,
               const vector<uint16> &group) const;

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

  int32 last_to_first_name_transition_cost_;
  DISALLOW_COPY_AND_ASSIGN(ImmutableConverterImpl);
};

ImmutableConverterImpl::ImmutableConverterImpl()
    : connector_(ConnectorFactory::GetConnector()),
      dictionary_(DictionaryFactory::GetDictionary()),
      last_to_first_name_transition_cost_(0) {
  last_to_first_name_transition_cost_
      = connector_->GetTransitionCost(
          POSMatcher::GetLastNameId(), POSMatcher::GetFirstNameId());
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
        POSMatcher::IsNumber(compound_node->lid) &&
        !POSMatcher::IsNumber(compound_node->rid) &&
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
    if (compound_node->lid != POSMatcher::GetLastNameId() ||
        compound_node->rid != POSMatcher::GetFirstNameId()) {
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
              Segmenter::IsBoundary(lnode, rnode, false)) {   // Constraint 3.
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
        (best_last_name_node->lid != POSMatcher::GetLastNameId() &&
         best_first_name_node->rid != POSMatcher::GetFirstNameId())) {
      continue;
    }

    // Constraint 4.b
    if (len == 3 &&
        (best_last_name_node->lid != POSMatcher::GetLastNameId() ||
         best_first_name_node->rid != POSMatcher::GetFirstNameId())) {
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
    last_name_node->rid = POSMatcher::GetLastNameId();
    last_name_node->wcost = wcost;
    last_name_node->node_type = Node::NOR_NODE;
    last_name_node->bnext = NULL;

    // insert last_name into the lattice
    lattice->Insert(pos, last_name_node);

    Node *first_name_node = lattice->NewNode();
    CHECK(first_name_node);
    first_name_node->key = best_first_name_node->key;
    first_name_node->value = best_first_name_node->value;
    first_name_node->lid = POSMatcher::GetFirstNameId();
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

Node *ImmutableConverterImpl::Lookup(const char *begin,
                                     const char *end,
                                     bool is_reverse,
                                     Lattice *lattice) const {
  lattice->node_allocator()->set_max_nodes_size(8192);
  Node *result_node = NULL;
  if (is_reverse) {
    result_node =
        dictionary_->LookupReverse(begin,
                                   static_cast<int>(end - begin),
                                   lattice->node_allocator());
  } else {
    result_node =
        dictionary_->LookupPrefix(begin,
                                  static_cast<int>(end - begin),
                                  lattice->node_allocator());
  }
  return AddCharacterTypeBasedNodes(begin, end, lattice, result_node);
}

Node *ImmutableConverterImpl::AddCharacterTypeBasedNodes(
    const char *begin, const char *end, Lattice *lattice, Node *nodes) const {

  size_t mblen = 0;
  const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);

  const Util::ScriptType first_script_type = Util::GetScriptType(w);
  const Util::FormType first_form_type = Util::GetFormType(w);

  // Add 1 character node. It can be either UnknownId or NumberId.
  Node *new_node = lattice->NewNode();
  CHECK(new_node);
  if (first_script_type == Util::NUMBER) {
    new_node->lid = POSMatcher::GetNumberId();
    new_node->rid = POSMatcher::GetNumberId();
  } else {
    new_node->lid = POSMatcher::GetUnknownId();
    new_node->rid = POSMatcher::GetUnknownId();
  }

  new_node->wcost = kMaxCost;
  new_node->value.assign(begin, mblen);
  new_node->key.assign(begin, mblen);
  new_node->node_type = Node::NOR_NODE;
  new_node->bnext = nodes;
  nodes = new_node;

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
    const uint16 w = Util::UTF8ToUCS2(p, end, &mblen);
    if (first_script_type != Util::GetScriptType(w) ||
        first_form_type != Util::GetFormType(w)) {
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
      new_node->lid = POSMatcher::GetNumberId();
      new_node->rid = POSMatcher::GetNumberId();
    } else {
      new_node->lid = POSMatcher::GetUnknownId();
      new_node->rid = POSMatcher::GetUnknownId();
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

  for (size_t pos = 0; pos <= key.size(); ++pos) {
    for (Node *rnode = lattice.begin_nodes(pos);
         rnode != NULL; rnode = rnode->bnext) {
      int best_cost = INT_MAX;
      Node* best_node = NULL;
      for (Node *lnode = lattice.end_nodes(pos);
           lnode != NULL; lnode = lnode->enext) {
        int cost = 0;
        switch (GetConnectionType(lnode, rnode, group, segments)) {
          case CONNECTED:
            cost = lnode->cost + GetCost(lnode, rnode);
            break;
          case WEAK_CONNECTED:
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
          case NOT_CONNECTED:
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
    history_key.append(segments->segment(i).key());
  }

  for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
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
  lattice->SetKey(key);

  if (is_reverse) {
    // Reverse lookup for each prefix string in key is slow with current
    // implementation, so run it for them at once and cache the result.
    dictionary_->PopulateReverseLookupCache(key.c_str(), key.size(),
                                            lattice->node_allocator());
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
    dictionary_->ClearReverseLookupCache(lattice->node_allocator());
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
  const char *key_end = key.data() + key.size();
  const char *key_begin = key.data();

  size_t segments_pos = 0;
  uint16 last_rid = 0;

  for (size_t s = 0; s < history_segments_size; ++s) {
    const Segment &segment = segments->segment(s);
    if (segment.segment_type() != Segment::HISTORY &&
        segment.segment_type() != Segment::SUBMITTED) {
      LOG(WARNING) << "inconsistent history";
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
      rnode2->wcost = 1500;  // =~ -500 * log(1/20)
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
      const Node *node = Lookup(key_begin + segments_pos, key_end,
                                is_reverse,
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
        if (GetPosGroup(candidate.lid) != GetPosGroup(compound_node->lid)) {
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
        // trans(last_rid, candidate.lid) + candidate.wcost
        // + trans(candidate.rid, new_node->lid)
        // + new_node->wcost ==
        // trans(last_rid, compound_node->lid) + compound_node->wcost
        //
        // i.e.
        // new_node->wcost =
        // trans(last_rid, compound_node->lid) + compound_node->wcost
        //   - trans(last_rid, candidate.lid) - candidate.wcost -
        // trans(candidate.rid, new_node->lid)
        new_node->wcost =
            connector_->GetTransitionCost(last_rid, compound_node->lid)
            + compound_node->wcost
            - connector_->GetTransitionCost(last_rid, candidate.lid)
            - candidate.wcost
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
    key_corrector.reset(new KeyCorrector(key, mode));
  }

  const bool is_reverse =
      (segments->request_type() == Segments::REVERSE_CONVERSION);
  const char *key_end = key.data() + key.size();
  const char *key_begin = key.data();
  for (size_t pos = history_key.size(); pos < key.size(); ++pos) {
    if (lattice->end_nodes(pos) != NULL) {
      Node *rnode = Lookup(key_begin + pos, key_end, is_reverse, lattice);
      // If history key is NOT empty and user input seems to starts with
      // a particle ("はにで..."), mark the node as STARTS_WITH_PARTICLE.
      // We change the segment boundary if STARTS_WITH_PARTICLE attribute
      // is assigned.
      if (!history_key.empty() && pos == history_key.size()) {
        for (Node *node = rnode; node != NULL; node = node->bnext) {
          if (POSMatcher::IsAcceptableParticleAtBeginOfSegment(node->lid) &&
              node->lid == node->rid) {  // not a compound.
            node->attributes |= Node::STARTS_WITH_PARTICLE;
          }
        }
      }
      CHECK(rnode != NULL);
      lattice->Insert(pos, rnode);
      // Insert corrected nodes like みんあ -> みんな
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
    node->wcost += Segmenter::GetPrefixPenalty(node->lid);
  }

  for (Node *node = lattice->end_nodes(key.size());
       node != NULL; node = node->enext) {
    node->wcost += Segmenter::GetSuffixPenalty(node->rid);
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
               Segmenter::IsBoundary(node, node->next, is_prediction)) {
      Segment *segment = is_prediction ?
          segments->mutable_segment(segments->segments_size() - 1) :
          segments->add_segment();
      scoped_ptr<NBestGenerator> nbest(new NBestGenerator);
      CHECK(segment);
      CHECK(nbest.get());
      if (!is_prediction) {
        segment->clear_candidates();
      }
      nbest->Init(prev, node->next, &lattice,
                  is_prediction);
      segment->set_key(key);
      ExpandCandidates(nbest.get(), segment, segments->request_type(),
                       expand_size);
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
  Lattice lattice;
  if (!MakeLattice(&lattice, segments)) {
    LOG(WARNING) << "could not make lattice";
    return false;
  }

  vector<uint16> group;
  MakeGroup(segments, &group);

  if (!Viterbi(segments, lattice, group)) {
    LOG(WARNING) << "viterbi failed";
    return false;
  }

  if (!MakeSegments(segments, lattice, group)) {
    LOG(WARNING) << "make segments failed";
    return false;
  }

  return true;
}

ImmutableConverterInterface *g_immutable_converter = NULL;
}   // anonymous namespace

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
