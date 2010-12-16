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

#include "rewriter/date_rewriter.h"

#include <stdio.h>
#include <time.h>
#include <algorithm>
#include <string>
#include <vector>
#include "base/util.h"
#include "converter/segments.h"
#include "session/config_handler.h"
#include "session/config.pb.h"

namespace mozc {

// const char kDatePrefix[] = "\xE2\x86\x92";  // "→" (ATOK style)
const char *kDatePrefix = NULL;

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
  },
  {
    // あした will show tomorrow's date
    "\xE3\x81\x82\xE3\x81\x97\xE3\x81\x9F",
    "\xE6\x98\x8E\xE6\x97\xA5",
    "\xE6\x98\x8E\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    1
  },
  {
    // あす will show tomorrow's date
    "\xE3\x81\x82\xE3\x81\x99",
    "\xE6\x98\x8E\xE6\x97\xA5",
    "\xE6\x98\x8E\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    1
  },
  {
    // さくじつ will show yesterday's date
    "\xE3\x81\x95\xE3\x81\x8F\xE3\x81\x98\xE3\x81\xA4",
    "\xE6\x98\xA8\xE6\x97\xA5",
    "\xE6\x98\xA8\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    -1
  },
  {
    // きのう will show yesterday's date
    "\xE3\x81\x8D\xE3\x81\xAE\xE3\x81\x86",
    "\xE6\x98\xA8\xE6\x97\xA5",
    "\xE6\x98\xA8\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    -1
  },
  {
    // おととい will show the date of 2 days ago
    "\xE3\x81\x8A\xE3\x81\xA8\xE3\x81\xA8\xE3\x81\x84",
    "\xE4\xB8\x80\xE6\x98\xA8\xE6\x97\xA5",
    "2\xE6\x97\xA5\xE5\x89\x8D\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    -2
  },
  {
    // さきおととい will show the date of 3 days ago
    "\xe3\x81\x95\xe3\x81\x8d\xe3\x81\x8a\xe3\x81\xa8"
    "\xe3\x81\xa8\xe3\x81\x84",
    "\xe4\xb8\x80\xe6\x98\xa8\xe6\x98\xa8\xe6\x97\xa5",
    "3\xE6\x97\xA5\xE5\x89\x8D\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    -3
  },
  {
    // あさって will show the date of 2 days from now
    "\xE3\x81\x82\xE3\x81\x95\xE3\x81\xA3\xE3\x81\xA6",
    "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5",
    "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    2,
  },
  {
    // みょうごにち will show the date of 2 days from now
    "\xE3\x81\xBF\xE3\x82\x87\xE3\x81\x86"
    "\xE3\x81\x94\xE3\x81\xAB\xE3\x81\xA1",
    "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5",
    "\xE6\x98\x8E\xE5\xBE\x8C\xE6\x97\xA5\xE3\x81\xAE\xE6\x97\xA5\xE4\xBB\x98",
    2
  },
  {
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
  },
  {
    // "げつようび"
    "\xe3\x81\x92\xe3\x81\xa4\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe6\x9c\x88\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe6\x9c\x88\xe6\x9b\x9c\xe6\x97\xa5",
    1
  },
  {
    // "かようび"
    "\xe3\x81\x8b\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe7\x81\xab\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe7\x81\xab\xe6\x9b\x9c\xe6\x97\xa5",
    2
  },
  {
    // "すいようび"
    "\xe3\x81\x99\xe3\x81\x84\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe6\xb0\xb4\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe6\xb0\xb4\xe6\x9b\x9c\xe6\x97\xa5",
    3
  },
  {
    // "もくようび"
    "\xe3\x82\x82\xe3\x81\x8f\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe6\x9c\xa8\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe6\x9c\xa8\xe6\x9b\x9c\xe6\x97\xa5",
    4
  },
  {
    // "きんようび"
    "\xe3\x81\x8d\xe3\x82\x93\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe9\x87\x91\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe9\x87\x91\xe6\x9b\x9c\xe6\x97\xa5",
    5
  },
  {
    // "どようび"
    "\xe3\x81\xa9\xe3\x82\x88\xe3\x81\x86\xe3\x81\xb3",
    "\xe5\x9c\x9f\xe6\x9b\x9c\xe6\x97\xa5",
    "\xe6\xac\xa1\xe3\x81\xae\xe5\x9c\x9f\xe6\x9b\x9c\xe6\x97\xa5",
    6
  },
  {
    // "にちよう"
    "\xE3\x81\xAB\xE3\x81\xA1\xE3\x82\x88\xE3\x81\x86",
    "\xE6\x97\xA5\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE6\x97\xA5\xE6\x9B\x9C\xE6\x97\xA5",
    0
  },
  {
    // "げつよう"
    "\xE3\x81\x92\xE3\x81\xA4\xE3\x82\x88\xE3\x81\x86",
    "\xE6\x9C\x88\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE6\x9C\x88\xE6\x9B\x9C\xE6\x97\xA5",
    1
  },
  {
    // "かよう"
    "\xE3\x81\x8B\xE3\x82\x88\xE3\x81\x86",
    "\xE7\x81\xAB\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE7\x81\xAB\xE6\x9B\x9C\xE6\x97\xA5",
    2
  },
  {
    // "すいよう"
    "\xE3\x81\x99\xE3\x81\x84\xE3\x82\x88\xE3\x81\x86",
    "\xE6\xB0\xB4\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE6\xB0\xB4\xE6\x9B\x9C\xE6\x97\xA5",
    3
  },
  {
    // "もくよう"
    "\xE3\x82\x82\xE3\x81\x8F\xE3\x82\x88\xE3\x81\x86",
    "\xE6\x9C\xA8\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE6\x9C\xA8\xE6\x9B\x9C\xE6\x97\xA5",
    4
  },
  {
    // "きんよう"
    "\xE3\x81\x8D\xE3\x82\x93\xE3\x82\x88\xE3\x81\x86",
    "\xE9\x87\x91\xE6\x9B\x9C",
    "\xE6\xAC\xA1\xE3\x81\xAE\xE9\x87\x91\xE6\x9B\x9C\xE6\x97\xA5",
    5
  },
  {
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
  },
  {
    // "らいねん"
    "\xE3\x82\x89\xE3\x81\x84\xE3\x81\xAD\xE3\x82\x93",
    "\xE6\x9D\xA5\xE5\xB9\xB4",
    "\xE6\x9D\xA5\xE5\xB9\xB4",
    1
  },
  {
    // "さくねん"
    "\xE3\x81\x95\xE3\x81\x8F\xE3\x81\xAD\xE3\x82\x93",
    "\xE6\x98\xA8\xE5\xB9\xB4",
    "\xE6\x98\xA8\xE5\xB9\xB4",
    -1
  },
  {
    // "きょねん"
    "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\xAD\xE3\x82\x93",
    "\xE5\x8E\xBB\xE5\xB9\xB4",
    "\xE5\x8E\xBB\xE5\xB9\xB4",
    -1
  },
  {
    // "おととし"
    "\xE3\x81\x8A\xE3\x81\xA8\xE3\x81\xA8\xE3\x81\x97",
    "\xE4\xB8\x80\xE6\x98\xA8\xE5\xB9\xB4",
    "\xE4\xB8\x80\xE6\x98\xA8\xE5\xB9\xB4",
    -2
  },
  {
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
  },
  {
    // "らいげつ"
    "\xE3\x82\x89\xE3\x81\x84\xE3\x81\x92\xE3\x81\xA4",
    "\xE6\x9D\xA5\xE6\x9C\x88",
    "\xE6\x9D\xA5\xE6\x9C\x88",
    1
  },
  {
    // "せんげつ"
    "\xE3\x81\x9B\xE3\x82\x93\xE3\x81\x92\xE3\x81\xA4",
    "\xE5\x85\x88\xE6\x9C\x88",
    "\xE5\x85\x88\xE6\x9C\x88",
    -1
  },
  {
    // "せんせんげつ"
    "\xE3\x81\x9B\xE3\x82\x93\xE3\x81\x9B\xE3\x82\x93\xE3\x81\x92\xE3\x81\xA4",
    "\xE5\x85\x88\xE3\x80\x85\xE6\x9C\x88",
    "\xE5\x85\x88\xE3\x80\x85\xE6\x9C\x88",
    -2
  },
  {
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
  },
  {
    // "じこく"
    "\xE3\x81\x98\xE3\x81\x93\xE3\x81\x8F",
    "\xE6\x99\x82\xE5\x88\xBB",
    "\xE7\x8F\xBE\xE5\x9C\xA8\xE3\x81\xAE\xE6\x99\x82\xE5\x88\xBB",
    0
  }
};

