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

#include "rewriter/transliteration_rewriter.h"

#include <string>
#include <vector>

#include "base/text_normalizer.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "session/commands.pb.h"
#include "session/request_handler.h"
// For T13N normalize
#include "transliteration/transliteration.h"

namespace mozc {
namespace {
struct T13NIds {
  uint16 hiragana_lid;
  uint16 hiragana_rid;
  uint16 katakana_lid;
  uint16 katakana_rid;
  uint16 ascii_lid;
  uint16 ascii_rid;
  T13NIds() : hiragana_lid(0), hiragana_rid(0),
              katakana_lid(0), katakana_rid(0),
              ascii_lid(0), ascii_rid(0) {}
};

bool IsValidRequest(const ConversionRequest &request, const Segments *segments) {
  if (!request.has_composer()) {
    return false;
  }

  string conversion_query;
  request.composer().GetQueryForConversion(&conversion_query);
  if (segments->request_type() == Segments::PARTIAL_PREDICTION ||
      segments->request_type() == Segments::PARTIAL_SUGGESTION) {
    conversion_query =
        Util::SubString(conversion_query, 0, request.composer().GetCursor());
  }

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

void NormalizeT13Ns(vector<string> *t13ns) {
  DCHECK(t13ns);
  string normalized;
  for (size_t i = 0; i < t13ns->size(); ++i) {
    normalized.clear();
    TextNormalizer::NormalizeTransliterationText(
        t13ns->at(i), &normalized);
    t13ns->at(i) = normalized;
  }
}

void EraseNon12KeyT13Ns(const string &key, vector<string> *t13ns) {
  using transliteration::TransliterationType;
  static const TransliterationType kAsciiTransliterationTypeList[] = {
    transliteration::HALF_ASCII,
    transliteration::HALF_ASCII_UPPER,
    transliteration::HALF_ASCII_LOWER,
    transliteration::HALF_ASCII_CAPITALIZED,
    transliteration::FULL_ASCII,
    transliteration::FULL_ASCII_UPPER,
    transliteration::FULL_ASCII_LOWER,
    transliteration::FULL_ASCII_CAPITALIZED,
  };

  for (size_t i = 0; i < arraysize(kAsciiTransliterationTypeList); ++i) {
    TransliterationType type = kAsciiTransliterationTypeList[i];
    if (!Util::IsArabicNumber((*t13ns)[type])) {
      // The translitarated string contains a non-number character.
      // So erase it.
      // Hack: because of the t13n implementation on upper layer, we cannot
      //   "erase" the element because the number of t13n entries is fixed.
      //   Also, just clearing it (i.e. make it an empty string) cannot work.
      //   Thus, as a work around, we set original key, so that it'll be
      //   reduced in the later phase by de-dupping.
      (*t13ns)[type] = key;
    }
  }
}

bool IsTransliterated(const vector<string> &t13ns) {
  if (t13ns.empty() || t13ns[0].empty()) {
    return false;
  }

  const string &base_candidate = t13ns[0];
  for (size_t i = 1; i < t13ns.size(); ++i) {
    if (t13ns[i] != base_candidate) {
      return true;
    }
  }
  return false;
}

void InitT13NCandidate(const string &key,
                       const string &value,
                       uint16 lid,
                       uint16 rid,
                       Segment::Candidate *cand) {
  DCHECK(cand);
  cand->Init();
  cand->value = value;
  cand->key = key;
  cand->content_value = value;
  cand->content_key = key;
  cand->lid = (lid != 0) ? lid : POSMatcher::GetUnknownId();
  cand->rid = (rid != 0) ? rid : POSMatcher::GetUnknownId();
}

void SetTransliterations(
    const vector<string> &t13ns, const T13NIds &ids, Segment *segment) {
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
                    ids.hiragana_lid, ids.hiragana_rid,
                    &meta_candidates->at(transliteration::HIRAGANA));

  InitT13NCandidate(key, t13ns[transliteration::FULL_KATAKANA],
                    ids.katakana_lid, ids.katakana_rid,
                    &meta_candidates->at(transliteration::FULL_KATAKANA));

  InitT13NCandidate(key, t13ns[transliteration::HALF_ASCII],
                    ids.ascii_lid, ids.ascii_rid,
                    &meta_candidates->at(transliteration::HALF_ASCII));

  InitT13NCandidate(key, t13ns[transliteration::HALF_ASCII_UPPER],
                    ids.ascii_lid, ids.ascii_rid,
                    &meta_candidates->at(transliteration::HALF_ASCII_UPPER));

  InitT13NCandidate(key, t13ns[transliteration::HALF_ASCII_LOWER],
                    ids.ascii_lid, ids.ascii_rid,
                    &meta_candidates->at(transliteration::HALF_ASCII_LOWER));

  InitT13NCandidate(
      key, t13ns[transliteration::HALF_ASCII_CAPITALIZED],
      ids.ascii_lid, ids.ascii_rid,
      &meta_candidates->at(transliteration::HALF_ASCII_CAPITALIZED));

  InitT13NCandidate(key, t13ns[transliteration::FULL_ASCII],
                    ids.ascii_lid, ids.ascii_rid,
                    &meta_candidates->at(transliteration::FULL_ASCII));

  InitT13NCandidate(key, t13ns[transliteration::FULL_ASCII_UPPER],
                    ids.ascii_lid, ids.ascii_rid,
                    &meta_candidates->at(transliteration::FULL_ASCII_UPPER));

  InitT13NCandidate(key, t13ns[transliteration::FULL_ASCII_LOWER],
                    ids.ascii_lid, ids.ascii_rid,
                    &meta_candidates->at(transliteration::FULL_ASCII_LOWER));

  InitT13NCandidate(
      key, t13ns[transliteration::FULL_ASCII_CAPITALIZED],
      ids.ascii_lid, ids.ascii_rid,
      &meta_candidates->at(transliteration::FULL_ASCII_CAPITALIZED));

  InitT13NCandidate(key, t13ns[transliteration::HALF_KATAKANA],
                    ids.katakana_lid, ids.katakana_rid,
                    &meta_candidates->at(transliteration::HALF_KATAKANA));
}

// Get T13N candidate ids from existing candidates.
void GetIds(const Segment &segment, T13NIds *ids) {
  DCHECK(ids);
  // reverse loop to use the highest rank results for each type
  for (int i = segment.candidates_size() - 1; i >= 0; --i) {
    const Segment::Candidate &candidate = segment.candidate(i);
    Util::ScriptType type = Util::GetScriptType(candidate.value);
    if (type == Util::HIRAGANA) {
      ids->hiragana_lid = candidate.lid;
      ids->hiragana_rid = candidate.rid;
    } else if (type == Util::KATAKANA) {
      ids->katakana_lid = candidate.lid;
      ids->katakana_rid = candidate.rid;
    } else if (type == Util::ALPHABET) {
      ids->ascii_lid = candidate.lid;
      ids->ascii_rid = candidate.rid;
    }
  }
}

bool FillT13NsFromComposer(const ConversionRequest &request,
                           Segments *segments) {
  bool modified = false;
  size_t composition_pos = 0;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *segment = segments->mutable_conversion_segment(i);
    CHECK(segment);
    if (segment->key().empty()) {
      continue;
    }
    const size_t composition_len = Util::CharsLen(segment->key());
    vector<string> t13ns;
    request.composer().GetSubTransliterations(composition_pos,
                                              composition_len,
                                              &t13ns);
    composition_pos += composition_len;
    T13NIds ids;
    GetIds(*segment, &ids);

    // Especially for mobile-flick input, the input key is sometimes
    // non-transliteration. For example, the i-flick is '_', which is not
    // the transliteration at all. So, for those input mode, we just accept
    // only 12-keys number layouts.
    commands::Request::SpecialRomanjiTable special_romanji_table =
        GET_REQUEST(special_romanji_table);
    if (special_romanji_table == commands::Request::TWELVE_KEYS_TO_HIRAGANA ||
        special_romanji_table == commands::Request::FLICK_TO_HIRAGANA ||
        special_romanji_table == commands::Request::TOGGLE_FLICK_TO_HIRAGANA) {
      EraseNon12KeyT13Ns(segment->key(), &t13ns);
    }

    NormalizeT13Ns(&t13ns);
    if (!IsTransliterated(t13ns)) {
      continue;
    }
    SetTransliterations(t13ns, ids, segment);
    modified = true;
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
    if (segment->key().empty()) {
      continue;
    }
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

    T13NIds ids;
    GetIds(*segment, &ids);

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

    NormalizeT13Ns(&t13ns);
    if (!IsTransliterated(t13ns)) {
      continue;
    }
    SetTransliterations(t13ns, ids, segment);
    modified = true;
  }
  return modified;
}
}   // namespace

TransliterationRewriter::TransliterationRewriter() {}
TransliterationRewriter::~TransliterationRewriter() {}

int TransliterationRewriter::capability() const {
  if (GET_REQUEST(mixed_conversion)) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool TransliterationRewriter::Rewrite(Segments *segments) const {
  return FillT13NsFromKey(segments);
}

bool TransliterationRewriter::RewriteForRequest(
    const ConversionRequest &request, Segments *segments) const {
  if (!IsValidRequest(request, segments)) {
    return FillT13NsFromKey(segments);
  }
  return FillT13NsFromComposer(request, segments);
}
}  // namespace mozc
