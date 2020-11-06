// Copyright 2010-2020, Google Inc.
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

#include "engine/engine_builder.h"

#include <memory>
#include <utility>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/thread.h"
#include "data_manager/data_manager.h"
#include "engine/engine.h"
#include "protocol/engine_builder.pb.h"

namespace mozc {
namespace {

EngineReloadResponse::Status ConvertStatus(DataManager::Status status) {
  switch (status) {
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

class EngineBuilder::Preparator : public Thread {
 public:
  explicit Preparator(const EngineReloadRequest &request) {
    *response_.mutable_request() = request;
  }

  ~Preparator() override = default;

  void Run() override {
    const EngineReloadRequest &request = response_.request();

    std::unique_ptr<DataManager> tmp_data_manager(new DataManager());
    const DataManager::Status status =
        InitDataManager(request, tmp_data_manager.get());
    if (status != DataManager::Status::OK) {
      LOG(ERROR) << "Failed to load data [" << status << "] "
                 << request.Utf8DebugString();
      response_.set_status(ConvertStatus(status));
      return;
    }

    if (request.has_install_location() &&
        !FileUtil::AtomicRename(request.file_path(),
                                request.install_location())) {
      LOG(ERROR) << "Atomic rename faild: " << request.Utf8DebugString();
      response_.set_status(EngineReloadResponse::INSTALL_FAILURE);
      return;
    }

    response_.set_status(EngineReloadResponse::RELOAD_READY);
    data_manager_ = std::move(tmp_data_manager);
  }

 private:
  DataManager::Status InitDataManager(const EngineReloadRequest &request,
                                      DataManager *data_manager) {
    if (request.has_magic_number()) {
      return data_manager->InitFromFile(request.file_path(),
                                        request.magic_number());
    }
    return data_manager->InitFromFile(request.file_path());
  }

  friend class EngineBuilder;
  EngineReloadResponse response_;
  std::unique_ptr<DataManager> data_manager_;
};

EngineBuilder::EngineBuilder() = default;
EngineBuilder::~EngineBuilder() = default;

void EngineBuilder::PrepareAsync(const EngineReloadRequest &request,
                                 EngineReloadResponse *response) {
  *response->mutable_request() = request;
  if (preparator_) {
    if (preparator_->IsRunning()) {
      response->set_status(EngineReloadResponse::ALREADY_RUNNING);
      return;
    }
    preparator_->Join();
    VLOG(1) << "Previously loaded data is discarded";
  }
  preparator_.reset(new Preparator(request));
  preparator_->SetJoinable(true);
  preparator_->Start("EngineBuilder::Preparator");
  response->set_status(EngineReloadResponse::ACCEPTED);
}

void EngineBuilder::Wait() {
  if (preparator_) {
    preparator_->Join();
  }
}

bool EngineBuilder::HasResponse() const {
  return preparator_ && !preparator_->IsRunning();
}

void EngineBuilder::GetResponse(EngineReloadResponse *response) const {
  if (!HasResponse()) {
    return;
  }
  *response = preparator_->response_;
}

std::unique_ptr<EngineInterface> EngineBuilder::BuildFromPreparedData() {
  if (!HasResponse() || !preparator_->data_manager_ ||
      preparator_->response_.status() != EngineReloadResponse::RELOAD_READY) {
    LOG(ERROR) << "Build() is called in invalid state";
    return nullptr;
  }
  mozc::StatusOr<std::unique_ptr<Engine>> engine;
  switch (preparator_->response_.request().engine_type()) {
    case EngineReloadRequest::DESKTOP:
      engine =
          Engine::CreateDesktopEngine(std::move(preparator_->data_manager_));
      break;
    case EngineReloadRequest::MOBILE:
      engine =
          Engine::CreateMobileEngine(std::move(preparator_->data_manager_));
      break;
    default:
      LOG(DFATAL) << "Should not reach here";
      break;
  }

  if (!engine.ok()) {
    LOG(ERROR) << engine.status();
    return nullptr;
  }
  return *std::move(engine);
}

void EngineBuilder::Clear() {
  if (!preparator_) {
    return;
  }
  preparator_->Join();
  preparator_.reset();
}

}  // namespace mozc
