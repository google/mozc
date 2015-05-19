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

#include "languages/pinyin/punctuation_table.h"

#include "base/singleton.h"

namespace mozc {
namespace pinyin {
namespace punctuation {

namespace {

const char *kPunctuationDefaultCandidatesTable[] = {
  // "·", "，", "。", "「", "」", "、", "：", "；", "？", "！",
  "\xC2\xB7",     "\xEF\xBC\x8C", "\xE3\x80\x82", "\xE3\x80\x8C",
  "\xE3\x80\x8D", "\xE3\x80\x81", "\xEF\xBC\x9A", "\xEF\xBC\x9B",
  "\xEF\xBC\x9F", "\xEF\xBC\x81",
};

const struct PunctuationCandidatesTable {
  const char key;
  // We should allocate enough memory to contain candidates or we will get
  // compile errors.
  const char *candidates[11];
} kPunctuationCandidatesTable[] = {
  {  // "！", "﹗", "‼", "⁉"
    '!',
    { "\xEF\xBC\x81", "\xEF\xB9\x97", "\xE2\x80\xBC", "\xE2\x81\x89", NULL },
  }, {  // "“", "”", "＂"
    '"',
    { "\xE2\x80\x9C", "\xE2\x80\x9D", "\xEF\xBC\x82", NULL },
  }, {  // "＃", "﹟", "♯"
    '#',
    { "\xEF\xBC\x83", "\xEF\xB9\x9F", "\xE2\x99\xAF", NULL },
  }, {  // "＄", "€", "﹩", "￠", "￡", "￥"
    '$',
    { "\xEF\xBC\x84", "\xE2\x82\xAC", "\xEF\xB9\xA9", "\xEF\xBF\xA0",
      "\xEF\xBF\xA1", "\xEF\xBF\xA5", NULL },
  }, {  // "％", "﹪", "‰", "‱", "㏙", "㏗"
    '%',
    { "\xEF\xBC\x85", "\xEF\xB9\xAA", "\xE2\x80\xB0", "\xE2\x80\xB1",
      "\xE3\x8F\x99", "\xE3\x8F\x97", NULL },
  }, {  // "＆", "﹠"
    '&',
    { "\xEF\xBC\x86", "\xEF\xB9\xA0", NULL },
  }, {  // "、", "‘", "’"
    '\'',
    { "\xE3\x80\x81", "\xE2\x80\x98", "\xE2\x80\x99", NULL },
  }, {  // "（", "︵", "﹙"
    '(',
    { "\xEF\xBC\x88", "\xEF\xB8\xB5", "\xEF\xB9\x99", NULL },
  }, {  // "）", "︶", "﹚"
    ')',
    { "\xEF\xBC\x89", "\xEF\xB8\xB6", "\xEF\xB9\x9A", NULL },
  }, {  // "＊", "×", "※", "╳", "﹡", "⁎", "⁑", "⁂", "⌘"
    '*',
    { "\xEF\xBC\x8A", "\xC3\x97",     "\xE2\x80\xBB", "\xE2\x95\xB3",
      "\xEF\xB9\xA1", "\xE2\x81\x8E", "\xE2\x81\x91", "\xE2\x81\x82",
      "\xE2\x8C\x98", NULL },
  }, {  // "＋", "±", "﹢"
    '+',
    { "\xEF\xBC\x8B", "\xC2\xB1",    "\xEF\xB9\xA2", NULL },
  }, {  // "，", "、", "﹐", "﹑"
    ',',
    { "\xEF\xBC\x8C", "\xE3\x80\x81", "\xEF\xB9\x90", "\xEF\xB9\x91", NULL },
  }, {  // "…", "—", "－", "¯", "﹉", "￣", "﹊", "ˍ", "–", "‥"
    '-',
    { "\xE2\x80\xA6", "\xE2\x80\x94", "\xEF\xBC\x8D", "\xC2\xAF",
      "\xEF\xB9\x89", "\xEF\xBF\xA3", "\xEF\xB9\x8A", "\xCB\x8D",
      "\xE2\x80\x93", "\xE2\x80\xA5", NULL },
  }, {  // "。", "·", "‧", "﹒", "．"
    '.',
    { "\xE3\x80\x82", "\xC2\xB7",     "\xE2\x80\xA7", "\xEF\xB9\x92",
      "\xEF\xBC\x8E", NULL },
  }, {  // "／", "÷", "↗", "↙", "∕"
    '/',
    { "\xEF\xBC\x8F", "\xC3\xB7",     "\xE2\x86\x97", "\xE2\x86\x99",
      "\xE2\x88\x95", NULL },
  }, {  // "０", "0"
    '0',
    { "\xEF\xBC\x90", "0", NULL },
  }, {  // "１", "1"
    '1',
    { "\xEF\xBC\x91", "1", NULL },
  }, {  // "２", "2"
    '2',
    { "\xEF\xBC\x92", "2", NULL },
  }, {  // "３", "3"
    '3',
    { "\xEF\xBC\x93", "3", NULL },
  }, {  // "４", "4"
    '4',
    { "\xEF\xBC\x94", "4", NULL },
  }, {  // "５", "5"
    '5',
    { "\xEF\xBC\x95", "5", NULL },
  }, {  // "６", "6"
    '6',
    { "\xEF\xBC\x96", "6", NULL },
  }, {  // "７", "7"
    '7',
    { "\xEF\xBC\x97", "7", NULL },
  }, {  // "８", "8"
    '8',
    { "\xEF\xBC\x98", "8", NULL },
  }, {  // "９", "9"
    '9',
    { "\xEF\xBC\x99", "9", NULL },
  }, {  // "：", "︰", "﹕"
    ':',
    { "\xEF\xBC\x9A", "\xEF\xB8\xB0", "\xEF\xB9\x95", NULL },
  }, {  // "；", "﹔"
    ';',
    { "\xEF\xBC\x9B", "\xEF\xB9\x94", NULL },
  }, {  // "＜", "〈", "《", "︽", "︿", "﹤"
    '<',
    { "\xEF\xBC\x9C", "\xE3\x80\x88", "\xE3\x80\x8A", "\xEF\xB8\xBD",
      "\xEF\xB8\xBF", "\xEF\xB9\xA4", NULL },
  }, {  // "＝", "≒", "≠", "≡", "≦", "≧", "﹦"
    '=',
    { "\xEF\xBC\x9D", "\xE2\x89\x92", "\xE2\x89\xA0", "\xE2\x89\xA1",
      "\xE2\x89\xA6", "\xE2\x89\xA7", "\xEF\xB9\xA6", NULL },
  }, {  // "＞", "〉", "》", "︾", "﹀", "﹥"
    '>',
    { "\xEF\xBC\x9E", "\xE3\x80\x89", "\xE3\x80\x8B", "\xEF\xB8\xBE",
      "\xEF\xB9\x80", "\xEF\xB9\xA5", NULL },
  }, {  // "？", "﹖", "⁇", "⁈"
    '?',
    { "\xEF\xBC\x9F", "\xEF\xB9\x96", "\xE2\x81\x87", "\xE2\x81\x88", NULL },
  }, {  // "＠", "⊕", "⊙", "㊣", "﹫", "◉", "◎"
    '@',
    { "\xEF\xBC\xA0", "\xE2\x8A\x95", "\xE2\x8A\x99", "\xE3\x8A\xA3",
      "\xEF\xB9\xAB", "\xE2\x97\x89", "\xE2\x97\x8E", NULL },
  }, {  // "Ａ", "A"
    'A',
    { "\xEF\xBC\xA1", "A", NULL },
  }, {  // "Ｂ", "B"
    'B',
    { "\xEF\xBC\xA2", "B", NULL },
  }, {  // "Ｃ", "C"
    'C',
    { "\xEF\xBC\xA3", "C", NULL },
  }, {  // "Ｄ", "D"
    'D',
    { "\xEF\xBC\xA4", "D", NULL },
  }, {  // "Ｅ", "E"
    'E',
    { "\xEF\xBC\xA5", "E", NULL },
  }, {  // "Ｆ", "F"
    'F',
    { "\xEF\xBC\xA6", "F", NULL },
  }, {  // "Ｇ", "G"
    'G',
    { "\xEF\xBC\xA7", "G", NULL },
  }, {  // "Ｈ", "H"
    'H',
    { "\xEF\xBC\xA8", "H", NULL },
  }, {  // "Ｉ", "I"
    'I',
    { "\xEF\xBC\xA9", "I", NULL },
  }, {  // "Ｊ", "J"
    'J',
    { "\xEF\xBC\xAA", "J", NULL },
  }, {  // "Ｋ", "K"
    'K',
    { "\xEF\xBC\xAB", "K", NULL },
  }, {  // "Ｌ", "L"
    'L',
    { "\xEF\xBC\xAC", "L", NULL },
  }, {  // "Ｍ", "M"
    'M',
    { "\xEF\xBC\xAD", "M", NULL },
  }, {  // "Ｎ", "N"
    'N',
    { "\xEF\xBC\xAE", "N", NULL },
  }, {  // "Ｏ", "O"
    'O',
    { "\xEF\xBC\xAF", "O", NULL },
  }, {  // "Ｐ", "P"
    'P',
    { "\xEF\xBC\xB0", "P", NULL },
  }, {  // "Ｑ", "Q"
    'Q',
    { "\xEF\xBC\xB1", "Q", NULL },
  }, {  // "Ｒ", "R"
    'R',
    { "\xEF\xBC\xB2", "R", NULL },
  }, {  // "Ｓ", "S"
    'S',
    { "\xEF\xBC\xB3", "S", NULL },
  }, {  // "Ｔ", "T"
    'T',
    { "\xEF\xBC\xB4", "T", NULL },
  }, {  // "Ｕ", "U"
    'U',
    { "\xEF\xBC\xB5", "U", NULL },
  }, {  // "Ｖ", "V"
    'V',
    { "\xEF\xBC\xB6", "V", NULL },
  }, {  // "Ｗ", "W"
    'W',
    { "\xEF\xBC\xB7", "W", NULL },
  }, {  // "Ｘ", "X"
    'X',
    { "\xEF\xBC\xB8", "X", NULL },
  }, {  // "Ｙ", "Y"
    'Y',
    { "\xEF\xBC\xB9", "Y", NULL },
  }, {  // "Ｚ", "Z"
    'Z',
    { "\xEF\xBC\xBA", "Z", NULL },
  }, {  // "「", "［", "『", "【", "｢", "︻", "﹁", "﹃"
    '[',
    { "\xE3\x80\x8C", "\xEF\xBC\xBB", "\xE3\x80\x8E", "\xE3\x80\x90",
      "\xEF\xBD\xA2", "\xEF\xB8\xBB", "\xEF\xB9\x81", "\xEF\xB9\x83", NULL },
  }, {  //  "＼", "↖", "↘", "﹨"
    '\\',
    { "\xEF\xBC\xBC", "\xE2\x86\x96", "\xE2\x86\x98", "\xEF\xB9\xA8", NULL },
  }, {  // "」", "］", "』", "】", "｣", "︼", "﹂", "﹄"
    ']',
    { "\xE3\x80\x8D", "\xEF\xBC\xBD", "\xE3\x80\x8F", "\xE3\x80\x91",
      "\xEF\xBD\xA3", "\xEF\xB8\xBC", "\xEF\xB9\x82", "\xEF\xB9\x84", NULL },
  }, {  // "︿", "〈", "《", "︽", "﹤", "＜"
    '^',
    { "\xEF\xB8\xBF", "\xE3\x80\x88", "\xE3\x80\x8A", "\xEF\xB8\xBD",
      "\xEF\xB9\xA4", "\xEF\xBC\x9C", NULL },
  }, {  // "＿", "╴", "←", "→"
    '_',
    { "\xEF\xBC\xBF", "\xE2\x95\xB4", "\xE2\x86\x90", "\xE2\x86\x92", NULL },
  }, {  // "‵", "′"
    '`',
    { "\xE2\x80\xB5", "\xE2\x80\xB2", NULL },
  }, {  // "ａ", "a"
    'a',
    { "\xEF\xBD\x81", "a", NULL },
  }, {  // "ｂ", "b"
    'b',
    { "\xEF\xBD\x82", "b", NULL },
  }, {  // "ｃ", "c"
    'c',
    { "\xEF\xBD\x83", "c", NULL },
  }, {  // "ｄ", "d"
    'd',
    { "\xEF\xBD\x84", "d", NULL },
  }, {  // "ｅ", "e"
    'e',
    { "\xEF\xBD\x85", "e", NULL },
  }, {  // "ｆ", "f"
    'f',
    { "\xEF\xBD\x86", "f", NULL },
  }, {  // "ｇ", "g"
    'g',
    { "\xEF\xBD\x87", "g", NULL },
  }, {  // "ｈ", "h"
    'h',
    { "\xEF\xBD\x88", "h", NULL },
  }, {  // "ｉ", "i"
    'i',
    { "\xEF\xBD\x89", "i", NULL },
  }, {  // "ｊ", "j"
    'j',
    { "\xEF\xBD\x8A", "j", NULL },
  }, {  // "ｋ", "k"
    'k',
    { "\xEF\xBD\x8B", "k", NULL },
  }, {  // "ｌ", "l"
    'l',
    { "\xEF\xBD\x8C", "l", NULL },
  }, {  // "ｍ", "m"
    'm',
    { "\xEF\xBD\x8D", "m", NULL },
  }, {  // "ｎ", "n"
    'n',
    { "\xEF\xBD\x8E", "n", NULL },
  }, {  // "ｏ", "o"
    'o',
    { "\xEF\xBD\x8F", "o", NULL },
  }, {  // "ｐ", "p"
    'p',
    { "\xEF\xBD\x90", "p", NULL },
  }, {  // "ｑ", "q"
    'q',
    { "\xEF\xBD\x91", "q", NULL },
  }, {  // "ｒ", "r"
    'r',
    { "\xEF\xBD\x92", "r", NULL },
  }, {  // "ｓ", "s"
    's',
    { "\xEF\xBD\x93", "s", NULL },
  }, {  // "ｔ", "t"
    't',
    { "\xEF\xBD\x94", "t", NULL },
  }, {  // "ｕ", "u"
    'u',
    { "\xEF\xBD\x95", "u", NULL },
  }, {  // "ｖ", "v"
    'v',
    { "\xEF\xBD\x96", "v", NULL },
  }, {  // "ｗ", "w"
    'w',
    { "\xEF\xBD\x97", "w", NULL },
  }, {  // "ｘ", "x"
    'x',
    { "\xEF\xBD\x98", "x", NULL },
  }, {  // "ｙ", "y"
    'y',
    { "\xEF\xBD\x99", "y", NULL },
  }, {  // "ｚ", "z"
    'z',
    { "\xEF\xBD\x9A", "z", NULL },
  }, {  // "｛", "︷", "﹛", "〔", "﹝", "︹"
    '{',
    { "\xEF\xBD\x9B", "\xEF\xB8\xB7", "\xEF\xB9\x9B", "\xE3\x80\x94",
      "\xEF\xB9\x9D", "\xEF\xB8\xB9", NULL },
  }, {  // "｜", "↑", "↓", "∣", "∥", "︱", "︳", "︴", "￤"
    '|',
    { "\xEF\xBD\x9C", "\xE2\x86\x91", "\xE2\x86\x93", "\xE2\x88\xA3",
      "\xE2\x88\xA5", "\xEF\xB8\xB1", "\xEF\xB8\xB3", "\xEF\xB8\xB4",
      "\xEF\xBF\xA4", NULL },
  }, {  // "｝", "︸", "﹜", "〕", "﹞", "︺"
    '}',
    { "\xEF\xBD\x9D", "\xEF\xB8\xB8", "\xEF\xB9\x9C", "\xE3\x80\x95",
      "\xEF\xB9\x9E", "\xEF\xB8\xBA", NULL },
  }, {  // "～", "﹋", "﹌"
    '~',
    { "\xEF\xBD\x9E", "\xEF\xB9\x8B", "\xEF\xB9\x8C", NULL },
  },
};

const struct PunctuationDirectCommitTable {
 public:
  const char key;
  const char *value;
} kPunctuationDirectCommitTable[] = {
  { '!', "\xEF\xBC\x81" },  // "！"
  // TODO(hsumita): Consider a previously committed character.
  // We should alternately insert "“" and "”".
  { '"', "\xE2\x80\x9C" },  // "“"
  { '$', "\xEF\xBF\xA5" },  // "￥"
  // TODO(hsumita): Consider a previously committed character.
  // We should alternately insert "‘" and "’".
  { '\'', "\xE2\x80\x98" },  // "‘"
  { '(', "\xEF\xBC\x88" },  // "（"
  { ')', "\xEF\xBC\x89" },  // "）"
  { ',', "\xEF\xBC\x8C" },  // "，"
  // TODO(hsumita): Consider a previously committed character.
  // We should insert "." after a number character.
  { '.', "\xE3\x80\x82" },  // "。"
  { ':', "\xEF\xBC\x9A" },  // "："
  { ';', "\xEF\xBC\x9B" },  // "；"
  { '<', "\xE3\x80\x8A" },  // "《"
  { '>', "\xE3\x80\x8B" },  // "》"
  { '?', "\xEF\xBC\x9F" },  // "？"
  { '[', "\xE3\x80\x90" },  // "【"
  { '\\', "\xE3\x80\x81" },  // "、"
  { ']', "\xE3\x80\x91" },  // "】"
  { '^', "\xE2\x80\xA6\xE2\x80\xA6" },  // "……"
  { '_', "\xE2\x80\x94\xE2\x80\x94" },  // "——"
  { '{', "\xE3\x80\x8E" },  // "『"
  { '}', "\xE3\x80\x8F" },  // "』"
  { '~', "\xEF\xBD\x9E" },  // "～"
};

}  // namespace

PunctuationTable::PunctuationTable() {
  const size_t default_candidates_table_size =
      ARRAYSIZE(kPunctuationDefaultCandidatesTable);
  const char **table = kPunctuationDefaultCandidatesTable;
  default_candidates_.assign(table, table + default_candidates_table_size);

  const size_t candidates_table_size = ARRAYSIZE(kPunctuationCandidatesTable);
  for (size_t i = 0; i < candidates_table_size; ++i) {
    const PunctuationCandidatesTable &data = kPunctuationCandidatesTable[i];

    size_t candidates_num = 0;
    for (size_t j = 0; j < ARRAYSIZE(data.candidates); ++j) {
      if (data.candidates[j] == NULL) {
        candidates_num = j;
        break;
      }
    }
    DCHECK_NE(0, candidates_num);

    const vector<string> table(data.candidates,
                               data.candidates + candidates_num);
    conversion_map_.insert(make_pair(data.key, table));
  }

  const size_t commit_table_size =
      ARRAYSIZE(kPunctuationDirectCommitTable);
  for (size_t i = 0; i < commit_table_size; ++i) {
    const PunctuationDirectCommitTable &item =
        kPunctuationDirectCommitTable[i];
    commit_map_.insert(make_pair(item.key, item.value));
  }
}

PunctuationTable::~PunctuationTable() {
}

bool PunctuationTable::GetCandidates(char key,
                                     vector<string> *candidates) const {
  DCHECK(candidates);

  const map<char, vector<string> >::const_iterator iter =
      conversion_map_.find(key);
  if (iter == conversion_map_.end()) {
    DLOG(ERROR) << "Can't find candidates for " << key;
    return false;
  }
  candidates->assign(iter->second.begin(), iter->second.end());

  return true;
}

void PunctuationTable::GetDefaultCandidates(vector<string> *candidates) const {
  DCHECK(candidates);
  candidates->assign(default_candidates_.begin(), default_candidates_.end());
}

bool PunctuationTable::GetDirectCommitTextForSimplifiedChinese(
    char key, string *commit_text) const {
  DCHECK(commit_text);
  return GetDirectCommitTextInternal(key, commit_text);
}

bool PunctuationTable::GetDirectCommitTextForTraditionalChinese(
    char key, string *commit_text) const {
  DCHECK(commit_text);

  switch (key) {
    case '<':
      commit_text->assign("\xEF\xBC\x8C");  // "，"
      return true;
    case '>':
      commit_text->assign("\xE3\x80\x82");  // "。"
      return true;
    case '[':
      commit_text->assign("\xE3\x80\x8C");  // "「"
      return true;
    case ']':
      commit_text->assign("\xE3\x80\x8D");  // "」"
      return true;
    default:
      return GetDirectCommitTextInternal(key, commit_text);
  }
};

bool PunctuationTable::GetDirectCommitTextInternal(
    char key, string *commit_text) const {
  DCHECK(commit_text);

  map<char, string>::const_iterator iter = commit_map_.find(key);
  if (iter == commit_map_.end()) {
    return false;
  }
  commit_text->assign(iter->second);
  return true;
}

}  // namespace punctuation
}  // namespace pinyin
}  // namespace mozc
