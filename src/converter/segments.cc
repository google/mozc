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

#include <string>
#include <algorithm>
#include <sstream>
#include "base/base.h"
#include "base/mutex.h"
#include "base/util.h"
#include "base/freelist.h"
#include "transliteration/transliteration.h"
#include "converter/character_form_manager.h"
#include "converter/lattice.h"
#include "converter/nbest_generator.h"
#include "converter/node.h"
#include "converter/pos_matcher.h"
#include "converter/segments.h"

namespace mozc {

namespace {

static const size_t kMaxHistorySize = 32;

// Ad-hoc function to detect English candidate
bool IsKatakanaT13NValue(const string &value) {
  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == 0x20 || value[i] == 0x21 || value[i] == 0x2D ||
        // " ", "!", "-"
        (value[i] >= 0x41 && value[i] <= 0x5A) || // A..Z
        (value[i] >= 0x61 && value[i] <= 0x7A)) { // a..z
      // do nothing
    } else {
      return false;
    }
  }
  return true;
}

bool IsKatakanaT13N(const Segment::Candidate &candidate) {
  return (Util::GetScriptType(candidate.content_key) == Util::HIRAGANA &&
          IsKatakanaT13NValue(candidate.content_value));
}
}  // namespace

// "ひらがな"
static char kHiragana[] = "\xE3\x81\xB2\xE3\x82\x89\xE3\x81\x8C\xE3\x81\xAA";
// "カタカナ"
static char kKatakana[] = "\xE3\x82\xAB\xE3\x82\xBF\xE3\x82\xAB\xE3\x83\x8A";
// "数字"
static char kNumber[] = "\xE6\x95\xB0\xE5\xAD\x97";
// "アルファベット"
static char kAlphabet[] = "\xE3\x82\xA2\xE3\x83\xAB\xE3\x83\x95\xE3\x82\xA1"
                          "\xE3\x83\x99\xE3\x83\x83\xE3\x83\x88";
// "漢字"
static char kKanji[] = "\xe6\xbc\xa2\xe5\xad\x97";


void Segment::Candidate::SetDefaultDescription(int description_type) {
  string message;

  if (description_type & Segment::Candidate::CHARACTER_FORM) {
    const string &value = this->value;
    const Util::ScriptType type = Util::GetScriptTypeWithoutSymbols(value);
    switch (type) {
      case Util::HIRAGANA:
        message = kHiragana;
        // don't need to set full/half, because hiragana only has
        // full form
        description_type &= ~Segment::Candidate::FULL_HALF_WIDTH;
        break;
      case Util::KATAKANA:
        // message = "カタカナ";
        message = kKatakana;
        break;
      case Util::NUMBER:
        // message = "数字";
        message = kNumber;
        break;
      case Util::ALPHABET:
        // message = "アルファベット";
        message = kAlphabet;
        break;
      case Util::KANJI:
        // don't need to have annotation for kanji, since it's obvious
        description_type &= ~Segment::Candidate::FULL_HALF_WIDTH;
        break;
      case Util::UNKNOWN_SCRIPT:   // mixed character
        description_type &= ~Segment::Candidate::FULL_HALF_WIDTH;
        break;
      default:
        break;
    }
  }

  SetDescription(description_type, message);
}

void Segment::Candidate::SetTransliterationDescription() {
  string message;

  int description_type = Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER;

  const string &value = this->value;
  const Util::ScriptType type = Util::GetScriptType(value);
  switch (type) {
    case Util::HIRAGANA:
      message = kHiragana;
      break;
    case Util::KATAKANA:
      // message = "カタカナ";
      message = kKatakana;
      description_type |= Segment::Candidate::FULL_HALF_WIDTH;
      break;
    case Util::NUMBER:
      // message = "数字";
      message = kNumber;
      description_type |= Segment::Candidate::FULL_HALF_WIDTH;
      break;
    case Util::ALPHABET:
      // message = "アルファベット";
      message = kAlphabet;
      description_type |= Segment::Candidate::FULL_HALF_WIDTH;
      break;
    case Util::KANJI:
      // message = "漢字";
      message = kKanji;
      break;
    case Util::UNKNOWN_SCRIPT:   // mixed character
      // ex. "Mozc!", "＄１００", etc.
      description_type |= Segment::Candidate::FULL_HALF_WIDTH;
      break;
    default:
      break;
  }

  SetDescription(description_type, message);
}

