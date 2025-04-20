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

// Date comment style are following.
//  - If the input number converts strictly 2 character with padding, comment
//  format is like "HH" or "MM".
//   e.g.) "YYYY/MM/DD HH:MM" ->  "2011/01/30 03:20"
//  - If the input number converts string without padding, comment format is
//  like "H" or "M"
//   e.g.) "Y/M/D H:M" -> "645/2/3 9:2"

#include "rewriter/date_rewriter.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/time/civil_time.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "base/japanese_util.h"
#include "base/number_util.h"
#include "base/strings/unicode.h"
#include "base/util.h"
#include "base/vlog.h"
#include "composer/composer.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {
using ::mozc::date_rewriter_internal::DateCandidate;

enum {
  YEAR,
  MONTH,
  DATE,
  WEEKDAY,
  CURRENT_TIME,
  DATE_AND_CURRENT_TIME,
};

constexpr DateRewriter::DateData kDateData[] = {
    // Date rewrites.
    {"きょう", "今日", "今日の日付", 0, DATE},
    {"あした", "明日", "明日の日付", 1, DATE},
    {"あす", "明日", "明日の日付", 1, DATE},
    {"さくじつ", "昨日", "昨日の日付", -1, DATE},
    {"きのう", "昨日", "昨日の日付", -1, DATE},
    {"おととい", "一昨日", "2日前の日付", -2, DATE},
    {"おとつい", "一昨日", "2日前の日付", -2, DATE},
    {"いっさくじつ", "一昨日", "2日前の日付", -2, DATE},
    {"さきおととい", "一昨昨日", "3日前の日付", -3, DATE},
    {"あさって", "明後日", "明後日の日付", 2, DATE},
    {"みょうごにち", "明後日", "明後日の日付", 2, DATE},
    {"しあさって", "明明後日", "明明後日の日付", 3, DATE},

    // Weekday rewrites
    // Absl::Weekday starts from Monday, while std::tm.tm_wday starts from
    // Sunday.
    {"げつようび", "月曜日", "次の月曜日", 0, WEEKDAY},
    {"かようび", "火曜日", "次の火曜日", 1, WEEKDAY},
    {"すいようび", "水曜日", "次の水曜日", 2, WEEKDAY},
    {"もくようび", "木曜日", "次の木曜日", 3, WEEKDAY},
    {"きんようび", "金曜日", "次の金曜日", 4, WEEKDAY},
    {"どようび", "土曜日", "次の土曜日", 5, WEEKDAY},
    {"にちようび", "日曜日", "次の日曜日", 6, WEEKDAY},
    {"げつよう", "月曜", "次の月曜日", 0, WEEKDAY},
    {"かよう", "火曜", "次の火曜日", 1, WEEKDAY},
    {"すいよう", "水曜", "次の水曜日", 2, WEEKDAY},
    {"もくよう", "木曜", "次の木曜日", 3, WEEKDAY},
    {"きんよう", "金曜", "次の金曜日", 4, WEEKDAY},
    {"どよう", "土曜", "次の土曜日", 5, WEEKDAY},
    {"にちよう", "日曜", "次の日曜日", 6, WEEKDAY},

    // Year rewrites
    {"ことし", "今年", "今年", 0, YEAR},
    {"らいねん", "来年", "来年", 1, YEAR},
    {"さくねん", "昨年", "昨年", -1, YEAR},
    {"きょねん", "去年", "去年", -1, YEAR},
    {"おととし", "一昨年", "一昨年", -2, YEAR},
    {"さらいねん", "再来年", "再来年", 2, YEAR},

    // Month rewrites
    {"こんげつ", "今月", "今月", 0, MONTH},
    {"らいげつ", "来月", "来月", 1, MONTH},
    {"せんげつ", "先月", "先月", -1, MONTH},
    {"せんせんげつ", "先々月", "先々月", -2, MONTH},
    {"さらいげつ", "再来月", "再来月", 2, MONTH},

    // Time rewrites.
    {"いま", "今", "現在の時刻", 0, CURRENT_TIME},
    {"じこく", "時刻", "現在の時刻", 0, CURRENT_TIME},

    // Date and time rewrites.
    {"にちじ", "日時", "現在の日時", 0, DATE_AND_CURRENT_TIME},
    {"なう", "ナウ", "現在の日時", 0, DATE_AND_CURRENT_TIME}};

// Absl::Weekday starts from Monday, while std::tm.tm_wday starts from Sunday.
constexpr absl::string_view kWeekDayString[] = {"月", "火", "水", "木",
                                                "金", "土", "日"};

constexpr absl::string_view kDateDescription = "日付";
constexpr absl::string_view kTimeDescription = "時刻";

struct YearData {
  int ad;                 // AD year
  absl::string_view era;  // Japanese year a.k.a., GENGO
  absl::string_view key;  // reading of the `era`
};

