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

#include "rewriter/number_compound_util.h"

#include <algorithm>
#include <cstdint>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "base/util.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"

using mozc::dictionary::PosMatcher;

namespace mozc {
namespace number_compound_util {

bool SplitStringIntoNumberAndCounterSuffix(
    const SerializedStringArray &suffix_array, absl::string_view input,
    absl::string_view *number, absl::string_view *counter_suffix,
    uint32_t *script_type) {
  *script_type = NONE;
  absl::string_view s = input, rest = input;
  while (!s.empty()) {
    char32_t c;
    if (!Util::SplitFirstChar32(s, &c, &rest)) {
      return false;
    }
    // Ascii numbers: [0-9]
    if (0x0030 <= c && c <= 0x0039) {
      *script_type |= HALFWIDTH_ARABIC;
      s = rest;
      continue;
    }
    // Full-width arabic numbers: [０-９]
    if (0xFF10 <= c && c <= 0xFF19) {
      *script_type |= FULLWIDTH_ARABIC;
      s = rest;
      continue;
    }
    switch (c) {
      // Cases where c is one of "〇零一二三四五六七八九十百千".
      case 0x3007:
      case 0x4E00:
      case 0x4E03:
      case 0x4E09:
      case 0x4E5D:
      case 0x4E8C:
      case 0x4E94:
      case 0x516B:
      case 0x516D:
      case 0x5341:
      case 0x5343:
      case 0x56DB:
      case 0x767E:
      case 0x96F6:
        *script_type |= KANJI;
        s = rest;
        continue;
      // Cases where c is one of "壱弐参".
      case 0x58F1:
      case 0x5F10:
      case 0x53C2:
        *script_type |= OLD_KANJI;
        s = rest;
        continue;
    }
    break;
  }
  *number = input.substr(0, input.size() - s.size());
  *counter_suffix = s;
  return counter_suffix->empty() ||
         std::binary_search(suffix_array.begin(), suffix_array.end(),
                            *counter_suffix);
}

bool IsNumber(const SerializedStringArray &suffix_array,
              const PosMatcher &pos_matcher, const Segment::Candidate &cand) {
  // Compound number entries have the left POS ID of number.
  if (pos_matcher.IsNumber(cand.lid) || pos_matcher.IsKanjiNumber(cand.lid)) {
    return true;
  }
  // Some number candidates possibly have noun POS, e.g., 一階.  We further
  // check the opportunities of rewriting such number nouns.
  // TODO(toshiyuki, team): It may be better to set number POS to such number
  // noun entries at dictionary build time.  correct POS.  Then, we can omit the
  // following runtime structure check.
  if (!pos_matcher.IsGeneralNoun(cand.lid)) {
    return false;
  }
  // Try splitting candidate's content value to number and counter suffix.  If
  // it's successful and the resulting number component is nonempty, we may
  // assume the candidate is number.  This check prevents, e.g., the following
  // misrewrite: 百舌鳥(もず, noun) -> 100舌鳥, １００舌鳥, etc.
  absl::string_view number, suffix;
  uint32_t script_type = 0;
  if (!number_compound_util::SplitStringIntoNumberAndCounterSuffix(
          suffix_array, cand.content_value, &number, &suffix, &script_type)) {
    return false;
  }
  // Some number including general noun should be excluded from IsNumber().
  static const absl::NoDestructor<
      absl::flat_hash_set<std::pair<absl::string_view, absl::string_view>>>
      kExceptions({
          {"いざよい", "十六夜"},
          {"いちぎ", "一義"},
          {"いちじょ", "一女"},
          {"いっか", "一家"},
          {"いっこう", "一行"},
          {"いったい", "一体"},
          {"いっとき", "一時"},
          {"おはこ", "十八番"},
          {"ここのえ", "九重"},
          {"ごごん", "五言"},
          {"ごしちにち", "五七日"},
          {"ごじゅうさんつぎ", "五十三次"},
          {"ごせち", "五節"},
          {"さんきゃく", "三脚"},
          {"さんさんくど", "三三九度"},
          {"さんしちにち", "三七日"},
          {"さんしゃ", "三者"},
          {"しき", "四季"},
          {"しきゅう", "四球"},
          {"しちごん", "七言"},
          {"しちりん", "七輪"},
          {"しで", "四手"},
          {"じゅうじ", "十字"},
          {"しろくばん", "四六判"},
          {"しろくぶん", "四六文"},
          {"しんし", "参差"},
          {"せんきん", "千金"},
          {"せんげん", "千言"},
          {"せんざい", "千歳"},
          {"せんしん", "千進"},
          {"せんすじ", "千筋"},
          {"せんみつ", "千三つ"},
          {"ちくさ", "千種"},
          {"ちとせ", "千年"},
          {"ちとせ", "千歳"},
          {"ちよ", "千代"},
          {"とうしゃく", "十尺"},
          {"とえ", "十重"},
          {"とき", "十寸"},
          {"ななえ", "七重"},
          {"ななくさ", "七種"},
          {"なななぬか", "七七日"},
          {"はたえ", "二十重"},
          {"はちじゅうはちや", "八十八夜"},
          {"はっぴゃくやちょう", "八百八町"},
          {"ひとえ", "一重"},
          {"ひとなのか", "一七日"},
          {"ひゃっこう", "百行"},
          {"ふたえ", "二重"},
          {"ふたご", "二子"},
          {"ふたなのか", "二七日"},
          {"ふたば", "二葉"},
          {"ふため", "二目"},
          {"みけ", "三毛"},
          {"みそか", "三十日"},
          {"みつくち", "三口"},
          {"みつご", "三児"},
          {"みつご", "三子"},
          {"みつば", "三葉"},
          {"みつめ", "三目"},
          {"みなのか", "三七日"},
          {"むかで", "百足"},
          {"ももとせ", "百歳"},
          {"やえ", "八重"},
          {"やちよ", "八千代"},
          {"やつがしら", "八頭"},
          {"やつくち", "八口"},
          {"やつで", "八手"},
          {"やつはし", "八橋"},
          {"ゆり", "百合"},
          {"よも", "四方"},
          {"りくごう", "六合"},
          {"りくたい", "六体"},
          {"れいほん", "零本"},
          {"ろくどう", "六道"},
          {"ろっかい", "六界"},
          {"ろっぽう", "六方"},
          {"ろっぽう", "六法"},
  });
  if (kExceptions->contains(std::make_pair(cand.key, cand.value))) {
    return false;
  }
  return !number.empty();
}

}  // namespace number_compound_util
}  // namespace mozc