void Segment::Candidate::SetDescription(
    int description_type,
    const string &message) {
  description_message = message;
  ResetDescription(description_type);
}

void Segment::Candidate::ResetDescription(int description_type) {
  description.clear();

  // full/half char description
  if (description_type & Segment::Candidate::FULL_HALF_WIDTH) {
    const Util::FormType form = Util::GetFormType(value);
    switch (form) {
      case Util::FULL_WIDTH:
        // description = "[全]";
        description = "[\xE5\x85\xA8]";
        break;
      case Util::HALF_WIDTH:
        // description = "[半]";
        description = "[\xE5\x8D\x8A]";
      break;
    default:
      break;
    }
  }

  // add main message
  if (!description_message.empty()) {
    if (!description.empty()) {
      description += " ";
    }
    description += description_message;
  }

  // Platform dependent char description
  if (description_type & Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER) {
    const Util::CharacterSet cset = Util::GetCharacterSet(value);
    if (cset >= Util::JISX0212) {
      if (!description.empty()) {
        description += " ";
      }
      //      description += "<機種依存文字>";
      description += "<\xE6\xA9\x9F\xE7\xA8\xAE"
          "\xE4\xBE\x9D\xE5\xAD\x98\xE6\x96\x87\xE5\xAD\x97>";
    }
  }

  // Zipcode description
  if ((description_type & Segment::Candidate::ZIPCODE) &&
      POSMatcher::IsZipcode(lid) && lid == rid) {
    description = content_key;
  }

  // Spelling Correction description
  if (is_spelling_correction) {
    // description = "<もしかして>"
    description = "<\xE3\x82\x82\xE3\x81\x97\xE3\x81\x8B\xE3\x81\x97"
        "\xE3\x81\xA6>";
  }
}

string Segment::Candidate::functional_key() const {
  if (key.size() <= content_key.size()) {
    return "";
  }
  return key.substr(content_key.size(),
                    key.size() - content_key.size());
}

string Segment::Candidate::functional_value() const {
  if (value.size() <= content_value.size()) {
    return "";
  }
  return value.substr(content_value.size(),
                      value.size() - content_value.size());
}

Segment::Segment()
    : segment_type_(FREE),
      requested_candidates_size_(0),
      nbest_generator_(new NBestGenerator),
      pool_(new ObjectPool<Candidate>(16)),
      initialized_transliterations_(false),
      all_expanded_(false),
      katakana_t13n_length_(0) {}

Segment::~Segment() {}

const Segment::SegmentType Segment::segment_type() const {
  return segment_type_;
}

Segment::SegmentType *Segment::mutable_segment_type() {
  return &segment_type_;
}

void Segment::set_segment_type(
    const Segment::SegmentType &segment_type) {
  segment_type_ = segment_type;
}

const string& Segment::key() const {
  return key_;
}

namespace {
void InitT13NCandidate(const string &key,
                       const string &value,
                       Segment::Candidate *cand) {
  cand->Init();
  cand->value = value;
  cand->content_value = value;
  cand->content_key = key;
  cand->SetTransliterationDescription();
}
}  // namespace

