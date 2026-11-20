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

#include "absl/flags/flag.h"
#include "base/container/tuple.h"
#include "base/port.h"
#include "data_manager/data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "engine/modules.h"
#include "rewriter/a11y_description_rewriter.h"
#include "rewriter/calculator_rewriter.h"
#include "rewriter/collocation_rewriter.h"
#include "rewriter/command_rewriter.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/date_rewriter.h"
#include "rewriter/dice_rewriter.h"
#include "rewriter/emoji_rewriter.h"
#include "rewriter/emoticon_rewriter.h"
#include "rewriter/english_variants_rewriter.h"
#include "rewriter/environmental_filter_rewriter.h"
#include "rewriter/focus_candidate_rewriter.h"
#include "rewriter/fortune_rewriter.h"
#include "rewriter/ivs_variants_rewriter.h"
#include "rewriter/language_aware_rewriter.h"
#include "rewriter/number_rewriter.h"
#include "rewriter/remove_redundant_candidate_rewriter.h"
#include "rewriter/single_kanji_rewriter.h"
#include "rewriter/small_letter_rewriter.h"
#include "rewriter/symbol_rewriter.h"
#include "rewriter/t13n_promotion_rewriter.h"
#include "rewriter/transliteration_rewriter.h"
#include "rewriter/unicode_rewriter.h"
#include "rewriter/user_boundary_history_rewriter.h"
#include "rewriter/user_dictionary_rewriter.h"
#include "rewriter/user_segment_history_rewriter.h"
#include "rewriter/variants_rewriter.h"
#include "rewriter/version_rewriter.h"
#include "rewriter/zipcode_rewriter.h"

#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter.h"
#endif  // NO_USAGE_REWRITER

// HistoryRewriter is used only for application builds because it accesses
// local files at initialization time.
ABSL_FLAG(bool, use_history_rewriter, mozc::port::IsApplicationBuild(),
          "Use history rewriter or not.");

namespace mozc {

Rewriter::Rewriter(const engine::Modules& modules) {
  const DataManager& data_manager = modules.GetDataManager();
  const dictionary::DictionaryInterface& dictionary = modules.GetDictionary();
  const dictionary::PosMatcher& pos_matcher = modules.GetPosMatcher();
  const dictionary::PosGroup& pos_group = modules.GetPosGroup();
  const dictionary::SingleKanjiDictionary& single_kanji_dictionary =
      modules.GetSingleKanjiDictionary();

  if constexpr (port::IsApplicationBuild()) {
    // UserDictionaryRewriter is only for application builds because it accesses
    // local files per user.
    AddRewriter(std::make_unique<UserDictionaryRewriter>());
  }

  AddRewriter(make_unique_from_tuples<FocusCandidateRewriter>(
      data_manager.GetCounterSuffixSortedArray(), pos_matcher));
  AddRewriter(std::make_unique<LanguageAwareRewriter>(pos_matcher, dictionary));
  AddRewriter(std::make_unique<TransliterationRewriter>(pos_matcher));
  AddRewriter(std::make_unique<EnglishVariantsRewriter>(pos_matcher));
  AddRewriter(make_unique_from_tuples<NumberRewriter>(
      data_manager.GetCounterSuffixSortedArray(), pos_matcher));
  AddRewriter(apply_from_tuples(CollocationRewriter::Create, pos_matcher,
                                data_manager.GetCollocationData()));
  AddRewriter(std::make_unique<SingleKanjiRewriter>(pos_matcher,
                                                    single_kanji_dictionary));
  AddRewriter(std::make_unique<IvsVariantsRewriter>());
  AddRewriter(make_unique_from_tuples<EmoticonRewriter>(
      data_manager.GetEmoticonRewriterData()));
  AddRewriter(make_unique_from_tuples<EmojiRewriter>(
      data_manager.GetEmojiRewriterData()));
  AddRewriter(std::make_unique<CalculatorRewriter>());
  AddRewriter(make_unique_from_tuples<SymbolRewriter>(
      data_manager.GetSymbolRewriterData()));
  AddRewriter(std::make_unique<UnicodeRewriter>());
  if (absl::GetFlag(FLAGS_use_history_rewriter)) {
    AddRewriter(
        std::make_unique<UserSegmentHistoryRewriter>(pos_matcher, pos_group));
  }

  AddRewriter(std::make_unique<VariantsRewriter>(pos_matcher));
  AddRewriter(std::make_unique<ZipcodeRewriter>(pos_matcher));
  AddRewriter(std::make_unique<DiceRewriter>());
  AddRewriter(std::make_unique<SmallLetterRewriter>());

  if (absl::GetFlag(FLAGS_use_history_rewriter)) {
    AddRewriter(std::make_unique<UserBoundaryHistoryRewriter>());
  }

  if constexpr (port::IsApplicationBuild()) {
    // DateRewriter may return date information that is possibly different from
    // the user's environment.
    AddRewriter(std::make_unique<DateRewriter>(dictionary));
    // FortuneRewriter changes the result when invoked on another day and also
    // suffers from locale inconsistency between the server and the user's
    // environment.
    AddRewriter(std::make_unique<FortuneRewriter>());
  }

  // CommandRewriter is not tested well on Android or iOS, so we temporarily
  // disable it.
  // TODO(yukawa, team): Enable CommandRewriter on Android if necessary.
  if constexpr (port::IsApplicationBuild() && !port::IsAndroid() &&
                !port::IsIos()) {
    AddRewriter(std::make_unique<CommandRewriter>());
  }

#ifndef NO_USAGE_REWRITER
  if constexpr (port::IsApplicationBuild()) {
    // UsageRewriter is not used by non-application builds.
    AddRewriter(make_unique_from_tuples<UsageRewriter>(
        data_manager.GetUsageRewriterData(), dictionary, pos_matcher));
  }
#endif  // NO_USAGE_REWRITER

  AddRewriter(std::make_unique<VersionRewriter>(data_manager.GetDataVersion()));
  AddRewriter(make_unique_from_tuples<CorrectionRewriter>(
      modules, data_manager.GetReadingCorrectionData()));
  AddRewriter(std::make_unique<T13nPromotionRewriter>());
  AddRewriter(make_unique_from_tuples<EnvironmentalFilterRewriter>(
      data_manager.GetEmojiRewriterData()));
  AddRewriter(std::make_unique<RemoveRedundantCandidateRewriter>());
  AddRewriter(make_unique_from_tuples<A11yDescriptionRewriter>(
      data_manager.GetA11yDescriptionRewriterData()));
}

}  // namespace mozc
