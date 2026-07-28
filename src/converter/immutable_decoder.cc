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

#include "converter/immutable_decoder.h"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/container/trie.h"
#include "base/japanese_util.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "base/vlog.h"
#include "converter/attribute.h"
#include "converter/candidate.h"
#include "converter/caching_connector.h"
#include "converter/connector.h"
#include "converter/inner_segment.h"
#include "converter/lattice.h"
#include "converter/nbest_generator.h"
#include "converter/node.h"
#include "converter/node_list_builder.h"
#include "converter/segmenter.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "request/options.h"

namespace mozc {
using ::mozc::converter::Attribute;
using ::mozc::converter::Candidate;

namespace {

constexpr size_t kMaxCharLength = 1024;
constexpr int kMaxCost = 32767;

// Reasonably big cost. Cannot use INT_MAX because a new cost will be
// calculated based on kVeryBigCost.
// It is OK that kVeryBigCost is greater than kMaxCost (32767) because it is
// used as a sentinel/very large cost to discourage paths containing
// unknown/bad nodes, and we want it to be significantly larger than any valid
// path cost (which can be a sum of multiple kMaxCost's) without overflowing
// when added to other costs.
constexpr int kVeryBigCost = (INT_MAX >> 2);
constexpr int kDefaultNumberCost = 3000;
constexpr int kMaxNodesSize = 8192;

constexpr int kSingleSegmentCharCoverage = 12;
constexpr int kMaxCostDiffForFirstInnerSegment = 3107;

std::vector<absl::string_view> GetBoundaryInfo(const Candidate& c) {
  std::vector<absl::string_view> ret;
  ret.reserve(c.inner_segments().size());
  for (const auto& iter : c.inner_segments()) {
    ret.emplace_back(iter.GetKey());
  }
  return ret;
}

// Performs a simple full/half width normalization.
// If the key/value are digits, adds the last digit only in order
// to increase the coverage. It is enough to use that
// the prev context is a number. "123" -> "3" only.
std::pair<std::string, std::string> NormalizeKeyAndValue(
    const converter::InnerSegments::IteratorData& iter) {
  auto normalize = [](absl::string_view s) {
    return japanese_util::FullWidthAsciiToHalfWidthAscii(s);
  };
  std::string norm_key = normalize(iter.GetKey());
  std::string norm_value = normalize(iter.GetValue());
  // normalized key, value, content_key, and content_value are all the same.
  if (norm_key.size() > 1 && norm_key == norm_value &&
      norm_value == normalize(iter.GetContentValue()) &&
      norm_key == normalize(iter.GetContentKey()) &&
      Util::GetScriptType(norm_key) == Util::NUMBER &&
      absl::ascii_isdigit(norm_key.back())) {
    // Takes the last digit.
    norm_key = norm_key.back();
    norm_value = norm_key;
  }
  return {norm_key, norm_value};
}

// Helper class to filter out redundant or high-cost partial candidates
// during suggestion generation (DecodeWithPartial).
//
// Example:
//   Input key: "なまえは" (namaeha)
//   Already added (full candidate):
//     - "名前は" (key: "なまえは") -> Trie contains "名前は"
//
//   Checking partial candidates:
//     - "名前" (key: "なまえ", partial):
//       "名前" is not in Trie, and has no prefix in Trie.
//       -> Kept. Trie now contains "名前は", "名前".
//
//     - "名前は" (key: "なまえ", partial):
//       "名前は" is filtered because "名前" (already added) is a prefix.
//       -> Filtered.
//
//     - "名前はっ" (key: "なまえ", partial):
//       "名前はっ" is filtered because "名前" is a prefix.
//       -> Filtered.
class PartialCandidateFilter {
 public:
  // Initializes the filter with already added target candidates.
  // Populates the trie with their values and tracks the minimum cost of
  // Kanji candidates.
  PartialCandidateFilter(absl::Span<const Candidate* const> targets,
                         int cost_max_diff)
      : cost_max_diff_(cost_max_diff) {
    for (const Candidate* c : targets) {
      trie_.AddEntry(c->value, true);
      if (Util::ContainsScriptType(c->value, Util::KANJI)) {
        if (!min_cost_.has_value()) {
          min_cost_ = c->cost;
        } else {
          *min_cost_ = std::min(*min_cost_, c->cost);
        }
      }
    }
  }

