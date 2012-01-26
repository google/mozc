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

#ifndef MOZC_REWRITER_DATE_REWRITER_H_
#define MOZC_REWRITER_DATE_REWRITER_H_

#include <string>
#include <vector>
#include "base/base.h"
#include "rewriter/rewriter_interface.h"
#include "testing/base/public/gunit_prod.h"  // for FRIEND_TEST()

namespace mozc {

class Converter;
class Segment;
class Segments;

class DateRewriter: public RewriterInterface  {
 public:
  DateRewriter();
  virtual ~DateRewriter();

  virtual int capability() const;

  virtual bool Rewrite(Segments *segments) const;
 private:
  FRIEND_TEST(DateRewriterTest, ADToERA);
  FRIEND_TEST(DateRewriterTest, ConvertTime);
  FRIEND_TEST(DateRewriterTest, ConvertDateTest);

  bool RewriteTime(Segment *segment,
                   const char *key,
                   const char *value,
                   const char *description,
                   int type, int diff) const;
  bool RewriteDate(Segment *segment) const;
  bool RewriteMonth(Segment *segment) const;
  bool RewriteYear(Segment *segment) const;
  bool RewriteCurrentTime(Segment *segment) const;
  bool RewriteDateAndCurrentTime(Segment *segment) const;
  bool RewriteEra(Segment *current_segment,
                  const Segment &next_segment) const;
  bool RewriteWeekday(Segment *segment) const;

  // When segment has four number characters,this function adds date and time
  // candidates.
  // e.g.)
  //   key  -> candidates will be added
  //   ------------------------------------------------
  //   0101 -> "1月1日、01/01、1時1分,午前1時1分、1:01"
  //   2020 -> "20時20分、午後8時20分、20:20"
  //   2930 -> "29時30分、29時半、午前5時30分、午前5時半"
  bool RewriteFourDigits(Segment *segment) const;

  bool ADtoERA(int year, vector<string> *results) const;

  // Converts given time to string expression.
  // If given time information is invalid, this function does nothing and
  // returns false.
  // Over 24-hour expression is only allowed in case of lower than 30 hour.
  // The "hour" argument only accepts between 0 and 29.
  // The "min" argument only accepts between 0 and 59.
  // e.g.)
  //   hour : min -> strings will be pushed into vectors.
  //    1   :   1 -> "1時1分、午前1時1分、午前1時1分"
  //    1   :  30 -> "1時30分、午前1時30分、午前1時半、1時半、1:30"
  //   25   :  30 -> "25時30分、25時半、午前1時30分、午前1時半、25:30"
  bool ConvertTime(uint32 hour, uint32 min, vector<string> *results) const;

  // Converts given date to string expression.
  // If given date information is invalid, this function does nothing and
  // returns false.
  // The "year" argument only accepts positive integer.
  // The "month" argument only accepts between 1 and 12.
  // The "day" argument only accept valid day. This function deals with leap
  // year.
  // e.g.)
  //   year:month:day -> strings will be pushed into vectors.
  //   2011:  1  :  1 -> "平成23年1月1日,2011年1月1日,2011-01-01,2011/01/01"
  //   2011:  5  : 18 -> "平成23年5月18日,2011年5月18日,2011-05-18,2011/05/18"
  //   2000:  2  : 29 -> "平成12年2月29日,2000年2月29日,2000-02-29,2000/02/29"
  bool ConvertDateWithYear(uint32 year, uint32 month, uint32 day,
                           vector<string> *results) const;

  // Converts given date to string expression without year information.
  // This function validate month/day information with current year.
  // If given date information is invalid, this function does nothing and
  // retruns false.
  // The "month" argument only accepts between 1 and 12.
  // The "day" argument only accept valid day. This function deals with leap
  // year.
  // e.g.)
  //   month : day -> strings will be pushed into vectors
  //     1   :   1 -> "1月1日、01/01"
  //     2   :  28 -> "2月28日、02/28"
  bool ConvertDateWithoutYear(uint32 month, uint32 day,
                              vector<string> *results) const;
};
}

#endif  // MOZC_REWRITER_DATE_REWRITER_H_
