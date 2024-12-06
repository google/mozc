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
#include "converter/immutable_converter_interface.h"
#include "data_manager/data_manager_interface.h"
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
      std::unique_ptr<const DataManagerInterface> data_manager);

  // Helper function for the above factory, where data manager is instantiated
  // by a default constructor.  Intended to be used for OssDataManager etc.
  template <typename DataManagerType>
  static absl::StatusOr<std::unique_ptr<Engine>> CreateDesktopEngineHelper() {
    return CreateDesktopEngine(std::make_unique<const DataManagerType>());
  }

  // Creates an instance with mobile configuration from a data manager.  The
  // ownership of data manager is passed to the engine instance.
  static absl::StatusOr<std::unique_ptr<Engine>> CreateMobileEngine(
      std::unique_ptr<const DataManagerInterface> data_manager);

  // Helper function for the above factory, where data manager is instantiated
  // by a default constructor.  Intended to be used for OssDataManager etc.
  template <typename DataManagerType>
  static absl::StatusOr<std::unique_ptr<Engine>> CreateMobileEngineHelper() {
    return CreateMobileEngine(std::make_unique<const DataManagerType>());
  }

  // Creates an instance with the given modules and is_mobile flag.
  static absl::StatusOr<std::unique_ptr<Engine>> CreateEngine(
      std::unique_ptr<engine::Modules> modules, bool is_mobile);

  // Creates an engine with no initialization.
  static std::unique_ptr<Engine> CreateEngine();

  Engine(const Engine &) = delete;
  Engine &operator=(const Engine &) = delete;

  ConverterInterface *GetConverter() const override {
    return initialized_ ? converter_.get() : minimal_engine_.GetConverter();
  }
  absl::string_view GetPredictorName() const override {
    if (initialized_) {
      return predictor_ ? predictor_->GetPredictorName() : absl::string_view();
    } else {
      return minimal_engine_.GetPredictorName();
    }
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
    return GetDataManager()->GetDataVersion();
  }

  const DataManagerInterface *GetDataManager() const {
    return initialized_ ? &modules_->GetDataManager()
                        : minimal_engine_.GetDataManager();
  }

  // Returns a list of part-of-speech (e.g. "名詞", "動詞一段") to be used for
  // the user dictionary GUI tool.
  // Since the POS set may differ per LM, this function returns
  // available POS items. In practice, the POS items are rarely changed.
  std::vector<std::string> GetPosList() const override {
    return initialized_ ? modules_->GetUserDictionary()->GetPosList()
                        : minimal_engine_.GetPosList();
  }

  // For testing only.
  engine::Modules *GetModulesForTesting() const { return modules_.get(); }

  // Maybe reload a new data manager. Returns true if reloaded.
  bool MaybeReloadEngine(EngineReloadResponse *response) override;
  bool SendEngineReloadRequest(const EngineReloadRequest &request) override;
  bool SendSupplementalModelReloadRequest(
      const EngineReloadRequest &request) override;

  void SetDataLoaderForTesting(std::unique_ptr<DataLoader> loader) {
    loader_ = std::move(loader);
  }
  void SetAlwaysWaitForLoaderResponseFutureForTesting(bool value) {
    loader_->SetAlwaysWaitForLoaderResponseFutureForTesting(value);
  }

 private:
  Engine();

  // Initializes the engine object by the given modules and is_mobile flag.
  // The is_mobile flag is used to select DefaultPredictor and MobilePredictor.
  absl::Status Init(std::unique_ptr<engine::Modules> modules, bool is_mobile);

  // If initialized_ is false, minimal_engine_ is used as a fallback engine.
  bool initialized_ = false;
  MinimalEngine minimal_engine_;

  std::unique_ptr<DataLoader> loader_;
  std::unique_ptr<engine::Modules> modules_;
  std::unique_ptr<ImmutableConverterInterface> immutable_converter_;
  std::unique_ptr<engine::SupplementalModelInterface> supplemental_model_;

  // TODO(noriyukit): Currently predictor and rewriter are created by this class
  // but owned by converter_. Since this class creates these two, it'd be better
  // if Engine class owns these two instances.
  prediction::PredictorInterface *predictor_ = nullptr;
  RewriterInterface *rewriter_ = nullptr;

  std::unique_ptr<Converter> converter_;
};

}  // namespace mozc

#endif  // MOZC_ENGINE_ENGINE_H_
