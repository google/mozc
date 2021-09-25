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

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "config/character_form_manager.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using ::mozc::config::CharacterFormManager;
using ::mozc::dictionary::PosMatcher;

// Append |src| to |dst| with a separator ' '.
void AppendString(absl::string_view src, std::string *dst) {
  DCHECK(dst);
  if (!src.empty()) {
    if (!dst->empty()) {
      dst->append(1, ' ');
    }
    dst->append(src.data(), src.size());
  }
}

// Return true if all charcters in |value| are UNKNOWN_SCRIPT
// and FormType of |value| are consistent, e.g. all fullwith or
// all halfwidth.
// Example:
// "&-()" => true (all symbol and all half)
// "&-（）" => false (all symbol but contains both full/half width)
// "google" => false (not symbol)
bool HasCharacterFormDescription(const std::string &value) {
  if (value.empty()) {
    return false;
  }
  Util::FormType prev = Util::UNKNOWN_FORM;

  for (ConstChar32Iterator iter(value); !iter.Done(); iter.Next()) {
    const char32 ucs4 = iter.Get();
    const Util::FormType type = Util::GetFormType(ucs4);
    if (prev != Util::UNKNOWN_FORM && prev != type) {
      return false;
    }
    if (Util::UNKNOWN_SCRIPT != Util::GetScriptType(ucs4)) {
      return false;
    }
    prev = type;
  }
  return true;
}

}  // namespace

VariantsRewriter::VariantsRewriter(const PosMatcher pos_matcher)
    : pos_matcher_(pos_matcher) {}

VariantsRewriter::~VariantsRewriter() = default;

// static
void VariantsRewriter::SetDescriptionForCandidate(
    const PosMatcher &pos_matcher, Segment::Candidate *candidate) {
  SetDescription(
      pos_matcher,
      (FULL_HALF_WIDTH | CHARACTER_FORM | ZIPCODE | SPELLING_CORRECTION),
      candidate);
}

// static
void VariantsRewriter::SetDescriptionForTransliteration(
    const PosMatcher &pos_matcher, Segment::Candidate *candidate) {
  SetDescription(pos_matcher,
                 (FULL_HALF_WIDTH | FULL_HALF_WIDTH_WITH_UNKNOWN |
                  CHARACTER_FORM | SPELLING_CORRECTION),
                 candidate);
}

// static
void VariantsRewriter::SetDescriptionForPrediction(
    const PosMatcher &pos_matcher, Segment::Candidate *candidate) {
  SetDescription(pos_matcher, ZIPCODE | SPELLING_CORRECTION, candidate);
}

