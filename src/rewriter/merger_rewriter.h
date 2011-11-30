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

#ifndef MOZC_REWRITER_MERGER_REWRITER_H_
#define MOZC_REWRITER_MERGER_REWRITER_H_

#include <vector>

#include "base/base.h"
#include "base/stl_util.h"
#include "converter/segments.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class MergerRewriter : public RewriterInterface {
 public:
  MergerRewriter() {}
  virtual ~MergerRewriter() {
    STLDeleteElements(&rewriters_);
  }

  // return true if rewriter can be called with the segments.
  bool CheckCapablity(Segments *segments,
                      RewriterInterface *rewriter) const {
    if (segments == NULL) {
      return false;
    }
    switch (segments->request_type()) {
      case Segments::CONVERSION:
        return (rewriter->capability() & RewriterInterface::CONVERSION);

      case Segments::PREDICTION:
      case Segments::PARTIAL_PREDICTION:
        return (rewriter->capability() & RewriterInterface::PREDICTION);

      case Segments::SUGGESTION:
      case Segments::PARTIAL_SUGGESTION:
        return (rewriter->capability() & RewriterInterface::SUGGESTION);

      case Segments::REVERSE_CONVERSION:
      default:
        return false;
    }
  }

  // This instance owns the rewriter.
  void AddRewriter(RewriterInterface *rewriter) {
    rewriters_.push_back(rewriter);
  }

  // Rewrites request and/or result.
  virtual bool Rewrite(Segments *segments) const {
    bool result = false;
    for (size_t i = 0; i < rewriters_.size(); ++i) {
      if (CheckCapablity(segments, rewriters_[i])) {
        result |= rewriters_[i]->Rewrite(segments);
      }
    }
    return result;
  }

  // This method is mainly called when user puts SPACE key
  // and changes the focused candidate.
  // In this method, Converter will find bracketing matching.
  // e.g., when user selects "「",  corresponding closing bracket "」"
  // is chosen in the preedit.
  virtual bool Focus(Segments *segments,
                     size_t segment_index,
                     int candidate_index) const {
    bool result = false;
    for (size_t i = 0; i < rewriters_.size(); ++i) {
      result |= rewriters_[i]->Focus(segments,
                                     segment_index,
                                     candidate_index);
    }
    return result;
  }

  // Hook(s) for all mutable operations
  virtual void Finish(Segments *segments) {
    for (size_t i = 0; i < rewriters_.size(); ++i) {
      rewriters_[i]->Finish(segments);
    }
  }

  // Syncs internal data to local file system.
  virtual bool Sync() {
    bool result = false;
    for (size_t i = 0; i < rewriters_.size(); ++i) {
      result |= rewriters_[i]->Sync();
    }
    return result;
  }

  // Reloads internal data from local file system.
  virtual bool Reload() {
    bool result = false;
    for (size_t i = 0; i < rewriters_.size(); ++i) {
      result |= rewriters_[i]->Reload();
    }
    return result;
  }

  // Clears internal data
  virtual void Clear() {
    for (size_t i = 0; i < rewriters_.size(); ++i) {
      rewriters_[i]->Clear();
    }
  }

 private:
  vector<RewriterInterface *> rewriters_;

  DISALLOW_COPY_AND_ASSIGN(MergerRewriter);
};

}  // mozc

#endif  // MOZC_REWRITER_MERGER_REWRITER_H_
