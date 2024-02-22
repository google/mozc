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

#include "engine/engine.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/logging.h"
#include "composer/query.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/modules.h"
#include "engine/spellchecker_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

namespace mozc {
namespace engine {

namespace {
class MockSpellchecker : public engine::SpellcheckerInterface {
 public:
  MOCK_METHOD(commands::CheckSpellingResponse, CheckSpelling,
              (const commands::CheckSpellingRequest &), (const, override));
  MOCK_METHOD(std::optional<std::vector<composer::TypeCorrectedQuery>>,
              CheckCompositionSpelling,
              (absl::string_view, absl::string_view, const commands::Request &),
              (const, override));
  MOCK_METHOD(void, MaybeApplyHomonymCorrection, (Segments *),
              (const override));
};
}  // namespace

TEST(EngineTest, ReloadModulesTest) {
  auto data_manager = std::make_unique<testing::MockDataManager>();
  absl::StatusOr<std::unique_ptr<Engine>> engine_status =
      Engine::CreateMobileEngine(std::move(data_manager));
  CHECK_OK(engine_status);

  std::unique_ptr<Engine> engine = std::move(engine_status.value());
  MockSpellchecker spellchecker;
  engine->SetSpellchecker(&spellchecker);
  EXPECT_EQ(engine->GetModulesForTesting()->GetSpellchecker(), &spellchecker);

  auto modules = std::make_unique<engine::Modules>();
  CHECK_OK(modules->Init(std::make_unique<testing::MockDataManager>()));

  const bool is_mobile = true;
  CHECK_OK(engine->ReloadModules(std::move(modules), is_mobile));

  EXPECT_EQ(engine->GetModulesForTesting()->GetSpellchecker(), &spellchecker);
}

}  // namespace engine
}  // namespace mozc
