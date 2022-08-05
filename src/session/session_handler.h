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

// Session manager of Mozc server.

#ifndef MOZC_SESSION_SESSION_HANDLER_H_
#define MOZC_SESSION_SESSION_HANDLER_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/port.h"
#include "composer/table.h"
#include "engine/engine_builder_interface.h"
#include "engine/engine_interface.h"
#include "session/common.h"
#include "session/session_handler_interface.h"
#include "storage/lru_cache.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {

#ifndef MOZC_DISABLE_SESSION_WATCHDOG
class SessionWatchDog;
#endif  // MOZC_DISABLE_SESSION_WATCHDOG
class Stopwatch;

namespace commands {
class Command;
class Request;
}  // namespace commands

namespace session {
class SessionInterface;
class SessionObserverHandler;
class SessionObserverInterface;
}  // namespace session

namespace user_dictionary {
class UserDictionarySessionHandler;
}  // namespace user_dictionary

class SessionHandler : public SessionHandlerInterface {
 public:
  explicit SessionHandler(std::unique_ptr<EngineInterface> engine);
  SessionHandler(std::unique_ptr<EngineInterface> engine,
                 std::unique_ptr<EngineBuilderInterface> engine_builder);
  ~SessionHandler() override;

  // Returns true if SessionHandle is available.
  bool IsAvailable() const override;

  bool EvalCommand(commands::Command *command) override;

  // Starts watch dog timer to cleanup sessions.
  bool StartWatchDog() override;

  // NewSession returns new Session.
  // Client needs to delete it properly
  session::SessionInterface *NewSession();

  void AddObserver(session::SessionObserverInterface *observer) override;
  absl::string_view GetDataVersion() const override {
    return engine_->GetDataVersion();
  }

  const EngineInterface &engine() const { return *engine_; }

 private:
  FRIEND_TEST(SessionHandlerTest, StorageTest);

  using SessionMap =
      mozc::storage::LruCache<SessionID, session::SessionInterface *>;
  using SessionElement = SessionMap::Element;

  void Init(std::unique_ptr<EngineInterface> engine,
            std::unique_ptr<EngineBuilderInterface> engine_builder);

  // Sets config to all the modules managed by this handler.  This does not
  // affect the stored config in the local storage.
  void SetConfig(const config::Config &config);
  // Updates the stored config, if the |command| contains the config.
  void MaybeUpdateStoredConfig(commands::Command *command);

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

  bool Cleanup(commands::Command *command);
  bool SendUserDictionaryCommand(commands::Command *command);
  bool SendEngineReloadRequest(commands::Command *command);
  bool NoOperation(commands::Command *command);
  bool CheckSpelling(commands::Command *command);
  bool ReloadSpellChecker(commands::Command *command);

  SessionID CreateNewSessionID();
  bool DeleteSessionID(SessionID id);

  std::unique_ptr<SessionMap> session_map_;
#ifndef MOZC_DISABLE_SESSION_WATCHDOG
  std::unique_ptr<SessionWatchDog> session_watch_dog_;
#endif  // MOZC_DISABLE_SESSION_WATCHDOG
  bool is_available_ = false;
  uint32_t max_session_size_ = 0;
  uint64_t last_session_empty_time_ = 0;
  uint64_t last_cleanup_time_ = 0;
  uint64_t last_create_session_time_ = 0;

  std::unique_ptr<EngineInterface> engine_;
  std::unique_ptr<EngineBuilderInterface> engine_builder_;
  std::unique_ptr<session::SessionObserverHandler> observer_handler_;
  std::unique_ptr<Stopwatch> stopwatch_;
  std::unique_ptr<user_dictionary::UserDictionarySessionHandler>
      user_dictionary_session_handler_;
  std::unique_ptr<composer::TableManager> table_manager_;
  std::unique_ptr<const commands::Request> request_;
  std::unique_ptr<const config::Config> config_;

  DISALLOW_COPY_AND_ASSIGN(SessionHandler);
};

}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_HANDLER_H_
