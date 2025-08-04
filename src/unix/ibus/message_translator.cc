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

#include <cstddef>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_split.h"
#include "base/util.h"

namespace {

struct TranslationMap {
  const char *message;
  const char *translated;
};

const TranslationMap kUTF8JapaneseMap[] = {
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
};

}  // namespace

namespace mozc {
namespace ibus {

MessageTranslatorInterface::~MessageTranslatorInterface() = default;

NullMessageTranslator::NullMessageTranslator() = default;

std::string NullMessageTranslator::MaybeTranslate(
    const std::string &message) const {
  return message;
}

LocaleBasedMessageTranslator::LocaleBasedMessageTranslator(
    const std::string &locale_name) {
  // Currently we support ja_JP.UTF-8 and ja_JP.utf8 only.
  std::vector<std::string> tokens =
      absl::StrSplit(locale_name, '.', absl::SkipEmpty());
  if (tokens.size() != 2) {
    return;
  }
  const std::string &language_code = tokens[0];
  if (language_code != "ja_JP") {
    return;
  }

  Util::LowerString(&tokens[1]);
  const std::string &lowser_char_set_name = tokens[1];
  if (lowser_char_set_name != "utf-8" && lowser_char_set_name != "utf8") {
    return;
  }

  for (size_t i = 0; i < std::size(kUTF8JapaneseMap); ++i) {
    const TranslationMap &mapping = kUTF8JapaneseMap[i];
    DCHECK(mapping.message);
    DCHECK(mapping.translated);
    utf8_japanese_map_.insert(
        std::make_pair(mapping.message, mapping.translated));
  }
}

std::string LocaleBasedMessageTranslator::MaybeTranslate(
    const std::string &message) const {
  std::map<std::string, std::string>::const_iterator itr =
      utf8_japanese_map_.find(message);
  if (itr == utf8_japanese_map_.end()) {
    return message;
  }

  return itr->second;
}

}  // namespace ibus
}  // namespace mozc
