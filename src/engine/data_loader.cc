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
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/thread.h"
#include "data_manager/data_manager.h"
#include "engine/modules.h"
#include "protocol/engine_builder.pb.h"

namespace mozc {
namespace {
EngineReloadResponse::Status ConvertStatus(DataManager::Status status) {
  switch (status) {
    case DataManager::Status::OK:
      return EngineReloadResponse::RELOAD_READY;
    case DataManager::Status::ENGINE_VERSION_MISMATCH:
      return EngineReloadResponse::ENGINE_VERSION_MISMATCH;
    case DataManager::Status::DATA_MISSING:
      return EngineReloadResponse::DATA_MISSING;
    case DataManager::Status::DATA_BROKEN:
      return EngineReloadResponse::DATA_BROKEN;
    case DataManager::Status::MMAP_FAILURE:
      return EngineReloadResponse::MMAP_FAILURE;
    case DataManager::Status::UNKNOWN:
      return EngineReloadResponse::UNKNOWN_ERROR;
    default:
      LOG(DFATAL) << "Should not reach here";
  }
  return EngineReloadResponse::UNKNOWN_ERROR;
}
}  // namespace

uint64_t DataLoader::GetRequestId(const EngineReloadRequest &request) const {
  return Fingerprint(request.SerializeAsString());
}

uint64_t DataLoader::RegisterRequest(const EngineReloadRequest &request) {
  const uint64_t id = GetRequestId(request);

  // Already requesting the top priority request.
  // No need to register the same ID again.
  if (top_request_id_ == id) {
    DCHECK(!requests_.empty() && requests_.front().id == id);
    return top_request_id_;
  }

  // The request is invalid since it has already been unregistered.
  if (unregistered_.contains(id)) {
    DCHECK_EQ(top_request_id_, requests_.empty() ? 0 : requests_.front().id);
    return top_request_id_;
  }

  ++sequence_id_;

  auto it = std::find_if(requests_.begin(), requests_.end(),
                         [id](const RequestData &v) { return v.id == id; });
  if (it != requests_.end()) {
    it->sequence_id = sequence_id_;
  } else {
    requests_.emplace_back(
        RequestData{id, request.priority(), sequence_id_, request});
  }

  // Sorts the requests so requests[0] stores the request with
  // the highest priority.
  std::sort(requests_.begin(), requests_.end(),
            [](const RequestData &lhs, const RequestData &rhs) {
              return (lhs.priority < rhs.priority ||
                      (lhs.priority == rhs.priority &&
                       lhs.sequence_id > rhs.sequence_id));
            });

  top_request_id_ = requests_.front().id;
  return top_request_id_;
}

uint64_t DataLoader::ReportLoadFailure(uint64_t id) {
  const auto it =
      std::remove_if(requests_.begin(), requests_.end(),
                     [id](const RequestData &v) { return v.id == id; });
  if (it != requests_.end()) {
    requests_.erase(it, requests_.end());
    unregistered_.emplace(id);
  }

  // Update the top request ID from the remaining sorted requests.
  top_request_id_ = requests_.empty() ? 0 : requests_.front().id;
  return top_request_id_;
}

namespace {
DataLoader::Response BuildResponse(uint64_t id, EngineReloadRequest request) {
  DataLoader::Response result;
  result.id = id;
  result.response.set_status(EngineReloadResponse::DATA_MISSING);
  *result.response.mutable_request() = request;

  // Initializes DataManager
  auto data_manager = std::make_unique<DataManager>();
  {
    const DataManager::Status status =
        request.has_magic_number()
            ? data_manager->InitFromFile(request.file_path(),
                                         request.magic_number())
            : data_manager->InitFromFile(request.file_path());
    if (status != DataManager::Status::OK) {
      LOG(ERROR) << "Failed to load data [" << status << "] " << request;
      result.response.set_status(ConvertStatus(status));
      DCHECK_NE(result.response.status(), EngineReloadResponse::RELOAD_READY);
      return result;
    }
  }

  // Copies file to install location.
  if (request.has_install_location()) {
    const absl::Status s = FileUtil::LinkOrCopyFile(request.file_path(),
                                                    request.install_location());
    if (!s.ok()) {
      LOG(ERROR) << "Copy faild: " << request << ": " << s;
      result.response.set_status(EngineReloadResponse::INSTALL_FAILURE);
      return result;
    }
  }

  auto modules = std::make_unique<engine::Modules>();
  {
    const absl::Status status = modules->Init(std::move(data_manager));
    if (!status.ok()) {
      LOG(ERROR) << "Failed to load modules [" << status << "] " << request;
      result.response.set_status(EngineReloadResponse::DATA_BROKEN);
      return result;
    }
  }

  result.response.set_status(EngineReloadResponse::RELOAD_READY);

  // Stores modules.
  result.modules = std::move(modules);

  return result;
}
}  // namespace

DataLoader::ResponseFuture DataLoader::Build(uint64_t id) const {
  // Finds the request associated with `id`.
  const auto it =
      std::find_if(requests_.begin(), requests_.end(),
                   [id](const RequestData &v) { return v.id == id; });
  if (it == requests_.end()) {
    return DataLoader::ResponseFuture([id]() {
      Response response;
      response.id = id;
      response.response.set_status(EngineReloadResponse::DATA_MISSING);
      return response;
    });
  }

  EngineReloadRequest request = it->request;
  return DataLoader::ResponseFuture([id, request = std::move(request)]() {
    return BuildResponse(id, request);
  });
}

bool DataLoader::StartNewDataBuildTask() {
  // Checks if already using the highest priority data.
  if (current_request_id_ == top_request_id_ || top_request_id_ == 0) {
    return false;
  }

  if (loader_response_future_.has_value()) {
    // Checks if the currently loading task is the highest priority request.
    // Note, Get() may wait until loader_response_future_ is ready.
    if (loader_response_future_->Get().id == top_request_id_) {
      return true;
    }

    // Do not use this result as the new request is already queued.
    mozc::DataLoader::Response loader_response =
        (*std::move(loader_response_future_)).Get();
    loader_response_future_.reset();

    // If the response is invalid, report it as failure not to retry for the
    // same request in future.
    if (!loader_response.modules || loader_response.response.status() !=
                                        EngineReloadResponse::RELOAD_READY) {
      ReportLoadFailure(loader_response.id);
    }
  }

  LOG(INFO) << "Building a new module (current ID =" << current_request_id_
            << ", top ID =" << top_request_id_ << ")";
  loader_response_future_ = Build(top_request_id_);
  return true;
}

bool DataLoader::IsBuildResponseReady() const {
  return loader_response_future_.has_value() &&
         loader_response_future_->Ready();
}

std::unique_ptr<DataLoader::Response>
DataLoader::MaybeMoveDataLoaderResponse() {
  // Checks if an existing load process.
  if (!loader_response_future_) {
    return nullptr;
  }

  // Waits the loading if the no new data is loaded so far.
  if (current_request_id_ == 0 || always_wait_for_loader_response_future_) {
    loader_response_future_->Wait();
  }

  if (!loader_response_future_->Ready()) {
    return nullptr;
  }

  // Returns the new DataLoader::Response that is ready to use.
  mozc::DataLoader::Response loader_response =
      (*std::move(loader_response_future_)).Get();
  loader_response_future_.reset();

  return std::make_unique<DataLoader::Response>(std::move(loader_response));
}

void DataLoader::Clear() {
  requests_.clear();
  sequence_id_ = 0;
}

}  // namespace mozc
