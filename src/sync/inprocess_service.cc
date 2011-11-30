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

#include "sync/inprocess_service.h"

#include <algorithm>
#include <time.h>
#include <stdlib.h>
#include <string>
#include "base/base.h"
#include "base/util.h"

namespace mozc {
namespace sync {

namespace {
template <typename Request, typename Response>
bool IsValidRequest(Request *request, Response *response) {
  DCHECK(request);
  DCHECK(response);
  if (request->version() != 1) {
    response->set_error(ime_sync::SYNC_VERSION_TOO_OLD);
    return false;
  }
  return true;
}
}  // anonymous namespace

InprocessService::InprocessService() : timestamp_(time(NULL)) {}

InprocessService::~InprocessService() {}

bool InprocessService::Upload(ime_sync::UploadRequest *request,
                              ime_sync::UploadResponse *response) {
  scoped_lock l(&mutex_);
  CHECK(request);
  CHECK(response);

  response->Clear();

  if (!IsValidRequest(request, response)) {
    return true;
  }

  for (size_t i = 0; i < request->items_size(); ++i) {
    const ime_sync::SyncItem &item = request->items(i);
    string key;
    item.key().SerializeToString(&key);
    data_[key] = make_pair(timestamp_, item);
    // timestamp is not a real time.
    ++timestamp_;
  }

  response->set_error(ime_sync::SYNC_OK);

  return true;
}

bool InprocessService::Download(ime_sync::DownloadRequest *request,
                                ime_sync::DownloadResponse *response) {
  scoped_lock l(&mutex_);
  CHECK(request);
  CHECK(response);

  response->Clear();

  if (!IsValidRequest(request, response)) {
    return true;
  }

  const uint64 last_download_timestamp = request->last_download_timestamp();

  uint64 timestamp = last_download_timestamp;
  for (map<string, pair<uint64, ime_sync::SyncItem> >::const_iterator it =
           data_.begin(); it != data_.end(); ++it) {
    if (it->second.first > last_download_timestamp) {
      response->add_items()->CopyFrom(it->second.second);
      timestamp = max(timestamp, it->second.first);
    }
  }

  response->set_download_timestamp(timestamp);
  response->set_error(ime_sync::SYNC_OK);

  return true;
}

bool InprocessService::Clear(ime_sync::ClearRequest *request,
                             ime_sync::ClearResponse *response) {
  scoped_lock l(&mutex_);
  CHECK(request);
  CHECK(response);

  response->Clear();

  if (!IsValidRequest(request, response)) {
    return true;
  }

  data_.clear();
  response->set_error(ime_sync::SYNC_OK);

  return true;
}
}  // sync
}  // mozc
