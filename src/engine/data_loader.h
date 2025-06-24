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

#ifndef MOZC_ENGINE_DATA_LOADER_H_
#define MOZC_ENGINE_DATA_LOADER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "base/thread.h"
#include "engine/modules.h"
#include "protocol/engine_builder.pb.h"

namespace mozc {

// DataLoader receives requests for loading language model data and loads the
// data from the top priority request. The language model data is asynchronously
// loaded in a sub-thread. `StartNewDataBuildTask` accepts the request and
// returns immediately. Once the model is loaded, the UpdateClalback is called.
// Note that the callback is executed in different thread (not a caller's
// thread)
class DataLoader {
 public:
  DataLoader() = default;
  DataLoader(const DataLoader &) = delete;
  DataLoader &operator=(const DataLoader &) = delete;
  virtual ~DataLoader();

  struct Response {
    EngineReloadResponse response;
    std::unique_ptr<engine::Modules> modules;
  };

  using ReloadedCallback =
      std::function<absl::Status(std::unique_ptr<Response> response)>;

  // Start a new data build and reload task. Returns true the `request` is
  // accepted. This method returns immediately. Actual data-loading task is
  // executed asynchronously in a different thread. When the new module is
  // loaded successfully, `callback` is called to pass the ownership of the new
  // modules from the loader to caller. Note that `callback` is also executed in
  // a different thread asynchronously. `callback` is not called when
  // the data-loading failed.
  bool StartNewDataBuildTask(const EngineReloadRequest &request,
                             ReloadedCallback callback);

  // Waits for loading thread.
  void Wait() const;

  // Returns true if the loading thread is running.
  bool IsRunning() const;

  // Disables specific handling for high priority data.
  void NotifyHighPriorityDataRegisteredForTesting() {
    high_priority_data_registered_.Notify();
  }

 private:
  struct RequestData {
    uint64_t id = 0;           // Fingerprint of request.
    uint32_t sequence_id = 0;  // Sequential id.
    EngineReloadRequest request;

    template <typename Sink>
    friend void AbslStringify(Sink &sink, const RequestData &p) {
      absl::Format(&sink, "id=%d priority = %d sequence_id=%d file_path=%s",
                   p.id, p.request.priority(),
                   p.sequence_id, p.request.file_path());
    }
  };

  // Builds new response from `request_data`.
  std::unique_ptr<Response> BuildResponse(const RequestData &request_data);

  // Accepts engine reload request and immediately returns whether
  // the `request` is accepted or not.
  bool RegisterRequest(const EngineReloadRequest &request);

  // Returns the RequestData to be processed. Return std::nullopt when
  // no request should be processed.
  std::optional<RequestData> GetPendingRequestData() const;

  // Returns the request id associated with the request.
  uint64_t GetRequestId(const EngineReloadRequest &request) const;

  // Unregister the request.
  void ReportLoadFailure(const RequestData &request_data);

  // Register the request.
  void ReportLoadSuccess(const RequestData &request_data);

  void StartReloadLoop(DataLoader::ReloadedCallback callback);

  // The internal data are accessed by the main thread and loader's thread
  // so need to protect them via Mutex.
  mutable absl::Mutex mutex_;

  absl::flat_hash_set<uint64_t> unregistered_ ABSL_GUARDED_BY(mutex_);
  std::vector<RequestData> requests_ ABSL_GUARDED_BY(mutex_);

  // Id of the request for the current data.
  // 0 means that no data has been updated yet.
  uint64_t current_request_id_ ABSL_GUARDED_BY(mutex_) = 0;

  // Sequential counter assigned to RequestData.
  // When the priority is the same, the larger sequence_id is preferred,
  // meaning that the model registered later is preferred.
  uint32_t sequence_id_ ABSL_GUARDED_BY(mutex_) = 0;

  // Notify when a new high priority data is registered.
  absl::Notification high_priority_data_registered_;

  std::optional<BackgroundFuture<void>> load_;
};

}  // namespace mozc

#endif  // MOZC_ENGINE_DATA_LOADER_H_
