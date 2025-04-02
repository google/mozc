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

// Mock of IPCClientFactoryInterface and IPCClientInterface for unittesting.

#include "ipc/ipc_mock.h"

#include <cstdint>
#include <memory>
#include <string>

#include "absl/time/time.h"
#include "base/strings/zstring_view.h"
#include "base/version.h"
#include "ipc/ipc.h"

namespace mozc {

IPCClientMock::IPCClientMock(IPCClientFactoryMock *caller)
    : caller_(caller),
      connected_(false),
      server_protocol_version_(0),
      server_product_version_(Version::GetMozcVersion()),
      server_process_id_(0),
      result_(false) {}

bool IPCClientMock::Connected() const { return connected_; }

uint32_t IPCClientMock::GetServerProtocolVersion() const {
  return server_protocol_version_;
}

const std::string &IPCClientMock::GetServerProductVersion() const {
  return server_product_version_;
}

uint32_t IPCClientMock::GetServerProcessId() const {
  return server_process_id_;
}

bool IPCClientMock::Call(const std::string &request, std::string *response,
                         const absl::Duration timeout) {
  caller_->SetGeneratedRequest(request);
  if (!connected_ || !result_) {
    return false;
  }
  response->assign(response_);
  return true;
}

IPCClientFactoryMock::IPCClientFactoryMock()
    : connection_(false),
      result_(false),
      server_protocol_version_(IPC_PROTOCOL_VERSION) {}

std::unique_ptr<IPCClientInterface> IPCClientFactoryMock::NewClient(
    zstring_view unused_name, zstring_view path_name) {
  return NewClientMock();
}

std::unique_ptr<IPCClientInterface> IPCClientFactoryMock::NewClient(
    zstring_view unused_name) {
  return NewClientMock();
}

const std::string &IPCClientFactoryMock::GetGeneratedRequest() const {
  return request_;
}

void IPCClientFactoryMock::SetGeneratedRequest(const std::string &request) {
  request_ = request;
}

void IPCClientFactoryMock::SetMockResponse(const std::string &response) {
  response_ = response;
}

void IPCClientFactoryMock::SetConnection(const bool connection) {
  connection_ = connection;
}

void IPCClientFactoryMock::SetResult(const bool result) { result_ = result; }

void IPCClientFactoryMock::SetServerProtocolVersion(
    const uint32_t server_protocol_version) {
  server_protocol_version_ = server_protocol_version;
}

void IPCClientFactoryMock::SetServerProductVersion(
    const std::string &server_product_version) {
  server_product_version_ = server_product_version;
}

void IPCClientFactoryMock::SetServerProcessId(
    const uint32_t server_process_id) {
  server_process_id_ = server_process_id;
}

std::unique_ptr<IPCClientMock> IPCClientFactoryMock::NewClientMock() {
  auto client = std::make_unique<IPCClientMock>(this);
  client->set_connection(connection_);
  client->set_result(result_);
  client->set_response(response_);
  client->set_server_protocol_version(server_protocol_version_);
  client->set_server_product_version(server_product_version_);
  return client;
}

}  // namespace mozc