void Segment::set_key(const string &key) {
  key_ = key;

  const string &hiragana = key;
  string full_katakana, half_ascii, full_ascii, half_katakana;
  Util::HiraganaToKatakana(hiragana, &full_katakana);
  Util::HiraganaToRomanji(hiragana, &half_ascii);
  Util::HiraganaToFullwidthRomanji(hiragana, &full_ascii);
  Util::HiraganaToHalfwidthKatakana(hiragana, &half_katakana);

  meta_candidates_.resize(transliteration::NUM_T13N_TYPES);

  InitT13NCandidate(key, hiragana,
                    &(meta_candidates_[transliteration::HIRAGANA]));

  InitT13NCandidate(key, full_katakana,
                    &(meta_candidates_[transliteration::FULL_KATAKANA]));

  InitT13NCandidate(key, half_ascii,
                    &(meta_candidates_[transliteration::HALF_ASCII]));

  // This function is used for a fail-safe.  So UPPER, LOWER,
  // CAPITALIZED cases are omitted and the as-is case is used.
  InitT13NCandidate(key, half_ascii,
                    &(meta_candidates_[transliteration::HALF_ASCII_UPPER]));

  InitT13NCandidate(key, half_ascii,
                    &(meta_candidates_[transliteration::HALF_ASCII_LOWER]));

  InitT13NCandidate(
      key, half_ascii,
      &(meta_candidates_[transliteration::HALF_ASCII_CAPITALIZED]));

  InitT13NCandidate(key, full_ascii,
                    &(meta_candidates_[transliteration::FULL_ASCII]));

  InitT13NCandidate(key, full_ascii,
                    &(meta_candidates_[transliteration::FULL_ASCII_UPPER]));

  InitT13NCandidate(key, full_ascii,
                    &(meta_candidates_[transliteration::FULL_ASCII_LOWER]));

  InitT13NCandidate(
      key, full_ascii,
      &(meta_candidates_[transliteration::FULL_ASCII_CAPITALIZED]));

  InitT13NCandidate(key, half_katakana,
                    &(meta_candidates_[transliteration::HALF_KATAKANA]));
}

void Segment::SetTransliterations(const vector<string> &t13ns) {
  meta_candidates_.resize(transliteration::NUM_T13N_TYPES);

  InitT13NCandidate(key_, t13ns[transliteration::HIRAGANA],
                    &(meta_candidates_[transliteration::HIRAGANA]));

  InitT13NCandidate(key_, t13ns[transliteration::FULL_KATAKANA],
                    &(meta_candidates_[transliteration::FULL_KATAKANA]));

  InitT13NCandidate(key_, t13ns[transliteration::HALF_ASCII],
                    &(meta_candidates_[transliteration::HALF_ASCII]));

  InitT13NCandidate(key_, t13ns[transliteration::HALF_ASCII_UPPER],
                    &(meta_candidates_[transliteration::HALF_ASCII_UPPER]));

  InitT13NCandidate(key_, t13ns[transliteration::HALF_ASCII_LOWER],
                    &(meta_candidates_[transliteration::HALF_ASCII_LOWER]));

  InitT13NCandidate(
      key_, t13ns[transliteration::HALF_ASCII_CAPITALIZED],
      &(meta_candidates_[transliteration::HALF_ASCII_CAPITALIZED]));

  InitT13NCandidate(key_, t13ns[transliteration::FULL_ASCII],
                    &(meta_candidates_[transliteration::FULL_ASCII]));

  InitT13NCandidate(key_, t13ns[transliteration::FULL_ASCII_UPPER],
                    &(meta_candidates_[transliteration::FULL_ASCII_UPPER]));

  InitT13NCandidate(key_, t13ns[transliteration::FULL_ASCII_LOWER],
                    &(meta_candidates_[transliteration::FULL_ASCII_LOWER]));

  InitT13NCandidate(
      key_, t13ns[transliteration::FULL_ASCII_CAPITALIZED],
      &(meta_candidates_[transliteration::FULL_ASCII_CAPITALIZED]));

  InitT13NCandidate(key_, t13ns[transliteration::HALF_KATAKANA],
                    &(meta_candidates_[transliteration::HALF_KATAKANA]));

  initialized_transliterations_ = true;
}


NBestGenerator *Segment::nbest_generator() const {
  return nbest_generator_.get();
}

const Segment::Candidate &Segment::candidate(int i) const {
  if (i < 0) {
    return meta_candidate(-i-1);
  }
  return *candidates_[i];
}

const Segment::Candidate &Segment::meta_candidate(size_t i) const {
  if (i >= meta_candidates_.size()) {
    LOG(ERROR) << "Invalid index number of meta_candidate: " << i;
    i = 0;
  }
  return meta_candidates_[i];
}


Segment::Candidate *Segment::mutable_candidate(int i) {
  if (i < 0) {
    return &meta_candidates_[-i-1];
  }
  return candidates_[i];
}

int Segment::indexOf(const Segment::Candidate *candidate) {
  if (candidate == NULL) {
    return static_cast<int>(candidates_size());
  }

  for (int i = 0; i < static_cast<int>(candidates_.size()); ++i) {
    if (candidates_[i] == candidate) {
      return i;
    }
  }

  for (int i = 0; i < static_cast<int>(meta_candidates_.size()); ++i) {
    if (&(meta_candidates_[i]) == candidate) {
      return -i-1;
    }
  }

  return static_cast<int>(candidates_size());
}