constexpr YearData kEraData[] = {
    // "元徳", "建武" and "明徳" are used for both south and north courts.
    {645, "大化", "たいか"},
    {650, "白雉", "はくち"},
    {686, "朱鳥", "しゅちょう"},
    {701, "大宝", "たいほう"},
    {704, "慶雲", "けいうん"},
    {708, "和銅", "わどう"},
    {715, "霊亀", "れいき"},
    {717, "養老", "ようろう"},
    {724, "神亀", "じんき"},
    {729, "天平", "てんぴょう"},
    {749, "天平感宝", "てんぴょうかんぽう"},
    {749, "天平勝宝", "てんぴょうしょうほう"},
    {757, "天平宝字", "てんぴょうほうじ"},
    {765, "天平神護", "てんぴょうじんご"},
    {767, "神護景雲", "じんごけいうん"},
    {770, "宝亀", "ほうき"},
    {781, "天応", "てんおう"},
    {782, "延暦", "えんりゃく"},
    {806, "大同", "たいどう"},
    {810, "弘仁", "こうにん"},
    {824, "天長", "てんちょう"},
    {834, "承和", "じょうわ"},
    {848, "嘉祥", "かしょう"},
    {851, "仁寿", "にんじゅ"},
    {854, "斉衡", "さいこう"},
    {857, "天安", "てんなん"},
    {859, "貞観", "じょうかん"},
    {877, "元慶", "がんぎょう"},
    {885, "仁和", "にんな"},
    {889, "寛平", "かんぴょう"},
    {898, "昌泰", "しょうたい"},
    {901, "延喜", "えんぎ"},
    {923, "延長", "えんちょう"},
    {931, "承平", "じょうへい"},
    {938, "天慶", "てんぎょう"},
    {947, "天暦", "てんりゃく"},
    {957, "天徳", "てんとく"},
    {961, "応和", "おうわ"},
    {964, "康保", "こうほう"},
    {968, "安和", "あんな"},
    {970, "天禄", "てんろく"},
    {973, "天延", "てんえん"},
    {976, "貞元", "じょうげん"},
    {978, "天元", "てんげん"},
    {983, "永観", "えいかん"},
    {985, "寛和", "かんな"},
    {987, "永延", "えいえん"},
    {989, "永祚", "えいそ"},
    {990, "正暦", "しょうりゃく"},
    {995, "長徳", "ちょうとく"},
    {999, "長保", "ちょうほう"},
    {1004, "寛弘", "かんこう"},
    {1012, "長和", "ちょうわ"},
    {1017, "寛仁", "かんにん"},
    {1021, "治安", "じあん"},
    {1024, "万寿", "まんじゅ"},
    {1028, "長元", "ちょうげん"},
    {1037, "長暦", "ちょうりゃく"},
    {1040, "長久", "ちょうきゅう"},
    {1044, "寛徳", "かんとく"},
    {1046, "永承", "えいしょう"},
    {1053, "天喜", "てんき"},
    {1058, "康平", "こうへい"},
    {1065, "治暦", "じりゃく"},
    {1069, "延久", "えんきゅう"},
    {1074, "承保", "じょうほう"},
    {1077, "承暦", "じょうりゃく"},
    {1081, "永保", "えいほ"},
    {1084, "応徳", "おうとく"},
    {1087, "寛治", "かんじ"},
    {1094, "嘉保", "かほう"},
    {1096, "永長", "えいちょう"},
    {1097, "承徳", "じょうとく"},
    {1099, "康和", "こうわ"},
    {1104, "長治", "ちょうじ"},
    {1106, "嘉承", "かしょう"},
    {1108, "天仁", "てんにん"},
    {1110, "天永", "てんえい"},
    {1113, "永久", "えいきゅう"},
    {1118, "元永", "げんえい"},
    {1120, "保安", "ほうあん"},
    {1124, "天治", "てんじ"},
    {1126, "大治", "だいじ"},
    {1131, "天承", "てんじょう"},
    {1132, "長承", "ちょうじょう"},
    {1135, "保延", "ほうえん"},
    {1141, "永治", "えいじ"},
    {1142, "康治", "こうじ"},
    {1144, "天養", "てんよう"},
    {1145, "久安", "きゅうあん"},
    {1151, "仁平", "にんぺい"},
    {1154, "久寿", "きゅうじゅ"},
    {1156, "保元", "ほうげん"},
    {1159, "平治", "へいじ"},
    {1160, "永暦", "えいりゃく"},
    {1161, "応保", "おうほ"},
    {1163, "長寛", "ちょうかん"},
    {1165, "永万", "えいまん"},
    {1166, "仁安", "にんあん"},
    {1169, "嘉応", "かおう"},
    {1171, "承安", "しょうあん"},
    {1175, "安元", "あんげん"},
    {1177, "治承", "じしょう"},
    {1181, "養和", "ようわ"},
    {1182, "寿永", "じゅえい"},
    {1184, "元暦", "げんりゃく"},
    {1185, "文治", "ぶんじ"},
    {1190, "建久", "けんきゅう"},
    {1199, "正治", "しょうじ"},
    {1201, "建仁", "けんにん"},
    {1204, "元久", "げんきゅう"},
    {1206, "建永", "けんえい"},
    {1207, "承元", "じょうげん"},
    {1211, "建暦", "けんりゃく"},
    {1213, "建保", "けんぽう"},
    {1219, "承久", "しょうきゅう"},
    {1222, "貞応", "じょうおう"},
    {1224, "元仁", "げんにん"},
    {1225, "嘉禄", "かろく"},
    {1227, "安貞", "あんてい"},
    {1229, "寛喜", "かんき"},
    {1232, "貞永", "じょうえい"},
    {1233, "天福", "てんぷく"},
    {1234, "文暦", "ぶんりゃく"},
    {1235, "嘉禎", "かてい"},
    {1238, "暦仁", "りゃくにん"},
    {1239, "延応", "えんおう"},
    {1240, "仁治", "にんじゅ"},
    {1243, "寛元", "かんげん"},
    {1247, "宝治", "ほうじ"},
    {1249, "建長", "けんちょう"},
    {1256, "康元", "こうげん"},
    {1257, "正嘉", "しょうか"},
    {1259, "正元", "しょうげん"},
    {1260, "文応", "ぶんおう"},
    {1261, "弘長", "こうちょう"},
    {1264, "文永", "ぶんえい"},
    {1275, "建治", "けんじ"},
    {1278, "弘安", "こうあん"},
    {1288, "正応", "しょうおう"},
    {1293, "永仁", "えいにん"},
    {1299, "正安", "しょうあん"},
    {1302, "乾元", "けんげん"},
    {1303, "嘉元", "かげん"},
    {1306, "徳治", "とくじ"},
    {1308, "延慶", "えんぎょう"},
    {1311, "応長", "おうちょう"},
    {1312, "正和", "しょうわ"},
    {1317, "文保", "ぶんぽう"},
    {1319, "元応", "げんおう"},
    {1321, "元亨", "げんこう"},
    {1324, "正中", "しょうちゅう"},
    {1326, "嘉暦", "かりゃく"},
    {1329, "元徳", "げんとく"},
    {1331, "元弘", "げんこう"},
    {1334, "建武", "けんむ"},
    {1336, "延元", "えんげん"},
    {1340, "興国", "こうこく"},
    {1346, "正平", "しょうへい"},
    {1370, "建徳", "けんとく"},
    {1372, "文中", "ぶんちゅう"},
    {1375, "天授", "てんじゅ"},
    {1381, "弘和", "こうわ"},
    {1384, "元中", "げんちゅう"},
    {1390, "明徳", "めいとく"},
    {1394, "応永", "おうえい"},
    {1428, "正長", "しょうちょう"},
    {1429, "永享", "えいきょう"},
    {1441, "嘉吉", "かきつ"},
    {1444, "文安", "ぶんあん"},
    {1449, "宝徳", "ほうとく"},
    {1452, "享徳", "きょうとく"},
    {1455, "康正", "こうしょう"},
    {1457, "長禄", "ちょうろく"},
    {1460, "寛正", "かんしょう"},
    {1466, "文正", "ぶんしょう"},
    {1467, "応仁", "おうにん"},
    {1469, "文明", "ぶんめい"},
    {1487, "長享", "ちょうきょう"},
    {1489, "延徳", "えんとく"},
    {1492, "明応", "めいおう"},
    {1501, "文亀", "ぶんき"},
    {1504, "永正", "えいしょう"},
    {1521, "大永", "だいえい"},
    {1528, "享禄", "きょうろく"},
    {1532, "天文", "てんぶん"},
    {1555, "弘治", "こうじ"},
    {1558, "永禄", "えいろく"},
    {1570, "元亀", "げんき"},
    {1573, "天正", "てんしょう"},
    {1592, "文禄", "ぶんろく"},
    {1596, "慶長", "けいちょう"},
    {1615, "元和", "げんな"},
    {1624, "寛永", "かんえい"},
    {1644, "正保", "しょうほう"},
    {1648, "慶安", "けいあん"},
    {1652, "承応", "じょうおう"},
    {1655, "明暦", "めいれき"},
    {1658, "万治", "まんじ"},
    {1661, "寛文", "かんぶん"},
    {1673, "延宝", "えんぽう"},
    {1681, "天和", "てんな"},
    {1684, "貞享", "じょうきょう"},
    {1688, "元禄", "げんろく"},
    {1704, "宝永", "ほうえい"},
    {1711, "正徳", "しょうとく"},
    {1716, "享保", "きょうほ"},
    {1736, "元文", "げんぶん"},
    {1741, "寛保", "かんぽ"},
    {1744, "延享", "えんきょう"},
    {1748, "寛延", "かんえん"},
    {1751, "宝暦", "ほうれき"},
    {1764, "明和", "めいわ"},
    {1772, "安永", "あんえい"},
    {1781, "天明", "てんめい"},
    {1789, "寛政", "かんせい"},
    {1801, "享和", "きょうわ"},
    {1804, "文化", "ぶんか"},
    {1818, "文政", "ぶんせい"},
    {1830, "天保", "てんぽう"},
    {1844, "弘化", "こうか"},
    {1848, "嘉永", "かえい"},
    {1854, "安政", "あんせい"},
    {1860, "万延", "まんえん"},
    {1861, "文久", "ぶんきゅう"},
    {1864, "元治", "げんじ"},
    {1865, "慶応", "けいおう"},
    {1868, "明治", "めいじ"},
    {1912, "大正", "たいしょう"},
    {1926, "昭和", "しょうわ"},
    {1989, "平成", "へいせい"},
    {2019, "令和", "れいわ"}};

