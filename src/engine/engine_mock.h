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

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "engine/engine_converter_interface.h"
#include "engine/engine_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "testing/gmock.h"

namespace mozc {

class MockEngine : public EngineInterface {
 public:
  MOCK_METHOD(absl::string_view, GetDataVersion, (), (const, override));
  MOCK_METHOD(std::unique_ptr<engine::EngineConverterInterface>,
              CreateEngineConverter, (), (const, override));
  MOCK_METHOD(bool, Reload, (), (override));
  MOCK_METHOD(bool, Sync, (), (override));
  MOCK_METHOD(bool, Wait, (), (override));
  MOCK_METHOD(bool, ClearUserHistory, (), (override));
  MOCK_METHOD(bool, ClearUserPrediction, (), (override));
  MOCK_METHOD(bool, ClearUnusedUserPrediction, (), (override));
  MOCK_METHOD(bool, ReloadAndWait, (), (override));
  MOCK_METHOD(std::vector<std::string>, GetPosList, (), (const, override));
};

}  // namespace mozc

#endif  // MOZC_ENGINE_ENGINE_MOCK_H_
