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

#ifndef MOZC_CONVERTER_IMMUTABLE_DECODER_H_
#define MOZC_CONVERTER_IMMUTABLE_DECODER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "converter/candidate.h"
#include "converter/connector.h"
#include "converter/lattice.h"
#include "converter/node_list_builder.h"
#include "converter/segmenter.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "engine/modules.h"
#include "prediction/result.h"
#include "prediction/suggestion_filter.h"
#include "request/options.h"

namespace mozc {

// ImmutableDecoder is a simplified version of ImmutableConverter that does not
// depend on the legacy `Segments` class.
//
// Current Status:
// - This class is not yet used in production.
// - It is planned to replace RealtimeDecoder and the PREDICTION/SUGGESTION
//    modes of ImmutableConverter.
// - We are currently verifying the result identity (compatibility) and
//    benchmarking the performance against the existing converter.
//
// Future Plans:
// - Uses this class both in CONVERSION and PREDICTION/SUGGESTION.
// - Implement caching or node contraction optimizations (like BestMap) to
//   improve efficiency and close the 5% performance gap.
// - Integrate more advanced decoding algorithms, e.g., n-gram or neural
//   language models.
class ImmutableDecoder {
 public:
  explicit ImmutableDecoder(const engine::Modules& modules);
  ~ImmutableDecoder() = default;

  ImmutableDecoder(const ImmutableDecoder&) = delete;
  ImmutableDecoder& operator=(const ImmutableDecoder&) = delete;

  // TODO(taku, b/505595752): Eventually we want to use this method (or a
  // variation of it) instead of ImmutableConverter::Convert in CONVERSION mode.
  //
  // Differences and Limitations:
  // - Correctness: Even ignoring segment boundary differences, the final
  //   literal results can differ because:
  //   - ImmutableDecoder does not integrate KeyCorrector, so it lacks typing
  //     corrections (e.g., Roman-to-Kana correction nodes) in the lattice.
  //   - The Viterbi search in ImmutableDecoder is simplified and does not
  //     support all conversion constraints (e.g., weak connections between POS
  //     tags).
  // - Performance: ImmutableDecoder::Decode is about 5-6% slower than
  //   RealtimeDecoder::Decode (which uses ImmutableConverter). The main
  //   bottleneck is that ImmutableDecoder's Viterbi search (ViterbiInternal)
  //   does not implement node contraction (using BestMap) to reduce calls
  //   to Connector::GetTransitionCost.
  //
  // Decodes the given key with history result and options.
  // Returns a list of prediction results.
  [[nodiscard]] std::vector<prediction::Result> Decode(
      absl::string_view key, const ConversionOptions& options,
      const prediction::Result& history_result) const;

 private:
  friend class ImmutableDecoderTestPeer;

  // Fills the lattice with history nodes and conversion nodes.
  // |history_key_size| is an output parameter that receives the byte length of
  // the history key inserted into the lattice.
  // Returns false if it fails to make lattice.
  bool MakeLattice(absl::string_view key, const ConversionOptions& options,
                   const prediction::Result& history_result,
                   Lattice* absl_nonnull lattice,
                   size_t& history_key_size) const;

  // Inserts history nodes into the lattice based on history_result.
  bool MakeLatticeForHistory(const prediction::Result& history_result,
                             const ConversionOptions& options,
                             Lattice* absl_nonnull lattice) const;

  // Inserts conversion nodes into the lattice for the key part.
  void MakeLatticeForConversion(const ConversionOptions& options,
                                size_t history_key_size,
                                Lattice* absl_nonnull lattice) const;

  // Runs Viterbi search on the lattice.
  bool Viterbi(Lattice* absl_nonnull lattice) const;

  // Internal implementation of Viterbi search for a range.
  // Runs Viterbi from |calc_begin_pos| to |calc_end_pos| (inclusive).
  void ViterbiInternal(int calc_begin_pos, int calc_end_pos,
                       Lattice* absl_nonnull lattice) const;

  // Looks up dictionary and returns a list of nodes at |pos|.
  // Note: This method modifies |lattice| by allocating new nodes on its
  // node allocator.
  std::vector<Node*> Lookup(size_t pos, const ConversionOptions& options,
                            bool is_reverse,
                            Lattice* absl_nonnull lattice) const;

  // Adds nodes based on character types (e.g. numbers, alphabets).
  void AddCharacterTypeBasedNodes(
      absl::string_view key_substr, Lattice* absl_nonnull lattice,
      BaseNodeListBuilder* absl_nonnull builder) const;

  // Fixes for "好む" vs "この|無", "大|代" vs "代々" preferences.
  // If the last node ends with "prefix", give an extra
  // wcost penalty. In this case  "無" doesn't tend to appear at
  // user input.
  void ApplyPrefixSuffixPenalty(const ConversionOptions& options,
                                absl::string_view conversion_key,
                                Lattice* absl_nonnull lattice) const;

  // Converts a converter::Candidate to a prediction::Result.
  prediction::Result CandidateToResult(
      absl::string_view key, const converter::Candidate& candidate) const;

  // Helper for Decode when partial candidates are not requested.
  std::vector<prediction::Result> DecodeWithoutPartial(
      const ConversionOptions& options, absl::string_view key,
      const Lattice& lattice, const Node* absl_nonnull prev) const;

  // Helper for Decode when partial candidates are requested.
  std::vector<prediction::Result> DecodeWithPartial(
      const ConversionOptions& options, absl::string_view key,
      const Lattice& lattice, const Node* absl_nonnull prev) const;

  // Inserts placeholder candidates (e.g. Hiragana/Katakana) to fill the list.
  void InsertPlaceHolderCandidates(
      absl::string_view key, size_t expand_size,
      std::vector<std::unique_ptr<converter::Candidate>>* absl_nonnull
          candidates) const;

  const dictionary::DictionaryInterface& dictionary_;
  const dictionary::UserDictionaryInterface& user_dictionary_;
  const Connector& connector_;
  const Segmenter& segmenter_;
  const dictionary::PosMatcher& pos_matcher_;
  const dictionary::PosGroup& pos_group_;
  const SuggestionFilter& suggestion_filter_;

  // Cache for POS ids.
  const uint16_t first_name_id_;
  const uint16_t last_name_id_;
  const uint16_t number_id_;
  const uint16_t unknown_id_;

  // Cache for transition cost.
  const int32_t last_to_first_name_transition_cost_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_IMMUTABLE_DECODER_H_
