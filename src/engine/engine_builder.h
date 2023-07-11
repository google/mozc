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

#ifndef MOZC_ENGINE_ENGINE_BUILDER_H_
#define MOZC_ENGINE_ENGINE_BUILDER_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>

#include "base/thread2.h"
#include "data_manager/data_manager.h"
#include "engine/engine_builder_interface.h"
#include "engine/engine_interface.h"
#include "protocol/engine_builder.pb.h"

namespace mozc {

class EngineBuilder : public EngineBuilderInterface {
 public:
  EngineBuilder() = default;
  EngineBuilder(const EngineBuilder &) = delete;
  EngineBuilder &operator=(const EngineBuilder &) = delete;
  ~EngineBuilder() override;

  // Implementation of EngineBuilderInterface.  PrepareAsync() is implemented
  // using Thread.
  void PrepareAsync(const EngineReloadRequest &request,
                    EngineReloadResponse *response) override;
  bool HasResponse() const override;
  void GetResponse(EngineReloadResponse *response) const override;
  std::unique_ptr<EngineInterface> BuildFromPreparedData() override;
  void Clear() override;

  // Waits for internal thread to complete.
  void Wait();

 private:
  std::atomic<uint64_t> model_path_fp_ = 0;

  struct Prepared {
    EngineReloadResponse response;
    std::unique_ptr<DataManager> data_manager;
  };
  std::optional<BackgroundFuture<Prepared>> prepare_;
};

}  // namespace mozc

#endif  // MOZC_ENGINE_ENGINE_BUILDER_H_