struct YearData {
  int ad;      // AD year
  const char *era;   // Japanese year a.k.a, GENGO
};

const YearData kEraData[] = {
  { 645, "\xE5\xA4\xA7\xE5\x8C\x96" },   // "大化"
  { 650, "\xE7\x99\xBD\xE9\x9B\x89" },   // "白雉"
  { 686, "\xE6\x9C\xB1\xE9\xB3\xA5" },   // "朱鳥"
  { 701, "\xE5\xA4\xA7\xE5\xAE\x9D" },   // "大宝"
  { 704, "\xE6\x85\xB6\xE9\x9B\xB2" },   // "慶雲"
  { 708, "\xE5\x92\x8C\xE9\x8A\x85" },   // "和銅"
  { 715, "\xE9\x9C\x8A\xE4\xBA\x80" },   // "霊亀
  { 717, "\xE9\xA4\x8A\xE8\x80\x81" },   // "養老
  { 724, "\xE7\xA5\x9E\xE4\xBA\x80" },   // "神亀
  { 729, "\xE5\xA4\xA9\xE5\xB9\xB3" },   // "天平
  // "天平感宝"
  { 749, "\xE5\xA4\xA9\xE5\xB9\xB3\xE6\x84\x9F\xE5\xAE\x9D" },
  // "天平勝宝"
  { 749, "\xE5\xA4\xA9\xE5\xB9\xB3\xE5\x8B\x9D\xE5\xAE\x9D" },
  // "天平宝字"
  { 757, "\xE5\xA4\xA9\xE5\xB9\xB3\xE5\xAE\x9D\xE5\xAD\x97" },
  // "天平神護"
  { 765, "\xE5\xA4\xA9\xE5\xB9\xB3\xE7\xA5\x9E\xE8\xAD\xB7" },
  // "神護景雲"
  { 767, "\xE7\xA5\x9E\xE8\xAD\xB7\xE6\x99\xAF\xE9\x9B\xB2" },
  { 770, "\xE5\xAE\x9D\xE4\xBA\x80" },   // "宝亀"
  { 781, "\xE5\xA4\xA9\xE5\xBF\x9C" },   // "天応"
  { 782, "\xE5\xBB\xB6\xE6\x9A\xA6" },   // "延暦"
  { 806, "\xE5\xA4\xA7\xE5\x90\x8C" },   // "大同"
  { 810, "\xE5\xBC\x98\xE4\xBB\x81" },   // "弘仁"
  { 824, "\xE5\xA4\xA9\xE9\x95\xB7" },   // "天長"
  { 834, "\xE6\x89\xBF\xE5\x92\x8C" },   // "承和"
  { 848, "\xE5\x98\x89\xE7\xA5\xA5" },   // "嘉祥"
  { 851, "\xE4\xBB\x81\xE5\xAF\xBF" },   // "仁寿"
  { 854, "\xE6\x96\x89\xE8\xA1\xA1" },   // "斉衡"
  { 857, "\xE5\xA4\xA9\xE5\xAE\x89" },   // "天安"
  { 859, "\xE8\xB2\x9E\xE8\xA6\xB3" },   // "貞観"
  { 877, "\xE5\x85\x83\xE6\x85\xB6" },   // "元慶"
  { 885, "\xE4\xBB\x81\xE5\x92\x8C" },   // "仁和"
  { 889, "\xE5\xAF\x9B\xE5\xB9\xB3" },   // "寛平"
  { 898, "\xE6\x98\x8C\xE6\xB3\xB0" },   // "昌泰"
  { 901, "\xE5\xBB\xB6\xE5\x96\x9C" },   // "延喜"
  { 923, "\xE5\xBB\xB6\xE9\x95\xB7" },   // "延長"
  { 931, "\xE6\x89\xBF\xE5\xB9\xB3" },   // "承平"
  { 938, "\xE5\xA4\xA9\xE6\x85\xB6" },   // "天慶"
  { 947, "\xE5\xA4\xA9\xE6\x9A\xA6" },   // "天暦"
  { 957, "\xE5\xA4\xA9\xE5\xBE\xB3" },   // "天徳"
  { 961, "\xE5\xBF\x9C\xE5\x92\x8C" },   // "応和"
  { 964, "\xE5\xBA\xB7\xE4\xBF\x9D" },   // "康保"
  { 968, "\xE5\xAE\x89\xE5\x92\x8C" },   // "安和"
  { 970, "\xE5\xA4\xA9\xE7\xA6\x84" },   // "天禄"
  { 973, "\xE5\xA4\xA9\xE5\xBB\xB6" },   // "天延"
  { 976, "\xE8\xB2\x9E\xE5\x85\x83" },   // "貞元"
  { 978, "\xE5\xA4\xA9\xE5\x85\x83" },   // "天元"
  { 983, "\xE6\xB0\xB8\xE8\xA6\xB3" },   // "永観"
  { 985, "\xE5\xAF\x9B\xE5\x92\x8C" },   // "寛和"
  { 987, "\xE6\xB0\xB8\xE5\xBB\xB6" },   // "永延"
  { 989, "\xE6\xB0\xB8\xE7\xA5\x9A" },   // "永祚"
  { 990, "\xE6\xAD\xA3\xE6\x9A\xA6" },   // "正暦"
  { 995, "\xE9\x95\xB7\xE5\xBE\xB3" },   // "長徳"
  { 999, "\xE9\x95\xB7\xE4\xBF\x9D" },   // "長保"
  { 1004, "\xE5\xAF\x9B\xE5\xBC\x98" },   // "寛弘"
  { 1012, "\xE9\x95\xB7\xE5\x92\x8C" },   // "長和"
  { 1017, "\xE5\xAF\x9B\xE4\xBB\x81" },   // "寛仁"
  { 1021, "\xE6\xB2\xBB\xE5\xAE\x89" },   // "治安"
  { 1024, "\xE4\xB8\x87\xE5\xAF\xBF" },   // "万寿"
  { 1028, "\xE9\x95\xB7\xE5\x85\x83" },   // "長元"
  { 1037, "\xE9\x95\xB7\xE6\x9A\xA6" },   // "長暦"
  { 1040, "\xE9\x95\xB7\xE4\xB9\x85" },   // "長久"
  { 1044, "\xE5\xAF\x9B\xE5\xBE\xB3" },   // "寛徳"
  { 1046, "\xE6\xB0\xB8\xE6\x89\xBF" },   // "永承"
  { 1053, "\xE5\xA4\xA9\xE5\x96\x9C" },   // "天喜"
  { 1058, "\xE5\xBA\xB7\xE5\xB9\xB3" },   // "康平"
  { 1065, "\xE6\xB2\xBB\xE6\x9A\xA6" },   // "治暦"
  { 1069, "\xE5\xBB\xB6\xE4\xB9\x85" },   // "延久"
  { 1074, "\xE6\x89\xBF\xE4\xBF\x9D" },   // "承保"
  { 1077, "\xE6\x89\xBF\xE6\x9A\xA6" },   // "承暦"
  { 1081, "\xE6\xB0\xB8\xE4\xBF\x9D" },   // "永保"
  { 1084, "\xE5\xBF\x9C\xE5\xBE\xB3" },   // "応徳"
  { 1087, "\xE5\xAF\x9B\xE6\xB2\xBB" },   // "寛治"
  { 1094, "\xE5\x98\x89\xE4\xBF\x9D" },   // "嘉保"
  { 1096, "\xE6\xB0\xB8\xE9\x95\xB7" },   // "永長"
  { 1097, "\xE6\x89\xBF\xE5\xBE\xB3" },   // "承徳"
  { 1099, "\xE5\xBA\xB7\xE5\x92\x8C" },   // "康和"
  { 1104, "\xE9\x95\xB7\xE6\xB2\xBB" },   // "長治"
  { 1106, "\xE5\x98\x89\xE6\x89\xBF" },   // "嘉承"
  { 1108, "\xE5\xA4\xA9\xE4\xBB\x81" },   // "天仁"
  { 1110, "\xE5\xA4\xA9\xE6\xB0\xB8" },   // "天永"
  { 1113, "\xE6\xB0\xB8\xE4\xB9\x85" },   // "永久"
  { 1118, "\xE5\x85\x83\xE6\xB0\xB8" },   // "元永"
  { 1120, "\xE4\xBF\x9D\xE5\xAE\x89" },   // "保安"
  { 1124, "\xE5\xA4\xA9\xE6\xB2\xBB" },   // "天治"
  { 1126, "\xE5\xA4\xA7\xE6\xB2\xBB" },   // "大治"
  { 1131, "\xE5\xA4\xA9\xE6\x89\xBF" },   // "天承"
  { 1132, "\xE9\x95\xB7\xE6\x89\xBF" },   // "長承"
  { 1135, "\xE4\xBF\x9D\xE5\xBB\xB6" },   // "保延"
  { 1141, "\xE6\xB0\xB8\xE6\xB2\xBB" },   // "永治"
  { 1142, "\xE5\xBA\xB7\xE6\xB2\xBB" },   // "康治"
  { 1144, "\xE5\xA4\xA9\xE9\xA4\x8A" },   // "天養"
  { 1145, "\xE4\xB9\x85\xE5\xAE\x89" },   // "久安"
  { 1151, "\xE4\xBB\x81\xE5\xB9\xB3" },   // "仁平"
  { 1154, "\xE4\xB9\x85\xE5\xAF\xBF" },   // "久寿"
  { 1156, "\xE4\xBF\x9D\xE5\x85\x83" },   // "保元"
  { 1159, "\xE5\xB9\xB3\xE6\xB2\xBB" },   // "平治"
  { 1160, "\xE6\xB0\xB8\xE6\x9A\xA6" },   // "永暦"
  { 1161, "\xE5\xBF\x9C\xE4\xBF\x9D" },   // "応保"
  { 1163, "\xE9\x95\xB7\xE5\xAF\x9B" },   // "長寛"
  { 1165, "\xE6\xB0\xB8\xE4\xB8\x87" },   // "永万"
  { 1166, "\xE4\xBB\x81\xE5\xAE\x89" },   // "仁安"
  { 1169, "\xE5\x98\x89\xE5\xBF\x9C" },   // "嘉応"
  { 1171, "\xE6\x89\xBF\xE5\xAE\x89" },   // "承安"
  { 1175, "\xE5\xAE\x89\xE5\x85\x83" },   // "安元"
  { 1177, "\xE6\xB2\xBB\xE6\x89\xBF" },   // "治承"
  { 1181, "\xE9\xA4\x8A\xE5\x92\x8C" },   // "養和"
  { 1182, "\xE5\xAF\xBF\xE6\xB0\xB8" },   // "寿永"
  { 1184, "\xE5\x85\x83\xE6\x9A\xA6" },   // "元暦"
  { 1185, "\xE6\x96\x87\xE6\xB2\xBB" },   // "文治"
  { 1190, "\xE5\xBB\xBA\xE4\xB9\x85" },   // "建久"
  { 1199, "\xE6\xAD\xA3\xE6\xB2\xBB" },   // "正治"
  { 1201, "\xE5\xBB\xBA\xE4\xBB\x81" },   // "建仁"
  { 1204, "\xE5\x85\x83\xE4\xB9\x85" },   // "元久"
  { 1206, "\xE5\xBB\xBA\xE6\xB0\xB8" },   // "建永"
  { 1207, "\xE6\x89\xBF\xE5\x85\x83" },   // "承元"
  { 1211, "\xE5\xBB\xBA\xE6\x9A\xA6" },   // "建暦"
  { 1213, "\xE5\xBB\xBA\xE4\xBF\x9D" },   // "建保"
  { 1219, "\xE6\x89\xBF\xE4\xB9\x85" },   // "承久"
  { 1222, "\xE8\xB2\x9E\xE5\xBF\x9C" },   // "貞応"
  { 1224, "\xE5\x85\x83\xE4\xBB\x81" },   // "元仁"
  { 1225, "\xE5\x98\x89\xE7\xA6\x84" },   // "嘉禄"
  { 1227, "\xE5\xAE\x89\xE8\xB2\x9E" },   // "安貞"
  { 1229, "\xE5\xAF\x9B\xE5\x96\x9C" },   // "寛喜"
  { 1232, "\xE8\xB2\x9E\xE6\xB0\xB8" },   // "貞永"
  { 1233, "\xE5\xA4\xA9\xE7\xA6\x8F" },   // "天福"
  { 1234, "\xE6\x96\x87\xE6\x9A\xA6" },   // "文暦"
  { 1235, "\xE5\x98\x89\xE7\xA6\x8E" },   // "嘉禎"
  { 1238, "\xE6\x9A\xA6\xE4\xBB\x81" },   // "暦仁"
  { 1239, "\xE5\xBB\xB6\xE5\xBF\x9C" },   // "延応"
  { 1240, "\xE4\xBB\x81\xE6\xB2\xBB" },   // "仁治"
  { 1243, "\xE5\xAF\x9B\xE5\x85\x83" },   // "寛元"
  { 1247, "\xE5\xAE\x9D\xE6\xB2\xBB" },   // "宝治"
  { 1249, "\xE5\xBB\xBA\xE9\x95\xB7" },   // "建長"
  { 1256, "\xE5\xBA\xB7\xE5\x85\x83" },   // "康元"
  { 1257, "\xE6\xAD\xA3\xE5\x98\x89" },   // "正嘉"
  { 1259, "\xE6\xAD\xA3\xE5\x85\x83" },   // "正元"
  { 1260, "\xE6\x96\x87\xE5\xBF\x9C" },   // "文応"
  { 1261, "\xE5\xBC\x98\xE9\x95\xB7" },   // "弘長"
  { 1264, "\xE6\x96\x87\xE6\xB0\xB8" },   // "文永"
  { 1275, "\xE5\xBB\xBA\xE6\xB2\xBB" },   // "建治"
  { 1278, "\xE5\xBC\x98\xE5\xAE\x89" },   // "弘安"
  { 1288, "\xE6\xAD\xA3\xE5\xBF\x9C" },   // "正応"
  { 1293, "\xE6\xB0\xB8\xE4\xBB\x81" },   // "永仁"
  { 1299, "\xE6\xAD\xA3\xE5\xAE\x89" },   // "正安"
  { 1302, "\xE4\xB9\xBE\xE5\x85\x83" },   // "乾元"
  { 1303, "\xE5\x98\x89\xE5\x85\x83" },   // "嘉元"
  { 1306, "\xE5\xBE\xB3\xE6\xB2\xBB" },   // "徳治"
  { 1308, "\xE5\xBB\xB6\xE6\x85\xB6" },   // "延慶"
  { 1311, "\xE5\xBF\x9C\xE9\x95\xB7" },   // "応長"
  { 1312, "\xE6\xAD\xA3\xE5\x92\x8C" },   // "正和"
  { 1317, "\xE6\x96\x87\xE4\xBF\x9D" },   // "文保"
  { 1319, "\xE5\x85\x83\xE5\xBF\x9C" },   // "元応"
  { 1321, "\xE5\x85\x83\xE4\xBA\xA8" },   // "元亨"
  { 1324, "\xE6\xAD\xA3\xE4\xB8\xAD" },   // "正中"
  { 1326, "\xE5\x98\x89\xE6\x9A\xA6" },   // "嘉暦"
  // "元徳" is used for both south and north courts.
  { 1329, "\xE5\x85\x83\xE5\xBE\xB3" },   // "元徳"
  { 1331, "\xE5\x85\x83\xE5\xBC\x98" },   // "元弘"
  { 1334, "\xE5\xBB\xBA\xE6\xAD\xA6" },   // "建武"
  { 1336, "\xE5\xBB\xB6\xE5\x85\x83" },   // "延元"
  { 1340, "\xE8\x88\x88\xE5\x9B\xBD" },   // "興国"
  { 1346, "\xE6\xAD\xA3\xE5\xB9\xB3" },   // "正平"
  { 1370, "\xE5\xBB\xBA\xE5\xBE\xB3" },   // "建徳"
  { 1372, "\xE6\x96\x87\xE4\xB8\xAD" },   // "文中"
  { 1375, "\xE5\xA4\xA9\xE6\x8E\x88" },   // "天授"
  { 1381, "\xE5\xBC\x98\xE5\x92\x8C" },   // "弘和"
  { 1384, "\xE5\x85\x83\xE4\xB8\xAD" },   // "元中"
  { 1390, "\xE6\x98\x8E\xE5\xBE\xB3" },   // "明徳"
  { 1394, "\xE5\xBF\x9C\xE6\xB0\xB8" },   // "応永"
  { 1428, "\xE6\xAD\xA3\xE9\x95\xB7" },   // "正長"
  { 1429, "\xE6\xB0\xB8\xE4\xBA\xAB" },   // "永享"
  { 1441, "\xE5\x98\x89\xE5\x90\x89" },   // "嘉吉"
  { 1444, "\xE6\x96\x87\xE5\xAE\x89" },   // "文安"
  { 1449, "\xE5\xAE\x9D\xE5\xBE\xB3" },   // "宝徳"
  { 1452, "\xE4\xBA\xAB\xE5\xBE\xB3" },   // "享徳"
  { 1455, "\xE5\xBA\xB7\xE6\xAD\xA3" },   // "康正"
  { 1457, "\xE9\x95\xB7\xE7\xA6\x84" },   // "長禄"
  { 1460, "\xE5\xAF\x9B\xE6\xAD\xA3" },   // "寛正"
  { 1466, "\xE6\x96\x87\xE6\xAD\xA3" },   // "文正"
  { 1467, "\xE5\xBF\x9C\xE4\xBB\x81" },   // "応仁"
  { 1469, "\xE6\x96\x87\xE6\x98\x8E" },   // "文明"
  { 1487, "\xE9\x95\xB7\xE4\xBA\xAB" },   // "長享"
  { 1489, "\xE5\xBB\xB6\xE5\xBE\xB3" },   // "延徳"
  { 1492, "\xE6\x98\x8E\xE5\xBF\x9C" },   // "明応"
  { 1501, "\xE6\x96\x87\xE4\xBA\x80" },   // "文亀"
  { 1504, "\xE6\xB0\xB8\xE6\xAD\xA3" },   // "永正"
  { 1521, "\xE5\xA4\xA7\xE6\xB0\xB8" },   // "大永"
  { 1528, "\xE4\xBA\xAB\xE7\xA6\x84" },   // "享禄"
  { 1532, "\xE5\xA4\xA9\xE6\x96\x87" },   // "天文"
  { 1555, "\xE5\xBC\x98\xE6\xB2\xBB" },   // "弘治"
  { 1558, "\xE6\xB0\xB8\xE7\xA6\x84" },   // "永禄"
  { 1570, "\xE5\x85\x83\xE4\xBA\x80" },   // "元亀"
  { 1573, "\xE5\xA4\xA9\xE6\xAD\xA3" },   // "天正"
  { 1592, "\xE6\x96\x87\xE7\xA6\x84" },   // "文禄"
  { 1596, "\xE6\x85\xB6\xE9\x95\xB7" },   // "慶長"
  { 1615, "\xE5\x85\x83\xE5\x92\x8C" },   // "元和"
  { 1624, "\xE5\xAF\x9B\xE6\xB0\xB8" },   // "寛永"
  { 1644, "\xE6\xAD\xA3\xE4\xBF\x9D" },   // "正保"
  { 1648, "\xE6\x85\xB6\xE5\xAE\x89" },   // "慶安"
  { 1652, "\xE6\x89\xBF\xE5\xBF\x9C" },   // "承応"
  { 1655, "\xE6\x98\x8E\xE6\x9A\xA6" },   // "明暦"
  { 1658, "\xE4\xB8\x87\xE6\xB2\xBB" },   // "万治"
  { 1661, "\xE5\xAF\x9B\xE6\x96\x87" },   // "寛文"
  { 1673, "\xE5\xBB\xB6\xE5\xAE\x9D" },   // "延宝"
  { 1681, "\xE5\xA4\xA9\xE5\x92\x8C" },   // "天和"
  { 1684, "\xE8\xB2\x9E\xE4\xBA\xAB" },   // "貞享"
  { 1688, "\xE5\x85\x83\xE7\xA6\x84" },   // "元禄"
  { 1704, "\xE5\xAE\x9D\xE6\xB0\xB8" },   // "宝永"
  { 1711, "\xE6\xAD\xA3\xE5\xBE\xB3" },   // "正徳"
  { 1716, "\xE4\xBA\xAB\xE4\xBF\x9D" },   // "享保"
  { 1736, "\xE5\x85\x83\xE6\x96\x87" },   // "元文"
  { 1741, "\xE5\xAF\x9B\xE4\xBF\x9D" },   // "寛保"
  { 1744, "\xE5\xBB\xB6\xE4\xBA\xAB" },   // "延享"
  { 1748, "\xE5\xAF\x9B\xE5\xBB\xB6" },   // "寛延"
  { 1751, "\xE5\xAE\x9D\xE6\x9A\xA6" },   // "宝暦"
  { 1764, "\xE6\x98\x8E\xE5\x92\x8C" },   // "明和"
  { 1772, "\xE5\xAE\x89\xE6\xB0\xB8" },   // "安永"
  { 1781, "\xE5\xA4\xA9\xE6\x98\x8E" },   // "天明"
  { 1789, "\xE5\xAF\x9B\xE6\x94\xBF" },   // "寛政"
  { 1801, "\xE4\xBA\xAB\xE5\x92\x8C" },   // "享和"
  { 1804, "\xE6\x96\x87\xE5\x8C\x96" },   // "文化"
  { 1818, "\xE6\x96\x87\xE6\x94\xBF" },   // "文政"
  { 1830, "\xE5\xA4\xA9\xE4\xBF\x9D" },   // "天保"
  { 1844, "\xE5\xBC\x98\xE5\x8C\x96" },   // "弘化"
  { 1848, "\xE5\x98\x89\xE6\xB0\xB8" },   // "嘉永"
  { 1854, "\xE5\xAE\x89\xE6\x94\xBF" },   // "安政"
  { 1860, "\xE4\xB8\x87\xE5\xBB\xB6" },   // "万延"
  { 1861, "\xE6\x96\x87\xE4\xB9\x85" },   // "文久"
  { 1864, "\xE5\x85\x83\xE6\xB2\xBB" },   // "元治"
  { 1865, "\xE6\x85\xB6\xE5\xBF\x9C" },   // "慶応"
  { 1868, "\xE6\x98\x8E\xE6\xB2\xBB" },   // "明治"
  { 1912, "\xE5\xA4\xA7\xE6\xAD\xA3" },   // "大正"
  { 1926, "\xE6\x98\xAD\xE5\x92\x8C" },   // "昭和"
  { 1989, "\xE5\xB9\xB3\xE6\x88\x90" },   // "平成"
};