size_t Segment::candidates_size() const {
  return candidates_.size();
}

size_t Segment::meta_candidates_size() const {
  return meta_candidates_.size();
}

size_t Segment::requested_candidates_size() const {
  return requested_candidates_size_;
}

void Segment::clear_candidates() {
  pool_->Free();
  candidates_.clear();
  requested_candidates_size_ = 0;
  all_expanded_ = false;
  katakana_t13n_length_ = 0;
}

Segment::Candidate *Segment::push_back_candidate() {
  Candidate *candidate = pool_->Alloc();
  candidate->Init();
  candidates_.push_back(candidate);
  return candidate;
}

Segment::Candidate *Segment::push_front_candidate() {
  Candidate *candidate = pool_->Alloc();
  candidate->Init();
  candidates_.push_front(candidate);
  return candidate;
}

Segment::Candidate *Segment::add_candidate() {
  return push_back_candidate();
}

Segment::Candidate *Segment::insert_candidate(int i) {
  if (i < 0 || i > static_cast<int>(candidates_.size())) {
    LOG(WARNING) << "invalid index";
    return NULL;
  }
  Candidate *candidate = pool_->Alloc();
  candidate->Init();
  candidates_.insert(candidates_.begin() + i, candidate);
  return candidate;
}

void Segment::pop_front_candidate() {
  if (!candidates_.empty()) {
    Candidate *c = candidates_.front();
    pool_->Release(c);
    candidates_.pop_front();
  }
}

void Segment::pop_back_candidate() {
  if (!candidates_.empty()) {
    Candidate *c = candidates_.back();
    pool_->Release(c);
    candidates_.pop_back();
  }
}

void Segment::erase_candidate(int i) {
  if (i < 0 || i >= static_cast<int>(candidates_size())) {
    LOG(WARNING) << "invalid index";
    return;
  }
  pool_->Release(mutable_candidate(i));
  candidates_.erase(candidates_.begin() + i);
}

void Segment::erase_candidates(int i, size_t size) {
  const size_t end = i + size;
  if (i <= 0 ||
      i >= static_cast<int>(candidates_size()) ||
      end > candidates_size()) {
    LOG(WARNING) << "invalid index";
    return;
  }
  for (int j = i; j < static_cast<int>(end); ++j) {
    pool_->Release(mutable_candidate(j));
  }
  candidates_.erase(candidates_.begin() + i,
                    candidates_.begin() + end);
}

void Segment::move_candidate(int old_idx, int new_idx) {
  // meta candidates
  if (old_idx < 0 && old_idx >= -transliteration::NUM_T13N_TYPES) {
    Candidate *c = insert_candidate(new_idx);
    *c = meta_candidates_[-old_idx - 1];
    return;
  }

  // normal segment
  if (old_idx < 0 || old_idx >= static_cast<int>(candidates_size()) ||
      new_idx >= static_cast<int>(candidates_size()) || old_idx == new_idx) {
    VLOG(1) << "old_idx and new_idx are the same";
    return;
  }
  if (old_idx > new_idx) {  // promotion
    Candidate *c = candidates_[old_idx];
    for (int i = old_idx; i >= new_idx + 1; --i) {
      candidates_[i] = candidates_[i - 1];
    }
    candidates_[new_idx] = c;
  } else {  // demotion
    Candidate *c = candidates_[old_idx];
    for (int i = old_idx; i <  new_idx; ++i) {
      candidates_[i] = candidates_[i + 1];
    }
    candidates_[new_idx] = c;
  }
}

void Segment::Clear() {
  clear();
}

void Segment::clear() {
  clear_candidates();
  key_.clear();
  meta_candidates_.clear();
  initialized_transliterations_ = false;
  segment_type_ = FREE;
  nbest_generator_->Reset();
}

bool Segment::GetCandidates(size_t size) {
  if (size > candidates_.size()) {
    return Expand(size - candidates_.size());
  }
  return true;
}

