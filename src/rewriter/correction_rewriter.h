// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_REWRITER_CORRECTION_REWRITER_H_
#define MOZC_REWRITER_CORRECTION_REWRITER_H_

#include <map>
#include <string>
#include <vector>

#include "base/base.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

struct ReadingCorrectionItem {
  // ex. (value, error, correction) = ("雰囲気", "ふいんき", "ふんいき")
  const char *value;
  const char *error;
  const char *correction;
};

class ConversionRequest;
class DataManagerInterface;
class Segments;

class CorrectionRewriter : public RewriterInterface  {
 public:
  // Returnes an instance of ReadingCorrectionRewriter initialized with the
  // default provided by data_manager.  The caller takes the ownership of the
  // instance.
  static CorrectionRewriter *CreateCorrectionRewriter(
      const DataManagerInterface *data_manager);

  CorrectionRewriter(const ReadingCorrectionItem *reading_corrections,
                     size_t array_size);
  virtual ~CorrectionRewriter();
  virtual bool Rewrite(const ConversionRequest &request,
                       Segments *segments) const;

  virtual int capability(const ConversionRequest &request) const {
    return RewriterInterface::ALL;
  }

 private:
  const ReadingCorrectionItem *reading_corrections_;
  size_t size_;

  // Looks up corrections with key and value. Return true if at least
  // one correction is found in the internal dictionary.
  // If |value| is empty, looks up corrections only using the key.
  // The matched results are saved in |results|.
  // Return false if |results| is empty.
  bool LookupCorrection(
      const string &key,
      const string &value,
      vector<const ReadingCorrectionItem *> *results) const;
};

}  // namespace mozc
#endif  // MOZC_REWRITER_CORRECTION_REWRITER_H_
