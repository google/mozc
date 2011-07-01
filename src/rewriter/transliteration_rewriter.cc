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

#include "rewriter/transliteration_rewriter.h"

#include <string>
#include <vector>

#include "base/util.h"
#include "composer/composer.h"
#include "converter/segments.h"
#include "session/commands.pb.h"
// For T13N normalize
#include "session/internal/session_normalizer.h"
#include "transliteration/transliteration.h"

namespace mozc {
namespace {
bool IsValidComposer(const Segments *segments) {
  if (segments->composer() == NULL) {
    return false;
  }
  string conversion_query;
  segments->composer()->GetQueryForConversion(&conversion_query);
  string segments_key;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    segments_key.append(segments->conversion_segment(i).key());
  }
  if (conversion_query != segments_key) {
    DLOG(WARNING) << "composer seems invalid: composer_key "
                  << conversion_query << " segments_key "
                  << segments_key;
    return false;
  }
  return true;
}

void NormalizeT13Ns(const vector<string> &t13ns,
                    vector<string> *t13ns_normalized) {
  DCHECK(t13ns_normalized);
  t13ns_normalized->clear();
  string normalized;
  for (size_t j = 0; j < t13ns.size(); ++j) {
    normalized.clear();
    session::SessionNormalizer::NormalizeTransliterationText(
        t13ns[j], &normalized);
    t13ns_normalized->push_back(normalized);
  }
}

void InitT13NCandidate(const string &key,
                       const string &value,
                       Segment::Candidate *cand) {
  DCHECK(cand);
  cand->Init();
  cand->value = value;
  cand->content_value = value;
  cand->content_key = key;
}

void SetTransliterations(const vector<string> &t13ns, Segment *segment) {
  if (t13ns.size() != transliteration::NUM_T13N_TYPES) {
    LOG(WARNING) << "t13ns size is invalid";
    return;
  }
  segment->clear_meta_candidates();
  const string &key = segment->key();

  vector<Segment::Candidate> *meta_candidates =
      segment->mutable_meta_candidates();
  meta_candidates->resize(transliteration::NUM_T13N_TYPES);

  InitT13NCandidate(key, t13ns[transliteration::HIRAGANA],
                    &meta_candidates->at(transliteration::HIRAGANA));

  InitT13NCandidate(key, t13ns[transliteration::FULL_KATAKANA],
                    &meta_candidates->at(transliteration::FULL_KATAKANA));

  InitT13NCandidate(key, t13ns[transliteration::HALF_ASCII],
                    &meta_candidates->at(transliteration::HALF_ASCII));

  InitT13NCandidate(key, t13ns[transliteration::HALF_ASCII_UPPER],
                    &meta_candidates->at(transliteration::HALF_ASCII_UPPER));

  InitT13NCandidate(key, t13ns[transliteration::HALF_ASCII_LOWER],
                    &meta_candidates->at(transliteration::HALF_ASCII_LOWER));

  InitT13NCandidate(
      key, t13ns[transliteration::HALF_ASCII_CAPITALIZED],
      &meta_candidates->at(transliteration::HALF_ASCII_CAPITALIZED));

  InitT13NCandidate(key, t13ns[transliteration::FULL_ASCII],
                    &meta_candidates->at(transliteration::FULL_ASCII));

  InitT13NCandidate(key, t13ns[transliteration::FULL_ASCII_UPPER],
                    &meta_candidates->at(transliteration::FULL_ASCII_UPPER));

  InitT13NCandidate(key, t13ns[transliteration::FULL_ASCII_LOWER],
                    &meta_candidates->at(transliteration::FULL_ASCII_LOWER));

  InitT13NCandidate(
      key, t13ns[transliteration::FULL_ASCII_CAPITALIZED],
      &meta_candidates->at(transliteration::FULL_ASCII_CAPITALIZED));

  InitT13NCandidate(key, t13ns[transliteration::HALF_KATAKANA],
                    &meta_candidates->at(transliteration::HALF_KATAKANA));
}

bool FillT13NsFromComposer(Segments *segments) {
  bool modified = false;
  size_t composition_pos = 0;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    CHECK(segment);
    modified = true;
    const size_t composition_len = Util::CharsLen(segment->key());
    vector<string> t13ns;
    segments->composer()->GetSubTransliterations(composition_pos,
                                                 composition_len,
                                                 &t13ns);
    vector<string> t13ns_normalized;
    NormalizeT13Ns(t13ns, &t13ns_normalized);
    SetTransliterations(t13ns_normalized, segment);
    composition_pos += composition_len;
  }
  return modified;
}

// This function is used for a fail-safe.
// Ambiguities of roman rule are ignored here.
// ('n' or 'nn' for "ん", etc)
bool FillT13NsFromKey(Segments *segments) {
  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    CHECK(segment);
    modified = true;

    const string &hiragana = segment->key();
    string full_katakana, ascii;
    Util::HiraganaToKatakana(hiragana, &full_katakana);
    Util::HiraganaToRomanji(hiragana, &ascii);
    string half_ascii, full_ascii, half_katakana;
    Util::FullWidthAsciiToHalfWidthAscii(ascii, &half_ascii);
    Util::HalfWidthAsciiToFullWidthAscii(half_ascii, &full_ascii);
    Util::FullWidthToHalfWidth(full_katakana, &half_katakana);
    string half_ascii_upper = half_ascii;
    string half_ascii_lower = half_ascii;
    string half_ascii_capitalized = half_ascii;
    Util::UpperString(&half_ascii_upper);
    Util::LowerString(&half_ascii_lower);
    Util::CapitalizeString(&half_ascii_capitalized);
    string full_ascii_upper = full_ascii;
    string full_ascii_lower = full_ascii;
    string full_ascii_capitalized = full_ascii;
    Util::UpperString(&full_ascii_upper);
    Util::LowerString(&full_ascii_lower);
    Util::CapitalizeString(&full_ascii_capitalized);

    vector<string> t13ns;
    t13ns.resize(transliteration::NUM_T13N_TYPES);
    t13ns[transliteration::HIRAGANA] = hiragana;
    t13ns[transliteration::FULL_KATAKANA] = full_katakana;
    t13ns[transliteration::HALF_KATAKANA] = half_katakana;
    t13ns[transliteration::HALF_ASCII] = half_ascii;
    t13ns[transliteration::HALF_ASCII_UPPER] = half_ascii_upper;
    t13ns[transliteration::HALF_ASCII_LOWER] = half_ascii_lower;
    t13ns[transliteration::HALF_ASCII_CAPITALIZED] = half_ascii_capitalized;
    t13ns[transliteration::FULL_ASCII] = full_ascii;
    t13ns[transliteration::FULL_ASCII_UPPER] = full_ascii_upper;
    t13ns[transliteration::FULL_ASCII_LOWER] = full_ascii_lower;
    t13ns[transliteration::FULL_ASCII_CAPITALIZED] = full_ascii_capitalized;

    vector<string> t13ns_normalized;
    NormalizeT13Ns(t13ns, &t13ns_normalized);
    SetTransliterations(t13ns_normalized, segment);
  }
  return modified;
}
}   // namespace

TransliterationRewriter::TransliterationRewriter() {}
TransliterationRewriter::~TransliterationRewriter() {}

int TransliterationRewriter::capability() const {
  return RewriterInterface::CONVERSION;
}

bool TransliterationRewriter::Rewrite(Segments *segments) const {
  if (!IsValidComposer(segments)) {
    return FillT13NsFromKey(segments);
  }
  return FillT13NsFromComposer(segments);
}
}  // namespace mozc
