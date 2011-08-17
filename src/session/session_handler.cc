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

// Session Handler of Mozc server.
// Migrated from ipc/interpreter and session/session_manager
#include "session/session_handler.h"

#include <algorithm>
#include <vector>

#include "base/base.h"
#include "base/util.h"
#include "base/process.h"
#include "base/singleton.h"
#include "base/stopwatch.h"
#include "config/config_handler.h"
#include "config/config.pb.h"
#include "converter/user_data_manager_interface.h"
#include "session/commands.pb.h"
#include "session/session_factory_manager.h"
#include "session/session_interface.h"
#include "session/session_observer_handler.h"
#include "session/session_watch_dog.h"

DEFINE_int32(timeout, -1,
             "server timeout. "
             "if sessions get empty for \"timeout\", "
             "shutdown message is automatically emitted");

DEFINE_int32(max_session_size, 64,
             "maximum sessions size. "
             "if size of sessions reaches to \"max_session_size\", "
             "oldest session is removed");

DEFINE_int32(create_session_min_interval, 0,
             "minimum interval (sec) for create session");

DEFINE_int32(watch_dog_interval, 180,
             "watch dog timer intaval (sec)");

DEFINE_int32(last_command_timeout, 3600,
             "remove session if it is not accessed for "
             "\"last_command_timeout\" sec");

DEFINE_int32(last_create_session_timeout, 300,
             "remove session if it is not accessed for "
             "\"last_create_session_timeout\" sec "
             "after create session command");

DEFINE_bool(restricted, false,
            "Launch server with restricted setting");