const YearData kNorthEraData[] = {
  // "元徳" is used for both south and north courts.
  { 1329, "\xE5\x85\x83\xE5\xBE\xB3" },   // "元徳"
  { 1332, "\xE6\xAD\xA3\xE6\x85\xB6" },   // "正慶"
  { 1334, "\xE5\xBB\xBA\xE6\xAD\xA6" },   // "建武"
  { 1338, "\xE6\x9A\xA6\xE5\xBF\x9C" },   // "暦応"
  { 1342, "\xE5\xBA\xB7\xE6\xB0\xB8" },   // "康永"
  { 1345, "\xE8\xB2\x9E\xE5\x92\x8C" },   // "貞和"
  { 1350, "\xE8\xA6\xB3\xE5\xBF\x9C" },   // "観応"
  { 1352, "\xE6\x96\x87\xE5\x92\x8C" },   // "文和"
  { 1356, "\xE5\xBB\xB6\xE6\x96\x87" },   // "延文"
  { 1361, "\xE5\xBA\xB7\xE5\xAE\x89" },   // "康安"
  { 1362, "\xE8\xB2\x9E\xE6\xB2\xBB" },   // "貞治"
  { 1368, "\xE5\xBF\x9C\xE5\xAE\x89" },   // "応安"
  { 1375, "\xE6\xB0\xB8\xE5\x92\x8C" },   // "永和"
  { 1379, "\xE5\xBA\xB7\xE6\x9A\xA6" },   // "康暦"
  { 1381, "\xE6\xB0\xB8\xE5\xBE\xB3" },   // "永徳"
  { 1384, "\xE8\x87\xB3\xE5\xBE\xB3" },   // "至徳"
  { 1387, "\xE5\x98\x89\xE6\x85\xB6" },   // "嘉慶"
  { 1389, "\xE5\xBA\xB7\xE5\xBF\x9C" },   // "康応"
  { 1390, "\xE6\x98\x8E\xE5\xBE\xB3" },   // "明徳"
};

