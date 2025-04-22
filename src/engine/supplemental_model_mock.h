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

#ifndef MOZC_ENGINE_SUPPLEMENTAL_MODEL_MOCK_H_
#define MOZC_ENGINE_SUPPLEMENTAL_MODEL_MOCK_H_

#include <optional>
#include <vector>

#include "absl/types/span.h"
#include "composer/query.h"
#include "engine/supplemental_model_interface.h"
#include "prediction/result.h"
#include "protocol/commands.pb.h"
#include "protocol/engine_builder.pb.h"
#include "request/conversion_request.h"
#include "testing/gmock.h"

namespace mozc::engine {

class MockSupplementalModel : public SupplementalModelInterface {
 public:
  MOCK_METHOD(bool, LoadAsync, (const EngineReloadRequest &request),
              (override));
  MOCK_METHOD(EngineReloadResponse, Load, (const EngineReloadRequest &request),
              (override));
  MOCK_METHOD(std::optional<std::vector<composer::TypeCorrectedQuery>>,
              CorrectComposition, (const ConversionRequest &request),
              (const, override));
  MOCK_METHOD(void, PopulateTypeCorrectedQuery,
              (const ConversionRequest &request,
               absl::Span<prediction::Result> results),
              (const, override));
  MOCK_METHOD(void, PostCorrect,
              (const ConversionRequest &request,
               std::vector<prediction::Result> &results),
              (const, override));
  MOCK_METHOD(void, RescoreResults,
              (const ConversionRequest &request,
               absl::Span<prediction::Result> results),
              (const, override));
  MOCK_METHOD(bool, Predict,
              (const ConversionRequest &request,
               std::vector<prediction::Result> &results),
              (const, override));
};

}  // namespace mozc::engine

#endif  // MOZC_ENGINE_SUPPLEMENTAL_MODEL_MOCK_H_
