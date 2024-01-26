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

// Session Handler of Mozc server.
// Migrated from ipc/interpreter and session/session_manager

#include "session/session_handler.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/random/random.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "base/logging.h"
#include "base/protobuf/message.h"
#include "base/stopwatch.h"
#include "base/vlog.h"
#include "composer/table.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "dictionary/user_dictionary_session_handler.h"
#include "engine/engine_builder.h"
#include "engine/engine_interface.h"
#include "engine/user_data_manager_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "protocol/engine_builder.pb.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "session/common.h"
#include "session/internal/keymap.h"
#include "session/session.h"
#include "session/session_interface.h"
#include "session/session_observer_handler.h"
#include "session/session_observer_interface.h"
#include "usage_stats/usage_stats.h"

#ifndef MOZC_DISABLE_SESSION_WATCHDOG
#include "base/process.h"
#include "session/session_watch_dog.h"
#endif  // MOZC_DISABLE_SESSION_WATCHDOG


// TODO(b/275437228): Convert this to use `absl::Duration`. Note that existing
// clients assume a negative value means we do not timeout at all.
ABSL_FLAG(int32_t, timeout, -1,
          "server timeout. "
          "if sessions get empty for \"timeout\", "
          "shutdown message is automatically emitted");

ABSL_FLAG(int32_t, max_session_size, 64,
          "maximum sessions size. "
          "if size of sessions reaches to \"max_session_size\", "
          "oldest session is removed");

// TODO(b/275437228): Convert this to `absl::Duration`.
ABSL_FLAG(int32_t, create_session_min_interval, 0,
          "minimum interval (sec) for create session");

// TODO(b/275437228): Convert this to `absl::Duration`.
ABSL_FLAG(int32_t, watch_dog_interval, 180, "watch dog timer intaval (sec)");

// TODO(b/275437228): Convert this to `absl::Duration`.
ABSL_FLAG(int32_t, last_command_timeout, 3600,
          "remove session if it is not accessed for "
          "\"last_command_timeout\" sec");

// TODO(b/275437228): Convert this to `absl::Duration`.
ABSL_FLAG(int32_t, last_create_session_timeout, 300,
          "remove session if it is not accessed for "
          "\"last_create_session_timeout\" sec "
          "after create session command");

ABSL_FLAG(bool, restricted, false, "Launch server with restricted setting");

