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

#include "base/singleton.h"
#include "rewriter/calculator_rewriter.h"
#include "rewriter/collocation_rewriter.h"
#include "rewriter/command_rewriter.h"
#include "rewriter/date_rewriter.h"
#include "rewriter/dice_rewriter.h"
#include "rewriter/emoticon_rewriter.h"
#include "rewriter/english_variants_rewriter.h"
#include "rewriter/focus_candidate_rewriter.h"
#include "rewriter/fortune_rewriter.h"
#include "rewriter/merger_rewriter.h"
#include "rewriter/number_rewriter.h"
#include "rewriter/normalization_rewriter.h"
#include "rewriter/remove_redundant_candidate_rewriter.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/single_kanji_rewriter.h"
#include "rewriter/symbol_rewriter.h"
#include "rewriter/transliteration_rewriter.h"
#include "rewriter/unicode_rewriter.h"
#include "rewriter/user_boundary_history_rewriter.h"
#include "rewriter/user_segment_history_rewriter.h"
#include "rewriter/variants_rewriter.h"
#include "rewriter/version_rewriter.h"
#include "rewriter/zipcode_rewriter.h"
#if defined(OS_MACOSX) || defined(OS_WINDOWS) || defined(OS_CHROMEOS)
// TODO(horo): Usage is available only in Mac and Windows and ChromeOS now.
#include "rewriter/usage_rewriter.h"
#endif
DEFINE_bool(use_history_rewriter, true, "Use history rewriter or not.");

namespace mozc {
namespace {

class RewriterImpl : public MergerRewriter {
 public:
  RewriterImpl();
};

RewriterImpl::RewriterImpl() {
  AddRewriter(new FocusCandidateRewriter);
  AddRewriter(new TransliterationRewriter);
  AddRewriter(new EnglishVariantsRewriter);
  AddRewriter(new NumberRewriter);
  AddRewriter(new CollocationRewriter);
  AddRewriter(new SingleKanjiRewriter);
  AddRewriter(new EmoticonRewriter);
  AddRewriter(new CalculatorRewriter);
  AddRewriter(new SymbolRewriter);
  AddRewriter(new UnicodeRewriter);
  AddRewriter(new VariantsRewriter);
  AddRewriter(new ZipcodeRewriter);
  AddRewriter(new DiceRewriter);

  if (FLAGS_use_history_rewriter) {
    AddRewriter(new UserBoundaryHistoryRewriter);
    AddRewriter(new UserSegmentHistoryRewriter);
  }
  AddRewriter(new DateRewriter);
  AddRewriter(new FortuneRewriter);
//  AddRewriter(new CommandRewriter);
  AddRewriter(new VersionRewriter);
#if defined(OS_MACOSX) || defined(OS_WINDOWS) || defined(OS_CHROMEOS)
  // TODO(horo): Because infolist renderer window is implimented
  //             only in Mac and Windows and ChromeOS,
  //             usage is available only in these OS.
  AddRewriter(new UsageRewriter);
#endif  // defined(OS_MACOSX) || defined(OS_WINDOWS) || defined(OS_CHROMEOS)

  AddRewriter(new NormalizationRewriter);
  AddRewriter(new RemoveRedundantCandidateRewriter);
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