constexpr YearData kNorthEraData[] = {
    // clang-format off
    // "元徳", "建武" and "明徳" are used for both south and north courts.
    {1329, "元徳", "げんとく"},
    {1332, "正慶", "しょうけい"},
    {1334, "建武", "けんむ"},
    {1338, "暦応", "りゃくおう"},
    {1342, "康永", "こうえい"},
    {1345, "貞和", "じょうわ"},
    {1350, "観応", "かんおう"},
    {1352, "文和", "ぶんわ"},
    {1356, "延文", "えんぶん"},
    {1361, "康安", "こうあん"},
    {1362, "貞治", "じょうじ"},
    {1368, "応安", "おうあん"},
    {1375, "永和", "えいわ"},
    {1379, "康暦", "こうりゃく"},
    {1381, "永徳", "えいとく"},
    {1384, "至徳", "しとく"},
    {1387, "嘉慶", "かけい"},
    {1389, "康応", "こうおう"},
    {1390, "明徳", "めいとく"},
    // clang-format on
};

bool PrintUint32(const char *format, uint32_t num, char *buf, size_t buf_size) {
  const int ret = std::snprintf(buf, buf_size, format, num);
  return 0 <= ret && ret < buf_size;
}

// Helper function to generate "H時M分" time formats.
void GenerateKanjiTimeFormats(const char *hour_format, const char *min_format,
                              uint32_t hour, uint32_t min,
                              std::vector<DateCandidate> *results) {
  char hour_s[4], min_s[4];
  if (!PrintUint32(hour_format, hour, hour_s, 4) ||
      !PrintUint32(min_format, min, min_s, 4)) {
    return;
  }
  results->emplace_back(absl::StrFormat("%s時%s分", hour_s, min_s),
                        kTimeDescription);
  // "H時半".  Don't generate it when the printed hour starts with 0 because
  // formats like "03時半" is rarely used (but "3時半" is ok).
  if (hour_s[0] != '0' && min == 30) {
    results->emplace_back(absl::StrFormat("%s時半", hour_s), kTimeDescription);
  }
}

// Helper function to generate "午前..." and "午後..." time formats.
void GenerateGozenGogoTimeFormats(const char *hour_format,
                                  const char *min_format, uint32_t hour,
                                  uint32_t min,
                                  std::vector<DateCandidate> *results) {
  // "午前" and "午後" prefixes are only used for [0, 11].
  if (hour >= 12) {
    return;
  }
  char hour_s[4], min_s[4];
  if (!PrintUint32(hour_format, hour, hour_s, 4) ||
      !PrintUint32(min_format, min, min_s, 4)) {
    return;
  }
  results->emplace_back(absl::StrFormat("午前%s時%s分", hour_s, min_s),
                        kTimeDescription);
  if (min == 30) {
    results->emplace_back(absl::StrFormat("午前%s時半", hour_s),
                          kTimeDescription);
  }

  results->emplace_back(absl::StrFormat("午後%s時%s分", hour_s, min_s),
                        kTimeDescription);
  if (min == 30) {
    results->emplace_back(absl::StrFormat("午後%s時半", hour_s),
                          kTimeDescription);
  }
}

// Converts a prefix and year number to Japanese Kanji Representation
// arguments :
//      - input "prefix" : a japanese style year counter prefix.
//      - input "year" : represent integer must be smaller than 100.
//      - output "result" : will be stored candidate strings
// If the input year is invalid ( accept only [ 1 - 99 ] ) , this function
// returns false and clear output vector.
bool ExpandYear(const absl::string_view prefix, int year,
                std::vector<std::string> *result) {
  DCHECK(result);
  if (year <= 0 || year >= 100) {
    result->clear();
    return false;
  }

  if (year == 1) {
    //  "元年"
    result->push_back(absl::StrCat(prefix, "元"));
    return true;
  }

  result->push_back(absl::StrCat(prefix, year));

  std::string arabic = std::to_string(year);

  std::vector<NumberUtil::NumberString> output;

  NumberUtil::ArabicToKanji(arabic, &output);

  for (int i = 0; i < output.size(); i++) {
    if (output[i].style == NumberUtil::NumberString::NUMBER_KANJI) {
      result->push_back(absl::StrCat(prefix, output[i].value));
    }
  }

  return true;
}

std::unique_ptr<Segment::Candidate> CreateCandidate(
    const Segment::Candidate &base_candidate, std::string value,
    std::string description) {
  auto candidate = std::make_unique<Segment::Candidate>();
  candidate->lid = base_candidate.lid;
  candidate->rid = base_candidate.rid;
  candidate->cost = base_candidate.cost;
  candidate->value = std::move(value);
  candidate->key = base_candidate.key;
  candidate->content_key = base_candidate.content_key;
  candidate->attributes |= (Segment::Candidate::NO_LEARNING |
                            Segment::Candidate::NO_VARIANTS_EXPANSION);
  candidate->category = Segment::Candidate::OTHER;
  candidate->description = std::move(description);
  return candidate;
}