bool Segment::has_candidate_value(const string &value) const {
  for (size_t i = 0; i < candidates_.size(); ++i) {
    if (candidates_[i]->value == value) {
      return true;
    }
  }
  return false;
}

bool Segment::ExpandAlternative(int i) {
  if (i < 0 || i >= candidates_size()) {
    LOG(WARNING) << "invalid index";
    return false;
  }

  Candidate *org_c = mutable_candidate(i);
  if (org_c == NULL) {
    LOG(WARNING) << "org_c is NULL";
    return false;
  }

  if (!org_c->can_expand_alternative) {
    VLOG(1) << "Canidate has NO_NORMALIZATION node";
    return false;
  }

  string default_value, alternative_value;
  if (!CharacterFormManager::GetCharacterFormManager()->
      ConvertConversionStringWithAlternative(
          org_c->value,
          &default_value, &alternative_value)) {
    VLOG(1) << "ConvertConversionStringWithAlternative failed";
    return false;
  }

  string default_content_value,  alternative_content_value;
  if (org_c->value != org_c->content_value) {
    CharacterFormManager::GetCharacterFormManager()->
        ConvertConversionStringWithAlternative(
            org_c->content_value,
            &default_content_value, &alternative_content_value);
  } else {
    default_content_value = default_value;
    alternative_content_value = alternative_value;
  }

  Candidate *new_c = insert_candidate(i);
  if (new_c == NULL) {
    LOG(WARNING) << "new_c is NULL";
    return false;
  }

  new_c->Init();
  new_c->content_key = org_c->content_key;
  new_c->cost = org_c->cost;
  new_c->structure_cost = org_c->structure_cost;
  new_c->lid = org_c->lid;
  new_c->rid = org_c->rid;
  new_c->value = default_value;
  new_c->content_value = default_content_value;
  new_c->SetDefaultDescription(
      Segment::Candidate::FULL_HALF_WIDTH |
      Segment::Candidate::CHARACTER_FORM |
      Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER);

  org_c->value = alternative_value;
  org_c->content_value = alternative_content_value;
  org_c->description.clear();
  org_c->SetDefaultDescription(
      Segment::Candidate::FULL_HALF_WIDTH |
      Segment::Candidate::CHARACTER_FORM |
      Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER);

  return true;
}

// static
bool Segment::ExpandEnglishVariants(const string &input,
                                    vector<string> *variants) {
  if (input.empty()) {
    return false;
  }

  // multi-word
  if (input.find(" ") != string::npos) {
    return false;
  }

  string lower = input;
  string upper = input;
  string capitalized = input;
  Util::LowerString(&lower);
  Util::UpperString(&upper);
  Util::CapitalizeString(&capitalized);

  if (lower == upper) {
    // given word is non-ascii.
    return false;
  }

  variants->clear();
  if (input != lower && input != upper && input != capitalized) {
    variants->push_back(lower);
    return true;
  }

  if (input != lower) {
    variants->push_back(lower);
  }
  if (input != capitalized) {
    variants->push_back(capitalized);
  }
  if (input != upper) {
    variants->push_back(upper);
  }

  return true;
}

