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

#include "rewriter/variants_rewriter.h"

#include <string>
#include <vector>
#include "base/base.h"
#include "base/util.h"
#include "converter/segments.h"
#include "converter/pos_matcher.h"
#include "converter/character_form_manager.h"

namespace mozc {
namespace {
// "ひらがな"
const char kHiragana[] = "\xE3\x81\xB2\xE3\x82\x89\xE3\x81\x8C\xE3\x81\xAA";
// "カタカナ"
const char kKatakana[] = "\xE3\x82\xAB\xE3\x82\xBF\xE3\x82\xAB\xE3\x83\x8A";
// "数字"
const char kNumber[] = "\xE6\x95\xB0\xE5\xAD\x97";
// "アルファベット"
const char kAlphabet[] = "\xE3\x82\xA2\xE3\x83\xAB\xE3\x83\x95\xE3\x82\xA1"
    "\xE3\x83\x99\xE3\x83\x83\xE3\x83\x88";
// "漢字"
const char kKanji[] = "\xe6\xbc\xa2\xe5\xad\x97";

// "[全]"
const char kFullWidth[] = "[\xE5\x85\xA8]";

// "[半]"
const char kHalfWidth[] = "[\xE5\x8D\x8A]";

// "<機種依存文字>"
const char kPlatformDependent[] = "<\xE6\xA9\x9F\xE7\xA8\xAE"
    "\xE4\xBE\x9D\xE5\xAD\x98\xE6\x96\x87\xE5\xAD\x97>";

// "<もしかして>"
const char kDidYouMean[] =
    "<\xE3\x82\x82\xE3\x81\x97\xE3\x81\x8B\xE3\x81\x97\xE3\x81\xA6>";

// Append |src| to |dst| with a separator ' '.
void AppendString(const string &src, string *dst) {
  CHECK(dst);
  if (!src.empty()) {
    if (!dst->empty()) {
      dst->append(" ");
    }
    dst->append(src);
  }
}

// Return true if all charcters in |value| are UNKNOWN_SCRIPT
// and FormType of |value| are consistent, e.g. all fullwith or
// all halfwidth.
// Example:
// "&-()" => true (all symbol and all half)
// "&-（）" => false (all symbol but contains both full/half width)
// "google" => false (not symbol)
bool HasCharacterFormDescription(const string &value) {
  if (value.empty()) {
    return false;
  }
  const char *begin = value.data();
  const char *end = value.data() + value.size();
  Util::FormType prev = Util::UNKNOWN_FORM;
  while (begin < end) {
    size_t mblen = 0;
    const uint16 ucs2 = Util::UTF8ToUCS2(begin, end, &mblen);
    const Util::FormType type = Util::GetFormType(ucs2);
    if (prev != Util::UNKNOWN_FORM && prev != type) {
      return false;
    }
    if (Util::UNKNOWN_SCRIPT != Util::GetScriptType(ucs2)) {
      return false;
    }
    prev = type;
    begin += mblen;
  }
  return true;
}
}  // namespace

VariantsRewriter::VariantsRewriter() {}

VariantsRewriter::~VariantsRewriter() {}

// static
void VariantsRewriter::SetDescriptionForCandidate(
    Segment::Candidate *candidate) {
  SetDescription(FULL_HALF_WIDTH |
                 CHARACTER_FORM |
                 PLATFORM_DEPENDENT_CHARACTER |
                 ZIPCODE |
                 SPELLING_CORRECTION,
                 candidate);
}

// static
void VariantsRewriter::SetDescriptionForTransliteration(
    Segment::Candidate *candidate) {
  SetDescription(FULL_HALF_WIDTH |
                 FULL_HALF_WIDTH_WITH_UNKNOWN |
                 CHARACTER_FORM |
                 PLATFORM_DEPENDENT_CHARACTER |
                 SPELLING_CORRECTION,
                 candidate);
}

// static
void VariantsRewriter::SetDescriptionForPrediction(
    Segment::Candidate *candidate) {
  SetDescription(PLATFORM_DEPENDENT_CHARACTER |
                 ZIPCODE |
                 SPELLING_CORRECTION,
                 candidate);
}

// static
void VariantsRewriter::SetDescription(int description_type,
                                      Segment::Candidate *candidate) {
  string description;
  string character_form_message;

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
        // don't need to have full/half annotation for kanji,
        // since it's obvious
        description_type &= ~FULL_HALF_WIDTH;
        break;
      case Util::UNKNOWN_SCRIPT:   // mixed character
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
    character_form_message.clear();
  }

  // full/half char description
  if (description_type & FULL_HALF_WIDTH) {
    const Util::FormType form = Util::GetFormType(candidate->value);
    switch (form) {
      case Util::FULL_WIDTH:
        // description = "[全]";
        description = kFullWidth;
        break;
      case Util::HALF_WIDTH:
        // description = "[半]";
        description = kHalfWidth;
        break;
      default:
        break;
    }
  } else if (description_type & FULL_WIDTH) {
    // description = "[全]";
    description = kFullWidth;
  } else if (description_type & HALF_WIDTH) {
    // description = "[半]";
    description = kHalfWidth;
  }

  // add character_form_message
  AppendString(character_form_message, &description);

  // add main message
  AppendString(candidate->description, &description);

  // Platform dependent char description
  if (description_type & PLATFORM_DEPENDENT_CHARACTER &&
      Util::GetCharacterSet(candidate->value) >= Util::JISX0212) {
    AppendString(kPlatformDependent, &description);
  }

