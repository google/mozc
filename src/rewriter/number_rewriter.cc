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

#include "rewriter/number_rewriter.h"

#include <stdio.h>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/util.h"
#include "converter/pos_matcher.h"
#include "converter/segments.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

namespace mozc {
namespace {

const char* const kNumKanjiDigits[] = {
  "\xe3\x80\x87", "\xe4\xb8\x80", "\xe4\xba\x8c", "\xe4\xb8\x89",
  "\xe5\x9b\x9b", "\xe4\xba\x94", "\xe5\x85\xad", "\xe4\xb8\x83",
  "\xe5\x85\xab", "\xe4\xb9\x9d", NULL
  //   "〇", "一", "二", "三", "四", "五", "六", "七", "八", "九", NULL
};
const char* const kNumWideDigits[] = {
  "\xef\xbc\x90", "\xef\xbc\x91", "\xef\xbc\x92", "\xef\xbc\x93",
  "\xef\xbc\x94", "\xef\xbc\x95", "\xef\xbc\x96", "\xef\xbc\x97",
  "\xef\xbc\x98", "\xef\xbc\x99", NULL
  //   "０", "１", "２", "３", "４", "５", "６", "７", "８", "９", NULL
};
const char* const kNumHalfDigits[] = {
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", NULL
};

const char* const kNumKanjiOldDigits[] = {
  NULL, "\xe5\xa3\xb1", "\xe5\xbc\x90", "\xe5\x8f\x82", "\xe5\x9b\x9b",
  "\xe4\xba\x94", "\xe5\x85\xad", "\xe4\xb8\x83", "\xe5\x85\xab",
  "\xe4\xb9\x9d"
  //   NULL, "壱", "弐", "参", "四", "五", "六", "七", "八", "九"
};

const char* const kNumKanjiRanks[] = {
  NULL, "", "\xe5\x8d\x81", "\xe7\x99\xbe", "\xe5\x8d\x83"
  //   NULL, "", "十", "百", "千"
};
const char* const kNumKanjiBiggerRanks[] = {
  "", "\xe4\xb8\x87", "\xe5\x84\x84", "\xe5\x85\x86", "\xe4\xba\xac"
  //   "", "万", "億", "兆", "京"
};

const char* const* const kKanjiDigitsVariations[] = {
  kNumKanjiDigits, kNumKanjiOldDigits, NULL
};
const char* const* const kSingleDigitsVariations[] = {
  kNumKanjiDigits, kNumWideDigits, NULL
};
const char* const* const kNumDigitsVariations[] = {
  kNumHalfDigits, kNumWideDigits, NULL
};

const char* kRomanNumbersCapital[] = {
  NULL, "\xe2\x85\xa0", "\xe2\x85\xa1", "\xe2\x85\xa2", "\xe2\x85\xa3",
  "\xe2\x85\xa4", "\xe2\x85\xa5", "\xe2\x85\xa6", "\xe2\x85\xa7",
  "\xe2\x85\xa8", "\xe2\x85\xa9", "\xe2\x85\xaa", "\xe2\x85\xab", NULL
  //   NULL, "Ⅰ", "Ⅱ", "Ⅲ", "Ⅳ", "Ⅴ", "Ⅵ", "Ⅶ", "Ⅷ", "Ⅸ", "Ⅹ", "Ⅺ", "Ⅻ", NULL
};

const char* kRomanNumbersSmall[] = {
  NULL, "\xe2\x85\xb0", "\xe2\x85\xb1", "\xe2\x85\xb2", "\xe2\x85\xb3",
  "\xe2\x85\xb4", "\xe2\x85\xb5", "\xe2\x85\xb6", "\xe2\x85\xb7",
  "\xe2\x85\xb8", "\xe2\x85\xb9", "\xe2\x85\xba", "\xe2\x85\xbb", NULL
  //   NULL, "ⅰ", "ⅱ", "ⅲ", "ⅳ", "ⅴ", "ⅵ", "ⅶ", "ⅷ", "ⅸ", "ⅹ", "ⅺ", "ⅻ", NULL
};

const char* kCircledNumbers[] = {
  NULL, "\xe2\x91\xa0", "\xe2\x91\xa1", "\xe2\x91\xa2", "\xe2\x91\xa3",
  "\xe2\x91\xa4", "\xe2\x91\xa5", "\xe2\x91\xa6", "\xe2\x91\xa7",
  "\xe2\x91\xa8", "\xe2\x91\xa9",
  //   NULL, "①", "②", "③", "④", "⑤", "⑥", "⑦", "⑧", "⑨", "⑩",
  "\xe2\x91\xaa", "\xe2\x91\xab", "\xe2\x91\xac", "\xe2\x91\xad",
  "\xe2\x91\xae", "\xe2\x91\xaf", "\xe2\x91\xb0", "\xe2\x91\xb1",
  "\xe2\x91\xb2", "\xe2\x91\xb3",
  //   "⑪", "⑫", "⑬", "⑭", "⑮", "⑯", "⑰", "⑱", "⑲", "⑳",
  "\xE3\x89\x91", "\xE3\x89\x92", "\xE3\x89\x93", "\xE3\x89\x94",
  "\xE3\x89\x95", "\xE3\x89\x96", "\xE3\x89\x97", "\xE3\x89\x98",
  "\xE3\x89\x99", "\xE3\x89\x9A", "\xE3\x89\x9B", "\xE3\x89\x9C",
  "\xE3\x89\x9D", "\xE3\x89\x9E", "\xE3\x89\x9F",
  // 21-35
  "\xE3\x8A\xB1", "\xE3\x8A\xB2", "\xE3\x8A\xB3", "\xE3\x8A\xB4",
  "\xE3\x8A\xB5", "\xE3\x8A\xB6", "\xE3\x8A\xB7", "\xE3\x8A\xB8",
  "\xE3\x8A\xB9", "\xE3\x8A\xBA", "\xE3\x8A\xBB", "\xE3\x8A\xBC",
  "\xE3\x8A\xBD", "\xE3\x8A\xBE", "\xE3\x8A\xBF",
  //36-50
  NULL
};

const char* const* const kSpecialNumericVariations[] = {
  kRomanNumbersCapital, kRomanNumbersSmall, kCircledNumbers,
  NULL
};
const int kSpecialNumericSizes[] = {
  arraysize(kRomanNumbersCapital),
  arraysize(kRomanNumbersSmall),
  arraysize(kCircledNumbers),
  -1
};

const char* const kNumZero = "\xe9\x9b\xb6";
// const char* const kNumZero = "零";
const char* const kNumOldTen = "\xe6\x8b\xbe";
// const char* const kNumOldTen = "拾";
const char* const kNumOldTwenty = "\xe5\xbb\xbf";
// const char* const kNumOldTwenty = "廿";
const char* const kNumOldThousand = "\xe9\x98\xa1";
// const char* const kNumOldThousand = "阡";
const char* const kNumOldTenThousand = "\xe8\x90\xac";
// const char* const kNumOldTenThousand = "萬";
const char* const kNumGoogol =
"100000000000000000000000000000000000000000000000000"
"00000000000000000000000000000000000000000000000000";

// Helper functions.
void AppendToEachElement(const string& s,
                         vector<pair<string, uint16> >* out) {
  for (vector<pair<string, uint16> >::iterator it = out->begin();
       it != out->end(); ++it) {
    it->first.append(s);
  }
}

void ReplaceElement(const string& before, const string& after,
                    vector<pair<string, uint16> >* texts) {
  for (size_t i = 0; i < texts->size(); ++i) {
    if ((*texts)[i].first.find(before) != string::npos) {
      string replaced = (*texts)[i].first;
      size_t tpos = 0;
      while ((tpos = replaced.find(before, tpos)) != string::npos) {
        replaced.replace(tpos, before.size(), after);
      }
      // ReplaceElement is used for old kanji
      texts->push_back(make_pair(replaced,
                                 Segment::Candidate::NUMBER_OLD_KANJI));
    }
  }
}

void PushBackCandidate(const string& value, const string& desc,
                       uint16 style,
                       vector<Segment::Candidate>* results) {
  bool found = false;
  for (vector<Segment::Candidate>::const_iterator it = results->begin();
       it != results->end(); ++it) {
    if (it->value == value) {
      found = true;
      break;
    }
  }
  if (!found) {
    Segment::Candidate cand;
    cand.value = value;
    cand.description = desc;
    cand.style = style;
    results->push_back(cand);
  }
}

// Number Converters main functions.
// They receives two arguments:
//  - input_num: a string consisting of arabic numeric characters
//  - output: a pointer to a vector of string, which contains the
//    converted number representations.
// If the input_num is invalid or cannot represent as the form, this
// function does nothing.  If finds more than one representations,
// pushes all candidates into the output.
void ArabicToKanji(const string& input_num,
                   vector<Segment::Candidate>* output) {
  for (const char* const* const* digits_ptr = kKanjiDigitsVariations;
       *digits_ptr; digits_ptr++) {
    bool is_old = (*digits_ptr == kNumKanjiOldDigits);
    const char* const* const digits = *digits_ptr;

    const char* input_ptr = input_num.data();
    int input_len = static_cast<int>(input_num.size());
    while (input_len > 0 && *input_ptr == '0') {
      ++input_ptr;
      --input_len;
    }
    if (input_len == 0) {
      // "大字"
      // http://ja.wikipedia.org/wiki/%E5%A4%A7%E5%AD%97_(%E6%95%B0%E5%AD%97)
      PushBackCandidate(kNumZero, "\xE5\xA4\xA7\xE5\xAD\x97",
                        Segment::Candidate::NUMBER_OLD_KANJI, output);
      break;
    }
    int bigger_ranks = input_len / 4;
    if (bigger_ranks * 4 == input_len) {
      --bigger_ranks;
    }
    if (bigger_ranks < static_cast<int>(arraysize(kNumKanjiBiggerRanks))) {
      vector<pair<string, uint16> > results;  // pair of value and type
      const uint16 kStyle = is_old ? Segment::Candidate::NUMBER_OLD_KANJI :
          Segment::Candidate::NUMBER_KANJI;
      results.push_back(make_pair("", kStyle));
      for (; bigger_ranks >= 0; --bigger_ranks) {
        bool is_printed = false;
        int smaller_rank_len = input_len - bigger_ranks * 4;
        for (int i = smaller_rank_len; i > 0; --i, ++input_ptr, --input_len) {
          uint32 n = *input_ptr - '0';
          if (n != 0) {
            is_printed = true;
          }
          if (!is_old && i == 4 && bigger_ranks > 0 &&
              strncmp(input_ptr, "1000", 4) == 0) {
            AppendToEachElement(digits[n], &results);
            AppendToEachElement(kNumKanjiRanks[i], &results);
            input_ptr += 4;
            input_len -= 4;
            break;
          }
          if (n == 1) {
            if (is_old) {
              AppendToEachElement(digits[n], &results);
            } else if (i == 4) {
              if (is_old) {
                AppendToEachElement(digits[n], &results);
              } else {
                const size_t len = results.size();
                for (size_t j = 0; j < len; ++j) {
                  results.push_back(make_pair(results[j].first + digits[n],
                                              kStyle));
                }
              }
            } else if (i == 1) {
              AppendToEachElement(digits[n], &results);
            }
          } else if (n >= 2 && n <= 9) {
            AppendToEachElement(digits[n], &results);
          }
          if (n > 0 && n <= 9) {
            AppendToEachElement(
                (i == 2 && is_old)? kNumOldTen : kNumKanjiRanks[i], &results);
          }
        }
        if (is_printed) {
          AppendToEachElement(kNumKanjiBiggerRanks[bigger_ranks], &results);
        }
      }
      if (is_old) {
        ReplaceElement("\xe5\x8d\x83", kNumOldThousand, &results);
        //         ReplaceElement("千", kNumOldThousand, &results);
        ReplaceElement("\xe5\xbc\x90\xe6\x8b\xbe", kNumOldTwenty, &results);
        //         ReplaceElement("弐拾", kNumOldTwenty, &results);
        ReplaceElement(kNumKanjiBiggerRanks[1], kNumOldTenThousand, &results);
      }
      for (vector<pair<string, uint16> >::const_iterator it = results.begin();
           it != results.end(); ++it) {
        if (it->second == Segment::Candidate::NUMBER_OLD_KANJI) {
          // "大字"
          // http://ja.wikipedia.org/wiki/%E5%A4%A7%E5%AD%97_(%E6%95%B0%E5%AD%97)
          PushBackCandidate(it->first, "\xE5\xA4\xA7\xE5\xAD\x97",
                            it->second, output);
        } else {
          // "漢数字"
          PushBackCandidate(it->first, "\xE6\xBC\xA2\xE6\x95\xB0\xE5\xAD\x97",
                            it->second, output);
        }
      }
    }
  }
}

void ArabicToSeparatedArabic(const string& input_num,
                             vector<Segment::Candidate>* output) {
  if (input_num[0] == '0') {
    // We don't add separator to number starting with '0'
    return;
  }

  const char* kSeparaters[] = {",", "\xef\xbc\x8c", NULL};
  const uint16 kStyles[] = {
    Segment::Candidate::NUMBER_SEPARATED_ARABIC_HALFWIDTH,
    Segment::Candidate::NUMBER_SEPARATED_ARABIC_FULLWIDTH,
    0,
  };

  for (size_t i = 0; kNumDigitsVariations[i] != NULL; ++i) {
    int counter = 2 - ((input_num.size() - 1) % 3);
    string result;
    for (size_t j = 0; j < input_num.size(); ++j) {
      // We don't add separater first
      if (j != 0 && counter % 3 == 0 && kSeparaters[i]) {
        result.append(kSeparaters[i]);
      }
      const uint32 d = input_num[j] - '0';
      if (d <= 9 && kNumDigitsVariations[i][d]) {
        result.append(kNumDigitsVariations[i][d]);
      }
      ++counter;
    }
    // "数字"
    PushBackCandidate(result, "\xE6\x95\xB0\xE5\xAD\x97",
                      kStyles[i], output);
  }
}

void ArabicToWideArabic(const string& input_num,
                        vector<Segment::Candidate>* output) {
  const uint16 kStyles[] = {
    Segment::Candidate::NUMBER_KANJI_ARABIC,
    Segment::Candidate::DEFAULT,
    // use default for wide arabic, because half/full width for
    // normal number is learned by charactor form manager.
    0,
  };

  const char *kStylesName[] = {
    "\xE6\xBC\xA2\xE6\x95\xB0\xE5\xAD\x97",  // "漢数字"
    "\xE6\x95\xB0\xE5\xAD\x97",              // "数字"
    NULL
  };

  for (size_t i = 0; kSingleDigitsVariations[i] != NULL; ++i) {
    string result;
    for (size_t j = 0; j < input_num.size(); ++j) {
      uint32 n = input_num[j] - '0';
      if (n <= 9 && kSingleDigitsVariations[i][n]) {
        result.append(kSingleDigitsVariations[i][n]);
      } else {
        break;
      }
    }
    if (!result.empty()) {
      PushBackCandidate(result, kStylesName[i], kStyles[i], output);
    }
  }
}

void ArabicToOtherForms(const string& input_num,
                        vector<Segment::Candidate>* output) {
  if (input_num == kNumGoogol) {
    PushBackCandidate("Googol", "", Segment::Candidate::DEFAULT, output);
  }
  int32 n = 0;
  for (size_t i = 0; i < input_num.size(); ++i) {
    uint32 d = input_num[i] - '0';
    if (d <= 9) {
      n = n * 10 + input_num[i] - '0';
      if (n > 99) {
        return;
      }
    } else {
      break;
    }
  }

  const uint16 kStyles[] = {
    Segment::Candidate::NUMBER_ROMAN_CAPITAL,
    Segment::Candidate::NUMBER_ROMAN_SMALL,
    Segment::Candidate::NUMBER_CIRCLED,
    0,
  };

  // "ローマ数字(大文字)",
  // "ローマ数字(小文字)",
  // "丸数字"
  const char *kStylesName[] = {
    "\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x9E\xE6\x95\xB0"
    "\xE5\xAD\x97(\xE5\xA4\xA7\xE6\x96\x87\xE5\xAD\x97)",
    "\xE3\x83\xAD\xE3\x83\xBC\xE3\x83\x9E\xE6\x95\xB0"
    "\xE5\xAD\x97(\xE5\xB0\x8F\xE6\x96\x87\xE5\xAD\x97)",
    "\xE4\xB8\xB8\xE6\x95\xB0\xE5\xAD\x97"
  };


  for (int i = 0; kSpecialNumericVariations[i]; ++i) {
    if (n < kSpecialNumericSizes[i] && kSpecialNumericVariations[i][n]) {
      PushBackCandidate(kSpecialNumericVariations[i][n], kStylesName[i],
                        kStyles[i], output);
    }
  }
}

void ArabicToOtherRadixes(const string& input_num,
                          vector<Segment::Candidate>* output) {
  // uint64 size of digits is smaller than 20.
#define MAX_INT64_SIZE 20
  if (input_num.size() >= MAX_INT64_SIZE) {
    return;
  }
  uint64 n = 0;
  for (string::const_iterator i = input_num.begin();
       i != input_num.end(); ++i) {
    n = 10 * n + (*i) - '0';
  }
  if (n > 9) {
    // Hexadecimal
    string hexadecimal("0x");
    char buf[MAX_INT64_SIZE];
    int len = snprintf(buf, MAX_INT64_SIZE, "%llx", n);
    hexadecimal.append(buf, len);
    // 16\xE9\x80\xB2\xE6\x95\xB0 is "16進数"
    PushBackCandidate(hexadecimal, "16\xE9\x80\xB2\xE6\x95\xB0",
                      Segment::Candidate::NUMBER_HEX, output);
  }
  if (n > 1) {
    // octal and binary
    string octal;
    string binary;
    bool put_octal = (n > 7);
    while (n > 0) {
      octal.push_back('0' + static_cast<char>(n & 0x7));
      for (int i = 0; i < 3 && n > 0; ++i) {
        binary.push_back('0' + static_cast<char>(n & 0x1));
        n >>= 1;
      }
    }
    if (put_octal) {
      reverse(octal.begin(), octal.end());
      // 8\xE9\x80\xB2\xE6\x95\xB0 is "8進数"
      PushBackCandidate(string("0") + octal, "8\xE9\x80\xB2\xE6\x95\xB0",
                        Segment::Candidate::NUMBER_OCT, output);
    }
    reverse(binary.begin(), binary.end());
    // 2\xE9\x80\xB2\xE6\x95\xB0 is "2進数"
    PushBackCandidate(string("0b") + binary, "2\xE9\x80\xB2\xE6\x95\xB0",
                      Segment::Candidate::NUMBER_BIN, output);
  }
}

bool IsNumber(uint16 lid) {
  return
      (POSMatcher::IsNumber(lid) ||
       POSMatcher::IsKanjiNumber(lid));
}

// Return true if rewriter should insert numerical variants.
// *base_candidate_pos: candidate index of base_candidate. POS information
// for numerical variants are coped from the base_candidate.
// *insert_pos: the candidate index from which numerical variants
// should be inserted.
bool GetNumericCandidatePositions(Segment *seg,
                                  int *base_candidate_pos, int *insert_pos) {
  CHECK(base_candidate_pos);
  CHECK(insert_pos);
  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    const Segment::Candidate &c = seg->candidate(i);
    if (!IsNumber(c.lid)) {
      continue;
    }

    if (Util::GetScriptType(c.content_value) == Util::NUMBER) {
      *base_candidate_pos = i;
      // +2 as fullwidht/(or halfwidth) variant is on i + 1 postion.
      *insert_pos = i + 2;
      return true;
    }

    string kanji_number, arabic_number, half_width_new_content_value;
    Util::FullWidthToHalfWidth(c.content_key,
                               &half_width_new_content_value);
    // try to get normalized kanji_number and arabic_number.
    // if it failed, do nothing.
    if (!Util::NormalizeNumbers(c.content_value, true,
                                  &kanji_number, &arabic_number) ||
        arabic_number == half_width_new_content_value) {
      return false;
    }

    // Insert arabic number first
    Segment::Candidate *arabic_c = seg->insert_candidate(i + 1);
    DCHECK(arabic_c);
    const string suffix =
        c.value.substr(c.content_value.size(),
                       c.value.size() - c.content_value.size());
    arabic_c->Init();
    arabic_c->value = arabic_number + suffix;
    arabic_c->content_value = arabic_number;
    arabic_c->key = c.key;
    arabic_c->content_key = c.content_key;
    arabic_c->cost = c.cost;
    arabic_c->structure_cost = c.structure_cost;
    arabic_c->lid = c.lid;
    arabic_c->rid = c.rid;
    arabic_c->SetDefaultDescription(
        Segment::Candidate::CHARACTER_FORM |
        Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER);
    seg->ExpandAlternative(i + 1);

    // If top candidate is Kanji numeric, we want to expand at least
    // 5 candidates here.
    // http://b/issue?id=2872048
    const int kArabicNumericOffset = 5;
    *base_candidate_pos = i + 1;
    *insert_pos = i + kArabicNumericOffset;
    return true;
  }

