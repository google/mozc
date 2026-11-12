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

#include "unix/ibus/message_translator.h"

#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "base/container/flat_map.h"

namespace mozc {
namespace ibus {
namespace {

constexpr auto kUTF8JapaneseMap =
    CreateFlatMap<absl::string_view, absl::string_view>({
        {"Direct input", "直接入力"},
        {"Hiragana", "ひらがな"},
        {"Katakana", "カタカナ"},
        {"Latin", "半角英数"},
        {"Wide Latin", "全角英数"},
        {"Half width katakana", "半角カタカナ"},
        {"Tools", "ツール"},
        {"Properties", "プロパティ"},
        {"Dictionary Tool", "辞書ツール"},
        {"Add Word", "単語登録"},
        {"Input Mode", "入力モード"},
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
        {"About Mozc", "Google 日本語入力について"},
#else   // GOOGLE_JAPANESE_INPUT_BUILD
        {"About Mozc", "Mozc について"},
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
    });

}  // namespace

MessageTranslatorInterface::~MessageTranslatorInterface() = default;

NullMessageTranslator::NullMessageTranslator() = default;

std::string NullMessageTranslator::MaybeTranslate(
    absl::string_view message) const {
  return std::string(message);
}

LocaleBasedMessageTranslator::LocaleBasedMessageTranslator(
    absl::string_view locale_name) {
  // Currently we support ja_JP.UTF-8 and ja_JP.utf8 only.
  const std::vector<absl::string_view> tokens =
      absl::StrSplit(locale_name, '.', absl::SkipEmpty());
  if (tokens.size() != 2 || tokens[0] != "ja_JP") {
    return;
  }
  is_japanese_locale_ = absl::EqualsIgnoreCase(tokens[1], "utf-8") ||
                        absl::EqualsIgnoreCase(tokens[1], "utf8");
}

std::string LocaleBasedMessageTranslator::MaybeTranslate(
    absl::string_view message) const {
  if (!is_japanese_locale_) {
    return std::string(message);
  }
  if (const absl::string_view* translated =
          kUTF8JapaneseMap.FindOrNull(message);
      translated != nullptr) {
    return std::string(*translated);
  }
  return std::string(message);
}

}  // namespace ibus
}  // namespace mozc
