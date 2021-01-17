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

#ifndef MOZC_REWRITER_SINGLE_KANJI_REWRITER_H_
#define MOZC_REWRITER_SINGLE_KANJI_REWRITER_H_

#include <memory>

#include "base/port.h"
#include "base/serialized_string_array.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/serialized_dictionary.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/rewriter_interface.h"
#include "absl/strings/string_view.h"

namespace mozc {

class SingleKanjiRewriter : public RewriterInterface {
 public:
  explicit SingleKanjiRewriter(const DataManagerInterface &data_manager);
  ~SingleKanjiRewriter() override;

  int capability(const ConversionRequest &request) const override;

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

 private:
  const dictionary::POSMatcher pos_matcher_;

  absl::string_view single_kanji_token_array_;
  SerializedStringArray single_kanji_string_array_;

  SerializedStringArray variant_type_array_;

  absl::string_view variant_token_array_;
  SerializedStringArray variant_string_array_;

  // Since noun_prefix_dictionary_ is just a tentative workaround,
  // we copy the SingleKanji structure so that we can remove this workaround
  // easily. Also, the logic of NounPrefix insertion is put independently from
  // the single kanji dictionary. Ideally, we want to regenerate our
  // language model for fixing noun-prefix issue.
  std::unique_ptr<SerializedDictionary> noun_prefix_dictionary_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_SINGLE_KANJI_REWRITER_H_