// static
void VariantsRewriter::SetDescription(const PosMatcher &pos_matcher,
                                      int description_type,
                                      Segment::Candidate *candidate) {
  absl::string_view character_form_message;

  // Add Character form.
  if (description_type & CHARACTER_FORM) {
    const Util::ScriptType type =
        Util::GetScriptTypeWithoutSymbols(candidate->value);
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
        break;
      case Util::NUMBER:
        // character_form_message = "数字";
        character_form_message = kNumber;
        break;
      case Util::ALPHABET:
        // character_form_message = "アルファベット";
        character_form_message = kAlphabet;
        break;
      case Util::KANJI:
      case Util::EMOJI:
        // don't need to have full/half annotation for kanji and emoji,
        // since it's obvious
        description_type &= ~FULL_HALF_WIDTH;
        break;
      case Util::UNKNOWN_SCRIPT:  // mixed character
        if ((description_type & FULL_HALF_WIDTH_WITH_UNKNOWN) ||
            HasCharacterFormDescription(candidate->value)) {
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
  if (!candidate->description.empty()) {
    character_form_message = absl::string_view();
  }

  std::string description;
  // full/half char description
  if (description_type & FULL_HALF_WIDTH) {
    const Util::FormType form = Util::GetFormType(candidate->value);
    switch (form) {
      case Util::FULL_WIDTH:
        // description = "[全]";
        description = std::string(kFullWidth);
        break;
      case Util::HALF_WIDTH:
        // description = "[半]";
        description = std::string(kHalfWidth);
        break;
      default:
        break;
    }
  } else if (description_type & FULL_WIDTH) {
    // description = "[全]";
    description = std::string(kFullWidth);
  } else if (description_type & HALF_WIDTH) {
    // description = "[半]";
    description = std::string(kHalfWidth);
  }

  // add character_form_message
  AppendString(character_form_message, &description);

  // add main message
  if (candidate->value == "\\" ||
      candidate->value == "＼") {  // full-width backslash
    // if "\" (half-width backslash) or "＼" ()
    AppendString("バックスラッシュ", &description);
  } else if (candidate->value == "¥") {
    // if "¥" (half-width Yen sign), append kYenKigou.
    AppendString(kYenKigou, &description);
  } else if (candidate->value == "￥") {
    // if "￥" (full-width Yen sign), append only kYenKigou
    AppendString(kYenKigou, &description);
  } else {
    AppendString(candidate->description, &description);
  }

  // The follwoing description tries to overwrite existing description.
  // TODO(taku): reconsider this behavior.
  // Zipcode description
  if ((description_type & ZIPCODE) && pos_matcher.IsZipcode(candidate->lid) &&
      candidate->lid == candidate->rid) {
    description = candidate->content_key;
    // Append default description because it may contain extra description.
    AppendString(candidate->description, &description);
  }

  // The follwoing description tries to overwrite existing description.
  // TODO(taku): reconsider this behavior.
  // Spelling Correction description
  if ((description_type & SPELLING_CORRECTION) &&
      (candidate->attributes & Segment::Candidate::SPELLING_CORRECTION)) {
    description = std::string(kDidYouMean);
    // Add prefix to distinguish this candidate.
    candidate->prefix = "→ ";
    // Append default description because it may contain extra description.
    AppendString(candidate->description, &description);
  }

  // set new description
  candidate->description = std::move(description);
  candidate->attributes |= Segment::Candidate::NO_EXTRA_DESCRIPTION;
}

int VariantsRewriter::capability(const ConversionRequest &request) const {
  return RewriterInterface::ALL;
}

bool VariantsRewriter::RewriteSegment(RewriteType type, Segment *seg) const {
  CHECK(seg);
  bool modified = false;

  // Meta Candidate
  for (size_t i = 0; i < seg->meta_candidates_size(); ++i) {
    Segment::Candidate *candidate =
        seg->mutable_candidate(-static_cast<int>(i) - 1);
    DCHECK(candidate);
    if (candidate->attributes & Segment::Candidate::NO_EXTRA_DESCRIPTION) {
      continue;
    }
    SetDescriptionForTransliteration(pos_matcher_, candidate);
  }

  // Regular Candidate
  std::string default_value, alternative_value;
  std::string default_content_value, alternative_content_value;
  std::vector<uint32_t> default_inner_segment_boundary;
  std::vector<uint32_t> alternative_inner_segment_boundary;
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    Segment::Candidate *original_candidate = seg->mutable_candidate(i);
    DCHECK(original_candidate);

    if (original_candidate->attributes &
        Segment::Candidate::NO_EXTRA_DESCRIPTION) {
      continue;
    }

    if (original_candidate->attributes &
        Segment::Candidate::NO_VARIANTS_EXPANSION) {
      SetDescriptionForCandidate(pos_matcher_, original_candidate);
      VLOG(1) << "Candidate has NO_NORMALIZATION node";
      continue;
    }

    if (!GenerateAlternatives(*original_candidate, &default_value,
                              &alternative_value, &default_content_value,
                              &alternative_content_value,
                              &default_inner_segment_boundary,
                              &alternative_inner_segment_boundary)) {
      SetDescriptionForCandidate(pos_matcher_, original_candidate);
      continue;
    }

    CharacterFormManager::FormType default_form =
        CharacterFormManager::UNKNOWN_FORM;
    CharacterFormManager::FormType alternative_form =
        CharacterFormManager::UNKNOWN_FORM;

    int default_description_type =
        (CHARACTER_FORM | ZIPCODE | SPELLING_CORRECTION);

    int alternative_description_type =
        (CHARACTER_FORM | ZIPCODE | SPELLING_CORRECTION);

    if (CharacterFormManager::GetFormTypesFromStringPair(
            default_value, &default_form, alternative_value,
            &alternative_form)) {
      if (default_form == CharacterFormManager::HALF_WIDTH) {
        default_description_type |= HALF_WIDTH;
      } else if (default_form == CharacterFormManager::FULL_WIDTH) {
        default_description_type |= FULL_WIDTH;
      }
      if (alternative_form == CharacterFormManager::HALF_WIDTH) {
        alternative_description_type |= HALF_WIDTH;
      } else if (alternative_form == CharacterFormManager::FULL_WIDTH) {
        alternative_description_type |= FULL_WIDTH;
      }
    } else {
      default_description_type |= FULL_HALF_WIDTH;
      alternative_description_type |= FULL_HALF_WIDTH;
    }

    if (type == EXPAND_VARIANT) {
      // Insert default candidate to position |i| and
      // rewrite original(|i+1|) to altenative
      Segment::Candidate *new_candidate = seg->insert_candidate(i);
      DCHECK(new_candidate);

      new_candidate->Init();
      new_candidate->key = original_candidate->key;
      new_candidate->value = default_value;
      new_candidate->content_key = original_candidate->content_key;
      new_candidate->content_value = default_content_value;
      new_candidate->consumed_key_size = original_candidate->consumed_key_size;
      new_candidate->cost = original_candidate->cost;
      new_candidate->structure_cost = original_candidate->structure_cost;
      new_candidate->lid = original_candidate->lid;
      new_candidate->rid = original_candidate->rid;
      new_candidate->description = original_candidate->description;
      new_candidate->inner_segment_boundary.swap(
          default_inner_segment_boundary);
      new_candidate->attributes = original_candidate->attributes;
      SetDescription(pos_matcher_, default_description_type, new_candidate);

      original_candidate->value = alternative_value;
      original_candidate->content_value = alternative_content_value;
      original_candidate->inner_segment_boundary.swap(
          alternative_inner_segment_boundary);
      SetDescription(pos_matcher_, alternative_description_type,
                     original_candidate);
      ++i;  // skip inserted candidate
    } else if (type == SELECT_VARIANT) {
      // Rewrite original to default
      original_candidate->value = default_value;
      original_candidate->content_value = default_content_value;
      original_candidate->inner_segment_boundary.swap(
          default_inner_segment_boundary);
      SetDescription(pos_matcher_, default_description_type,
                     original_candidate);
    }
    modified = true;
  }
  return modified;
}

// Try generating default and alternative character forms.  Inner segment
// boundary is taken into account.  When no rewrite happens, false is returned.
bool VariantsRewriter::GenerateAlternatives(
    const Segment::Candidate &original, std::string *default_value,
    std::string *alternative_value, std::string *default_content_value,
    std::string *alternative_content_value,
    std::vector<uint32_t> *default_inner_segment_boundary,
    std::vector<uint32_t> *alternative_inner_segment_boundary) const {
  default_value->clear();
  alternative_value->clear();
  default_content_value->clear();
  alternative_content_value->clear();
  default_inner_segment_boundary->clear();
  alternative_inner_segment_boundary->clear();

  const config::CharacterFormManager *manager =
      CharacterFormManager::GetCharacterFormManager();

  // TODO(noriyukit): Some rewriter may rewrite key and/or value and make the
  // inner segment boundary inconsistent.  Ideally, it should always be valid.
  // Accessing inner segments with broken boundary information is very
  // dangerous. So here checks the validity.  For invalid candidate, inner
  // segment boundary is ignored.
  const bool is_valid = original.IsValid();
  if (!is_valid) {
    VLOG(2) << "Invalid candidate: " << original.DebugString();
  }
  if (original.inner_segment_boundary.empty() || !is_valid) {
    if (!manager->ConvertConversionStringWithAlternative(
            original.value, default_value, alternative_value)) {
      return false;
    }
    if (original.value != original.content_value) {
      manager->ConvertConversionStringWithAlternative(
          original.content_value, default_content_value,
          alternative_content_value);
    } else {
      default_content_value->assign(*default_value);
      alternative_content_value->assign(*alternative_value);
    }
    return true;
  }

  // When inner segment boundary is present, rewrite each inner segment.  If at
  // least one inner segment is rewritten, the whole segment is considered
  // rewritten.
  bool at_least_one_modified = false;
  std::string tmp, inner_default_value, inner_alternative_value;
  std::string inner_default_content_value, inner_alternative_content_value;
  for (Segment::Candidate::InnerSegmentIterator iter(&original); !iter.Done();
       iter.Next()) {
    tmp.assign(iter.GetValue().data(), iter.GetValue().size());
    inner_default_value.clear();
    inner_alternative_value.clear();
    if (!manager->ConvertConversionStringWithAlternative(
            tmp, &inner_default_value, &inner_alternative_value)) {
      inner_default_value.assign(iter.GetValue().data(),
                                 iter.GetValue().size());
      inner_alternative_value.assign(iter.GetValue().data(),
                                     iter.GetValue().size());
    } else {
      at_least_one_modified = true;
    }
    if (iter.GetValue() != iter.GetContentValue()) {
      inner_default_content_value.clear();
      inner_alternative_content_value.clear();
      tmp.assign(iter.GetContentValue().data(), iter.GetContentValue().size());
      manager->ConvertConversionStringWithAlternative(
          tmp, &inner_default_content_value, &inner_alternative_content_value);
    } else {
      inner_default_content_value = inner_default_value;
      inner_alternative_content_value = inner_alternative_value;
    }
    default_value->append(inner_default_value);
    alternative_value->append(inner_alternative_value);
    default_content_value->append(inner_default_content_value);
    alternative_content_value->append(inner_alternative_content_value);
    default_inner_segment_boundary->push_back(Segment::Candidate::EncodeLengths(
        iter.GetKey().size(), inner_default_value.size(),
        iter.GetContentKey().size(), inner_default_content_value.size()));
    alternative_inner_segment_boundary->push_back(
        Segment::Candidate::EncodeLengths(
            iter.GetKey().size(), inner_alternative_value.size(),
            iter.GetContentKey().size(),
            inner_alternative_content_value.size()));
  }
  return at_least_one_modified;
}

void VariantsRewriter::Finish(const ConversionRequest &request,
                              Segments *segments) {
  if (!request.request().mixed_conversion() &&
      segments->request_type() != Segments::CONVERSION) {
    return;
  }

  // save character form
  for (int i = 0; i < segments->conversion_segments_size(); ++i) {
    const Segment &segment = segments->conversion_segment(i);
    if (segment.candidates_size() <= 0 ||
        segment.segment_type() != Segment::FIXED_VALUE ||
        segment.candidate(0).attributes &
            Segment::Candidate::NO_HISTORY_LEARNING) {
      continue;
    }

    const Segment::Candidate &candidate = segment.candidate(0);
    if (candidate.attributes & Segment::Candidate::NO_VARIANTS_EXPANSION) {
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
  } else if (segments->request_type() == Segments::SUGGESTION) {
    type = SELECT_VARIANT;
  } else {
    type = EXPAND_VARIANT;
  }

  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    modified |= RewriteSegment(type, seg);
  }

  return modified;
}

}  // namespace mozc