bool Segment::Expand(size_t size) {
  if (size <= 0) {
    LOG(WARNING) << "invalid size";
    return false;
  }

  if (nbest_generator_.get() == NULL) {
    LOG(WARNING) << "nbest_generator is NULL";
    return false;
  }

  // already expanded
  if (all_expanded_) {
    return true;
  }

  const size_t target_size = candidates_size() + size;
  if (requested_candidates_size() >= target_size) {
    // Avaliable candidates have been already generated.
    return true;
  }
  requested_candidates_size_ = target_size;

  // if NBestGenerator::Next() returns NULL,
  // no more entries are generated.
  while (candidates_size() < target_size) {
    Candidate *c = push_back_candidate();
    if (c == NULL) {
      LOG(ERROR) << "candidate is NULL";
      return false;
    }
    c->Init();

    if (!nbest_generator_->Next(c)) {
      pop_back_candidate();
      // set all_expanded_ to be true that Expand() is never called again.
      // http://b/issue?id=2868423
      all_expanded_ = true;   // no more entries
      break;
    }

    c->SetDefaultDescription(
        Segment::Candidate::FULL_HALF_WIDTH |
        Segment::Candidate::CHARACTER_FORM |
        Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER |
        Segment::Candidate::ZIPCODE);

    const vector<const Node *> &nodes = c->nodes;
    DCHECK(!nodes.empty());

    bool remove_candidate = false;

    // Check all nodes are DEFAULT_NORMALIZATION
    // Otherwise, don't call ExpandAlternative()
    for (size_t i = 0; i < nodes.size(); ++i) {
      const Node *node = nodes[i];
      // KatakanaT13N must be the prefix of the candidate.
      // TODO(taku): better to move this logic inside CandidateFilter.
      if (node != nodes[0] && IsKatakanaT13NValue(node->value)) {
        remove_candidate = true;
        break;
      }
    }

    if (remove_candidate) {
      pop_back_candidate();
      continue;   // next while() loop
    }

    // Make KatakanaT13n Candidates
    if (IsKatakanaT13N(*c)) {
      c->can_expand_alternative = false;
      if (katakana_t13n_length_ != 0 &&
          katakana_t13n_length_ != c->content_key.size()) {
        VLOG(1) << c->value << " is remvoed";
        pop_back_candidate();
        continue;   // next while() loop
      } else {
        vector<string> variants;
        // TODO(taku): could be possible to move this logic to Rewriter.
        if (Segment::ExpandEnglishVariants(c->content_value, &variants)) {
          katakana_t13n_length_ = c->content_key.size();
          for (size_t i = 0; i < variants.size(); ++i) {
            Candidate *variant_c = add_candidate();
            DCHECK(variant_c);
            variant_c->Init();
            variant_c->value = variants[i] +
                c->value.substr(c->content_value.size(),
                                c->value.size() - c->content_value.size());
            variant_c->content_value = variants[i];
            variant_c->content_key = c->content_key;
            variant_c->cost = c->cost + 1;
            variant_c->structure_cost = c->structure_cost + 1;
            variant_c->nodes = c->nodes;
            variant_c->lid = c->lid;
            variant_c->rid = c->rid;
            variant_c->can_expand_alternative = false;
            variant_c->SetDefaultDescription(
                Segment::Candidate::FULL_HALF_WIDTH |
                Segment::Candidate::CHARACTER_FORM |
                Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER);
          }
        }
        // Don't call ExpandAlternative() for Katakana T13n candidates
      }
    } else {
      ExpandAlternative(candidates_size() - 1);
    }
  }

  // Append katakana candidate at the bottom.
  // In general, Converter cannot know how many candidates the session
  // layer wants to request. Here we simply add a Katakana candidate
  // only when the number of results NbestGenerator() returns
  // is smaller than requested_candidates_size().
  if (candidates_size() > 0 && all_expanded_) {
    const string &hiragana_value = key();
    string katakana_value;
    Util::HiraganaToKatakana(key(), &katakana_value);

    // key() may contain non-katakana sequences. In this situation,
    // don't make pure hiragana candidate.
    if (Util::GetScriptType(hiragana_value) == Util::HIRAGANA) {
      Candidate *last_c = mutable_candidate(candidates_size() - 1);
      Candidate *target_c = add_candidate();
      DCHECK(target_c);
      DCHECK(last_c);
      target_c->Init();
      target_c->value = hiragana_value;
      target_c->content_value = hiragana_value;
      target_c->content_key = key();
      target_c->cost = last_c->cost + 1;
      target_c->structure_cost = last_c->structure_cost + 1;
      target_c->lid = last_c->lid;
      target_c->rid = last_c->rid;
      target_c->SetDefaultDescription(
          Segment::Candidate::FULL_HALF_WIDTH |
          Segment::Candidate::CHARACTER_FORM |
          Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER);
      ExpandAlternative(candidates_size() - 1);
    }

    // key() may contain non-katakana sequences. In this situation,
    // don't make pure katakana candidate.
    if (Util::GetScriptType(katakana_value) == Util::KATAKANA) {
      Candidate *last_c = mutable_candidate(candidates_size() - 1);
      Candidate *target_c = add_candidate();
      DCHECK(target_c);
      DCHECK(last_c);
      target_c->Init();
      target_c->value = katakana_value;
      target_c->content_value = katakana_value;
      target_c->content_key = key();
      target_c->cost = last_c->cost + 1;
      target_c->structure_cost = last_c->structure_cost + 1;
      target_c->lid = last_c->lid;
      target_c->rid = last_c->rid;
      target_c->SetDefaultDescription(
          Segment::Candidate::FULL_HALF_WIDTH |
          Segment::Candidate::CHARACTER_FORM |
          Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER);
      ExpandAlternative(candidates_size() - 1);
    }
  }

  return true;
}