  return false;
 }
}   // namespace

NumberRewriter::NumberRewriter() {}
NumberRewriter::~NumberRewriter() {}

bool NumberRewriter::Rewrite(Segments *segments) const {
  if (!GET_CONFIG(use_number_conversion)) {
    VLOG(2) << "no use_number_conversion";
    return false;
  }

  bool modified = false;
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    int base_candidate_pos = 0;
    int insert_pos = 0;
    if (!GetNumericCandidatePositions(seg,
                                      &base_candidate_pos,
                                      &insert_pos)) {
      continue;
    }

    const Segment::Candidate &base_cand = seg->candidate(base_candidate_pos);

    if (base_cand.content_value.size() > base_cand.value.size()) {
      LOG(ERROR) << "Invalid content_value/value: ";
      continue;
    }

    string base_content_value;
    Util::FullWidthToHalfWidth(base_cand.content_value, &base_content_value);

    if (Util::GetScriptType(base_content_value) != Util::NUMBER) {
      LOG(ERROR) << "base_content_value is not number: " << base_content_value;
      continue;
    }

    seg->GetCandidates(insert_pos);
    insert_pos = min(insert_pos, static_cast<int>(seg->candidates_size()));

    modified = true;
    vector<Segment::Candidate> converted_numbers;
    ArabicToWideArabic(base_content_value, &converted_numbers);
    ArabicToSeparatedArabic(base_content_value, &converted_numbers);
    ArabicToKanji(base_content_value, &converted_numbers);
    ArabicToOtherForms(base_content_value, &converted_numbers);
    if (segments->conversion_segments_size() == 1) {
      ArabicToOtherRadixes(base_content_value, &converted_numbers);
    }

    const string suffix =
        base_cand.value.substr(base_cand.content_value.size(),
                               base_cand.value.size() -
                               base_cand.content_value.size());

    for (vector<Segment::Candidate>::const_iterator iter =
             converted_numbers.begin();
         iter != converted_numbers.end(); ++iter) {
      Segment::Candidate* c = seg->insert_candidate(insert_pos++);
      DCHECK(c);
      c->lid = base_cand.lid;
      c->rid = base_cand.rid;
      c->cost = base_cand.cost;
      c->value = iter->value + suffix;
      c->content_value = iter->value;
      c->key = base_cand.key;
      c->content_key = base_cand.content_key;
      c->style = iter->style;
      c->SetDescription(Segment::Candidate::PLATFORM_DEPENDENT_CHARACTER |
                        Segment::Candidate::CHARACTER_FORM |
                        Segment::Candidate::FULL_HALF_WIDTH,
                        iter->description);
    }
  }

  return modified;
}
}  // namespace mozc
