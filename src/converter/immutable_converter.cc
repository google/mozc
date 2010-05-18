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

#include "converter/immutable_converter_interface.h"

#include <algorithm>
#include <climits>
#include <string>
#include <utility>
#include <vector>
#include "base/base.h"
#include "base/config_file_stream.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "base/util.h"
#include "converter/key_corrector.h"
#include "converter/segments.h"
#include "converter/converter_data.h"
#include "converter/connector.h"
#include "converter/nbest_generator.h"
#include "converter/pos_matcher.h"
#include "converter/segmenter.h"
#include "session/config_handler.h"
#include "session/config.pb.h"
#include "dictionary/dictionary_interface.h"

namespace mozc {
namespace {

#include "converter/embedded_connection_data.h"

const size_t kMaxSegmentsSize   = 256;
const size_t kMaxCharLength     = 1024;
const int    kMaxCost           = 32767;
const int    kDefaultNumberCost = 3000;
const int    kEOSPenalty        = 700;

enum {
  CONNECTED,
  WEAK_CONNECTED,
  NOT_CONNECTED,
};

Node *InitBOSNode(ConverterData *data, uint16 length) {
  Node *bos_node = data->NewNode();
  bos_node->rid = 0;
  bos_node->lid = 0;
  bos_node->key.clear();
  bos_node->value = "BOS";
  bos_node->node_type = Node::BOS_NODE;
  bos_node->wcost = 0;
  bos_node->cost = 0;
  bos_node->begin_pos = length;
  bos_node->end_pos = length;
  return bos_node;
}

// For EOS node, we use both pure EOS node
// and "サ変名詞". Since many users still inputs via single-segment-conversion,
// the right word of user input is not always end of sentence.
// If you see the side effect of this treatment,
// set some penalty to node->wcost.
Node *InitEOSNode(ConverterData *data, uint16 length) {
  Node *eos_node = data->NewNode();
  eos_node->rid = 0;   // pure EOS
  eos_node->lid = 0;
  eos_node->key.clear();
  eos_node->value = "EOS";
  eos_node->node_type = Node::EOS_NODE;
  eos_node->wcost = 0;
  eos_node->cost = 0;
  eos_node->begin_pos = length;
  eos_node->end_pos = length;

  Node *eos_noun_node = data->NewNode();
  // "サ変名詞"
  // POSMatcher::GetUnknownId(), POSMatcher::GetUnknownId()
  // returns IDs for "サ変名詞"
  eos_noun_node->rid = POSMatcher::GetUnknownId();
  eos_noun_node->lid = POSMatcher::GetUnknownId();
  eos_noun_node->key.clear();
  eos_noun_node->value = "EOS";
  eos_noun_node->node_type = Node::EOS_NODE;
  eos_noun_node->wcost = kEOSPenalty;   // add some a constant as penalty
  eos_noun_node->cost = 0;
  eos_noun_node->begin_pos = length;
  eos_noun_node->end_pos = length;

  // chain nodes
  eos_node->bnext = eos_noun_node;

  return eos_node;
}

void InsertNodes(size_t pos, Node *node, ConverterData *data) {
  Node **begin_nodes_list = data->begin_nodes_list();
  Node **end_nodes_list = data->end_nodes_list();

  for (Node *rnode = node; rnode != NULL; rnode = rnode->bnext) {
    rnode->begin_pos = static_cast<uint16>(pos);
    rnode->end_pos = static_cast<uint16>(pos + rnode->key.size());
    rnode->prev = NULL;
    rnode->next = NULL;
    rnode->cost = 0;
    const size_t x = rnode->key.size() + pos;
    rnode->enext = end_nodes_list[x];
    end_nodes_list[x] = rnode;
  }

  if (begin_nodes_list[pos] == NULL) {
    begin_nodes_list[pos] = node;
  } else {
    for (Node *rnode = node; rnode != NULL; rnode = rnode->bnext) {
      if (rnode->bnext == NULL) {
        rnode->bnext = begin_nodes_list[pos];
        begin_nodes_list[pos] = node;
        break;
      }
    }
  }
}

// TODO(taku): move it to KeyCorrector
int GetCorrectedCostPenalty(const string &key) {
  // "んん" and "っっ" must be mis-spelling.
  // if (key.find("んん") != string::npos ||
  //     key.find("っっ") != string::npos) {
  if (key.find("\xE3\x82\x93\xE3\x82\x93") != string::npos ||
      key.find("\xE3\x81\xA3\xE3\x81\xA3") != string::npos) {
    return 0;
  }
  // add 3000 to the original word cost
  const int kCorrectedCostPenalty = 3000;
  return kCorrectedCostPenalty;
}

void InsertCorrectedNodes(size_t pos, const string &key,
                          const KeyCorrector &key_corrector,
                          DictionaryInterface *dictionary,
                          ConverterData *data) {
  size_t length = 0;
  const char *str = key_corrector.GetCorrectedPrefix(pos, &length);
  if (str == NULL || length == 0) {
    return;
  }

  Node *r_node = dictionary->LookupPrefix(str, length,
                                          data->node_allocator());
  Node *prev = NULL;
  for (Node *node = r_node; node != NULL; node = node->bnext) {
    const size_t offset =
        key_corrector.GetOriginalOffset(pos, node->key.size());
    if (KeyCorrector::IsValidPosition(offset) && offset > 0) {
      // rewrite key
      node->key = key.substr(pos, offset);
      node->wcost += GetCorrectedCostPenalty(node->key);
      prev = node;
    } else {
      if (prev == NULL) {
        r_node = node;    // drop the first node
      } else {
        prev->bnext = node->bnext;    // change the chain
      }
    }
  }

  if (r_node != NULL) {
    InsertNodes(pos, r_node, data);
  }
}

int GetConnectionType(const Node *lnode,
                      const Node *rnode,
                      const vector<uint16> &group,
                      const Segments *segments) {
  // UNU (unusued) nodes
  if (lnode->cost == INT_MAX ||
      lnode->node_type == Node::UNU_NODE ||
      rnode->node_type == Node::UNU_NODE ||
      (rnode->node_type != Node::EOS_NODE &&
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

  const bool is_rule_boundary = Segmenter::IsBoundary(lnode, rnode);
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

void DecomposeNumber(const string &input,
                     string *number, string *suffix) {
  const char *begin = input.data();
  const char *end = input.data() + input.size();
  size_t pos = 0;
  while (begin < end) {
    if (*begin >= '0' && *begin <= '9') {
      ++pos;
      ++begin;
      continue;
    }
    break;
  }
  *number = input.substr(0, pos);
  *suffix = input.substr(pos, input.size() - pos);
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
    segment->set_key(key);

    // Ad-hoc rewrite for Numbers
    // Since number candidate is generative, i.e, any number can be
    // written by users, we normalize the value here. normalzied number
    // is used for the ranking tweaking based on history
    if (key.size() > 1 &&
        key == c->value &&
        key == c->content_value &&
        key == c->content_key &&
        Util::GetScriptType(key) == Util::NUMBER &&
        key[key.size() - 1] >= '0' && key[key.size() - 1] <= '9') {
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
  Node *Lookup(const char *begin,
               const char *end,
               ConverterData *data) const;

  void Resegment(size_t pos, ConverterData *data) const;

  // return true resegmentation happened
  bool ResegmentArabicNumberAndSuffix(size_t pos, ConverterData *data) const;
  bool ResegmentPersonalName(size_t pos, ConverterData *data) const;

  bool MakeLattice(Segments *segments) const;
  bool ModifyLattice(Segments *segments) const;

  bool Viterbi(Segments *segments,
               const vector<uint16> &group) const;

  bool MakeSegments(Segments *segments,
                    const vector<uint16> &group) const;

  inline int GetCost(const Node *lnode, const Node *rnode) const {
    const int kInvalidPenaltyCost = 100000;
    if (rnode->constrained_prev != NULL && lnode != rnode->constrained_prev) {
      return kInvalidPenaltyCost;
    }
    return connector_->GetTransitionCost(lnode->rid, rnode->lid) + rnode->wcost;
  }

  scoped_ptr<ConnectorInterface> connector_;
  DictionaryInterface *dictionary_;

  int32 last_to_first_name_transition_cost_;
  DISALLOW_COPY_AND_ASSIGN(ImmutableConverterImpl);
};

ImmutableConverterImpl::ImmutableConverterImpl()
    : connector_(
        ConnectorInterface::OpenFromArray(
            kConnectionData_data, kConnectionData_size)),
      dictionary_(DictionaryFactory::GetDictionary()),
      last_to_first_name_transition_cost_(0) {
  CHECK(connector_.get());
  last_to_first_name_transition_cost_
      = connector_->GetTransitionCost(
          POSMatcher::GetLastNameId(), POSMatcher::GetFirstNameId());
}

void ImmutableConverterImpl::Resegment(size_t pos, ConverterData *data) const {
  if (ResegmentArabicNumberAndSuffix(pos, data)) {
    VLOG(1) << "ResegmentArabicNumberAndSuffix returned true";
    return;
  }

  if (ResegmentPersonalName(pos, data)) {
    VLOG(1) << "ResegmentPersonalName returned true";
    return;
  }
}

// Currently, only arabic_number + suffix patterns are resegmented
// TODO(taku): consider kanji number into consideration
bool ImmutableConverterImpl::ResegmentArabicNumberAndSuffix(
    size_t pos, ConverterData *data) const {
  Node **begin_nodes_list = data->begin_nodes_list();
  const Node *bnode = begin_nodes_list[pos];
  if (bnode == NULL) {
    VLOG(1) << "bnode is NULL";
    return false;
  }

  bool modified = false;

  for (const Node *compound_node = bnode;
       compound_node != NULL; compound_node = compound_node->bnext) {
    if (bnode->value.size() > 0 &&
        bnode->value[0] >= '0' && bnode->value[0] <= '9' &&
        bnode->key[0] >= '0' && bnode->key[0] <= '9' &&
        POSMatcher::IsNumber(compound_node->lid) &&
        !POSMatcher::IsNumber(compound_node->rid)) {
      string number_value, number_key;
      string suffix_value, suffix_key;
      DecomposeNumber(compound_node->value, &number_value, &suffix_value);
      DecomposeNumber(compound_node->key,   &number_key, &suffix_key);

      if (suffix_value.empty() || suffix_key.empty()) {
        continue;
      }

      // not compatibile
      if (number_value != number_key) {
        LOG(WARNING) << "Incompatible key/value number pair";
        continue;
      }

      const int32 wcost = compound_node->wcost / 2;

      Node *number_node = data->NewNode();
      CHECK(number_node);
      number_node->key = number_key;
      number_node->value = number_value;
      number_node->lid = compound_node->lid;
      number_node->rid = 0;   // 0 to 0 transition cost is 0
      number_node->wcost = wcost;
      number_node->node_type = Node::NOR_NODE;
      number_node->bnext = NULL;

      // insert number into the lattice
      InsertNodes(pos, number_node, data);

      Node *suffix_node = data->NewNode();
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
      InsertNodes(pos + number_node->key.size(), suffix_node, data);
      VLOG(1) << "Resegmented: " << compound_node->value << " "
              << number_node->value << " " << suffix_node->value;

      modified = true;
    }
  }

  return modified;
}

bool ImmutableConverterImpl::ResegmentPersonalName(
    size_t pos, ConverterData *data) const {
  Node **begin_nodes_list = data->begin_nodes_list();
  const Node *bnode = begin_nodes_list[pos];
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
          compound_node->value.find(lnode->value) == 0) {
        // rnode(first_name) is a suffix of compound, Constraint 1.
        for (const Node *rnode = begin_nodes_list[pos + lnode->key.size()];
             rnode != NULL; rnode = rnode->bnext) {
          if ((lnode->value.size() + rnode->value.size())
              == compound_node->value.size() &&
              (lnode->value + rnode->value) == compound_node->value &&
              Segmenter::IsBoundary(lnode, rnode)) {   // Constraint 3.
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

    Node *last_name_node = data->NewNode();
    CHECK(last_name_node);
    last_name_node->key = best_last_name_node->key;
    last_name_node->value = best_last_name_node->value;
    last_name_node->lid = compound_node->lid;
    last_name_node->rid = POSMatcher::GetLastNameId();
    last_name_node->wcost = wcost;
    last_name_node->node_type = Node::NOR_NODE;
    last_name_node->bnext = NULL;

    // insert last_name into the lattice
    InsertNodes(pos, last_name_node, data);

    Node *first_name_node = data->NewNode();
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
    InsertNodes(pos + last_name_node->key.size(), first_name_node, data);

    VLOG(2) << "Resegmented: " << compound_node->value << " "
            << last_name_node->value << " " << first_name_node->value;

    modified = true;
  }

  return modified;
}

Node *ImmutableConverterImpl::Lookup(const char *begin,
                                     const char *end,
                                     ConverterData *data) const {
  data->node_allocator()->set_max_nodes_size(8192);
  Node *result_node =
      dictionary_->LookupPrefix(begin,
                                static_cast<int>(end - begin),
                                data->node_allocator());

  size_t mblen = 0;
  const uint16 w = Util::UTF8ToUCS2(begin, end, &mblen);

  const Util::ScriptType first_script_type = Util::GetScriptType(w);
  const Util::FormType first_form_type = Util::GetFormType(w);

  Node *new_node = data->NewNode();
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
  new_node->node_type = Node::UNK_NODE;
  new_node->bnext = result_node;
  result_node = new_node;

  if (first_script_type == Util::NUMBER) {
    result_node->wcost = kDefaultNumberCost;
    return result_node;
  }

  if (first_script_type != Util::ALPHABET &&
      first_script_type != Util::KATAKANA) {
    return result_node;
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
    Node *new_node = data->NewNode();
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
    new_node->node_type = Node::UNK_NODE;
    new_node->bnext = result_node;
    result_node = new_node;
  }

  return result_node;
}

bool ImmutableConverterImpl::Viterbi(Segments *segments,
                                     const vector<uint16> &group) const {
  ConverterData *data = segments->converter_data();
  Node **begin_nodes_list = data->begin_nodes_list();
  Node **end_nodes_list = data->end_nodes_list();

  const string &key = segments->converter_data()->key();

  for (size_t pos = 0; pos <= key.size(); ++pos) {
    for (Node *rnode = begin_nodes_list[pos];
         rnode != NULL; rnode = rnode->bnext) {
      int bestCost = INT_MAX;
      Node* bestNode = NULL;
      for (Node *lnode = end_nodes_list[pos];
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
            cost = lnode->cost + (GetCost(lnode, rnode) * 2);
            rnode->is_weak_connected = true;
            break;
          case NOT_CONNECTED:
            cost = INT_MAX - 1;
            break;
          default:
            break;
        }

        if (cost < bestCost) {
          bestNode = lnode;
          bestCost = cost;
        }
      }
      rnode->prev = bestNode;
      rnode->cost = bestCost;
    }
  }

  // we may have multiple EOS nodes
  {
    Node *eos_node = segments->eos_node();
    for (Node *node = segments->eos_node();
         node != NULL; node = node->bnext) {
      // find the best eos_node having the smallest cost
      if (node->cost < eos_node->cost) {
        eos_node = node;
      }
    }
    // overwrite eos_node
    data->set_eos_node(eos_node);
  }

  Node *node = segments->eos_node();
  Node *prev = NULL;

  while (node->prev != NULL) {
    prev = node->prev;
    prev->next = node;
    node = prev;
  }

  if (segments->bos_node() != prev) {
    LOG(WARNING) << "cannot make lattice";
    return false;
  }

  return true;
}

bool ImmutableConverterImpl::MakeLattice(Segments *segments) const {
  // TOOD(taku) Code refactoring.
  // It is not an optimal solution to calling MakeLattice
  // when after ResizeSegment lattice is already made.
  NormalizeHistorySegments(segments);

  if (segments->has_lattice() && !segments->has_resized()) {
    string key;
    for (size_t i = 0; i < segments->segments_size(); ++i) {
      key += segments->segment(i).key();
    }
    if (key != segments->converter_data()->key()) {
      LOG(WARNING) << "inconsistent input key";
      return false;
    }
    return true;
  }

  if (segments->segments_size() >= kMaxSegmentsSize) {
    LOG(WARNING) << "too many segments";
    return false;
  }

  string history_key, conversion_key;
  const size_t history_segments_size = segments->history_segments_size();
  for (size_t i = 0; i < history_segments_size; ++i) {
    history_key += segments->segment(i).key();
  }

  for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
    conversion_key += segments->segment(i).key();
  }

  if (history_key.size() + conversion_key.size() >= kMaxCharLength) {
    LOG(WARNING) << "too long input chars";
    return false;
  }

  if (conversion_key.empty()) {
    LOG(WARNING) << "key is empty";
    return false;
  }

  const string key = history_key + conversion_key;

  ConverterData *data = segments->converter_data();
  KeyCorrector::InputMode mode = KeyCorrector::ROMAN;
  if (GET_CONFIG(preedit_method) != config::Config::ROMAN) {
    mode = KeyCorrector::KANA;
  }
  data->set_key(key, mode);

  Node *bosNode = InitBOSNode(data, 0);
  Node *eosNode = InitEOSNode(data, static_cast<uint16>(key.size()));

  data->set_bos_node(bosNode);
  data->set_eos_node(eosNode);

  Node **end_nodes_list = data->end_nodes_list();
  Node **begin_nodes_list = data->begin_nodes_list();
  end_nodes_list[0] = bosNode;
  begin_nodes_list[key.size()] = eosNode;

  size_t segments_pos = 0;
  const char *key_end = key.data() + key.size();
  const char *key_begin = key.data();

  uint16 last_rid = 0;
  for (size_t s = 0; s < history_segments_size; ++s) {
    const Segment &seg = segments->segment(s);
    if (seg.segment_type() != Segment::HISTORY &&
        seg.segment_type() != Segment::SUBMITTED) {
      LOG(WARNING) << "inconsistent history";
      return false;
    }
    const Segment::Candidate &c = seg.candidate(0);

    // basically, we add a new node as an
    // empty (BOS/EOS) node
    Node *rnode = data->NewNode();
    rnode->lid = 0;
    rnode->rid = 0;
    rnode->wcost = 0;
    rnode->value = c.value;
    rnode->key = seg.key();
    rnode->node_type = Node::HIS_NODE;
    rnode->bnext = NULL;
    InsertNodes(segments_pos, rnode, data);

    // For the last history segment,  we also insert a new node having
    // a rid as a contextual information. Viterbi algorithm will find the
    // best path from rnode(BOS) and rnode2(context).
    // It is almost true that user input unit is equivalent to mozc segment.
    // So we add a penalty constant so that BOS node is prefered.
    // We changed it from 2000 to 500 after bigram.
    const int kContextNodePenalty = 500;
    if (s + 1 == history_segments_size) {
      Node *rnode2 = data->NewNode();
      rnode2->lid = 0;
      rnode2->rid = c.rid;
      rnode2->wcost = kContextNodePenalty;
      rnode2->value = c.value;
      rnode2->key = seg.key();
      rnode2->node_type = Node::HIS_NODE;
      rnode2->bnext = NULL;
      InsertNodes(segments_pos, rnode2, data);
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
                                data);
      for (const Node *compound_node = node; compound_node != NULL;
           compound_node = compound_node->bnext) {
        // No overlapps
        if (compound_node->key.size() <= rnode->key.size() ||
            compound_node->value.size() <= rnode->value.size() ||
            compound_node->key.find(rnode->key) != 0 ||
            compound_node->value.find(rnode->value) != 0) {   // not a prefix
          continue;
        }

        // make new virtual node
        Node *new_node = data->NewNode();
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
        // trans(last_rid, rnode->lid) + rnode->wcost +
        // trans(rnode->rid, new_node->lid) + new_node->wcost ==
        // trans(last_rid, compound_node->lid) + compound_node->wcost
        //
        // i.e.
        // new_node->wcost =
        // trans(last_rid, compound_node->lid) + compound_node->wcost
        //  - { trans(last_rid, new_node->lid) + rnode->wcost +
        //      trans(rnode->rid, new_node->lid) }
        //
        // Also,
        // c.cost =   trans(last_rid, rnode->lid)
        //          + rnode->wcost
        //          + trans(rnode->rid, EOS_lid(0))
        // i.e,
        // trans(last_rid, rnode->lid) + rnode->wcost ==
        //  c.cost - trans(rnode->rid, EOS_lid(0))
        new_node->wcost =
            connector_->GetTransitionCost(last_rid, compound_node->lid)
            + compound_node->wcost
            - c.cost
            + connector_->GetTransitionCost(rnode->rid, 0)
            - connector_->GetTransitionCost(rnode->rid, compound_node->lid);

        new_node->constrained_prev = rnode;

        // Added as new node
        InsertNodes(segments_pos + rnode->key.size(), new_node, data);

        VLOG(2) << "Added: " << new_node->key << " " << new_node->value;
      }
    }

    // update segment pos
    segments_pos += rnode->key.size();
    last_rid = rnode->rid;
  }

  if (end_nodes_list[history_key.size()] == NULL) {
    LOG(WARNING) << "cannot build lattice from input";
    return false;
  }

  // Dictionary Lookup for conversion segment
  const KeyCorrector &key_corrector = data->key_corrector();

  for (size_t pos = history_key.size(); pos < key.size(); ++pos) {
    if (end_nodes_list[pos] != NULL) {
      Node *rnode = Lookup(key_begin + pos, key_end, data);
      CHECK(rnode != NULL);
      InsertNodes(pos, rnode, data);
      // Insert corrected nodes like みんあ -> みんな
      InsertCorrectedNodes(pos, key,
                           key_corrector,
                           dictionary_, data);
    }
  }

  if (end_nodes_list[key.size()] == NULL) {
    LOG(WARNING) << "cannot build lattice from input";
    return false;
  }

  // resegments
  for (size_t pos = history_key.size(); pos < key.size(); ++pos) {
    Resegment(pos, data);
  }

  return true;
}

bool ImmutableConverterImpl::ModifyLattice(Segments *segments) const {
  ConverterData *data = segments->converter_data();
  Node **begin_nodes_list = data->begin_nodes_list();
  const string &key = data->key();

  // disable all CON_NODE
  for (size_t pos = 0; pos <= key.size(); ++pos) {
    for (Node *node = begin_nodes_list[pos];
         node != NULL; node = node->bnext) {
      node->cost = 0;  // reset cost
      if (node->node_type == Node::CON_NODE) {
        node->node_type = Node::UNU_NODE;
      }
    }
  }

  // enable CON_NODE
  size_t segments_pos = 0;
  for (size_t s = 0; s < segments->segments_size(); ++s) {
    const Segment &seg = segments->segment(s);
    if (seg.segment_type() == Segment::FIXED_VALUE) {
      const Segment::Candidate &c = seg.candidate(0);
      Node *rnode = data->NewNode();
      rnode->lid       = c.lid;
      rnode->rid       = c.rid;
      rnode->wcost     = -kMaxCost;
      rnode->value     = c.value;
      rnode->key       = seg.key();
      rnode->node_type = Node::CON_NODE;
      rnode->con_segment = &seg;
      rnode->bnext     = NULL;
      InsertNodes(segments_pos, rnode, data);
    }
    segments_pos += seg.key().size();
  }

  return true;
}

bool ImmutableConverterImpl::MakeSegments(Segments *segments,
                                          const vector<uint16> &group) const {
  // skip HIS_NODE(s)
  Node *prev = segments->bos_node();
  for (Node *node = segments->bos_node()->next;
       node->next != NULL && node->node_type == Node::HIS_NODE;
       node = node->next) {
    prev = node;
  }

  string key;
  const size_t history_segments_size = segments->history_segments_size();
  const size_t old_segments_size = segments->segments_size();

  for (Node *node = prev->next; node->next != NULL; node = node->next) {
    key += node->key;
    const Segment &old_seg = segments->segment(group[node->begin_pos]);
    // Condition 1: prev->next is NOT a boundary. Very strong constraint
    if (node->next->node_type != Node::EOS_NODE &&
        old_seg.segment_type() == Segment::FIXED_BOUNDARY &&
        group[node->begin_pos] == group[node->next->begin_pos]) {
       // do nothing
      // Condition 2: prev->next is a boundary. Very strong constraint
    } else if (node->node_type == Node::CON_NODE ||
               (node->next->node_type != Node::EOS_NODE &&
                group[node->begin_pos] != group[node->next->begin_pos]) ||
               Segmenter::IsBoundary(node, node->next)) {
      Segment *segment = segments->add_segment();
      NBestGenerator *nbest = segment->nbest_generator();
      nbest->Init(prev, node->next, connector_.get(),
                  segments->converter_data());
      segment->set_key(key);
      segment->Expand(max(static_cast<size_t>(1), old_seg.candidates_size()));
      if (segment->candidates_size() == 0) {
        LOG(WARNING) << "Segment::Expand() returns 0 result";
        {
          Segment::Candidate *c = segment->push_back_candidate();
          c->Init();
          c->value = key;  // hiragana
          c->content_value = key;
          c->content_key = key;
        }
        {
          Segment::Candidate *c = segment->push_back_candidate();
          c->Init();
          Util::HiraganaToKatakana(key, &c->value);  // katakana
          c->content_value = c->value;
          c->content_key = key;
        }
      }
      if (node->node_type == Node::CON_NODE &&
          node->con_segment != NULL) {
        segment->set_segment_type(Segment::FIXED_VALUE);
        Segment::Candidate *c = segment->mutable_candidate(0);
        *c = node->con_segment->candidate(0);
      } else {
        segment->set_segment_type(Segment::FREE);
      }
      key.clear();
      prev = node;
    }
    // otherwise, not a boundary
  }

  // erase old segments
  segments->erase_segments(history_segments_size,
                           old_segments_size - history_segments_size);

  return true;
}

bool ImmutableConverterImpl::Convert(Segments *segments) const {
  if (segments == NULL) {
    LOG(WARNING) << "segment is NULL";
    return false;
  }

  if (!MakeLattice(segments)) {
    LOG(WARNING) << "could not make lattice";
    return false;
  }

  if (!ModifyLattice(segments)) {
    LOG(WARNING) << "could not modify lattice";
    return false;
  }

  vector<uint16> group;
  MakeGroup(segments, &group);

  if (!Viterbi(segments, group)) {
    LOG(WARNING) << "viterbi failed";
    return false;
  }

  if (!MakeSegments(segments, group)) {
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
