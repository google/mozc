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

// This class is used to filter out generated candidate by its
// cost, structure and relation with previously added candidates.

#include "converter/candidate_filter.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/number_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "converter/candidate.h"
#include "converter/node.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "prediction/suggestion_filter.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "request/request_util.h"

namespace mozc {
namespace converter {
namespace {

using ::mozc::converter::candidate_filter_internal::CandidateId;
using ::mozc::dictionary::PosMatcher;
using ::mozc::dictionary::UserDictionaryInterface;

constexpr size_t kSizeThresholdForWeakCompound = 10;

constexpr size_t kMaxCandidatesSize = 200;  // how many candidates we expand

// Currently, the cost (logprob) is calcurated as cost = -500 * log(prob).
// Suppose having two candidates A and B and prob(A) = C * prob(B), where
// C = 1000 (some constant variable). The word "A" appears 1000 times more
// frequently than B.
// In this case,
// cost(B) - cost(A) = -500 * [log(prob(B)) - log (C * prob(B)) ]
//                   = -500 * [-log(C) + log(prob(B)) - log(prob(B))]
//                   = 500 * log(C)
// This implies that it is more reasonable to filter candidates
// by an absolute difference of costs between cost(B) and cost(A).
//
// Here's "C" and cost-diff relation:
// C       cost diff: 500 * log(C)
// 10      1151.29
// 100     2302.58
// 1000    3453.87
// 10000   4605.17
// 100000  5756.46
// 1000000 6907.75
constexpr int kMinCost = 100;
constexpr int kCostOffset = 6907;
constexpr int kStructureCostOffset = 3453;
constexpr int kMinStructureCostOffset = 1151;
constexpr int32_t kStopEnmerationCacheSize = 30;

// Returns true if the given node sequence is noisy weak compound.
// Please refer to the comment in FilterCandidateInternal for the idea.
inline bool IsNoisyWeakCompound(const absl::Span<const Node *const> nodes,
                                const dictionary::PosMatcher &pos_matcher,
                                const Candidate *candidate) {
  if (nodes.size() <= 1) {
    return false;
  }
  if (nodes[0]->lid != nodes[0]->rid) {
    // nodes[0] is COMPOUND entry in dictionary.
    return false;
  }
  if (pos_matcher.IsWeakCompoundFillerPrefix(nodes[0]->lid)) {
    // Word that starts with 'filler' is always noisy.
    MOZC_CANDIDATE_LOG(candidate, "Noise: Word that starts with 'filler'");
    return true;
  }
  if (nodes[1]->lid != nodes[1]->rid) {
    // If the second node is a compound word (i.e. lid != rid),
    // it is basically filtered.
    // However, the second node is an anti_phrase word (e.g. とともに),
    // the node is not filtered. Since there is no direct way
    // to determine the anti_phrase word, we use the following heuristic.

    const bool is_possible_anti_phrase_connection =
        pos_matcher.IsContentNoun(nodes[0]->rid) &&
        pos_matcher.IsAcceptableParticleAtBeginOfSegment(nodes[1]->lid);
    if (!is_possible_anti_phrase_connection) {
      // Some node +  COMPOUND node may be noisy.
      MOZC_CANDIDATE_LOG(candidate, "Noise: Some node + COMPOUND node");
      return true;
    }
  }
  if (pos_matcher.IsWeakCompoundNounPrefix(nodes[0]->lid) &&
      !pos_matcher.IsWeakCompoundNounSuffix(nodes[1]->lid)) {
    // Noun prefix + not noun
    MOZC_CANDIDATE_LOG(candidate, "Noise: Noun prefix + not noun");
    return true;
  }
  if (pos_matcher.IsWeakCompoundVerbPrefix(nodes[0]->lid) &&
      !pos_matcher.IsWeakCompoundVerbSuffix(nodes[1]->lid)) {
    // Verb prefix + not verb
    MOZC_CANDIDATE_LOG(candidate, "Noise: Verb prefix + not verb");
    return true;
  }
  return false;
}

// Returns true if the given node sequence is connected weak compound.
// Please refer to the comment in FilterCandidateInternal for the idea.
inline bool IsConnectedWeakCompound(const absl::Span<const Node *const> nodes,
                                    const dictionary::PosMatcher &pos_matcher,
                                    const Candidate *candidate) {
  if (nodes.size() <= 1) {
    return false;
  }
  if (nodes[0]->lid != nodes[0]->rid || nodes[1]->lid != nodes[1]->rid) {
    // nodes[0/1] is COMPOUND entry in dictionary.
    return false;
  }
  if (pos_matcher.IsWeakCompoundNounPrefix(nodes[0]->lid) &&
      pos_matcher.IsWeakCompoundNounSuffix(nodes[1]->lid)) {
    // Noun prefix + noun
    MOZC_CANDIDATE_LOG(candidate, "Connected: Noun prefix + noun");
    return true;
  }
  if (pos_matcher.IsWeakCompoundVerbPrefix(nodes[0]->lid) &&
      pos_matcher.IsWeakCompoundVerbSuffix(nodes[1]->lid)) {
    // Verb prefix + verb
    MOZC_CANDIDATE_LOG(candidate, "Connected: Verb prefix + verb");
    return true;
  }
  return false;
}

bool IsIsolatedWordOrGeneralSymbol(const dictionary::PosMatcher &pos_matcher,
                                   uint16_t pos_id) {
  return pos_matcher.IsIsolatedWord(pos_id) ||
         pos_matcher.IsGeneralSymbol(pos_id);
}

bool ContainsIsolatedWordOrGeneralSymbol(
    const dictionary::PosMatcher &pos_matcher,
    const absl::Span<const Node *const> nodes) {
  for (const Node *node : nodes) {
    if (IsIsolatedWordOrGeneralSymbol(pos_matcher, node->lid)) {
      return true;
    }
  }
  return false;
}

bool IsNormalOrConstrainedNode(const Node *node) {
  return node != nullptr && (node->node_type == Node::NOR_NODE ||
                             node->node_type == Node::CON_NODE);
}

bool IsCompoundCandidate(const absl::Span<const Node *const> nodes) {
  return nodes.size() == 1 && nodes[0]->lid != nodes[0]->rid;
}

bool IsSuffixNode(const dictionary::PosMatcher &pos_matcher, const Node &node) {
  return pos_matcher.IsSuffixWord(node.lid) &&
         pos_matcher.IsSuffixWord(node.rid);
}

bool IsFunctionalNode(const dictionary::PosMatcher &pos_matcher,
                      const Node &node) {
  return pos_matcher.IsFunctional(node.lid) &&
         pos_matcher.IsFunctional(node.rid);
}

// Returns true if the node structure is
// content_word + suffix_word*N + (suffix_word|functional_word).
// Example: "行き+ます", "山+が", etc.
bool IsTypicalNodeStructure(const dictionary::PosMatcher &pos_matcher,
                            const absl::Span<const Node *const> nodes) {
  DCHECK_GT(nodes.size(), 1);
  if (IsSuffixNode(pos_matcher, *nodes[0])) {
    return false;
  }
  for (size_t i = 1; i < nodes.size() - 1; ++i) {
    if (!IsSuffixNode(pos_matcher, *nodes[i])) {
      return false;
    }
  }
  return IsSuffixNode(pos_matcher, *nodes.back()) ||
         IsFunctionalNode(pos_matcher, *nodes.back());
}

// Returns true if |lnodes| and |rnodes| have the same Pos structure.
bool IsSameNodeStructure(const absl::Span<const Node *const> lnodes,
                         const absl::Span<const Node *const> rnodes) {
  if (lnodes.size() != rnodes.size()) {
    return false;
  }
  for (int i = 0; i < lnodes.size(); ++i) {
    if (lnodes[i]->lid != rnodes[i]->lid || lnodes[i]->rid != rnodes[i]->rid) {
      return false;
    }
  }
  return true;
}

// Returns true if there is a number node that does not follow the
bool IsNoisyNumberCandidate(const dictionary::PosMatcher &pos_matcher,
                            const absl::Span<const Node *const> nodes) {
  auto is_converted_number = [&](const Node &node) {
    if (node.lid != node.rid) {
      return false;
    }
    if (!Util::IsScriptType(node.key, Util::HIRAGANA)) {
      return false;
    }
    return pos_matcher.IsNumber(node.lid) ||
           pos_matcher.IsKanjiNumber(node.rid);
  };
  for (int i = 0; i < nodes.size(); ++i) {
    if (!is_converted_number(*nodes[i])) {
      continue;
    }
    if (i + 1 < nodes.size() && !is_converted_number(*nodes[i + 1]) &&
        !pos_matcher.IsCounterSuffixWord(nodes[i + 1]->lid)) {
      // "にいく": "2行く"
      return true;
    }
    if (i - 1 >= 0 && pos_matcher.IsUniqueNoun(nodes[i - 1]->rid)) {
      // "しんじゅくに": "新宿2"
      return true;
    }
  }
  return false;
}

}  // namespace

CandidateFilter::CandidateFilter(const UserDictionaryInterface &user_dictionary,
                                 const PosMatcher &pos_matcher,
                                 const SuggestionFilter &suggestion_filter)
    : user_dictionary_(user_dictionary),
      pos_matcher_(pos_matcher),
      suggestion_filter_(suggestion_filter),
      top_candidate_(nullptr) {}

void CandidateFilter::Reset() {
  seen_.clear();
  top_candidate_ = nullptr;
}

CandidateFilter::ResultType CandidateFilter::CheckRequestType(
    const ConversionRequest &request, const absl::string_view original_key,
    const Candidate &candidate,
    const absl::Span<const Node *const> nodes) const {
  // Filtering by the suggestion filter, which is applied only for the
  // PREDICTION and SUGGESTION modes.
  switch (request.request_type()) {
    case ConversionRequest::PREDICTION:
      // - For Mobile
      // (To be precise, in mixed_conversion mode;
      //  Mobile IME with physical keyboard can set mixed_conversion=false and
      //  act similar to the Desktop version)
      // Most users do not trigger conversion explicitly.
      // So we don't apply the suggestion filter when the input key is
      // exactly the same as candidate's.
      //
      // Note:
      // SUGGESTION command is not called when mixed_conversion=true

      // - For Desktop
      // In the PREDICTION mode, the suggestion filter is not applied and the
      // same filtering rule as the CONVERSION mode is used because the
      // PREDICTION is triggered by user action (hitting tab keys), i.e.,
      // prediction candidates are not automatically shown to users. On the
      // contrary, since a user hit tab keys to run prediction, even unfavorable
      // words might be what the user wants to type.  Therefore, filtering rule
      // is relaxed for the PREDICTION mode: we don't apply the suggestion
      // filter if the user input key is exactly the same as candidate's.
      if (original_key == candidate.key) {
        break;
      }
      [[fallthrough]];
    case ConversionRequest::SUGGESTION:
      // - For Desktop
      // In contrast to the PREDICTION mode, the SUGGESTION is triggered without
      // any user actions, i.e., suggestion candidates are automatically
      // displayed to users.  Therefore, it's better to filter unfavorable words
      // in this mode.
      if (suggestion_filter_.IsBadSuggestion(candidate.value)) {
        MOZC_CANDIDATE_LOG(&candidate, "IsBadsuggestion(candidate)");
        return BAD_CANDIDATE;
      }
      // TODO(noriyukit): In the implementation below, the possibility remains
      // that multiple nodes constitute bad candidates. For stronger filtering,
      // we may want to check all the possibilities.
      for (size_t i = 0; i < nodes.size(); ++i) {
        if (suggestion_filter_.IsBadSuggestion(nodes[i]->value)) {
          MOZC_CANDIDATE_LOG(&candidate, "IsBadsuggestion(node)");
          return BAD_CANDIDATE;
        }
      }
      break;
    default:
      break;
  }
  return GOOD_CANDIDATE;
}

CandidateFilter::ResultType CandidateFilter::FilterCandidateInternal(
    const ConversionRequest &request, const absl::string_view original_key,
    const Candidate *candidate, const absl::Span<const Node *const> top_nodes,
    const absl::Span<const Node *const> nodes) {
  DCHECK(candidate);

  if (ResultType result =
          CheckRequestType(request, original_key, *candidate, nodes);
      result != GOOD_CANDIDATE) {
    return result;
  }

  // In general, the cost of constrained node tends to be overestimated.
  // If the top candidate has constrained node, we skip the main body
  // of CandidateFilter, meaning that the node is not treated as the top
  // node for CandidateFilter.
  if (candidate->attributes & Candidate::CONTEXT_SENSITIVE) {
    return CandidateFilter::GOOD_CANDIDATE;
  }

  if (request_util::ShouldFilterNoisyNumberCandidate(request)) {
    if (IsNoisyNumberCandidate(pos_matcher_, nodes)) {
      return CandidateFilter::BAD_CANDIDATE;
    }
  }

  const size_t candidate_size = seen_.size();
  if (top_candidate_ == nullptr || candidate_size == 0) {
    top_candidate_ = candidate;
  }

  CHECK(top_candidate_);

  // "短縮よみ" or "記号,一般" must have only 1 node.  Note that "顔文字" POS
  // from user dictionary is converted to "記号,一般" in Mozc engine.
  if (nodes.size() > 1 &&
      ContainsIsolatedWordOrGeneralSymbol(pos_matcher_, nodes)) {
    MOZC_CANDIDATE_LOG(candidate, "ContainsIsolatedWordOrGeneralSymbol");
    return CandidateFilter::BAD_CANDIDATE;
  }
  // This case tests the case where the isolated word or general symbol is in
  // content word.
  if (IsIsolatedWordOrGeneralSymbol(pos_matcher_, nodes[0]->lid) &&
      (IsNormalOrConstrainedNode(nodes[0]->prev) ||
       IsNormalOrConstrainedNode(nodes[0]->next))) {
    MOZC_CANDIDATE_LOG(candidate, "IsIsolatedWordOrGeneralSymbol");
    return CandidateFilter::BAD_CANDIDATE;
  }

  // Remove "抑制単語" just in case.
  if (user_dictionary_.IsSuppressedEntry(candidate->key, candidate->value) ||
      (candidate->key != candidate->content_key &&
       candidate->value != candidate->content_value &&
       user_dictionary_.IsSuppressedEntry(candidate->content_key,
                                          candidate->content_value))) {
    MOZC_CANDIDATE_LOG(candidate, "SuppressEntry");
    return CandidateFilter::BAD_CANDIDATE;
  }

  // Don't remove duplications if USER_DICTIONARY.
  if (candidate->attributes & Candidate::USER_DICTIONARY) {
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // too many candidates size
  if (candidate_size + 1 >= kMaxCandidatesSize) {
    return CandidateFilter::STOP_ENUMERATION;
  }

  // The candidate is already seen.
  if (seen_.contains(*candidate)) {
    MOZC_CANDIDATE_LOG(candidate, "already seen");
    return CandidateFilter::BAD_CANDIDATE;
  }

  CHECK(!nodes.empty());

  // Suppress "書います", "書いすぎ", "買いて"
  // Basic idea:
  //  - WagyoRenyoConnectionVerb(= "動詞,*,*,*,五段・ワ行促音便,連用形",
  // "買い", "言い", "使い", etc) should not connect to TeSuffix(= "て",
  // "てる", "ちゃう", "とく", etc).
  //  - KagyoTaConnectionVerb(= 動詞,*,*,*,五段・カ行(促|イ)音便,連用タ接続",
  // "書い", "歩い", "言っ", etc) should not connect to verb suffix other
  // than TeSuffix
  if (Util::GetScriptType(nodes[0]->value) != Util::HIRAGANA) {
    if (nodes.size() >= 2) {
      // For node sequence
      if (pos_matcher_.IsKagyoTaConnectionVerb(nodes[0]->rid) &&
          pos_matcher_.IsVerbSuffix(nodes[1]->lid) &&
          !pos_matcher_.IsTeSuffix(nodes[1]->lid)) {
        // "書い" | "ます", "過ぎ", etc
        MOZC_CANDIDATE_LOG(candidate, "IsKagyoTaConnectionVerb");
        return CandidateFilter::BAD_CANDIDATE;
      }
      if (pos_matcher_.IsWagyoRenyoConnectionVerb(nodes[0]->rid) &&
          pos_matcher_.IsTeSuffix(nodes[1]->lid)) {
        // "買い" | "て"
        MOZC_CANDIDATE_LOG(candidate, "IsWagyoRenyoConnectionVerb");
        return CandidateFilter::BAD_CANDIDATE;
      }
    }
    if (nodes[0]->lid != nodes[0]->rid) {
      // For compound
      if (pos_matcher_.IsKagyoTaConnectionVerb(nodes[0]->lid) &&
          pos_matcher_.IsVerbSuffix(nodes[0]->rid) &&
          !pos_matcher_.IsTeSuffix(nodes[0]->rid)) {
        // "書い" | "ます", "過ぎ", etc
        MOZC_CANDIDATE_LOG(candidate, "IsKagyoTaConnectionVerb");
        return CandidateFilter::BAD_CANDIDATE;
      }
      if (pos_matcher_.IsWagyoRenyoConnectionVerb(nodes[0]->lid) &&
          pos_matcher_.IsTeSuffix(nodes[0]->rid)) {
        // "買い" | "て"
        MOZC_CANDIDATE_LOG(candidate, "IsWagyoRenyoConnectionVerb");
        return CandidateFilter::BAD_CANDIDATE;
      }
    }
  }

  // The candidate consists of only one token
  if (nodes.size() == 1) {
    MOZC_VLOG(1) << "don't filter single segment: " << candidate->value;
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // don't drop single character
  if (Util::CharsLen(candidate->value) == 1) {
    MOZC_VLOG(1) << "don't filter single character: " << candidate->value;
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // Remove noisy candidates from prefix.
  // For example, "お危機します" for "おききします".
  //
  // Basic idea:
  //   "weak compound": words consist from "prefix + content"
  //   "connected weak compound": noun prefix("名詞接続") + noun words("体言")
  //      or verb prefix("動詞接続") + verb words("用言")
  //   "noisy weak compound": types of prefix and content do not match.
  //   - We do not allow noisy weak compound except for the top result. Even for
  //     the top result, we will check other conditions for filtering.
  //   - We do not allow connected weak compound if the rank is low enough.
  const bool is_noisy_weak_compound =
      IsNoisyWeakCompound(nodes, pos_matcher_, candidate);
  const bool is_connected_weak_compound =
      IsConnectedWeakCompound(nodes, pos_matcher_, candidate);

  if (is_noisy_weak_compound && candidate_size >= 1) {
    MOZC_CANDIDATE_LOG(candidate, "is_noisy_weak_compound");
    return CandidateFilter::BAD_CANDIDATE;
  }

  if (is_connected_weak_compound &&
      candidate_size >= kSizeThresholdForWeakCompound) {
    MOZC_CANDIDATE_LOG(candidate, "is_connected_weak_compound");
    return CandidateFilter::BAD_CANDIDATE;
  }

  // don't drop lid/rid are the same as those
  // of top candidate.
  // http://b/issue?id=4285213
  if (!is_noisy_weak_compound && top_candidate_->structure_cost == 0 &&
      candidate->lid == top_candidate_->lid &&
      candidate->rid == top_candidate_->rid) {
    MOZC_VLOG(1) << "don't filter lid/rid are the same:" << candidate->value;
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // "好かっ|たり" vs  "良かっ|たり" have same non_content_value.
  // "良かっ|たり" is also a good candidate but it is not the top candidate.
  // non_content_value ("たり") should be Hiragana.
  // Background:
  // 名詞,接尾 nodes ("済み", "損", etc) can also be non_content_value.
  const absl::string_view top_non_content_value(
      top_candidate_->value.data() + top_candidate_->content_value.size());
  const absl::string_view non_content_value(candidate->value.data() +
                                            candidate->content_value.size());
  if (!is_noisy_weak_compound && top_candidate_ != candidate &&
      top_candidate_->content_value != top_candidate_->value &&
      Util::GetScriptType(top_non_content_value) == Util::HIRAGANA &&
      top_non_content_value == non_content_value) {
    MOZC_VLOG(1) << "don't filter if non-content value are the same: "
                 << candidate->value;
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // Check Katakana transliterations
  // Skip this check when the conversion mode is real-time;
  // otherwise this ruins the whole sentence
  // that starts with alphabets.
  if (!(candidate->attributes & Candidate::REALTIME_CONVERSION)) {
    const bool is_top_english_t13n =
        (Util::GetScriptType(nodes[0]->key) == Util::HIRAGANA &&
         Util::IsEnglishTransliteration(nodes[0]->value));
    for (size_t i = 1; i < nodes.size(); ++i) {
      // EnglishT13N must be the prefix of the candidate.
      if (Util::GetScriptType(nodes[i]->key) == Util::HIRAGANA &&
          Util::IsEnglishTransliteration(nodes[i]->value)) {
        MOZC_CANDIDATE_LOG(candidate, "IsEnglishTransliteration");
        return CandidateFilter::BAD_CANDIDATE;
      }
      // nodes[1..] are non-functional candidates.
      // In other words, the node just after KatakanaT13n candidate should
      // be a functional word.
      if (is_top_english_t13n && !pos_matcher_.IsFunctional(nodes[i]->lid)) {
        MOZC_CANDIDATE_LOG(candidate, "!IsFunctional");
        return CandidateFilter::BAD_CANDIDATE;
      }
    }
  }

  const int64_t top_cost = std::max(kMinCost, top_candidate_->cost);
  const int64_t top_structure_cost =
      std::max(kMinCost, top_candidate_->structure_cost);

  // If candidate size < 3, don't filter candidate aggressively
  // TODO(taku): This is a tentative workaround for the case where
  // TOP candidate is compound and the structure cost for it is "0"
  // If 2nd or 3rd candidates are regular candidate but not having
  // non-zero cost, they might be removed. This hack removes such case.
  if (IsCompoundCandidate(top_nodes) && candidate_size < 3 &&
      candidate->cost < top_cost + 2302 && candidate->structure_cost < 6907) {
    return CandidateFilter::GOOD_CANDIDATE;
  }

  // Don't drop personal names aggressively.
  // We have to show personal names even if they are too minor.
  // We basically ignores the cost threadshould. Filter candidate
  // only by structure cost.
  int cost_offset = kCostOffset;
  if (candidate->lid == pos_matcher_.GetLastNameId() ||
      candidate->lid == pos_matcher_.GetFirstNameId()) {
    cost_offset = INT_MAX - top_cost;
  }

  // Filters out candidates with higher cost.
  if (top_cost + cost_offset < candidate->cost &&
      top_structure_cost + kMinStructureCostOffset <
          candidate->structure_cost) {
    // Stops candidates enumeration when we see sufficiently high cost
    // candidate.
    MOZC_VLOG(2) << "cost is invalid: " << "top_cost=" << top_cost
                 << " cost_offset=" << cost_offset
                 << " value=" << candidate->value << " cost=" << candidate->cost
                 << " top_structure_cost=" << top_structure_cost
                 << " structure_cost=" << candidate->structure_cost
                 << " lid=" << candidate->lid << " rid=" << candidate->rid;
    if (candidate_size < kStopEnmerationCacheSize) {
      // Even when the current candidate is classified as bad candidate,
      // we don't return STOP_ENUMERATION here.
      // When the current candidate is removed only with the "structure_cost",
      // there might exist valid candidates just after the current candidate.
      // We don't want to miss them.
      MOZC_CANDIDATE_LOG(candidate, "invalid cost");
      return CandidateFilter::BAD_CANDIDATE;
    } else {
      return CandidateFilter::STOP_ENUMERATION;
    }
  }

  // Filters out candidates with higher cost structure.
  if (top_structure_cost + kStructureCostOffset > INT_MAX ||
      std::max(top_structure_cost,
               static_cast<int64_t>(kMinStructureCostOffset)) +
              kStructureCostOffset <
          candidate->structure_cost) {
    // We don't stop enumeration here. Just drops high cost structure
    // looks enough.
    // |top_structure_cost| can be so small especially for compound or
    // web dictionary entries.
    // For avoiding over filtering, we use kMinStructureCostOffset if
    // |top_structure_cost| is small.
    MOZC_VLOG(2) << "structure cost is invalid:  " << candidate->value << " "
                 << candidate->content_value << " " << candidate->structure_cost
                 << " " << candidate->cost;
    MOZC_CANDIDATE_LOG(candidate, "structure cost is invalid");
    return CandidateFilter::BAD_CANDIDATE;
  }

  // Filters multiple number nodes.
  // "2|十三重"
  // "4|重|5|号室"
  // Note that we do not want to filter
  // "1|0|円"
  // "5|万" for the key, "5まん"
  if (nodes.size() >= 2) {
    int number_nodes = 0;
    uint16_t prev_lid = 0;
    for (const auto &node : nodes) {
      if (Util::IsScriptType(node->key, Util::NUMBER)) {
        continue;
      }
      const absl::string_view value = node->value;
      size_t mblen = 0;
      const Util::ScriptType first_value_script_type =
          Util::GetFirstScriptType(value, &mblen);
      if (first_value_script_type == Util::NUMBER && prev_lid != node->lid) {
        ++number_nodes;
      } else if (first_value_script_type == Util::KANJI) {
        const absl::string_view first_kanji = value.substr(0, mblen);
        const std::string converted =
            NumberUtil::KanjiNumberToArabicNumber(first_kanji);
        if (first_kanji != converted && prev_lid != node->lid) {
          ++number_nodes;
        }
      }
      prev_lid = node->lid;
    }
    if (number_nodes >= 2) {
      MOZC_CANDIDATE_LOG(candidate, "multile number node");
      return CandidateFilter::BAD_CANDIDATE;
    }
  }
  return CandidateFilter::GOOD_CANDIDATE;
}

CandidateFilter::ResultType CandidateFilter::FilterCandidate(
    const ConversionRequest &request, const absl::string_view original_key,
    const Candidate *candidate, const absl::Span<const Node *const> top_nodes,
    const absl::Span<const Node *const> nodes) {
  if (request.request_type() == ConversionRequest::REVERSE_CONVERSION) {
    // In reverse conversion, only remove duplicates because the filtering
    // criteria of FilterCandidateInternal() are completely designed for
    // (forward) conversion.
    const bool inserted = seen_.insert(CandidateId(*candidate)).second;
    return inserted ? GOOD_CANDIDATE : BAD_CANDIDATE;
  } else {
    const ResultType result = FilterCandidateInternal(
        request, original_key, candidate, top_nodes, nodes);
    if (result != GOOD_CANDIDATE) {
      return result;
    }
    seen_.insert(CandidateId(*candidate));
    return result;
  }
}

}  // namespace converter
}  // namespace mozc
