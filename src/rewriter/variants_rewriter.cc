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

#include "rewriter/variants_rewriter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "base/japanese_util.h"
#include "base/number_util.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "base/vlog.h"
#include "config/character_form_manager.h"
#include "converter/candidate.h"
#include "converter/inner_segment.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

using ::mozc::config::CharacterFormManager;
using ::mozc::converter::Candidate;
using ::mozc::dictionary::PosMatcher;

// Returns true if |full| has the corresponding half width form.
bool IsConvertibleToHalfWidthForm(const absl::string_view full) {
  // TODO(b/209357879): remove this line once FullWidthToHalfWidth() itself will
  // support the conversion.
  const std::string tmp =
      absl::StrReplaceAll(full, {{"＼", "\\"}, {"￥", "¥"}});

  std::string half = japanese_util::FullWidthToHalfWidth(tmp);
  return full != half;
}

// Returns true if |value| meets all the following 1) ~ 3) conditions.
// 1) all charcters in |value| are UNKNOWN_SCRIPT
// 2) FormType of |value| are consistent, e.g. all fullwith or all halfwidth
// 3) if they're all fullwidth characters, they're potentially convertible to
//    their corresponding halfwidth form, e.g. '／' => '/'
// Example:
// "&-()" => true (all symbol and all half)
// "／" => true (all symbol, all full and convertible to the corresponding half)
// "&-（）" => false (all symbol but contains both full/half width)
// "google" => false (not symbol)
// "㌫" => false (all symbol, all full but not convertible to the corresponding
// half)
bool HasCharacterFormDescription(const absl::string_view value) {
  if (value.empty()) {
    return false;
  }
  Util::FormType prev = Util::UNKNOWN_FORM;

  for (const char32_t codepoint : Utf8AsChars32(value)) {
    const Util::FormType type = Util::GetFormType(codepoint);
    if (prev != Util::UNKNOWN_FORM && prev != type) {
      return false;
    }
    if (Util::UNKNOWN_SCRIPT != Util::GetScriptType(codepoint)) {
      return false;
    }
    prev = type;
  }

  if (prev == Util::HALF_WIDTH) {
    return true;
  }

  // returns false here only if all the characters are fullwidth and they're not
  // convertible to their corresponding halfwidth forms.
  return IsConvertibleToHalfWidthForm(value);
}

// Returns NumberString::Style corresponding to the given form
NumberUtil::NumberString::Style GetStyle(
    const NumberUtil::NumberString::Style original_style,
    bool is_half_width_form) {
  switch (original_style) {
    case NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH:
    case NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH:
      return is_half_width_form
                 ? NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH
                 : NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH;
    case NumberUtil::NumberString::NUMBER_ARABIC_AND_KANJI_HALFWIDTH:
    case NumberUtil::NumberString::NUMBER_ARABIC_AND_KANJI_FULLWIDTH:
      return is_half_width_form
                 ? NumberUtil::NumberString::NUMBER_ARABIC_AND_KANJI_HALFWIDTH
                 : NumberUtil::NumberString::NUMBER_ARABIC_AND_KANJI_FULLWIDTH;
    default:
      return original_style;
  }
}

}  // namespace

// static
void VariantsRewriter::SetDescriptionForCandidate(const PosMatcher pos_matcher,
                                                  Candidate *candidate) {
  SetDescription(
      pos_matcher,
      (FULL_HALF_WIDTH | CHARACTER_FORM | ZIPCODE | SPELLING_CORRECTION),
      candidate);
}

// static
void VariantsRewriter::SetDescriptionForTransliteration(
    const PosMatcher pos_matcher, Candidate *candidate) {
  SetDescription(pos_matcher,
                 (FULL_HALF_WIDTH | CHARACTER_FORM | SPELLING_CORRECTION),
                 candidate);
}

