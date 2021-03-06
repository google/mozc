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

#ifndef MOZC_REWRITER_DATE_REWRITER_H_
#define MOZC_REWRITER_DATE_REWRITER_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/port.h"
#include "dictionary/dictionary_interface.h"
#include "rewriter/rewriter_interface.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"
#include "absl/strings/string_view.h"

namespace mozc {

class Converter;
class ConversionRequest;
class Segment;
class Segments;

namespace composer {
class Composer;
}

class DateRewriter : public RewriterInterface {
 public:
  explicit DateRewriter(const dictionary::DictionaryInterface *dictionary);
  DateRewriter();
  ~DateRewriter() override;

  int capability(const ConversionRequest &request) const override;

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

  struct DateData {
    const char *key;
    const char *value;
    const char *description;
    int diff;  // diff from the current time in day or month or year
    int type;  // type of diff (e.g. year, month, date, etc).
  };

  // In general, Japanese era can be identified without the month.
  // However, during the era migration time, we have to check the month., i.e.,
  // 2019/01-04 => Heisei, 2019/05- => new era.
  // The `month` field is only checked at the year of 2019.
  // The case o year=2019 and month=0 is treated as "entire year" and returns
  // both Heisei and the new e
  static bool AdToEra(int year, int month, std::vector<std::string> *results);

  // For backward compatibility
  static bool AdToEra(int year, std::vector<std::string> *results) {
    return AdToEra(year, 1, results);
  }

  // Converts AD to Japanese ERA.
  // If given string is invalid, this function does not nothing and
  // returns false
  // The results will have multiple variants.
  // e.g.)
  //   key              -> results, descriptions
  //   -----------------------------------------------
  //   "へいせい20ねん" -> {"2008年", "２００８年", "二〇〇八年"},
  //                       {"平成20年", "平成20年", "平成20年"}
  //   "しょうわ2ねん"  -> {"1927年", "１９２７年", "一九二七年",
  //                        "1313年", "１３１３年", "一三一三年" },
  //                       {"昭和2年", "昭和2年", "昭和2年",
  //                        "正和2年", "正和2年", "正和2年"}
  static bool EraToAd(const std::string &key, std::vector<std::string> *results,
                      std::vector<std::string> *descriptions);

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
  static bool ConvertTime(uint32_t hour, uint32_t min,
                          std::vector<std::string> *results);

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
  static bool ConvertDateWithYear(uint32_t year, uint32_t month, uint32_t day,
                                  std::vector<std::string> *results);

 private:
  static bool RewriteDate(Segment *segment);
  static bool RewriteEra(Segment *current_segment, const Segment &next_segment);
  static bool RewriteAd(Segment *segment);

  // When only one conversion segment has consecutive number characters,
  // this function adds date and time candidates.
  // e.g.)
  //   key  -> candidates will be added
  //   ------------------------------------------------
  //   0101 -> "1月1日、01/01、1時1分,午前1時1分、1:01"
  //   2020 -> "20時20分、午後8時20分、20:20"
  //   2930 -> "29時30分、29時半、午前5時30分、午前5時半"
  //   123  -> "1月23日、01/23、1:23"
  static bool RewriteConsecutiveDigits(const composer::Composer &composer,
                                       int insert_position, Segments *segments);

  // Helper functions for RewriteConsecutiveDigits().
  static bool RewriteConsecutiveTwoDigits(
      absl::string_view str,
      std::vector<std::pair<std::string, const char *>> *results);
  static bool RewriteConsecutiveThreeDigits(
      absl::string_view str,
      std::vector<std::pair<std::string, const char *>> *results);
  static bool RewriteConsecutiveFourDigits(
      absl::string_view str,
      std::vector<std::pair<std::string, const char *>> *results);
};

}  // namespace mozc

#endif  // MOZC_REWRITER_DATE_REWRITER_H_
