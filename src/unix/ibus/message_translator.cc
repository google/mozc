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

#include "unix/ibus/message_translator.h"

namespace {

struct TranslationMap {
  const char *message;
  const char *translated;
};

const TranslationMap kUTF8JapaneseMap[] = {
  // "直接入力"
  { "Direct input", "\xE7\x9B\xB4\xE6\x8E\xA5\xE5\x85\xA5\xE5\x8A\x9B" },
  // "ひらがな"
  { "Hiragana", "\xE3\x81\xB2\xE3\x82\x89\xE3\x81\x8C\xE3\x81\xAA" },
  // "カタカナ"
  { "Katakana", "\xE3\x82\xAB\xE3\x82\xBF\xE3\x82\xAB\xE3\x83\x8A" },
  // "半角英数"
  { "Latin", "\xE5\x8D\x8A\xE8\xA7\x92\xE8\x8B\xB1\xE6\x95\xB0" },
  // "全角英数"
  { "Wide Latin", "\xE5\x85\xA8\xE8\xA7\x92\xE8\x8B\xB1\xE6\x95\xB0"},
  // "半角カタカナ"
  { "Half width katakana",
    "\xE5\x8D\x8A\xE8\xA7\x92"
    "\xE3\x82\xAB\xE3\x82\xBF\xE3\x82\xAB\xE3\x83\x8A" },
  // "プロパティ"
  { "Properties",
    "\xE3\x83\x97\xE3\x83\xAD\xE3\x83\x91\xE3\x83\x86\xE3\x82\xA3" },
  // "辞書ツール"
  { "Dictionary tool",
    "\xE8\xBE\x9E\xE6\x9B\xB8\xE3\x83\x84\xE3\x83\xBC\xE3\x83\xAB" },
  // "単語登録"
  { "Add Word",
    "\xE5\x8D\x98\xE8\xAA\x9E\xE7\x99\xBB\xE9\x8C\xB2" },
  // "手書き文字入力"
  { "Handwrinting",
    "\xE6\x89\x8B\xE6\x9B\xB8\xE3\x81\x8D"
    "\xE6\x96\x87\xE5\xAD\x97\xE5\x85\xA5\xE5\x8A\x9B" },
  // "文字パレット"
  { "Character Palette",
    "\xE6\x96\x87\xE5\xAD\x97"
    "\xE3\x83\x91\xE3\x83\xAC\xE3\x83\x83\xE3\x83\x88" },
  // "Mozc について"
  // TODO(team): This should be "Google 日本語入力について"
  //     for official branding build.
  { "About Mozc",
    "\x4D\x6F\x7A\x63\x20\xE3\x81\xAB\xE3\x81\xA4\xE3\x81\x84\xE3\x81\xA6" },
};

}  // namespace

namespace mozc {
namespace ibus {

MessageTranslatorInterface::~MessageTranslatorInterface() {}

NullMessageTranslator::NullMessageTranslator() {}

string NullMessageTranslator::MaybeTranslate(const string &message) const {
  return message;
}

LocaleBasedMessageTranslator::LocaleBasedMessageTranslator(
    const string &locale_name) {
  // Currently we support ja_JP.UTF-8 only.
  if (locale_name != "ja_JP.UTF-8") {
    return;
  }

  for (size_t i = 0; i < arraysize(kUTF8JapaneseMap); ++i) {
    const TranslationMap &mapping = kUTF8JapaneseMap[i];
    DCHECK(mapping.message);
    DCHECK(mapping.translated);
    utf8_japanese_map_.insert(
        make_pair(mapping.message, mapping.translated));
  }
}

string LocaleBasedMessageTranslator::MaybeTranslate(
    const string &message) const {
  map<string, string>::const_iterator itr =
      utf8_japanese_map_.find(message);
  if (itr == utf8_japanese_map_.end()) {
    return message;
  }

  return itr->second;
}

}  // namespace ibus
}  // namespace mozc