// static
void VariantsRewriter::SetDescriptionForPrediction(const PosMatcher pos_matcher,
                                                   Candidate *candidate) {
  SetDescription(pos_matcher, ZIPCODE | SPELLING_CORRECTION, candidate);
}

// static
std::string VariantsRewriter::GetDescription(const PosMatcher pos_matcher,
                                             int description_type,
                                             const Candidate &candidate) {
  absl::string_view character_form_message;
  std::vector<absl::string_view> pieces;

  // Add Character form.
  if (description_type & CHARACTER_FORM) {
    const Util::ScriptType type =
        Util::GetScriptTypeWithoutSymbols(candidate.value);
    switch (type) {
      case Util::HIRAGANA:
        character_form_message = kHiragana;
        // don't need to set full/half, because hiragana only has
        // full form
        description_type &= ~FULL_HALF_WIDTH;
        break;
      case Util::KATAKANA:
        // character_form_message = "カタカナ";
        character_form_message = kKatakana;
        // don't need to proactively set full, because katakana is mostly used
        // in full form.
        description_type &= ~FULL_HALF_WIDTH;
        description_type |= HALF_WIDTH;
        break;
      case Util::NUMBER:
        // character_form_message = "数字";
        character_form_message = kNumber;
        // don't need to proactively set half, because number is mostly used in
        // half form.
        description_type &= ~FULL_HALF_WIDTH;
        description_type |= FULL_WIDTH;
        break;
      case Util::ALPHABET:
        // character_form_message = "アルファベット";
        character_form_message = kAlphabet;
        // don't need to proactively set half, because alphabet is mostly used
        // in half form.
        description_type &= ~FULL_HALF_WIDTH;
        description_type |= FULL_WIDTH;
        break;
      case Util::KANJI:
      case Util::EMOJI:
        // don't need to have full/half annotation for kanji and emoji,
        // since it's obvious
        description_type &= ~FULL_HALF_WIDTH;
        break;
      case Util::UNKNOWN_SCRIPT:  // mixed character
        if (HasCharacterFormDescription(candidate.value)) {
          description_type |= FULL_HALF_WIDTH;
        } else {
          description_type &= ~FULL_HALF_WIDTH;
        }
        break;
      default:
        // Do nothing
        break;
    }
  }

  // If candidate already has a description, clear it.
  // Currently, character_form_message is treated as a "default"
  // description.
  if (!candidate.description.empty()) {
    character_form_message = absl::string_view();
  }

  const Util::FormType form = Util::GetFormType(candidate.value);
  // full/half char description
  if (description_type & FULL_HALF_WIDTH) {
    switch (form) {
      case Util::FULL_WIDTH:
        // description = "[全]";
        pieces.push_back(kFullWidth);
        break;
      case Util::HALF_WIDTH:
        // description = "[半]";
        pieces.push_back(kHalfWidth);
        break;
      default:
        break;
    }
  } else if (description_type & FULL_WIDTH && form == Util::FULL_WIDTH) {
    // description = "[全]";
    pieces.push_back(kFullWidth);
  } else if (description_type & HALF_WIDTH && form == Util::HALF_WIDTH) {
    // description = "[半]";
    pieces.push_back(kHalfWidth);
  }

  // add character_form_message
  if (!character_form_message.empty()) {
    pieces.push_back(character_form_message);
  }

  // add main message
  if (candidate.value == "\\" || candidate.value == "＼") {
    // if "\" (half-width backslash) or "＼" (full-width backslash)
    pieces.push_back("バックスラッシュ");
  } else if (candidate.value == "¥") {
    // if "¥" (half-width Yen sign), append kYenKigou.
    pieces.push_back(kYenKigou);
  } else if (candidate.value == "￥") {
    // if "￥" (full-width Yen sign), append only kYenKigou
    pieces.push_back(kYenKigou);
  } else if (candidate.value == "~") {
    pieces.push_back("チルダ");
  } else if (!candidate.description.empty()) {
    pieces.push_back(candidate.description);
  }

  // The following description tries to overwrite existing description.
  // TODO(taku): reconsider this behavior.
  // Zipcode description
  if ((description_type & ZIPCODE) && pos_matcher.IsZipcode(candidate.lid) &&
      candidate.lid == candidate.rid) {
    if (!candidate.content_key.empty()) {
      pieces = {candidate.content_key};
    } else {
      pieces.clear();
    }
    // Append default description because it may contain extra description.
    if (!candidate.description.empty()) {
      pieces.push_back(candidate.description);
    }
  }

  // The following description tries to overwrite existing description.
  // TODO(taku): reconsider this behavior.
  // Spelling Correction description
  if ((description_type & SPELLING_CORRECTION) &&
      (candidate.attributes & Candidate::SPELLING_CORRECTION)) {
    // Append default description because it may contain extra description.
    if (candidate.description.empty()) {
      pieces = {kDidYouMean};
    } else {
      pieces = {kDidYouMean, candidate.description};
    }
  }
  return absl::StrJoin(pieces, " ");
}