bool AdToEraForCourt(const YearData *data, int size, int year,
                     std::vector<std::string> *results) {
  for (int i = size - 1; i >= 0; --i) {
    if (i == size - 1 && year > data[i].ad) {
      ExpandYear(data[i].era, year - data[i].ad + 1, results);
      return true;
    } else if (i > 0 && data[i - 1].ad < year && year <= data[i].ad) {
      // have two representations:
      // 1989 -> "昭和64" and "平成元"
      if (year == data[i].ad) {
        ExpandYear(data[i].era, 1, results);
      }
      ExpandYear(data[i - 1].era, year - data[i - 1].ad + 1, results);
      return true;
    } else if (i == 0 && data[i].ad <= year) {
      if (year == data[i].ad) {
        ExpandYear(data[i].era, 1, results);
      } else {
        ExpandYear(data[i - 1].era, year - data[i - 1].ad + 1, results);
      }
      return true;
    }
  }

  return false;
}

constexpr absl::string_view kNenKey = "ねん";
constexpr absl::string_view kNenValue = "年";

std::string GetDescription(absl::string_view era, absl::string_view year,
                           bool should_add_suffix) {
  return should_add_suffix ? absl::StrCat(era, year, kNenValue)
                           : absl::StrCat(era, year);
}

bool ExtractYearFromKey(const YearData &year_data, const absl::string_view key,
                        int *year, bool *has_suffix_out,
                        std::string *description) {
  constexpr absl::string_view kGanKey = "がん";
  constexpr absl::string_view kGanValue = "元";

  if (!key.starts_with(year_data.key)) {
    return false;
  }

  // Append the `kNenValue` if the `key` has the `kNenKey` suffix.
  const bool has_suffix = key.ends_with(kNenKey);
  *has_suffix_out = has_suffix;

  // key="しょうわ59ねん" -> era_year_str="59"
  // key="へいせいがんねん" -> era_year_str="がん"
  const size_t year_start = Util::CharsLen(year_data.key);
  const size_t year_length = Util::CharsLen(key) - year_start -
                             (has_suffix ? Util::CharsLen(kNenKey) : 0);
  const absl::string_view era_year_str =
      Util::Utf8SubString(key, year_start, year_length);

  if (era_year_str == kGanKey) {
    *year = 1;
    *description = GetDescription(year_data.era, kGanValue, has_suffix);
    return true;
  }

  if (!NumberUtil::IsArabicNumber(era_year_str)) {
    return false;
  }
  *year = NumberUtil::SimpleAtoi(era_year_str);
  if (*year <= 0) {
    return false;
  }
  *description = GetDescription(year_data.era, era_year_str, has_suffix);

  return true;
}

void EraToAdForCourt(const YearData *data, size_t size,
                     const absl::string_view key,
                     std::vector<std::pair<std::string, std::string>>
                         &results_and_descriptions) {
  for (size_t i = 0; i < size; ++i) {
    const YearData &year_data = data[i];
    if (!key.starts_with(year_data.key)) {
      continue;
    }

    // key="しょうわ59ねん" -> era_year=59, description="昭和59年"
    // key="へいせいがんねん" -> era_year=1, description="平成元年"
    int era_year = 0;
    bool has_suffix = false;
    std::string description;
    if (!ExtractYearFromKey(year_data, key, &era_year, &has_suffix,
                            &description)) {
      continue;
    }
    const int ad_year = era_year + year_data.ad - 1;

    // Get wide arabic numbers
    // e.g.) 1989 -> "１９８９", "一九八九"
    std::vector<NumberUtil::NumberString> output;
    const std::string ad_year_str(std::to_string(ad_year));
    NumberUtil::ArabicToWideArabic(ad_year_str, &output);
    // add half-width arabic number to `output` (e.g. "1989")
    output.push_back(NumberUtil::NumberString(
        ad_year_str, "", NumberUtil::NumberString::DEFAULT_STYLE));

    for (size_t j = 0; j < output.size(); ++j) {
      // "元徳", "建武" and "明徳" require dedupe
      std::string value = output[j].value;
      if (has_suffix) {
        value.append(kNenValue);
      }
      if (absl::c_find_if(results_and_descriptions,
                          [&value](const std::pair<std::string, std::string>
                                       &result_and_description) {
                            return result_and_description.first == value;
                          }) != results_and_descriptions.end()) {
        continue;
      }
      results_and_descriptions.push_back({std::move(value), description});
    }
  }
}

// Checks if the given date is valid or not.
// Over 24 hour expression is allowed in this function.
// Acceptable hour is between 0 and 29.
bool IsValidTime(uint32_t hour, uint32_t minute) {
  return hour < 30 && minute < 60;
}

// Returns February last day.
// This function deals with leap year with Gregorian calendar.
uint32_t GetFebruaryLastDay(uint32_t year) {
  uint32_t february_end = (year % 4 == 0) ? 29 : 28;
  if (year % 100 == 0 && year % 400 != 0) {
    february_end = 28;
  }
  return february_end;
}

// Checks given date is valid or not.
bool IsValidDate(uint32_t year, uint32_t month, uint32_t day) {
  if (day < 1) {
    return false;
  }

  if (year == 0 || year > 2100) {
    return false;
  }

  switch (month) {
    case 2: {
      if (day > GetFebruaryLastDay(year)) {
        return false;
      } else {
        return true;
      }
    }
    case 4:
    case 6:
    case 9:
    case 11: {
      if (day > 30) {
        return false;
      } else {
        return true;
      }
    }
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12: {
      if (day > 31) {
        return false;
      } else {
        return true;
      }
    }
    default: {
      return false;
    }
  }
}

// Checks if a pair of month and day is valid.
bool IsValidMonthAndDay(uint32_t month, uint32_t day) {
  if (day == 0) {
    return false;
  }
  switch (month) {
    case 2:
      return day <= 29;
    case 4:
    case 6:
    case 9:
    case 11:
      return day <= 30;
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      return day <= 31;
    default:
      return false;
  }
}

}  // namespace

// convert AD to Japanese ERA.
// The results will have multiple variants.
bool DateRewriter::AdToEra(int year, int month,
                           std::vector<std::string> *results) {
  if (year < 645) {
    return false;
  }

  // This block should be kept until 2020
  // to change the year depending on the month.
  if (year == 2019) {
    if (month == 0) {
      // When month is not specified, show both.
      // Shows 令和 first.
      ExpandYear("令和", 1, results);
      ExpandYear("平成", 31, results);
    } else if (month <= 4) {
      ExpandYear("平成", 31, results);
    } else {
      ExpandYear("令和", 1, results);
    }
    return true;
  }

  if (year > 2200) {
    return false;
  }

  // The order is south to north.
  std::vector<std::string> eras;
  bool r = false;
  r = AdToEraForCourt(kEraData, std::size(kEraData), year, &eras);
  if (year > 1331 && year < 1393) {
    r |= AdToEraForCourt(kNorthEraData, std::size(kNorthEraData), year, &eras);
  }
  // 1334 requires dedupe
  for (int i = 0; i < eras.size(); ++i) {
    bool found = false;
    for (int j = 0; j < i; ++j) {
      if (eras[j] == eras[i]) {
        found = true;
      }
    }
    if (!found) {
      results->push_back(eras[i]);
    }
  }
  return r;
}

