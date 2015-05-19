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

#ifndef MOZC_REWRITER_TRANSLITERATION_REWRITER_H_
#define MOZC_REWRITER_TRANSLITERATION_REWRITER_H_

#include <string>
#include <vector>
#include "base/port.h"
#include "converter/segments.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class ConversionRequest;
class POSMatcher;
class Segments;
class Segment;

class TransliterationRewriter : public RewriterInterface  {
 public:
  struct T13NIds {
    uint16 hiragana_lid;
    uint16 hiragana_rid;
    uint16 katakana_lid;
    uint16 katakana_rid;
    uint16 ascii_lid;
    uint16 ascii_rid;
    T13NIds() : hiragana_lid(0), hiragana_rid(0),
                katakana_lid(0), katakana_rid(0),
                ascii_lid(0), ascii_rid(0) {}
  };

  explicit TransliterationRewriter(const POSMatcher &pos_matcher);
  virtual ~TransliterationRewriter();

  virtual int capability() const;

  virtual bool Rewrite(const ConversionRequest &request,
                       Segments *segments) const;

 private:
  void InitT13NCandidate(const string &key,
                         const string &value,
                         uint16 lid,
                         uint16 rid,
                         Segment::Candidate *cand) const;
  void SetTransliterations(const vector<string> &t13ns,
                           const T13NIds &ids,
                           Segment *segment) const;
  bool FillT13NsFromComposer(const ConversionRequest &request,
                             Segments *segments) const;
  bool FillT13NsFromKey(Segments *segments) const;

  const uint16 unknown_id_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_TRANSLITERATION_REWRITER_H_
