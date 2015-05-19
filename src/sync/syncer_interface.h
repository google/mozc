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

#ifndef MOZC_SYNC_SYNCER_INTERFACE_H_
#define MOZC_SYNC_SYNCER_INTERFACE_H_

#include "base/base.h"

namespace mozc {
namespace sync {
class OAuth2Util;

class SyncerInterface {
 public:
  SyncerInterface() {}
  virtual ~SyncerInterface() {}

  // Note that Start are called in the main converter thread.
  // We can implement a logic inside this method,
  // like preparing sync items.
  virtual bool Start() = 0;

  // Download/Upload items from/to the cloud.
  // Return true if Sync operation finishes successfully.
  // If there exist remote updates on the cloud and needs
  // to apply them to the current converter, set |*reload_required|
  // to be true.
  // Note that Sync will be executed out of the main converter thread.
  // Sync method must be thread-safe.
  virtual bool Sync(bool *reload_required) = 0;

  // Clear user data on the cloud.
  // Local data is not cleared with this method.
  // Return true if Sync operation finishes successfully.
  // Note that Clear will be executed out of the main converter thread.
  // Sync method must be thread-safe.
  virtual bool Clear() = 0;
};

class SyncerFactory {
 public:
  // return signleton object.
  static SyncerInterface *GetSyncer();

  // set syncer object for unittesting.
  static void SetSyncer(SyncerInterface *syncer);

  // set the authentication library which puts authentication to the
  // syncer channel.
  static void SetOAuth2(OAuth2Util *oauth2);
};
}  // sync
}  // mozc

#endif  // MOZC_SYNC_SYNCER_H_