std::vector<std::string> DateRewriter::AdToEra(int year, int month) {
  std::vector<std::string> results;
  if (!AdToEra(year, month, &results)) {
    results.clear();
  }
  return results;
}

std::vector<std::pair<std::string, std::string>> DateRewriter::EraToAd(
    const absl::string_view key) {
  std::vector<std::pair<std::string, std::string>> results_and_descriptions;
  // The order is south to north, older to newer
  EraToAdForCourt(kEraData, std::size(kEraData), key, results_and_descriptions);
  EraToAdForCourt(kNorthEraData, std::size(kNorthEraData), key,
                  results_and_descriptions);
  return results_and_descriptions;
}

bool DateRewriter::ConvertTime(uint32_t hour, uint32_t min,
                               std::vector<std::string> *results) {
  DCHECK(results);
  if (!IsValidTime(hour, min)) {
    return false;
  }
  results->push_back(absl::StrFormat("%d:%2.2d", hour, min));
  results->push_back(absl::StrFormat("%d時%2.2d分", hour, min));
  if (min == 30) {
    results->push_back(absl::StrFormat("%d時半", hour));
  }

  if ((hour % 24) * 60 + min < 720) {  // 0:00 -- 11:59
    results->push_back(absl::StrFormat("午前%d時%d分", hour % 24, min));
    if (min == 30) {
      results->push_back(absl::StrFormat("午前%d時半", hour % 24));
    }
  } else {
    results->push_back(absl::StrFormat("午後%d時%d分", (hour - 12) % 24, min));
    if (min == 30) {
      results->push_back(absl::StrFormat("午後%d時半", (hour - 12) % 24));
    }
  }
  return true;
}

std::vector<std::string> DateRewriter::ConvertTime(uint32_t hour,
                                                   uint32_t min) {
  std::vector<std::string> results;
  if (!ConvertTime(hour, min, &results)) {
    results.clear();
  }
  return results;
}

bool DateRewriter::ConvertDateWithYear(uint32_t year, uint32_t month,
                                       uint32_t day,
                                       std::vector<std::string> *results) {
  DCHECK(results);
  if (!IsValidDate(year, month, day)) {
    return false;
  }
  // Generate "Y/MM/DD", "Y-MM-DD" and "Y年M月D日" formats.
  results->push_back(absl::StrFormat("%d/%2.2d/%2.2d", year, month, day));
  results->push_back(absl::StrFormat("%d-%2.2d-%2.2d", year, month, day));
  results->push_back(absl::StrFormat("%d年%d月%d日", year, month, day));
  return true;
}

std::vector<std::string> DateRewriter::ConvertDateWithYear(uint32_t year,
                                                           uint32_t month,
                                                           uint32_t day) {
  std::vector<std::string> results;
  if (!ConvertDateWithYear(year, month, day, &results)) {
    results.clear();
  }
  return results;
}

namespace {
absl::CivilMinute GetCivilMinuteWithDiff(int type, int diff) {
  const absl::Time at = Clock::GetAbslTime();
  const absl::TimeZone &tz = Clock::GetTimeZone();

  if (type == DATE) {
    const absl::CivilDay c_day = absl::ToCivilDay(at, tz) + diff;
    return absl::CivilMinute(c_day);
  }
  if (type == MONTH) {
    const absl::CivilMonth c_mon = absl::ToCivilMonth(at, tz) + diff;
    return absl::CivilMinute(c_mon);
  }
  if (type == YEAR) {
    const absl::CivilYear c_year = absl::ToCivilYear(at, tz) + diff;
    return absl::CivilMinute(c_year);
  }
  if (type == WEEKDAY) {
    const absl::CivilDay c_day = absl::ToCivilDay(at, tz);
    const int weekday = static_cast<int>(absl::GetWeekday(c_day));
    const int weekday_diff = (diff + 7 - weekday) % 7;
    return c_day + weekday_diff;
  }

  return absl::ToCivilMinute(at, tz);
}

std::vector<std::string> GetConversions(const DateRewriter::DateData &data,
                                        const absl::string_view extra_format) {
  std::vector<std::string> results;
  const absl::CivilMinute cm = GetCivilMinuteWithDiff(data.type, data.diff);

  if (!extra_format.empty()) {
    const absl::TimeZone &tz = Clock::GetTimeZone();
    const absl::Time at = absl::FromCivil(cm, tz);
    results.push_back(absl::FormatTime(extra_format, at, tz));
  }

  switch (data.type) {
    case DATE:
    case WEEKDAY: {
      DateRewriter::ConvertDateWithYear(cm.year(), cm.month(), cm.day(),
                                        &results);
      std::vector<std::string> era;
      if (DateRewriter::AdToEra(cm.year(), cm.month(), &era) && !era.empty()) {
        results.push_back(
            absl::StrFormat("%s年%d月%d日", era[0], cm.month(), cm.day()));
      }
      const int weekday = static_cast<int>(absl::GetWeekday(cm));
      results.push_back(absl::StrFormat("%s曜日", kWeekDayString[weekday]));
      break;
    }

    case MONTH: {
      results.push_back(absl::StrFormat("%d", cm.month()));
      results.push_back(absl::StrFormat("%d月", cm.month()));
      break;
    }

    case YEAR: {
      results.push_back(absl::StrFormat("%d", cm.year()));
      results.push_back(absl::StrFormat("%d年", cm.year()));

      std::vector<std::string> era;
      if (DateRewriter::AdToEra(cm.year(), 0, /* unknown month */ &era) &&
          !era.empty()) {
        for (auto rit = era.crbegin(); rit != era.crend(); ++rit) {
          results.push_back(absl::StrFormat("%s年", *rit));
        }
      }
      break;
    }

    case CURRENT_TIME: {
      DateRewriter::ConvertTime(cm.hour(), cm.minute(), &results);
      break;
    }

    case DATE_AND_CURRENT_TIME: {
      // Y/MM/DD H:MM
      results.push_back(absl::StrFormat("%d/%2.2d/%2.2d %2d:%2.2d", cm.year(),
                                        cm.month(), cm.day(), cm.hour(),
                                        cm.minute()));
      break;
    }

    default: {
      // Unknown type
    }
  }

  return results;
}
}  // namespace

