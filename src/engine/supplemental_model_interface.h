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

#ifndef MOZC_ENGINE_SUPPLEMENTAL_MODEL_INTERFACE_H_
#define MOZC_ENGINE_SUPPLEMENTAL_MODEL_INTERFACE_H_

#include <optional>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "composer/query.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "protocol/engine_builder.pb.h"
#include "request/conversion_request.h"

namespace mozc::engine {

// Abstract interface of supplemental model.
class SupplementalModelInterface {
 public:
  virtual ~SupplementalModelInterface() = default;

  // Loads supplemental model asynchronously defined in the `request`.
  // Returns false if the LoadAsync is already running.
  virtual bool LoadAsync(const EngineReloadRequest& request) { return false; }

  // Loads supplemental model defined in the `request`.
  virtual EngineReloadResponse Load(const EngineReloadRequest& request) {
    return EngineReloadResponse();
  }

  // Returns true if supplemental model is available.
  // Useful to run intensive operations before using supplemental model.
  //
  // if (supplemental_model->IsAvailable()) {
  //    auto input = MakeInputWithIntensiveOperations();
  //    auto output = supplemental_model->ProcessXXX(input);
  // }
  virtual bool IsAvailable() const { return false; }

  // Performs spelling correction for composition (pre-edit) Hiragana sequence.
  // Returns empty result when no correction is required.
  // Returns std::nullopt when the composition spellchecker is not
  // enabled/available.
  virtual std::optional<std::vector<composer::TypeCorrectedQuery>>
  CorrectComposition(const ConversionRequest& request) const {
    return std::nullopt;
  }

  // Populates the typing correction penalty and attribute to `results`.
  virtual void PopulateTypeCorrectedQuery(
      const ConversionRequest& request,
      absl::Span<prediction::Result> results) const {}

  // Performs general post correction on `results`.
  virtual void PostCorrect(const ConversionRequest& request,
                           std::vector<prediction::Result>& results) const {}

  // Performs rescoring for `results` given the context `results`.
  virtual void RescoreResults(const ConversionRequest& request,
                              absl::Span<prediction::Result> results) const {}

  // Performs next word/phrase prediction given the context in `request`.
  // Results are appended to `results`. Returns true if prediction was
  // performed.
  virtual bool Predict(const ConversionRequest& request,
                       std::vector<prediction::Result>& results) const {
    return false;
  }

  // Returns character-by-mora reading to surface alignment.
  // {東,とう},{京,きょう} = GetReadingAlignment("東京", "とうきょう")
  // Returns empty list when no alignment is available.
  virtual std::vector<std::pair<absl::string_view, absl::string_view>>
  GetReadingAlignment(absl::string_view surface,
                      absl::string_view reading) const {
    return {};
  }
};

class SupplementalModelStub : public SupplementalModelInterface {};

}  // namespace mozc::engine

#endif  // MOZC_ENGINE_SUPPLEMENTAL_MODEL_INTERFACE_H_