// static
absl::string_view VariantsRewriter::GetPrefix(const int description_type,
                                              const Candidate &candidate) {
  if ((description_type & SPELLING_CORRECTION) &&
      (candidate.attributes & Candidate::SPELLING_CORRECTION)) {
    // Add prefix to distinguish this candidate.
    return "→ ";
  }
  return "";
}

void VariantsRewriter::SetDescription(const PosMatcher pos_matcher,
                                      int description_type,
                                      Candidate *candidate) {
  // set new description
  candidate->description =
      GetDescription(pos_matcher, description_type, *candidate);
  candidate->prefix = GetPrefix(description_type, *candidate);
  candidate->attributes |= Candidate::NO_EXTRA_DESCRIPTION;
}

int VariantsRewriter::capability(const ConversionRequest &request) const {
  return RewriterInterface::ALL;
}

namespace {

bool IsHalfWidthVoiceSoundMark(char32_t ch) {
  // 0xFF9E: Halfwidth voice sound mark
  // 0xFF9F: Halfwidth semi-voice sound mark
  return ch == 0xFF9E || ch == 0xFF9F;
}

// Skip halfwidth voice/semi-voice sound mark as they are treated as one
// character.
Utf8AsChars32::const_iterator SkipHalfWidthVoiceSoundMark(
    Utf8AsChars32::const_iterator it, Utf8AsChars32::const_iterator last) {
  while (it != last && IsHalfWidthVoiceSoundMark(*it)) {
    ++it;
  }
  return it;
}

}  // namespace

std::pair<Util::FormType, Util::FormType>
VariantsRewriter::GetFormTypesFromStringPair(absl::string_view input1,
                                             absl::string_view input2) {
  static constexpr std::pair<Util::FormType, Util::FormType> kUnknownForm = {
      Util::UNKNOWN_FORM, Util::UNKNOWN_FORM};
  Util::FormType output_form1 = Util::UNKNOWN_FORM;
  Util::FormType output_form2 = Util::UNKNOWN_FORM;

  Utf8AsChars32 chars1(input1), chars2(input2);
  auto it1 = chars1.begin(), it2 = chars2.begin();
  for (; it1 != chars1.end() && it2 != chars2.end(); ++it1, ++it2) {
    it1 = SkipHalfWidthVoiceSoundMark(it1, chars1.end());
    it2 = SkipHalfWidthVoiceSoundMark(it2, chars2.end());
    if (it1 == chars1.end() || it2 == chars2.end()) {
      break;
    }

    // TODO(taku): have to check that normalized w1 and w2 are identical
    if (Util::GetScriptType(*it1) != Util::GetScriptType(*it2)) {
      return kUnknownForm;
    }

    const Util::FormType form1 = Util::GetFormType(*it1);
    const Util::FormType form2 = Util::GetFormType(*it2);
    DCHECK_NE(form1, Util::UNKNOWN_FORM);
    DCHECK_NE(form2, Util::UNKNOWN_FORM);

    // when having different forms, record the diff in the next step.
    if (form1 == form2) {
      continue;
    }

    const bool is_consistent =
        (output_form1 == Util::UNKNOWN_FORM || output_form1 == form1) &&
        (output_form2 == Util::UNKNOWN_FORM || output_form2 == form2);
    if (!is_consistent) {
      // inconsistent with the previous forms.
      return kUnknownForm;
    }

    output_form1 = form1;
    output_form2 = form2;
  }

  // length should be the same
  if (it1 != chars1.end() || it2 != chars2.end()) {
    return kUnknownForm;
  }

  if (output_form1 == Util::UNKNOWN_FORM ||
      output_form2 == Util::UNKNOWN_FORM) {
    return kUnknownForm;
  }

  return std::make_pair(output_form1, output_form2);
}

