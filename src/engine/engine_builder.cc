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

#include "engine/engine_builder.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/file_util.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/protobuf/message.h"
#include "base/thread2.h"
#include "data_manager/data_manager.h"
#include "engine/engine.h"
#include "engine/engine_interface.h"
#include "protocol/engine_builder.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace mozc {
namespace {

uint64_t GetRequestHash(const EngineReloadRequest &request) {
  if (request.file_path().empty() || request.install_location().empty()) {
    return 0;
  }
  return Fingerprint(request.SerializeAsString());
}

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

EngineBuilder::~EngineBuilder() { Clear(); }

void EngineBuilder::PrepareAsync(const EngineReloadRequest &request,
                                 EngineReloadResponse *response) {
  *response->mutable_request() = request;

  // Skips the sync when loading the model with the same path.
  const auto model_path_fp = model_path_fp_.load();
  if (model_path_fp != 0 && model_path_fp == GetRequestHash(request)) {
    LOG(INFO)
        << "PrepareAsync is skipped because the same model is already loaded.";
    response->set_status(EngineReloadResponse::RELOADED);
    return;
  }

  if (prepare_.has_value()) {
    if (!prepare_->Ready()) {
      response->set_status(EngineReloadResponse::ALREADY_RUNNING);
      return;
    }
    VLOG(1) << "Previously loaded data is discarded";
  }

  prepare_.emplace([request]() -> Prepared {
    EngineReloadResponse response;
    *response.mutable_request() = request;

    auto init_data_manager = [](const EngineReloadRequest &request,
                                DataManager *data_manager) {
      if (request.has_magic_number()) {
        return data_manager->InitFromFile(request.file_path(),
                                          request.magic_number());
      }
      return data_manager->InitFromFile(request.file_path());
    };

    auto data_manager = std::make_unique<DataManager>();
    if (DataManager::Status status =
            init_data_manager(request, data_manager.get());
        status != DataManager::Status::OK) {
      LOG(ERROR) << "Failed to load data [" << status << "] "
                 << protobuf::Utf8Format(request);
      response.set_status(ConvertStatus(status));
      return {std::move(response), std::move(data_manager)};
    }

    if (request.has_install_location()) {
      if (absl::Status s = FileUtil::LinkOrCopyFile(request.file_path(),
                                                    request.install_location());
          !s.ok()) {
        LOG(ERROR) << "Copy faild: " << protobuf::Utf8Format(request) << ": "
                   << s;
        response.set_status(EngineReloadResponse::INSTALL_FAILURE);
        return {
            std::move(response),
            std::move(data_manager),
        };
      }
    }

    response.set_status(EngineReloadResponse::RELOAD_READY);
    return {std::move(response), std::move(data_manager)};
  });
  response->set_status(EngineReloadResponse::ACCEPTED);
}

void EngineBuilder::Wait() {
  if (prepare_.has_value()) {
    prepare_->Wait();
  }
}

bool EngineBuilder::HasResponse() const {
  return prepare_.has_value() && prepare_->Ready();
}

void EngineBuilder::GetResponse(EngineReloadResponse *response) const {
  if (!HasResponse()) {
    return;
  }
  *response = prepare_->Get().response;
}

std::unique_ptr<EngineInterface> EngineBuilder::BuildFromPreparedData() {
  if (!HasResponse() || prepare_->Get().data_manager == nullptr ||
      prepare_->Get().response.status() != EngineReloadResponse::RELOAD_READY) {
    LOG(ERROR) << "Build() is called in invalid state";
    return nullptr;
  }
  // operator-> doesn't support rvalue delegation :/
  Prepared prepared = (*std::move(prepare_)).Get();
  prepare_.reset();

  absl::StatusOr<std::unique_ptr<Engine>> engine;
  switch (prepared.response.request().engine_type()) {
    case EngineReloadRequest::DESKTOP:
      engine = Engine::CreateDesktopEngine(std::move(prepared.data_manager));
      break;
    case EngineReloadRequest::MOBILE:
      engine = Engine::CreateMobileEngine(std::move(prepared.data_manager));
      break;
    default:
      LOG(DFATAL) << "Should not reach here";
      break;
  }

  if (!engine.ok()) {
    LOG(ERROR) << engine.status();
    return nullptr;
  }

  model_path_fp_.store(GetRequestHash(prepared.response.request()));

  return *std::move(engine);
}

void EngineBuilder::Clear() {
  if (!prepare_.has_value()) {
    return;
  }
  prepare_->Wait();
  prepare_.reset();
}

}  // namespace mozc
