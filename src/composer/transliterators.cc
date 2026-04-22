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

#include "composer/transliterators.h"

#include <cstddef>
#include <string>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "base/japanese_util.h"
#include "base/strings/assign.h"
#include "base/util.h"
#include "base/vlog.h"
#include "config/character_form_manager.h"

namespace mozc {
namespace composer {
namespace {

using ::mozc::config::CharacterFormManager;

bool SplitPrimaryString(const size_t position, const absl::string_view primary,
                        const absl::string_view secondary,
                        std::string* primary_lhs, std::string* primary_rhs,
                        std::string* secondary_lhs,
                        std::string* secondary_rhs) {
  DCHECK(primary_lhs);
  DCHECK(primary_rhs);
  DCHECK(secondary_lhs);
  DCHECK(secondary_rhs);

  Util::Utf8SubString(primary, 0, position, primary_lhs);
  Util::Utf8SubString(primary, position, std::string::npos, primary_rhs);

  // Set fallback strings.
  *secondary_lhs = *primary_lhs;
  *secondary_rhs = *primary_rhs;

  // If secondary and primary have the same suffix like "ttk" and "っtk",
  // Separation of primary can be more intelligent.

  if (secondary.size() < primary_rhs->size()) {
    // If the size of suffix of primary is greater than the whole size
    // of secondary, it must not a "ttk" and "っtk" case.
    return false;
  }

  // Position on the secondary string, where secondary and primary have the
  // same suffix.
  const size_t secondary_position = secondary.size() - primary_rhs->size();

  // Check if the secondary and primary have the same suffix.
  if (secondary_position != secondary.rfind(*primary_rhs)) {
    return false;
  }

  *secondary_rhs = *primary_rhs;
  strings::Assign(*secondary_lhs, secondary.substr(0, secondary_position));
  return true;
}

// Always uses a converted string rather than a raw string.
class ConversionStringSelector {
 public:
  static std::string Transliterate(const absl::string_view raw,
                                   const absl::string_view converted) {
    return std::string(converted);
  }

  static bool Split(size_t position, const absl::string_view raw,
                    const absl::string_view converted, std::string* raw_lhs,
                    std::string* raw_rhs, std::string* converted_lhs,
                    std::string* converted_rhs) {
    return Transliterators::SplitConverted(position, raw, converted, raw_lhs,
                                           raw_rhs, converted_lhs,
                                           converted_rhs);
  }
};

// Always uses a raw string rather than a converted string.
class RawStringSelector {
 public:
  static std::string Transliterate(const absl::string_view raw,
                                   const absl::string_view converted) {
    return std::string(raw);
  }

  static bool Split(size_t position, const absl::string_view raw,
                    const absl::string_view converted, std::string* raw_lhs,
                    std::string* raw_rhs, std::string* converted_lhs,
                    std::string* converted_rhs) {
    return Transliterators::SplitRaw(position, raw, converted, raw_lhs, raw_rhs,
                                     converted_lhs, converted_rhs);
  }
};

class HiraganaTransliterator {
 public:
  static std::string Transliterate(const absl::string_view raw,
                                   const absl::string_view converted) {
    std::string full = japanese_util::HalfWidthToFullWidth(converted);
    std::string output;
    CharacterFormManager::GetCharacterFormManager()->ConvertPreeditString(
        full, &output);
    return output;
  }

  static bool Split(size_t position, const absl::string_view raw,
                    const absl::string_view converted, std::string* raw_lhs,
                    std::string* raw_rhs, std::string* converted_lhs,
                    std::string* converted_rhs) {
    return Transliterators::SplitConverted(position, raw, converted, raw_lhs,
                                           raw_rhs, converted_lhs,
                                           converted_rhs);
  }
};

class FullKatakanaTransliterator {
 public:
  static std::string Transliterate(const absl::string_view raw,
                                   const absl::string_view converted) {
    std::string t13n = japanese_util::HiraganaToKatakana(converted);
    std::string full = japanese_util::HalfWidthToFullWidth(t13n);

    std::string output;
    CharacterFormManager::GetCharacterFormManager()->ConvertPreeditString(
        full, &output);
    return output;
  }

  static bool Split(size_t position, const absl::string_view raw,
                    const absl::string_view converted, std::string* raw_lhs,
                    std::string* raw_rhs, std::string* converted_lhs,
                    std::string* converted_rhs) {
    return Transliterators::SplitConverted(position, raw, converted, raw_lhs,
                                           raw_rhs, converted_lhs,
                                           converted_rhs);
  }
};

class HalfKatakanaTransliterator {
 public:
  static std::string HalfKatakanaToHiragana(
      const absl::string_view half_katakana) {
    std::string full_katakana =
        japanese_util::HalfWidthKatakanaToFullWidthKatakana(half_katakana);
    return japanese_util::KatakanaToHiragana(full_katakana);
  }

  static std::string Transliterate(const absl::string_view raw,
                                   const absl::string_view converted) {
    std::string katakana_output = japanese_util::HiraganaToKatakana(converted);
    return japanese_util::FullWidthToHalfWidth(katakana_output);
  }