  // The follwoing description tries to overwrite exisiting description.
  // TODO(taku): reconsider this behavior.
  // Zipcode description
  if ((description_type & ZIPCODE) &&
      POSMatcher::IsZipcode(candidate->lid) &&
      candidate->lid == candidate->rid) {
    description = candidate->content_key;
    // Append default description because it may contain extra description.
    AppendString(candidate->description, &description);
  }

  // The follwoing description tries to overwrite exisiting description.
  // TODO(taku): reconsider this behavior.
  // Spelling Correction description
  if ((description_type & SPELLING_CORRECTION) &&
      (candidate->attributes & Segment::Candidate::SPELLING_CORRECTION)) {
    description = kDidYouMean;
    // Add prefix to distinguish this candidate.
    candidate->prefix = "\xE2\x86\x92 ";  // "→ "
    // Append default description because it may contain extra description.
    AppendString(candidate->description, &description);
  }

  // set new description
  candidate->description = description;
  candidate->attributes |= Segment::Candidate::NO_EXTRA_DESCRIPTION;
}

bool VariantsRewriter::RewriteSegment(Segment *seg) const {
  CHECK(seg);
  bool modified = false;

  // Meta Candidate
  for (size_t i = 0; i < seg->meta_candidates_size(); ++i) {
    Segment::Candidate *candidate =
        seg->mutable_candidate(-static_cast<int>(i)-1);
    DCHECK(candidate);
    if (candidate->attributes & Segment::Candidate::NO_EXTRA_DESCRIPTION) {
      continue;
    }
    SetDescriptionForTransliteration(candidate);
  }

  // Regular Candidate
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    Segment::Candidate *original_candidate = seg->mutable_candidate(i);
    DCHECK(original_candidate);

    if (original_candidate->attributes &
        Segment::Candidate::NO_EXTRA_DESCRIPTION) {
      continue;
    }

    if (original_candidate->attributes &
        Segment::Candidate::NO_VARIANTS_EXPANSION) {
      SetDescriptionForCandidate(original_candidate);
      VLOG(1) << "Canidate has NO_NORMALIZATION node";
      continue;
    }

    string default_value, alternative_value;
    if (!CharacterFormManager::GetCharacterFormManager()->
        ConvertConversionStringWithAlternative(
            original_candidate->value,
            &default_value, &alternative_value)) {
      SetDescriptionForCandidate(original_candidate);
      VLOG(1) << "ConvertConversionStringWithAlternative failed";
      continue;
    }

    string default_content_value, alternative_content_value;
    if (original_candidate->value != original_candidate->content_value) {
      CharacterFormManager::GetCharacterFormManager()->
          ConvertConversionStringWithAlternative(
              original_candidate->content_value,
              &default_content_value, &alternative_content_value);
    } else {
      default_content_value = default_value;
      alternative_content_value = alternative_value;
    }

    CharacterFormManager::FormType default_form
        = CharacterFormManager::UNKNOWN_FORM;
    CharacterFormManager::FormType alternative_form
        = CharacterFormManager::UNKNOWN_FORM;

    int default_description_type =
        (CHARACTER_FORM | PLATFORM_DEPENDENT_CHARACTER |
         ZIPCODE | SPELLING_CORRECTION);

    int alternative_description_type =
        (CHARACTER_FORM | PLATFORM_DEPENDENT_CHARACTER |
         ZIPCODE | SPELLING_CORRECTION);

    if (CharacterFormManager::GetFormTypesFromStringPair(
            default_value,
            &default_form,
            alternative_value,
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
      default_description_type     |= FULL_HALF_WIDTH;
      alternative_description_type |= FULL_HALF_WIDTH;
    }

    modified = true;
    Segment::Candidate *new_candidate = seg->insert_candidate(i);
    DCHECK(new_candidate);

    new_candidate->Init();
    new_candidate->key = original_candidate->key;
    new_candidate->value = default_value;
    new_candidate->content_key = original_candidate->content_key;
    new_candidate->content_value = default_content_value;
    new_candidate->cost = original_candidate->cost;
    new_candidate->structure_cost = original_candidate->structure_cost;
    new_candidate->lid = original_candidate->lid;
    new_candidate->rid = original_candidate->rid;
    new_candidate->description = original_candidate->description;
    SetDescription(default_description_type, new_candidate);

    original_candidate->value = alternative_value;
    original_candidate->content_value = alternative_content_value;
    SetDescription(alternative_description_type, original_candidate);

    ++i;  // skip one item
  }

  return modified;
}

void VariantsRewriter::Finish(Segments *segments) {
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
      case Segment::Candidate::NUMBER_SEPARATED_ARABIC_HALFWIDTH:
        // treat NUMBER_SEPARATED_ARABIC as half_width num
        CharacterFormManager::GetCharacterFormManager()->
            SetCharacterForm("0", config::Config::HALF_WIDTH);
        break;
      case Segment::Candidate::NUMBER_SEPARATED_ARABIC_FULLWIDTH:
        // treat NUMBER_SEPARATED_WIDE_ARABIC as full_width num
        CharacterFormManager::GetCharacterFormManager()->
            SetCharacterForm("0", config::Config::FULL_WIDTH);
        break;
      default:
        CharacterFormManager::GetCharacterFormManager()->
            GuessAndSetCharacterForm(candidate.value);
        break;
    }
  }
}

void VariantsRewriter::Clear() {
  CharacterFormManager::GetCharacterFormManager()->ClearHistory();
}

bool VariantsRewriter::Rewrite(Segments *segments) const {
  CHECK(segments);
  bool modified = false;

  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    modified |= RewriteSegment(seg);
  }

  return modified;
}
}  // namespace mozc