VariantsRewriter::AlternativeCandidateResult
VariantsRewriter::CreateAlternativeCandidate(
    const Candidate &original_candidate) const {
  std::string primary_value, secondary_value;
  std::string primary_content_value, secondary_content_value;
  converter::InnerSegmentBoundary primary_inner_segment_boundary;
  converter::InnerSegmentBoundary secondary_inner_segment_boundary;

  AlternativeCandidateResult result;
  if (!GenerateAlternatives(
          original_candidate, &primary_value, &secondary_value,
          &primary_content_value, &secondary_content_value,
          &primary_inner_segment_boundary, &secondary_inner_segment_boundary)) {
    return result;
  }

  // auto = std::pair<Util::FormType, Util::FormType>
  const auto [primary_form, secondary_form] =
      GetFormTypesFromStringPair(primary_value, secondary_value);
  const bool is_primary_half_width = (primary_form == Util::HALF_WIDTH);
  const bool is_secondary_half_width = (secondary_form == Util::HALF_WIDTH);

  auto get_description_type = [](Util::FormType form) {
    constexpr int kBaseTypes = CHARACTER_FORM | ZIPCODE | SPELLING_CORRECTION;
    switch (form) {
      case Util::FULL_WIDTH:
        return VariantsRewriter::FULL_WIDTH | kBaseTypes;
      case Util::HALF_WIDTH:
        return VariantsRewriter::HALF_WIDTH | kBaseTypes;
      default:
        return VariantsRewriter::FULL_HALF_WIDTH | kBaseTypes;
    }
  };
  const int primary_description_type = get_description_type(primary_form);
  const int secondary_description_type = get_description_type(secondary_form);

  auto new_candidate = std::make_unique<Candidate>(original_candidate);

  if (original_candidate.value == primary_value) {
    result.is_original_candidate_primary = true;
    result.original_candidate_description_type = primary_description_type;

    new_candidate->value = std::move(secondary_value);
    new_candidate->content_value = std::move(secondary_content_value);
    new_candidate->inner_segment_boundary =
        std::move(secondary_inner_segment_boundary);
    new_candidate->style =
        GetStyle(original_candidate.style, is_secondary_half_width);
    SetDescription(pos_matcher_, secondary_description_type,
                   new_candidate.get());
  } else {
    result.is_original_candidate_primary = false;
    result.original_candidate_description_type = secondary_description_type;

    new_candidate->value = std::move(primary_value);
    new_candidate->content_value = std::move(primary_content_value);
    new_candidate->inner_segment_boundary =
        std::move(primary_inner_segment_boundary);
    new_candidate->style =
        GetStyle(original_candidate.style, is_primary_half_width);
    SetDescription(pos_matcher_, primary_description_type, new_candidate.get());
  }
  result.alternative_candidate = std::move(new_candidate);
  return result;
}