Segments::Segments(): max_history_segments_size_(0),
                      max_prediction_candidates_size_(0),
                      resized_(false),
                      use_user_history_(true),
                      request_type_(Segments::CONVERSION),
                      lattice_(new Lattice),
                      pool_(new ObjectPool<Segment>(32)) {}

Segments::~Segments() {}

void Segments::enable_user_history() {
  use_user_history_ = true;
}

void Segments::disable_user_history() {
  use_user_history_ = false;
}

bool Segments::use_user_history() const {
  return use_user_history_;
}

Segments::RequestType Segments::request_type() const {
  return request_type_;
}

void Segments::set_request_type(Segments::RequestType request_type) {
  request_type_ = request_type;
}

const Segment &Segments::segment(size_t i) const {
  return *segments_[i];
}

Segment *Segments::mutable_segment(size_t i) {
  return segments_[i];
}

const Segment &Segments::history_segment(size_t i) const {
  return *segments_[i];
}

Segment *Segments::mutable_history_segment(size_t i) {
  return segments_[i];
}

const Segment &Segments::conversion_segment(size_t i) const {
  return *segments_[i + history_segments_size()];
}

Segment *Segments::mutable_conversion_segment(size_t i) {
  return segments_[i + history_segments_size()];
}

Segment *Segments::add_segment() {
  return push_back_segment();
}

Segment *Segments::insert_segment(size_t i) {
  Segment *segment = pool_->Alloc();
  segment->Clear();
  segments_.insert(segments_.begin() + i, segment);
  return segment;
}

Segment *Segments::push_back_segment() {
  Segment *segment = pool_->Alloc();
  segment->Clear();
  segments_.push_back(segment);
  return segment;
}

Segment *Segments::push_front_segment() {
  Segment *segment = pool_->Alloc();
  segment->Clear();
  segments_.push_front(segment);
  return segment;
}

size_t Segments::segments_size() const {
  return segments_.size();
}

size_t Segments::history_segments_size() const {
  size_t result = 0;
  for (size_t i = 0; i < segments_size(); ++i) {
    if (segment(i).segment_type() != Segment::HISTORY &&
        segment(i).segment_type() != Segment::SUBMITTED) {
      break;
    }
    ++result;
  }
  return result;
}

size_t Segments::conversion_segments_size() const {
  return (segments_size() - history_segments_size());
}

void Segments::erase_segment(size_t i) {
  if (i >= segments_size()) {
    return;
  }
  pool_->Release(mutable_segment(i));
  segments_.erase(segments_.begin() + i);
}

void Segments::erase_segments(size_t i, size_t size) {
  const size_t end = i + size;
  if (i >= segments_size() || end > segments_size()) {
    return;
  }
  for (size_t j = i ; j < end; ++j) {
    pool_->Release(mutable_segment(j));
  }
  segments_.erase(segments_.begin() + i,
                  segments_.begin() + end);
}

void Segments::pop_front_segment() {
  if (!segments_.empty()) {
    Segment *seg = segments_.front();
    pool_->Release(seg);
    segments_.pop_front();
  }
}

void Segments::pop_back_segment() {
  if (!segments_.empty()) {
    Segment *seg = segments_.back();
    pool_->Release(seg);
    segments_.pop_back();
  }
}

void Segments::RemoveTailOfHistorySegments(size_t num_of_characters) {
  size_t history_size = history_segments_size();
  while (history_size > 0 &&
         num_of_characters > 0) {
    Segment *last_history_segment =
        mutable_history_segment(history_size - 1);

    const size_t length_of_last_history_segment =
        Util::CharsLen(last_history_segment->candidate(0).value);

    if (length_of_last_history_segment <= num_of_characters) {
      num_of_characters -= length_of_last_history_segment;
      --history_size;
      erase_segment(history_size);
      continue;
    }

    // Remove part of last history segment.
    // We should change rId of the candidate too, but currently ignore it.
    // TODO(komatsu): Treat rId properly.
    last_history_segment->mutable_candidate(0)->value =
        Util::SubString(last_history_segment->candidate(0).value,
                        0,
                        length_of_last_history_segment - num_of_characters);
    break;
  }
}