namespace mozc {
namespace {

using mozc::usage_stats::UsageStats;

bool IsApplicationAlive(const session::SessionInterface *session) {
#ifndef MOZC_DISABLE_SESSION_WATCHDOG
  const commands::ApplicationInfo &info = session->application_info();
  // When the thread/process's current status is unknown, i.e.,
  // if IsThreadAlive/IsProcessAlive functions failed to know the
  // status of the thread/process, return true just in case.
  // Here, we want to kill the session only when the target thread/process
  // are terminated with 100% probability. Otherwise, it's better to do
  // nothing to prevent any side effects.
#ifdef _WIN32
  if (info.has_thread_id()) {
    return Process::IsThreadAlive(static_cast<size_t>(info.thread_id()), true);
  }
#else   // _WIN32
  if (info.has_process_id()) {
    return Process::IsProcessAlive(static_cast<size_t>(info.process_id()),
                                   true);
  }
#endif  // _WIN32
#endif  // MOZC_DISABLE_SESSION_WATCHDOG
  return true;
}
}  // namespace

SessionHandler::SessionHandler(std::unique_ptr<EngineInterface> engine) {
  Init(std::move(engine), std::unique_ptr<EngineBuilder>());
}

SessionHandler::SessionHandler(std::unique_ptr<EngineInterface> engine,
                               std::unique_ptr<EngineBuilder> engine_builder) {
  Init(std::move(engine), std::move(engine_builder));
}

void SessionHandler::Init(std::unique_ptr<EngineInterface> engine,
                          std::unique_ptr<EngineBuilder> engine_builder) {
  is_available_ = false;
  max_session_size_ = 0;
  last_session_empty_time_ = Clock::GetAbslTime();
  last_cleanup_time_ = absl::InfinitePast();
  last_create_session_time_ = absl::InfinitePast();
  engine_ = std::move(engine);
  engine_builder_ = std::move(engine_builder);
  observer_handler_ = std::make_unique<session::SessionObserverHandler>();
  user_dictionary_session_handler_ =
      std::make_unique<user_dictionary::UserDictionarySessionHandler>();
  table_manager_ = std::make_unique<composer::TableManager>();
  request_ = std::make_unique<commands::Request>();
  config_ = config::ConfigHandler::GetConfig();
  key_map_manager_ = std::make_unique<keymap::KeyMapManager>(*config_);

  if (absl::GetFlag(FLAGS_restricted)) {
    MOZC_VLOG(1) << "Server starts with restricted mode";
    // --restricted is almost always specified when mozc_client is inside Job.
    // The typical case is Startup processes on Vista.
    // On Vista, StartUp processes are in Job for 60 seconds. In order
    // to launch new mozc_server inside sandbox, we set the timeout
    // to be 60sec. Client application hopefully re-launch mozc_server.
    absl::SetFlag(&FLAGS_timeout, 60);
    absl::SetFlag(&FLAGS_max_session_size, 8);
    absl::SetFlag(&FLAGS_watch_dog_interval, 15);
    absl::SetFlag(&FLAGS_last_create_session_timeout, 60);
    absl::SetFlag(&FLAGS_last_command_timeout, 60);
  }

  // allow [2..128] sessions
  max_session_size_ =
      std::max(2, std::min(absl::GetFlag(FLAGS_max_session_size), 128));
  session_map_ = std::make_unique<SessionMap>(max_session_size_);

  if (!engine_) {
    return;
  }


  // everything is OK
  is_available_ = true;
}

SessionHandler::~SessionHandler() {
  for (SessionElement *element =
           const_cast<SessionElement *>(session_map_->Head());
       element != nullptr; element = element->next) {
    delete element->value;
    element->value = nullptr;
  }
  session_map_->Clear();
}

bool SessionHandler::IsAvailable() const { return is_available_; }

void SessionHandler::StartWatchDog() {
#ifndef MOZC_DISABLE_SESSION_WATCHDOG
  if (!session_watch_dog_.has_value()) {
    session_watch_dog_.emplace(
        absl::Seconds(absl::GetFlag(FLAGS_watch_dog_interval)));
  }
#endif  // MOZC_DISABLE_SESSION_WATCHDOG
}

void SessionHandler::UpdateSessions(const config::Config &config,
                                    const commands::Request &request) {
  auto new_config = std::make_unique<config::Config>(config);
  auto new_request = std::make_unique<commands::Request>(request);
  const auto *data_manager = engine_->GetDataManager();
  const composer::Table *table =
      data_manager != nullptr
          ? table_manager_->GetTable(*new_request, *new_config, *data_manager)
          : nullptr;
  auto new_key_map_manager =
      keymap::KeyMapManager::IsSameKeyMapManagerApplicable(*config_,
                                                           *new_config)
          ? nullptr
          : std::make_unique<keymap::KeyMapManager>(*new_config);

  for (SessionElement *element =
           const_cast<SessionElement *>(session_map_->Head());
       element != nullptr; element = element->next) {
    if (element->value != nullptr) {
      element->value->SetConfig(new_config.get());
      element->value->SetKeyMapManager(
          (new_key_map_manager ? new_key_map_manager : key_map_manager_).get());
      element->value->SetRequest(new_request.get());
      if (table != nullptr) {
        element->value->SetTable(table);
      }
    }
  }
  config::CharacterFormManager::GetCharacterFormManager()->ReloadConfig(
      *new_config);
  // Now no references to the current config/key_map_manager/request
  // should exist. We can destroy them here.
  config_ = std::move(new_config);
  if (new_key_map_manager) {
    key_map_manager_ = std::move(new_key_map_manager);
  }
  request_ = std::move(new_request);
}

bool SessionHandler::SyncData(commands::Command *command) {
  MOZC_VLOG(1) << "Syncing user data";
  engine_->GetUserDataManager()->Sync();
  engine_->GetUserDataManager()->Wait();
  return true;
}

bool SessionHandler::Shutdown(commands::Command *command) {
  MOZC_VLOG(1) << "Shutdown server";
  SyncData(command);
  is_available_ = false;
  UsageStats::IncrementCount("ShutDown");
  return true;
}

bool SessionHandler::Reload(commands::Command *command) {
  MOZC_VLOG(1) << "Reloading server";
  UpdateSessions(*config::ConfigHandler::GetConfig(), *request_);
  engine_->Reload();
  return true;
}

bool SessionHandler::ReloadAndWait(commands::Command *command) {
  MOZC_VLOG(1) << "Reloading server and wait for reloader";
  UpdateSessions(*config::ConfigHandler::GetConfig(), *request_);
  engine_->ReloadAndWait();
  return true;
}

bool SessionHandler::ClearUserHistory(commands::Command *command) {
  MOZC_VLOG(1) << "Clearing user history";
  engine_->GetUserDataManager()->ClearUserHistory();
  UsageStats::IncrementCount("ClearUserHistory");
  return true;
}

bool SessionHandler::ClearUserPrediction(commands::Command *command) {
  MOZC_VLOG(1) << "Clearing user prediction";
  engine_->GetUserDataManager()->ClearUserPrediction();
  UsageStats::IncrementCount("ClearUserPrediction");
  return true;
}

bool SessionHandler::ClearUnusedUserPrediction(commands::Command *command) {
  MOZC_VLOG(1) << "Clearing unused user prediction";
  engine_->GetUserDataManager()->ClearUnusedUserPrediction();
  UsageStats::IncrementCount("ClearUnusedUserPrediction");
  return true;
}

bool SessionHandler::GetConfig(commands::Command *command) {
  MOZC_VLOG(1) << "Getting config";
  config::ConfigHandler::GetConfig(command->mutable_output()->mutable_config());
  // Ensure the onmemory config is same as the locally stored one
  // because the local data could be changed by sync.
  UpdateSessions(command->output().config(), *request_);
  return true;
}

bool SessionHandler::SetConfig(commands::Command *command) {
  MOZC_VLOG(1) << "Setting user config";
  if (!command->input().has_config()) {
    LOG(WARNING) << "config is empty";
    return false;
  }

  *command->mutable_output()->mutable_config() = command->input().config();
  MaybeUpdateConfig(command);

  UsageStats::IncrementCount("SetConfig");
  return true;
}

bool SessionHandler::SetRequest(commands::Command *command) {
  MOZC_VLOG(1) << "Setting client's request";
  if (!command->input().has_request()) {
    LOG(WARNING) << "request is empty";
    return false;
  }
  UpdateSessions(*config_, command->input().request());
  return true;
}

bool SessionHandler::EvalCommand(commands::Command *command) {
  if (!is_available_) {
    LOG(ERROR) << "SessionHandler is not available.";
    return false;
  }

  bool eval_succeeded = false;
  Stopwatch stopwatch;
  stopwatch.Start();

  switch (command->input().type()) {
    case commands::Input::CREATE_SESSION:
      eval_succeeded = CreateSession(command);
      break;
    case commands::Input::DELETE_SESSION:
      eval_succeeded = DeleteSession(command);
      break;
    case commands::Input::SEND_KEY:
      eval_succeeded = SendKey(command);
      break;
    case commands::Input::TEST_SEND_KEY:
      eval_succeeded = TestSendKey(command);
      break;
    case commands::Input::SEND_COMMAND:
      eval_succeeded = SendCommand(command);
      break;
    case commands::Input::SYNC_DATA:
      eval_succeeded = SyncData(command);
      break;
    case commands::Input::CLEAR_USER_HISTORY:
      eval_succeeded = ClearUserHistory(command);
      break;
    case commands::Input::CLEAR_USER_PREDICTION:
      eval_succeeded = ClearUserPrediction(command);
      break;
    case commands::Input::CLEAR_UNUSED_USER_PREDICTION:
      eval_succeeded = ClearUnusedUserPrediction(command);
      break;
    case commands::Input::GET_CONFIG:
      eval_succeeded = GetConfig(command);
      break;
    case commands::Input::SET_CONFIG:
      eval_succeeded = SetConfig(command);
      break;
    case commands::Input::SET_REQUEST:
      eval_succeeded = SetRequest(command);
      break;
    case commands::Input::SHUTDOWN:
      eval_succeeded = Shutdown(command);
      break;
    case commands::Input::RELOAD:
      eval_succeeded = Reload(command);
      break;
    case commands::Input::RELOAD_AND_WAIT:
      eval_succeeded = ReloadAndWait(command);
      break;
    case commands::Input::CLEANUP:
      eval_succeeded = Cleanup(command);
      break;
    case commands::Input::SEND_USER_DICTIONARY_COMMAND:
      eval_succeeded = SendUserDictionaryCommand(command);
      break;
    case commands::Input::SEND_ENGINE_RELOAD_REQUEST:
      eval_succeeded = SendEngineReloadRequest(command);
      break;
    case commands::Input::NO_OPERATION:
      eval_succeeded = NoOperation(command);
      break;
    case commands::Input::CHECK_SPELLING:
      eval_succeeded = CheckSpelling(command);
      break;
    case commands::Input::RELOAD_SPELL_CHECKER:
      eval_succeeded = ReloadSpellChecker(command);
      break;
    default:
      eval_succeeded = false;
  }

  if (eval_succeeded) {
    UsageStats::IncrementCount("SessionAllEvent");
    if (command->input().type() != commands::Input::CREATE_SESSION) {
      // Fill a session ID even if command->input() doesn't have a id to ensure
      // that response size should not be 0, which causes disconnection of IPC.
      command->mutable_output()->set_id(command->input().id());
    }
  } else {
    command->mutable_output()->set_id(0);
    command->mutable_output()->set_error_code(
        commands::Output::SESSION_FAILURE);
  }

  if (eval_succeeded) {
    // TODO(komatsu): Make sre if checking eval_succeeded is necessary or not.
    observer_handler_->EvalCommandHandler(*command);
  }

  stopwatch.Stop();
  UsageStats::UpdateTiming(
      "ElapsedTimeUSec",
      static_cast<uint32_t>(absl::ToInt64Microseconds(stopwatch.GetElapsed())));

  return is_available_;
}

session::SessionInterface *SessionHandler::NewSession() {
  // Session doesn't take the ownership of engine.
  return new session::Session(engine_.get());
}

void SessionHandler::AddObserver(session::SessionObserverInterface *observer) {
  observer_handler_->AddObserver(observer);
}

void SessionHandler::MaybeUpdateConfig(commands::Command *command) {
  if (!command->output().has_config()) {
    return;
  }

  config::ConfigHandler::SetConfig(command->output().config());
  Reload(command);
}

bool SessionHandler::SendKey(commands::Command *command) {
  const SessionID id = command->input().id();
  session::SessionInterface **session = session_map_->MutableLookup(id);
  if (session == nullptr || *session == nullptr) {
    LOG(WARNING) << "SessionID " << id << " is not available";
    return false;
  }
  (*session)->SendKey(command);
  MaybeUpdateConfig(command);
  return true;
}

bool SessionHandler::TestSendKey(commands::Command *command) {
  const SessionID id = command->input().id();
  session::SessionInterface **session = session_map_->MutableLookup(id);
  if (session == nullptr || *session == nullptr) {
    LOG(WARNING) << "SessionID " << id << " is not available";
    return false;
  }
  (*session)->TestSendKey(command);
  return true;
}

bool SessionHandler::SendCommand(commands::Command *command) {
  const SessionID id = command->input().id();
  session::SessionInterface **session =
      const_cast<session::SessionInterface **>(session_map_->Lookup(id));
  if (session == nullptr || *session == nullptr) {
    LOG(WARNING) << "SessionID " << id << " is not available";
    return false;
  }
  (*session)->SendCommand(command);
  MaybeUpdateConfig(command);
  return true;
}

bool SessionHandler::CreateSession(commands::Command *command) {
  // prevent DOS attack
  // don't allow CreateSession in very short period.
  const absl::Duration create_session_minimum_interval = std::max(
      absl::ZeroDuration(),
      std::min(absl::Seconds(absl::GetFlag(FLAGS_create_session_min_interval)),
               absl::Seconds(10)));

  const absl::Time current_time = Clock::GetAbslTime();
  const absl::Duration create_session_interval =
      current_time - last_create_session_time_;
  // `create_session_interval` can be nagative if a user modifies their system
  // clock.
  if (create_session_interval >= absl::Duration() &&
      create_session_interval < create_session_minimum_interval) {
    return false;
  }

  last_create_session_time_ = current_time;

  // if session map is FULL, remove the oldest item from the LRU
  SessionElement *oldest_element = nullptr;
  if (session_map_->Size() >= max_session_size_) {
    oldest_element = const_cast<SessionElement *>(session_map_->Tail());
    if (oldest_element == nullptr) {
      LOG(ERROR) << "oldest SessionElement is NULL";
      return false;
    }
    delete oldest_element->value;
    oldest_element->value = nullptr;
    session_map_->Erase(oldest_element->key);
    MOZC_VLOG(1) << "Session is FULL, oldest SessionID " << oldest_element->key
                 << " is removed";
  }

  // Maybe build new engine if new request is received.
  // EngineBuilder::Build just returns a future object so
  // client needs to replace the new engine when the future is the ready to use.
  if (engine_builder_ && !engine_response_future_ &&
      current_engine_id_ != latest_engine_id_ && latest_engine_id_ != 0) {
    engine_response_future_ = engine_builder_->Build(latest_engine_id_);
    // Wait the engine if the no new engine is loaded so far.
    if (current_engine_id_ == 0 || always_wait_for_engine_response_future_) {
      engine_response_future_->Wait();
    }
  }

  // Replaces the engine when the new engine is ready to use.
  if (engine_builder_ && session_map_->Size() == 0 && engine_response_future_ &&
      engine_response_future_->Ready()) {
    auto &&engine_response = std::move(*engine_response_future_).Get();
    engine_response_future_.reset();
    *command->mutable_output()->mutable_engine_reload_response() =
        engine_response.response;
    if (engine_response.engine && engine_response.response.status() ==
                                      EngineReloadResponse::RELOAD_READY) {
      if (engine_ && engine_->GetUserDataManager()) {
        engine_->GetUserDataManager()->Wait();
      }
      engine_ = std::move(engine_response.engine);
      current_engine_id_ = engine_response.id;
      command->mutable_output()->mutable_engine_reload_response()->set_status(
          EngineReloadResponse::RELOADED);
      LOG_IF(FATAL, !engine_) << "Critical failure in engine replace";
      table_manager_->ClearCaches();
    } else {
      // This engine id causes a critical error, so rollback the id.
      LOG(ERROR) << "Failure in engine loading: "
                 << protobuf::Utf8Format(engine_response.response);
      const uint64_t rollback_id =
          engine_builder_->UnregisterRequest(engine_response.id);
      // Update latest_engine_id_ if latest_engine_id_ == engine_response.id.
      // Otherwise, latest_engine_id_ may already be updated by the new request.
      latest_engine_id_.compare_exchange_strong(engine_response.id,
                                                rollback_id);
    }
  }

  session::SessionInterface *session = NewSession();
  if (session == nullptr) {
    LOG(ERROR) << "Cannot allocate new Session";
    return false;
  }


  const SessionID new_id = CreateNewSessionID();
  SessionElement *element = session_map_->Insert(new_id);
  element->value = session;
  command->mutable_output()->set_id(new_id);

  // The oldes item should be reused
  DCHECK(oldest_element == nullptr || oldest_element == element);

  if (command->input().has_capability()) {
    session->set_client_capability(command->input().capability());
  }

  if (command->input().has_application_info()) {
    session->set_application_info(command->input().application_info());
  }

  // The created session has not been fully initialized yet.
  // SetConfig() will complete the initialization by setting information
  // (e.g., config, request, keymap, ...) to all the sessions,
  // including the newly created one.
  UpdateSessions(*config::ConfigHandler::GetConfig(), *request_);

  // session is not empty.
  last_session_empty_time_ = absl::InfinitePast();

  UsageStats::IncrementCount("SessionCreated");

  return true;
}

bool SessionHandler::DeleteSession(commands::Command *command) {
  DeleteSessionID(command->input().id());
  if (engine_->GetUserDataManager()) {
    engine_->GetUserDataManager()->Sync();
  }
  return true;
}

// Scan all sessions and find and delete session which is either
// (a) The session is not activated for 60min
// (b) The session is created but not accessed for 5min
// (c) application is already terminated.
// Also, if timeout is enabled, shutdown server if there is
// no active session and client doesn't send any conversion
// request to the server for FLAGS_timeout sec.
bool SessionHandler::Cleanup(commands::Command *command) {
  const absl::Time current_time = Clock::GetAbslTime();

  // suspend/hibernation may happen
  absl::Duration suspend_time = absl::ZeroDuration();
#ifndef MOZC_DISABLE_SESSION_WATCHDOG
  if (last_cleanup_time_ != absl::InfinitePast() &&
      session_watch_dog_.has_value() &&
      (current_time - last_cleanup_time_) >
          2 * session_watch_dog_->interval()) {
    suspend_time =
        current_time - last_cleanup_time_ - session_watch_dog_->interval();
    LOG(WARNING) << "server went to suspend mode for " << suspend_time;
  }
#endif  // MOZC_DISABLE_SESSION_WATCHDOG

  // allow [1..600] sec. default: 300
  const absl::Duration create_session_timeout =
      suspend_time + std::max(absl::Seconds(1),
                              std::min(absl::Seconds(absl::GetFlag(
                                           FLAGS_last_create_session_timeout)),
                                       absl::Seconds(600)));

  // allow [10..7200] sec. default 3600
  const absl::Duration last_command_timeout =
      suspend_time +
      std::max(
          absl::Seconds(10),
          std::min(absl::Seconds(absl::GetFlag(FLAGS_last_command_timeout)),
                   absl::Seconds(7200)));

  std::vector<SessionID> remove_ids;
  for (SessionElement *element =
           const_cast<SessionElement *>(session_map_->Head());
       element != nullptr; element = element->next) {
    session::SessionInterface *session = element->value;
    if (!IsApplicationAlive(session)) {
      MOZC_VLOG(2) << "Application is not alive. Removing: " << element->key;
      remove_ids.push_back(element->key);
    } else if (session->last_command_time() == absl::InfinitePast()) {
      // no command is exectuted
      if ((current_time - session->create_session_time()) >=
          create_session_timeout) {
        remove_ids.push_back(element->key);
      }
    } else {  // some commands are executed already
      if ((current_time - session->last_command_time()) >=
          last_command_timeout) {
        remove_ids.push_back(element->key);
      }
    }
  }

  for (size_t i = 0; i < remove_ids.size(); ++i) {
    DeleteSessionID(remove_ids[i]);
    MOZC_VLOG(1) << "Session ID " << remove_ids[i] << " is removed by server";
  }

  // Sync all data. This is a regression bug fix http://b/3033708
  engine_->GetUserDataManager()->Sync();

  // timeout is enabled.
  if (absl::GetFlag(FLAGS_timeout) > 0 &&
      (current_time - last_session_empty_time_) >=
          suspend_time + absl::Seconds(absl::GetFlag(FLAGS_timeout))) {
    Shutdown(command);
  }

  last_cleanup_time_ = current_time;

  return true;
}

bool SessionHandler::SendUserDictionaryCommand(commands::Command *command) {
  if (!command->input().has_user_dictionary_command()) {
    return false;
  }
  user_dictionary::UserDictionaryCommandStatus status;
  const bool result = user_dictionary_session_handler_->Evaluate(
      command->input().user_dictionary_command(), &status);
  if (result) {
    status.Swap(
        command->mutable_output()->mutable_user_dictionary_command_status());
  }
  return result;
}

bool SessionHandler::SendEngineReloadRequest(commands::Command *command) {
  if (!engine_builder_ || !command->input().has_engine_reload_request()) {
    return false;
  }
  latest_engine_id_ = engine_builder_->RegisterRequest(
      command->input().engine_reload_request());
  command->mutable_output()->mutable_engine_reload_response()->set_status(
      EngineReloadResponse::ACCEPTED);
  return true;
}

bool SessionHandler::NoOperation(commands::Command *command) { return true; }

bool SessionHandler::CheckSpelling(commands::Command *command) {
  if (!command->input().has_check_spelling_request() ||
      command->input().check_spelling_request().text().empty()) {
    return true;
  }


  return true;
}

bool SessionHandler::ReloadSpellChecker(commands::Command *command) {
  return true;
}

// Create Random Session ID in order to make the session id unpredicable
SessionID SessionHandler::CreateNewSessionID() {
  while (true) {
    // don't allow id == 0, as it is reserved for "invalid id"
    const SessionID id =
        absl::Uniform<SessionID>(absl::IntervalClosed, bitgen_, 1,
                                 std::numeric_limits<SessionID>::max());
    if (!session_map_->HasKey(id)) {
      return id;
    }

    LOG(WARNING) << "Session ID " << id << " is already used. retry";
  }
}

bool SessionHandler::DeleteSessionID(SessionID id) {
  session::SessionInterface **session = session_map_->MutableLookup(id);
  if (session == nullptr || *session == nullptr) {
    LOG_IF(WARNING, id != 0) << "cannot find SessionID " << id;
    return false;
  }
  delete *session;

  session_map_->Erase(id);  // remove from LRU

  // if session gets empty, save the timestamp
  if (last_session_empty_time_ == absl::InfinitePast() &&
      session_map_->Size() == 0) {
    last_session_empty_time_ = Clock::GetAbslTime();
  }

  return true;
}
}  // namespace mozc