bool VariantsRewriter::RewriteSegment(RewriteType type, Segment *seg) const {
  CHECK(seg);
  bool modified = false;

  // Meta Candidate
  for (Candidate &candidate : *seg->mutable_meta_candidates()) {
    if (candidate.attributes & Candidate::NO_EXTRA_DESCRIPTION) {
      continue;
    }
    SetDescriptionForTransliteration(pos_matcher_, &candidate);
  }

  // Regular Candidate
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    Candidate *original_candidate = seg->mutable_candidate(i);
    DCHECK(original_candidate);

    if (original_candidate->attributes & Candidate::NO_EXTRA_DESCRIPTION) {
      continue;
    }

    if (original_candidate->attributes & Candidate::NO_VARIANTS_EXPANSION) {
      SetDescriptionForCandidate(pos_matcher_, original_candidate);
      MOZC_VLOG(1) << "Candidate has NO_NORMALIZATION node";
      continue;
    }

    AlternativeCandidateResult result =
        CreateAlternativeCandidate(*original_candidate);
    if (result.alternative_candidate == nullptr) {
      SetDescriptionForCandidate(pos_matcher_, original_candidate);
      continue;
    }

    if (original_candidate->description.empty() &&
        original_candidate->attributes & Candidate::USER_HISTORY_PREDICTION) {
      SetDescriptionForPrediction(pos_matcher_, original_candidate);
      continue;
    }

    SetDescription(pos_matcher_, result.original_candidate_description_type,
                   original_candidate);
    if (type == EXPAND_VARIANT) {
      // If the original candidate is the primary candidate, insert alternative
      // candidate after the original candidate as the secondary candidate.
      const int index = result.is_original_candidate_primary ? i + 1 : i;
      seg->insert_candidate(index, std::move(result.alternative_candidate));
      ++i;  // skip inserted candidate
    } else if (!result.is_original_candidate_primary) {
      DCHECK_EQ(type, SELECT_VARIANT);
      // If the original candidate is not the primary candidate, remove it and
      // insert the alternative candidate as a replacement.
      seg->erase_candidate(i);
      seg->insert_candidate(i, std::move(result.alternative_candidate));
    }
    modified = true;
  }
  return modified;
}

// Try generating default and alternative character forms.  Inner segment
// boundary is taken into account.  When no rewrite happens, false is returned.
bool VariantsRewriter::GenerateAlternatives(
    const Candidate &original, std::string *primary_value,
    std::string *secondary_value, std::string *primary_content_value,
    std::string *secondary_content_value,
    converter::InnerSegmentBoundary *primary_inner_segment_boundary,
    converter::InnerSegmentBoundary *secondary_inner_segment_boundary) const {
  primary_value->clear();
  secondary_value->clear();
  primary_content_value->clear();
  secondary_content_value->clear();
  primary_inner_segment_boundary->clear();
  secondary_inner_segment_boundary->clear();

  const config::CharacterFormManager *manager =
      CharacterFormManager::GetCharacterFormManager();

  // When inner segment boundary is present, rewrite each inner segment.  If at
  // least one inner segment is rewritten, the whole segment is considered
  // rewritten.
  bool at_least_one_modified = false;
  std::string inner_primary_value, inner_secondary_value;
  std::string inner_primary_content_value, inner_secondary_content_value;
  converter::InnerSegmentBoundaryBuilder primary_builder, secondary_builder;

  for (const auto &iter : original.inner_segments()) {
    inner_primary_value.clear();
    inner_secondary_value.clear();
    if (!manager->ConvertConversionStringWithAlternative(
            iter.GetValue(), &inner_primary_value, &inner_secondary_value)) {
      inner_primary_value = iter.GetValue();
      inner_secondary_value = iter.GetValue();
    } else {
      at_least_one_modified = true;
    }
    if (iter.GetValue() != iter.GetContentValue()) {
      inner_primary_content_value.clear();
      inner_secondary_content_value.clear();
      manager->ConvertConversionStringWithAlternative(
          iter.GetContentValue(), &inner_primary_content_value,
          &inner_secondary_content_value);
    } else {
      inner_primary_content_value = inner_primary_value;
      inner_secondary_content_value = inner_secondary_value;
    }
    absl::StrAppend(primary_value, inner_primary_value);
    absl::StrAppend(secondary_value, inner_secondary_value);
    absl::StrAppend(primary_content_value, inner_primary_content_value);
    absl::StrAppend(secondary_content_value, inner_secondary_content_value);
    primary_builder.Add(iter.GetKey().size(), inner_primary_value.size(),
                        iter.GetContentKey().size(),
                        inner_primary_content_value.size());
    secondary_builder.Add(iter.GetKey().size(), inner_secondary_value.size(),
                          iter.GetContentKey().size(),
                          inner_secondary_content_value.size());
  }

  *primary_inner_segment_boundary =
      primary_builder.Build(original.key, *primary_value);
  *secondary_inner_segment_boundary =
      secondary_builder.Build(original.key, *secondary_value);
  return at_least_one_modified;
}