void Segments::clear() {
  clear_segments();
  clear_lattice();
  clear_revert_entries();
}

void Segments::Clear() {
  clear();
}

void Segments::clear_segments() {
  pool_->Free();
  resized_ = false;
  segments_.clear();
}

void Segments::clear_history_segments() {
  while (!segments_.empty()) {
    Segment *seg = segments_.front();
    if (seg->segment_type() != Segment::HISTORY &&
        seg->segment_type() != Segment::SUBMITTED) {
      break;
    }
    pop_front_segment();
  }
}

void Segments::clear_conversion_segments() {
  const size_t size = history_segments_size();
  for (size_t i = size; i < segments_size(); ++i) {
    pool_->Release(mutable_segment(i));
  }
  clear_revert_entries();
  resized_ = false;
  segments_.resize(size);
}

void Segments::clear_lattice() {
  lattice_->Clear();
}

Lattice *Segments::lattice() const {
  return lattice_.get();
}

NodeAllocatorInterface *Segments::node_allocator() const {
  return lattice_->node_allocator();
}

size_t Segments::max_history_segments_size() const {
  return max_history_segments_size_;
}

void Segments::set_max_history_segments_size(size_t max_history_segments_size) {
  max_history_segments_size_ =
      max(static_cast<size_t>(0),
          min(max_history_segments_size, kMaxHistorySize));
}

void Segments::set_resized(bool resized) {
  resized_ = resized;
}

bool Segments::has_resized() const {
  return resized_;
}

size_t Segments::max_prediction_candidates_size() const {
  return max_prediction_candidates_size_;
}

void Segments::set_max_prediction_candidates_size(size_t size) {
  max_prediction_candidates_size_ = size;
}

void Segments::clear_revert_entries() {
  revert_entries_.clear();
}

size_t Segments::revert_entries_size() const {
  return revert_entries_.size();
}

Segments::RevertEntry *Segments::push_back_revert_entry() {
  revert_entries_.resize(revert_entries_.size() + 1);
  Segments::RevertEntry *entry = &revert_entries_.back();
  entry->revert_entry_type = Segments::RevertEntry::CREATE_ENTRY;
  entry->id = 0;
  entry->timestamp = 0;
  entry->key.clear();
  return entry;
}

const Segments::RevertEntry &Segments::revert_entry(size_t i) const {
  return revert_entries_[i];
}

Segments::RevertEntry *Segments::mutable_revert_entry(size_t i) {
  return &revert_entries_[i];
}

void Segments::DebugString(string *output) const {
  if (output == NULL) {
    return;
  }
  output->clear();
  stringstream os;
  os << "(" << endl;
  for (size_t i = 0; i < this->segments_size(); ++i) {
    const mozc::Segment &seg = this->segment(i);
    os << " (key " << seg.segment_type()
       << " " << seg.key() << endl;
    for (int j = 0; j < static_cast<int>(seg.candidates_size()); ++j) {
      os << "   (value " << j << " " << seg.candidate(j).value << " "
         << seg.candidate(j).content_value
         << " cost=" << seg.candidate(j).cost
         << " scost=" << seg.candidate(j).structure_cost
         << " wcost=" << seg.candidate(j).wcost
         << " lid=" << seg.candidate(j).lid
         << " rid=" << seg.candidate(j).rid
         << " prefix=" << seg.candidate(j).prefix
         << " suffix=" << seg.candidate(j).suffix
         << " desc=" << seg.candidate(j).description
         << " nodes_size=" << seg.candidate(j).nodes.size();
      for (int k = 0; k < seg.candidate(j).nodes.size(); ++k) {
        os << "[" << seg.candidate(j).nodes[k]->lid << " "
           << seg.candidate(j).nodes[k]->rid << "]";
      }
      os << ")" << endl;
    }
  }
  os << ")" << endl;
  *output = os.str();
}
}   // mozc