void Insert(Segment *segment,
            const Segment::Candidate &base_candidate,
            int position,
            const string &value,
            const char *description,
            const char *prefix) {
  Segment::Candidate *c = segment->insert_candidate(position);
  c->lid = base_candidate.lid;
  c->rid = base_candidate.rid;
  c->cost = base_candidate.cost;
  c->value = value;
  c->learning_type = Segment::Candidate::NO_LEARNING;
  c->key = base_candidate.key;
  c->content_key = base_candidate.content_key;
  if (description != NULL) {
    c->description = description;
  }
  if (prefix != NULL) {
    c->prefix = prefix;
  }
}

void ExpandYear(const string &prefix,
                int year,
                vector<string> *results) {
  if (year == 1) {
    results->push_back(prefix + "\xE5\x85\x83");       // "元"年
  } else {
    // TODO(taku) add Kansuji as well
    results->push_back(prefix + Util::SimpleItoa(year));
  }
}

enum {
  REWRITE_YEAR,
  REWRITE_DATE,
  REWRITE_MONTH,
  REWRITE_CURRENT_TIME,
};

bool ADtoERAforCourt(const YearData *data, int size,
                     int year, vector<string> *results) {
  for (int i = size - 1; i >= 0; --i) {
    if (i == size - 1 && year > data[i].ad) {
      ExpandYear(data[i].era, year - data[i].ad + 1, results);
      return true;
    } else if (i > 0 && data[i-1].ad < year && year <= data[i].ad) {
      // have two representations:
      // 1989 -> 昭和64 and 平成元
      if (year == data[i].ad) {
        ExpandYear(data[i].era, 1, results);
      }
      ExpandYear(data[i-1].era, year - data[i-1].ad + 1, results);
      return true;
    } else if (i == 0 && data[i].ad <= year) {
      if (year == data[i].ad) {
        ExpandYear(data[i].era, 1, results);
      } else {
        ExpandYear(data[i-1].era, year - data[i-1].ad + 1, results);
      }
      return true;
    }
  }

  return false;
}
}  // namespace

