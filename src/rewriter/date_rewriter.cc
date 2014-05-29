// Copyright 2010-2014, Google Inc.
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
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "session/commands.pb.h"

namespace mozc {

namespace {
struct DateData {
  const char *key;
  const char *value;
  const char *description;
  int diff;   // diff from the current time in day or month or year
};

const struct DateData kDateData[] = {
  {
    // きょう will show today's date
    "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
    "\xE4\xBB\x8A\xE6\x97\xA5",
    "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    0
  }, {
    // あした will show tomorrow's date
    "\xE3\x81\x82\xE3\x81\x97\xE3\x81\x9F",
    "\xE6\x98\x8E\xE6\x97\xA5",
    "\xE6\x98\x8E\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    1
  }, {
    // あす will show tomorrow's date
    "\xE3\x81\x82\xE3\x81\x99",
    "\xE6\x98\x8E\xE6\x97\xA5",
    "\xE6\x98\x8E\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    1
  }, {
    // さくじつ will show yesterday's date
    "\xE3\x81\x95\xE3\x81\x8F\xE3\x81\x98\xE3\x81\xA4",
    "\xE6\x98\xA8\xE6\x97\xA5",
    "\xE6\x98\xA8\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    -1
  }, {
    // きのう will show yesterday's date
    "\xE3\x81\x8D\xE3\x81\xAE\xE3\x81\x86",
    "\xE6\x98\xA8\xE6\x97\xA5",
    "\xE6\x98\xA8\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    -1
  }, {
    // おととい will show the date of 2 days ago
    "\xE3\x81\x8A\xE3\x81\xA8\xE3\x81\xA8\xE3\x81\x84",
    "\xE4\xB8\x80\xE6\x98\xA8\xE6\x97\xA5",
    "2\xE6\x97\xA5\xE5\x89\x8D\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    -2
  }, {
    // おとつい will show the date of 2 days ago
    "\xE3\x81\x8A\xE3\x81\xA8\xE3\x81\xA4\xE3\x81\x84",
    "\xE4\xB8\x80\xE6\x98\xA8\xE6\x97\xA5",
    "2\xE6\x97\xA5\xE5\x89\x8D\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    -2
  }, {
    // いっさくじつ will show the date of 2 days ago
    "\xE3\x81\x84\xE3\x81\xA3\xE3\x81\x95\xE3\x81\x8F\xE3\x81\x98\xE3\x81\xA4",
    "\xE4\xB8\x80\xE6\x98\xA8\xE6\x97\xA5",
    "2\xE6\x97\xA5\xE5\x89\x8D\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    -2
  }, {
    // さきおととい will show the date of 3 days ago
    "\xe3\x81\x95\xe3\x81\x8d\xe3\x81\x8a\xe3\x81\xa8"
    "\xe3\x81\xa8\xe3\x81\x84",
    "\xe4\xb8\x80\xe6\x98\xa8\xe6\x98\xa8\xe6\x97\xa5",
    "3\xE6\x97\xA5\xE5\x89\x8D\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    -3
  }, {
    // あさって will show the date of 2 days from now
    "\xE3\x81\x82\xE3\x81\x95\xE3\x81\xA3\xE3\x81\xA6",
    "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5",
    "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    2,
  }, {
    // みょうごにち will show the date of 2 days from now
    "\xE3\x81\xBF\xE3\x82\x87\xE3\x81\x86"
    "\xE3\x81\x94\xE3\x81\xAB\xE3\x81\xA1",
    "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5",
    "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    2
  }, {
    // しあさって will show the date of 3 days from now
    "\xE3\x81\x97\xE3\x81\x82\xE3\x81\x95"
    "\xE3\x81\xA3\xE3\x81\xA6",
    "\xE6\x98\x8E\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5",
    "\xE6\x98\x8E\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5\xE3\x81\xAE"
    "\xE6\x97\xA5\xE4\xBB\x98",
    3
  }
};

const struct DateData kWeekDayData[] = {
  {
    // "にちようび"
    "\xe3\x81\xab\xe3\x81\xa1\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe6\x97\xa5\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe6\x97\xa5\xe6\x9b\x9c\xe6\x97\xa5",
    0
  }, {
    // "げつようび"
    "\xe3\x81\x92\xe3\x81\xa4\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe6\x9c\x88\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe6\x9c\x88\xe6\x9b\x9c\xe6\x97\xa5",
    1
  }, {
    // "かようび"
    "\xe3\x81\x8b\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe7\x81\xab\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe7\x81\xab\xe6\x9b\x9c\xe6\x97\xa5",
    2
  }, {
    // "すいようび"
    "\xe3\x81\x99\xe3\x81\x84\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe6\xb0\xb4\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe6\xb0\xb4\xe6\x9b\x9c\xe6\x97\xa5",
    3
  }, {
    // "もくようび"
    "\xe3\x82\x82\xe3\x81\x8f\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe6\x9c\xa8\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe6\x9c\xa8\xe6\x9b\x9c\xe6\x97\xa5",
    4
  }, {
    // "きんようび"
    "\xe3\x81\x8d\xe3\x82\x93\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe9\x87\x91\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe9\x87\x91\xe6\x9b\x9c\xe6\x97\xa5",
    5
  }, {
    // "どようび"
    "\xe3\x81\xa9\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe5\x9c\x9f\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe5\x9c\x9f\xe6\x9b\x9c\xe6\x97\xa5",
    6
  }, {
    // "にちよう"
    "\xE3\x81\xAB\xE3\x81\xA1\xE3\x82\x88\xE3\x81\x86",
    "\xE6\x97\xA5\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE6\x97\xA5\xE6\x9B\x9C\xE6\x97\xA5",
    0
  }, {
    // "げつよう"
    "\xE3\x81\x92\xE3\x81\xA4\xE3\x82\x88\xE3\x81\x86",
    "\xE6\x9C\x88\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE6\x9C\x88\xE6\x9B\x9C\xE6\x97\xA5",
    1
  }, {
    // "かよう"
    "\xE3\x81\x8B\xE3\x82\x88\xE3\x81\x86",
    "\xE7\x81\xAB\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE7\x81\xAB\xE6\x9B\x9C\xE6\x97\xA5",
    2
  }, {
    // "すいよう"
    "\xE3\x81\x99\xE3\x81\x84\xE3\x82\x88\xE3\x81\x86",
    "\xE6\xB0\xB4\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE6\xB0\xB4\xE6\x9B\x9C\xE6\x97\xA5",
    3
  }, {
    // "もくよう"
    "\xE3\x82\x82\xE3\x81\x8F\xE3\x82\x88\xE3\x81\x86",
    "\xE6\x9C\xA8\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE6\x9C\xA8\xE6\x9B\x9C\xE6\x97\xA5",
    4
  }, {
    // "きんよう"
    "\xE3\x81\x8D\xE3\x82\x93\xE3\x82\x88\xE3\x81\x86",
    "\xE9\x87\x91\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE9\x87\x91\xE6\x9B\x9C\xE6\x97\xA5",
    5
  }, {
    // "どよう"
    "\xE3\x81\xA9\xE3\x82\x88\xE3\x81\x86",
    "\xE5\x9C\x9F\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE5\x9C\x9F\xE6\x9B\x9C\xE6\x97\xA5",
    6
  }
};

const struct DateData kYearData[] = {
  {
    // "ことし"
    "\xE3\x81\x93\xE3\x81\xA8\xE3\x81\x97",
    "\xE4\xBB\x8A\xE5\xB9\xB4",
    "\xE4\xBB\x8A\xE5\xB9\xB4",
    0
  }, {
    // "らいねん"
    "\xE3\x82\x89\xE3\x81\x84\xE3\x81\xAD\xE3\x82\x93",
    "\xE6\x9D\xA5\xE5\xB9\xB4",
    "\xE6\x9D\xA5\xE5\xB9\xB4",
    1
  }, {
    // "さくねん"
    "\xE3\x81\x95\xE3\x81\x8F\xE3\x81\xAD\xE3\x82\x93",
    "\xE6\x98\xA8\xE5\xB9\xB4",
    "\xE6\x98\xA8\xE5\xB9\xB4",
    -1
  }, {
    // "きょねん"
    "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\xAD\xE3\x82\x93",
    "\xE5\x8E\xBB\xE5\xB9\xB4",
    "\xE5\x8E\xBB\xE5\xB9\xB4",
    -1
  }, {
    // "おととし"
    "\xE3\x81\x8A\xE3\x81\xA8\xE3\x81\xA8\xE3\x81\x97",
    "\xE4\xB8\x80\xE6\x98\xA8\xE5\xB9\xB4",
    "\xE4\xB8\x80\xE6\x98\xA8\xE5\xB9\xB4",
    -2
  }, {
    // "さらいねん"
    "\xE3\x81\x95\xE3\x82\x89\xE3\x81\x84\xE3\x81\xAD\xE3\x82\x93",
    "\xE5\x86\x8D\xE6\x9D\xA5\xE5\xB9\xB4",
    "\xE5\x86\x8D\xE6\x9D\xA5\xE5\xB9\xB4",
    2
  }
};

const struct DateData kMonthData[] = {
  {
    // "こんげつ"
    "\xE3\x81\x93\xE3\x82\x93\xE3\x81\x92\xE3\x81\xA4",
    "\xE4\xBB\x8A\xE6\x9C\x88",
    "\xE4\xBB\x8A\xE6\x9C\x88",
    0
  }, {
    // "らいげつ"
    "\xE3\x82\x89\xE3\x81\x84\xE3\x81\x92\xE3\x81\xA4",
    "\xE6\x9D\xA5\xE6\x9C\x88",
    "\xE6\x9D\xA5\xE6\x9C\x88",
    1
  }, {
    // "せんげつ"
    "\xE3\x81\x9B\xE3\x82\x93\xE3\x81\x92\xE3\x81\xA4",
    "\xE5\x85\x88\xE6\x9C\x88",
    "\xE5\x85\x88\xE6\x9C\x88",
    -1
  }, {
    // "せんせんげつ"
    "\xE3\x81\x9B\xE3\x82\x93\xE3\x81\x9B\xE3\x82\x93\xE3\x81\x92\xE3\x81\xA4",
    "\xE5\x85\x88\xE3\x80\x85\xE6\x9C\x88",
    "\xE5\x85\x88\xE3\x80\x85\xE6\x9C\x88",
    -2
  }, {
    // "さらいげつ"
    "\xE3\x81\x95\xE3\x82\x89\xE3\x81\x84\xE3\x81\x92\xE3\x81\xA4",
    "\xE5\x86\x8D\xE6\x9D\xA5\xE6\x9C\x88",
    "\xE5\x86\x8D\xE6\x9D\xA5\xE6\x9C\x88",
    2
  }
};

const struct DateData kCurrentTimeData[] = {
  {
    // "いま"
    "\xE3\x81\x84\xE3\x81\xBE",
    "\xE4\xBB\x8A",
    "\xE7\x8F\xBE\xE5\x9C\xA8\xE3\x81\xAE\xE6\x99\x82\xE5\x88\xBB",
    0
  }, {
    // "じこく"
    "\xE3\x81\x98\xE3\x81\x93\xE3\x81\x8F",
    "\xE6\x99\x82\xE5\x88\xBB",
    "\xE7\x8F\xBE\xE5\x9C\xA8\xE3\x81\xAE\xE6\x99\x82\xE5\x88\xBB",
    0
  }
};

const struct DateData kDateAndCurrentTimeData[] = {
  {
    // "にちじ"
    "\xE3\x81\xAB\xE3\x81\xA1\xE3\x81\x98",
    "\xE6\x97\xA5\xE6\x99\x82",
    "\xE7\x8F\xBE\xE5\x9C\xA8\xE3\x81\xAE\xE6\x97\xA5\xE6\x99\x82",
    0
  },
};

struct YearData {
  int ad;      // AD year
  const char *era;   // Japanese year a.k.a, GENGO
  const char *key;   // reading of the `era`
};

const YearData kEraData[] = {
  // "元徳", "建武" and "明徳" are used for both south and north courts.
  {
    // "大化" "たいか"
    645,
    "\xE5\xA4\xA7\xE5\x8C\x96",
    "\xE3\x81\x9F\xE3\x81\x84\xE3\x81\x8B",
  }, {
    // "白雉" "はくち"
    650,
    "\xE7\x99\xBD\xE9\x9B\x89",
    "\xE3\x81\xAF\xE3\x81\x8F\xE3\x81\xA1",
  }, {
    // "朱鳥" "しゅちょう"
    686,
    "\xE6\x9C\xB1\xE9\xB3\xA5",
    "\xE3\x81\x97\xE3\x82\x85\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "大宝" "たいほう"
    701,
    "\xE5\xA4\xA7\xE5\xAE\x9D",
    "\xE3\x81\x9F\xE3\x81\x84\xE3\x81\xBB\xE3\x81\x86",
  }, {
    // "慶雲" "けいうん"
    704,
    "\xE6\x85\xB6\xE9\x9B\xB2",
    "\xE3\x81\x91\xE3\x81\x84\xE3\x81\x86\xE3\x82\x93",
  }, {
    // "和銅" "わどう"
    708,
    "\xE5\x92\x8C\xE9\x8A\x85",
    "\xE3\x82\x8F\xE3\x81\xA9\xE3\x81\x86",
  }, {
    // "霊亀" "れいき"
    715,
    "\xE9\x9C\x8A\xE4\xBA\x80",
    "\xE3\x82\x8C\xE3\x81\x84\xE3\x81\x8D",
  }, {
    // "養老" "ようろう"
    717,
    "\xE9\xA4\x8A\xE8\x80\x81",
    "\xE3\x82\x88\xE3\x81\x86\xE3\x82\x8D\xE3\x81\x86",
  }, {
    // "神亀" "じんき"
    724,
    "\xE7\xA5\x9E\xE4\xBA\x80",
    "\xE3\x81\x98\xE3\x82\x93\xE3\x81\x8D",
  }, {
    // "天平" "てんぴょう"
    729,
    "\xE5\xA4\xA9\xE5\xB9\xB3",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xB4\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "天平感宝" "てんぴょうかんぽう"
    749,
    "\xE5\xA4\xA9\xE5\xB9\xB3\xE6\x84\x9F\xE5\xAE\x9D",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xB4\xE3\x82\x87\xE3\x81\x86"
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\xBD\xE3\x81\x86",
  }, {
    // "天平勝宝" "てんぴょうしょうほう"
    749,
    "\xE5\xA4\xA9\xE5\xB9\xB3\xE5\x8B\x9D\xE5\xAE\x9D",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xB4\xE3\x82\x87\xE3\x81\x86"
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\xBB\xE3\x81\x86",
  }, {
    // "天平宝字" "てんぴょうほうじ"
    757,
    "\xE5\xA4\xA9\xE5\xB9\xB3\xE5\xAE\x9D\xE5\xAD\x97",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xB4\xE3\x82\x87\xE3\x81\x86"
    "\xE3\x81\xBB\xE3\x81\x86\xE3\x81\x98",
  }, {
    // "天平神護" "てんぴょうじんご"
    765,
    "\xE5\xA4\xA9\xE5\xB9\xB3\xE7\xA5\x9E\xE8\xAD\xB7",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xB4\xE3\x82\x87\xE3\x81\x86"
    "\xE3\x81\x98\xE3\x82\x93\xE3\x81\x94",
  }, {
    // "神護景雲" "じんごけいうん"
    767,
    "\xE7\xA5\x9E\xE8\xAD\xB7\xE6\x99\xAF\xE9\x9B\xB2",
    "\xE3\x81\x98\xE3\x82\x93\xE3\x81\x94"
    "\xE3\x81\x91\xE3\x81\x84\xE3\x81\x86\xE3\x82\x93",
  }, {
    // "宝亀" "ほうき"
    770,
    "\xE5\xAE\x9D\xE4\xBA\x80",
    "\xE3\x81\xBB\xE3\x81\x86\xE3\x81\x8D",
  }, {
    // "天応" "てんおう"
    781,
    "\xE5\xA4\xA9\xE5\xBF\x9C",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "延暦" "えんりゃく"
    782,
    "\xE5\xBB\xB6\xE6\x9A\xA6",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "大同" "たいどう"
    806,
    "\xE5\xA4\xA7\xE5\x90\x8C",
    "\xE3\x81\x9F\xE3\x81\x84\xE3\x81\xA9\xE3\x81\x86",
  }, {
    // "弘仁" "こうにん"
    810,
    "\xE5\xBC\x98\xE4\xBB\x81",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\xAB\xE3\x82\x93",
  }, {
    // "天長" "てんちょう"
    824,
    "\xE5\xA4\xA9\xE9\x95\xB7",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "承和" "じょうわ"
    834,
    "\xE6\x89\xBF\xE5\x92\x8C",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8F",
  }, {
    // "嘉祥" "かしょう"
    848,
    "\xE5\x98\x89\xE7\xA5\xA5",
    "\xE3\x81\x8B\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "仁寿" "にんじゅ"
    851,
    "\xE4\xBB\x81\xE5\xAF\xBF",
    "\xE3\x81\xAB\xE3\x82\x93\xE3\x81\x98\xE3\x82\x85",
  }, {
    // "斉衡" "さいこう"
    854,
    "\xE6\x96\x89\xE8\xA1\xA1",
    "\xE3\x81\x95\xE3\x81\x84\xE3\x81\x93\xE3\x81\x86",
  }, {
    // "天安" "てんなん"
    857,
    "\xE5\xA4\xA9\xE5\xAE\x89",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xAA\xE3\x82\x93",
  }, {
    // "貞観" "じょうかん"
    859,
    "\xE8\xB2\x9E\xE8\xA6\xB3",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\x8B\xE3\x82\x93",
  }, {
    // "元慶" "がんぎょう"
    877,
    "\xE5\x85\x83\xE6\x85\xB6",
    "\xE3\x81\x8C\xE3\x82\x93\xE3\x81\x8E\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "仁和" "にんな"
    885,
    "\xE4\xBB\x81\xE5\x92\x8C",
    "\xE3\x81\xAB\xE3\x82\x93\xE3\x81\xAA",
  }, {
    // "寛平" "かんぴょう"
    889,
    "\xE5\xAF\x9B\xE5\xB9\xB3",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\xB4\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "昌泰" "しょうたい"
    898,
    "\xE6\x98\x8C\xE6\xB3\xB0",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\x9F\xE3\x81\x84",
  }, {
    // "延喜" "えんぎ"
    901,
    "\xE5\xBB\xB6\xE5\x96\x9C",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x81\x8E",
  }, {
    // "延長" "えんちょう"
    923,
    "\xE5\xBB\xB6\xE9\x95\xB7",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "承平" "じょうへい"
    931,
    "\xE6\x89\xBF\xE5\xB9\xB3",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\xB8\xE3\x81\x84",
  }, {
    // "天慶" "てんぎょう"
    938,
    "\xE5\xA4\xA9\xE6\x85\xB6",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x8E\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "天暦" "てんりゃく"
    947,
    "\xE5\xA4\xA9\xE6\x9A\xA6",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "天徳" "てんとく"
    957,
    "\xE5\xA4\xA9\xE5\xBE\xB3",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "応和" "おうわ"
    961,
    "\xE5\xBF\x9C\xE5\x92\x8C",
    "\xE3\x81\x8A\xE3\x81\x86\xE3\x82\x8F",
  }, {
    // "康保" "こうほう"
    964,
    "\xE5\xBA\xB7\xE4\xBF\x9D",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\xBB\xE3\x81\x86",
  }, {
    // "安和" "あんな"
    968,
    "\xE5\xAE\x89\xE5\x92\x8C",
    "\xE3\x81\x82\xE3\x82\x93\xE3\x81\xAA",
  }, {
    // "天禄" "てんろく"
    970,
    "\xE5\xA4\xA9\xE7\xA6\x84",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x82\x8D\xE3\x81\x8F",
  }, {
    // "天延" "てんえん"
    973,
    "\xE5\xA4\xA9\xE5\xBB\xB6",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x88\xE3\x82\x93",
  }, {
    // "貞元" "じょうげん"
    976,
    "\xE8\xB2\x9E\xE5\x85\x83",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "天元" "てんげん"
    978,
    "\xE5\xA4\xA9\xE5\x85\x83",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "永観" "えいかん"
    983,
    "\xE6\xB0\xB8\xE8\xA6\xB3",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\x8B\xE3\x82\x93",
  }, {
    // "寛和" "かんな"
    985,
    "\xE5\xAF\x9B\xE5\x92\x8C",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\xAA",
  }, {
    // "永延" "えいえん"
    987,
    "\xE6\xB0\xB8\xE5\xBB\xB6",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\x88\xE3\x82\x93",
  }, {
    // "永祚" "えいそ"
    989,
    "\xE6\xB0\xB8\xE7\xA5\x9A",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\x9D",
  }, {
    // "正暦" "しょうりゃく"
    990,
    "\xE6\xAD\xA3\xE6\x9A\xA6",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "長徳" "ちょうとく"
    995,
    "\xE9\x95\xB7\xE5\xBE\xB3",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "長保" "ちょうほう"
    999,
    "\xE9\x95\xB7\xE4\xBF\x9D",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\xBB\xE3\x81\x86",
  }, {
    // "寛弘" "かんこう"
    1004,
    "\xE5\xAF\x9B\xE5\xBC\x98",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x93\xE3\x81\x86",
  }, {
    // "長和" "ちょうわ"
    1012,
    "\xE9\x95\xB7\xE5\x92\x8C",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8F",
  }, {
    // "寛仁" "かんにん"
    1017,
    "\xE5\xAF\x9B\xE4\xBB\x81",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\xAB\xE3\x82\x93",
  }, {
    // "治安" "じあん"
    1021,
    "\xE6\xB2\xBB\xE5\xAE\x89",
    "\xE3\x81\x98\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "万寿" "まんじゅ"
    1024,
    "\xE4\xB8\x87\xE5\xAF\xBF",
    "\xE3\x81\xBE\xE3\x82\x93\xE3\x81\x98\xE3\x82\x85",
  }, {
    // "長元" "ちょうげん"
    1028,
    "\xE9\x95\xB7\xE5\x85\x83",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "長暦" "ちょうりゃく"
    1037,
    "\xE9\x95\xB7\xE6\x9A\xA6",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "長久" "ちょうきゅう"
    1040,
    "\xE9\x95\xB7\xE4\xB9\x85",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x86",
  }, {
    // "寛徳" "かんとく"
    1044,
    "\xE5\xAF\x9B\xE5\xBE\xB3",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "永承" "えいしょう"
    1046,
    "\xE6\xB0\xB8\xE6\x89\xBF",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "天喜" "てんき"
    1053,
    "\xE5\xA4\xA9\xE5\x96\x9C",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x8D",
  }, {
    // "康平" "こうへい"
    1058,
    "\xE5\xBA\xB7\xE5\xB9\xB3",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\xB8\xE3\x81\x84",
  }, {
    // "治暦" "じりゃく"
    1065,
    "\xE6\xB2\xBB\xE6\x9A\xA6",
    "\xE3\x81\x98\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "延久" "えんきゅう"
    1069,
    "\xE5\xBB\xB6\xE4\xB9\x85",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x86",
  }, {
    // "承保" "じょうほう"
    1074,
    "\xE6\x89\xBF\xE4\xBF\x9D",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\xBB\xE3\x81\x86",
  }, {
    // "承暦" "じょうりゃく"
    1077,
    "\xE6\x89\xBF\xE6\x9A\xA6",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "永保" "えいほ"
    1081,
    "\xE6\xB0\xB8\xE4\xBF\x9D",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\xBB",
  }, {
    // "応徳" "おうとく"
    1084,
    "\xE5\xBF\x9C\xE5\xBE\xB3",
    "\xE3\x81\x8A\xE3\x81\x86\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "寛治" "かんじ"
    1087,
    "\xE5\xAF\x9B\xE6\xB2\xBB",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x98",
  }, {
    // "嘉保" "かほう"
    1094,
    "\xE5\x98\x89\xE4\xBF\x9D",
    "\xE3\x81\x8B\xE3\x81\xBB\xE3\x81\x86",
  }, {
    // "永長" "えいちょう"
    1096,
    "\xE6\xB0\xB8\xE9\x95\xB7",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "承徳" "じょうとく"
    1097,
    "\xE6\x89\xBF\xE5\xBE\xB3",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "康和" "こうわ"
    1099,
    "\xE5\xBA\xB7\xE5\x92\x8C",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x82\x8F",
  }, {
    // "長治" "ちょうじ"
    1104,
    "\xE9\x95\xB7\xE6\xB2\xBB",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x98",
  }, {
    // "嘉承" "かしょう"
    1106,
    "\xE5\x98\x89\xE6\x89\xBF",
    "\xE3\x81\x8B\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "天仁" "てんにん"
    1108,
    "\xE5\xA4\xA9\xE4\xBB\x81",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xAB\xE3\x82\x93",
  }, {
    // "天永" "てんえい"
    1110,
    "\xE5\xA4\xA9\xE6\xB0\xB8",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "永久" "えいきゅう"
    1113,
    "\xE6\xB0\xB8\xE4\xB9\x85",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x86",
  }, {
    // "元永" "げんえい"
    1118,
    "\xE5\x85\x83\xE6\xB0\xB8",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "保安" "ほうあん"
    1120,
    "\xE4\xBF\x9D\xE5\xAE\x89",
    "\xE3\x81\xBB\xE3\x81\x86\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "天治" "てんじ"
    1124,
    "\xE5\xA4\xA9\xE6\xB2\xBB",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x98",
  }, {
    // "大治" "だいじ"
    1126,
    "\xE5\xA4\xA7\xE6\xB2\xBB",
    "\xE3\x81\xA0\xE3\x81\x84\xE3\x81\x98",
  }, {
    // "天承" "てんじょう"
    1131,
    "\xE5\xA4\xA9\xE6\x89\xBF",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "長承" "ちょうじょう"
    1132,
    "\xE9\x95\xB7\xE6\x89\xBF",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "保延" "ほうえん"
    1135,
    "\xE4\xBF\x9D\xE5\xBB\xB6",
    "\xE3\x81\xBB\xE3\x81\x86\xE3\x81\x88\xE3\x82\x93",
  }, {
    // "永治" "えいじ"
    1141,
    "\xE6\xB0\xB8\xE6\xB2\xBB",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\x98",
  }, {
    // "康治" "こうじ"
    1142,
    "\xE5\xBA\xB7\xE6\xB2\xBB",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x98",
  }, {
    // "天養" "てんよう"
    1144,
    "\xE5\xA4\xA9\xE9\xA4\x8A",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x82\x88\xE3\x81\x86",
  }, {
    // "久安" "きゅうあん"
    1145,
    "\xE4\xB9\x85\xE5\xAE\x89",
    "\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x86\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "仁平" "にんぺい"
    1151,
    "\xE4\xBB\x81\xE5\xB9\xB3",
    "\xE3\x81\xAB\xE3\x82\x93\xE3\x81\xBA\xE3\x81\x84",
  }, {
    // "久寿" "きゅうじゅ"
    1154,
    "\xE4\xB9\x85\xE5\xAF\xBF",
    "\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x86\xE3\x81\x98\xE3\x82\x85",
  }, {
    // "保元" "ほうげん"
    1156,
    "\xE4\xBF\x9D\xE5\x85\x83",
    "\xE3\x81\xBB\xE3\x81\x86\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "平治" "へいじ"
    1159,
    "\xE5\xB9\xB3\xE6\xB2\xBB",
    "\xE3\x81\xB8\xE3\x81\x84\xE3\x81\x98",
  }, {
    // "永暦" "えいりゃく"
    1160,
    "\xE6\xB0\xB8\xE6\x9A\xA6",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "応保" "おうほ"
    1161,
    "\xE5\xBF\x9C\xE4\xBF\x9D",
    "\xE3\x81\x8A\xE3\x81\x86\xE3\x81\xBB",
  }, {
    // "長寛" "ちょうかん"
    1163,
    "\xE9\x95\xB7\xE5\xAF\x9B",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x8B\xE3\x82\x93",
  }, {
    // "永万" "えいまん"
    1165,
    "\xE6\xB0\xB8\xE4\xB8\x87",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\xBE\xE3\x82\x93",
  }, {
    // "仁安" "にんあん"
    1166,
    "\xE4\xBB\x81\xE5\xAE\x89",
    "\xE3\x81\xAB\xE3\x82\x93\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "嘉応" "かおう"
    1169,
    "\xE5\x98\x89\xE5\xBF\x9C",
    "\xE3\x81\x8B\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "承安" "しょうあん"
    1171,
    "\xE6\x89\xBF\xE5\xAE\x89",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "安元" "あんげん"
    1175,
    "\xE5\xAE\x89\xE5\x85\x83",
    "\xE3\x81\x82\xE3\x82\x93\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "治承" "じしょう"
    1177,
    "\xE6\xB2\xBB\xE6\x89\xBF",
    "\xE3\x81\x98\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "養和" "ようわ"
    1181,
    "\xE9\xA4\x8A\xE5\x92\x8C",
    "\xE3\x82\x88\xE3\x81\x86\xE3\x82\x8F",
  }, {
    // "寿永" "じゅえい"
    1182,
    "\xE5\xAF\xBF\xE6\xB0\xB8",
    "\xE3\x81\x98\xE3\x82\x85\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "元暦" "げんりゃく"
    1184,
    "\xE5\x85\x83\xE6\x9A\xA6",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "文治" "ぶんじ"
    1185,
    "\xE6\x96\x87\xE6\xB2\xBB",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\x98",
  }, {
    // "建久" "けんきゅう"
    1190,
    "\xE5\xBB\xBA\xE4\xB9\x85",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x86",
  }, {
    // "正治" "しょうじ"
    1199,
    "\xE6\xAD\xA3\xE6\xB2\xBB",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\x98",
  }, {
    // "建仁" "けんにん"
    1201,
    "\xE5\xBB\xBA\xE4\xBB\x81",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x81\xAB\xE3\x82\x93",
  }, {
    // "元久" "げんきゅう"
    1204,
    "\xE5\x85\x83\xE4\xB9\x85",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x86",
  }, {
    // "建永" "けんえい"
    1206,
    "\xE5\xBB\xBA\xE6\xB0\xB8",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "承元" "じょうげん"
    1207,
    "\xE6\x89\xBF\xE5\x85\x83",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "建暦" "けんりゃく"
    1211,
    "\xE5\xBB\xBA\xE6\x9A\xA6",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "建保" "けんぽう"
    1213,
    "\xE5\xBB\xBA\xE4\xBF\x9D",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x81\xBD\xE3\x81\x86",
  }, {
    // "承久" "しょうきゅう"
    1219,
    "\xE6\x89\xBF\xE4\xB9\x85",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x86",
  }, {
    // "貞応" "じょうおう"
    1222,
    "\xE8\xB2\x9E\xE5\xBF\x9C",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "元仁" "げんにん"
    1224,
    "\xE5\x85\x83\xE4\xBB\x81",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\xAB\xE3\x82\x93",
  }, {
    // "嘉禄" "かろく"
    1225,
    "\xE5\x98\x89\xE7\xA6\x84",
    "\xE3\x81\x8B\xE3\x82\x8D\xE3\x81\x8F",
  }, {
    // "安貞" "あんてい"
    1227,
    "\xE5\xAE\x89\xE8\xB2\x9E",
    "\xE3\x81\x82\xE3\x82\x93\xE3\x81\xA6\xE3\x81\x84",
  }, {
    // "寛喜" "かんき"
    1229,
    "\xE5\xAF\x9B\xE5\x96\x9C",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x8D",
  }, {
    // "貞永" "じょうえい"
    1232,
    "\xE8\xB2\x9E\xE6\xB0\xB8",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "天福" "てんぷく"
    1233,
    "\xE5\xA4\xA9\xE7\xA6\x8F",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xB7\xE3\x81\x8F",
  }, {
    // "文暦" "ぶんりゃく"
    1234,
    "\xE6\x96\x87\xE6\x9A\xA6",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "嘉禎" "かてい"
    1235,
    "\xE5\x98\x89\xE7\xA6\x8E",
    "\xE3\x81\x8B\xE3\x81\xA6\xE3\x81\x84",
  }, {
    // "暦仁" "りゃくにん"
    1238,
    "\xE6\x9A\xA6\xE4\xBB\x81",
    "\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F\xE3\x81\xAB\xE3\x82\x93",
  }, {
    // "延応" "えんおう"
    1239,
    "\xE5\xBB\xB6\xE5\xBF\x9C",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "仁治" "にんじゅ"
    1240,
    "\xE4\xBB\x81\xE6\xB2\xBB",
    "\xE3\x81\xAB\xE3\x82\x93\xE3\x81\x98\xE3\x82\x85",
  }, {
    // "寛元" "かんげん"
    1243,
    "\xE5\xAF\x9B\xE5\x85\x83",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "宝治" "ほうじ"
    1247,
    "\xE5\xAE\x9D\xE6\xB2\xBB",
    "\xE3\x81\xBB\xE3\x81\x86\xE3\x81\x98",
  }, {
    // "建長" "けんちょう"
    1249,
    "\xE5\xBB\xBA\xE9\x95\xB7",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "康元" "こうげん"
    1256,
    "\xE5\xBA\xB7\xE5\x85\x83",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "正嘉" "しょうか"
    1257,
    "\xE6\xAD\xA3\xE5\x98\x89",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\x8B",
  }, {
    // "正元" "しょうげん"
    1259,
    "\xE6\xAD\xA3\xE5\x85\x83",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "文応" "ぶんおう"
    1260,
    "\xE6\x96\x87\xE5\xBF\x9C",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "弘長" "こうちょう"
    1261,
    "\xE5\xBC\x98\xE9\x95\xB7",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "文永" "ぶんえい"
    1264,
    "\xE6\x96\x87\xE6\xB0\xB8",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "建治" "けんじ"
    1275,
    "\xE5\xBB\xBA\xE6\xB2\xBB",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x81\x98",
  }, {
    // "弘安" "こうあん"
    1278,
    "\xE5\xBC\x98\xE5\xAE\x89",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "正応" "しょうおう"
    1288,
    "\xE6\xAD\xA3\xE5\xBF\x9C",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "永仁" "えいにん"
    1293,
    "\xE6\xB0\xB8\xE4\xBB\x81",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\xAB\xE3\x82\x93",
  }, {
    // "正安" "しょうあん"
    1299,
    "\xE6\xAD\xA3\xE5\xAE\x89",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "乾元" "けんげん"
    1302,
    "\xE4\xB9\xBE\xE5\x85\x83",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "嘉元" "かげん"
    1303,
    "\xE5\x98\x89\xE5\x85\x83",
    "\xE3\x81\x8B\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "徳治" "とくじ"
    1306,
    "\xE5\xBE\xB3\xE6\xB2\xBB",
    "\xE3\x81\xA8\xE3\x81\x8F\xE3\x81\x98",
  }, {
    // "延慶" "えんぎょう"
    1308,
    "\xE5\xBB\xB6\xE6\x85\xB6",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x81\x8E\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "応長" "おうちょう"
    1311,
    "\xE5\xBF\x9C\xE9\x95\xB7",
    "\xE3\x81\x8A\xE3\x81\x86\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "正和" "しょうわ"
    1312,
    "\xE6\xAD\xA3\xE5\x92\x8C",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8F",
  }, {
    // "文保" "ぶんぽう"
    1317,
    "\xE6\x96\x87\xE4\xBF\x9D",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\xBD\xE3\x81\x86",
  }, {
    // "元応" "げんおう"
    1319,
    "\xE5\x85\x83\xE5\xBF\x9C",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "元亨" "げんこう"
    1321,
    "\xE5\x85\x83\xE4\xBA\xA8",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\x93\xE3\x81\x86",
  }, {
    // "正中" "しょうちゅう"
    1324,
    "\xE6\xAD\xA3\xE4\xB8\xAD",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA1\xE3\x82\x85\xE3\x81\x86",
  }, {
    // "嘉暦" "かりゃく"
    1326,
    "\xE5\x98\x89\xE6\x9A\xA6",
    "\xE3\x81\x8B\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "元徳" "げんとく"
    1329,
    "\xE5\x85\x83\xE5\xBE\xB3",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "元弘" "げんこう"
    1331,
    "\xE5\x85\x83\xE5\xBC\x98",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\x93\xE3\x81\x86",
  }, {
    // "建武" "けんむ"
    1334,
    "\xE5\xBB\xBA\xE6\xAD\xA6",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x82\x80",
  }, {
    // "延元" "えんげん"
    1336,
    "\xE5\xBB\xB6\xE5\x85\x83",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x81\x92\xE3\x82\x93",
  }, {
    // "興国" "こうこく"
    1340,
    "\xE8\x88\x88\xE5\x9B\xBD",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x93\xE3\x81\x8F",
  }, {
    // "正平" "しょうへい"
    1346,
    "\xE6\xAD\xA3\xE5\xB9\xB3",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\xB8\xE3\x81\x84",
  }, {
    // "建徳" "けんとく"
    1370,
    "\xE5\xBB\xBA\xE5\xBE\xB3",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "文中" "ぶんちゅう"
    1372,
    "\xE6\x96\x87\xE4\xB8\xAD",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\xA1\xE3\x82\x85\xE3\x81\x86",
  }, {
    // "天授" "てんじゅ"
    1375,
    "\xE5\xA4\xA9\xE6\x8E\x88",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x98\xE3\x82\x85",
  }, {
    // "弘和" "こうわ"
    1381,
    "\xE5\xBC\x98\xE5\x92\x8C",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x82\x8F",
  }, {
    // "元中" "げんちゅう"
    1384,
    "\xE5\x85\x83\xE4\xB8\xAD",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\xA1\xE3\x82\x85\xE3\x81\x86",
  }, {
    // "明徳" "めいとく"
    1390,
    "\xE6\x98\x8E\xE5\xBE\xB3",
    "\xE3\x82\x81\xE3\x81\x84\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "応永" "おうえい"
    1394,
    "\xE5\xBF\x9C\xE6\xB0\xB8",
    "\xE3\x81\x8A\xE3\x81\x86\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "正長" "しょうちょう"
    1428,
    "\xE6\xAD\xA3\xE9\x95\xB7",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "永享" "えいきょう"
    1429,
    "\xE6\xB0\xB8\xE4\xBA\xAB",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "嘉吉" "かきつ"
    1441,
    "\xE5\x98\x89\xE5\x90\x89",
    "\xE3\x81\x8B\xE3\x81\x8D\xE3\x81\xA4",
  }, {
    // "文安" "ぶんあん"
    1444,
    "\xE6\x96\x87\xE5\xAE\x89",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "宝徳" "ほうとく"
    1449,
    "\xE5\xAE\x9D\xE5\xBE\xB3",
    "\xE3\x81\xBB\xE3\x81\x86\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "享徳" "きょうとく"
    1452,
    "\xE4\xBA\xAB\xE5\xBE\xB3",
    "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "康正" "こうしょう"
    1455,
    "\xE5\xBA\xB7\xE6\xAD\xA3",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "長禄" "ちょうろく"
    1457,
    "\xE9\x95\xB7\xE7\xA6\x84",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8D\xE3\x81\x8F",
  }, {
    // "寛正" "かんしょう"
    1460,
    "\xE5\xAF\x9B\xE6\xAD\xA3",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "文正" "ぶんしょう"
    1466,
    "\xE6\x96\x87\xE6\xAD\xA3",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "応仁" "おうにん"
    1467,
    "\xE5\xBF\x9C\xE4\xBB\x81",
    "\xE3\x81\x8A\xE3\x81\x86\xE3\x81\xAB\xE3\x82\x93",
  }, {
    // "文明" "ぶんめい"
    1469,
    "\xE6\x96\x87\xE6\x98\x8E",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x82\x81\xE3\x81\x84",
  }, {
    // "長享" "ちょうきょう"
    1487,
    "\xE9\x95\xB7\xE4\xBA\xAB",
    "\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "延徳" "えんとく"
    1489,
    "\xE5\xBB\xB6\xE5\xBE\xB3",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "明応" "めいおう"
    1492,
    "\xE6\x98\x8E\xE5\xBF\x9C",
    "\xE3\x82\x81\xE3\x81\x84\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "文亀" "ぶんき"
    1501,
    "\xE6\x96\x87\xE4\xBA\x80",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\x8D",
  }, {
    // "永正" "えいしょう"
    1504,
    "\xE6\xB0\xB8\xE6\xAD\xA3",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "大永" "だいえい"
    1521,
    "\xE5\xA4\xA7\xE6\xB0\xB8",
    "\xE3\x81\xA0\xE3\x81\x84\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "享禄" "きょうろく"
    1528,
    "\xE4\xBA\xAB\xE7\xA6\x84",
    "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8D\xE3\x81\x8F",
  }, {
    // "天文" "てんぶん"
    1532,
    "\xE5\xA4\xA9\xE6\x96\x87",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xB6\xE3\x82\x93",
  }, {
    // "弘治" "こうじ"
    1555,
    "\xE5\xBC\x98\xE6\xB2\xBB",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x98",
  }, {
    // "永禄" "えいろく"
    1558,
    "\xE6\xB0\xB8\xE7\xA6\x84",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x82\x8D\xE3\x81\x8F",
  }, {
    // "元亀" "げんき"
    1570,
    "\xE5\x85\x83\xE4\xBA\x80",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\x8D",
  }, {
    // "天正" "てんしょう"
    1573,
    "\xE5\xA4\xA9\xE6\xAD\xA3",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "文禄" "ぶんろく"
    1592,
    "\xE6\x96\x87\xE7\xA6\x84",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x82\x8D\xE3\x81\x8F",
  }, {
    // "慶長" "けいちょう"
    1596,
    "\xE6\x85\xB6\xE9\x95\xB7",
    "\xE3\x81\x91\xE3\x81\x84\xE3\x81\xA1\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "元和" "げんな"
    1615,
    "\xE5\x85\x83\xE5\x92\x8C",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\xAA",
  }, {
    // "寛永" "かんえい"
    1624,
    "\xE5\xAF\x9B\xE6\xB0\xB8",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "正保" "しょうほう"
    1644,
    "\xE6\xAD\xA3\xE4\xBF\x9D",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\xBB\xE3\x81\x86",
  }, {
    // "慶安" "けいあん"
    1648,
    "\xE6\x85\xB6\xE5\xAE\x89",
    "\xE3\x81\x91\xE3\x81\x84\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "承応" "じょうおう"
    1652,
    "\xE6\x89\xBF\xE5\xBF\x9C",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "明暦" "めいれき"
    1655,
    "\xE6\x98\x8E\xE6\x9A\xA6",
    "\xE3\x82\x81\xE3\x81\x84\xE3\x82\x8C\xE3\x81\x8D",
  }, {
    // "万治" "まんじ"
    1658,
    "\xE4\xB8\x87\xE6\xB2\xBB",
    "\xE3\x81\xBE\xE3\x82\x93\xE3\x81\x98",
  }, {
    // "寛文" "かんぶん"
    1661,
    "\xE5\xAF\x9B\xE6\x96\x87",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\xB6\xE3\x82\x93",
  }, {
    // "延宝" "えんぽう"
    1673,
    "\xE5\xBB\xB6\xE5\xAE\x9D",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x81\xBD\xE3\x81\x86",
  }, {
    // "天和" "てんな"
    1681,
    "\xE5\xA4\xA9\xE5\x92\x8C",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xAA",
  }, {
    // "貞享" "じょうきょう"
    1684,
    "\xE8\xB2\x9E\xE4\xBA\xAB",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "元禄" "げんろく"
    1688,
    "\xE5\x85\x83\xE7\xA6\x84",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x82\x8D\xE3\x81\x8F",
  }, {
    // "宝永" "ほうえい"
    1704,
    "\xE5\xAE\x9D\xE6\xB0\xB8",
    "\xE3\x81\xBB\xE3\x81\x86\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "正徳" "しょうとく"
    1711,
    "\xE6\xAD\xA3\xE5\xBE\xB3",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "享保" "きょうほ"
    1716,
    "\xE4\xBA\xAB\xE4\xBF\x9D",
    "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xBB",
  }, {
    // "元文" "げんぶん"
    1736,
    "\xE5\x85\x83\xE6\x96\x87",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\xB6\xE3\x82\x93",
  }, {
    // "寛保" "かんぽ"
    1741,
    "\xE5\xAF\x9B\xE4\xBF\x9D",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\xBD",
  }, {
    // "延享" "えんきょう"
    1744,
    "\xE5\xBB\xB6\xE4\xBA\xAB",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "寛延" "かんえん"
    1748,
    "\xE5\xAF\x9B\xE5\xBB\xB6",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x88\xE3\x82\x93",
  }, {
    // "宝暦" "ほうれき"
    1751,
    "\xE5\xAE\x9D\xE6\x9A\xA6",
    "\xE3\x81\xBB\xE3\x81\x86\xE3\x82\x8C\xE3\x81\x8D",
  }, {
    // "明和" "めいわ"
    1764,
    "\xE6\x98\x8E\xE5\x92\x8C",
    "\xE3\x82\x81\xE3\x81\x84\xE3\x82\x8F",
  }, {
    // "安永" "あんえい"
    1772,
    "\xE5\xAE\x89\xE6\xB0\xB8",
    "\xE3\x81\x82\xE3\x82\x93\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "天明" "てんめい"
    1781,
    "\xE5\xA4\xA9\xE6\x98\x8E",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x82\x81\xE3\x81\x84",
  }, {
    // "寛政" "かんせい"
    1789,
    "\xE5\xAF\x9B\xE6\x94\xBF",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x9B\xE3\x81\x84",
  }, {
    // "享和" "きょうわ"
    1801,
    "\xE4\xBA\xAB\xE5\x92\x8C",
    "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8F",
  }, {
    // "文化" "ぶんか"
    1804,
    "\xE6\x96\x87\xE5\x8C\x96",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\x8B",
  }, {
    // "文政" "ぶんせい"
    1818,
    "\xE6\x96\x87\xE6\x94\xBF",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\x9B\xE3\x81\x84",
  }, {
    // "天保" "てんぽう"
    1830,
    "\xE5\xA4\xA9\xE4\xBF\x9D",
    "\xE3\x81\xA6\xE3\x82\x93\xE3\x81\xBD\xE3\x81\x86",
  }, {
    // "弘化" "こうか"
    1844,
    "\xE5\xBC\x98\xE5\x8C\x96",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x8B",
  }, {
    // "嘉永" "かえい"
    1848,
    "\xE5\x98\x89\xE6\xB0\xB8",
    "\xE3\x81\x8B\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "安政" "あんせい"
    1854,
    "\xE5\xAE\x89\xE6\x94\xBF",
    "\xE3\x81\x82\xE3\x82\x93\xE3\x81\x9B\xE3\x81\x84",
  }, {
    // "万延" "まんえん"
    1860,
    "\xE4\xB8\x87\xE5\xBB\xB6",
    "\xE3\x81\xBE\xE3\x82\x93\xE3\x81\x88\xE3\x82\x93",
  }, {
    // "文久" "ぶんきゅう"
    1861,
    "\xE6\x96\x87\xE4\xB9\x85",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x81\x8D\xE3\x82\x85\xE3\x81\x86",
  }, {
    // "元治" "げんじ"
    1864,
    "\xE5\x85\x83\xE6\xB2\xBB",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\x98",
  }, {
    // "慶応" "けいおう"
    1865,
    "\xE6\x85\xB6\xE5\xBF\x9C",
    "\xE3\x81\x91\xE3\x81\x84\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "明治" "めいじ"
    1868,
    "\xE6\x98\x8E\xE6\xB2\xBB",
    "\xE3\x82\x81\xE3\x81\x84\xE3\x81\x98",
  }, {
    // "大正" "たいしょう"
    1912,
    "\xE5\xA4\xA7\xE6\xAD\xA3",
    "\xE3\x81\x9F\xE3\x81\x84\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86",
  }, {
    // "昭和" "しょうわ"
    1926,
    "\xE6\x98\xAD\xE5\x92\x8C",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8F",
  }, {
    // "平成" "へいせい"
    1989,
    "\xE5\xB9\xB3\xE6\x88\x90",
    "\xE3\x81\xB8\xE3\x81\x84\xE3\x81\x9B\xE3\x81\x84",
  }
};

const YearData kNorthEraData[] = {
  // "元徳", "建武" and "明徳" are used for both south and north courts.
  {
    // "元徳" "げんとく"
    1329,
    "\xE5\x85\x83\xE5\xBE\xB3",
    "\xE3\x81\x92\xE3\x82\x93\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "正慶" "しょうけい"
    1332,
    "\xE6\xAD\xA3\xE6\x85\xB6",
    "\xE3\x81\x97\xE3\x82\x87\xE3\x81\x86\xE3\x81\x91\xE3\x81\x84",
  }, {
    // "建武" "けんむ"
    1334,
    "\xE5\xBB\xBA\xE6\xAD\xA6",
    "\xE3\x81\x91\xE3\x82\x93\xE3\x82\x80",
  }, {
    // "暦応" "りゃくおう"
    1338,
    "\xE6\x9A\xA6\xE5\xBF\x9C",
    "\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "康永" "こうえい"
    1342,
    "\xE5\xBA\xB7\xE6\xB0\xB8",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x88\xE3\x81\x84",
  }, {
    // "貞和" "じょうわ"
    1345,
    "\xE8\xB2\x9E\xE5\x92\x8C",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x82\x8F",
  }, {
    // "観応" "かんおう"
    1350,
    "\xE8\xA6\xB3\xE5\xBF\x9C",
    "\xE3\x81\x8B\xE3\x82\x93\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "文和" "ぶんわ"
    1352,
    "\xE6\x96\x87\xE5\x92\x8C",
    "\xE3\x81\xB6\xE3\x82\x93\xE3\x82\x8F",
  }, {
    // "延文" "えんぶん"
    1356,
    "\xE5\xBB\xB6\xE6\x96\x87",
    "\xE3\x81\x88\xE3\x82\x93\xE3\x81\xB6\xE3\x82\x93",
  }, {
    // "康安" "こうあん"
    1361,
    "\xE5\xBA\xB7\xE5\xAE\x89",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "貞治" "じょうじ"
    1362,
    "\xE8\xB2\x9E\xE6\xB2\xBB",
    "\xE3\x81\x98\xE3\x82\x87\xE3\x81\x86\xE3\x81\x98",
  }, {
    // "応安" "おうあん"
    1368,
    "\xE5\xBF\x9C\xE5\xAE\x89",
    "\xE3\x81\x8A\xE3\x81\x86\xE3\x81\x82\xE3\x82\x93",
  }, {
    // "永和" "えいわ"
    1375,
    "\xE6\xB0\xB8\xE5\x92\x8C",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x82\x8F",
  }, {
    // "康暦" "こうりゃく"
    1379,
    "\xE5\xBA\xB7\xE6\x9A\xA6",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x82\x8A\xE3\x82\x83\xE3\x81\x8F",
  }, {
    // "永徳" "えいとく"
    1381,
    "\xE6\xB0\xB8\xE5\xBE\xB3",
    "\xE3\x81\x88\xE3\x81\x84\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "至徳" "しとく"
    1384,
    "\xE8\x87\xB3\xE5\xBE\xB3",
    "\xE3\x81\x97\xE3\x81\xA8\xE3\x81\x8F",
  }, {
    // "嘉慶" "かけい"
    1387,
    "\xE5\x98\x89\xE6\x85\xB6",
    "\xE3\x81\x8B\xE3\x81\x91\xE3\x81\x84",
  }, {
    // "康応" "こうおう"
    1389,
    "\xE5\xBA\xB7\xE5\xBF\x9C",
    "\xE3\x81\x93\xE3\x81\x86\xE3\x81\x8A\xE3\x81\x86",
  }, {
    // "明徳" "めいとく"
    1390,
    "\xE6\x98\x8E\xE5\xBE\xB3",
    "\xE3\x82\x81\xE3\x81\x84\xE3\x81\xA8\xE3\x81\x8F",
  }
};

const char *kWeekDayString[] = {
  "\xE6\x97\xA5",   // "日"
  "\xE6\x9C\x88",   // "月"
  "\xE7\x81\xAB",   // "火"
  "\xE6\xB0\xB4",   // "水"
  "\xE6\x9C\xA8",   // "木"
  "\xE9\x87\x91",   // "金"
  "\xE5\x9C\x9F",   // "土"
};

// Converts a prefix and year number to Japanese Kanji Representation
// arguments :
//      - input "prefix" : a japanese style year counter prefix.
//      - input "year" : represent integer must be smaller than 100.
//      - output "result" : will be stored candidate strings
// If the input year is invalid ( accept only [ 1 - 99 ] ) , this function
// returns false and clear output vector.
bool ExpandYear(const string &prefix, int year, vector<string> *result) {
  DCHECK(result);
  if (year <= 0 || year >= 100) {
    result->clear();
    return false;
  }

  if (year == 1) {
    //  "元年"
    result->push_back(prefix + "\xE5\x85\x83");
    return true;
  }

  result->push_back(prefix + NumberUtil::SimpleItoa(year));

  string arabic = NumberUtil::SimpleItoa(year);

  vector<NumberUtil::NumberString> output;

  NumberUtil::ArabicToKanji(arabic, &output);

  for (int i = 0; i < output.size(); i++) {
    if (output[i].style == NumberUtil::NumberString::NUMBER_KANJI) {
      result->push_back(prefix + output[i].value);
    }
  }

  return true;
}

void Insert(const Segment::Candidate &base_candidate,
            int position,
            const string &value,
            const char *description,
            Segment *segment) {
  Segment::Candidate *c = segment->insert_candidate(position);
  DCHECK(c);
  c->Init();
  c->lid = base_candidate.lid;
  c->rid = base_candidate.rid;
  c->cost = base_candidate.cost;
  c->value = value;
  c->key = base_candidate.key;
  c->content_key = base_candidate.content_key;
  c->attributes |= Segment::Candidate::NO_LEARNING;
  c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  if (description != NULL) {
    c->description = description;
  }
}

enum {
  REWRITE_YEAR,
  REWRITE_DATE,
  REWRITE_MONTH,
  REWRITE_CURRENT_TIME,
  REWRITE_DATE_AND_CURRENT_TIME
};

bool AdToEraForCourt(const YearData *data, int size,
                     int year, vector<string> *results) {
  for (int i = size - 1; i >= 0; --i) {
    if (i == size - 1 && year > data[i].ad) {
      ExpandYear(data[i].era, year - data[i].ad + 1, results);
      return true;
    } else if (i > 0 && data[i-1].ad < year && year <= data[i].ad) {
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

// "ねん"
const char kNenKey[] = "\xE3\x81\xAD\xE3\x82\x93";
// "年"
const char kNenValue[] = "\xE5\xB9\xB4";

bool ExtractYearFromKey(const YearData &year_data,
                        const string &key,
                        int *year,
                        string *description) {
  // "がん"
  const char kGanKey[] = "\xE3\x81\x8C\xE3\x82\x93";
  // "元"
  const char kGanValue[] = "\xE5\x85\x83";

  // Util::EndsWith(key, kNenKey) is expected to always return true
  DCHECK(Util::EndsWith(key, kNenKey));
  if (!Util::StartsWith(key, year_data.key)) {
    return false;
  }
  // key="しょうわ59ねん" -> era_year_str="59"
  // key="へいせいがんねん" -> era_year_str="がん"
  const size_t year_start = Util::CharsLen(year_data.key);
  const size_t year_length = Util::CharsLen(key) -
      year_start - Util::CharsLen(kNenKey);
  string era_year_str(Util::SubString(key, year_start, year_length));

  // "元年"
  if (era_year_str == kGanKey) {
    *year = 1;
    description->assign(year_data.era).append(kGanValue).append(kNenValue);
    return true;
  }

  if (!NumberUtil::IsArabicNumber(era_year_str)) {
    return false;
  }
  *year = atoi32(era_year_str.c_str());
  if (*year <= 0) {
    return false;
  }
  *description = year_data.era + era_year_str + kNenValue;

  return true;
}

bool EraToAdForCourt(const YearData *data, size_t size, const string &key,
                     vector<string> *results, vector<string> *descriptions) {
  if (!Util::EndsWith(key, kNenKey)) {
    return false;
  }

  bool modified = false;
  for (size_t i = 0; i < size; ++i) {
    const YearData &year_data = data[i];
    if (!Util::StartsWith(key, year_data.key)) {
      continue;
    }

    // key="しょうわ59ねん" -> era_year=59, description="昭和59年"
    // key="へいせいがんねん" -> era_year=1, description="平成元年"
    int era_year = 0;
    string description = "";
    if (!ExtractYearFromKey(year_data, key, &era_year, &description)) {
      continue;
    }
    const int ad_year = era_year + year_data.ad - 1;

    // Get wide arabic numbers
    // e.g.) 1989 -> "１９８９", "一九八九"
    vector<NumberUtil::NumberString> output;
    const string ad_year_str(NumberUtil::SimpleItoa(ad_year));
    NumberUtil::ArabicToWideArabic(ad_year_str, &output);
    // add half-width arabic number to `output` (e.g. "1989")
    output.push_back(NumberUtil::NumberString(
        ad_year_str,
        "",
        NumberUtil::NumberString::DEFAULT_STYLE));

    for (size_t j = 0; j < output.size(); ++j) {
      // "元徳", "建武" and "明徳" require dedupe
      const string value(output[j].value + kNenValue);
      vector<string>::const_iterator found = find(results->begin(),
                                                  results->end(),
                                                  value);
      if (found != results->end()) {
        continue;
      }
      results->push_back(value);
      descriptions->push_back(description);
    }
    modified = true;
  }
  return modified;
}

// Checkes given date is valid or not.
// Over 24 hour expression is allowed in this function.
// Acceptable hour is between 0 and 29.
bool IsValidTime(uint32 hour, uint32 minute) {
  if (hour >= 30 || minute >= 60) {
    return false;
  } else {
    return true;
  }
}

// Returns February last day.
// This function deals with leap year with Gregorian calendar.
uint32 GetFebruaryLastDay(uint32 year) {
  uint32 february_end = (year % 4 == 0) ? 29 : 28;
  if (year % 100 == 0 && year % 400 != 0) {
    february_end = 28;
  }
  return february_end;
}

// Checkes given date is valid or not.
bool IsValidDate(uint32 year, uint32 month, uint32 day) {
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
    case 4: case 6: case 9: case 11: {
      if (day > 30) {
        return false;
      } else {
        return true;
      }
    }
    case 1: case 3: case 5: case 7: case 8: case 10: case 12: {
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

// Checks given date is valid or not in this year
bool IsValidDateInThisYear(uint32 month, uint32 day) {
  struct tm t_st;
  if (!Util::GetTmWithOffsetSecond(&t_st, 0)) {
    LOG(ERROR) << "GetTmWithOffsetSecond() failed";
    return false;
  }
  return IsValidDate(t_st.tm_year + 1900, month, day);
}
}  // namespace

// convert AD to Japanese ERA.
// The results will have multiple variants.
bool DateRewriter::AdToEra(int year, vector<string> *results) const {
  if (year < 645 || year > 2050) {    // TODO(taku) is it enough?
    return false;
  }

  // The order is south to north.
  vector<string> eras;
  bool r = false;
  r = AdToEraForCourt(kEraData, arraysize(kEraData), year, &eras);
  if (year > 1331 && year < 1393) {
    r |= AdToEraForCourt(kNorthEraData, arraysize(kNorthEraData),
                          year, &eras);
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

bool DateRewriter::EraToAd(const string &key,
                           vector<string> *results,
                           vector<string> *descritions) const {
  bool ret = false;
  // The order is south to north, older to newer
  ret |= EraToAdForCourt(kEraData, arraysize(kEraData),
                         key, results, descritions);
  ret |= EraToAdForCourt(kNorthEraData, arraysize(kNorthEraData),
                         key, results, descritions);
  return ret;
}

bool DateRewriter::ConvertTime(uint32 hour, uint32 min,
                               vector<string> *results) const {
  DCHECK(results);
  if (!IsValidTime(hour, min)) {
    return false;
  }

  results->push_back(Util::StringPrintf("%d:%2.2d", hour, min));

  // "H時MM分"
  results->push_back(Util::StringPrintf("%d\xE6\x99\x82%2.2d\xE5\x88\x86",
                                        hour, min));

  if (min == 30) {
    // "H時半"
    results->push_back(Util::StringPrintf("%d\xE6\x99\x82\xE5\x8D\x8A", hour));
  }

  if ((hour % 24) * 60 + min < 720) {   // 0:00 -- 11:59
    // "午前H時MM分"
    results->push_back(Util::StringPrintf(
        "\xE5\x8D\x88\xE5\x89\x8D%d\xE6\x99\x82%d\xE5\x88\x86",
        hour % 24, min));

    if (min == 30) {
      // "午前H時半"
      results->push_back(Util::StringPrintf(
          "\xE5\x8D\x88\xE5\x89\x8D%d\xE6\x99\x82\xE5\x8D\x8A",
          hour % 24));
    }
  } else {
    // "午後H時MM分"
    results->push_back(Util::StringPrintf(
        "\xE5\x8D\x88\xE5\xBE\x8C%d\xE6\x99\x82%d\xE5\x88\x86",
        (hour - 12) % 24, min));

    if (min == 30) {
      // "午後H時半"
      results->push_back(Util::StringPrintf(
          "\xE5\x8D\x88\xE5\xBE\x8C%d\xE6\x99\x82\xE5\x8D\x8A",
          (hour - 12) % 24));
    }
  }

  return true;
}

bool DateRewriter::ConvertDateWithYear(uint32 year, uint32 month, uint32 day,
                                       vector<string> *results) const {
    DCHECK(results);
    if (!IsValidDate(year, month, day)) {
      return false;
    }

    // "Y/MM/DD"
    results->push_back(Util::StringPrintf("%d/%2.2d/%2.2d", year, month, day));

    // "Y-MM-DD"
    results->push_back(Util::StringPrintf("%d-%2.2d-%2.2d", year, month, day));

    // "Y年M月D日"
    results->push_back(Util::StringPrintf(
        "%d" "\xE5\xB9\xB4" "%d" "\xE6\x9C\x88" "%d" "\xE6\x97\xA5",
        year, month, day));

    return true;
}

bool DateRewriter::ConvertDateWithoutYear(uint32 month, uint32 day,
                                          vector<string> *results) const {
    DCHECK(results);
    if (!IsValidDateInThisYear(month, day)) {
      return false;
    }

    // "MM/DD"
    results->push_back(Util::StringPrintf("%2.2d/%2.2d", month, day));

    // "M月D日"
    results->push_back(Util::StringPrintf("%d\xE6\x9C\x88%d\xE6\x97\xA5",
                                          month, day));

    return true;
}

bool DateRewriter::RewriteTime(Segment *segment,
                               const char *key,
                               const char *value,
                               const char *description,
                               int type, int diff) const {
  if (segment->key() != key) {   // only exact match
    return false;
  }

  const size_t kMinSize = 10;
  const size_t size = min(kMinSize, segment->candidates_size());

  for (size_t cand_idx = 0; cand_idx < size; ++cand_idx) {
    const Segment::Candidate &cand = segment->candidate(cand_idx);
    if (cand.value != value) {
      continue;
    }
    // Date candidates are too many, therefore highest candidate show at most
    // 3rd.
    // TODO(nona): learn date candidate even if the date is changed.
    const size_t kMinimumDateCandidateIdx = 3;
    const size_t insert_idx = (size < kMinimumDateCandidateIdx) ?
        size : max(cand_idx + 1, kMinimumDateCandidateIdx);

    struct tm t_st;
    vector<string> era;
    switch (type) {
      case REWRITE_DATE: {
        if (!Util::GetTmWithOffsetSecond(&t_st, diff * 86400)) {
          LOG(ERROR) << "GetTmWithOffsetSecond() failed";
          return false;
        }
        vector<string> results;
        ConvertDateWithYear(t_st.tm_year + 1900, t_st.tm_mon + 1, t_st.tm_mday,
                            &results);
        if (AdToEra(t_st.tm_year + 1900, &era) && !era.empty()) {
          // "平成Y年M月D日"
          results.push_back(Util::StringPrintf(
              "%s" "\xE5\xB9\xB4" "%d" "\xE6\x9C\x88" "%d" "\xE6\x97\xA5",
              era[0].c_str(), t_st.tm_mon + 1, t_st.tm_mday));
        }
        // "WDAY曜日"
        results.push_back(Util::StringPrintf("%s" "\xE6\x9B\x9C\xE6\x97\xA5",
                                             kWeekDayString[t_st.tm_wday]));

        for (vector<string>::reverse_iterator rit = results.rbegin();
             rit != results.rend(); ++rit) {
          Insert(cand, insert_idx , *rit, description, segment);
        }
        return true;
      }

      case REWRITE_MONTH: {
        if (!Util::GetCurrentTm(&t_st)) {
          LOG(ERROR) << "GetCurrentTm failed";
          return false;
        }
        const int month = (t_st.tm_mon + diff + 12) % 12 + 1;
        // "月"
        Insert(cand, insert_idx, Util::StringPrintf("%d\xE6\x9C\x88", month),
               description, segment);
        Insert(cand, insert_idx, Util::StringPrintf("%d", month), description,
               segment);
        return true;
      }

      case REWRITE_YEAR: {
        if (!Util::GetCurrentTm(&t_st)) {
          LOG(ERROR) << "GetCurrentTm failed";
          return false;
        }
        const int year = (t_st.tm_year + diff + 1900);
        if (AdToEra(year, &era) && !era.empty()) {
          // "年"
          Insert(cand, insert_idx,
                 Util::StringPrintf("%s\xE5\xB9\xB4", era[0].c_str()),
                 description, segment);
        }
        // "年"
        Insert(cand, insert_idx, Util::StringPrintf("%d\xE5\xB9\xB4", year),
               description, segment);
        Insert(cand, insert_idx, Util::StringPrintf("%d", year), description,
               segment);
        return true;
      }

      case REWRITE_CURRENT_TIME: {
        if (!Util::GetCurrentTm(&t_st)) {
          LOG(ERROR) << "GetCurrentTm failed";
          return false;
        }
        vector<string> times;
        ConvertTime(t_st.tm_hour, t_st.tm_min, &times);
        for (vector<string>::reverse_iterator rit = times.rbegin();
             rit != times.rend(); ++rit) {
          Insert(cand, insert_idx, *rit, description, segment);
        }
        return true;
      }

      case REWRITE_DATE_AND_CURRENT_TIME: {
        if (!Util::GetCurrentTm(&t_st)) {
          LOG(ERROR) << "GetCurrentTm failed";
          return false;
        }
        // Y/MM/DD H:MM
        const string ymmddhmm = Util::StringPrintf("%d/%2.2d/%2.2d %2d:%2.2d",
                                                   t_st.tm_year + 1900,
                                                   t_st.tm_mon + 1,
                                                   t_st.tm_mday,
                                                   t_st.tm_hour,
                                                   t_st.tm_min);
        Insert(cand, insert_idx, ymmddhmm, description, segment);
        return true;
      }
    }
  }

  return false;
}

bool DateRewriter::RewriteDate(Segment *segment) const {
  for (size_t i = 0; i < arraysize(kDateData); ++i) {
    if (RewriteTime(segment,
                    kDateData[i].key, kDateData[i].value,
                    kDateData[i].description,
                    REWRITE_DATE,
                    kDateData[i].diff)) {
      VLOG(1) << "RewriteDate: "
              << kDateData[i].key << " " << kDateData[i].value;
      return true;
    }
  }
  return false;
}

bool DateRewriter::RewriteMonth(Segment *segment) const {
  for (size_t i = 0; i < arraysize(kMonthData); ++i) {
    if (RewriteTime(segment,
                    kMonthData[i].key, kMonthData[i].value,
                    kMonthData[i].description,
                    REWRITE_MONTH,
                    kMonthData[i].diff)) {
      VLOG(1) << "RewriteMonth: "
              << kMonthData[i].key << " " << kMonthData[i].value;
      return true;
    }
  }
  return false;
}

bool DateRewriter::RewriteYear(Segment *segment) const {
  for (size_t i = 0; i < arraysize(kYearData); ++i) {
    if (RewriteTime(segment,
                    kYearData[i].key, kYearData[i].value,
                    kYearData[i].description,
                    REWRITE_YEAR,
                    kYearData[i].diff)) {
      VLOG(1) << "RewriteYear: "
              << kYearData[i].key << " " << kYearData[i].value;
      return true;
    }
  }
  return false;
}

bool DateRewriter::RewriteWeekday(Segment *segment) const {
  struct tm t_st;
  if (!Util::GetCurrentTm(&t_st)) {
    LOG(ERROR) << "GetCurrentTm failed";
    return false;
  }

  for (size_t i = 0; i < arraysize(kWeekDayData); ++i) {
    const int weekday = kWeekDayData[i].diff % 7;
    const int additional_dates = (weekday + 7 - t_st.tm_wday) % 7;
    if (RewriteTime(segment,
                    kWeekDayData[i].key, kWeekDayData[i].value,
                    kWeekDayData[i].description,
                    REWRITE_DATE,
                    additional_dates)) {
      VLOG(1) << "RewriteWeekday: "
              << kWeekDayData[i].key << " " << kWeekDayData[i].value;
      return true;
    }
  }

  return false;
}

bool DateRewriter::RewriteCurrentTime(Segment *segment) const {
  for (size_t i = 0; i < arraysize(kCurrentTimeData); ++i) {
    if (RewriteTime(segment,
                    kCurrentTimeData[i].key, kCurrentTimeData[i].value,
                    kCurrentTimeData[i].description,
                    REWRITE_CURRENT_TIME,
                    0)) {
      VLOG(1) << "RewriteCurrentTime: "
              << kCurrentTimeData[i].key << " "
              << kCurrentTimeData[i].value;
      return true;
    }
  }
  return false;
}

bool DateRewriter::RewriteDateAndCurrentTime(Segment *segment) const {
  for (size_t i = 0; i < arraysize(kDateAndCurrentTimeData); ++i) {
    if (RewriteTime(segment,
                    kDateAndCurrentTimeData[i].key,
                    kDateAndCurrentTimeData[i].value,
                    kDateAndCurrentTimeData[i].description,
                    REWRITE_DATE_AND_CURRENT_TIME,
                    0)) {
      VLOG(1) << "RewriteDateAndCurrentTime: "
              << kDateAndCurrentTimeData[i].key << " "
              << kDateAndCurrentTimeData[i].value;
      return true;
    }
  }
  return false;
}

bool DateRewriter::RewriteEra(Segment *current_segment,
                              const Segment &next_segment) const {
  if (current_segment->candidates_size() <= 0 ||
      next_segment.candidates_size() <= 0) {
    LOG(ERROR) << "Candidate size is 0";
    return false;
  }

  const string &current_key = current_segment->key();
  const string &next_value = next_segment.candidate(0).value;

  // "年"
  if (next_value != "\xE5\xB9\xB4") {
    return false;
  }

  if (Util::GetScriptType(current_key) != Util::NUMBER) {
    return false;
  }

  const size_t len = Util::CharsLen(current_key);
  if (len < 3 || len > 4) {
    LOG(WARNING) << "Too long year";
    return false;
  }

  string year_str;
  Util::FullWidthAsciiToHalfWidthAscii(current_key, &year_str);

  const uint32 year = atoi32(year_str.c_str());

  vector<string> results;
  if (!AdToEra(year, &results)) {
    return false;
  }

  const int kInsertPosition = 2;
  const int position
      = min(kInsertPosition,
            static_cast<int>(current_segment->candidates_size()));

  // "和暦"
  const char kDescription[] = "\xE5\x92\x8C\xE6\x9A\xA6";
  for (int i = static_cast<int>(results.size()) - 1; i >= 0; --i) {
    Insert(current_segment->candidate(0),
           position,
           results[i],
           kDescription,
           current_segment);
    current_segment->mutable_candidate(position)->attributes
        &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
  }

  return true;
}

bool DateRewriter::RewriteAd(Segment *segment) const {
  const string &key = segment->key();
  if (!Util::EndsWith(key, kNenKey)) {
    return false;
  }
  if (segment->candidates_size() == 0) {
    VLOG(2) << "No candidates are found";
    return false;
  }
  vector<string> results, descriptions;
  const bool ret = EraToAd(key, &results, &descriptions);

  // Insert position is the last of candidates
  const int position = static_cast<int>(segment->candidates_size());
  for (size_t i = 0; i < results.size(); ++i)  {
    Insert(segment->candidate(0),
           position,
           results[i],
           descriptions[i].c_str(),
           segment);
  }
  return ret;
}

namespace {
bool IsFourDigits(const string &value) {
  return Util::CharsLen(value) == 4 &&
         Util::GetScriptType(value) == Util::NUMBER;
}

// Gets four digits if possible.
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
// Prerequisit: |segments| has only one conversion segment.
const bool GetFourDigits(const composer::Composer &composer,
                         const Segments &segments,
                         string *output) {
  DCHECK(output);
  DCHECK_EQ(1, segments.conversion_segments_size());
  const Segment &segment = segments.conversion_segment(0);

  // 1. Segment's key
  if (IsFourDigits(segment.key())) {
    *output = segment.key();
    return true;
  }

  // 2. Meta candidates
  for (size_t i = 0; i < segment.meta_candidates_size(); ++i) {
    if (IsFourDigits(segment.meta_candidate(i).value)) {
      *output = segment.meta_candidate(i).value;
      return true;
    }
  }

  // 3. Raw input
  string raw;
  // Note that only one segment is in the Segments, but sometimes like
  // on partial conversion, segment.key() is different from the size of
  // the whole composition.
  composer.GetRawSubString(0, Util::CharsLen(segment.key()), &raw);
  if (IsFourDigits(raw)) {
    *output = raw;
    return true;
  }

  // No trials succeeded.
  return false;
}
}  // namespace

bool DateRewriter::RewriteFourDigits(const composer::Composer &composer,
                                     Segments *segments) const {
  if (segments->conversion_segments_size() != 1) {
    // This method rewrites a segment only when the segments has only
    // one conversion segment.
    // This is spec matter.
    // Rewriting multiple segments will not make users happier.
    return false;
  }

  string key;
  if (!GetFourDigits(composer, *segments, &key)) {
    // No four digit key is available.
    return false;
  }

  Segment *segment = segments->mutable_conversion_segment(0);

  string number_str;
  Util::FullWidthAsciiToHalfWidthAscii(key, &number_str);

  const uint32 number = atoi32(number_str.c_str());
  const uint32 upper_number = number / 100;
  const uint32 lower_number = number % 100;

  if (segment->candidates_size() == 0 &&
      segment->meta_candidates_size() == 0) {
    VLOG(2) << "No (meta) candidates are found";
    return false;
  }

  const Segment::Candidate &cand = (segment->candidates_size() > 0) ?
      segment->candidate(0) : segment->meta_candidate(0);

  bool is_modified = false;
  vector<string> result;
  is_modified |= ConvertDateWithoutYear(upper_number, lower_number, &result);

  for (size_t i = 0; i < result.size(); ++i) {
    // Always insert after last candidate.
    const int position = static_cast<int>(segment->candidates_size());
    // "日付"
    Insert(cand, position, result[i], "\xE6\x97\xA5\xE4\xBB\x98",
           segment);
  }

  result.clear();
  is_modified |= ConvertTime(upper_number, lower_number, &result);

  for (size_t i = 0; i < result.size(); ++i) {
    // Always insert after last candidate.
    const int position = static_cast<int>(segment->candidates_size());
    // "時刻"
    Insert(cand, position, result[i], "\xE6\x99\x82\xE5\x88\xBB",
           segment);
  }
  return is_modified;
}

DateRewriter::DateRewriter() {}
DateRewriter::~DateRewriter() {}

int DateRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool DateRewriter::Rewrite(const ConversionRequest &request,
                           Segments *segments) const {
  if (!GET_CONFIG(use_date_conversion)) {
    VLOG(2) << "no use_date_conversion";
    return false;
  }

  bool modified = false;

  // Japanese ERA to AD works for resegmented input only
  if (segments->conversion_segments_size() == 1) {
    Segment *seg = segments->mutable_segment(0);
    if (RewriteAd(seg)) {
      return true;
    }
  }

  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    if (seg == NULL) {
      LOG(ERROR) << "Segment is NULL";
      return false;
    }

    if (RewriteDate(seg) || RewriteWeekday(seg) ||
        RewriteMonth(seg) || RewriteYear(seg) ||
        RewriteCurrentTime(seg) ||
        RewriteDateAndCurrentTime(seg)) {
      modified = true;
    } else if (i + 1 < segments->segments_size() &&
               RewriteEra(seg, segments->segment(i + 1))) {
      modified = true;
      ++i;  // skip one more
    }
  }

  if (request.has_composer()) {
    modified |= RewriteFourDigits(request.composer(), segments);
  }

  return modified;
}
}  // namespace mozc
