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

#include "session/session_handler_tool.h"

#include <memory>

#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/number_util.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/protobuf/text_format.h"
#include "base/status.h"
#include "base/util.h"
#include "composer/key_parser.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "engine/engine_factory.h"
#include "engine/user_data_manager_interface.h"
#include "prediction/user_history_predictor.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/request_test_util.h"
#include "session/session_handler.h"
#include "session/session_handler_interface.h"
#include "session/session_usage_observer.h"
#include "storage/registry.h"
#include "usage_stats/usage_stats.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

using mozc::commands::CandidateList;
using mozc::commands::CandidateWord;
using mozc::commands::CompositionMode;
using mozc::commands::Input;
using mozc::commands::KeyEvent;
using mozc::commands::Output;
using mozc::commands::Request;
using mozc::commands::RequestForUnitTest;
using mozc::config::CharacterFormManager;
using mozc::config::Config;
using mozc::config::ConfigHandler;
using mozc::protobuf::FieldDescriptor;
using mozc::protobuf::Message;
using mozc::protobuf::TextFormat;
using mozc::session::SessionHandlerTool;

namespace mozc {
namespace session {

using commands::Command;
using config::CharacterFormManager;
using config::ConfigHandler;

bool CreateSession(SessionHandlerInterface *handler, uint64 *id) {
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

bool DeleteSession(SessionHandlerInterface *handler, uint64 id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::DELETE_SESSION);
  return handler->EvalCommand(&command);
}

bool CleanUp(SessionHandlerInterface *handler, uint64 id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::CLEANUP);
  return handler->EvalCommand(&command);
}

bool IsGoodSession(SessionHandlerInterface *handler, uint64 id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::SEND_KEY);
  command.mutable_input()->mutable_key()->set_special_key(
      commands::KeyEvent::SPACE);
  handler->EvalCommand(&command);
  return (command.output().error_code() == commands::Output::SESSION_SUCCESS);
}

SessionHandlerTool::SessionHandlerTool(std::unique_ptr<EngineInterface> engine)
    : id_(0),
      usage_observer_(new SessionUsageObserver),
      data_manager_(engine->GetUserDataManager()),
      handler_(new SessionHandler(std::move(engine))) {
  handler_->AddObserver(usage_observer_.get());
}

SessionHandlerTool::~SessionHandlerTool() {}

bool SessionHandlerTool::CreateSession() {
  Command command;
  command.mutable_input()->set_type(commands::Input::CREATE_SESSION);
  command.mutable_input()->mutable_capability()->set_text_deletion(
      commands::Capability::DELETE_PRECEDING_TEXT);
  handler_->EvalCommand(&command);
  id_ = command.has_output() ? command.output().id() : 0;
  return (command.output().error_code() == commands::Output::SESSION_SUCCESS);
}

bool SessionHandlerTool::DeleteSession() {
  Command command;
  command.mutable_input()->set_id(id_);
  command.mutable_input()->set_type(commands::Input::DELETE_SESSION);
  return handler_->EvalCommand(&command);
}

bool SessionHandlerTool::CleanUp() {
  Command command;
  command.mutable_input()->set_id(id_);
  command.mutable_input()->set_type(commands::Input::CLEANUP);
  return handler_->EvalCommand(&command);
}

bool SessionHandlerTool::ClearUserPrediction() {
  Command command;
  command.mutable_input()->set_id(id_);
  command.mutable_input()->set_type(commands::Input::CLEAR_USER_PREDICTION);
  return handler_->EvalCommand(&command);
}

