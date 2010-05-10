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

#include "converter/key_corrector.h"

#include <string.h>

#include "base/base.h"
#include "base/util.h"

namespace mozc {
namespace {
// maximum key length KeyCorrector can handle
// if key is too long, we don't do key correction
const size_t kMaxSize = 128;

// invalid alignment marker
const size_t kInvalidPos = static_cast<size_t>(-1);

// "ん" (few "n" pettern)
// "んあ" -> "んな"
// "んい" -> "んに"
// "んう" -> "んぬ"
// "んえ" -> "んね"
// "んお" -> "んの"
bool RewriteNN(size_t key_pos,
               const char *begin, const char *end,
               size_t *mblen, string *output) {
  if (key_pos == 0) {
    *mblen = 0;
    return false;
  }

  const uint16 ucs2 = Util::UTF8ToUCS2(begin, end, mblen);
  if (ucs2 != 0x3093) {  // "ん"
    *mblen = 0;
    return false;
  }

  if (begin + *mblen >= end) {
    *mblen = 0;
    return false;
  }

  size_t mblen2 = 0;
  const uint16 next_ucs2 = Util::UTF8ToUCS2(begin + *mblen, end, &mblen2);
  uint16 output_ucs2 = 0x0000;
  switch (next_ucs2) {
    case 0x3042:  // "あ"
      output_ucs2 = 0x306A;   // "な"
      break;
    case 0x3044:  // "い"
      output_ucs2 = 0x306B;   // "に"
      break;
    case 0x3046:  // "う"
      output_ucs2 = 0x306C;   // "ぬ"
      break;
    case 0x3048:  // "え"
      output_ucs2 = 0x306D;   // "ね"
      break;
    case 0x304A:  // "お"
      output_ucs2 = 0x306E;   // "の"
      break;
    default:
      break;
  }

  if (output_ucs2 != 0x0000) {   // "ん[あいうえお]"
    Util::UCS2ToUTF8Append(ucs2, output);
    Util::UCS2ToUTF8Append(output_ucs2, output);
    *mblen += mblen2;
    return true;
  } else {   // others
    *mblen = 0;
    return false;
  }

  return false;
}

// "んん" (many "n" pattern)
// "([^ん])んん[ん]" -> ignore
// "([^ん])んん[あいうえお]" ->  $1 and leave "ん[あいうえお]"
// "([^ん])んん[^あいうえお]" -> $1"ん" and leave "[^あいうえお]"
bool RewriteDoubleNN(size_t key_pos,
                     const char *begin, const char *end,
                     size_t *mblen, string *output) {
  // 0x3093: "ん"
  static const uint16 kPattern[] = { 0x0000, 0x3093, 0x3093 };

  *mblen = 0;
  uint16 first_char = 0x0000;
  size_t first_mblen = 0;
  for (size_t i = 0; i < arraysize(kPattern); ++i) {
    if (begin >= end) {
      *mblen = 0;
      return false;
    }
    size_t mblen2 = 0;
    const uint16 ucs2 = Util::UTF8ToUCS2(begin, end, &mblen2);
    if ((kPattern[i] == 0x0000 && ucs2 != 0x3093 &&
         Util::GetScriptType(ucs2) == Util::HIRAGANA) ||
        (kPattern[i] == 0x3093 && ucs2 == 0x3093)) {
      if (i == 0) {
        first_mblen = mblen2;
        first_char = ucs2;
      }
    } else {
      *mblen = 0;
      return false;
    }
    begin += mblen2;
    *mblen += mblen2;
  }

  if (begin >= end) {
    *mblen = 0;
    return false;
  }

  if (first_char == 0x0000 || first_mblen == 0) {
    *mblen = 0;
    return false;
  }

  size_t mblen2 = 0;
  const uint16 ucs2 = Util::UTF8ToUCS2(begin, end, &mblen2);
  if (ucs2 == 0x3093) {   // "ん" ignore
    *mblen = 0;
    return false;
  } else if (ucs2 == 0x3042 ||      // "[あいうえお]"
             ucs2 == 0x3044 ||
             ucs2 == 0x3046 ||
             ucs2 == 0x3048 ||
             ucs2 == 0x304A) {
    // drop first "ん" and leave "ん[あいうえお]"
    // remained part will be handled by RewriteNN(), e.g, "んあ" -> "んな"
    Util::UCS2ToUTF8Append(first_char, output);
    *mblen = first_mblen + 3;
    return true;
  } else {  // "[^あいうえお]"
    Util::UCS2ToUTF8Append(first_char, output);
    Util::UCS2ToUTF8Append(0x3093, output);   // "ん"
    return true;
  }

  return false;
}

// "に" pattern
// "にゃ" -> "んや"
// "にゅ" -> "んゆ"
// "にょ" -> "んよ"
bool RewriteNI(size_t key_pos,
               const char *begin, const char *end,
               size_t *mblen, string *output) {
  const uint16 ucs2 = Util::UTF8ToUCS2(begin, end, mblen);
  if (ucs2 != 0x306B) {  // "に"
    *mblen = 0;
    return false;
  }

  if (begin + *mblen >= end) {
    *mblen = 0;
    return false;
  }

  size_t mblen2 = 0;
  const uint16 next_ucs2 = Util::UTF8ToUCS2(begin + *mblen, end, &mblen2);
  uint16 output_ucs2 = 0x0000;
  switch (next_ucs2) {
    case 0x3083:   // "ゃ"
      output_ucs2 = 0x3084;  // "や"
      break;
    case 0x3085:   // "ゅ"
      output_ucs2 = 0x3086;  // "ゆ"
      break;
    case 0x3087:   // "ょ"
      output_ucs2 = 0x3088;  // "よ"
      break;
    default:
      output_ucs2 = 0x0000;
      break;
  }

  if (output_ucs2 != 0x0000) {
    Util::UCS2ToUTF8Append(0x3093, output);    // "ん"
    Util::UCS2ToUTF8Append(output_ucs2, output);
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
bool RewriteM(size_t key_pos,
              const char *begin, const char *end,
              size_t *mblen, string *output) {
  if (key_pos == 0) {
    *mblen = 0;
    return false;
  }
  const uint16 ucs2 = Util::UTF8ToUCS2(begin, end, mblen);
  // "m" or "ｍ" (don't take capitial letter, as "M" might not be a misspelling)
  if (ucs2 != 0x006D && ucs2 != 0xFF4D) {
    *mblen = 0;
    return false;
  }

  if (begin + *mblen >= end) {
    *mblen = 0;
    return false;
  }

  size_t mblen2 = 0;
  const uint16 next_ucs2 = Util::UTF8ToUCS2(begin + *mblen, end, &mblen2);
  // "[はばぱひびぴふぶぷへべぺほぼぽ]" => [0x306F .. 0X307D]
  // Here we want to take "[は..ぽ]" except for "はひふへほ"
  if (next_ucs2 % 3 != 0 &&   // not "はひふへほ"
      next_ucs2 >= 0x306F && next_ucs2 <= 0x307D) {  // "は..ぽ"
    Util::UCS2ToUTF8Append(0x3093, output);    // "ん"
    Util::UCS2ToUTF8Append(next_ucs2, output);
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
// e.g, "かっっった" -> "かっっった"
bool RewriteSmallTSU(size_t key_pos,
                     const char *begin, const char *end,
                     size_t *mblen, string *output) {
  // 0x0000 is a place holder for "[^っ]"
  // "っ": 0x3063
  static const uint16 kPattern[] = { 0x0000, 0x3063, 0x3063, 0x0000 };

  uint16 first_char = 0x0000;
  uint16 last_char = 0x0000;
  for (size_t i = 0; i < arraysize(kPattern); ++i) {
    if (begin >= end) {
      *mblen = 0;
      return false;
    }
    size_t mblen2 = 0;
    const uint16 ucs2 = Util::UTF8ToUCS2(begin, end, &mblen2);
    if ((kPattern[i] == 0x0000 && ucs2 != 0x3063 &&
         Util::GetScriptType(ucs2) == Util::HIRAGANA) ||
        (kPattern[i] == 0x3063 && ucs2 == 0x3063)) {
      if (i == 0) {
        first_char = ucs2;
      } else if (i == arraysize(kPattern) - 1) {
        last_char = ucs2;
      }
    } else {
      *mblen = 0;
      return false;
    }
    begin += mblen2;
    *mblen += mblen2;
  }

  if (first_char == 0x0000 || last_char == 0x0000) {
    *mblen = 0;
    return false;
  }

  Util::UCS2ToUTF8Append(first_char, output);
  Util::UCS2ToUTF8Append(0x3063, output);   // "っ"
  Util::UCS2ToUTF8Append(last_char, output);

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
bool RewriteYu(size_t key_pos,
               const char *begin, const char *end,
               size_t *mblen, string *output) {
  const uint16 first_char = Util::UTF8ToUCS2(begin, end, mblen);
  if (first_char != 0x304D && first_char != 0x3057 &&
      first_char != 0x3061 && first_char != 0x306B &&
      first_char != 0x3072 && first_char != 0x308A) {  // !"きしちにひり"
    *mblen = 0;
    return false;
  }

  if (begin + *mblen >= end) {
    *mblen = 0;
    return false;
  }

  size_t mblen2 = 0;
  const uint16 next_char = Util::UTF8ToUCS2(begin + *mblen, end, &mblen2);
  if (next_char != 0x3085) {   // "ゅ"
    *mblen = 0;
    return false;
  }

  if (begin + *mblen + mblen2 >= end) {
    *mblen = 0;
    return false;
  }

  size_t mblen3 = 0;
  const uint16 last_char = Util::UTF8ToUCS2(begin + *mblen + mblen2,
                                            end, &mblen3);
  if (last_char == 0x3046) {   // "う"
    *mblen = 0;
    return false;
  }

  // OK, rewrite
  *mblen += mblen2;
  Util::UCS2ToUTF8Append(first_char, output);
  Util::UCS2ToUTF8Append(next_char, output);   // "ゅ"
  Util::UCS2ToUTF8Append(0x3046, output);      // "う"


  return true;
}
}  // namespace

KeyCorrector::KeyCorrector(const string &key, InputMode mode)
    : available_(false), mode_(mode) {
  CorrectKey(key, mode);
}

KeyCorrector::KeyCorrector()
    : available_(false), mode_(ROMAN) {}

KeyCorrector::~KeyCorrector() {}

KeyCorrector::InputMode KeyCorrector::mode() const {
  return mode_;
}

bool KeyCorrector::IsAvailable() const {
  return available_;
}

// return corrected key;
const string &KeyCorrector::corrected_key() const {
  return corrected_key_;
}

const string &KeyCorrector::original_key() const {
  return original_key_;
}

size_t KeyCorrector::GetCorrectedPosition(const size_t original_key_pos) const {
  if (original_key_pos >= 0 &&
      original_key_pos < alignment_.size()) {
    return alignment_[original_key_pos];
  }
  return kInvalidPos;
}

size_t KeyCorrector::GetOriginalPosition(const size_t corrected_key_pos) const {
  if (corrected_key_pos >= 0 &&
      corrected_key_pos < rev_alignment_.size()) {
    return rev_alignment_[corrected_key_pos];
  }
  return kInvalidPos;
}

void KeyCorrector::Clear() {
  available_ = false;
  original_key_.clear();
  corrected_key_.clear();
  alignment_.clear();
  rev_alignment_.clear();
}

bool KeyCorrector::CorrectKey(const string &key, InputMode mode) {
  Clear();

  // TOOD(taku)  support KANA
  if (mode == KANA) {
    return false;
  }

  if (key.size() == 0 || key.size() >= kMaxSize) {
    VLOG(1) << "invalid key length";
    return false;
  }

  original_key_ = key;

  const char *begin = key.data();
  const char *end = key.data() + key.size();
  size_t key_pos = 0;

  while (begin < end) {
    size_t mblen = 0;
    const size_t org_len = corrected_key_.size();
    if (RewriteDoubleNN(key_pos, begin, end, &mblen, &corrected_key_) ||
        RewriteNN(key_pos, begin, end, &mblen, &corrected_key_) ||
        RewriteYu(key_pos,  begin, end, &mblen, &corrected_key_) ||
        RewriteNI(key_pos, begin, end, &mblen, &corrected_key_) ||
        RewriteSmallTSU(key_pos, begin, end, &mblen, &corrected_key_) ||
        RewriteM(key_pos,  begin, end, &mblen, &corrected_key_)) {
      // do nothing
    } else {
      const uint16 ucs2 = Util::UTF8ToUCS2(begin, end, &mblen);
      Util::UCS2ToUTF8Append(ucs2, &corrected_key_);
    }

    const size_t corrected_mblen = corrected_key_.size() - org_len;

    if (corrected_mblen <= 0 && mblen <= 0) {
      LOG(ERROR) << "Invalid pattern: " << key;
      Clear();
      return false;
    }

    // one to one mapping
    if (mblen == corrected_mblen) {
      const size_t len = static_cast<size_t>(begin - key.data());
      for (size_t i = 0; i < mblen; ++i) {
        alignment_.push_back(org_len + i);
        rev_alignment_.push_back(len + i);
      }
    } else {
    // NOT a one to one maping, we take fist/last alignment only
      alignment_.push_back(org_len);
      for (size_t i = 1; i < mblen; ++i) {
        alignment_.push_back(kInvalidPos);
      }
      rev_alignment_.push_back(static_cast<size_t>(begin - key.data()));
      for (size_t i = 1; i < corrected_mblen; ++i) {
        rev_alignment_.push_back(kInvalidPos);
      }
    }

    begin += mblen;
    ++key_pos;
  }

  DCHECK_EQ(original_key_.size(), alignment_.size());
  DCHECK_EQ(corrected_key_.size(), rev_alignment_.size());

  available_ = true;
  return true;
}

const char *KeyCorrector::GetCorrectedPrefix(const size_t original_key_pos,
                                             size_t *length) const {
  if (!IsAvailable()) {
    *length = 0;
    return NULL;
  }

  if (mode_ == KANA) {
    *length = 0;
    return NULL;
  }

  const size_t corrected_key_pos = GetCorrectedPosition(original_key_pos);
  if (!IsValidPosition(corrected_key_pos)) {
    *length = 0;
    return NULL;
  }

  const char *corrected_substr = corrected_key_.data() + corrected_key_pos;
  const size_t corrected_length = corrected_key_.size() - corrected_key_pos;
  const char *original_substr = original_key_.data() + original_key_pos;
  const size_t original_length = original_key_.size() - original_key_pos;
  // substrs are different
  if (original_length != corrected_length ||
      memcmp(original_substr, corrected_substr, original_length) != 0) {
    *length = corrected_length;
    return corrected_substr;
  }

  *length = 0;
  return NULL;
}

size_t KeyCorrector::GetOriginalOffset(const size_t original_key_pos,
                                       const size_t new_key_offset) const {
  if (!IsAvailable()) {
    return kInvalidPos;
  }

  if (mode_ == KANA) {
    return kInvalidPos;
  }

  const size_t corrected_key_pos =
      GetCorrectedPosition(original_key_pos);
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
bool KeyCorrector::IsValidPosition(size_t pos) {
  return (pos != kInvalidPos);
}

// static
bool KeyCorrector::IsInvalidPosition(size_t pos) {
  return (pos == kInvalidPos);
}

// static
size_t KeyCorrector::InvalidPosition() {
  return kInvalidPos;
}
}  // namespace mozc