namespace mozc {

namespace {
bool IsApplicationAlive(const session::SessionInterface *session) {
  const commands::ApplicationInfo &info = session->application_info();
  // When the thread/process's current status is unknown, i.e.,
  // if IsThreadAlive/IsProcessAlive functions failed to know the
  // status of the thread/process, return true just in case.
  // Here, we want to kill the session only when the target thread/process
  // are terminated with 100% probability. Otherwise, it's better to do
  // nothing to prevent any side effects.
#ifdef OS_WINDOWS
  if (info.has_thread_id()) {
    return Process::IsThreadAlive(
        static_cast<size_t>(info.thread_id()), true);
  }
#else   // OS_WINDOWS
  if (info.has_process_id()) {
    return Process::IsProcessAlive(
        static_cast<size_t>(info.process_id()), true);
  }
#endif  // OS_WINDOWS
  return true;
}
}  // namespace

SessionHandler::SessionHandler()
    : is_available_(false),
      keyevent_counter_(0),
      max_session_size_(0),
      last_session_empty_time_(Util::GetTime()),
      last_cleanup_time_(0),
      last_create_session_time_(0),
      session_factory_(
          session::SessionFactoryManager::GetSessionFactory()),
      observer_handler_(new session::SessionObserverHandler()),
      stopwatch_(new Stopwatch) {
  if (FLAGS_restricted) {
    VLOG(1) << "Server starts with restricted mode";
    // --restricted is almost always specified when mozc_client is inside Job.
    // The typical case is Startup processes on Vista.
    // On Vista, StartUp processes are in Job for 60 seconds. In order
    // to launch new mozc_server inside sandbox, we set the timeout
    // to be 60sec. Client application hopefully re-launch mozc_server.
    FLAGS_timeout = 60;
    FLAGS_max_session_size = 8;
    FLAGS_watch_dog_interval = 15;
    FLAGS_last_create_session_timeout = 60;
    FLAGS_last_command_timeout = 60;
  }

  session_watch_dog_.reset(new SessionWatchDog(FLAGS_watch_dog_interval));

  // allow [2..128] sessions
  max_session_size_ = max(2, min(FLAGS_max_session_size, 128));
  session_map_.reset(new SessionMap(max_session_size_));

  if (session_factory_ == NULL || !session_factory_->IsAvailable()) {
    return;
  }

  // everything is OK
  is_available_ = true;
}

SessionHandler::~SessionHandler() {
  for (SessionElement *element =
           const_cast<SessionElement *>(session_map_->Head());
       element != NULL; element = element->next) {
    delete element->value;
    element->value = NULL;
  }
  session_map_->Clear();
  if (session_watch_dog_->IsRunning()) {
    session_watch_dog_->Terminate();
  }
}

bool SessionHandler::IsAvailable() const {
  return is_available_;
}

bool SessionHandler::StartWatchDog() {
  if (!session_watch_dog_->IsRunning()) {
    session_watch_dog_->Start();
  }
  return session_watch_dog_->IsRunning();
}

void SessionHandler::ReloadSession() {
  observer_handler_->Reload();
  ReloadConfig();
}

void SessionHandler::ReloadConfig() {
  for (SessionElement *element =
           const_cast<SessionElement *>(session_map_->Head());
       element != NULL; element = element->next) {
    if (element->value != NULL) {
      element->value->ReloadConfig();
    }
  }
}

bool SessionHandler::SyncData(commands::Command *command) {
  VLOG(1) << "Syncing user data";
  session_factory_->GetUserDataManager()->Sync();
  command->mutable_output()->set_id(command->input().id());
  return true;
}

bool SessionHandler::Shutdown(commands::Command *command) {
  VLOG(1) << "Shutdown server";
  SyncData(command);
  ReloadSession();   // for saving log_commands
  is_available_ = false;
  return true;
}

bool SessionHandler::Reload(commands::Command *command) {
  VLOG(1) << "Reloading server";
  ReloadSession();
  session_factory_->Reload();
  RunReloaders();  // call all reloaders defined in .cc file
  command->mutable_output()->set_id(command->input().id());
  return true;
}

bool SessionHandler::ClearUserHistory(commands::Command *command) {
  VLOG(1) << "Clearing user history";
  session_factory_->GetUserDataManager()->ClearUserHistory();
  command->mutable_output()->set_id(command->input().id());
  return true;
}

bool SessionHandler::ClearUserPrediction(commands::Command *command) {
  VLOG(1) << "Clearing user prediction";
  session_factory_->GetUserDataManager()->ClearUserPrediction();
  command->mutable_output()->set_id(command->input().id());
  return true;
}

bool SessionHandler::ClearUnusedUserPrediction(commands::Command *command) {
  VLOG(1) << "Clearing unused user prediction";
  session_factory_->GetUserDataManager()->ClearUnusedUserPrediction();
  command->mutable_output()->set_id(command->input().id());
  return true;
}

bool SessionHandler::GetStoredConfig(commands::Command *command) {
  VLOG(1) << "Getting stored config";
  // Ensure the onmemory config is same as the locally stored one
  // because the local data could be changed by sync.
  ReloadConfig();

  // Use GetStoredConfig instead of GetConfig because GET_CONFIG
  // command should return raw stored config, which is not
  // affected by imposed config.
  if (!config::ConfigHandler::GetStoredConfig(
          command->mutable_output()->mutable_config())) {
    LOG(WARNING) << "cannot get config";
    return false;
  }
  command->mutable_output()->set_id(command->input().id());
  return true;
}

bool SessionHandler::SetStoredConfig(commands::Command *command) {
  VLOG(1) << "Setting user config";
  if (!command->input().has_config()) {
    LOG(WARNING) << "config is empty";
    return false;
  }

  const mozc::config::Config &config = command->input().config();
  if (!config::ConfigHandler::SetConfig(config)) {
    return false;
  }

  command->mutable_output()->mutable_config()->CopyFrom(config);

  Reload(command);

  return true;
}

bool SessionHandler::SetImposedConfig(commands::Command *command) {
  VLOG(1) << "Setting imposed config";
  if (!command->input().has_config()) {
    LOG(WARNING) << "config is empty";
    return false;
  }

  const mozc::config::Config &config = command->input().config();
  config::ConfigHandler::SetImposedConfig(config);

  Reload(command);

  return true;
}


bool SessionHandler::EvalCommand(commands::Command *command) {
  if (!is_available_) {
    return false;
  }

  bool eval_succeeded = false;
  stopwatch_->Reset();
  stopwatch_->Start();

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
      eval_succeeded = GetStoredConfig(command);
      break;
    case commands::Input::SET_CONFIG:
      eval_succeeded = SetStoredConfig(command);
      break;
    case commands::Input::SET_IMPOSED_CONFIG:
      eval_succeeded = SetImposedConfig(command);
      break;
    case commands::Input::SHUTDOWN:
      eval_succeeded = Shutdown(command);
      break;
    case commands::Input::RELOAD:
      eval_succeeded = Reload(command);
      break;
    case commands::Input::CLEANUP:
      eval_succeeded = Cleanup(command);
      break;
    case commands::Input::NO_OPERATION:
      eval_succeeded = NoOperation(command);
      break;
    default:
      eval_succeeded = false;
  }

