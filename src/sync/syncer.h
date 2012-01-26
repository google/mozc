// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_SYNC_SYNCER_H_
#define MOZC_SYNC_SYNCER_H_

#include <map>
#include "base/base.h"
// for FRIEND_TEST()
#include "sync/syncer_interface.h"
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace sync {
class AdapterInterface;
class ServiceInterface;

// Syncer is a class for managing Service and
// Adapter, dispatching adapters request to service.
class Syncer : public SyncerInterface {
 public:
  explicit Syncer(ServiceInterface *service);
  virtual ~Syncer();

  bool RegisterAdapter(AdapterInterface *adapter);

  virtual bool Start();

  virtual bool Sync(bool *reload_required);
  virtual bool Clear();

 private:
  FRIEND_TEST(SyncerTest, Timestamp);
  FRIEND_TEST(SyncerTest, Clear);
  FRIEND_TEST(SyncerTest, Download);
  FRIEND_TEST(SyncerTest, Upload);
  FRIEND_TEST(SyncerTest, CheckConfig);

  bool Download(bool *reload_required);
  bool Upload();

  uint64 GetLastDownloadTimestamp() const;
  void SetLastDownloadTimestamp(uint64 value);

  typedef map<uint32, AdapterInterface*> AdapterMap;
  ServiceInterface *service_;
  AdapterMap adapters_;
};
}  // sync
}  // mozc

#endif  // MOZC_SYNC_SYNCER_H_