bool SessionHandlerTool::SendKeyWithOption(const commands::KeyEvent &key,
                                          const commands::Input &option,
                                          commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_KEY);
  input.mutable_key()->CopyFrom(key);
  input.MergeFrom(option);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::TestSendKeyWithOption(const commands::KeyEvent &key,
                                              const commands::Input &option,
                                              commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::TEST_SEND_KEY);
  input.mutable_key()->CopyFrom(key);
  input.MergeFrom(option);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SelectCandidate(uint32 id, commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::SELECT_CANDIDATE);
  input.mutable_command()->set_id(id);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SubmitCandidate(uint32 id, commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::SUBMIT_CANDIDATE);
  input.mutable_command()->set_id(id);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::ExpandSuggestion(commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(
      commands::SessionCommand::EXPAND_SUGGESTION);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::Reload() {
  commands::Input input;
  input.set_type(commands::Input::RELOAD);
  return EvalCommand(&input, nullptr);
}

bool SessionHandlerTool::ResetContext() {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::RESET_CONTEXT);
  return EvalCommand(&input, nullptr);
}

bool SessionHandlerTool::UndoOrRewind(commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::UNDO_OR_REWIND);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SwitchInputMode(
    commands::CompositionMode composition_mode) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(
      commands::SessionCommand::SWITCH_INPUT_MODE);
  input.mutable_command()->set_composition_mode(composition_mode);
  return EvalCommand(&input, nullptr);
}

bool SessionHandlerTool::SetRequest(const commands::Request &request,
                                   commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SET_REQUEST);
  input.mutable_request()->CopyFrom(request);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SetConfig(const config::Config &config,
                                  commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SET_CONFIG);
  *input.mutable_config() = config;
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SyncData() {
  return data_manager_->Wait();
}

void SessionHandlerTool::SetCallbackText(const std::string &text) {
  callback_text_ = text;
}

bool SessionHandlerTool::EvalCommandInternal(commands::Input *input,
                                            commands::Output *output,
                                            bool allow_callback) {
  input->set_id(id_);
  commands::Command command;
  command.mutable_input()->CopyFrom(*input);
  bool result = handler_->EvalCommand(&command);
  if (result && output != nullptr) {
    output->CopyFrom(command.output());
  }

  // If callback is allowed and the callback field exists, evaluate the callback
  // command.
  if (result && allow_callback && command.output().has_callback() &&
      command.output().callback().has_session_command()) {
    commands::Input input2;
    input2.set_type(commands::Input::SEND_COMMAND);
    input2.mutable_command()->CopyFrom(
        command.output().callback().session_command());
    input2.mutable_command()->set_text(callback_text_);
    // Disallow further recursion.
    result = EvalCommandInternal(&input2, output, false);
  }
  callback_text_.clear();
  return result;
}

bool SessionHandlerTool::EvalCommand(commands::Input *input,
                                    commands::Output *output) {
  return EvalCommandInternal(input, output, true);
}

SessionHandlerInterpreter::SessionHandlerInterpreter()
    : SessionHandlerInterpreter(
          std::unique_ptr<EngineInterface>(EngineFactory::Create())) {}

SessionHandlerInterpreter::SessionHandlerInterpreter(
    std::unique_ptr<EngineInterface> engine) {

  client_ = absl::make_unique<SessionHandlerTool>(std::move(engine));
  config_ = absl::make_unique<Config>();
  last_output_ = absl::make_unique<Output>();
  request_ = absl::make_unique<Request>();

  ConfigHandler::GetConfig(config_.get());

  // Set up session.
  CHECK(client_->CreateSession()) << "Client initialization is failed.";
}

SessionHandlerInterpreter::~SessionHandlerInterpreter() {
  // Shut down.
  CHECK(client_->DeleteSession());

  ClearState();
  request_.reset();
  last_output_.reset();
  config_.reset();
  client_.reset();
}

void SessionHandlerInterpreter::ClearState() {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  ConfigHandler::SetConfig(config);

  // CharacterFormManager is not automatically updated when the config is
  // updated.
  CharacterFormManager::GetCharacterFormManager()->ReloadConfig(config);

  // Some destructors may save the state on storages. To clear the state, we
  // explicitly call destructors before clearing storages.
  mozc::storage::Registry::Clear();
  FileUtil::Unlink(mozc::ConfigFileStream::GetFileName("user://boundary.db"));
  FileUtil::Unlink(mozc::ConfigFileStream::GetFileName("user://segment.db"));
  FileUtil::Unlink(mozc::UserHistoryPredictor::GetUserHistoryFileName());
}

