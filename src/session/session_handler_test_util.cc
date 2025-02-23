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

#include "session/session_handler_test_util.h"

#include <cstdint>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "prediction/user_history_predictor.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/session_handler.h"

ABSL_DECLARE_FLAG(int32_t, max_session_size);
ABSL_DECLARE_FLAG(int32_t, create_session_min_interval);
ABSL_DECLARE_FLAG(int32_t, watch_dog_interval);
ABSL_DECLARE_FLAG(int32_t, last_command_timeout);
ABSL_DECLARE_FLAG(int32_t, last_create_session_timeout);
ABSL_DECLARE_FLAG(bool, restricted);

namespace mozc {
namespace session {
namespace testing {

using ::mozc::commands::Command;
using ::mozc::config::CharacterFormManager;
using ::mozc::config::ConfigHandler;

bool CreateSession(SessionHandler *handler, uint64_t *id) {
  Command command;
  command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
  command.mutable_input()->mutable_capability()->set_text_deletion(
      commands::Capability::DELETE_PRECEDING_TEXT);
  handler->EvalCommand(&command);
  if (id != nullptr) {
    *id = command.has_output() ? command.output().id() : 0;
  }
  return (command.output().error_code() == commands::Output::SESSION_SUCCESS);
}

bool DeleteSession(SessionHandler *handler, uint64_t id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::DELETE_SESSION);
  return handler->EvalCommand(&command);
}

bool CleanUp(SessionHandler *handler, uint64_t id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::CLEANUP);
  return handler->EvalCommand(&command);
}

bool ClearUserPrediction(SessionHandler *handler, uint64_t id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::CLEAR_USER_PREDICTION);
  return handler->EvalCommand(&command);
}

bool IsGoodSession(SessionHandler *handler, uint64_t id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::SEND_KEY);
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::SPACE);
  handler->EvalCommand(&command);
  return (command.output().error_code() == commands::Output::SESSION_SUCCESS);
}

void SessionHandlerTestBase::SetUp() {
  flags_max_session_size_backup_ = absl::GetFlag(FLAGS_max_session_size);
  flags_create_session_min_interval_backup_ =
      absl::GetFlag(FLAGS_create_session_min_interval);
  flags_watch_dog_interval_backup_ = absl::GetFlag(FLAGS_watch_dog_interval);
  flags_last_command_timeout_backup_ =
      absl::GetFlag(FLAGS_last_command_timeout);
  flags_last_create_session_timeout_backup_ =
      absl::GetFlag(FLAGS_last_create_session_timeout);
  flags_restricted_backup_ = absl::GetFlag(FLAGS_restricted);

  config_backup_ = ConfigHandler::GetCopiedConfig();
  ClearState();
}

void SessionHandlerTestBase::TearDown() {
  ClearState();
  ConfigHandler::SetConfig(config_backup_);

  absl::SetFlag(&FLAGS_max_session_size, flags_max_session_size_backup_);
  absl::SetFlag(&FLAGS_create_session_min_interval,
                flags_create_session_min_interval_backup_);
  absl::SetFlag(&FLAGS_watch_dog_interval, flags_watch_dog_interval_backup_);
  absl::SetFlag(&FLAGS_last_command_timeout,
                flags_last_command_timeout_backup_);
  absl::SetFlag(&FLAGS_last_create_session_timeout,
                flags_last_create_session_timeout_backup_);
  absl::SetFlag(&FLAGS_restricted, flags_restricted_backup_);
}

void SessionHandlerTestBase::ClearState() {
  config::Config config;
  ConfigHandler::GetDefaultConfig(&config);
  ConfigHandler::SetConfig(config);

  // CharacterFormManager is not automatically updated when the config is
  // updated.
  CharacterFormManager::GetCharacterFormManager()->ReloadConfig(config);

  // Some destructors may save the state on storages. To clear the state, we
  // explicitly call destructors before clearing storages.
  FileUtil::UnlinkOrLogError(
      ConfigFileStream::GetFileName("user://boundary.db"));
  FileUtil::UnlinkOrLogError(
      ConfigFileStream::GetFileName("user://segment.db"));
  FileUtil::UnlinkOrLogError(
      prediction::UserHistoryPredictor::GetUserHistoryFileName());
}

}  // namespace testing
}  // namespace session
}  // namespace mozc
