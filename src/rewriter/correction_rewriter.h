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

#ifndef MOZC_REWRITER_CORRECTION_REWRITER_H_
#define MOZC_REWRITER_CORRECTION_REWRITER_H_

#include <memory>
#include <vector>

#include "absl/strings/string_view.h"
#include "base/container/serialized_string_array.h"
#include "converter/candidate.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class CorrectionRewriter : public RewriterInterface {
 public:
  // Returns an instance of ReadingCorrectionRewriter initialized with the
  // default provided by data_manager.  The caller takes the ownership of the
  // instance.
  static std::unique_ptr<CorrectionRewriter> CreateCorrectionRewriter(
      const DataManager &data_manager);

  CorrectionRewriter(absl::string_view value_array_data,
                     absl::string_view error_array_data,
                     absl::string_view correction_array_data);

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

  int capability(const ConversionRequest &request) const override {
    return RewriterInterface::ALL;
  }

 private:
  struct ReadingCorrectionItem {
    ReadingCorrectionItem(absl::string_view v, absl::string_view e,
                          absl::string_view c)
        : value(v), error(e), correction(c) {}

    // ex. (value, error, correction) = ("雰囲気", "ふいんき", "ふんいき")
    absl::string_view value;
    absl::string_view error;
    absl::string_view correction;
  };

  // Sets |candidate| fields from |iterm|.
  static void SetCandidate(const ReadingCorrectionItem &item,
                           converter::Candidate *candidate);

  // Looks up corrections with key and value. Return true if at least
  // one correction is found in the internal dictionary.
  // If |value| is empty, looks up corrections only using the key.
  // The matched results are saved in |results|.
  // Return false if |results| is empty.
  bool LookupCorrection(absl::string_view key, absl::string_view value,
                        std::vector<ReadingCorrectionItem> *results) const;

  SerializedStringArray value_array_;
  SerializedStringArray error_array_;
  SerializedStringArray correction_array_;
};

}  // namespace mozc
#endif  // MOZC_REWRITER_CORRECTION_REWRITER_H_