void SessionHandlerInterpreter::ClearAll() {
  ResetContext();
  ClearUserPrediction();
  ClearUsageStats();
}

void SessionHandlerInterpreter::ResetContext() {
  CHECK(client_->ResetContext());
  last_output_->Clear();
}

void SessionHandlerInterpreter::SyncDataToStorage() {
  CHECK(client_->SyncData());
}

void SessionHandlerInterpreter::ClearUserPrediction() {
  CHECK(client_->ClearUserPrediction());
  SyncDataToStorage();
}

void SessionHandlerInterpreter::ClearUsageStats() {
  mozc::usage_stats::UsageStats::ClearAllStatsForTest();
}

const Output& SessionHandlerInterpreter::LastOutput() const {
  return *(last_output_.get());
}


bool SessionHandlerInterpreter::GetCandidateIdByValue(
    const absl::string_view value, uint32 *id) {
  const Output &output = LastOutput();

  auto find_id = [&value](const CandidateList &candidate_list,
                          uint32 *id) -> bool {
    for (const CandidateWord &candidate : candidate_list.candidates()) {
      if (candidate.has_value() && candidate.value() == value) {
        *id = candidate.id();
        return true;
      }
    }
    return false;
  };

  if (output.has_all_candidate_words() &&
      find_id(output.all_candidate_words(), id)) {
    return true;
  }

  if (output.has_removed_candidate_words_for_debug() &&
      find_id(output.removed_candidate_words_for_debug(), id)) {
    return true;
  }

  return false;
}

bool SetOrAddFieldValueFromString(const std::string &name,
                                  const std::string &value, Message *message) {
  const FieldDescriptor *field =
      message->GetDescriptor()->FindFieldByName(name);
  if (!field) {
    LOG(ERROR) << "Unknown field name: " << name;
    return false;
  }
  // String type value should be quoted for ParseFieldValueFromString().
  if (field->type() == FieldDescriptor::TYPE_STRING &&
      (value[0] != '"' || value[value.size() - 1] != '"')) {
    LOG(ERROR) << "String type value should be quoted: " << value;
    return false;
  }
  return TextFormat::ParseFieldValueFromString(value, field, message);
}

// Parses protobuf from string without validation.
// input sample: context.experimental_features="chrome_omnibox"
// We cannot use TextFormat::ParseFromString since it doesn't allow invalid
// protobuf. (e.g. lack of required field)
bool ParseProtobufFromString(const std::string &text, Message *message) {
  const size_t separator_pos = text.find('=');
  const std::string full_name = text.substr(0, separator_pos);
  const std::string value = text.substr(separator_pos + 1);
  std::vector<std::string> names;
  Util::SplitStringUsing(full_name, ".", &names);

  Message *msg = message;
  for (size_t i = 0; i < names.size() - 1; ++i) {
    const FieldDescriptor *field =
        msg->GetDescriptor()->FindFieldByName(names[i]);
    if (!field) {
      LOG(ERROR) << "Unknown field name: " << names[i];
      return false;
    }
    msg = msg->GetReflection()->MutableMessage(msg, field);
  }

  return SetOrAddFieldValueFromString(names[names.size() - 1], value, msg);
}

#define MOZC_ASSERT_EQ_MSG(expected, actual, message)  \
  if (expected != actual) {                            \
    return mozc::InvalidArgumentError(message);        \
  }
#define MOZC_ASSERT_EQ(expected, actual)    \
  if (expected != actual) {                 \
    return mozc::InvalidArgumentError("");  \
  }
#define MOZC_ASSERT_TRUE_MSG(result, message)    \
  if (!(result)) {                               \
    return mozc::InvalidArgumentError(message);  \
  }
#define MOZC_ASSERT_TRUE(result)            \
  if (!(result)) {                          \
    return mozc::InvalidArgumentError("");  \
  }