  // Returns true if the candidate is suitable for insertion.
  // Filters out:
  // 1. Partial candidates whose values are extensions of already added
  //    candidate values (to avoid redundant, longer suggestions).
  // 2. Candidates whose cost is too high compared to the best Kanji candidate.
  bool IsGoodCandidate(const Candidate& c, absl::string_view target_key) {
    if (c.key.size() != target_key.size() && IsPrefixAdded(c.value)) {
      return false;
    }
    if (min_cost_.has_value() && c.cost - *min_cost_ > cost_max_diff_) {
      return false;
    }
    return true;
  }

  // Adds a new candidate's value to the trie and updates the minimum Kanji
  // cost if applicable.
  void AddEntry(const Candidate& c) {
    trie_.AddEntry(c.value, true);
    if (Util::ContainsScriptType(c.value, Util::KANJI)) {
      if (!min_cost_.has_value()) {
        min_cost_ = c.cost;
      } else {
        *min_cost_ = std::min(*min_cost_, c.cost);
      }
    }
  }

 private:
  // Checks if the given value contains a prefix that has already been added
  // to the trie.
  bool IsPrefixAdded(absl::string_view value) const {
    bool data;
    size_t key_length;
    return trie_.LongestMatch(value, &data, &key_length);
  }
  int cost_max_diff_;
  std::optional<int> min_cost_;
  Trie<bool> trie_;
};

}  // namespace

ImmutableDecoder::ImmutableDecoder(const engine::Modules& modules)
    : dictionary_(modules.GetDictionary()),
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

std::vector<prediction::Result> ImmutableDecoder::Decode(
    absl::string_view key, const ConversionOptions& options,
    const prediction::Result& history_result) const {
#if defined(__ANDROID__) || defined(_WIN32) || defined(__APPLE__)
  thread_local Lattice lattice;
#else   // defined(__ANDROID__) || defined(_WIN32) || defined(__APPLE__)
  Lattice lattice;
#endif  // defined(__ANDROID__) || defined(_WIN32) || defined(__APPLE__)

  size_t history_key_size = 0;
  if (!MakeLattice(key, options, history_result, &lattice, history_key_size)) {
    LOG(WARNING) << "could not make lattice";
    return {};
  }

  if (!Viterbi(&lattice)) {
    LOG(WARNING) << "viterbi failed";
    return {};
  }

  MOZC_VLOG(2) << lattice.DebugString();

  const Node* prev = lattice.bos_node();
  for (Node* node = lattice.bos_node()->next;
       node->next != nullptr && node->node_type == Node::HIS_NODE;
       node = node->next) {
    prev = node;
  }

  if (!options.create_partial_candidates) {
    return DecodeWithoutPartial(options, key, lattice, prev);
  } else {
    return DecodeWithPartial(options, key, lattice, prev);
  }
}

// The behavior of this function should be the the same as the function
// in realtime_decoder.cc
prediction::Result ImmutableDecoder::CandidateToResult(
    absl::string_view key, const Candidate& candidate) const {
  prediction::Result result;
  result.key = candidate.key;
  result.value = candidate.value;
  result.cost = candidate.cost;
  result.wcost = candidate.wcost;
  result.lid = candidate.lid;
  result.rid = candidate.rid;
  result.inner_segment_boundary = candidate.inner_segment_boundary;
  result.SetTypesAndTokenAttributes(converter::Attribute::REALTIME_CONVERSION,
                                    dictionary::Token::NONE);
  result.attributes |= converter::Attribute::NO_VARIANTS_EXPANSION;
  result.consumed_key_size = Util::CharsLen(candidate.key);
  // assume thet the decoder returns single segment in legacy behavior.
  if (candidate.key.size() < key.size()) {
    result.attributes |= converter::Attribute::PARTIALLY_KEY_CONSUMED;
  }
  result.attributes |= candidate.attributes;
  return result;
}

std::vector<prediction::Result> ImmutableDecoder::DecodeWithoutPartial(
    const ConversionOptions& options, absl::string_view key,
    const Lattice& lattice, const Node* absl_nonnull prev) const {
  auto run_decoder = [&](auto& conn) {
    using NBestGenerator = ::mozc::NBestGenerator<std::decay_t<decltype(conn)>>;
    std::vector<prediction::Result> results;
    const size_t max_candidates_size = options.max_conversion_candidates_size;
    NBestGenerator nbest_generator(user_dictionary_, segmenter_, conn,
                                   pos_matcher_, lattice, suggestion_filter_);
    const Node* end_node = lattice.eos_node();
    typename NBestGenerator::Options nbest_options;
    nbest_options.boundary_mode = NBestGenerator::ONLY_EDGE;
    nbest_options.candidate_mode |= NBestGenerator::FILL_INNER_SEGMENT_INFO;

    nbest_generator.Reset(*prev, *end_node, nbest_options);
    const size_t expand_size = std::clamp<size_t>(max_candidates_size, 1, 512);

    std::vector<std::unique_ptr<Candidate>> cands;
    cands.reserve(expand_size);
    for (size_t i = 0; i < expand_size; ++i) {
      auto candidate = std::make_unique<Candidate>();
      if (!nbest_generator.Next(options, key, *candidate)) {
        break;
      }
      cands.push_back(std::move(candidate));
    }
    InsertPlaceHolderCandidates(key, expand_size, &cands);

    results.reserve(cands.size());
    for (const std::unique_ptr<Candidate>& candidate : cands) {
      results.emplace_back(CandidateToResult(key, *candidate));
    }
    return results;
  };

  if (options.particle_omission_transition_cost_bonus == 0) {
    CachingConnector<false> conn(connector_, 0, pos_matcher_);
    return run_decoder(conn);
  } else {
    CachingConnector<true> conn(
        connector_, options.particle_omission_transition_cost_bonus,
        pos_matcher_);
    return run_decoder(conn);
  }
}

std::vector<prediction::Result> ImmutableDecoder::DecodeWithPartial(
    const ConversionOptions& options, absl::string_view key,
    const Lattice& lattice, const Node* absl_nonnull prev) const {
  auto run_decoder = [&](auto& conn) {
    using NBestGenerator = ::mozc::NBestGenerator<std::decay_t<decltype(conn)>>;
    // Algorithm summary for DecodeWithPartial (mobile/suggestion mode):
    // 1. Generate full-segment (single-segment) candidates (up to 3).
    // 2. Insert placeholder candidates (Hiragana/Katakana) for the full key.
    // 3. Filter single-segment candidates based on cost and character coverage.
    // 4. Generate first-inner-segment candidates (partial conversions).
    // 5. Apply suffix penalty to partial candidates.
    // 6. Filter and merge partial candidates into the target list, ensuring
    //    they are not redundant prefixes of already added candidates and their
    //    costs are within threshold.
    // 7. Convert target candidates to prediction results.
    std::vector<prediction::Result> results;
    const size_t max_candidates_size = options.max_conversion_candidates_size;

    auto get_candidates = [&](typename NBestGenerator::Options nbest_options,
                              size_t max_size, const Node* end_node) {
      std::vector<std::unique_ptr<Candidate>> cands;
      cands.reserve(max_size);
      NBestGenerator nbest_generator(user_dictionary_, segmenter_, conn,
                                     pos_matcher_, lattice,
                                     suggestion_filter_);
      nbest_generator.Reset(*prev, *end_node, nbest_options);
      for (size_t i = 0; i < max_size; ++i) {
        auto candidate = std::make_unique<Candidate>();
        if (!nbest_generator.Next(options, key, *candidate)) {
          break;
        }
        cands.push_back(std::move(candidate));
      }
      return cands;
    };

    typename NBestGenerator::Options single_segment_options;
    single_segment_options.boundary_mode = NBestGenerator::ONLY_EDGE;
    single_segment_options.candidate_mode |=
        NBestGenerator::FILL_INNER_SEGMENT_INFO;

    // Step 1 & 2: Generate full-segment (single-segment) candidates (up to 3)
    // and insert placeholder candidates (Hiragana/Katakana) for the full key.
    std::vector<std::unique_ptr<Candidate>> single_candidates =
        get_candidates(single_segment_options, 3, lattice.eos_node());
    InsertPlaceHolderCandidates(key, 3, &single_candidates);
    std::vector<const Candidate*> result_candidates;
    result_candidates.reserve(max_candidates_size);
    absl::flat_hash_set<absl::string_view> added;

    // Step 3: Filter single-segment candidates based on cost and character
    // coverage.
    if (!single_candidates.empty()) {
      const Candidate* top_cand = single_candidates.front().get();
      const std::vector<absl::string_view> top_boundary =
          GetBoundaryInfo(*top_cand);
      int remaining_char_coverage = kSingleSegmentCharCoverage;

      for (size_t i = 0; i < single_candidates.size(); ++i) {
        const Candidate* c = single_candidates[i].get();
        constexpr int kCostDiff = 2302;  // 500 * log(100)
        if (c->cost - top_cand->cost > kCostDiff) {
          // Skip candidates that are too expensive compared to the top candidate.
          continue;
        }
        if (i != 0 && GetBoundaryInfo(*c) == top_boundary &&
            remaining_char_coverage < 0) {
          // Skip similar candidates if we already covered enough characters
          // (12 chars).
          // This avoids flooding the suggestion list with minor variations.
          continue;
        }
        result_candidates.push_back(c);
        added.insert(c->value);
        remaining_char_coverage -= Util::CharsLen(c->value);
      }
    }

    typename NBestGenerator::Options first_inner_options;
    first_inner_options.boundary_mode = NBestGenerator::ONLY_EDGE;
    first_inner_options.candidate_mode |=
        NBestGenerator::BUILD_FROM_ONLY_FIRST_INNER_SEGMENT;
    first_inner_options.candidate_mode |=
        NBestGenerator::FILL_INNER_SEGMENT_INFO;

    size_t first_inner_limit =
        max_candidates_size > result_candidates.size()
            ? max_candidates_size - result_candidates.size()
            : 0;
    // Step 4: Generate first-inner-segment candidates (partial conversions).
    std::vector<std::unique_ptr<Candidate>> first_candidates = get_candidates(
        first_inner_options, first_inner_limit, lattice.eos_node());

    PartialCandidateFilter filter(result_candidates,
                                  kMaxCostDiffForFirstInnerSegment);
    for (const std::unique_ptr<Candidate>& c_ptr : first_candidates) {
      Candidate* c = c_ptr.get();
      // Step 5: Add suffix penalty for partial candidates since they don't end
      // at the end of the input key, and NBestGenerator doesn't add it for
      // non-terminal nodes.
      // TODO(all): we would like to use (added.insert(c->value).second), but this
      // code `continue` the block before `added.insert()`. Not sure this is
      // intentional or not.
      if (added.contains(c->value)) {
        continue;
      }
      if (c->key.size() != key.size()) {
        // Explicitly add suffix penalty, since the penalty is not added for non
        // end nodes.
        const int32_t suffix_penalty = segmenter_.GetSuffixPenalty(c->rid);
        c->wcost += suffix_penalty;
        c->cost += suffix_penalty;
      }
      // Step 6: Filter and merge partial candidates into the target list,
      // ensuring they are not redundant prefixes of already added candidates and
      // their costs are within threshold.
      if (!filter.IsGoodCandidate(*c, key)) {
        continue;
      }
      result_candidates.push_back(c);
      filter.AddEntry(*c);
      added.insert(c->value);
    }

    // Step 7: Convert target candidates to prediction results.
    results.reserve(result_candidates.size());
    for (const Candidate* candidate : result_candidates) {
      results.emplace_back(CandidateToResult(key, *candidate));
    }
    return results;
  };

  if (options.particle_omission_transition_cost_bonus == 0) {
    CachingConnector<false> conn(connector_, 0, pos_matcher_);
    return run_decoder(conn);
  } else {
    CachingConnector<true> conn(
        connector_, options.particle_omission_transition_cost_bonus,
        pos_matcher_);
    return run_decoder(conn);
  }
}

void ImmutableDecoder::InsertPlaceHolderCandidates(
    absl::string_view key, size_t expand_size,
    std::vector<std::unique_ptr<converter::Candidate>>* absl_nonnull candidates)
    const {
  if (candidates->empty() || candidates->size() >= expand_size) {
    return;
  }

  const Candidate* top_candidate = candidates->front().get();
  const Candidate* last_candidate = candidates->back().get();

  // 1. Insert a placeholder candidate whose content_value is katakana.
  if (!top_candidate->functional_key().empty() &&
      !top_candidate->content_key.empty() &&
      Util::GetScriptType(top_candidate->content_key) == Util::HIRAGANA) {
    auto new_candidate = std::make_unique<Candidate>(*top_candidate);
    new_candidate->content_value =
        japanese_util::HiraganaToKatakana(top_candidate->content_key);
    new_candidate->value = absl::StrCat(new_candidate->content_value,
                                        top_candidate->functional_value());
    new_candidate->cost = last_candidate->cost + 1;
    new_candidate->wcost = last_candidate->wcost + 1;
    new_candidate->structure_cost = last_candidate->structure_cost + 1;
    new_candidate->attributes = 0;
    new_candidate->inner_segment_boundary.clear();
    candidates->push_back(std::move(new_candidate));
    last_candidate = candidates->back().get();
    if (candidates->size() >= expand_size) {
      return;
    }
  }

  // 2. Insert a placeholder hiragana candidate.
  if (Util::GetScriptType(key) == Util::HIRAGANA) {
    auto new_candidate = std::make_unique<Candidate>(*last_candidate);
    new_candidate->inner_segment_boundary.clear();
    new_candidate->cost = last_candidate->cost + 1;
    new_candidate->wcost = last_candidate->wcost + 1;
    new_candidate->structure_cost = last_candidate->structure_cost + 1;
    new_candidate->key = key;
    new_candidate->value = key;
    new_candidate->content_key = key;
    new_candidate->content_value = key;
    new_candidate->attributes = 0;
    // One character hiragana/katakana will cause side effect.
    // Type "し" and choose "シ". After that, "しました" will become "シました".
    if (Util::CharsLen(new_candidate->key) <= 1) {
      new_candidate->attributes |= Attribute::CONTEXT_SENSITIVE;
    }
    candidates->push_back(std::move(new_candidate));
    last_candidate = candidates->back().get();
    if (candidates->size() >= expand_size) {
      return;
    }
  }

  // 3. Insert a placeholder katakana candidate.
  const std::string katakana_value = japanese_util::HiraganaToKatakana(key);
  if (Util::GetScriptType(katakana_value) == Util::KATAKANA) {
    auto new_candidate = std::make_unique<Candidate>(*last_candidate);
    new_candidate->cost = last_candidate->cost + 1;
    new_candidate->wcost = last_candidate->wcost + 1;
    new_candidate->structure_cost = last_candidate->structure_cost + 1;
    new_candidate->key = key;
    new_candidate->value = katakana_value;
    new_candidate->content_key = key;
    new_candidate->content_value = katakana_value;
    new_candidate->attributes = 0;
    new_candidate->inner_segment_boundary.clear();
    // One character hiragana/katakana will cause side effect.
    // Type "し" and choose "シ". After that, "しました" will become "シました".
    if (Util::CharsLen(new_candidate->key) <= 1) {
      new_candidate->attributes |= Attribute::CONTEXT_SENSITIVE;
    }
    candidates->push_back(std::move(new_candidate));
  }
}

bool ImmutableDecoder::MakeLattice(absl::string_view key,
                                   const ConversionOptions& options,
                                   const prediction::Result& history_result,
                                   Lattice* absl_nonnull lattice,
                                   size_t& history_key_size) const {
  // Normalize history.
  std::string history_key;
  for (const auto& iter : history_result.inner_segments()) {
    auto [norm_key, norm_value] = NormalizeKeyAndValue(iter);
    history_key.append(norm_key);
  }

  // rename `key` as `conversion_key` to keep constituency with history_key.
  absl::string_view conversion_key = key;
  if (conversion_key.empty() || conversion_key.size() >= kMaxCharLength) {
    LOG(WARNING) << "Conversion key is empty or too long: " << conversion_key;
    return false;
  }

  if (history_key.size() + conversion_key.size() >= kMaxCharLength) {
    LOG(WARNING) << "Clear history segments due to the limit of key length.";
    history_key.clear();
  }

  history_key_size = history_key.size();

  {
    std::string lattice_key = absl::StrCat(history_key, conversion_key);
    lattice->SetKey(std::move(lattice_key), options.bos_id);
  }

  bool is_valid_lattice = true;
  if (!MakeLatticeForHistory(history_result, options, lattice) ||
      lattice->end_nodes(history_key.size()).empty()) {
    is_valid_lattice = false;
  }

  if (is_valid_lattice) {
    MakeLatticeForConversion(options, history_key.size(), lattice);
  }

  if (!is_valid_lattice) {
    return false;
  }

  if (lattice->end_nodes(lattice->key().size()).empty()) {
    LOG(WARNING) << "cannot build lattice from input";
    return false;
  }

  ApplyPrefixSuffixPenalty(options, conversion_key, lattice);

  return true;
}

bool ImmutableDecoder::MakeLatticeForHistory(
    const prediction::Result& history_result, const ConversionOptions& options,
    Lattice* absl_nonnull lattice) const {
  const bool is_reverse =
      (options.request_type == RequestType::REVERSE_CONVERSION);
  size_t segments_pos = 0;
  uint16_t last_rid = 0;
  const converter::InnerSegments inner_segments =
      history_result.inner_segments();
  const size_t history_segments_size = inner_segments.size();

  size_t segment_index = 0;
  for (const auto& iter : inner_segments) {
    auto [norm_key, norm_value] = NormalizeKeyAndValue(iter);

    const bool is_last_history = segment_index + 1 == history_segments_size;

    Node* absl_nonnull rnode = lattice->NewNode();
    // the inner segments do not have lid/rid, so fills them heuristically.
    // uses the last node's rid. Otherwise, set 0 (BOS/EOS).
    rnode->lid = 0;
    rnode->rid = is_last_history ? history_result.rid : 0;
    rnode->wcost = 0;
    rnode->value = norm_value;
    rnode->key = norm_key;
    rnode->node_type = Node::HIS_NODE;
    lattice->Insert(segments_pos, rnode);

    // For the last history segment, we also insert a new node having
    // EOS part-of-speech. Viterbi algorithm will find the
    // best path from rnode(context) and rnode2(EOS).
    if (is_last_history && rnode->rid != 0) {
      Node* rnode2 = lattice->NewNode();
      rnode2->lid = rnode->lid;  // always 0.
      rnode2->rid = 0;           // EOS id
      rnode2->wcost = 0;
      rnode2->value = rnode->value;
      rnode2->key = rnode->key;
      rnode2->node_type = Node::HIS_NODE;
      lattice->Insert(segments_pos, rnode2);
    }

    // Dictionary lookup for the candidates which are
    // overlapping between history and conversion.
    // Check only the last history segment at this moment.
    //
    // Example: history "やまだ(山田)", conversion: "たろう"
    // Here, try to find "山田太郎(やまだたろう)" from dictionary
    // and insert "太郎" as a new word node with a modified cost
    //
    // Note: The overlapping lookup is disabled for prediction, because it can
    // produce noisy realtime candidates such as "て配" for the history "に",
    // which comes from "にて" + "配".
    // The bigram-like lookup ("太郎" from "山田") is covered in
    // dictionary_predictor.
    const bool is_prediction =
        (options.request_type == RequestType::SUGGESTION ||
         options.request_type == RequestType::PREDICTION);
    if (!is_prediction && segment_index + 1 == history_segments_size) {
      // find compound "やまだたろう"
      for (const Node* compound_node :
           Lookup(segments_pos, options, is_reverse, lattice)) {
        // No overlaps.
        if (compound_node->key.size() <= rnode->key.size() ||
            compound_node->value.size() <= rnode->value.size() ||
            !compound_node->key.starts_with(rnode->key) ||
            !compound_node->value.starts_with(rnode->value)) {
          continue;
        }

        // Compound lid has different POS group from BOS(0).
        if (pos_group_.GetPosGroup(0) !=
            pos_group_.GetPosGroup(compound_node->lid)) {
          continue;
        }

        // Get the suffix part ("たろう/太郎")
        Node* absl_nonnull new_node = lattice->NewNode();
        new_node->key.assign(compound_node->key, rnode->key.size(),
                             compound_node->key.size() - rnode->key.size());
        new_node->value.assign(
            compound_node->value, rnode->value.size(),
            compound_node->value.size() - rnode->value.size());

        // rid/lid are derived from the compound.
        // lid is just an approximation
        new_node->rid = compound_node->rid;
        new_node->lid = compound_node->lid;
        new_node->node_type = Node::NOR_NODE;
        new_node->attributes |= Attribute::CONTEXT_SENSITIVE;

        // Recompute the wcost based on the length.
        // reduce the internal transiton cost (やまだ→たろう)
        // compound_node->wcost * (candidate len / compound_node len)
        // - trans(candidate.rid, new_node.lid)
        new_node->wcost =
            compound_node->wcost * norm_value.size() /
                compound_node->value.size() -
            connector_.GetTransitionCost(rnode->rid, new_node->lid);

        new_node->constrained_prev = rnode;
        lattice->Insert(segments_pos + rnode->key.size(), new_node);
      }
    }

    segments_pos += rnode->key.size();
    last_rid = rnode->rid;
    segment_index++;
  }
  return true;
}

void ImmutableDecoder::MakeLatticeForConversion(
    const ConversionOptions& options, size_t history_key_size,
    Lattice* absl_nonnull lattice) const {
  absl::string_view key = lattice->key();
  const bool is_reverse =
      (options.request_type == RequestType::REVERSE_CONVERSION);

  for (size_t pos = history_key_size; pos < key.size(); ++pos) {
    if (lattice->end_nodes(pos).empty()) {
      continue;
    }

    std::vector<Node*> rnodes = Lookup(pos, options, is_reverse, lattice);
    // If history key is NOT empty and user input seems to starts with
    // a particle ("はにで..."), mark the node as STARTS_WITH_PARTICLE.
    // We change the segment boundary if STARTS_WITH_PARTICLE attribute
    // is assigned.
    if (history_key_size != 0 && pos == history_key_size) {
      for (Node* node : rnodes) {
        if (pos_matcher_.IsAcceptableParticleAtBeginOfSegment(node->lid) &&
            node->lid == node->rid) {  // not a compound.
          node->attributes |= Node::STARTS_WITH_PARTICLE;
        }
      }
    }
    lattice->Insert(pos, rnodes);
  }
}

bool ImmutableDecoder::Viterbi(Lattice* absl_nonnull lattice) const {
  const size_t key_length = lattice->key().size();

  // Run Viterbi search from BOS (0) to EOS (key_length) in a single pass.
  // In history range [0, history_key_size], only user-selected history nodes
  // are inserted without competing candidates. Thus, a standard topological
  // left-to-right pass naturally preserves the history path as the prefix
  // context for conversion without requiring separate Viterbi passes.
  ViterbiInternal(0, key_length, lattice);

  Node* absl_nonnull node = lattice->eos_node();
  Node* prev = nullptr;
  while (node->prev != nullptr) {
    prev = node->prev;
    prev->next = node;
    node = prev;
  }

  if (lattice->bos_node() != prev) {
    LOG(WARNING) << "cannot make lattice";
    return false;
  }

  return true;
}

// Runs Viterbi algorithm at the range between [calc_begin_pos, calc_end_pos].
void ImmutableDecoder::ViterbiInternal(int calc_begin_pos, int calc_end_pos,
                                       Lattice* absl_nonnull lattice) const {
  for (int pos = calc_begin_pos; pos <= calc_end_pos; ++pos) {
    for (Node* rnode : lattice->begin_nodes(pos)) {
      int32_t best_cost = kVeryBigCost;
      Node* best_node = nullptr;

      if (rnode->constrained_prev != nullptr) {
        Node* lnode = rnode->constrained_prev;
        if (lnode->cost < kVeryBigCost) {
          const int32_t transition_cost =
              connector_.GetTransitionCost(lnode->rid, rnode->lid);
          best_cost = lnode->cost + transition_cost;
          best_node = lnode;
        }
      } else {
        // Find a valid node which connects to the rnode with minimum cost
        for (Node* lnode : lattice->end_nodes(pos)) {
          if (lnode->cost >= kVeryBigCost) {
            continue;
          }
          const int32_t transition_cost =
              connector_.GetTransitionCost(lnode->rid, rnode->lid);
          const int32_t cost = lnode->cost + transition_cost;
          if (cost < best_cost) {
            best_cost = cost;
            best_node = lnode;
          }
        }
      }

      if (best_node != nullptr) {
        rnode->prev = best_node;
        rnode->cost = best_cost + rnode->wcost;
      } else {
        rnode->cost = kVeryBigCost;  // Not found.
      }
    }
  }
}

// Looks up the dictionary and character-based fallback rules.
// Note: This method modifies `lattice` by allocating new `Node` objects
// using its node allocator.
std::vector<Node*> ImmutableDecoder::Lookup(
    size_t begin_pos, const ConversionOptions& options, bool is_reverse,
    Lattice* absl_nonnull lattice) const {
  absl::string_view key = lattice->key();
  const absl::string_view key_substr =
      key.substr(std::min<size_t>(begin_pos, key.size()));

  BaseNodeListBuilder builder(lattice->node_allocator(), kMaxNodesSize);
  if (is_reverse) {
    dictionary_.LookupReverse(key_substr, options, &builder);
  } else {
    dictionary_.LookupPrefix(key_substr, options, &builder);
  }
  AddCharacterTypeBasedNodes(key_substr, lattice, &builder);

  return builder.result();
}

void ImmutableDecoder::AddCharacterTypeBasedNodes(
    absl::string_view key_substr, Lattice* absl_nonnull lattice,
    BaseNodeListBuilder* absl_nonnull builder) const {
  const Utf8AsChars32 utf8_as_chars32(key_substr);
  Utf8AsChars32::const_iterator it = utf8_as_chars32.begin();
  DCHECK_NE(it, utf8_as_chars32.end());
  const char32_t codepoint = it.char32();

  const Util::ScriptType first_script_type = Util::GetScriptType(codepoint);
  const Util::FormType first_form_type = Util::GetFormType(codepoint);

  {
    // Add 1 character node. It can be either UnknownId or NumberId.
    Node* absl_nonnull new_node = lattice->NewNode();
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
    builder->AppendToResult(new_node);

    if (first_script_type == Util::NUMBER) {
      new_node->wcost = kDefaultNumberCost;
      return;
    }

    if (first_script_type != Util::ALPHABET &&
        first_script_type != Util::KATAKANA) {
      return;
    }
  }

  // group by same char type
  int num_char = 1;
  DCHECK_NE(it, utf8_as_chars32.end());
  for (++it; it != utf8_as_chars32.end(); ++it, ++num_char) {
    const char32_t next_codepoint = it.char32();
    if (first_script_type != Util::GetScriptType(next_codepoint) ||
        first_form_type != Util::GetFormType(next_codepoint)) {
      break;
    }
  }

  if (num_char > 1) {
    Node* absl_nonnull new_node = lattice->NewNode();
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
    builder->AppendToResult(new_node);
  }
}

void ImmutableDecoder::ApplyPrefixSuffixPenalty(
    const ConversionOptions& options, absl::string_view conversion_key,
    Lattice* absl_nonnull lattice) const {
  const absl::string_view key = lattice->key();
  DCHECK_LE(conversion_key.size(), key.size());

  if (!options.disable_prefix_penalty) {
    for (Node* node :
         lattice->begin_nodes(key.size() - conversion_key.size())) {
      // TODO(taku):
      // We might be able to tweak the penalty according to
      // the size of history segments.
      // If history-segments is non-empty, we can make the
      // penalty smaller so that history context is more likely
      // selected.
      node->wcost += segmenter_.GetPrefixPenalty(node->lid);
    }
  }

  for (Node* node : lattice->end_nodes(key.size())) {
    node->wcost += segmenter_.GetSuffixPenalty(node->rid);
  }
}

}  // namespace mozc