  if (!eval_succeeded) {
    command->mutable_output()->set_id(0);
    command->mutable_output()->set_error_code(
        commands::Output::SESSION_FAILURE);
  }


  // Since ElappsedTime is processed by UsageStats,
  // Stop the timer before usage stats aggregation.
  // This won't be good if UsageStats aggregator consumes a lot of
  // CPU time
  stopwatch_->Stop();
  command->mutable_output()->set_elapsed_time(
      static_cast<int32>(stopwatch_->GetElapsedMicroseconds()));

  if (eval_succeeded) {
    // TODO(komatsu): Make sre if checking eval_succeeded is necessary or not.
    observer_handler_->EvalCommandHandler(*command);
  }

  return is_available_;
}

session::SessionInterface *SessionHandler::NewSession() {
  return session_factory_->NewSession();
}

void SessionHandler::SetSessionFactory(
    session::SessionFactoryInterface *new_factory) {
  session_factory_ = new_factory;
}

void SessionHandler::AddObserver(session::SessionObserverInterface *observer) {
  observer_handler_->AddObserver(observer);
}

bool SessionHandler::SendKey(commands::Command *command) {
  const SessionID id = command->input().id();
  command->mutable_output()->set_id(id);

  session::SessionInterface **session = session_map_->MutableLookup(id);
  if (session == NULL || *session == NULL) {
    LOG(WARNING) << "SessionID " << id << " is not available";
    return false;
  }
  (*session)->SendKey(command);
  return true;
}

bool SessionHandler::TestSendKey(commands::Command *command) {
  const SessionID id = command->input().id();
  command->mutable_output()->set_id(id);
  session::SessionInterface **session = session_map_->MutableLookup(id);
  if (session == NULL || *session == NULL) {
    LOG(WARNING) << "SessionID " << id << " is not available";
    return false;
  }
  (*session)->TestSendKey(command);
  return true;
}

bool SessionHandler::SendCommand(commands::Command *command) {
  const SessionID id = command->input().id();
  command->mutable_output()->set_id(id);
  session::SessionInterface **session =
    const_cast<session::SessionInterface **>(session_map_->Lookup(id));
  if (session == NULL || *session == NULL) {
    LOG(WARNING) << "SessionID " << id << " is not available";
    return false;
  }
  (*session)->SendCommand(command);
  return true;
}

bool SessionHandler::CreateSession(commands::Command *command) {
  // prevent DOS attack
  // don't allow CreateSession in very short period.
  const int kCreateSessionMinimumInterval =
      max(0, min(FLAGS_create_session_min_interval, 10));

  uint64 current_time = Util::GetTime();
  if (last_create_session_time_ != 0 &&
      (current_time - last_create_session_time_) <
      kCreateSessionMinimumInterval) {
    last_create_session_time_ = current_time;
    return false;
  }

  last_create_session_time_ = current_time;

  // if session map is FULL, remove the oldest item from the LRU
  SessionElement *oldest_element = NULL;
  if (session_map_->Size() >= max_session_size_) {
    oldest_element = const_cast<SessionElement *>(session_map_->Tail());
    if (oldest_element == NULL) {
      LOG(ERROR) << "oldest SessionElement is NULL";
      return false;
    }
    delete oldest_element->value;
    oldest_element->value = NULL;
    session_map_->Erase(oldest_element->key);
    VLOG(1) << "Session is FULL, oldest SessionID "
            << oldest_element->key << " is removed";
  };

  session::SessionInterface *session = NewSession();
  if (session == NULL) {
    LOG(ERROR) << "Cannot allocate new Session";
    return false;
  }

  const SessionID new_id = CreateNewSessionID();
  SessionElement *element = session_map_->Insert(new_id);
  element->value = session;
  command->mutable_output()->set_id(new_id);

  // The oldes item should be reused
  DCHECK(oldest_element == NULL || oldest_element == element);

  if (command->input().has_capability()) {
    session->set_client_capability(command->input().capability());
  }

  if (command->input().has_application_info()) {
    session->set_application_info(command->input().application_info());
  }

  // Ensure the onmemory config is same as the locally stored one
  // because the local data could be changed by sync.
  ReloadConfig();

  // session is not empty.
  last_session_empty_time_ = 0;

  return true;
}