void VariantsRewriter::Finish(const ConversionRequest &request,
                              const Segments &segments) {
  if (request.config().history_learning_level() !=
      config::Config::DEFAULT_HISTORY) {
    MOZC_VLOG(2) << "history_learning_level is not DEFAULT_HISTORY";
    return;
  }
  if (!request.request().mixed_conversion() &&
      request.request_type() != ConversionRequest::CONVERSION) {
    return;
  }

  // save character form
  for (const Segment &segment : segments.conversion_segments()) {
    if (segment.candidates_size() <= 0 ||
        segment.segment_type() != Segment::FIXED_VALUE ||
        segment.candidate(0).attributes & Candidate::NO_HISTORY_LEARNING) {
      continue;
    }

    const Candidate &candidate = segment.candidate(0);
    if (candidate.attributes & Candidate::NO_VARIANTS_EXPANSION) {
      continue;
    }

    switch (candidate.style) {
      case NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_HALFWIDTH:
        // treat NUMBER_SEPARATED_ARABIC as half_width num
        CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
            "0", config::Config::HALF_WIDTH);
        continue;
      case NumberUtil::NumberString::NUMBER_SEPARATED_ARABIC_FULLWIDTH:
        // treat NUMBER_SEPARATED_WIDE_ARABIC as full_width num
        CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
            "0", config::Config::FULL_WIDTH);
        continue;
      default:
        break;
    }
    // Special handling for number compounds like 3時.  Note:
    // GuessAndSetCharacterForm() below this if-block cannot guess the
    // character form for number compounds.  Since this module adds
    // annotation in the description for character width, using it is
    // more reliable than guessing from |candidate.value|.
    if (Util::GetFirstScriptType(candidate.value) == Util::NUMBER) {
      if (absl::StrContains(candidate.description, kHalfWidth)) {
        CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
            "0", config::Config::HALF_WIDTH);
        continue;
      }
      if (absl::StrContains(candidate.description, kFullWidth)) {
        CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
            "0", config::Config::FULL_WIDTH);
        continue;
      }
    }
    CharacterFormManager::GetCharacterFormManager()->GuessAndSetCharacterForm(
        candidate.value);
  }
}

void VariantsRewriter::Clear() {
  CharacterFormManager::GetCharacterFormManager()->ClearHistory();
}

bool VariantsRewriter::Rewrite(const ConversionRequest &request,
                               Segments *segments) const {
  CHECK(segments);
  bool modified = false;

  RewriteType type;
  if (request.request().mixed_conversion()) {  // For mobile.
    type = EXPAND_VARIANT;
  } else if (request.request_type() == ConversionRequest::SUGGESTION) {
    type = SELECT_VARIANT;
  } else {
    type = EXPAND_VARIANT;
  }

  for (Segment &segment : segments->conversion_segments()) {
    modified |= RewriteSegment(type, &segment);
  }

  return modified;
}

}  // namespace mozc