  static bool Split(size_t position, const absl::string_view raw,
                    const absl::string_view converted, std::string* raw_lhs,
                    std::string* raw_rhs, std::string* converted_lhs,
                    std::string* converted_rhs) {
    const std::string half_katakana = Transliterate(raw, converted);
    std::string hk_raw_lhs, hk_raw_rhs, hk_converted_lhs, hk_converted_rhs;
    const bool result = Transliterators::SplitConverted(
        position, raw, half_katakana, &hk_raw_lhs, &hk_raw_rhs,
        &hk_converted_lhs, &hk_converted_rhs);

    if (result) {
      *raw_lhs = hk_raw_lhs;
      *raw_rhs = hk_raw_rhs;
    } else {
      *raw_lhs = HalfKatakanaToHiragana(hk_raw_lhs);
      *raw_rhs = HalfKatakanaToHiragana(hk_raw_rhs);
    }
    *converted_lhs = HalfKatakanaToHiragana(hk_converted_lhs);
    *converted_rhs = HalfKatakanaToHiragana(hk_converted_rhs);
    return result;
  }
};

class HalfAsciiTransliterator {
 public:
  static std::string Transliterate(const absl::string_view raw,
                                   const absl::string_view converted) {
    const absl::string_view input = raw.empty() ? converted : raw;
    return japanese_util::FullWidthAsciiToHalfWidthAscii(input);
  }

  static bool Split(size_t position, const absl::string_view raw,
                    const absl::string_view converted, std::string* raw_lhs,
                    std::string* raw_rhs, std::string* converted_lhs,
                    std::string* converted_rhs) {
    return Transliterators::SplitRaw(position, raw, converted, raw_lhs, raw_rhs,
                                     converted_lhs, converted_rhs);
  }
};

class FullAsciiTransliterator {
 public:
  static std::string Transliterate(const absl::string_view raw,
                                   const absl::string_view converted) {
    const absl::string_view input = raw.empty() ? converted : raw;
    return japanese_util::HalfWidthAsciiToFullWidthAscii(input);
  }

  static bool Split(size_t position, const absl::string_view raw,
                    const absl::string_view converted, std::string* raw_lhs,
                    std::string* raw_rhs, std::string* converted_lhs,
                    std::string* converted_rhs) {
    return Transliterators::SplitRaw(position, raw, converted, raw_lhs, raw_rhs,
                                     converted_lhs, converted_rhs);
  }
};

constexpr internal::TransliteratorInterface kConversionString = {
    &ConversionStringSelector::Transliterate,
    &ConversionStringSelector::Split};
constexpr internal::TransliteratorInterface kRawString = {
    &RawStringSelector::Transliterate, &RawStringSelector::Split};
constexpr internal::TransliteratorInterface kHiragana = {
    &HiraganaTransliterator::Transliterate, &HiraganaTransliterator::Split};
constexpr internal::TransliteratorInterface kFullKatakana = {
    &FullKatakanaTransliterator::Transliterate,
    &FullKatakanaTransliterator::Split};
constexpr internal::TransliteratorInterface kHalfKatakana = {
    &HalfKatakanaTransliterator::Transliterate,
    &HalfKatakanaTransliterator::Split};
constexpr internal::TransliteratorInterface kFullAscii = {
    &FullAsciiTransliterator::Transliterate, &FullAsciiTransliterator::Split};
constexpr internal::TransliteratorInterface kHalfAscii = {
    &HalfAsciiTransliterator::Transliterate, &HalfAsciiTransliterator::Split};

}  // namespace

// static
const internal::TransliteratorInterface* Transliterators::GetTransliterator(
    Transliterator transliterator) {
  MOZC_VLOG(2) << "Transliterators::GetTransliterator:" << transliterator;
  DCHECK(transliterator != LOCAL);
  switch (transliterator) {
    case CONVERSION_STRING:
      return &kConversionString;
    case RAW_STRING:
      return &kRawString;
    case HIRAGANA:
      return &kHiragana;
    case FULL_KATAKANA:
      return &kFullKatakana;
    case HALF_KATAKANA:
      return &kHalfKatakana;
    case FULL_ASCII:
      return &kFullAscii;
    case HALF_ASCII:
      return &kHalfAscii;
    default:
      LOG(ERROR) << "Unexpected transliterator: " << transliterator;
      // As fallback.
      return &kConversionString;
  }
}

// static
bool Transliterators::SplitRaw(const size_t position,
                               const absl::string_view raw,
                               const absl::string_view converted,
                               std::string* raw_lhs, std::string* raw_rhs,
                               std::string* converted_lhs,
                               std::string* converted_rhs) {
  return SplitPrimaryString(position, raw, converted, raw_lhs, raw_rhs,
                            converted_lhs, converted_rhs);
}

// static
bool Transliterators::SplitConverted(const size_t position,
                                     const absl::string_view raw,
                                     const absl::string_view converted,
                                     std::string* raw_lhs, std::string* raw_rhs,
                                     std::string* converted_lhs,
                                     std::string* converted_rhs) {
  return SplitPrimaryString(position, converted, raw, converted_lhs,
                            converted_rhs, raw_lhs, raw_rhs);
}

}  // namespace composer
}  // namespace mozc
