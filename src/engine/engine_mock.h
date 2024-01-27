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

#ifndef MOZC_ENGINE_ENGINE_MOCK_H_
#define MOZC_ENGINE_ENGINE_MOCK_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "converter/converter_interface.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/suppression_dictionary.h"
#include "engine/engine_interface.h"
#include "engine/spellchecker_interface.h"
#include "engine/user_data_manager_interface.h"
#include "prediction/predictor_interface.h"
#include "testing/gmock.h"

namespace mozc {

class MockEngine : public EngineInterface {
 public:
  MOCK_METHOD(ConverterInterface *, GetConverter, (), (const, override));
  MOCK_METHOD(prediction::PredictorInterface *, GetPredictor, (),
              (const, override));
  MOCK_METHOD(dictionary::SuppressionDictionary *, GetSuppressionDictionary, (),
              (override));
  MOCK_METHOD(bool, Reload, (), (override));
  MOCK_METHOD(bool, ReloadAndWait, (), (override));
  MOCK_METHOD(UserDataManagerInterface *, GetUserDataManager, (), (override));
  MOCK_METHOD(absl::string_view, GetDataVersion, (), (const, override));
  MOCK_METHOD(const DataManagerInterface *, GetDataManager, (),
              (const, override));
  MOCK_METHOD(std::vector<std::string>, GetPosList, (), (const, override));
  MOCK_METHOD(void, SetSpellchecker, (const engine::SpellcheckerInterface *),
              (override));
};

}  // namespace mozc

#endif  // MOZC_ENGINE_ENGINE_MOCK_H_