Status SessionHandlerInterpreter::ParseLine(const std::string &line_text) {
  if (line_text.empty() || line_text[0] == '#') {
    // Skip an empty or comment line.
    return Status();
  }

  SyncDataToStorage();

  std::vector<std::string> columns;
  Util::SplitStringUsing(line_text, "\t", &columns);
  MOZC_ASSERT_TRUE(!columns.empty());
  const std::string &command = columns[0];
  // TODO(hidehiko): Refactor out about each command when the number of
  //   supported commands is increased.
  if (command == "RESET_CONTEXT") {
    MOZC_ASSERT_EQ(1, columns.size());
    ResetContext();
  } else if (command == "SEND_KEYS") {
    MOZC_ASSERT_EQ(2, columns.size());
    const std::string &keys = columns[1];
    KeyEvent key_event;
    for (size_t i = 0; i < keys.size(); ++i) {
      key_event.Clear();
      key_event.set_key_code(keys[i]);
      MOZC_ASSERT_TRUE_MSG(client_->SendKey(key_event, last_output_.get()),
                           absl::StrCat("Failed at ", i, "th key"));
    }
  } else if (command == "SEND_KANA_KEYS") {
    MOZC_ASSERT_TRUE_MSG(
        columns.size() >= 3,
        absl::StrCat("SEND_KEY requires more than or equal to two columns ",
                     line_text));
    const std::string &keys = columns[1];
    const std::string &kanas = columns[2];
    MOZC_ASSERT_EQ_MSG(
        keys.size(), Util::CharsLen(kanas),
        "1st and 2nd column must have the same number of characters.");
    KeyEvent key_event;
    for (size_t i = 0; i < keys.size(); ++i) {
      key_event.Clear();
      key_event.set_key_code(keys[i]);
      key_event.set_key_string(std::string(Util::Utf8SubString(kanas, i, 1)));
      MOZC_ASSERT_TRUE_MSG(client_->SendKey(key_event, last_output_.get()),
                           absl::StrCat("Failed at ", i, "th ", line_text));
    }
  } else if (command == "SEND_KEY") {
    MOZC_ASSERT_EQ(2, columns.size());
    KeyEvent key_event;
    MOZC_ASSERT_TRUE(KeyParser::ParseKey(columns[1], &key_event));
    MOZC_ASSERT_TRUE(client_->SendKey(key_event, last_output_.get()));
  } else if (command == "SEND_KEY_WITH_OPTION") {
    MOZC_ASSERT_TRUE(columns.size() >= 3);
    KeyEvent key_event;
    MOZC_ASSERT_TRUE(KeyParser::ParseKey(columns[1], &key_event));
    Input option;
    for (size_t i = 2; i < columns.size(); ++i) {
      MOZC_ASSERT_TRUE(ParseProtobufFromString(columns[i], &option));
    }
    MOZC_ASSERT_TRUE(
        client_->SendKeyWithOption(key_event, option, last_output_.get()));
  } else if (command == "TEST_SEND_KEY") {
    MOZC_ASSERT_EQ(2, columns.size());
    KeyEvent key_event;
    MOZC_ASSERT_TRUE(KeyParser::ParseKey(columns[1], &key_event));
    MOZC_ASSERT_TRUE(client_->TestSendKey(key_event, last_output_.get()));
  } else if (command == "TEST_SEND_KEY_WITH_OPTION") {
    MOZC_ASSERT_TRUE(columns.size() >= 3);
    KeyEvent key_event;
    MOZC_ASSERT_TRUE(KeyParser::ParseKey(columns[1], &key_event));
    Input option;
    for (size_t i = 2; i < columns.size(); ++i) {
      MOZC_ASSERT_TRUE(ParseProtobufFromString(columns[i], &option));
    }
    MOZC_ASSERT_TRUE(client_->TestSendKeyWithOption(key_event, option,
                                                    last_output_.get()));
  } else if (command == "SELECT_CANDIDATE") {
    MOZC_ASSERT_EQ(2, columns.size());
    MOZC_ASSERT_TRUE(client_->SelectCandidate(
        NumberUtil::SimpleAtoi(columns[1]), last_output_.get()));
  } else if (command == "SELECT_CANDIDATE_BY_VALUE") {
    MOZC_ASSERT_EQ(2, columns.size());
    uint32 id;
    MOZC_ASSERT_TRUE(GetCandidateIdByValue(columns[1], &id));
    MOZC_ASSERT_TRUE(client_->SelectCandidate(id, last_output_.get()));
  } else if (command == "SUBMIT_CANDIDATE") {
    MOZC_ASSERT_EQ(2, columns.size());
    MOZC_ASSERT_TRUE(client_->SubmitCandidate(
        NumberUtil::SimpleAtoi(columns[1]), last_output_.get()));
  } else if (command == "SUBMIT_CANDIDATE_BY_VALUE") {
    MOZC_ASSERT_EQ(2, columns.size());
    uint32 id;
    MOZC_ASSERT_TRUE(GetCandidateIdByValue(columns[1], &id));
    MOZC_ASSERT_TRUE(client_->SubmitCandidate(id, last_output_.get()));
  } else if (command == "EXPAND_SUGGESTION") {
    MOZC_ASSERT_TRUE(client_->ExpandSuggestion(last_output_.get()));
  } else if (command == "UNDO_OR_REWIND") {
    MOZC_ASSERT_TRUE(client_->UndoOrRewind(last_output_.get()));
  } else if (command == "SWITCH_INPUT_MODE") {
    MOZC_ASSERT_EQ(2, columns.size());
    CompositionMode composition_mode;
    MOZC_ASSERT_TRUE_MSG(CompositionMode_Parse(columns[1], &composition_mode),
                         "Unknown CompositionMode");
    MOZC_ASSERT_TRUE(client_->SwitchInputMode(composition_mode));
  } else if (command == "SET_DEFAULT_REQUEST") {
    request_->CopyFrom(Request::default_instance());
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "SET_MOBILE_REQUEST") {
    RequestForUnitTest::FillMobileRequest(request_.get());
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "SET_REQUEST") {
    MOZC_ASSERT_EQ(3, columns.size());
    MOZC_ASSERT_TRUE(
        SetOrAddFieldValueFromString(columns[1], columns[2], request_.get()));
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "SET_CONFIG") {
    MOZC_ASSERT_EQ(3, columns.size());
    MOZC_ASSERT_TRUE(
        SetOrAddFieldValueFromString(columns[1], columns[2], config_.get()));
    MOZC_ASSERT_TRUE(client_->SetConfig(*config_, last_output_.get()));
  } else if (command == "SET_SELECTION_TEXT") {
    MOZC_ASSERT_EQ(2, columns.size());
    client_->SetCallbackText(columns[1]);
  } else if (command == "UPDATE_MOBILE_KEYBOARD") {
    MOZC_ASSERT_EQ(3, columns.size());
    Request::SpecialRomanjiTable special_romanji_table;
    MOZC_ASSERT_TRUE_MSG(Request::SpecialRomanjiTable_Parse(columns[1],
                                                        &special_romanji_table),
                     "Unknown SpecialRomanjiTable");
    Request::SpaceOnAlphanumeric space_on_alphanumeric;
    MOZC_ASSERT_TRUE_MSG(Request::SpaceOnAlphanumeric_Parse(columns[2],
                                                        &space_on_alphanumeric),
                     "Unknown SpaceOnAlphanumeric");
    request_->set_special_romanji_table(special_romanji_table);
    request_->set_space_on_alphanumeric(space_on_alphanumeric);
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "CLEAR_ALL") {
    MOZC_ASSERT_EQ(1, columns.size());
    ClearAll();
  } else if (command == "CLEAR_USER_PREDICTION") {
    MOZC_ASSERT_EQ(1, columns.size());
    ClearUserPrediction();
  } else if (command == "CLEAR_USAGE_STATS") {
    MOZC_ASSERT_EQ(1, columns.size());
    ClearUsageStats();
  } else {
    return Status(mozc::StatusCode::kUnimplemented, "");
  }

  return Status();
}

}  // namespace session
}  // namespace mozc
