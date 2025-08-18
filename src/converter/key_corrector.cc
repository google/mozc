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

#include "converter/key_corrector.h"

#include <cstdint>
#include <cstring>
#include <iterator>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "base/util.h"
#include "base/vlog.h"

namespace mozc {
namespace {

// "ん" (few "n" pattern)
// "んあ" -> "んな"
// "んい" -> "んに"
// "んう" -> "んぬ"
// "んえ" -> "んね"
// "んお" -> "んの"
bool RewriteNN(size_t key_pos, absl::string_view prefix, size_t* mblen,
               std::string* output) {
  if (key_pos == 0) {
    *mblen = 0;
    return false;
  }

  const char32_t codepoint = Util::Utf8ToCodepoint(prefix, mblen);
  if (codepoint != 0x3093) {  // "ん"
    *mblen = 0;
    return false;
  }

  prefix.remove_prefix(*mblen);
  if (prefix.empty()) {
    *mblen = 0;
    return false;
  }

  size_t mblen2 = 0;
  const uint16_t next_codepoint = Util::Utf8ToCodepoint(prefix, &mblen2);
  uint16_t output_codepoint = 0x0000;
  switch (next_codepoint) {
    case 0x3042:                  // "あ"
      output_codepoint = 0x306A;  // "な"
      break;
    case 0x3044:                  // "い"
      output_codepoint = 0x306B;  // "に"
      break;
    case 0x3046:                  // "う"
      output_codepoint = 0x306C;  // "ぬ"
      break;
    case 0x3048:                  // "え"
      output_codepoint = 0x306D;  // "ね"
      break;
    case 0x304A:                  // "お"
      output_codepoint = 0x306E;  // "の"
      break;
    default:
      break;
  }

  if (output_codepoint != 0x0000) {  // "ん[あいうえお]"
    Util::CodepointToUtf8Append(codepoint, output);
    Util::CodepointToUtf8Append(output_codepoint, output);
    *mblen += mblen2;
    return true;
  } else {  // others
    *mblen = 0;
    return false;
  }

  return false;
}

// "んん" (many "n" pattern)
// "([^ん])んん[ん]" -> ignore
// "([^ん])んん[あいうえお]" ->  $1 and leave "ん[あいうえお]"
// "([^ん])んん[^あいうえお]" -> $1"ん" and leave "[^あいうえお]"
bool RewriteDoubleNN(size_t key_pos, absl::string_view prefix, size_t* mblen,
                     std::string* output) {
  // 0x3093: "ん"
  static const uint16_t kPattern[] = {0x0000, 0x3093, 0x3093};

  *mblen = 0;
  uint16_t first_char = 0x0000;
  size_t first_mblen = 0;
  for (size_t i = 0; i < std::size(kPattern); ++i) {
    if (prefix.empty()) {
      *mblen = 0;
      return false;
    }
    size_t mblen2 = 0;
    const char32_t codepoint = Util::Utf8ToCodepoint(prefix, &mblen2);
    if ((kPattern[i] == 0x0000 && codepoint != 0x3093 &&
         Util::GetScriptType(codepoint) == Util::HIRAGANA) ||
        (kPattern[i] == 0x3093 && codepoint == 0x3093)) {
      if (i == 0) {
        first_mblen = mblen2;
        first_char = codepoint;
      }
    } else {
      *mblen = 0;
      return false;
    }
    prefix.remove_prefix(mblen2);
    *mblen += mblen2;
  }

  if (prefix.empty()) {
    *mblen = 0;
    return false;
  }

  if (first_char == 0x0000 || first_mblen == 0) {
    *mblen = 0;
    return false;
  }

  size_t mblen2 = 0;
  const char32_t codepoint = Util::Utf8ToCodepoint(prefix, &mblen2);
  if (codepoint == 0x3093) {  // "ん" ignore
    *mblen = 0;
    return false;
  } else if (codepoint == 0x3042 ||  // "[あいうえお]"
             codepoint == 0x3044 || codepoint == 0x3046 ||
             codepoint == 0x3048 || codepoint == 0x304A) {
    // drop first "ん" and leave "ん[あいうえお]"
    // remained part will be handled by RewriteNN(), e.g., "んあ" -> "んな"
    Util::CodepointToUtf8Append(first_char, output);
    // Skip one Hiragana character in UTF-8, which is 3 bytes.
    *mblen = first_mblen + 3;
    return true;
  } else {  // "[^あいうえお]"
    Util::CodepointToUtf8Append(first_char, output);
    Util::CodepointToUtf8Append(0x3093, output);  // "ん"
    return true;
  }

  return false;
}

// "に" pattern
// "にゃ" -> "んや"
// "にゅ" -> "んゆ"
// "にょ" -> "んよ"
bool RewriteNI(size_t key_pos, absl::string_view prefix, size_t* mblen,
               std::string* output) {
  const char32_t codepoint = Util::Utf8ToCodepoint(prefix, mblen);
  if (codepoint != 0x306B) {  // "に"
    *mblen = 0;
    return false;
  }

  prefix.remove_prefix(*mblen);
  if (prefix.empty()) {
    *mblen = 0;
    return false;
  }

  size_t mblen2 = 0;
  const uint16_t next_codepoint = Util::Utf8ToCodepoint(prefix, &mblen2);
  uint16_t output_codepoint = 0x0000;
  switch (next_codepoint) {
    case 0x3083:                  // "ゃ"
      output_codepoint = 0x3084;  // "や"
      break;
    case 0x3085:                  // "ゅ"
      output_codepoint = 0x3086;  // "ゆ"
      break;
    case 0x3087:                  // "ょ"
      output_codepoint = 0x3088;  // "よ"
      break;
    default:
      output_codepoint = 0x0000;
      break;
  }

  if (output_codepoint != 0x0000) {
    Util::CodepointToUtf8Append(0x3093, output);  // "ん"
    Util::CodepointToUtf8Append(output_codepoint, output);
    *mblen += mblen2;
    return true;
  } else {
    *mblen = 0;
    return false;
  }

  return false;
}

// "m" Pattern (not BOS)
// "m[ばびぶべぼぱぴぷぺぽ]" -> "ん[ばびぶべぼぱぴぷぺぽ]"
bool RewriteM(size_t key_pos, absl::string_view prefix, size_t* mblen,
              std::string* output) {
  if (key_pos == 0) {
    *mblen = 0;
    return false;
  }
  const char32_t codepoint = Util::Utf8ToCodepoint(prefix, mblen);
  // "m" or "ｍ" (don't take capitial letter, as "M" might not be a misspelling)
  if (codepoint != 0x006D && codepoint != 0xFF4D) {
    *mblen = 0;
    return false;
  }

  prefix.remove_prefix(*mblen);
  if (prefix.empty()) {
    *mblen = 0;
    return false;
  }

  size_t mblen2 = 0;
  const uint16_t next_codepoint = Util::Utf8ToCodepoint(prefix, &mblen2);
  // "[はばぱひびぴふぶぷへべぺほぼぽ]" => [0x306F .. 0X307D]
  // Here we want to take "[は..ぽ]" except for "はひふへほ"
  if (next_codepoint % 3 != 0 &&  // not "はひふへほ"
      next_codepoint >= 0x306F && next_codepoint <= 0x307D) {  // "は..ぽ"
    Util::CodepointToUtf8Append(0x3093, output);               // "ん"
    Util::CodepointToUtf8Append(next_codepoint, output);
    *mblen += mblen2;
    return true;
  } else {
    *mblen = 0;
    return false;
  }

  return true;
}

// "きっって" ->" きって"
// replace "([^っ])っっ([^っ])" => "$1っ$2"
// Don't consider more that three "っっっ"
// e.g., "かっっった" -> "かっっった"
bool RewriteSmallTSU(size_t key_pos, absl::string_view prefix, size_t* mblen,
                     std::string* output) {
  // 0x0000 is a place holder for "[^っ]"
  // "っ": 0x3063
  static const uint16_t kPattern[] = {0x0000, 0x3063, 0x3063, 0x0000};

  uint16_t first_char = 0x0000;
  uint16_t last_char = 0x0000;
  for (size_t i = 0; i < std::size(kPattern); ++i) {
    if (prefix.empty()) {
      *mblen = 0;
      return false;
    }
    size_t mblen2 = 0;
    const char32_t codepoint = Util::Utf8ToCodepoint(prefix, &mblen2);
    if ((kPattern[i] == 0x0000 && codepoint != 0x3063 &&
         Util::GetScriptType(codepoint) == Util::HIRAGANA) ||
        (kPattern[i] == 0x3063 && codepoint == 0x3063)) {
      if (i == 0) {
        first_char = codepoint;
      } else if (i == std::size(kPattern) - 1) {
        last_char = codepoint;
      }
    } else {
      *mblen = 0;
      return false;
    }
    prefix.remove_prefix(mblen2);
    *mblen += mblen2;
  }

  if (first_char == 0x0000 || last_char == 0x0000) {
    *mblen = 0;
    return false;
  }

  Util::CodepointToUtf8Append(first_char, output);
  Util::CodepointToUtf8Append(0x3063, output);  // "っ"
  Util::CodepointToUtf8Append(last_char, output);

  return true;
}

// Not implemented yet, as they looks minor
// "[子音][ゃゅょ][^う]" Pattern
// "きゅ[^う] -> きゅう"
// "しゅ[^う] -> しゅう"
// "ちゅ[^う] -> ちゅう"
// "にゅ[^う] -> にゅう"
// "ひゅ[^う] -> ひゅう"
// "りゅ[^う] -> りゅう"
bool RewriteYu(size_t key_pos, absl::string_view prefix, size_t* mblen,
               std::string* output) {
  const char32_t first_char = Util::Utf8ToCodepoint(prefix, mblen);
  if (first_char != 0x304D && first_char != 0x3057 && first_char != 0x3061 &&
      first_char != 0x306B && first_char != 0x3072 &&
      first_char != 0x308A) {  // !"きしちにひり"
    *mblen = 0;
    return false;
  }

  prefix.remove_prefix(*mblen);
  if (prefix.empty()) {
    *mblen = 0;
    return false;
  }

  size_t mblen2 = 0;
  const char32_t next_char = Util::Utf8ToCodepoint(prefix, &mblen2);
  if (next_char != 0x3085) {  // "ゅ"
    *mblen = 0;
    return false;
  }

  prefix.remove_prefix(mblen2);
  if (prefix.empty()) {
    *mblen = 0;
    return false;
  }

  size_t mblen3 = 0;
  const char32_t last_char = Util::Utf8ToCodepoint(prefix, &mblen3);
  if (last_char == 0x3046) {  // "う"
    *mblen = 0;
    return false;
  }

  // OK, rewrite
  *mblen += mblen2;
  Util::CodepointToUtf8Append(first_char, output);
  Util::CodepointToUtf8Append(next_char, output);  // "ゅ"
  Util::CodepointToUtf8Append(0x3046, output);     // "う"

  return true;
}
}  // namespace

size_t KeyCorrector::GetCorrectedPosition(const size_t original_key_pos) const {
  if (original_key_pos < alignment_.size()) {
    return alignment_[original_key_pos];
  }
  return kInvalidPos;
}

size_t KeyCorrector::GetOriginalPosition(const size_t corrected_key_pos) const {
  if (corrected_key_pos < rev_alignment_.size()) {
    return rev_alignment_[corrected_key_pos];
  }
  return kInvalidPos;
}

bool KeyCorrector::Init(absl::string_view key, InputMode mode,
                        size_t history_size) {
  // TODO(taku)  support KANA
  if (mode == KANA) {
    return false;
  }

  if (key.empty() || key.size() >= kMaxSize) {
    MOZC_VLOG(1) << "invalid key length";
    return false;
  }

  original_key_ = key;

  absl::string_view prefix = key;
  absl::string_view input_begin =
      prefix.substr(std::min(prefix.size(), history_size));
  size_t key_pos = 0;

  while (!prefix.empty()) {
    size_t mblen = 0;
    const size_t org_len = corrected_key_.size();
    if (prefix.data() < input_begin.data() ||
        (!RewriteDoubleNN(key_pos, prefix, &mblen, &corrected_key_) &&
         !RewriteNN(key_pos, prefix, &mblen, &corrected_key_) &&
         !RewriteYu(key_pos, prefix, &mblen, &corrected_key_) &&
         !RewriteNI(key_pos, prefix, &mblen, &corrected_key_) &&
         !RewriteSmallTSU(key_pos, prefix, &mblen, &corrected_key_) &&
         !RewriteM(key_pos, prefix, &mblen, &corrected_key_))) {
      const char32_t codepoint = Util::Utf8ToCodepoint(prefix, &mblen);
      Util::CodepointToUtf8Append(codepoint, &corrected_key_);
    }

    const size_t corrected_mblen = corrected_key_.size() - org_len;

    if (corrected_mblen <= 0 && mblen <= 0) {
      LOG(ERROR) << "Invalid pattern: " << key;
      return false;
    }

    // one to one mapping
    if (mblen == corrected_mblen) {
      const size_t len = static_cast<size_t>(prefix.data() - key.data());
      for (size_t i = 0; i < mblen; ++i) {
        alignment_.push_back(org_len + i);
        rev_alignment_.push_back(len + i);
      }
    } else {
      // NOT a one to one mapping, we take fist/last alignment only
      alignment_.push_back(org_len);
      for (size_t i = 1; i < mblen; ++i) {
        alignment_.push_back(kInvalidPos);
      }
      rev_alignment_.push_back(static_cast<size_t>(prefix.data() - key.data()));
      for (size_t i = 1; i < corrected_mblen; ++i) {
        rev_alignment_.push_back(kInvalidPos);
      }
    }

    prefix.remove_prefix(mblen);
    ++key_pos;
  }

  DCHECK_EQ(original_key_.size(), alignment_.size());
  DCHECK_EQ(corrected_key_.size(), rev_alignment_.size());

  available_ = true;
  return true;
}

absl::string_view KeyCorrector::GetCorrectedPrefix(
    const size_t original_key_pos) const {
  if (!IsAvailable()) {
    return "";
  }

  if (mode_ == KANA) {
    return "";
  }

  const size_t corrected_key_pos = GetCorrectedPosition(original_key_pos);
  if (!IsValidPosition(corrected_key_pos)) {
    return "";
  }

  absl::string_view corrected_substr =
      absl::string_view(corrected_key_).substr(corrected_key_pos);
  absl::string_view original_substr =
      absl::string_view(original_key_).substr(original_key_pos);
  if (corrected_substr != original_substr) {
    return corrected_substr;
  }

  return "";
}

size_t KeyCorrector::GetOriginalOffset(const size_t original_key_pos,
                                       const size_t new_key_offset) const {
  if (!IsAvailable()) {
    return kInvalidPos;
  }

  if (mode_ == KANA) {
    return kInvalidPos;
  }

  const size_t corrected_key_pos = GetCorrectedPosition(original_key_pos);
  if (!IsValidPosition(corrected_key_pos)) {
    return kInvalidPos;
  }

  if (rev_alignment_.size() == corrected_key_pos + new_key_offset) {
    return (alignment_.size() - GetOriginalPosition(corrected_key_pos));
  } else {
    // treat right edge
    const size_t original_key_pos2 =
        GetOriginalPosition(corrected_key_pos + new_key_offset);

    if (!IsValidPosition(original_key_pos2)) {
      return kInvalidPos;
    }

    // Don't allow NULL matching
    if (original_key_pos2 >= original_key_pos) {
      return static_cast<size_t>(original_key_pos2 - original_key_pos);
    }
  }

  return kInvalidPos;
}

// static
int KeyCorrector::GetCorrectedCostPenalty(absl::string_view key) {
  // "んん" and "っっ" must be mis-spelling.
  if (absl::StrContains(key, "んん") || absl::StrContains(key, "っっ")) {
    return 0;
  }
  // add 3000 to the original word cost
  constexpr int kCorrectedCostPenalty = 3000;
  return kCorrectedCostPenalty;
}

}  // namespace mozc