// convert AD to Japanese ERA.
// The results will have multiple variants.
bool DateRewriter::ADtoERA(int year, vector<string> *results) const {
  if (year < 645 || year > 2050) {    // TODO(taku) is it enough?
    return false;
  }

  // The order is south to north.
  vector<string> eras;
  bool r = false;
  r = ADtoERAforCourt(kEraData, arraysize(kEraData), year, &eras);
  if (year > 1331 && year < 1393) {
    r |= ADtoERAforCourt(kNorthEraData, arraysize(kNorthEraData),
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

bool DateRewriter::ConvertTime(int hour, int min,
                               vector<string> *results) const {
  char tmp[64];
  snprintf(tmp, sizeof(tmp), "%d:%2.2d", hour, min);
  results->push_back(tmp);

  snprintf(tmp, sizeof(tmp), "%d\xE6\x99\x82%2.2d\xE5\x88\x86",
           hour, min);
  results->push_back(tmp);

  if (hour * 60 + min < 720) {   // 0:00 -- 11:59
    // "午前x時x分"
    snprintf(tmp, sizeof(tmp),
             "\xE5\x8D\x88\xE5\x89\x8D%d\xE6\x99\x82%d\xE5\x88\x86",
             hour, min);
  } else {
    // "午後x時x分"
    snprintf(tmp, sizeof(tmp),
             "\xE5\x8D\x88\xE5\xBE\x8C%d\xE6\x99\x82%d\xE5\x88\x86",
             hour - 12, min);
  }
  results->push_back(tmp);

  return !results->empty();
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
  segment->GetCandidates(kMinSize);
  const size_t size = min(kMinSize, segment->candidates_size());

  for (int i = 0; i < static_cast<int>(size); ++i) {
    const Segment::Candidate &cand = segment->candidate(i);
    if (cand.value != value) {
      continue;
    }

    struct tm t_st;
    char tmp[64];
    vector<string> era;
    if (type == REWRITE_DATE) {
      if (!Util::GetTmWithOffsetSecond(&t_st, diff * 86400)) {
        LOG(ERROR) << "GetTmWithOffsetSecond() failed";
        return false;
      }
      if (ADtoERA(t_st.tm_year + 1900, &era) && !era.empty()) {
        // 平成YY年MM月DD日
        snprintf(tmp, sizeof(tmp),
                 "%s\xE5\xB9\xB4%d\xE6\x9C\x88%d\xE6\x97\xA5",
                 era[0].c_str(), t_st.tm_mon + 1, t_st.tm_mday);
        Insert(segment, cand, i + 1, tmp, description, kDatePrefix);
      }
      // YYYY年MM月DD日
      snprintf(tmp, sizeof(tmp),
               "%d\xE5\xB9\xB4%d\xE6\x9C\x88%d\xE6\x97\xA5",
               t_st.tm_year + 1900, t_st.tm_mon + 1, t_st.tm_mday);
      Insert(segment, cand, i + 1, tmp, description, kDatePrefix);
      // YYYY-MM-DD
      snprintf(tmp, sizeof(tmp),
               "%d-%2.2d-%2.2d",
               t_st.tm_year + 1900, t_st.tm_mon + 1, t_st.tm_mday);
      Insert(segment, cand, i + 1, tmp, description, kDatePrefix);
      // YYYY/MM/DD
      snprintf(tmp, sizeof(tmp),
               "%d/%2.2d/%2.2d",
               t_st.tm_year + 1900, t_st.tm_mon + 1, t_st.tm_mday);
      Insert(segment, cand, i + 1, tmp, description, kDatePrefix);
      return true;
    } else if (type == REWRITE_MONTH) {
      if (!Util::GetCurrentTm(&t_st)) {
        LOG(ERROR) << "GetCurrentTm failed";
        return false;
      }
      const int month = (t_st.tm_mon + diff + 12) % 12 + 1;
      snprintf(tmp, sizeof(tmp), "%d\xE6\x9C\x88", month);
      Insert(segment, cand, i + 1, tmp, description, kDatePrefix);
      snprintf(tmp, sizeof(tmp), "%d", month);
      Insert(segment, cand, i + 1, tmp, description, kDatePrefix);
      return true;
    } else if (type == REWRITE_YEAR) {
      if (!Util::GetCurrentTm(&t_st)) {
        LOG(ERROR) << "GetCurrentTm failed";
        return false;
      }
      const int year = (t_st.tm_year + diff + 1900);
      if (ADtoERA(year, &era) && !era.empty()) {
        snprintf(tmp, sizeof(tmp), "%s\xE5\xB9\xB4", era[0].c_str());
        Insert(segment, cand, i + 1, tmp, description, kDatePrefix);
      }
      snprintf(tmp, sizeof(tmp), "%d\xE5\xB9\xB4", year);
      Insert(segment, cand, i + 1, tmp, description, kDatePrefix);
      snprintf(tmp, sizeof(tmp), "%d", year);
      Insert(segment, cand, i + 1, tmp, description, kDatePrefix);
      return true;
    } else if (type == REWRITE_CURRENT_TIME) {
      if (!Util::GetCurrentTm(&t_st)) {
        LOG(ERROR) << "GetCurrentTm failed";
        return false;
      }
      vector<string> times;
      ConvertTime(t_st.tm_hour, t_st.tm_min, &times);
      for (int j = static_cast<int>(times.size()) - 1; j >= 0; --j) {
        Insert(segment, cand, i + 1, times[j], description, kDatePrefix);
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

bool DateRewriter::RewriteEra(Segment *current_segment,
                              const Segment &next_segment) const {
  if (current_segment->candidates_size() <= 0 ||
      next_segment.candidates_size() <= 0) {
    LOG(ERROR) << "Candidate size is 0";
    return false;
  }

  const string &current_value = current_segment->candidate(0).value;
  const string &next_value = next_segment.candidate(0).value;

  // "年"
  if (next_value != "\xE5\xB9\xB4") {
    return false;
  }

  if (Util::GetScriptType(current_value) != Util::NUMBER) {
    return false;
  }

  const size_t len = Util::CharsLen(current_value);
  if (len < 3 || len > 4) {
    LOG(WARNING) << "Too long year";
    return false;
  }

  string year_str;
  Util::FullWidthAsciiToHalfWidthAscii(current_value,
                                       &year_str);

  const uint32 year = atoi32(year_str.c_str());

  vector<string> results;
  if (!ADtoERA(year, &results)) {
    return false;
  }

  const int kInsertPosition = 2;
  current_segment->GetCandidates(kInsertPosition + 1);

  const int position
      = min(kInsertPosition,
            static_cast<int>(current_segment->candidates_size()));


  // "和暦"
  const char kDescription[] = "\xE5\x92\x8C\xE6\x9A\xA6";
  for (int i = static_cast<int>(results.size()) - 1; i >= 0; --i) {
    Insert(current_segment,
           current_segment->candidate(0),
           position,
           results[i],
           kDescription, NULL);
    if (current_segment->ExpandAlternative(position)) {
      current_segment->mutable_candidate(position)->description
          = kDescription;
      current_segment->mutable_candidate(position + 1)->description
          = kDescription;
    }
  }

  return true;
}

DateRewriter::DateRewriter() {}
DateRewriter::~DateRewriter() {}

bool DateRewriter::Rewrite(Segments *segments) const {
  if (!GET_CONFIG(use_date_conversion)) {
    VLOG(2) << "no use_date_conversion";
    return false;
  }

  bool modified = false;
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    if (seg == NULL) {
      LOG(ERROR) << "Segment is NULL";
      return false;
    }

    if (RewriteDate(seg) || RewriteWeekday(seg) ||
        RewriteMonth(seg) || RewriteYear(seg) ||
        RewriteCurrentTime(seg)) {
      modified = true;
    } else if (i + 1 < segments->segments_size() &&
               RewriteEra(seg, segments->segment(i + 1))) {
      modified = true;
      ++i;  // skip one more
    }
  }

  return modified;
}
}  // namespace mozc
