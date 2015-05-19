// Copyright 2010-2013, Google Inc.
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

// Session manager of Mozc server.

#ifndef MOZC_SESSION_SESSION_HANDLER_H_
#define MOZC_SESSION_SESSION_HANDLER_H_

#include <map>
#include <string>
#include <utility>

#include "base/base.h"
#include "base/scoped_ptr.h"
#include "composer/table.h"
#include "session/common.h"
#include "session/session_handler_interface.h"
#include "storage/lru_cache.h"

// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

#if defined(OS_ANDROID) || defined(__native_client__)
// Session watch dog is not aviable from android mozc for now.
#define MOZC_DISABLE_SESSION_WATCHDOG
#endif  // OS_ANDROID || __native_client__

namespace mozc {
#ifndef MOZC_DISABLE_SESSION_WATCHDOG
class SessionWatchDog;
#else  // MOZC_DISABLE_SESSION_WATCHDOG
// Session watch dog is not aviable from android mozc for now.
// TODO(kkojima): Remove this guard after
// enabling session watch dog for android.
#endif  // MOZC_DISABLE_SESSION_WATCHDOG
class Stopwatch;

namespace commands {
class Command;
}  // namespace commands

namespace session {
class SessionFactoryInterface;
class SessionInterface;
class SessionObserverHandler;
class SessionObserverInterface;
}  // namespace session

namespace sync {
class SyncHandler;
}  // namespace sync

namespace user_dictionary {
class UserDictionarySessionHandler;
}  // namespace user_dictionary

class SessionHandler : public SessionHandlerInterface {
 public:
  SessionHandler();
  virtual ~SessionHandler();

  // Returns true if SessionHandle is available.
  virtual bool IsAvailable() const;

  virtual bool EvalCommand(commands::Command *command);

  // Starts watch dog timer to cleanup sessions.
  virtual bool StartWatchDog();

  // NewSession returns new Sessoin.
  // Client needs to delete it properly
  session::SessionInterface *NewSession();

  virtual void AddObserver(session::SessionObserverInterface *observer);

  virtual void SetSyncHandler(sync::SyncHandler *sync_handler);

 private:
  FRIEND_TEST(SessionHandlerTest, StorageTest);

  typedef mozc::storage::LRUCache<SessionID, session::SessionInterface*>
      SessionMap;
  typedef SessionMap::Element SessionElement;

  // Reload settings which are managed by SessionHandler
  void ReloadSession();
  // Reload the configurations on the current sessions.
  void ReloadConfig();

  bool CreateSession(commands::Command *command);
  bool DeleteSession(commands::Command *command);
  bool TestSendKey(commands::Command *command);
  bool SendKey(commands::Command *command);
  bool SendCommand(commands::Command *command);
  bool SyncData(commands::Command *command);
  bool ClearUserHistory(commands::Command *command);
  bool ClearUserPrediction(commands::Command *command);
  bool ClearUnusedUserPrediction(commands::Command *command);
  bool Shutdown(commands::Command *command);
  bool Reload(commands::Command *command);
  bool GetStoredConfig(commands::Command *command);
  bool SetStoredConfig(commands::Command *command);
  bool SetImposedConfig(commands::Command *command);
  bool SetRequest(commands::Command *command);
  bool StartCloudSync(commands::Command *command);
  bool ClearCloudSync(commands::Command *command);
  bool GetCloudSyncStatus(commands::Command *command);
  bool AddAuthCode(commands::Command *command);

  bool InsertToStorage(commands::Command *command);
  bool ReadAllFromStorage(commands::Command *command);
  bool ClearStorage(commands::Command *command);
  bool Cleanup(commands::Command *command);
  bool SendUserDictionaryCommand(commands::Command *command);
  bool NoOperation(commands::Command *command);

  SessionID CreateNewSessionID();
  bool DeleteSessionID(SessionID id);

  scoped_ptr<SessionMap> session_map_;
#ifndef MOZC_DISABLE_SESSION_WATCHDOG
  scoped_ptr<SessionWatchDog> session_watch_dog_;
#else  // MOZC_DISABLE_SESSION_WATCHDOG
  // Session watch dog is not aviable from android mozc and nacl mozc for now.
  // TODO(kkojima): Remove this guard after
  // enabling session watch dog for android.
#endif  // MOZC_DISABLE_SESSION_WATCHDOG
  bool is_available_;
  uint32 max_session_size_;
  uint64 last_session_empty_time_;
  uint64 last_cleanup_time_;
  uint64 last_create_session_time_;

  session::SessionFactoryInterface *session_factory_;
  scoped_ptr<session::SessionObserverHandler> observer_handler_;
#ifdef ENABLE_CLOUD_SYNC
  sync::SyncHandler *sync_handler_;
#endif  // ENABLE_CLOUD_SYNC
  scoped_ptr<Stopwatch> stopwatch_;
  scoped_ptr<user_dictionary::UserDictionarySessionHandler>
      user_dictionary_session_handler_;
  scoped_ptr<composer::TableManager> table_manager_;
  scoped_ptr<commands::Request> request_;

  DISALLOW_COPY_AND_ASSIGN(SessionHandler);
};

}  // namespace mozc
#endif  // MOZC_SESSION_SESSION_HANDLER_H_
