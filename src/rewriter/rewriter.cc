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
#include "data_manager/data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "engine/modules.h"
#include "rewriter/a11y_description_rewriter.h"
#include "rewriter/calculator_rewriter.h"
#include "rewriter/collocation_rewriter.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/dice_rewriter.h"
#include "rewriter/emoji_rewriter.h"
#include "rewriter/emoticon_rewriter.h"
#include "rewriter/english_variants_rewriter.h"
#include "rewriter/environmental_filter_rewriter.h"
#include "rewriter/focus_candidate_rewriter.h"
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
#include "rewriter/user_segment_history_rewriter.h"
#include "rewriter/variants_rewriter.h"
#include "rewriter/version_rewriter.h"
#include "rewriter/zipcode_rewriter.h"

#ifdef __APPLE__
#include <TargetConditionals.h>  // for TARGET_OS_IPHONE
#endif                           // __APPLE__


// CommandRewriter is not tested well on Android or iOS.
// So we temporarily disable it.
// TODO(yukawa, team): Enable CommandRewriter on Android if necessary.
#if !(defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE))
#define MOZC_COMMAND_REWRITER
#endif  // !(__ANDROID__ || TARGET_OS_IPHONE)

// DateRewriter may return the date information that is possibly different from
// the user's environment.
#define MOZC_DATE_REWRITER

// FortuneRewriter changes the result when invoked in another day but it also
// suffers from the inconsistency of locale between server and user's
// environment.
#define MOZC_FORTUNE_REWRITER

// UsageRewriter is not used by non application build.
#ifndef NO_USAGE_REWRITER
#define MOZC_USAGE_REWRITER
#endif  // NO_USAGE_REWRITER

// UserDictionaryRewriter is only for application build because it will
// access to the local files per user.
#define MOZC_USER_DICTIONARY_REWRITER

// HistoryRewriter is used only for application build because it will
// access to the local files at the initialization timing.
#define MOZC_USER_HISTORY_REWRITER


#ifdef MOZC_COMMAND_REWRITER
#include "rewriter/command_rewriter.h"
#endif  // MOZC_COMMAND_REWRITER

#ifdef MOZC_DATE_REWRITER
#include "rewriter/date_rewriter.h"
#endif  // MOZC_DATE_REWRITER

#ifdef MOZC_FORTUNE_REWRITER
#include "rewriter/fortune_rewriter.h"
#endif  // MOZC_FORTUNE_REWRITER

#ifdef MOZC_USAGE_REWRITER
#include "rewriter/usage_rewriter.h"
#endif  // MOZC_USAGE_REWRITER

#ifdef MOZC_USER_DICTIONARY_REWRITER
#include "rewriter/user_dictionary_rewriter.h"
#endif  // MOZC_USER_DICTIONARY_REWRITER

#ifdef MOZC_USER_HISTORY_REWRITER
ABSL_FLAG(bool, use_history_rewriter, true, "Use history rewriter or not.");
#else   // MOZC_USER_HISTORY_REWRITER
ABSL_FLAG(bool, use_history_rewriter, false, "Use history rewriter or not.");
#endif  // MOZC_USER_HISTORY_REWRITER

namespace mozc {

Rewriter::Rewriter(const engine::Modules& modules) {
  const DataManager& data_manager = modules.GetDataManager();
  const dictionary::DictionaryInterface& dictionary = modules.GetDictionary();
  const dictionary::PosMatcher& pos_matcher = modules.GetPosMatcher();
  const dictionary::PosGroup& pos_group = modules.GetPosGroup();
  const dictionary::SingleKanjiDictionary& single_kanji_dictionary =
      modules.GetSingleKanjiDictionary();

#ifdef MOZC_USER_DICTIONARY_REWRITER
  AddRewriter(std::make_unique<UserDictionaryRewriter>());
#endif  // MOZC_USER_DICTIONARY_REWRITER

  AddRewriter(std::make_unique<FocusCandidateRewriter>(data_manager));
  AddRewriter(std::make_unique<LanguageAwareRewriter>(pos_matcher, dictionary));
  AddRewriter(std::make_unique<TransliterationRewriter>(pos_matcher));
  AddRewriter(std::make_unique<EnglishVariantsRewriter>(pos_matcher));
  AddRewriter(std::make_unique<NumberRewriter>(data_manager));
  AddRewriter(CollocationRewriter::Create(data_manager));
  AddRewriter(std::make_unique<SingleKanjiRewriter>(pos_matcher,
                                                    single_kanji_dictionary));
  AddRewriter(std::make_unique<IvsVariantsRewriter>());
  AddRewriter(std::make_unique<EmojiRewriter>(data_manager));
  AddRewriter(EmoticonRewriter::CreateFromDataManager(data_manager));
  AddRewriter(std::make_unique<CalculatorRewriter>());
  AddRewriter(std::make_unique<SymbolRewriter>(data_manager));
  AddRewriter(std::make_unique<UnicodeRewriter>());
  AddRewriter(std::make_unique<VariantsRewriter>(pos_matcher));
  AddRewriter(std::make_unique<ZipcodeRewriter>(pos_matcher));
  AddRewriter(std::make_unique<DiceRewriter>());
  AddRewriter(std::make_unique<SmallLetterRewriter>());

  if (absl::GetFlag(FLAGS_use_history_rewriter)) {
    AddRewriter(std::make_unique<UserBoundaryHistoryRewriter>());
    AddRewriter(
        std::make_unique<UserSegmentHistoryRewriter>(pos_matcher, pos_group));
  }

#ifdef MOZC_DATE_REWRITER
  AddRewriter(std::make_unique<DateRewriter>(dictionary));
#endif  // MOZC_DATE_REWRITER

#ifdef MOZC_FORTUNE_REWRITER
  AddRewriter(std::make_unique<FortuneRewriter>());
#endif  // MOZC_FORTUNE_REWRITER

#ifdef MOZC_COMMAND_REWRITER
  AddRewriter(std::make_unique<CommandRewriter>());
#endif  // MOZC_COMMAND_REWRITER

#ifdef MOZC_USAGE_REWRITER
  AddRewriter(std::make_unique<UsageRewriter>(data_manager, dictionary));
#endif  // MOZC_USAGE_REWRITER

  AddRewriter(std::make_unique<VersionRewriter>(data_manager.GetDataVersion()));
  AddRewriter(CorrectionRewriter::CreateCorrectionRewriter(modules));
  AddRewriter(std::make_unique<T13nPromotionRewriter>());
  AddRewriter(std::make_unique<EnvironmentalFilterRewriter>(data_manager));
  AddRewriter(std::make_unique<RemoveRedundantCandidateRewriter>());
  AddRewriter(std::make_unique<A11yDescriptionRewriter>(data_manager));
}

}  // namespace mozc
