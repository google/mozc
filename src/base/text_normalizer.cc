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

#include "base/text_normalizer.h"

#include <array>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "base/util.h"

namespace mozc {
namespace {

// Unicode vender specific character table:
// http://hp.vector.co.jp/authors/VA010341/unicode/
// http://www.notoinsatu.co.jp/font/omake/OTF_other.pdf
//
// Example: WAVE_DASH / FULLWIDTH TILDE
// http://ja.wikipedia.org/wiki/%E6%B3%A2%E3%83%80%E3%83%83%E3%82%B7%E3%83%A5
// Windows CP932 (shift-jis) maps WAVE_DASH to FULL_WIDTH_TILDA.
// Since the font of WAVE-DASH is ugly on Windows, here we convert WAVE-DHASH to
// FULL_WIDTH_TILDA as CP932 does.
//
// As Unicode has became the defacto default encoding, we have reduced
// the number of characters to be normalized.
inline char32_t NormalizeCharForWindows(char32_t c) {
  switch (c) {
    case 0x301C:      // WAVE DASH
      return 0xFF5E;  // FULLWIDTH TILDE
      break;
    case 0x2212:      // MINUS SIGN
      return 0xFF0D;  // FULLWIDTH HYPHEN MINUS
      break;
    default:
      return c;
      break;
  }
}

std::pair<int, int> ConvertJaCjkCompatToSvs(char32_t cjk_compat_char) {
  constexpr const std::pair<int, int> no_value = {0, 0};

  // value (2N):   codepoint of CJK compatibility character.
  // value (2N+1): codepoint of SVS base character.
  constexpr std::array<int, 16> exceptions = {
      0xF91D, 0x6B04,   // {欄, 欄} defined in KS X 1001.
      0xF928, 0x5ECA,   // {廊, 廊} defined in KS X 1001.
      0xF929, 0x6717,   // {朗, 朗} defined in KS X 1001.
      0xF936, 0x865C,   // {虜, 虜} defined in KS X 1001.
      0xF970, 0x6BBA,   // {殺, 殺} defined in KS X 1001.
      0xF9D0, 0x985E,   // {類, 類} defined in KS X 1001.
      0xF9DC, 0x9686,   // {隆, 隆} defined in KS X 1001.
      0xFA6C, 0x242EE,  // {𤋮, 𤋮} value is more than 16bits.
  };

  // value: codepoint of CJK compatibility character
  //        to be converted to SVS character with FE01.
  constexpr std::array<uint16_t, 3> fe01_chars = {
      0xFA57,  // 練 → 7DF4 FE01 練
      0xFA5E,  // 艹 → 8279 FE01 艹
      0xFA67,  // 逸 → 8279 FE01 艹
  };

  // index: codepoint of CJK compatibility character - 0xFA10.
  // value: codepoint of SVS base character.
  constexpr std::array<uint16_t, 94> conv_table = {
      // clang-format off
      // FA10
      //  塚     (﨑)     晴     (﨓)    (﨔)     凞      猪      益
      0x585A, 0x0000, 0x6674, 0x0000, 0x0000, 0x51DE, 0x732A, 0x76CA,
      // FA18
      //  礼      神      祥      福      靖      精      羽     (﨟)
      0x793C, 0x795E, 0x7965, 0x798F, 0x9756, 0x7CBE, 0x7FBD, 0x0000,
      // FA20
      //  蘒     (﨡)     諸     (﨣)    (﨤)     逸      都     (﨧)
      0x8612, 0x0000, 0x8AF8, 0x0000, 0x0000, 0x9038, 0x90FD, 0x0000,
      // FA28
      // (﨨)    (﨩)     飯      飼      館      鶴     (郞)    (隷)
      0x0000, 0x0000, 0x98EF, 0x98FC, 0x9928, 0x9DB4, 0x90DE, 0x96B7,
      // FA30
      //  侮      僧      免      勉      勤      卑      喝      嘆
      0x4FAE, 0x50E7, 0x514D, 0x52C9, 0x52E4, 0x5351, 0x559D, 0x5606,
      // FA38
      //  器      塀      墨      層      屮      悔      慨      憎
      0x5668, 0x5840, 0x58A8, 0x5C64, 0x5C6E, 0x6094, 0x6168, 0x618E,
      // FA40
      //  懲      敏      既      暑      梅      海      渚      漢
      0x61F2, 0x654F, 0x65E2, 0x6691, 0x6885, 0x6D77, 0x6E1A, 0x6F22,
      // FA48
      //  煮      爫      琢      碑      社      祉      祈      祐
      0x716E, 0x722B, 0x7422, 0x7891, 0x793E, 0x7949, 0x7948, 0x7950,
      // FA50
      //  祖      祝      禍      禎      穀      突      節      練
      0x7956, 0x795D, 0x798D, 0x798E, 0x7A40, 0x7A81, 0x7BC0, 0x7DF4,
      // FA58
      //  縉      繁      署      者      臭      艹      艹      著
      0x7E09, 0x7E41, 0x7F72, 0x8005, 0x81ED, 0x8279, 0x8279, 0x8457,
      // FA60
      //  褐      視      謁      謹      賓      贈      辶      逸
      0x8910, 0x8996, 0x8B01, 0x8B39, 0x8CD3, 0x8D08, 0x8FB6, 0x8279,
      // FA68 - FA6D
      //  難      響      頻      恵     (𤋮)     舘
      0x96E3, 0x97FF, 0x983B, 0x6075, 0x0000, 0x8218,
      // clang-format on
  };

  // If the char is out of all data range, return kNoValue.
  // kExceptionMap: 0xF91D - 0xFA6C
  // kFe01Set:      0xFA57 - 0xFA67
  // kConvTable:    0xFA10 - 0xFA6D
  if (cjk_compat_char < 0xF91D || cjk_compat_char > 0xFA6D) {
    return no_value;
  }

  int svs_base = 0;
  int svs_extend = 0xFE00;

  // Check the value in `exceptions`.
  for (int i = 0; i < exceptions.size(); i += 2) {
    if (cjk_compat_char == exceptions[i]) {
      svs_base = exceptions[i + 1];
      // svs_extend for all values in `exceptions` is 0xFE00;
      return std::make_pair(svs_base, svs_extend);
    }
  }

  // Check if the char is out of kConvTable. Upper range is already checked.
  if (cjk_compat_char < 0xFA10) {
    return no_value;
  }

  // Get the value from the table.
  svs_base = conv_table[cjk_compat_char - 0xFA10];
  if (svs_base == 0) {
    return no_value;
  }

  // Check if the SVS extend is FE01. In most cases, it's FE00.
  // There is no case of 0xFE02 or more for Japanese CJK compatibility chars.
  for (int i = 0; i < fe01_chars.size(); ++i) {
    if (cjk_compat_char == fe01_chars[i]) {
      svs_extend++;
      break;
    }
  }
  return std::make_pair(svs_base, svs_extend);
}

std::string NormalizeTextForWindows(absl::string_view input) {
  std::string output;
  for (ConstChar32Iterator iter(input); !iter.Done(); iter.Next()) {
    Util::CodepointToUtf8Append(NormalizeCharForWindows(iter.Get()), &output);
  }
  return output;
}
}  // namespace

std::string TextNormalizer::NormalizeTextWithFlag(absl::string_view input,
                                                  TextNormalizer::Flag flag) {
  if (flag == TextNormalizer::kDefault) {
#ifdef _WIN32
    flag = TextNormalizer::kAll;
#else   // _WIN32
    flag = TextNormalizer::kNone;
#endif  // _WIN32
  }

  if (flag != TextNormalizer::kAll) {
    return std::string(input.data(), input.size());
  }

  return NormalizeTextForWindows(input);
}

bool TextNormalizer::NormalizeTextToSvs(absl::string_view input,
                                        std::string* output) {
  std::u32string codepoints = Util::Utf8ToUtf32(input);
  std::u32string normalized;
  bool modified = false;
  for (const char32_t cp : codepoints) {
    const std::pair<int, int> svs = ConvertJaCjkCompatToSvs(cp);
    if (svs.first == 0) {
      normalized.push_back(cp);
    } else {
      modified = true;
      normalized.push_back(static_cast<char32_t>(svs.first));
      normalized.push_back(static_cast<char32_t>(svs.second));
    }
  }
  if (!modified) {
    return false;
  }

  *output = Util::Utf32ToUtf8(normalized);
  return true;
}

std::string TextNormalizer::NormalizeTextToSvs(absl::string_view input) {
  std::string output;
  if (NormalizeTextToSvs(input, &output)) {
    return output;
  }
  return std::string(input.data(), input.size());
}
}  // namespace mozc
