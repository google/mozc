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

#ifndef MOZC_REWRITER_DATE_REWRITER_H_
#define MOZC_REWRITER_DATE_REWRITER_H_

#include <string>
#include <vector>
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

  virtual bool Rewrite(Segments *segments) const;
 private:
  FRIEND_TEST(DateRewriterTest, ADToERA);
  FRIEND_TEST(DateRewriterTest, ConvertTime);

  bool RewriteTime(Segment *segment,
                   const char *key,
                   const char *value,
                   const char *description,
                   int type, int diff) const;
  bool RewriteDate(Segment *segment) const;
  bool RewriteMonth(Segment *segment) const;
  bool RewriteYear(Segment *segment) const;
  bool RewriteCurrentTime(Segment *segment) const;
  bool RewriteEra(Segment *current_segment,
                  const Segment &next_segment) const;
  bool RewriteWeekday(Segment *segment) const;

  bool ADtoERA(int year, vector<string> *results) const;
  bool ConvertTime(int hour, int min, vector<string> *results) const;
};
}

#endif  // MOZC_REWRITER_DATE_REWRITER_H_
