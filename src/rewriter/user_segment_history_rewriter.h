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

#ifndef MOZC_REWRITER_USER_SEGMENT_HISTORY_REWRITER_H_
#define MOZC_REWRITER_USER_SEGMENT_HISTORY_REWRITER_H_

#include <string>
#include <vector>

#include "rewriter/rewriter_interface.h"
#include "base/base.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"

namespace mozc {
class ConversionRequest;
class LRUStorage;
class POSMatcher;
class PosGroup;

class UserSegmentHistoryRewriter : public RewriterInterface {
 public:
  struct ScoreType {
    uint32 last_access_time;
    uint32 score;
    const Segment::Candidate *candidate;
  };

  UserSegmentHistoryRewriter(const POSMatcher *pos_matcher,
                             const PosGroup *pos_group);
  virtual ~UserSegmentHistoryRewriter();

  virtual bool Rewrite(const ConversionRequest &request,
                       Segments *segments) const;

  virtual void Finish(Segments *segments);

  virtual bool Reload();

  virtual void Clear();

  // Return the current active storage.
  // UserSegmentHistoryRewriter is basically singleton in converter thread.
  // GetStorage() returns the current active storage managed by converter.
  // TODO(taku): This design is ad-hoc. Needs refactoring.
  static LRUStorage *GetStorage();

 private:
  bool IsAvailable(const Segments &segments) const;
  bool GetScore(const Segments &segments,
                size_t segment_index,
                int candidate_index,
                uint32 *score,
                uint32 *last_access_time) const;
  bool Replaceable(const Segment::Candidate &lhs,
                   const Segment::Candidate &rhs) const;
  void RememberFirstCandidate(const Segments &segments,
                              size_t segment_index);
  void RememberNumberPreference(const Segment &segment);
  bool RewriteNumber(Segment *segment) const;
  bool ShouldRewrite(const Segment &segment,
                     size_t *max_candidates_size) const;
  void InsertTriggerKey(const Segment &segment);
  bool IsPunctuation(const Segment &seg,
                     const Segment::Candidate &candidate) const;
  bool GetFeatureLN(const Segments &segments,
                    size_t i,
                    const string &base_key,
                    const string &base_value,
                    string *value) const;
  bool GetFeatureRN(const Segments &segments,
                    size_t i,
                    const string &base_key,
                    const string &base_value,
                    string *value) const;
  bool SortCandidates(const vector<ScoreType> &sorted_scores,
                      Segment *segment) const;


  scoped_ptr<LRUStorage> storage_;
  const POSMatcher *pos_matcher_;
  const PosGroup *pos_group_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_USER_SEGMENT_HISTORY_REWRITER_H_
