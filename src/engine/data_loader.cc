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

#include "engine/data_loader.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "base/hash.h"
#include "base/thread.h"
#include "data_manager/data_manager.h"
#include "engine/modules.h"
#include "protocol/engine_builder.pb.h"

namespace mozc {
namespace {
EngineReloadResponse::Status ConvertStatus(absl::Status status) {
  switch (status.code()) {
    case absl::StatusCode::kOk:
      return EngineReloadResponse::RELOAD_READY;
    case absl::StatusCode::kFailedPrecondition:
      return EngineReloadResponse::ENGINE_VERSION_MISMATCH;
    case absl::StatusCode::kNotFound:
      return EngineReloadResponse::DATA_MISSING;
    case absl::StatusCode::kDataLoss:
      return EngineReloadResponse::DATA_BROKEN;
    case absl::StatusCode::kPermissionDenied:
      return EngineReloadResponse::MMAP_FAILURE;
    default:
      break;
  }
  return EngineReloadResponse::UNKNOWN_ERROR;
}
}  // namespace

DataLoader::~DataLoader() { Wait(); }

uint64_t DataLoader::GetRequestId(const EngineReloadRequest &request) const {
  return Fingerprint(request.SerializeAsString());
}

std::optional<DataLoader::RequestData> DataLoader::GetPendingRequestData()
    const {
  absl::ReaderMutexLock lock(&mutex_);

  if (requests_.empty()) {
    return std::nullopt;
  }

  const RequestData &top = requests_.front();
  if (top.id == current_request_id_) {
    return std::nullopt;
  }

  return top;
}

bool DataLoader::RegisterRequest(const EngineReloadRequest &request) {
  absl::WriterMutexLock lock(&mutex_);

  const uint64_t id = GetRequestId(request);

  if (id == current_request_id_) {
    return false;
  }

  // The request is invalid since it has already been unregistered.
  if (unregistered_.contains(id)) {
    return false;
  }

  ++sequence_id_;

  auto it = std::find_if(requests_.begin(), requests_.end(),
                         [id](const RequestData &v) { return v.id == id; });
  if (it != requests_.end()) {
    it->sequence_id = sequence_id_;
  } else {
    requests_.emplace_back(RequestData{id, sequence_id_, request});
    LOG(INFO) << "New request is registered: " << requests_.back();
  }

  // Sorts the requests so requests[0] stores the request with
  // the highest priority.
  std::sort(requests_.begin(), requests_.end(),
            [](const RequestData &lhs, const RequestData &rhs) {
              return (lhs.request.priority() < rhs.request.priority() ||
                      (lhs.request.priority() == rhs.request.priority() &&
                       lhs.sequence_id > rhs.sequence_id));
            });

  // Needs the reloading process only when requests[0] is different from
  // current_request_id_.
  return current_request_id_ != requests_.front().id;
}

void DataLoader::ReportLoadFailure(const DataLoader::RequestData &request) {
  absl::WriterMutexLock lock(&mutex_);
  LOG(ERROR) << "Failed to load data: " << request;
  const uint64_t id = request.id;
  const auto it =
      std::remove_if(requests_.begin(), requests_.end(),
                     [id](const RequestData &v) { return v.id == id; });
  if (it != requests_.end()) {
    requests_.erase(it, requests_.end());
    unregistered_.emplace(id);
  }
}

void DataLoader::ReportLoadSuccess(const DataLoader::RequestData &request) {
  absl::WriterMutexLock lock(&mutex_);
  LOG(INFO) << "New data is loaded: " << request;
  current_request_id_ = request.id;
}

std::unique_ptr<DataLoader::Response> DataLoader::BuildResponse(
    const DataLoader::RequestData &request_data) {
  auto result = std::make_unique<DataLoader::Response>();
  result->response.set_status(EngineReloadResponse::DATA_MISSING);

  const EngineReloadRequest &request = request_data.request;
  *result->response.mutable_request() = request;

  // Initializes DataManager
  absl::StatusOr<std::unique_ptr<const DataManager>> data_manager =
      request.has_magic_number()
          ? DataManager::CreateFromFile(request.file_path(),
                                        request.magic_number())
          : DataManager::CreateFromFile(request.file_path());
  if (!data_manager.ok()) {
    LOG(ERROR) << "Failed to load data [" << data_manager << "] "
               << request_data;
    result->response.set_status(ConvertStatus(data_manager.status()));
    DCHECK_NE(result->response.status(), EngineReloadResponse::RELOAD_READY);
    return result;
  }

  absl::StatusOr<std::unique_ptr<engine::Modules>> modules =
      engine::Modules::Create(std::move(data_manager.value()));
  if (!modules.ok()) {
    LOG(ERROR) << "Failed to load modules [" << modules << "] " << request_data;
    result->response.set_status(EngineReloadResponse::DATA_BROKEN);
    return result;
  }

  result->response.set_status(EngineReloadResponse::RELOAD_READY);
  result->modules = std::move(modules.value());

  return result;
}

// StartReloadLoop is executed in loader's thread.
void DataLoader::StartReloadLoop(DataLoader::ReloadedCallback callback) {
  while (true) {
    std::optional<RequestData> request_data = GetPendingRequestData();

    // No pending request.
    if (!request_data.has_value()) {
      break;
    }

    // When the high priority data is not registered, waits at most kTimeout
    // until a new high priority data is registered. Retry the loop when a new
    // high priority data is registered while waiting.
    constexpr absl::Duration kTimeout = absl::Milliseconds(100);
    if (!high_priority_data_registered_.HasBeenNotified() &&
        high_priority_data_registered_.WaitForNotificationWithTimeout(
            kTimeout)) {
      continue;
    }

    LOG(INFO) << "Building a new module: " << *request_data;
    std::unique_ptr<Response> response = BuildResponse(*request_data);
    if (response->response.status() != EngineReloadResponse::RELOAD_READY) {
      ReportLoadFailure(*request_data);
      continue;
    }

    // Passes the modules to the engine via callback.
    absl::Status reload_status = callback(std::move(response));
    if (!reload_status.ok()) {
      ReportLoadFailure(*request_data);
      continue;
    }

    ReportLoadSuccess(*request_data);
  }
}

// This method is called only by the main engine thread.
bool DataLoader::StartNewDataBuildTask(const EngineReloadRequest &request,
                                       DataLoader::ReloadedCallback callback) {
  if (!RegisterRequest(request)) {
    return false;
  }

  // Receives high priority data.
  constexpr int kHighPriority = 10;
  if (!high_priority_data_registered_.HasBeenNotified() &&
      request.priority() <= kHighPriority) {
    high_priority_data_registered_.Notify();
  }

  if (!IsRunning()) {
    // Restarts StartReloadLoop from scratch when the thread is not running.
    // Needs to copy the `callback` as the callback is executed in other thread.
    load_.emplace([this, callback]() { StartReloadLoop(callback); });
  }

  return true;
}

void DataLoader::Wait() const {
  if (load_.has_value()) {
    load_->Wait();
  }
}

bool DataLoader::IsRunning() const {
  return load_.has_value() && !load_->Ready();
}

}  // namespace mozc