bool DateRewriter::RewriteDate(Segment *segment,
                               const absl::string_view extra_format,
                               size_t &num_done_out) {
  absl::string_view key = segment->key();
  auto rit = std::find_if(std::begin(kDateData), std::end(kDateData),
                          [&key](auto data) { return key == data.key; });
  if (rit == std::end(kDateData)) {
    return false;
  }

  const DateData &data = *rit;
  std::vector<std::string> conversions = GetConversions(data, extra_format);
  if (conversions.empty()) {
    return false;
  }

  // Calculate insert_idx
  // Candidates will be inserted less than 10th candidate.
  constexpr size_t kMaxIdx = 10;
  const size_t end_idx = std::min(kMaxIdx, segment->candidates_size());
  size_t cand_idx = 0;
  for (cand_idx = 0; cand_idx < end_idx; ++cand_idx) {
    if (segment->candidate(cand_idx).value == data.value) {
      break;
    }
  }
  if (cand_idx == end_idx) {
    return false;
  }

  // Insert words.
  const Segment::Candidate &base_cand = segment->candidate(cand_idx);
  std::vector<std::unique_ptr<Segment::Candidate>> candidates;
  candidates.reserve(conversions.size());
  for (std::string &conversion : conversions) {
    candidates.push_back(CreateCandidate(base_cand, std::move(conversion),
                                         std::string(data.description)));
  }

  // Date candidates are too many, therefore highest candidate show at most 3rd.
  // TODO(nona): learn date candidate even if the date is changed.
  const size_t min_idx = std::min<size_t>(3, end_idx);
  const size_t insert_idx = std::clamp(cand_idx + 1, min_idx, end_idx);
  segment->insert_candidates(insert_idx, std::move(candidates));
  num_done_out = 1;
  return true;
}

bool DateRewriter::RewriteEra(Segments::range segments_range,
                              size_t &num_done_out) {
  // Rewrite:
  // * If the first segment ends with the `kNenKey`, or
  // * If the second segment starts with the `kNenKey`.
  Segment &segment = segments_range.front();
  absl::string_view key = segment.key();
  const bool has_suffix = key.ends_with(kNenKey);
  if (has_suffix) {
    key.remove_suffix(kNenKey.size());
  } else if (segments_range.size() < 2 ||
             !segments_range[1].key().starts_with(kNenKey)) {
    return false;
  }

  if (Util::GetScriptType(key) != Util::NUMBER) {
    return false;
  }

  const size_t len = Util::CharsLen(key);
  if (len < 3 || len > 4) {
    LOG(WARNING) << "Too long year";
    return false;
  }

  std::string year_str = japanese_util::FullWidthAsciiToHalfWidthAscii(key);

  uint32_t year = 0;
  if (!absl::SimpleAtoi(year_str, &year)) {
    return false;
  }

  std::vector<std::string> results;
  if (!AdToEra(year, 0, /* unknown month */ &results)) {
    return false;
  }

  constexpr absl::string_view kDescription = "和暦";
  const Segment::Candidate &base_cand = segment.candidate(0);
  std::vector<std::unique_ptr<Segment::Candidate>> candidates;
  candidates.reserve(results.size());
  for (std::string &value : results) {
    if (has_suffix) {
      value.append(kNenValue);
    }
    std::unique_ptr<Segment::Candidate> candidate =
        CreateCandidate(base_cand, std::move(value), std::string(kDescription));
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
    candidates.push_back(std::move(candidate));
  }

  constexpr int kInsertPosition = 2;
  segment.insert_candidates(kInsertPosition, std::move(candidates));

  num_done_out = has_suffix ? 1 : 2;
  return true;
}

bool DateRewriter::RewriteAd(Segments::range segments_range,
                             size_t &num_done_out) {
  // Rewrite:
  // * If the first segment ends with the `kNenKey`, or
  // * If the second segment starts with the `kNenKey`.
  Segment *segment = &segments_range.front();
  absl::string_view key = segment->key();
  const bool has_suffix = key.ends_with(kNenKey);
  if (!has_suffix) {
    if (segments_range.size() < 2 ||
        !segments_range[1].key().starts_with(kNenKey)) {
      return false;
    }
  }
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "No candidates are found";
    return false;
  }

  // Try to convert era to AD.
  const std::vector<std::pair<std::string, std::string>>
      results_anddescriptions = EraToAd(key);
  if (results_anddescriptions.empty()) {
    return false;
  }

  const Segment::Candidate &base_cand = segment->candidate(0);
  std::vector<std::unique_ptr<Segment::Candidate>> candidates;
  candidates.reserve(results_anddescriptions.size());
  for (auto &[result, description] : results_anddescriptions) {
    candidates.push_back(
        CreateCandidate(base_cand, std::move(result), std::move(description)));
  }

  // Insert position is the last of candidates
  const int position = static_cast<int>(segment->candidates_size());
  segment->insert_candidates(position, std::move(candidates));
  num_done_out = has_suffix ? 1 : 2;
  return true;
}

// This function changes the default conversion behavior. For example, when the
// input is "taishou2nen", it is converted to 3 segments by default without this
// function, but this function merges them to 1 segment. Users can still resize
// segments to get the behavior without this function, and the engine learns the
// resize for the same string next time, but there will be cases where the
// default conversion looks degraded from what users expect.
//
// Supporting multiple segments without resizing has benefits for users, such as
// they can still see other candidates of the era segment. But unlike
// `RewriteEra` which supports multiple segments without merging, this function
// needs to produce a candidate for 2 segments (the era and the digits), which
// isn't easy.
std::optional<RewriterInterface::ResizeSegmentsRequest>
DateRewriter::CheckResizeSegmentsForAd(const ConversionRequest &request,
                                       const Segments &segments,
                                       const size_t segment_index) const {
  // Find the first segment that ends with `kNenKey`.
  constexpr size_t kMaxSegments = 3;  // Only up to 3 segments.
  bool has_suffix = false;
  bool should_resize_last_segment = false;
  std::vector<absl::string_view> keys;
  for (const Segment &segment :
       segments.conversion_segments().drop(segment_index)) {
    const absl::string_view key{segment.key()};
    if (auto pos = key.find(kNenKey); pos != absl::string_view::npos) {
      if (pos == 0 && keys.size() == 1) {
        // If the second key starts with the `kNenKey`, `RewriteAd()` can handle
        // it without resizing.
        return std::nullopt;
      }
      pos += kNenKey.size();
      if (pos == key.size()) {
        keys.push_back(key);
      } else {
        // The segment has `kNenKey` and following characters; e.g., "nendesu".
        keys.push_back(key.substr(0, pos));
        should_resize_last_segment = true;
      }
      has_suffix = true;
      break;
    }
    if (keys.size() >= kMaxSegments - 1) {
      return std::nullopt;
    }
    keys.push_back(key);
  }
  if (!has_suffix || (keys.size() <= 1 && !should_resize_last_segment)) {
    return std::nullopt;
  }
  const std::string key = absl::StrJoin(keys, "");
  DCHECK(!key.empty());
  const size_t key_len = Util::CharsLen(key);
  if (key_len > std::numeric_limits<uint8_t>::max()) {
    return std::nullopt;
  }
  const uint8_t segment_size = static_cast<uint8_t>(key_len);

  // Try to convert era to AD.
  const std::vector<std::pair<std::string, std::string>>
      results_anddescriptions = EraToAd(key);
  if (results_anddescriptions.empty()) {
    return std::nullopt;
  }

  ResizeSegmentsRequest resize_request = {
      .segment_index = segment_index,
      .segment_sizes = {segment_size, 0, 0, 0, 0, 0, 0, 0},
  };
  return resize_request;
}

