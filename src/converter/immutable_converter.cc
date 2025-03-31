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

#include "converter/immutable_converter.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/trie.h"
#include "base/japanese_util.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "base/vlog.h"
#include "converter/connector.h"
#include "converter/key_corrector.h"
#include "converter/lattice.h"
#include "converter/nbest_generator.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "converter/node_list_builder.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"

namespace mozc {
namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::PosMatcher;
using ::mozc::dictionary::Token;

constexpr size_t kMaxSegmentsSize = 256;
constexpr size_t kMaxCharLength = 1024;
constexpr size_t kMaxCharLengthForReverseConversion = 600;  // 200 chars in UTF8
constexpr int kMaxCost = 32767;
constexpr int kMinCost = -32767;
constexpr int kDefaultNumberCost = 3000;

bool IsMobileRequest(const ConversionRequest &request) {
  return request.request().mixed_conversion();
}

class KeyCorrectedNodeListBuilder : public BaseNodeListBuilder {
 public:
  KeyCorrectedNodeListBuilder(size_t pos, absl::string_view original_lookup_key,
                              const KeyCorrector *key_corrector,
                              NodeAllocator *allocator)
      : BaseNodeListBuilder(allocator, allocator->max_nodes_size()),
        pos_(pos),
        original_lookup_key_(original_lookup_key),
        key_corrector_(key_corrector),
        tail_(nullptr) {}

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token &token) override {
    const size_t offset =
        key_corrector_->GetOriginalOffset(pos_, token.key.size());
    if (!KeyCorrector::IsValidPosition(offset) || offset == 0) {
      return TRAVERSE_NEXT_KEY;
    }
    Node *node = NewNodeFromToken(token);
    node->key.assign(original_lookup_key_.data() + pos_, offset);
    node->wcost += KeyCorrector::GetCorrectedCostPenalty(node->key);

    // Push back |node| to the end.
    if (result_ == nullptr) {
      result_ = node;
    } else {
      DCHECK(tail_ != nullptr);
      tail_->bnext = node;
    }
    tail_ = node;
    return TRAVERSE_CONTINUE;
  }

  Node *tail() const { return tail_; }

 private:
  const size_t pos_;
  const absl::string_view original_lookup_key_;
  const KeyCorrector *key_corrector_;
  Node *tail_;
};

void InsertCorrectedNodes(size_t pos, absl::string_view key,
                          const ConversionRequest &request,
                          const KeyCorrector *key_corrector,  // nullable
                          const DictionaryInterface &dictionary,
                          Lattice *lattice) {
  if (key_corrector == nullptr) {
    return;
  }
  size_t length = 0;
  const char *str = key_corrector->GetCorrectedPrefix(pos, &length);
  if (str == nullptr || length == 0) {
    return;
  }
  KeyCorrectedNodeListBuilder builder(pos, key, key_corrector,
                                      lattice->node_allocator());
  dictionary.LookupPrefix(absl::string_view(str, length), request, &builder);
  if (builder.tail() != nullptr) {
    builder.tail()->bnext = nullptr;
  }
  if (builder.result() != nullptr) {
    lattice->Insert(pos, builder.result());
  }
}

bool IsNumber(const char c) { return c >= '0' && c <= '9'; }

bool ContainsWhiteSpacesOnly(const absl::string_view s) {
  for (ConstChar32Iterator iter(s); !iter.Done(); iter.Next()) {
    switch (iter.Get()) {
      case 0x09:    // TAB
      case 0x20:    // Half-width space
      case 0x3000:  // Full-width space
        break;
      default:
        return false;
    }
  }
  return true;
}

// <number, suffix>
std::pair<absl::string_view, absl::string_view> DecomposeNumberAndSuffix(
    absl::string_view input) {
  const char *begin = input.data();
  const char *end = input.data() + input.size();
  size_t pos = 0;
  while (begin < end) {
    if (IsNumber(*begin)) {
      ++pos;
      ++begin;
    }
    break;
  }
  return std::make_pair(input.substr(0, pos),
                        input.substr(pos, input.size() - pos));
}

// <prefix, number>
std::pair<absl::string_view, absl::string_view> DecomposePrefixAndNumber(
    absl::string_view input) {
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
  return std::make_pair(input.substr(0, pos),
                        input.substr(pos, input.size() - pos));
}

void NormalizeHistorySegments(Segments *segments) {
  for (Segment &segment : segments->history_segments()) {
    if (segment.candidates_size() == 0) {
      continue;
    }

    Segment::Candidate *c = segment.mutable_candidate(0);
    absl::string_view history_key =
        (c->key.size() > segment.key().size()) ? c->key : segment.key();
    const std::string value = c->value;
    const std::string content_value = c->content_value;
    const std::string content_key = c->content_key;
    std::string key =
        japanese_util::FullWidthAsciiToHalfWidthAscii(history_key);
    c->value = japanese_util::FullWidthAsciiToHalfWidthAscii(value);
    c->content_value =
        japanese_util::FullWidthAsciiToHalfWidthAscii(content_value);
    c->content_key = japanese_util::FullWidthAsciiToHalfWidthAscii(content_key);
    c->key = key;
    segment.set_key(key);

    // Ad-hoc rewrite for Numbers
    // Since number candidate is generative, i.e., any number can be
    // written by users, we normalize the value here. normalzied number
    // is used for the ranking tweaking based on history
    if (key.size() > 1 && key == c->value && key == c->content_value &&
        key == c->key && key == c->content_key &&
        Util::GetScriptType(key) == Util::NUMBER &&
        IsNumber(key[key.size() - 1])) {
      key = key[key.size() - 1];  // use the last digit only
      segment.set_key(key);
      c->value = key;
      c->content_value = key;
      c->content_key = key;
    }
  }
}

Lattice *GetLattice(Segments *segments, bool is_prediction) {
  Lattice *lattice = segments->mutable_cached_lattice();
  if (lattice == nullptr) {
    return nullptr;
  }

  std::string history_key = "";
  for (const Segment &segment : segments->history_segments()) {
    history_key.append(segment.key());
  }
  std::string conversion_key = "";
  for (const Segment &segment : segments->conversion_segments()) {
    conversion_key.append(segment.key());
  }

  const size_t lattice_history_end_pos = lattice->history_end_pos();

  if (!is_prediction || Util::CharsLen(conversion_key) <= 1 ||
      lattice_history_end_pos != history_key.size()) {
    // Do not cache if conversion is not prediction.  In addition, if a user
    // input the key right after the finish of conversion, reset the lattice to
    // erase old nodes.  Even if the lattice key is not changed, we should reset
    // the lattice when the history size is changed.  When we submit the
    // candidate partially, the entire key will not changed, but the history
    // position will be changed.
    lattice->Clear();
  }

  return lattice;
}

// Returns the vector of the inner segment's key.
std::vector<absl::string_view> GetBoundaryInfo(const Segment::Candidate &c) {
  std::vector<absl::string_view> ret;
  for (Segment::Candidate::InnerSegmentIterator iter(&c); !iter.Done();
       iter.Next()) {
    ret.emplace_back(iter.GetKey());
  }
  return ret;
}

// Input is a list of FIRST_INNER_SEGMENT candidate, which is the first segment
// of each n-best path.
// Basic idea is that we can filter a candidate when we already added the
// candidate with the same key and there is a cost gap
//
// Example:
//   segment key: "わたしのなまえは"
//   candidates: {{"わたしの", "私の"}, {"わたしの", "渡しの"}, {"わたし",
//   "渡し"}}
// Here,"渡しの" will be filtered if there is a cost gap from "私の"
class FirstInnerSegmentCandidateChecker {
 public:
  explicit FirstInnerSegmentCandidateChecker(const Segment &target_segment,
                                             int cost_max_diff)
      : target_segment_(target_segment), cost_max_diff_(cost_max_diff) {}

  bool IsGoodCandidate(const Segment::Candidate &c) {
    if (c.key.size() != target_segment_.key().size() &&
        IsPrefixAdded(c.value)) {
      // Filter a candidate if its prefix is already added unless it is the
      // single segment candidate.
      return false;
    }

    if (min_cost_.has_value() && c.cost - *min_cost_ > cost_max_diff_) {
      return false;
    }

    return true;
  }

  void AddEntry(const Segment::Candidate &c) {
    constexpr bool kPlaceholderValue = true;
    trie_.AddEntry(c.value, kPlaceholderValue);

    if (Util::ContainsScriptType(c.value, Util::KANJI)) {
      // Do not use non-kanji entry's cost. Sometimes it is too small.
      if (!min_cost_.has_value()) {
        min_cost_ = c.cost;
      } else {
        *min_cost_ = std::min(*min_cost_, c.cost);
      }
    }
  }

