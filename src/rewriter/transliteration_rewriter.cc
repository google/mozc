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

#include "rewriter/transliteration_rewriter.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/japanese_util.h"
#include "base/number_util.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "composer/composer.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
// For T13n normalize
#include "transliteration/transliteration.h"

namespace mozc {
namespace {

bool IsComposerApplicable(const ConversionRequest &request,
                          const Segments *segments) {
  if (!request.has_composer()) {
    return false;
  }

  std::string conversion_query;
  if (request.request_type() == ConversionRequest::PREDICTION ||
      request.request_type() == ConversionRequest::SUGGESTION) {
    conversion_query = request.composer().GetQueryForPrediction();
  } else {
    conversion_query = request.composer().GetQueryForConversion();
    if (request.request_type() == ConversionRequest::PARTIAL_PREDICTION ||
        request.request_type() == ConversionRequest::PARTIAL_SUGGESTION) {
      Util::Utf8SubString(conversion_query, 0, request.composer().GetCursor(),
                          &conversion_query);
    }
  }

  std::string segments_key;
  for (const Segment &segment : segments->conversion_segments()) {
    segments_key.append(segment.key());
  }
  if (conversion_query != segments_key) {
    DLOG(WARNING) << "composer seems invalid: composer_key " << conversion_query
                  << " segments_key " << segments_key;
    return false;
  }
  return true;
}

void NormalizeT13ns(std::vector<std::string> *t13ns) {
  DCHECK(t13ns);
  for (size_t i = 0; i < t13ns->size(); ++i) {
    t13ns->at(i) = TextNormalizer::NormalizeText(t13ns->at(i));
  }
}

// Function object to check if c >= 0 && c < upper_bound, where T is
// std::true_type when char is unsigned; otherwise T is std::false_type.  By
// using template class instead of a function, we can avoid the compiler warning
// about unused function.
template <typename T>
struct IsNonnegativeAndLessThan {
  bool operator()(char c, size_t upper_bound) const;
};

template <>
struct IsNonnegativeAndLessThan<std::true_type> {
  bool operator()(char c, size_t upper_bound) const {
    // No check for "0 <= *c" for unsigned case.
    return c < upper_bound;
  }
};

template <>
struct IsNonnegativeAndLessThan<std::false_type> {
  bool operator()(char c, size_t upper_bound) const {
    return static_cast<signed char>(c) >= 0 &&
           static_cast<size_t>(c) < upper_bound;
  }
};

void ModifyT13nsForGodan(const absl::string_view key,
                         std::vector<std::string> *t13ns) {
  static const char *const kKeycodeToT13nMap[] = {
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, "",      "ya",    "axtu",  "ixtu",  "uxtu",  "",
      nullptr, nullptr, nullptr, "xi",    nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, "",      "ann",   "extu",  "inn",   nullptr,
      "oxtu",  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, "nn",    nullptr, "yu",    "xe",
      "",      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, "unn",   "yo",    "enn",   "onn",   nullptr,
  };

  const std::string &src = (*t13ns)[transliteration::HALF_ASCII];
  std::string dst;
  for (std::string::const_iterator c = src.begin(); c != src.end(); ++c) {
    using IsNonnegativeAndLessThanType = IsNonnegativeAndLessThan<
        std::is_unsigned<std::string::value_type>::type>;
    if (IsNonnegativeAndLessThanType()(*c, std::size(kKeycodeToT13nMap)) &&
        kKeycodeToT13nMap[*c] != nullptr) {
      dst.append(kKeycodeToT13nMap[*c]);
    } else {
      dst.append(1, *c);
    }
  }

  // Hack: Because of the t13n implementation on upper layer, we cannot
  //   "erase" an element because the number of t13n entries is fixed.
  //   Also, just clearing it (i.e. make it an empty string) doesn't work.
  //   Thus, as a work around, we set the original key, so that it'll be
  //   removed in the later phase of de-dupping.
  const absl::string_view half_ascii = dst.empty() ? key : dst;
  std::string full_ascii =
      japanese_util::HalfWidthAsciiToFullWidthAscii(half_ascii);
  std::string half_ascii_upper(half_ascii);
  std::string half_ascii_lower(half_ascii);
  std::string half_ascii_capitalized(half_ascii);
  Util::UpperString(&half_ascii_upper);
  Util::LowerString(&half_ascii_lower);
  Util::CapitalizeString(&half_ascii_capitalized);
  std::string full_ascii_upper = full_ascii;
  std::string full_ascii_lower = full_ascii;
  std::string full_ascii_capitalized = full_ascii;
  Util::UpperString(&full_ascii_upper);
  Util::LowerString(&full_ascii_lower);
  Util::CapitalizeString(&full_ascii_capitalized);

  (*t13ns)[transliteration::HALF_ASCII] = std::string(half_ascii);
  (*t13ns)[transliteration::HALF_ASCII_UPPER] = half_ascii_upper;
  (*t13ns)[transliteration::HALF_ASCII_LOWER] = half_ascii_lower;
  (*t13ns)[transliteration::HALF_ASCII_CAPITALIZED] = half_ascii_capitalized;
  (*t13ns)[transliteration::FULL_ASCII] = full_ascii;
  (*t13ns)[transliteration::FULL_ASCII_UPPER] = full_ascii_upper;
  (*t13ns)[transliteration::FULL_ASCII_LOWER] = full_ascii_lower;
  (*t13ns)[transliteration::FULL_ASCII_CAPITALIZED] = full_ascii_capitalized;
}

bool IsTransliterated(absl::Span<const std::string> t13ns) {
  if (t13ns.empty() || t13ns[0].empty()) {
    return false;
  }

  absl::string_view base_candidate = t13ns[0];
  for (size_t i = 1; i < t13ns.size(); ++i) {
    if (t13ns[i] != base_candidate) {
      return true;
    }
  }
  return false;
}

struct T13nIds {
  uint16_t hiragana_lid;
  uint16_t hiragana_rid;
  uint16_t katakana_lid;
  uint16_t katakana_rid;
  uint16_t ascii_lid;
  uint16_t ascii_rid;
  T13nIds()
      : hiragana_lid(0),
        hiragana_rid(0),
        katakana_lid(0),
        katakana_rid(0),
        ascii_lid(0),
        ascii_rid(0) {}
};

// Get T13n candidate ids from existing candidates.
void GetIds(const Segment &segment, T13nIds *ids) {
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

void ModifyT13ns(const ConversionRequest &request, const Segment &segment,
                 std::vector<std::string> *t13ns) {
  commands::Request::SpecialRomanjiTable special_romanji_table =
      request.request().special_romanji_table();
  if (special_romanji_table == commands::Request::GODAN_TO_HIRAGANA) {
    ModifyT13nsForGodan(segment.key(), t13ns);
  }

  NormalizeT13ns(t13ns);
}
}  // namespace

bool TransliterationRewriter::FillT13nsFromComposer(
    const ConversionRequest &request, Segments *segments) const {
  // If the size of conversion_segments is one, and the cursor is at
  // the end of the composition, the key for t13n should be equal to
  // the composition string.
  if (segments->conversion_segments_size() == 1 &&
      request.composer().GetLength() == request.composer().GetCursor()) {
    std::vector<std::string> t13ns;
    request.composer().GetTransliterations(&t13ns);
    Segment *segment = segments->mutable_conversion_segment(0);
    CHECK(segment);
    ModifyT13ns(request, *segment, &t13ns);
    const std::string key = request.composer().GetQueryForConversion();
    return SetTransliterations(t13ns, key, segment);
  }

  bool modified = false;
  size_t composition_pos = 0;
  for (Segment &segment : segments->conversion_segments()) {
    const std::string &key = segment.key();
    if (key.empty()) {
      continue;
    }
    const size_t composition_len = Util::CharsLen(key);
    std::vector<std::string> t13ns;
    request.composer().GetSubTransliterations(composition_pos, composition_len,
                                              &t13ns);
    composition_pos += composition_len;

    ModifyT13ns(request, segment, &t13ns);
    modified |= SetTransliterations(t13ns, key, &segment);
  }
  return modified;
}

// This function is used for a fail-safe.
// Ambiguities of roman rule are ignored here.
// ('n' or 'nn' for "ん", etc)
bool TransliterationRewriter::FillT13nsFromKey(Segments *segments) const {
  bool modified = false;
  for (Segment &segment : segments->conversion_segments()) {
    if (segment.key().empty()) {
      continue;
    }
    const std::string &hiragana = segment.key();
    std::string full_katakana = japanese_util::HiraganaToKatakana(hiragana);
    std::string ascii = japanese_util::HiraganaToRomanji(hiragana);
    std::string half_ascii =
        japanese_util::FullWidthAsciiToHalfWidthAscii(ascii);
    std::string full_ascii =
        japanese_util::HalfWidthAsciiToFullWidthAscii(half_ascii);
    std::string half_katakana =
        japanese_util::FullWidthToHalfWidth(full_katakana);
    std::string half_ascii_upper = half_ascii;
    std::string half_ascii_lower = half_ascii;
    std::string half_ascii_capitalized = half_ascii;
    Util::UpperString(&half_ascii_upper);
    Util::LowerString(&half_ascii_lower);
    Util::CapitalizeString(&half_ascii_capitalized);
    std::string full_ascii_upper = full_ascii;
    std::string full_ascii_lower = full_ascii;
    std::string full_ascii_capitalized = full_ascii;
    Util::UpperString(&full_ascii_upper);
    Util::LowerString(&full_ascii_lower);
    Util::CapitalizeString(&full_ascii_capitalized);

    std::vector<std::string> t13ns;
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

    NormalizeT13ns(&t13ns);
    modified |= SetTransliterations(t13ns, segment.key(), &segment);
  }
  return modified;
}

TransliterationRewriter::TransliterationRewriter(
    const dictionary::PosMatcher &pos_matcher)
    : unknown_id_(pos_matcher.GetUnknownId()) {}

int TransliterationRewriter::capability(
    const ConversionRequest &request) const {
  // For mobile, mixed conversion is on.  In this case t13n rewrite should be
  // triggered anytime.
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }

  // Otherwise t13n rewrite is triggered only on conversion.
  return RewriterInterface::CONVERSION;
}

bool TransliterationRewriter::AddRawNumberT13nCandidates(
    const ConversionRequest &request, Segments *segments) const {
  if (segments->conversion_segments_size() != 1) {
    // This method rewrites a segment only when the segments has only
    // one conversion segment.
    // This is spec matter.
    // Rewriting multiple segments will not make users happier.
    return false;
  }
  // This process is done on composer's data.
  // If the request doesn't have a composer, this method can do nothing.
  if (!request.has_composer()) {
    return false;
  }
  const composer::ComposerData &composer = request.composer();
  Segment *segment = segments->mutable_conversion_segment(0);
  // Get the half_ascii T13n text (nearly equal Raw input).
  // Note that only one segment is in the Segments, but sometimes like
  // on partial conversion, segment.key() is different from the size of
  // the whole composition.
  const std::string raw =
      composer.GetRawSubString(0, Util::CharsLen(segment->key()));
  if (raw.empty()) {
    return false;
  }
  if (!NumberUtil::IsArabicNumber(raw)) {
    return false;
  }
  // half_ascii is arabic number. So let's append additional candidates.
  T13nIds ids;
  GetIds(*segment, &ids);

  // Append half_ascii as normal candidate.
  // If half_ascii is equal to meta candidate's HALF_ASCII candidate, skip.
  if (segment->meta_candidates_size() < transliteration::HALF_ASCII ||
      segment->meta_candidate(transliteration::HALF_ASCII).value != raw) {
    Segment::Candidate *half_candidate = segment->add_candidate();
    InitT13nCandidate(raw, raw, ids.ascii_lid, ids.ascii_rid, half_candidate);
    // Keep the character form.
    // Without this attribute the form will be changed by VariantsRewriter.
    half_candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  }

  // Do the same thing on full form.
  std::string full_raw = japanese_util::HalfWidthAsciiToFullWidthAscii(raw);
  DCHECK(!full_raw.empty());
  if (segment->meta_candidates_size() < transliteration::FULL_ASCII ||
      segment->meta_candidate(transliteration::FULL_ASCII).value != full_raw) {
    Segment::Candidate *full_candidate = segment->add_candidate();
    InitT13nCandidate(raw, full_raw, ids.ascii_lid, ids.ascii_rid,
                      full_candidate);
    full_candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  }
  return true;
}

bool TransliterationRewriter::Rewrite(const ConversionRequest &request,
                                      Segments *segments) const {
  if (request.skip_slow_rewriters()) {
    return false;
  }

  bool modified = false;
  if (IsComposerApplicable(request, segments)) {
    modified |= FillT13nsFromComposer(request, segments);
  } else {
    modified |= FillT13nsFromKey(segments);
  }
  modified |= AddRawNumberT13nCandidates(request, segments);

  return modified;
}

void TransliterationRewriter::InitT13nCandidate(
    const absl::string_view key, const absl::string_view value,
    const uint16_t lid, const uint16_t rid, Segment::Candidate *cand) const {
  DCHECK(cand);
  cand->value = std::string(value);
  cand->key = std::string(key);
  cand->content_value = std::string(value);
  cand->content_key = std::string(key);
  cand->lid = (lid != 0) ? lid : unknown_id_;
  cand->rid = (rid != 0) ? rid : unknown_id_;
}

bool TransliterationRewriter::SetTransliterations(
    absl::Span<const std::string> t13ns, const absl::string_view key,
    Segment *segment) const {
  if (t13ns.size() != transliteration::NUM_T13N_TYPES ||
      !IsTransliterated(t13ns)) {
    return false;
  }

  segment->clear_meta_candidates();

  T13nIds ids;
  GetIds(*segment, &ids);

  std::vector<Segment::Candidate> *meta_candidates =
      segment->mutable_meta_candidates();
  meta_candidates->resize(transliteration::NUM_T13N_TYPES);

  InitT13nCandidate(key, t13ns[transliteration::HIRAGANA], ids.hiragana_lid,
                    ids.hiragana_rid,
                    &meta_candidates->at(transliteration::HIRAGANA));

  InitT13nCandidate(key, t13ns[transliteration::FULL_KATAKANA],
                    ids.katakana_lid, ids.katakana_rid,
                    &meta_candidates->at(transliteration::FULL_KATAKANA));

  InitT13nCandidate(key, t13ns[transliteration::HALF_ASCII], ids.ascii_lid,
                    ids.ascii_rid,
                    &meta_candidates->at(transliteration::HALF_ASCII));

  InitT13nCandidate(key, t13ns[transliteration::HALF_ASCII_UPPER],
                    ids.ascii_lid, ids.ascii_rid,
                    &meta_candidates->at(transliteration::HALF_ASCII_UPPER));

  InitT13nCandidate(key, t13ns[transliteration::HALF_ASCII_LOWER],
                    ids.ascii_lid, ids.ascii_rid,
                    &meta_candidates->at(transliteration::HALF_ASCII_LOWER));

  InitT13nCandidate(
      key, t13ns[transliteration::HALF_ASCII_CAPITALIZED], ids.ascii_lid,
      ids.ascii_rid,
      &meta_candidates->at(transliteration::HALF_ASCII_CAPITALIZED));

  InitT13nCandidate(key, t13ns[transliteration::FULL_ASCII], ids.ascii_lid,
                    ids.ascii_rid,
                    &meta_candidates->at(transliteration::FULL_ASCII));

  InitT13nCandidate(key, t13ns[transliteration::FULL_ASCII_UPPER],
                    ids.ascii_lid, ids.ascii_rid,
                    &meta_candidates->at(transliteration::FULL_ASCII_UPPER));

  InitT13nCandidate(key, t13ns[transliteration::FULL_ASCII_LOWER],
                    ids.ascii_lid, ids.ascii_rid,
                    &meta_candidates->at(transliteration::FULL_ASCII_LOWER));

  InitT13nCandidate(
      key, t13ns[transliteration::FULL_ASCII_CAPITALIZED], ids.ascii_lid,
      ids.ascii_rid,
      &meta_candidates->at(transliteration::FULL_ASCII_CAPITALIZED));

  InitT13nCandidate(key, t13ns[transliteration::HALF_KATAKANA],
                    ids.katakana_lid, ids.katakana_rid,
                    &meta_candidates->at(transliteration::HALF_KATAKANA));
  return true;
}

}  // namespace mozc
