// Copyright 2010-2011, Google Inc.
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

#ifndef MOZC_SYNC_SERVICE_INTERFACE_H_
#define MOZC_SYNC_SERVICE_INTERFACE_H_

#include "base/base.h"

namespace ime_sync {
class UploadRequest;
class UploadResponse;
class DownloadRequest;
class DownloadResponse;
class ClearRequest;
class ClearResponse;
}

namespace mozc {
namespace sync {

// Service is a high-level abstraction of network sync.
// We might want to provide several ways for network sync
// e.g., Gaia, Gdrive and Webdav. They can be implemented
// by inheriting the ServiceInterface class.
class ServiceInterface {
 public:
  ServiceInterface() {}
  virtual ~ServiceInterface() {}

  // request is not a const reference here.
  // This is because
  // - authentication message will be encoded in request,
  //   it depends on implementation.
  // - proto2::RpcChannel::Service stub accepts only
  //   non-const pointer.

  // Upload items encoded in request.
  virtual bool Upload(ime_sync::UploadRequest *request,
                      ime_sync::UploadResponse *response) = 0;

  // Download new items which should be synced
  virtual bool Download(ime_sync::DownloadRequest *request,
                        ime_sync::DownloadResponse *response) = 0;

  // Clear all items in the server.
  virtual bool Clear(ime_sync::ClearRequest *request,
                     ime_sync::ClearResponse *response) = 0;
};
}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_SERVICE_INTERFACE_H_