 private:
  // Returns true if the prefix of value has already added
  bool IsPrefixAdded(absl::string_view value) const {
    bool data;
    size_t key_length;
    return trie_.LongestMatch(value, &data, &key_length);
  }

  const Segment &target_segment_;
  int cost_max_diff_;
  std::optional<int> min_cost_;
  Trie<bool> trie_;
};

}  // namespace

ImmutableConverter::ImmutableConverter(const engine::Modules &modules)
    : dictionary_(modules.GetDictionary()),
      suffix_dictionary_(modules.GetSuffixDictionary()),
      user_dictionary_(modules.GetUserDictionary()),
      connector_(modules.GetConnector()),
      segmenter_(modules.GetSegmenter()),
      pos_matcher_(modules.GetPosMatcher()),
      pos_group_(modules.GetPosGroup()),
      suggestion_filter_(modules.GetSuggestionFilter()),
      first_name_id_(pos_matcher_.GetFirstNameId()),
      last_name_id_(pos_matcher_.GetLastNameId()),
      number_id_(pos_matcher_.GetNumberId()),
      unknown_id_(pos_matcher_.GetUnknownId()),
      last_to_first_name_transition_cost_(
          connector_.GetTransitionCost(last_name_id_, first_name_id_)) {}

void ImmutableConverter::InsertDummyCandidates(Segment *segment,
                                               size_t expand_size) const {
  const Segment::Candidate *top_candidate =
      segment->candidates_size() == 0 ? nullptr : segment->mutable_candidate(0);
  const Segment::Candidate *last_candidate =
      segment->candidates_size() == 0
          ? nullptr
          : segment->mutable_candidate(segment->candidates_size() - 1);

  // Insert a dummy candidate whose content_value is katakana.
  // If functional_key() is empty, no need to make a dummy candidate.
  if (segment->candidates_size() > 0 &&
      segment->candidates_size() < expand_size &&
      !segment->candidate(0).functional_key().empty() &&
      Util::GetScriptType(segment->candidate(0).content_key) ==
          Util::HIRAGANA) {
    // Use last_candidate as a reference of cost.
    // Use top_candidate as a refarence of lid/rid and key/value.
    DCHECK(top_candidate);
    DCHECK(last_candidate);
    Segment::Candidate *new_candidate = segment->add_candidate();
    DCHECK(new_candidate);

    *new_candidate = *top_candidate;
    new_candidate->content_value =
        japanese_util::HiraganaToKatakana(segment->candidate(0).content_key);
    new_candidate->value.clear();
    absl::StrAppend(&new_candidate->value, new_candidate->content_value,
                    top_candidate->functional_value());
    new_candidate->cost = last_candidate->cost + 1;
    new_candidate->wcost = last_candidate->wcost + 1;
    new_candidate->structure_cost = last_candidate->structure_cost + 1;
    new_candidate->attributes = 0;
    // We cannot copy inner_segment_boundary; see b/8109381.
    new_candidate->inner_segment_boundary.clear();
    DCHECK(new_candidate->IsValid());
    last_candidate = new_candidate;
  }

  // Insert a dummy hiragana candidate.
  if (segment->candidates_size() == 0 ||
      (segment->candidates_size() < expand_size &&
       Util::GetScriptType(segment->key()) == Util::HIRAGANA)) {
    Segment::Candidate *new_candidate = segment->add_candidate();
    DCHECK(new_candidate);

    if (last_candidate != nullptr) {
      *new_candidate = *last_candidate;
      // We cannot copy inner_segment_boundary; see b/8109381.
      new_candidate->inner_segment_boundary.clear();
    }
    new_candidate->key = segment->key();
    new_candidate->value = segment->key();
    new_candidate->content_key = segment->key();
    new_candidate->content_value = segment->key();
    if (last_candidate != nullptr) {
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
    DCHECK(new_candidate->IsValid());
  }

  // Insert a dummy katakana candidate.
  std::string katakana_value =
      japanese_util::HiraganaToKatakana(segment->key());
  if (segment->candidates_size() > 0 &&
      segment->candidates_size() < expand_size &&
      Util::GetScriptType(katakana_value) == Util::KATAKANA) {
    Segment::Candidate *new_candidate = segment->add_candidate();
    DCHECK(new_candidate);
    DCHECK(last_candidate);
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
    DCHECK(new_candidate->IsValid());
  }

  DCHECK_GT(segment->candidates_size(), 0);
}

void ImmutableConverter::ApplyResegmentRules(size_t pos,
                                             Lattice *lattice) const {
  if (ResegmentArabicNumberAndSuffix(pos, lattice)) {
    MOZC_VLOG(1) << "ResegmentArabicNumberAndSuffix returned true";
    return;
  }

  if (ResegmentPrefixAndArabicNumber(pos, lattice)) {
    MOZC_VLOG(1) << "ResegmentArabicNumberAndSuffix returned true";
    return;
  }

  if (ResegmentPersonalName(pos, lattice)) {
    MOZC_VLOG(1) << "ResegmentPersonalName returned true";
    return;
  }
}

// Currently, only arabic_number + suffix patterns are resegmented
// TODO(taku): consider kanji number into consideration
bool ImmutableConverter::ResegmentArabicNumberAndSuffix(
    size_t pos, Lattice *lattice) const {
  const Node *bnode = lattice->begin_nodes(pos);
  if (bnode == nullptr) {
    MOZC_VLOG(1) << "bnode is nullptr";
    return false;
  }

  bool modified = false;

  for (const Node *compound_node = bnode; compound_node != nullptr;
       compound_node = compound_node->bnext) {
    if (!compound_node->value.empty() && !compound_node->key.empty() &&
        pos_matcher_.IsNumber(compound_node->lid) &&
        !pos_matcher_.IsNumber(compound_node->rid) &&
        IsNumber(compound_node->value[0]) && IsNumber(compound_node->key[0])) {
      auto [number_value, suffix_value] =
          DecomposeNumberAndSuffix(compound_node->value);
      auto [number_key, suffix_key] =
          DecomposeNumberAndSuffix(compound_node->key);

      if (suffix_value.empty() || suffix_key.empty()) {
        continue;
      }

      // not compatible
      if (number_value != number_key) {
        LOG(WARNING) << "Incompatible key/value number pair";
        continue;
      }

      // do -1 so that resegmented nodes are boosted
      // over compound node.
      const int32_t wcost = std::max(compound_node->wcost / 2 - 1, 0);

      Node *number_node = lattice->NewNode();
      CHECK(number_node);
      number_node->key = number_key;
      number_node->value = number_value;
      number_node->lid = compound_node->lid;
      number_node->rid = 0;  // 0 to 0 transition cost is 0
      number_node->wcost = wcost;
      number_node->node_type = Node::NOR_NODE;
      number_node->bnext = nullptr;

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
      suffix_node->bnext = nullptr;

      suffix_node->constrained_prev = number_node;

      // insert suffix into the lattice
      lattice->Insert(pos + number_node->key.size(), suffix_node);
      MOZC_VLOG(1) << "Resegmented: " << compound_node->value << " "
                   << number_node->value << " " << suffix_node->value;

      modified = true;
    }
  }

  return modified;
}

bool ImmutableConverter::ResegmentPrefixAndArabicNumber(
    size_t pos, Lattice *lattice) const {
  const Node *bnode = lattice->begin_nodes(pos);
  if (bnode == nullptr) {
    MOZC_VLOG(1) << "bnode is nullptr";
    return false;
  }

  bool modified = false;

  for (const Node *compound_node = bnode; compound_node != nullptr;
       compound_node = compound_node->bnext) {
    // Unlike ResegmentArabicNumberAndSuffix, we don't
    // check POS as words ending with Arabic numbers are pretty rare.
    if (compound_node->value.size() > 1 && compound_node->key.size() > 1 &&
        !IsNumber(compound_node->value[0]) &&
        !IsNumber(compound_node->key[0]) &&
        IsNumber(compound_node->value[compound_node->value.size() - 1]) &&
        IsNumber(compound_node->key[compound_node->key.size() - 1])) {
      auto [prefix_value, number_value] =
          DecomposePrefixAndNumber(compound_node->value);
      auto [prefix_key, number_key] =
          DecomposePrefixAndNumber(compound_node->key);

      if (prefix_value.empty() || prefix_key.empty()) {
        continue;
      }

      // not compatible
      if (number_value != number_key) {
        LOG(WARNING) << "Incompatible key/value number pair";
        continue;
      }

      // do -1 so that resegmented nodes are boosted
      // over compound node.
      const int32_t wcost = std::max(compound_node->wcost / 2 - 1, 0);

      Node *prefix_node = lattice->NewNode();
      CHECK(prefix_node);
      prefix_node->key = prefix_key;
      prefix_node->value = prefix_value;
      prefix_node->lid = compound_node->lid;
      prefix_node->rid = 0;  // 0 to 0 transition cost is 0
      prefix_node->wcost = wcost;
      prefix_node->node_type = Node::NOR_NODE;
      prefix_node->bnext = nullptr;

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
      number_node->bnext = nullptr;

      number_node->constrained_prev = prefix_node;

      // insert number into the lattice
      lattice->Insert(pos + prefix_node->key.size(), number_node);
      MOZC_VLOG(1) << "Resegmented: " << compound_node->value << " "
                   << prefix_node->value << " " << number_node->value;

      modified = true;
    }
  }

  return modified;
}

bool ImmutableConverter::ResegmentPersonalName(size_t pos,
                                               Lattice *lattice) const {
  const Node *bnode = lattice->begin_nodes(pos);
  if (bnode == nullptr) {
    MOZC_VLOG(1) << "bnode is nullptr";
    return false;
  }

  bool modified = false;

  // find a combination of last_name and first_name, e.g. "田中麗奈".
  for (const Node *compound_node = bnode; compound_node != nullptr;
       compound_node = compound_node->bnext) {
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
    const Node *best_last_name_node = nullptr;
    const Node *best_first_name_node = nullptr;
    int best_cost = 0x7FFFFFFF;
    for (const Node *lnode = bnode; lnode != nullptr; lnode = lnode->bnext) {
      // lnode(last_name) is a prefix of compound, Constraint 1.
      if (compound_node->value.size() > lnode->value.size() &&
          compound_node->key.size() > lnode->key.size() &&
          absl::StartsWith(compound_node->value, lnode->value)) {
        // rnode(first_name) is a suffix of compound, Constraint 1.
        for (const Node *rnode = lattice->begin_nodes(pos + lnode->key.size());
             rnode != nullptr; rnode = rnode->bnext) {
          if ((lnode->value.size() + rnode->value.size()) ==
                  compound_node->value.size() &&
              (lnode->value + rnode->value) == compound_node->value &&
              segmenter_.IsBoundary(*lnode, *rnode, false)) {  // Constraint 3.
            const int32_t cost = lnode->wcost + GetCost(lnode, rnode);
            if (cost < best_cost) {  // choose the smallest ones
              best_last_name_node = lnode;
              best_first_name_node = rnode;
              best_cost = cost;
            }
          }
        }
      }
    }

    // No valid first/last names are found
    if (best_first_name_node == nullptr || best_last_name_node == nullptr) {
      continue;
    }

    // Constraint 4.a
    if (len >= 4 && (best_last_name_node->lid != last_name_id_ &&
                     best_first_name_node->rid != first_name_id_)) {
      continue;
    }

    // Constraint 4.b
    if (len == 3 && (best_last_name_node->lid != last_name_id_ ||
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
    // i.e.,
    // last_name_cost = first_name_cost =
    // (compound_cost - transition_cost) / 2;
    const int32_t wcost =
        (compound_node->wcost - last_to_first_name_transition_cost_) / 2;

    Node *last_name_node = lattice->NewNode();
    CHECK(last_name_node);
    last_name_node->key = best_last_name_node->key;
    last_name_node->value = best_last_name_node->value;
    last_name_node->lid = compound_node->lid;
    last_name_node->rid = last_name_id_;
    last_name_node->wcost = wcost;
    last_name_node->node_type = Node::NOR_NODE;
    last_name_node->bnext = nullptr;

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
    first_name_node->bnext = nullptr;

    first_name_node->constrained_prev = last_name_node;

    // insert first_name into the lattice
    lattice->Insert(pos + last_name_node->key.size(), first_name_node);

    MOZC_VLOG(2) << "Resegmented: " << compound_node->value << " "
                 << last_name_node->value << " " << first_name_node->value;

    modified = true;
  }

  return modified;
}

namespace {

class NodeListBuilderWithCacheEnabled : public NodeListBuilderForLookupPrefix {
 public:
  NodeListBuilderWithCacheEnabled(NodeAllocator *allocator,
                                  size_t min_key_length)
      : NodeListBuilderForLookupPrefix(allocator, allocator->max_nodes_size(),
                                       min_key_length) {
    DCHECK(allocator);
  }

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token &token) override {
    Node *node = NewNodeFromToken(token);
    node->attributes |= Node::ENABLE_CACHE;
    node->raw_wcost = node->wcost;
    PrependNode(node);
    return (limit_ <= 0) ? TRAVERSE_DONE : TRAVERSE_CONTINUE;
  }
};

}  // namespace

Node *ImmutableConverter::Lookup(const int begin_pos,
                                 const ConversionRequest &request,
                                 bool is_reverse, bool is_prediction,
                                 Lattice *lattice) const {
  absl::string_view key = lattice->key();
  CHECK_LT(begin_pos, key.size());
  const absl::string_view key_substr = absl::string_view{key}.substr(begin_pos);

  lattice->node_allocator()->set_max_nodes_size(8192);
  Node *result_node = nullptr;
  if (is_reverse) {
    BaseNodeListBuilder builder(lattice->node_allocator(),
                                lattice->node_allocator()->max_nodes_size());
    dictionary_.LookupReverse(key_substr, request, &builder);
    result_node = builder.result();
  } else {
    if (is_prediction) {
      NodeListBuilderWithCacheEnabled builder(
          lattice->node_allocator(), lattice->cache_info(begin_pos) + 1);
      dictionary_.LookupPrefix(key_substr, request, &builder);
      result_node = builder.result();
      lattice->SetCacheInfo(begin_pos, key_substr.length());
    } else {
      // When cache feature is not used, look up normally
      BaseNodeListBuilder builder(lattice->node_allocator(),
                                  lattice->node_allocator()->max_nodes_size());
      dictionary_.LookupPrefix(key_substr, request, &builder);
      result_node = builder.result();
    }
  }
  return AddCharacterTypeBasedNodes(key_substr, lattice, result_node);
}

Node *ImmutableConverter::AddCharacterTypeBasedNodes(
    absl::string_view key_substr, Lattice *lattice, Node *nodes) const {
  const Utf8AsChars32 utf8_as_chars32(key_substr);
  Utf8AsChars32::const_iterator it = utf8_as_chars32.begin();
  CHECK(it != utf8_as_chars32.end());
  const char32_t codepoint = it.char32();

  const Util::ScriptType first_script_type = Util::GetScriptType(codepoint);
  const Util::FormType first_form_type = Util::GetFormType(codepoint);

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
    new_node->value.assign(it.view());
    new_node->key.assign(it.view());
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
  CHECK(it != utf8_as_chars32.end());
  for (++it; it != utf8_as_chars32.end(); ++it, ++num_char) {
    const char32_t next_codepoint = it.char32();
    if (first_script_type != Util::GetScriptType(next_codepoint) ||
        first_form_type != Util::GetFormType(next_codepoint)) {
      break;
    }
  }

  if (num_char > 1) {
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
    const absl::string_view key_substr_up_to_it =
        key_substr.substr(0, it.to_address() - key_substr.data());
    new_node->value.assign(key_substr_up_to_it);
    new_node->key.assign(key_substr_up_to_it);
    new_node->node_type = Node::NOR_NODE;
    new_node->bnext = nodes;
    nodes = new_node;
  }

  return nodes;
}

namespace {

// A wrapper for Connector to minimize calls of Connector::GetTransitionCost()
// in Viterbi algorithm. This way the performance of Viterbi algorithm improves
// significantly in terms of time consumption because
// Connector::GetTransitionCost() is slow due to its compression format. This
// class implements a cache strategy designed for the access pattern in Viterbi
// algorithm.
//
// In Viterbi algorithm, the connection matrix is looked up in a nested loop as
// follows:
//
// for each right node `r`:
//   for each left node `l`:
//     ...
//     transition cost = connector.GetTransitionCost(l.rid, r.lid)
//     ...
//
// Therefore, in the inner loop, `r.lid` is fixed. So we can simply use an array
// to cache the transition cost for (l.rid, r.lid) in `cache[l.rid]`, and cache
// is reset before the inner loop. Moreover, right nodes are likely to be
// ordered by `r.lid` although it's not guaranteed in general. This fact is plus
// for this caching strategy as we only need to reset the cache if `r.lid` is
// different from the previous value.
//
// NOTE: This class is designed only for Viterbi algorithm and won't work for
// other purposes.
class CachingConnector final {
 public:
  explicit CachingConnector(const Connector &connector)
      : connector_{connector} {}

  CachingConnector(const CachingConnector &) = delete;
  CachingConnector &operator=(const CachingConnector &) = delete;

  void ResetCacheIfNecessary(uint16_t rnode_lid) {
    if (cache_lid_ != rnode_lid) {
      absl::c_fill(cache_, -1);
      cache_lid_ = rnode_lid;
    }
  }

  int GetTransitionCost(uint16_t lnode_rid, uint16_t rnode_lid) {
    DCHECK_EQ(cache_lid_, rnode_lid);
    // Values for rid >= kCacheSize cannot be cached. However, frequent PoSs
    // have smaller IDs, so caching only for rid in [0, kCacheSize) works well.
    if (lnode_rid >= kCacheSize) {
      return connector_.GetTransitionCost(lnode_rid, rnode_lid);
    }
    if (cache_[lnode_rid] != -1) {
      return cache_[lnode_rid];
    }
    cache_[lnode_rid] = connector_.GetTransitionCost(lnode_rid, rnode_lid);
    return cache_[lnode_rid];
  }

 private:
  constexpr static int kCacheSize = 2048;

  const Connector &connector_;
  std::array<int, kCacheSize> cache_;
  uint16_t cache_lid_ = std::numeric_limits<uint16_t>::max();
};

// Reasonably big cost. Cannot use INT_MAX because a new cost will be
// calculated based on kVeryBigCost.
constexpr int kVeryBigCost = (INT_MAX >> 2);

// Runs viterbi algorithm at position |pos|. The left_boundary/right_boundary
// are the next boundary looked from pos. (If pos is on the boundary,
// left_boundary should be the previous one, and right_boundary should be
// the next).
inline void ViterbiInternal(const Connector &connector, size_t pos,
                            size_t right_boundary, Lattice *lattice) {
  CachingConnector conn(connector);
  for (Node *rnode = lattice->begin_nodes(pos); rnode != nullptr;
       rnode = rnode->bnext) {
    if (rnode->end_pos > right_boundary) {
      // Invalid rnode.
      rnode->prev = nullptr;
      continue;
    }

    conn.ResetCacheIfNecessary(rnode->lid);

    if (rnode->constrained_prev != nullptr) {
      // Constrained node.
      if (rnode->constrained_prev->prev == nullptr) {
        rnode->prev = nullptr;
      } else {
        rnode->prev = rnode->constrained_prev;
        rnode->cost = rnode->prev->cost + rnode->wcost +
                      conn.GetTransitionCost(rnode->prev->rid, rnode->lid);
      }
      continue;
    }

    // Find a valid node which connects to the rnode with minimum cost.
    int best_cost = kVeryBigCost;
    Node *best_node = nullptr;
    for (Node *lnode = lattice->end_nodes(pos); lnode != nullptr;
         lnode = lnode->enext) {
      if (lnode->prev == nullptr) {
        // Invalid lnode.
        continue;
      }

      int cost = lnode->cost + conn.GetTransitionCost(lnode->rid, rnode->lid);
      if (cost < best_cost) {
        best_cost = cost;
        best_node = lnode;
      }
    }

    rnode->prev = best_node;
    rnode->cost = best_cost + rnode->wcost;
  }
}
}  // namespace

bool ImmutableConverter::Viterbi(const Segments &segments,
                                 Lattice *lattice) const {
  absl::string_view key = lattice->key();

  // Process BOS.
  {
    Node *bos_node = lattice->bos_nodes();
    // Ensure only one bos node is available.
    DCHECK(bos_node != nullptr);
    DCHECK(bos_node->enext == nullptr);

    const size_t right_boundary = segments.segment(0).key().size();
    for (Node *rnode = lattice->begin_nodes(0); rnode != nullptr;
         rnode = rnode->bnext) {
      if (rnode->end_pos > right_boundary) {
        // Invalid rnode.
        continue;
      }

      // Ensure no constraint.
      DCHECK(rnode->constrained_prev == nullptr);

      rnode->prev = bos_node;
      rnode->cost = bos_node->cost +
                    connector_.GetTransitionCost(bos_node->rid, rnode->lid) +
                    rnode->wcost;
    }
  }

  size_t left_boundary = 0;

  // Specialization for the first segment.
  // Don't run on the left boundary (the connection with BOS node),
  // because it is already run above.
  {
    const size_t right_boundary =
        left_boundary + segments.segment(0).key().size();
    for (size_t pos = left_boundary + 1; pos < right_boundary; ++pos) {
      ViterbiInternal(connector_, pos, right_boundary, lattice);
    }
    left_boundary = right_boundary;
  }

  // The condition to break is in the loop.
  for (const Segment &segment : segments.all().drop(1)) {
    // Run Viterbi for each position the segment.
    const size_t right_boundary = left_boundary + segment.key().size();
    for (size_t pos = left_boundary; pos < right_boundary; ++pos) {
      ViterbiInternal(connector_, pos, right_boundary, lattice);
    }
    left_boundary = right_boundary;
  }

  // Process EOS.
  {
    Node *eos_node = lattice->eos_nodes();

    // Ensure only one eos node.
    DCHECK(eos_node != nullptr);
    DCHECK(eos_node->bnext == nullptr);

    // No constrained prev.
    DCHECK(eos_node->constrained_prev == nullptr);

    left_boundary = key.size() - segments.all().back().key().size();
    // Find a valid node which connects to the rnode with minimum cost.
    int best_cost = kVeryBigCost;
    Node *best_node = nullptr;
    for (Node *lnode = lattice->end_nodes(key.size()); lnode != nullptr;
         lnode = lnode->enext) {
      if (lnode->prev == nullptr) {
        // Invalid lnode.
        continue;
      }

      int cost =
          lnode->cost + connector_.GetTransitionCost(lnode->rid, eos_node->lid);
      if (cost < best_cost) {
        best_cost = cost;
        best_node = lnode;
      }
    }

    eos_node->prev = best_node;
    eos_node->cost = best_cost + eos_node->wcost;
  }

  // Traverse the node from end to begin.
  Node *node = lattice->eos_nodes();
  CHECK(node->bnext == nullptr);
  Node *prev = nullptr;
  while (node->prev != nullptr) {
    prev = node->prev;
    prev->next = node;
    node = prev;
  }

  if (lattice->bos_nodes() != prev) {
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

bool ImmutableConverter::PredictionViterbi(const Segments &segments,
                                           Lattice *lattice) const {
  const size_t key_length = lattice->key().size();
  size_t history_length = 0;
  for (const Segment &segment : segments.history_segments()) {
    history_length += segment.key().size();
  }
  PredictionViterbiInternal(0, history_length, lattice);
  PredictionViterbiInternal(history_length, key_length, lattice);

  Node *node = lattice->eos_nodes();
  CHECK(node->bnext == nullptr);
  Node *prev = nullptr;
  while (node->prev != nullptr) {
    prev = node->prev;
    prev->next = node;
    node = prev;
  }

  if (lattice->bos_nodes() != prev) {
    LOG(WARNING) << "cannot make lattice";
    return false;
  }

  return true;
}

namespace {

// Mapping from lnode's rid to (cost, Node) of best way/cost, and vice versa.
// Note that, the average number of lid/rid variation is less than 30 in
// most cases. So, in order to avoid too many allocations for internal
// nodes of std::map, we use vector of key-value pairs.
using CostAndNode = std::pair<int, Node *>;
using BestMap = std::vector<std::pair<int, CostAndNode>>;

BestMap::iterator LowerBound(BestMap &best_map,
                             const std::pair<int, CostAndNode> &key) {
  return std::lower_bound(
      best_map.begin(), best_map.end(), key,
      [](const std::pair<int, CostAndNode> &l,
         const std::pair<int, CostAndNode> &r) { return l.first < r.first; });
}

}  // namespace

void ImmutableConverter::PredictionViterbiInternal(int calc_begin_pos,
                                                   int calc_end_pos,
                                                   Lattice *lattice) const {
  CHECK_LE(calc_begin_pos, calc_end_pos);

  BestMap lbest, rbest;
  lbest.reserve(128);
  rbest.reserve(128);

  const CostAndNode kInvalidValue(INT_MAX, nullptr);

  for (size_t pos = calc_begin_pos; pos <= calc_end_pos; ++pos) {
    lbest.clear();
    for (Node *lnode = lattice->end_nodes(pos); lnode != nullptr;
         lnode = lnode->enext) {
      const int rid = lnode->rid;
      const BestMap::value_type key(rid, kInvalidValue);
      const BestMap::iterator iter = LowerBound(lbest, key);
      if (iter == lbest.end() || iter->first != rid) {
        lbest.insert(
            iter, BestMap::value_type(rid, std::make_pair(lnode->cost, lnode)));
      } else if (lnode->cost < iter->second.first) {
        iter->second.first = lnode->cost;
        iter->second.second = lnode;
      }
    }

    if (lbest.empty()) {
      continue;
    }

    rbest.clear();
    Node *rnode_begin = lattice->begin_nodes(pos);
    for (Node *rnode = rnode_begin; rnode != nullptr; rnode = rnode->bnext) {
      if (rnode->end_pos > calc_end_pos) {
        continue;
      }
      const BestMap::value_type key(rnode->lid, kInvalidValue);
      const BestMap::const_iterator iter = LowerBound(rbest, key);
      if (iter == rbest.end() || iter->first != rnode->lid) {
        rbest.insert(iter, key);
      }
    }

    if (rbest.empty()) {
      continue;
    }

    for (BestMap::iterator liter = lbest.begin(); liter != lbest.end();
         ++liter) {
      for (BestMap::iterator riter = rbest.begin(); riter != rbest.end();
           ++riter) {
        const int cost = liter->second.first + connector_.GetTransitionCost(
                                                   liter->first, riter->first);
        if (cost < riter->second.first) {
          riter->second.first = cost;
          riter->second.second = liter->second.second;
        }
      }
    }

    for (Node *rnode = rnode_begin; rnode != nullptr; rnode = rnode->bnext) {
      if (rnode->end_pos > calc_end_pos) {
        continue;
      }
      const BestMap::value_type key(rnode->lid, kInvalidValue);
      const BestMap::const_iterator iter = LowerBound(rbest, key);
      if (iter == rbest.end() || iter->first != rnode->lid ||
          iter->second.second == nullptr) {
        continue;
      }

      rnode->cost = iter->second.first + rnode->wcost;
      rnode->prev = iter->second.second;
    }
  }
}

namespace {

// Adds penalty for predictive nodes when building a node list.
class NodeListBuilderForPredictiveNodes : public BaseNodeListBuilder {
 public:
  NodeListBuilderForPredictiveNodes(NodeAllocator *allocator, int limit,
                                    const PosMatcher &pos_matcher)
      : BaseNodeListBuilder(allocator, limit), pos_matcher_(pos_matcher) {}

  ~NodeListBuilderForPredictiveNodes() override = default;

  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const Token &token) override {
    Node *node = NewNodeFromToken(token);
    constexpr int kPredictiveNodeDefaultPenalty = 900;  // ~= -500 * log(1/6)
    int additional_cost = kPredictiveNodeDefaultPenalty;

    // Bonus for suffix word.
    if (pos_matcher_.IsSuffixWord(node->rid) &&
        pos_matcher_.IsSuffixWord(node->lid)) {
      constexpr int kSuffixWordBonus = 700;
      additional_cost -= kSuffixWordBonus;
    }

    // Penalty for unique noun word.
    if (pos_matcher_.IsUniqueNoun(node->rid) ||
        pos_matcher_.IsUniqueNoun(node->lid)) {
      constexpr int kUniqueNounPenalty = 500;
      additional_cost += kUniqueNounPenalty;
    }

    // Penalty for number.
    if (pos_matcher_.IsNumber(node->rid) || pos_matcher_.IsNumber(node->lid)) {
      constexpr int kNumberPenalty = 4000;
      additional_cost += kNumberPenalty;
    }

    node->wcost += additional_cost;
    PrependNode(node);
    return (limit_ <= 0) ? TRAVERSE_DONE : TRAVERSE_CONTINUE;
  }

 private:
  const PosMatcher &pos_matcher_;
};

}  // namespace

// Add predictive nodes from conversion key.
void ImmutableConverter::MakeLatticeNodesForPredictiveNodes(
    const Segments &segments, const ConversionRequest &request,
    Lattice *lattice) const {
  absl::string_view key = lattice->key();
  std::string conversion_key;
  for (const Segment &segment : segments.conversion_segments()) {
    conversion_key += segment.key();
  }
  DCHECK_NE(std::string::npos, key.find(conversion_key));
  const std::vector<std::string> conversion_key_chars =
      Util::SplitStringToUtf8Chars(conversion_key);

  // *** Current behaviors ***
  // - Starts suggestion from 6 characters, which is conservative.
  // - Predictive nodes with zero-length prefix string are not generated.
  // do nothing if the conversion key is short
  constexpr size_t kKeyMinLength = 7;
  if (conversion_key_chars.size() < kKeyMinLength) {
    return;
  }

  // Predictive search from suffix dictionary.
  // (search words with between 1 and 6 characters)
  {
    constexpr size_t kMaxSuffixLookupKey = 6;
    const size_t max_suffix_len =
        std::min(kMaxSuffixLookupKey, conversion_key_chars.size());
    size_t pos = key.size();

    for (size_t suffix_len = 1; suffix_len <= max_suffix_len; ++suffix_len) {
      pos -=
          conversion_key_chars[conversion_key_chars.size() - suffix_len].size();
      DCHECK_GE(key.size(), pos);
      NodeListBuilderForPredictiveNodes builder(
          lattice->node_allocator(),
          lattice->node_allocator()->max_nodes_size(), pos_matcher_);
      suffix_dictionary_.LookupPredictive(
          absl::string_view(key.data() + pos, key.size() - pos), request,
          &builder);
      if (builder.result() != nullptr) {
        lattice->Insert(pos, builder.result());
      }
    }
  }

  // Predictive search from system dictionary.
  // (search words with between 5 and 8 characters)
  {
    constexpr size_t kMinSystemLookupKey = 5;
    constexpr size_t kMaxSystemLookupKey = 8;
    const size_t max_suffix_len =
        std::min(kMaxSystemLookupKey, conversion_key_chars.size());
    size_t pos = key.size();
    for (size_t suffix_len = 1; suffix_len <= max_suffix_len; ++suffix_len) {
      pos -=
          conversion_key_chars[conversion_key_chars.size() - suffix_len].size();
      DCHECK_GE(key.size(), pos);

      if (suffix_len < kMinSystemLookupKey) {
        // Just update |pos|.
        continue;
      }

      NodeListBuilderForPredictiveNodes builder(
          lattice->node_allocator(),
          lattice->node_allocator()->max_nodes_size(), pos_matcher_);
      dictionary_.LookupPredictive(
          absl::string_view(key.data() + pos, key.size() - pos), request,
          &builder);
      if (builder.result() != nullptr) {
        lattice->Insert(pos, builder.result());
      }
    }
  }
}

bool ImmutableConverter::MakeLattice(const ConversionRequest &request,
                                     Segments *segments,
                                     Lattice *lattice) const {
  if (segments == nullptr) {
    LOG(ERROR) << "Segments is nullptr";
    return false;
  }

  if (lattice == nullptr) {
    LOG(ERROR) << "Lattice is nullptr";
    return false;
  }

  if (segments->segments_size() >= kMaxSegmentsSize) {
    LOG(WARNING) << "too many segments";
    return false;
  }

  NormalizeHistorySegments(segments);

  const bool is_reverse =
      (request.request_type() == ConversionRequest::REVERSE_CONVERSION);

  const bool is_prediction =
      (request.request_type() == ConversionRequest::SUGGESTION ||
       request.request_type() == ConversionRequest::PREDICTION);

  // In suggestion mode, ImmutableConverter will not accept multiple-segments.
  // The result always consists of one segment.
  if (is_reverse || is_prediction) {
    const Segments::range conversion_segments = segments->conversion_segments();
    if (conversion_segments.size() != 1 ||
        conversion_segments.front().segment_type() != Segment::FREE) {
      LOG(WARNING) << "ImmutableConverter doesn't support constrained requests";
      return false;
    }
  }

  // Make the conversion key.
  std::string conversion_key;
  for (const Segment &segment : segments->conversion_segments()) {
    DCHECK(!segment.key().empty());
    conversion_key.append(segment.key());
  }
  const size_t max_char_len =
      is_reverse ? kMaxCharLengthForReverseConversion : kMaxCharLength;
  if (conversion_key.empty() || conversion_key.size() >= max_char_len) {
    LOG(WARNING) << "Conversion key is empty or too long: " << conversion_key;
    return false;
  }

  // Make the history key.
  std::string history_key;
  for (const Segment &segment : segments->history_segments()) {
    DCHECK(!segment.key().empty());
    history_key.append(segment.key());
  }
  // Check if the total length (length of history_key + conversion_key) doesn't
  // exceed the maximum key length. If it exceeds the limit, we simply clears
  // such useless history segments, which is acceptable because such cases
  // rarely happen in normal use cases.
  if (history_key.size() + conversion_key.size() >= max_char_len) {
    LOG(WARNING) << "Clear history segments due to the limit of key length.";
    segments->clear_history_segments();
    history_key.clear();
  }

  const std::string key = history_key + conversion_key;
  lattice->UpdateKey(key);
  lattice->ResetNodeCost();

  if (is_reverse) {
    // Reverse lookup for each prefix string in key is slow with current
    // implementation, so run it for them at once and cache the result.
    dictionary_.PopulateReverseLookupCache(key);
  }

  bool is_valid_lattice = true;
  // Perform the main part of lattice construction.
  if (!MakeLatticeNodesForHistorySegments(*segments, request, lattice) ||
      lattice->end_nodes(history_key.size()) == nullptr) {
    is_valid_lattice = false;
  }

  // Can not apply key corrector to invalid lattice.
  if (is_valid_lattice) {
    MakeLatticeNodesForConversionSegments(*segments, request, history_key,
                                          lattice);
  }

  if (is_reverse) {
    // No reverse look up will happen afterwards.
    dictionary_.ClearReverseLookupCache();
  }

  // Nodes look up for real time conversion for desktop.
  // Note:
  // For mobile, we decided to stop adding predictive nodes based on
  // experiments.
  if (is_prediction && !IsMobileRequest(request)) {
    MakeLatticeNodesForPredictiveNodes(*segments, request, lattice);
  }

  if (!is_valid_lattice) {
    // Safely bail out, since reverse look up cache was released already.
    return false;
  }

  if (lattice->end_nodes(key.size()) == nullptr) {
    LOG(WARNING) << "cannot build lattice from input";
    return false;
  }

  ApplyPrefixSuffixPenalty(conversion_key, lattice);

  // Re-segment personal-names, numbers ...etc
  if (request.request_type() == ConversionRequest::CONVERSION) {
    Resegment(*segments, history_key, conversion_key, lattice);
  }

  return true;
}

bool ImmutableConverter::MakeLatticeNodesForHistorySegments(
    const Segments &segments, const ConversionRequest &request,
    Lattice *lattice) const {
  const bool is_reverse =
      (request.request_type() == ConversionRequest::REVERSE_CONVERSION);
  const size_t history_segments_size = segments.history_segments_size();

  size_t segments_pos = 0;
  uint16_t last_rid = 0;

  for (size_t s = 0; s < history_segments_size; ++s) {
    const Segment &segment = segments.segment(s);
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
    rnode->bnext = nullptr;
    lattice->Insert(segments_pos, rnode);

    // For the last history segment,  we also insert a new node having
    // EOS part-of-speech. Viterbi algorithm will find the
    // best path from rnode(context) and rnode2(EOS).
    if (s + 1 == history_segments_size && candidate.rid != 0) {
      Node *rnode2 = lattice->NewNode();
      CHECK(rnode2);
      rnode2->lid = candidate.lid;
      rnode2->rid = 0;  // 0 is BOS/EOS

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
      rnode2->bnext = nullptr;
      lattice->Insert(segments_pos, rnode2);
    }

    // Dictionary lookup for the candidates which are
    // overlapping between history and conversion.
    // Check only the last history segment at this moment.
    //
    // Example: history "おいかわ(及川)", conversion: "たくや"
    // Here, try to find "おいかわたくや(及川卓也)" from dictionary
    // and insert "卓也" as a new word node with a modified cost
    //
    // Note: The overlapping lookup is disabled for prediction, because it can
    // produce noisy realtime candidates such as "て配" for the history "に",
    // which comes from "にて" + "配".
    // The bigram-like lookup ("卓也" from "及川") is covered in
    // dictionary_predictor.
    const bool is_prediction =
        (request.request_type() == ConversionRequest::SUGGESTION ||
         request.request_type() == ConversionRequest::PREDICTION);
    if (!is_prediction && s + 1 == history_segments_size) {
      const Node *node =
          Lookup(segments_pos, request, is_reverse, is_prediction, lattice);
      for (const Node *compound_node = node; compound_node != nullptr;
           compound_node = compound_node->bnext) {
        // No overlaps
        if (compound_node->key.size() <= rnode->key.size() ||
            compound_node->value.size() <= rnode->value.size() ||
            !absl::StartsWith(compound_node->key, rnode->key) ||
            !absl::StartsWith(compound_node->value, rnode->value)) {
          // not a prefix
          continue;
        }

        // Must be in the same POS group.
        // http://b/issue?id=2977618
        if (pos_group_.GetPosGroup(candidate.lid) !=
            pos_group_.GetPosGroup(compound_node->lid)) {
          continue;
        }

        // make new virtual node
        Node *new_node = lattice->NewNode();
        CHECK(new_node);

        // get the suffix part ("たくや/卓也")
        new_node->key.assign(compound_node->key, rnode->key.size(),
                             compound_node->key.size() - rnode->key.size());
        new_node->value.assign(
            compound_node->value, rnode->value.size(),
            compound_node->value.size() - rnode->value.size());

        // rid/lid are derived from the compound.
        // lid is just an approximation
        new_node->rid = compound_node->rid;
        new_node->lid = compound_node->lid;
        new_node->bnext = nullptr;
        new_node->node_type = Node::NOR_NODE;
        new_node->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;

        // New cost recalcuration:
        //
        // compound_node->wcost * (candidate len / compound_node len)
        // - trans(candidate.rid, new_node.lid)
        new_node->wcost =
            compound_node->wcost * candidate.value.size() /
                compound_node->value.size() -
            connector_.GetTransitionCost(candidate.rid, new_node->lid);

        MOZC_VLOG(2) << " compound_node->lid=" << compound_node->lid
                     << " compound_node->rid=" << compound_node->rid
                     << " compound_node->wcost=" << compound_node->wcost;
        MOZC_VLOG(2) << " last_rid=" << last_rid
                     << " candidate.lid=" << candidate.lid
                     << " candidate.rid=" << candidate.rid
                     << " candidate.cost=" << candidate.cost
                     << " candidate.wcost=" << candidate.wcost;
        MOZC_VLOG(2) << " new_node->wcost=" << new_node->wcost;

        new_node->constrained_prev = rnode;

        // Added as new node
        lattice->Insert(segments_pos + rnode->key.size(), new_node);

        MOZC_VLOG(2) << "Added: " << new_node->key << " " << new_node->value;
      }
    }

    // update segment pos
    segments_pos += rnode->key.size();
    last_rid = rnode->rid;
  }
  lattice->set_history_end_pos(segments_pos);
  return true;
}

void ImmutableConverter::MakeLatticeNodesForConversionSegments(
    const Segments &segments, const ConversionRequest &request,
    absl::string_view history_key, Lattice *lattice) const {
  absl::string_view key = lattice->key();
  const bool is_conversion =
      (request.request_type() == ConversionRequest::CONVERSION);
  // Do not use KeyCorrector if user changes the boundary.
  // http://b/issue?id=2804996
  std::unique_ptr<KeyCorrector> key_corrector;
  if (is_conversion && !segments.resized()) {
    KeyCorrector::InputMode mode = KeyCorrector::ROMAN;
    if (request.config().preedit_method() != config::Config::ROMAN) {
      mode = KeyCorrector::KANA;
    }
    key_corrector =
        std::make_unique<KeyCorrector>(key, mode, history_key.size());
  }

  const bool is_reverse =
      (request.request_type() == ConversionRequest::REVERSE_CONVERSION);
  const bool is_prediction =
      (request.request_type() == ConversionRequest::SUGGESTION ||
       request.request_type() == ConversionRequest::PREDICTION);
  for (size_t pos = history_key.size(); pos < key.size(); ++pos) {
    if (lattice->end_nodes(pos) != nullptr) {
      Node *rnode = Lookup(pos, request, is_reverse, is_prediction, lattice);
      // If history key is NOT empty and user input seems to starts with
      // a particle ("はにで..."), mark the node as STARTS_WITH_PARTICLE.
      // We change the segment boundary if STARTS_WITH_PARTICLE attribute
      // is assigned.
      if (!history_key.empty() && pos == history_key.size()) {
        for (Node *node = rnode; node != nullptr; node = node->bnext) {
          if (pos_matcher_.IsAcceptableParticleAtBeginOfSegment(node->lid) &&
              node->lid == node->rid) {  // not a compound.
            node->attributes |= Node::STARTS_WITH_PARTICLE;
          }
        }
      }
      CHECK(rnode != nullptr);
      lattice->Insert(pos, rnode);
      InsertCorrectedNodes(pos, key, request, key_corrector.get(), dictionary_,
                           lattice);
    }
  }
}

void ImmutableConverter::ApplyPrefixSuffixPenalty(
    absl::string_view conversion_key, Lattice *lattice) const {
  absl::string_view key = lattice->key();
  DCHECK_LE(conversion_key.size(), key.size());
  for (Node *node = lattice->begin_nodes(key.size() - conversion_key.size());
       node != nullptr; node = node->bnext) {
    // TODO(taku):
    // We might be able to tweak the penalty according to
    // the size of history segments.
    // If history-segments is non-empty, we can make the
    // penalty smaller so that history context is more likely
    // selected.
    node->wcost += segmenter_.GetPrefixPenalty(node->lid);
  }

  for (Node *node = lattice->end_nodes(key.size()); node != nullptr;
       node = node->enext) {
    node->wcost += segmenter_.GetSuffixPenalty(node->rid);
  }
}

void ImmutableConverter::Resegment(const Segments &segments,
                                   absl::string_view history_key,
                                   absl::string_view conversion_key,
                                   Lattice *lattice) const {
  for (size_t pos = history_key.size();
       pos < history_key.size() + conversion_key.size(); ++pos) {
    ApplyResegmentRules(pos, lattice);
  }

  // Enable constrained node.
  size_t segments_pos = 0;
  for (const Segment &segment : segments) {
    if (segment.segment_type() == Segment::FIXED_VALUE) {
      const Segment::Candidate &candidate = segment.candidate(0);
      Node *rnode = lattice->NewNode();
      CHECK(rnode);
      rnode->lid = candidate.lid;
      rnode->rid = candidate.rid;
      rnode->wcost = kMinCost;
      rnode->value = candidate.value;
      rnode->key = segment.key();
      rnode->node_type = Node::CON_NODE;
      rnode->bnext = nullptr;
      lattice->Insert(segments_pos, rnode);
    }
    segments_pos += segment.key().size();
  }
}

// Single segment conversion results should be set to |segments|.
void ImmutableConverter::InsertFirstSegmentToCandidates(
    const ConversionRequest &request, Segments *segments,
    const Lattice &lattice, absl::Span<const uint16_t> group,
    size_t max_candidates_size, bool allow_exact) const {
  const size_t only_first_segment_candidate_pos =
      segments->conversion_segment(0).candidates_size();
  InsertCandidates(request, segments, lattice, group, max_candidates_size,
                   ONLY_FIRST_SEGMENT);
  // Note that inserted candidates might consume the entire key.
  // e.g. key: "なのは", value: "ナノは"
  // Erase them later.
  if (segments->conversion_segment(0).candidates_size() <=
      only_first_segment_candidate_pos) {
    return;
  }

  // Set new costs for only first segment candidates
  // Basically, only first segment candidates cost is smaller
  // than that of single segment conversion results.
  // For example, the cost of "私の" is smaller than "私の名前は".
  // To merge these two categories of results, we will add the
  // cost penalty based on the cost diff.
  const Segment &first_segment = segments->conversion_segment(0);
  const int base_cost_diff = std::max(
      0, (first_segment.candidate(0).cost -
          first_segment.candidate(only_first_segment_candidate_pos).cost));
  const int base_wcost_diff = std::max(
      0, (first_segment.candidate(0).wcost -
          first_segment.candidate(only_first_segment_candidate_pos).wcost));
  constexpr int kOnlyFirstSegmentOffset = 300;

  if (allow_exact) {
    for (size_t i = only_first_segment_candidate_pos;
         i < first_segment.candidates_size(); ++i) {
      Segment::Candidate *candidate =
          segments->mutable_conversion_segment(0)->mutable_candidate(i);
      if (candidate->key.size() < first_segment.key().size()) {
        candidate->cost += (base_cost_diff + kOnlyFirstSegmentOffset);
        candidate->wcost += (base_wcost_diff + kOnlyFirstSegmentOffset);
        DCHECK(!(candidate->attributes &
                 Segment::Candidate::PARTIALLY_KEY_CONSUMED));
        candidate->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
      }
      candidate->consumed_key_size = Util::CharsLen(candidate->key);
    }
  } else {
    for (size_t i = only_first_segment_candidate_pos;
         i < first_segment.candidates_size();) {
      Segment::Candidate *candidate =
          segments->mutable_conversion_segment(0)->mutable_candidate(i);
      if (candidate->key.size() >= first_segment.key().size()) {
        segments->mutable_conversion_segment(0)->erase_candidate(i);
        // If the size of candidate's key is greater than or
        // equal to 1st segment's key,
        // it means that the result consumes the entire key.
        // Such results are not appropriate for PARTIALLY_KEY_CONSUMED so erase
        // it.
        continue;
      }
      candidate->cost += (base_cost_diff + kOnlyFirstSegmentOffset);
      candidate->wcost += (base_wcost_diff + kOnlyFirstSegmentOffset);
      DCHECK(!(candidate->attributes &
               Segment::Candidate::PARTIALLY_KEY_CONSUMED));
      candidate->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
      candidate->consumed_key_size = Util::CharsLen(candidate->key);
      ++i;
    }
  }
}

bool ImmutableConverter::IsSegmentEndNode(const ConversionRequest &request,
                                          const Segments &segments,
                                          const Node *node,
                                          absl::Span<const uint16_t> group,
                                          bool is_single_segment) const {
  DCHECK(node->next);
  if (node->next->node_type == Node::EOS_NODE) {
    return true;
  }

  // In reverse conversion, group consecutive white spaces into one segment.
  // For example, "ほん むりょう" -> "ほん", " ", "むりょう".
  if (request.request_type() == ConversionRequest::REVERSE_CONVERSION) {
    const bool this_node_is_ws = ContainsWhiteSpacesOnly(node->key);
    const bool next_node_is_ws = ContainsWhiteSpacesOnly(node->next->key);
    if (this_node_is_ws) {
      return !next_node_is_ws;
    }
    if (next_node_is_ws) {
      return true;
    }
    // If this and next nodes are both non-white spaces, fall back to the
    // subsequent logic.
  }

  const Segment &old_segment = segments.segment(group[node->begin_pos]);
  // |node| and |node->next| should be in same segment due to FIXED_BOUNDAY
  // |node->next| is NOT a boundary. Very strong constraint.
  if (group[node->begin_pos] == group[node->next->begin_pos] &&
      old_segment.segment_type() == Segment::FIXED_BOUNDARY) {
    return false;
  }

  // |node->next| is a boundary. Very strong constraint.
  if (group[node->begin_pos] != group[node->next->begin_pos]) {
    return true;
  }

  // CON_NODE is generated for FIXED_VALUE candidate.
  if (node->node_type == Node::CON_NODE) {
    return true;
  }

  // Grammatically segmented.
  if (segmenter_.IsBoundary(*node, *node->next, is_single_segment)) {
    return true;
  }

  return false;
}

Segment *ImmutableConverter::GetInsertTargetSegment(
    const Lattice &lattice, absl::Span<const uint16_t> group,
    InsertCandidatesType type, size_t begin_pos, const Node *node,
    Segments *segments) const {
  if (type != MULTI_SEGMENTS) {
    // Realtime conversion that produces only one segment.
    return segments->mutable_segment(segments->segments_size() - 1);
  }

  // 'Normal' conversion. Add new segment and initialize it.
  Segment *segment = segments->add_segment();
  DCHECK(segment);
  segment->clear_candidates();
  segment->set_key(lattice.key().substr(begin_pos, node->end_pos - begin_pos));
  const Segment &old_segment = segments->segment(group[node->begin_pos]);
  segment->set_segment_type(old_segment.segment_type());
  return segment;
}

void ImmutableConverter::InsertCandidates(const ConversionRequest &request,
                                          Segments *segments,
                                          const Lattice &lattice,
                                          absl::Span<const uint16_t> group,
                                          size_t max_candidates_size,
                                          InsertCandidatesType type) const {
  // skip HIS_NODE(s)
  Node *prev = lattice.bos_nodes();
  for (Node *node = lattice.bos_nodes()->next;
       node->next != nullptr && node->node_type == Node::HIS_NODE;
       node = node->next) {
    prev = node;
  }

  const size_t expand_size = std::clamp<size_t>(max_candidates_size, 1, 512);

  const bool is_single_segment =
      (type == SINGLE_SEGMENT || type == FIRST_INNER_SEGMENT);
  NBestGenerator nbest_generator(user_dictionary_, segmenter_, connector_,
                                 pos_matcher_, lattice, suggestion_filter_);

  std::string original_key;
  for (const Segment &segment : segments->conversion_segments()) {
    original_key.append(segment.key());
  }

  size_t begin_pos = std::string::npos;
  for (Node *node = prev->next; node->next != nullptr; node = node->next) {
    if (begin_pos == std::string::npos) {
      begin_pos = node->begin_pos;
    }

    if (!IsSegmentEndNode(request, *segments, node, group, is_single_segment)) {
      continue;
    }

    Segment *segment =
        GetInsertTargetSegment(lattice, group, type, begin_pos, node, segments);
    CHECK(segment);

    NBestGenerator::Options options;
    if (type == SINGLE_SEGMENT || type == FIRST_INNER_SEGMENT) {
      // For real time conversion.
      options.boundary_mode = NBestGenerator::ONLY_EDGE;
      options.candidate_mode |= NBestGenerator::FILL_INNER_SEGMENT_INFO;
    } else if (segment->segment_type() == Segment::FIXED_BOUNDARY) {
      // Boundary is specified. Skip boundary check in n-best generator.
      options.boundary_mode = NBestGenerator::ONLY_MID;
    }
    if (type == FIRST_INNER_SEGMENT) {
      // Inserts only first segment from realtime conversion path.
      options.candidate_mode |=
          NBestGenerator::BUILD_FROM_ONLY_FIRST_INNER_SEGMENT;
      options.candidate_mode |= NBestGenerator::FILL_INNER_SEGMENT_INFO;
    }
    nbest_generator.Reset(prev, node->next, options);
    nbest_generator.SetCandidates(request, original_key, expand_size, segment);

    if (type == MULTI_SEGMENTS || type == SINGLE_SEGMENT) {
      InsertDummyCandidates(segment, expand_size);
    }

    if (node->node_type == Node::CON_NODE) {
      segment->set_segment_type(Segment::FIXED_VALUE);
    }

    if (type == ONLY_FIRST_SEGMENT) {
      break;
    }
    begin_pos = std::string::npos;
    prev = node;
  }
}

bool ImmutableConverter::MakeSegments(const ConversionRequest &request,
                                      const Lattice &lattice,
                                      absl::Span<const uint16_t> group,
                                      Segments *segments) const {
  if (segments == nullptr) {
    LOG(WARNING) << "Segments is nullptr";
    return false;
  }

  const ConversionRequest::RequestType type = request.request_type();

  if (type == ConversionRequest::CONVERSION ||
      type == ConversionRequest::REVERSE_CONVERSION) {
    InsertCandidatesForConversion(request, lattice, group, segments);
  } else {
    InsertCandidatesForPrediction(request, lattice, group, segments);
  }

  return true;
}

void ImmutableConverter::InsertCandidatesForConversion(
    const ConversionRequest &request, const Lattice &lattice,
    absl::Span<const uint16_t> group, Segments *segments) const {
  DCHECK(!request.create_partial_candidates());
  // Currently, we assume that REVERSE_CONVERSION only
  // requires 1 result.
  // TODO(taku): support to set the size on REVESER_CONVERSION mode.
  const size_t max_candidates_size =
      ((request.request_type() == ConversionRequest::REVERSE_CONVERSION)
           ? 1
           : request.max_conversion_candidates_size());

  // InsertCandidates inserts new segments after the existing
  // conversion segments. So we have to erase old conversion segments.
  // We have to keep old segments for calling InsertCandidates because
  // we need segment constraints like FIXED_BOUNDARY.
  // TODO(toshiyuki): We want more beautiful structure.
  const size_t old_conversion_segments_size =
      segments->conversion_segments_size();
  InsertCandidates(request, segments, lattice, group, max_candidates_size,
                   MULTI_SEGMENTS);
  if (old_conversion_segments_size > 0) {
    segments->erase_segments(segments->history_segments_size(),
                             old_conversion_segments_size);
  }
}

void ImmutableConverter::InsertCandidatesForRealtimeWithCandidateChecker(
    const ConversionRequest &request, const Lattice &lattice,
    absl::Span<const uint16_t> group, Segments *segments) const {
  constexpr int kSingleSegmentCharCoverage = 12;
  Segment *target_segment = segments->mutable_conversion_segment(0);
  absl::flat_hash_set<std::string> added;
  Segments tmp_segments = *segments;
  {
    // Candidates for the whole path
    constexpr int kMaxSize = 3;
    InsertCandidates(request, &tmp_segments, lattice, group, kMaxSize,
                     SINGLE_SEGMENT);

    // At least one candidate should be added.
    // Skip to add the similar candidates unless the char coverage is still
    // available.
    DCHECK_GT(tmp_segments.conversion_segment(0).candidates_size(), 0);
    const auto &top_cand = tmp_segments.conversion_segment(0).candidate(0);
    const std::vector<absl::string_view> top_boundary =
        GetBoundaryInfo(top_cand);
    int remaining_char_coverage = kSingleSegmentCharCoverage;
    for (int i = 0; i < tmp_segments.conversion_segment(0).candidates_size();
         ++i) {
      const auto &c = tmp_segments.conversion_segment(0).candidate(i);
      constexpr int kCostDiff = 2302;  // 500*log(100)
      if (c.cost - top_cand.cost > kCostDiff) {
        continue;
      }
      if (i != 0 && GetBoundaryInfo(c) == top_boundary &&
          remaining_char_coverage < 0) {
        // Skip to add the similar candidates when there is no remaining
        // coverage.
        continue;
      }
      Segment::Candidate *candidate = target_segment->add_candidate();
      *candidate = c;
      added.insert(c.value);
      remaining_char_coverage -= Util::CharsLen(c.value);
    }
  }
  tmp_segments.mutable_conversion_segment(0)->clear_candidates();

  {
    // Candidates for the first segment of each n-best path.
    InsertCandidates(request, &tmp_segments, lattice, group,
                     request.max_conversion_candidates_size() -
                         target_segment->candidates_size(),
                     FIRST_INNER_SEGMENT);
    constexpr int kMaxCostDiffForFirstInnerSegment = 3107;  // 500*log(500)
    FirstInnerSegmentCandidateChecker checker(*target_segment,
                                              kMaxCostDiffForFirstInnerSegment);
    for (int i = 0; i < tmp_segments.conversion_segment(0).candidates_size();
         ++i) {
      Segment::Candidate *c =
          tmp_segments.mutable_conversion_segment(0)->mutable_candidate(i);
      if (added.contains(c->value)) {
        continue;
      }
      if (c->key.size() != target_segment->key().size()) {
        // Explicitly add suffix penalty, since the penalty is not added for non
        // end nodes.
        const int32_t suffix_penalty = segmenter_.GetSuffixPenalty(c->rid);
        c->wcost += suffix_penalty;
        c->cost += suffix_penalty;
      }

      if (!checker.IsGoodCandidate(*c)) {
        continue;
      }
      Segment::Candidate *candidate = target_segment->add_candidate();
      *candidate = *c;
      checker.AddEntry(*c);
      added.insert(c->value);
    }
  }
}

void ImmutableConverter::InsertCandidatesForPrediction(
    const ConversionRequest &request, const Lattice &lattice,
    absl::Span<const uint16_t> group, Segments *segments) const {
  const size_t max_candidates_size = request.max_conversion_candidates_size();

  if (!request.create_partial_candidates()) {
    // Desktop (or physical keyboard / handwriting in Mobile)
    InsertCandidates(request, segments, lattice, group, max_candidates_size,
                     SINGLE_SEGMENT);
    return;
  }

  // Mobile
  InsertCandidatesForRealtimeWithCandidateChecker(request, lattice, group,
                                                  segments);
}

void ImmutableConverter::MakeGroup(const Segments &segments,
                                   std::vector<uint16_t> *group) const {
  group->clear();
  for (size_t i = 0; i < segments.segments_size(); ++i) {
    for (size_t j = 0; j < segments.segment(i).key().size(); ++j) {
      group->push_back(static_cast<uint16_t>(i));
    }
  }
  group->push_back(static_cast<uint16_t>(segments.segments_size()));
}

bool ImmutableConverter::ConvertForRequest(const ConversionRequest &request,
                                           Segments *segments) const {
  const bool is_prediction =
      (request.request_type() == ConversionRequest::PREDICTION ||
       request.request_type() == ConversionRequest::SUGGESTION);

  Lattice *lattice = GetLattice(segments, is_prediction);

  if (!MakeLattice(request, segments, lattice)) {
    LOG(WARNING) << "could not make lattice";
    return false;
  }

  std::vector<uint16_t> group;
  MakeGroup(*segments, &group);

  if (is_prediction) {
    if (!PredictionViterbi(*segments, lattice)) {
      LOG(WARNING) << "prediction_viterbi failed";
      return false;
    }
  } else {
    if (!Viterbi(*segments, lattice)) {
      LOG(WARNING) << "viterbi failed";
      return false;
    }
  }

  MOZC_VLOG(2) << lattice->DebugString();
  if (!MakeSegments(request, *lattice, group, segments)) {
    LOG(WARNING) << "make segments failed";
    return false;
  }

  return true;
}

}  // namespace mozc