namespace {
std::optional<std::string> VaridateNDigits(absl::string_view value, int n) {
  static_assert(U'9' - U'0' == 9);
  static_assert(U'９' - U'０' == 9);
  std::u32string u32value = strings::Utf8ToUtf32(value);
  if (u32value.size() != n) {
    return std::nullopt;
  }

  for (int i = 0; i < n; ++i) {
    const char32_t c = u32value[i];
    if (U'0' <= c && c <= U'9') {  // Half-width digits
      continue;
    }
    if (U'０' <= c && c <= U'９') {  // Full-width digits
      // Sets `u32value[i]` to the half-width digts of `c`.
      u32value[i] = (c - U'０' + U'0');
      continue;
    }
    return std::nullopt;
  }
  return strings::Utf32ToUtf8(u32value);
}

// Gets n digits if possible.
// Following trials will be performed in this order.
// 1. Checks segment's key.
// 2. Checks all the meta candidates.
// 3. Checks raw input.
//      This is mainly for mobile.
//      On 12keys-toggle-alphabet mode, a user types "2223" to get "cd".
//      In this case,
//      - Segment's key is "cd".
//      - All the meta candidates are based on "cd" (e.g. "CD", "Cd").
//      Therefore to get "2223" we should access the raw input.
// Prerequisite: |segments| has only one conversion segment.
std::optional<std::string> GetNDigits(const composer::ComposerData &composer,
                                      const Segments &segments, int n) {
  DCHECK_EQ(segments.conversion_segments_size(), 1);
  const Segment &segment = segments.conversion_segment(0);
  std::optional<std::string> validated;

  // 1. Segment's key
  if (validated = VaridateNDigits(segment.key(), n); validated) {
    return validated.value();
  }

  // 2. Meta candidates
  for (size_t i = 0; i < segment.meta_candidates_size(); ++i) {
    if (validated = VaridateNDigits(segment.meta_candidate(i).value, n);
        validated) {
      return validated.value();
    }
  }

  // 3. Raw input
  // Note that only one segment is in the Segments, but sometimes like
  // on partial conversion, segment.key() is different from the size of
  // the whole composition.
  const std::string raw = composer.GetRawSubString(0, segment.key_len());
  if (validated = VaridateNDigits(raw, n); validated) {
    return validated.value();
  }

  // No trials succeeded.
  return std::nullopt;
}
}  // namespace

bool DateRewriter::RewriteConsecutiveDigits(
    const composer::ComposerData &composer, int insert_position,
    Segments *segments) {
  if (segments->conversion_segments_size() != 1) {
    // This method rewrites a segment only when the segments has only
    // one conversion segment.
    // This is spec matter.
    // Rewriting multiple segments will not make users happier.
    return false;
  }
  Segment *segment = segments->mutable_conversion_segment(0);

  // segment->candidate(0) or segment->meta_candidate(0) is used as reference.
  // Check the existence before generating candidates to save time.
  if (segment->candidates_size() == 0 && segment->meta_candidates_size() == 0) {
    MOZC_VLOG(2) << "No (meta) candidates are found";
    return false;
  }

  // Generate candidates.  The results contain <candidate, description> pairs.
  std::optional<std::string> number_str;
  std::vector<DateCandidate> results;
  if (number_str = GetNDigits(composer, *segments, 2); number_str) {
    if (!RewriteConsecutiveTwoDigits(number_str.value(), &results)) {
      return false;
    }
  } else if (number_str = GetNDigits(composer, *segments, 3); number_str) {
    if (!RewriteConsecutiveThreeDigits(number_str.value(), &results)) {
      return false;
    }
  } else if (number_str = GetNDigits(composer, *segments, 4); number_str) {
    if (!RewriteConsecutiveFourDigits(number_str.value(), &results)) {
      return false;
    }
  }
  if (results.empty()) {
    return false;
  }

  // The existence of segment->candidate(0) or segment->meta_candidate(0) is
  // guaranteed at the above check.
  const Segment::Candidate &top_cand = (segment->candidates_size() > 0)
                                           ? segment->candidate(0)
                                           : segment->meta_candidate(0);
  std::vector<std::unique_ptr<Segment::Candidate>> candidates;
  candidates.reserve(results.size());
  for (DateCandidate &result : results) {
    candidates.push_back(CreateCandidate(top_cand, std::move(result.candidate),
                                         std::string(result.description)));
  }

  if (insert_position < 0) {
    insert_position = static_cast<int>(segment->candidates_size());
  }
  segment->insert_candidates(insert_position, std::move(candidates));
  return true;
}

bool DateRewriter::RewriteConsecutiveTwoDigits(
    absl::string_view str, std::vector<DateCandidate> *results) {
  DCHECK_EQ(str.size(), 2);
  const auto orig_size = results->size();
  const uint32_t high = static_cast<uint32_t>(str[0] - '0');
  const uint32_t low = static_cast<uint32_t>(str[1] - '0');
  if (IsValidMonthAndDay(high, low)) {
    results->emplace_back(absl::StrFormat("%c/%c", str[0], str[1]),
                          kDateDescription);
    results->emplace_back(absl::StrFormat("%c月%c日", str[0], str[1]),
                          kDateDescription);
  }
  if (IsValidTime(high, low)) {
    // "H時M分".
    GenerateKanjiTimeFormats("%d", "%d", high, low, results);
    // "午前H時M分".
    GenerateGozenGogoTimeFormats("%d", "%d", high, low, results);
  }
  return results->size() > orig_size;
}

