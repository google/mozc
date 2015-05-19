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

#ifndef MOZC_SYNC_CONTACT_SYNCER_H_
#define MOZC_SYNC_CONTACT_SYNCER_H_

#include "base/base.h"
#include "dictionary/user_dictionary_storage.pb.h"
#include "net/jsoncpp.h"
#include "sync/oauth2_util.h"
#include "sync/syncer_interface.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {
namespace sync {

// ContactSyncer is a class for syncing up contact list information.
// Contact list is stored in Google data server.
class ContactSyncer : public SyncerInterface {
 public:
  ContactSyncer(OAuth2Util* oauth2_util);
  ~ContactSyncer() {}

  // Merge information
  virtual bool Start();

  // Download data from GData server
  virtual bool Sync(bool *reload_required);

  // Do nothing currently
  virtual bool Clear();

 private:
  FRIEND_TEST(ContactSyncerTest, Timestamp);
  FRIEND_TEST(ContactSyncerTest, Download);

  OAuth2Util* oauth2_util_;

  bool Download(user_dictionary::UserDictionaryStorage *storage,
                bool *reload_required);
  bool Upload();

  // Getter and setter for dictionary information
  string GetUserDictionaryFileName() const;
  bool GetLastDownloadTimestamp(string *time_stamp) const;
  void SetLastDownloadTimestamp(const string &time_stamp);
};
}  // namespace sync
}  // namespace mozc

#endif  // MOZC_SYNC_CONTACT_SYNCER_H_
