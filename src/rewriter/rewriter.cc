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

#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include "base/util.h"
#include "converter/segments.h"
#include "rewriter/collocation_rewriter.h"
#include "rewriter/rewriter.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/date_rewriter.h"
#include "rewriter/number_rewriter.h"
#include "rewriter/single_kanji_rewriter.h"
#include "rewriter/symbol_rewriter.h"
#include "rewriter/user_segment_history_rewriter.h"
#include "rewriter/user_boundary_history_rewriter.h"
#include "rewriter/version_rewriter.h"

namespace mozc {

Rewriter::Rewriter() {
  rewriters_.push_back(new SingleKanjiRewriter);
  rewriters_.push_back(new SymbolRewriter);
  rewriters_.push_back(new NumberRewriter);
  rewriters_.push_back(new CollocationRewriter);
  rewriters_.push_back(new DateRewriter);
  rewriters_.push_back(new UserBoundaryHistoryRewriter);
  rewriters_.push_back(new UserSegmentHistoryRewriter);
  rewriters_.push_back(new VersionRewriter);
}

Rewriter::~Rewriter() {
  for (size_t i = 0; i < rewriters_.size(); ++i) {
    delete rewriters_[i];
  }
  rewriters_.clear();
}

void Rewriter::Finish(Segments *segments) {
  for (size_t i = 0; i < rewriters_.size(); ++i) {
    rewriters_[i]->Finish(segments);
  }
}

bool Rewriter::Rewrite(Segments *segments) const {
  bool result = false;
  for (size_t i = 0; i < rewriters_.size(); ++i) {
    result |= rewriters_[i]->Rewrite(segments);
  }
  return result;
}

bool Rewriter::Sync() {
  bool result = false;
  for (size_t i = 0; i < rewriters_.size(); ++i) {
    result |= rewriters_[i]->Sync();
  }
  return result;
}

void Rewriter::Clear() {
  for (size_t i = 0; i < rewriters_.size(); ++i) {
    rewriters_[i]->Clear();
  }
}
}  // namespace mozc
