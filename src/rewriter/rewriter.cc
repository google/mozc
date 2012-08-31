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

#include "rewriter/rewriter.h"

#include "base/logging.h"
#include "converter/converter_interface.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/calculator_rewriter.h"
#include "rewriter/collocation_rewriter.h"
#include "rewriter/command_rewriter.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/date_rewriter.h"
#include "rewriter/dice_rewriter.h"
#include "rewriter/emoticon_rewriter.h"
#include "rewriter/english_variants_rewriter.h"
#include "rewriter/focus_candidate_rewriter.h"
#include "rewriter/fortune_rewriter.h"
#include "rewriter/merger_rewriter.h"
#include "rewriter/normalization_rewriter.h"
#include "rewriter/number_rewriter.h"
#include "rewriter/remove_redundant_candidate_rewriter.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/single_kanji_rewriter.h"
#include "rewriter/symbol_rewriter.h"
#include "rewriter/transliteration_rewriter.h"
#include "rewriter/unicode_rewriter.h"
#include "rewriter/user_boundary_history_rewriter.h"
#include "rewriter/user_dictionary_rewriter.h"
#include "rewriter/user_segment_history_rewriter.h"
#include "rewriter/variants_rewriter.h"
#include "rewriter/version_rewriter.h"
#include "rewriter/zipcode_rewriter.h"
#ifdef USE_USAGE_REWRITER
#include "rewriter/usage_rewriter.h"
#endif  // USE_USAGE_REWRITER

DEFINE_bool(use_history_rewriter, true, "Use history rewriter or not.");

namespace mozc {

RewriterImpl::RewriterImpl(const ConverterInterface *parent_converter,
                           const DataManagerInterface *data_manager)
    : parent_converter_(parent_converter),
      pos_matcher_(data_manager->GetPOSMatcher()),
      pos_group_(data_manager->GetPosGroup()) {
  DCHECK(parent_converter_);
  DCHECK(pos_matcher_);
  DCHECK(pos_group_);
#ifndef __native_client__
  AddRewriter(new UserDictionaryRewriter);
#endif  // __native_client__
  AddRewriter(new FocusCandidateRewriter);
  AddRewriter(new TransliterationRewriter(*pos_matcher_));
  AddRewriter(new EnglishVariantsRewriter);
  AddRewriter(new NumberRewriter(pos_matcher_));
  AddRewriter(new CollocationRewriter(data_manager));
  AddRewriter(new SingleKanjiRewriter(*pos_matcher_));
  AddRewriter(new EmoticonRewriter);
  AddRewriter(new CalculatorRewriter(parent_converter_));
  AddRewriter(new SymbolRewriter(parent_converter_));
  AddRewriter(new UnicodeRewriter(parent_converter_));
  AddRewriter(new VariantsRewriter(pos_matcher_));
  AddRewriter(new ZipcodeRewriter(pos_matcher_));
  AddRewriter(new DiceRewriter);

#ifndef __native_client__
  if (FLAGS_use_history_rewriter) {
    AddRewriter(new UserBoundaryHistoryRewriter(parent_converter_));
    AddRewriter(new UserSegmentHistoryRewriter(pos_matcher_, pos_group_));
  }
#endif  // __native_client__

  AddRewriter(new DateRewriter);
  AddRewriter(new FortuneRewriter);
#ifndef OS_ANDROID
  // CommandRewriter is not tested well on Android.
  // So we temporarily disable it.
  // TODO(yukawa, team): Enable CommandRewriter on Android if necessary.
  AddRewriter(new CommandRewriter);
#endif  // OS_ANDROID
#ifdef USE_USAGE_REWRITER
  AddRewriter(new UsageRewriter(pos_matcher_));
#endif  // USE_USAGE_REWRITER

  AddRewriter(new VersionRewriter);
  AddRewriter(CorrectionRewriter::CreateCorrectionRewriter(data_manager));
  AddRewriter(new NormalizationRewriter);
  AddRewriter(new RemoveRedundantCandidateRewriter);
}

}  // namespace mozc
