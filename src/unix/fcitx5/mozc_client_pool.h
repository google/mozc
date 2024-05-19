// Copyright 2023-2023, Weng Xuetian <wengxt@gmail.com>
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

#ifndef UNIX_FCITX5_MOZC_CLIENT_POOL_H_
#define UNIX_FCITX5_MOZC_CLIENT_POOL_H_

#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "client/client_interface.h"
#include "unix/fcitx5/mozc_connection.h"

namespace fcitx {

class MozcClientPool;

class MozcClientHolder {
  friend class MozcClientPool;

 public:
  MozcClientHolder() = default;

  MozcClientHolder(MozcClientHolder &&) = delete;

  ~MozcClientHolder();

  mozc::client::ClientInterface *client() const { return client_.get(); }

 private:
  MozcClientPool *pool_;
  std::unique_ptr<mozc::client::ClientInterface> client_;
  std::string key_;
};

class MozcClientPool {
  friend class MozcClientHolder;

 public:
  MozcClientPool(MozcConnection *connection,
                 PropertyPropagatePolicy initialPolicy);

  void setPolicy(PropertyPropagatePolicy policy);
  PropertyPropagatePolicy policy() const { return policy_; }

  std::shared_ptr<MozcClientHolder> requestClient(InputContext *ic);

  MozcConnection *connection() const { return connection_; }

 private:
  void registerClient(const std::string &key,
                      std::shared_ptr<MozcClientHolder> client);
  void unregisterClient(const std::string &key);
  MozcConnection *connection_;
  PropertyPropagatePolicy policy_;
  std::unordered_map<std::string, std::weak_ptr<MozcClientHolder>> clients_;
};

}  // namespace fcitx

#endif
