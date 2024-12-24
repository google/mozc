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

#ifndef MOZC_ENGINE_ENGINE_H_
#define MOZC_ENGINE_ENGINE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "converter/converter.h"
#include "converter/converter_interface.h"
#include "data_manager/data_manager.h"
#include "engine/data_loader.h"
#include "engine/engine_interface.h"
#include "engine/minimal_engine.h"
#include "engine/modules.h"
#include "engine/supplemental_model_interface.h"
#include "prediction/predictor_interface.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

// Builds and manages a set of modules that are necessary for conversion engine.
class Engine : public EngineInterface {
 public:
  // There are two types of engine: desktop and mobile.  The differences are the
  // underlying prediction engine (DesktopPredictor or MobilePredictor) and
  // learning preference (to learn content word or not).  See Init() for the
  // details of implementation.

  // Creates an instance with desktop configuration from a data manager.  The
  // ownership of data manager is passed to the engine instance.
  static absl::StatusOr<std::unique_ptr<Engine>> CreateDesktopEngine(
      std::unique_ptr<const DataManager> data_manager);

  // Creates an instance with mobile configuration from a data manager.  The
  // ownership of data manager is passed to the engine instance.
  static absl::StatusOr<std::unique_ptr<Engine>> CreateMobileEngine(
      std::unique_ptr<const DataManager> data_manager);

  // Creates an instance from a data manager and is_mobile flag.
  static absl::StatusOr<std::unique_ptr<Engine>> CreateEngine(
      std::unique_ptr<const DataManager> data_manager, bool is_mobile);

  // Creates an instance with the given modules and is_mobile flag.
  static absl::StatusOr<std::unique_ptr<Engine>> CreateEngine(
      std::unique_ptr<engine::Modules> modules, bool is_mobile);

  // Creates an engine with no initialization.
  static std::unique_ptr<Engine> CreateEngine();

  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  // TODO(taku): Avoid returning pointer, as converter_ may be updated
  // dynamically and return value will become a dangling pointer.
  ConverterInterface *GetConverter() const override {
    return converter_ ? converter_.get() : minimal_engine_.GetConverter();
  }

  absl::string_view GetPredictorName() const override {
    return converter_ ? converter_->predictor()->GetPredictorName()
                      : minimal_engine_.GetPredictorName();
  }

  // Functions for Reload, Sync, Wait return true if successfully operated
  // or did nothing.
  bool Reload() override;
  bool Sync() override;
  bool Wait() override;
  bool ReloadAndWait() override;

  bool ClearUserHistory() override;
  bool ClearUserPrediction() override;
  bool ClearUnusedUserPrediction() override;

  absl::Status ReloadModules(std::unique_ptr<engine::Modules> modules,
                             bool is_mobile);

  absl::string_view GetDataVersion() const override {
    return converter_ ? converter_->modules()->GetDataManager().GetDataVersion()
                      : minimal_engine_.GetDataVersion();
  }

  // Returns a list of part-of-speech (e.g. "名詞", "動詞一段") to be used for
  // the user dictionary GUI tool.
  // Since the POS set may differ per LM, this function returns
  // available POS items. In practice, the POS items are rarely changed.
  std::vector<std::string> GetPosList() const override {
    return converter_ && converter_->modules()->GetUserDictionary()
               ? converter_->modules()->GetUserDictionary()->GetPosList()
               : minimal_engine_.GetPosList();
  }

  // For testing only.
  engine::Modules *GetModulesForTesting() const {
    return converter_->modules();
  }

  // Maybe reload a new data manager. Returns true if reloaded.
  bool MaybeReloadEngine(EngineReloadResponse *response) override;
  bool SendEngineReloadRequest(const EngineReloadRequest &request) override;
  bool SendSupplementalModelReloadRequest(
      const EngineReloadRequest &request) override;

  void SetAlwaysWaitForTesting(bool value) { always_wait_for_testing_ = value; }

 private:
  Engine() = default;

  // Initializes the engine object by the given modules and is_mobile flag.
  // The is_mobile flag is used to select DefaultPredictor and MobilePredictor.
  absl::Status Init(std::unique_ptr<engine::Modules> modules, bool is_mobile);

  MinimalEngine minimal_engine_;
  DataLoader loader_;

  std::unique_ptr<engine::SupplementalModelInterface> supplemental_model_;
  std::unique_ptr<Converter> converter_;
  std::unique_ptr<DataLoader::Response> loader_response_;
  bool always_wait_for_testing_ = false;
};

}  // namespace mozc

#endif  // MOZC_ENGINE_ENGINE_H_
