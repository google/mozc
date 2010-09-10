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
#include "base/singleton.h"
#include "converter/segments.h"
#include "rewriter/calculator_rewriter.h"
#include "rewriter/collocation_rewriter.h"
#include "rewriter/date_rewriter.h"
#include "rewriter/emoticon_rewriter.h"
#include "rewriter/fortune_rewriter.h"
#include "rewriter/number_rewriter.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/single_kanji_rewriter.h"
#include "rewriter/symbol_rewriter.h"
#include "rewriter/user_segment_history_rewriter.h"
#include "rewriter/user_boundary_history_rewriter.h"
#include "rewriter/version_rewriter.h"

namespace mozc {
namespace {

class RewriterImpl: public RewriterInterface {
 public:
  RewriterImpl();
  virtual ~RewriterImpl();

  // Rewrite request and/or result.
  // Can call converter if need be
  virtual bool Rewrite(Segments *segments) const;

  // Hook(s) for all mutable operations
  virtual void Finish(Segments *segments);

  virtual bool Sync();

  virtual void Clear();

 private:
  SingleKanjiRewriter *single_kanji_rewriter_;
  SymbolRewriter *symbol_rewriter_;
  NumberRewriter *number_rewriter_;
  CollocationRewriter *collocation_rewriter_;
  DateRewriter *date_rewriter_;
  UserBoundaryHistoryRewriter *user_boundary_history_rewriter_;
  UserSegmentHistoryRewriter  *user_segment_history_rewriter_;
  VersionRewriter *version_rewriter_;
  EmoticonRewriter *emoticon_rewriter_;
  CalculatorRewriter *calculator_rewriter_;
  FortuneRewriter *fortune_rewriter_;
  vector<RewriterInterface *> rewriters_;
};

RewriterImpl::RewriterImpl()
    : single_kanji_rewriter_(new SingleKanjiRewriter),
      symbol_rewriter_(new SymbolRewriter),
      number_rewriter_(new NumberRewriter),
      collocation_rewriter_(new CollocationRewriter),
      date_rewriter_(new DateRewriter),
      user_boundary_history_rewriter_(new UserBoundaryHistoryRewriter),
      user_segment_history_rewriter_(new UserSegmentHistoryRewriter),
      version_rewriter_(new VersionRewriter),
      emoticon_rewriter_(new EmoticonRewriter),
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
      calculator_rewriter_(new CalculatorRewriter),
#else
      calculator_rewriter_(NULL),
#endif
      fortune_rewriter_(new FortuneRewriter) {
  rewriters_.push_back(single_kanji_rewriter_);
  rewriters_.push_back(symbol_rewriter_);
  // Push back calculator_rewriter_ only if it is enabled.
  if (calculator_rewriter_) {
    rewriters_.push_back(calculator_rewriter_);
  }
  rewriters_.push_back(emoticon_rewriter_);
  rewriters_.push_back(fortune_rewriter_);
  rewriters_.push_back(number_rewriter_);
  rewriters_.push_back(collocation_rewriter_);
  rewriters_.push_back(date_rewriter_);
  rewriters_.push_back(user_boundary_history_rewriter_);
  rewriters_.push_back(user_segment_history_rewriter_);
  rewriters_.push_back(version_rewriter_);
}

RewriterImpl::~RewriterImpl() {
  delete single_kanji_rewriter_;
  delete symbol_rewriter_;
  delete number_rewriter_;
  delete collocation_rewriter_;
  delete date_rewriter_;
  delete user_boundary_history_rewriter_;
  delete user_segment_history_rewriter_;
  delete version_rewriter_;
  delete emoticon_rewriter_;
  delete fortune_rewriter_;
  delete calculator_rewriter_;
  rewriters_.clear();
}

void RewriterImpl::Finish(Segments *segments) {
  for (size_t i = 0; i < rewriters_.size(); ++i) {
    rewriters_[i]->Finish(segments);
  }
}

bool RewriterImpl::Rewrite(Segments *segments) const {
  bool result = false;
  for (size_t i = 0; i < rewriters_.size(); ++i) {
    result |= rewriters_[i]->Rewrite(segments);
  }
  return result;
}

bool RewriterImpl::Sync() {
  bool result = false;
  for (size_t i = 0; i < rewriters_.size(); ++i) {
    result |= rewriters_[i]->Sync();
  }
  return result;
}

void RewriterImpl::Clear() {
  for (size_t i = 0; i < rewriters_.size(); ++i) {
    rewriters_[i]->Clear();
  }
}

RewriterInterface *g_rewriter = NULL;
}  // namespace

RewriterInterface *RewriterFactory::GetRewriter() {
  if (g_rewriter == NULL) {
    return Singleton<RewriterImpl>::get();
  } else {
    return g_rewriter;
  }
}

void RewriterFactory::SetRewriter(RewriterInterface *rewriter) {
  g_rewriter = rewriter;
}
}  // namespace mozc
