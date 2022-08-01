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

#include "rewriter/rewriter.h"

#include <memory>

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
#include "rewriter/emoji_rewriter.h"
#include "rewriter/emoticon_rewriter.h"
#include "rewriter/english_variants_rewriter.h"
#include "rewriter/focus_candidate_rewriter.h"
#include "rewriter/fortune_rewriter.h"
#include "rewriter/katakana_promotion_rewriter.h"
#include "rewriter/language_aware_rewriter.h"
#include "rewriter/merger_rewriter.h"
#include "rewriter/normalization_rewriter.h"
#include "rewriter/number_rewriter.h"
#include "rewriter/remove_redundant_candidate_rewriter.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/single_kanji_rewriter.h"
#include "rewriter/small_letter_rewriter.h"
#include "rewriter/symbol_rewriter.h"
#include "rewriter/transliteration_rewriter.h"
#include "rewriter/unicode_rewriter.h"
#include "rewriter/user_boundary_history_rewriter.h"
#include "rewriter/user_dictionary_rewriter.h"
#include "rewriter/user_segment_history_rewriter.h"
#include "rewriter/variants_rewriter.h"
#include "rewriter/version_rewriter.h"
#include "rewriter/zipcode_rewriter.h"
#include "absl/flags/flag.h"
#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter.h"
#endif  // NO_USAGE_REWRITER

ABSL_FLAG(bool, use_history_rewriter, true, "Use history rewriter or not.");

namespace mozc {
namespace {

using dictionary::DictionaryInterface;
using dictionary::PosGroup;

}  // namespace

RewriterImpl::RewriterImpl(const ConverterInterface *parent_converter,
                           const DataManagerInterface *data_manager,
                           const PosGroup *pos_group,
                           const DictionaryInterface *dictionary)
    : pos_matcher_(data_manager->GetPosMatcherData()) {
  DCHECK(parent_converter);
  DCHECK(data_manager);
  DCHECK(pos_group);
  // |dictionary| can be NULL

  AddRewriter(std::make_unique<UserDictionaryRewriter>());
  AddRewriter(std::make_unique<FocusCandidateRewriter>(data_manager));
  AddRewriter(
      std::make_unique<LanguageAwareRewriter>(pos_matcher_, dictionary));
  AddRewriter(std::make_unique<TransliterationRewriter>(pos_matcher_));
  AddRewriter(std::make_unique<EnglishVariantsRewriter>());
  AddRewriter(std::make_unique<NumberRewriter>(data_manager));
  AddRewriter(std::make_unique<CollocationRewriter>(data_manager));
  AddRewriter(std::make_unique<SingleKanjiRewriter>(*data_manager));
  AddRewriter(std::make_unique<EmojiRewriter>(*data_manager));
  AddRewriter(EmoticonRewriter::CreateFromDataManager(*data_manager));
  AddRewriter(std::make_unique<CalculatorRewriter>(parent_converter));
  AddRewriter(std::make_unique<SymbolRewriter>(parent_converter, data_manager));
  AddRewriter(std::make_unique<UnicodeRewriter>(parent_converter));
  AddRewriter(std::make_unique<VariantsRewriter>(pos_matcher_));
  AddRewriter(std::make_unique<ZipcodeRewriter>(&pos_matcher_));
  AddRewriter(std::make_unique<DiceRewriter>());
  AddRewriter(std::make_unique<SmallLetterRewriter>(parent_converter));

  if (absl::GetFlag(FLAGS_use_history_rewriter)) {
    AddRewriter(
        std::make_unique<UserBoundaryHistoryRewriter>(parent_converter));
    AddRewriter(
        std::make_unique<UserSegmentHistoryRewriter>(&pos_matcher_, pos_group));
  }

  AddRewriter(std::make_unique<DateRewriter>(dictionary));
  AddRewriter(std::make_unique<FortuneRewriter>());
#if !(defined(OS_ANDROID) || defined(OS_IOS))
  // CommandRewriter is not tested well on Android or iOS.
  // So we temporarily disable it.
  // TODO(yukawa, team): Enable CommandRewriter on Android if necessary.
  AddRewriter(std::make_unique<CommandRewriter>());
#endif  // !(OS_ANDROID || OS_IOS)
#ifndef NO_USAGE_REWRITER
  AddRewriter(std::make_unique<UsageRewriter>(data_manager, dictionary));
#endif  // NO_USAGE_REWRITER
  AddRewriter(
      std::make_unique<VersionRewriter>(data_manager->GetDataVersion()));
  AddRewriter(CorrectionRewriter::CreateCorrectionRewriter(data_manager));
  AddRewriter(std::make_unique<KatakanaPromotionRewriter>());
  AddRewriter(std::make_unique<NormalizationRewriter>());
  AddRewriter(std::make_unique<RemoveRedundantCandidateRewriter>());
}

}  // namespace mozc