bool SessionHandler::DeleteSession(commands::Command *command) {
  const SessionID id = command->input().id();
  command->mutable_output()->set_id(id);
  DeleteSessionID(id);
  session_factory_->GetUserDataManager()->Sync();
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
  const SessionID id = command->input().id();
  command->mutable_output()->set_id(id);

  const uint64 current_time = Util::GetTime();

  // suspend/hibernation may happen
  uint64 suspend_time = 0;
  if (last_cleanup_time_ != 0 &&
      session_watch_dog_->IsRunning() &&
      (current_time - last_cleanup_time_) >
      2 * session_watch_dog_->interval()) {
    suspend_time = current_time - last_cleanup_time_ -
        session_watch_dog_->interval();
    LOG(WARNING) << "server went to suspend mode for "
                 << suspend_time << " sec";
  }

  // allow [1..600] sec. default: 300
  const uint64 kCreateSessionTimeout =
      suspend_time +
      max(1, min(FLAGS_last_create_session_timeout, 600));

  // allow [10..7200] sec. default 3600
  const uint64 kLastCommandTimeout =
      suspend_time +
      max(10, min(FLAGS_last_command_timeout, 7200));

  vector<SessionID> remove_ids;
  for (SessionElement *element =
           const_cast<SessionElement *>(session_map_->Head());
       element != NULL; element = element->next) {
    session::SessionInterface *session = element->value;
    if (!IsApplicationAlive(session)) {
      VLOG(2) << "Application is not alive. Removing: " << element->key;
      remove_ids.push_back(element->key);
    } else if (session->last_command_time() == 0) {
      // no command is exectuted
      if ((current_time - session->create_session_time()) >=
          kCreateSessionTimeout) {
        remove_ids.push_back(element->key);
      }
    } else {  // some commands are executed already
      if ((current_time - session->last_command_time()) >=
          kLastCommandTimeout) {
        remove_ids.push_back(element->key);
      }
    }
  }

  for (size_t i = 0; i < remove_ids.size(); ++i) {
    DeleteSessionID(remove_ids[i]);
    VLOG(1) << "Session ID " << remove_ids[i] << " is removed by server";
  }

  // Sync all data.
  // This is a regression bug fix http://b/issue?id=3033708
  session_factory_->GetUserDataManager()->Sync();

  // timeout is enabled.
  if (FLAGS_timeout > 0 &&
      last_session_empty_time_ != 0 &&
      (current_time - last_session_empty_time_)
      >= suspend_time + FLAGS_timeout) {
    Shutdown(command);
  }

  last_cleanup_time_ = current_time;

  return true;
}

bool SessionHandler::NoOperation(commands::Command *command) {
  const SessionID id = command->input().id();
  command->mutable_output()->set_id(id);
  return true;
}

// Create Random Session ID in order to make the session id unpredicable
SessionID SessionHandler::CreateNewSessionID() {
  SessionID id = 0;
  while (true) {
    if (!Util::GetSecureRandomSequence(
            reinterpret_cast<char *>(&id), sizeof(id))) {
      LOG(ERROR) << "GetSecureRandomSequence() failed. use rand()";
      id = static_cast<uint64>(rand());
    }

    // don't allow id == 0, as it is reserved for
    // "invalid id"
    if (id != 0 && !session_map_->HasKey(id)) {
      break;
    }

    LOG(WARNING) << "Session ID " << id << " is already used. retry";
  }

  return id;
}

bool SessionHandler::DeleteSessionID(SessionID id) {
  session::SessionInterface **session = session_map_->MutableLookup(id);
  if (session == NULL || *session == NULL) {
    LOG_IF(WARNING, id != 0) << "cannot find SessionID " << id;
    return false;
  }
  delete *session;

  session_map_->Erase(id);   // remove from LRU

  // if session gets empty, save the timestamp
  if (last_session_empty_time_ == 0 &&
      session_map_->Size() == 0) {
    last_session_empty_time_ = Util::GetTime();
  }

  return true;
}
}  // namespace mozc
