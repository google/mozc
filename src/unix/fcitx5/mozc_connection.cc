// Copyright 2010-2012, Google Inc.
// Copyright 2012~2013, Weng Xuetian <wengxt@gmail.com>
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

#include "unix/fcitx5/mozc_connection.h"

#include <memory>

#include "base/vlog.h"
#include "client/client.h"
#include "ipc/ipc.h"
#include "protocol/commands.pb.h"

namespace fcitx {

std::unique_ptr<mozc::client::ClientInterface> CreateAndConfigureClient() {
  auto client = mozc::client::ClientFactory::NewClient();
  // Currently client capability is fixed.
  mozc::commands::Capability capability;
  capability.set_text_deletion(
      mozc::commands::Capability::DELETE_PRECEDING_TEXT);
  client->set_client_capability(capability);
  return client;
}

MozcConnection::MozcConnection()
    : client_factory_(mozc::IPCClientFactory::GetIPCClientFactory()) {
  MOZC_VLOG(1) << "MozcConnection is created";
}

MozcConnection::~MozcConnection() {
  MOZC_VLOG(1) << "MozcConnection is destroyed";
}

std::unique_ptr<mozc::client::ClientInterface> MozcConnection::CreateClient() {
  auto client = CreateAndConfigureClient();
  client->SetIPCClientFactory(client_factory_);
  return client;
}

}  // namespace fcitx