bool DateRewriter::RewriteConsecutiveThreeDigits(
    absl::string_view str, std::vector<DateCandidate> *results) {
  DCHECK_EQ(str.size(), 3);
  const auto orig_size = results->size();

  const uint32_t n[] = {static_cast<uint32_t>(str[0] - '0'),
                        static_cast<uint32_t>(str[1] - '0'),
                        static_cast<uint32_t>(str[2] - '0')};

  // Split pattern 1: N|NN
  const uint32_t high1 = n[0];
  const uint32_t low1 = 10 * n[1] + n[2];
  const bool is_valid_date1 = IsValidMonthAndDay(high1, low1) && str[1] != '0';
  const bool is_valid_time1 = IsValidTime(high1, low1);

  // Split pattern 2: NN|N
  const uint32_t high2 = 10 * n[0] + n[1];
  const uint32_t low2 = n[2];
  const bool is_valid_date2 = IsValidMonthAndDay(high2, low2) && str[0] != '0';
  const bool is_valid_time2 = IsValidTime(high2, low2) && str[0] != '0';

  if (is_valid_date1) {
    // "M/DD"
    results->emplace_back(absl::StrFormat("%c/%c%c", str[0], str[1], str[2]),
                          kDateDescription);
  }
  if (is_valid_date2) {
    // "MM/D"
    results->emplace_back(absl::StrFormat("%c%c/%c", str[0], str[1], str[2]),
                          kDateDescription);
  }
  if (is_valid_time1) {
    // "H:MM"
    results->emplace_back(absl::StrFormat("%c:%c%c", str[0], str[1], str[2]),
                          kTimeDescription);
  }
  // Don't generate HH:M form as it is unusual.

  if (is_valid_date1) {
    // "M月DD日".
    results->emplace_back(absl::StrFormat("%c月%c%c日", str[0], str[1], str[2]),
                          kDateDescription);
  }
  if (is_valid_date2) {
    // "MM月D日"
    results->emplace_back(absl::StrFormat("%c%c月%c日", str[0], str[1], str[2]),
                          kDateDescription);
  }
  if (is_valid_time1) {
    // "M時DD分" etc.
    GenerateKanjiTimeFormats("%d", "%02d", high1, low1, results);
  }
  if (is_valid_time2) {
    // "MM時D分" etc.
    GenerateKanjiTimeFormats("%d", "%d", high2, low2, results);
  }
  if (is_valid_time1) {
    // "午前M時DD分" etc.
    GenerateGozenGogoTimeFormats("%d", "%02d", high1, low1, results);
  }
  if (is_valid_time2) {
    // "午前MM時D分" etc.
    GenerateGozenGogoTimeFormats("%d", "%d", high2, low2, results);
  }

  return results->size() > orig_size;
}

bool DateRewriter::RewriteConsecutiveFourDigits(
    absl::string_view str, std::vector<DateCandidate> *results) {
  DCHECK_EQ(str.size(), 4);
  const auto orig_size = results->size();

  const uint32_t high = (10 * static_cast<uint32_t>(str[0] - '0') +
                         static_cast<uint32_t>(str[1] - '0'));
  const uint32_t low = (10 * static_cast<uint32_t>(str[2] - '0') +
                        static_cast<uint32_t>(str[3] - '0'));

  const bool is_valid_date = IsValidMonthAndDay(high, low);
  const bool is_valid_time = IsValidTime(high, low);

  if (is_valid_date) {
    // "MM/DD"
    results->emplace_back(
        absl::StrFormat("%c%c/%c%c", str[0], str[1], str[2], str[3]),
        kDateDescription);
  }
  if (is_valid_time) {
    // "MM:DD"
    results->emplace_back(
        absl::StrFormat("%c%c:%c%c", str[0], str[1], str[2], str[3]),
        kTimeDescription);
  }
  if (is_valid_date && str[0] != '0' && str[2] != '0') {
    // "MM月DD日".  Don't generate this form if there is a leading zero in
    // month or day because it's rarely written like "01月01日".  Don't
    // generate "1月1日" too, as we shouldn't remove the zero explicitly added
    // by user.
    results->emplace_back(
        absl::StrFormat("%c%c月%c%c日", str[0], str[1], str[2], str[3]),
        kDateDescription);
  }
  if (is_valid_time) {
    // "MM時DD分" etc.
    GenerateKanjiTimeFormats("%02d", "%02d", high, low, results);
    if (high >= 10) {
      // "午前MM時DD分" etc.
      GenerateGozenGogoTimeFormats("%d", "%02d", high, low, results);
    }
  }

  return results->size() > orig_size;
}

int DateRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

namespace {
std::string ConvertExtraFormat(const absl::string_view base) {
  return absl::StrReplaceAll(base, {{"%", "%%"},
                                    {"{YEAR}", "%Y"},
                                    {"{MONTH}", "%m"},
                                    {"{DATE}", "%d"},
                                    {"{{}", "{"}});
}

std::string GetExtraFormat(const dictionary::DictionaryInterface *dictionary) {
  if (dictionary == nullptr) {
    return "";
  }

  class EntryCollector : public dictionary::DictionaryInterface::Callback {
   public:
    explicit EntryCollector(std::string *token) : token_(token) {}
    ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                       const dictionary::Token &token) override {
      if (token.attributes != dictionary::Token::USER_DICTIONARY) {
        return TRAVERSE_CONTINUE;
      }
      *token_ = token.value;
      return TRAVERSE_DONE;
    }
    std::string *token_;
  };

  std::string extra_format;

  ConversionRequest crequest;
  EntryCollector callback(&extra_format);
  dictionary->LookupExact(DateRewriter::kExtraFormatKey, crequest, &callback);

  return ConvertExtraFormat(extra_format);
}
}  // namespace

std::optional<RewriterInterface::ResizeSegmentsRequest>
DateRewriter::CheckResizeSegmentsRequest(const ConversionRequest &request,
                                         const Segments &segments) const {
  if (!request.config().use_date_conversion()) {
    MOZC_VLOG(2) << "no use_date_conversion";
    return std::nullopt;
  }

  if (segments.resized()) {
    // If the given segments are resized by user, don't modify anymore.
    return std::nullopt;
  }

  for (size_t segment_index = 0;
       segment_index < segments.conversion_segments_size(); ++segment_index) {
    std::optional<RewriterInterface::ResizeSegmentsRequest> resize_request =
        CheckResizeSegmentsForAd(request, segments, segment_index);
    if (resize_request.has_value()) {
      return resize_request;
    }
  }

  return std::nullopt;
}

bool DateRewriter::Rewrite(const ConversionRequest &request,
                           Segments *segments) const {
  if (!request.config().use_date_conversion()) {
    MOZC_VLOG(2) << "no use_date_conversion";
    return false;
  }

  const Segments::range conversion_segments = segments->conversion_segments();
  if (conversion_segments.empty()) {
    return false;
  }

  bool modified = false;
  const std::string extra_format = GetExtraFormat(dictionary_);
  size_t num_done = 1;
  for (Segments::range rest_segments = conversion_segments;
       !rest_segments.empty(); rest_segments = rest_segments.drop(num_done)) {
    Segment *seg = &rest_segments.front();
    if (seg == nullptr) {
      LOG(ERROR) << "Segment is nullptr";
      return false;
    }

    if (RewriteAd(rest_segments, num_done) ||
        RewriteDate(seg, extra_format, num_done) ||
        RewriteEra(rest_segments, num_done)) {
      modified = true;
      continue;
    }

    num_done = 1;
  }

  // Select the insert position by Romaji table.  Note:
  // TOGGLE_FLICK_TO_HIRAGANA uses digits for Hiragana composing, date/time
  // conversion is performed even when typing Hiragana characters.  Thus, it
  // should not be promoted.
  int insert_pos =
      static_cast<int>(conversion_segments.front().candidates_size());
  switch (request.request().special_romanji_table()) {
    case commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII:
    case commands::Request::FLICK_TO_NUMBER:
    case commands::Request::TOGGLE_FLICK_TO_NUMBER:
      insert_pos = 1;
      break;
    default:
      break;
  }
  modified |=
      RewriteConsecutiveDigits(request.composer(), insert_pos, segments);

  return modified;
}

}  // namespace mozc
