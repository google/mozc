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

#ifndef MOZC_REWRITER_USER_SEGMENT_HISTORY_REWRITER_H_
#define MOZC_REWRITER_USER_SEGMENT_HISTORY_REWRITER_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "storage/lru_cache.h"
#include "storage/lru_storage.h"

namespace mozc {

class UserSegmentHistoryRewriter : public RewriterInterface {
 public:
  UserSegmentHistoryRewriter(const dictionary::PosMatcher& pos_matcher,
                             const dictionary::PosGroup& pos_group);

  bool Rewrite(const ConversionRequest& request,
               Segments* segments) const override;

  void Finish(const ConversionRequest& request,
              const Segments& segments) override;
  bool Sync() override;
  bool Reload() override;
  void Clear() override;
  void Revert(const Segments& segments) override;
  bool ClearHistoryEntry(const Segments& segments, size_t segment_index,
                         int candidate_index) override;

 private:
  friend class UserSegmentHistoryRewriterTestPeer;

  struct Score {
    constexpr void Update(const Score other) {
      score = std::max(score, other.score);
      last_access_time = std::max(last_access_time, other.last_access_time);
    }

    friend constexpr bool operator>(const Score a, const Score b) {
      if (a.score == b.score) {
        return a.last_access_time > b.last_access_time;
      }
      return a.score > b.score;
    }

    uint32_t score, last_access_time;
  };

  struct ScoreCandidate : public Score {
    ScoreCandidate(const Score s, const converter::Candidate* candidate)
        : Score(s), candidate(candidate) {}

    const converter::Candidate* candidate;
  };

  static Segments MakeLearningSegmentsFromInnerSegments(
      const ConversionRequest& request, const Segments& segments);

  bool IsAvailable(const ConversionRequest& request,
                   const Segments& segments) const;
  Score GetScore(const ConversionRequest& request, const Segments& segments,
                 size_t segment_index, int candidate_index) const;
  bool Replaceable(const ConversionRequest& request,
                   const converter::Candidate& best_candidate,
                   const converter::Candidate& target_candidate) const;
  // |revert_entries| will be stored to Segments and used to revert last
  // Finish() operation in Revert().
  void RememberFirstCandidate(const ConversionRequest& request,
                              const Segments& segments, size_t segment_index,
                              std::vector<std::string>& revert_entries);
  void RememberNumberPreference(const Segment& segment,
                                std::vector<std::string>& revert_entries);
  bool RewriteNumber(Segment* segment) const;
  bool ShouldRewrite(const Segment& segment, size_t* max_candidates_size) const;
  void InsertTriggerKey(const Segment& segment);
  bool IsPunctuation(const Segment& seg,
                     const converter::Candidate& candidate) const;
  bool SortCandidates(absl::Span<const ScoreCandidate> sorted_scores,
                      Segment* segment) const;
  Score Fetch(absl::string_view key, uint32_t weight) const;
  void Insert(absl::string_view key, bool force,
              std::vector<std::string>& revert_entries);
  void MaybeInsertRevertEntry(absl::string_view key,
                              std::vector<std::string>& revert_entries);
  // Returns true if deletion succeeded.
  bool DeleteEntry(absl::string_view key);

  std::unique_ptr<storage::LruStorage> storage_;
  const dictionary::PosMatcher* pos_matcher_;
  const dictionary::PosGroup* pos_group_;

  // Internal LRU cache to store reverted key.
  storage::LruCache<uint64_t, std::vector<std::string>> revert_cache_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_USER_SEGMENT_HISTORY_REWRITER_H_
