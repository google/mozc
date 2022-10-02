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

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/number_util.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/protobuf/text_format.h"
#include "base/util.h"
#include "composer/key_parser.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "engine/engine_factory.h"
#include "engine/user_data_manager_interface.h"
#include "prediction/user_history_predictor.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/request_test_util.h"
#include "session/session_handler.h"
#include "session/session_handler_interface.h"
#include "session/session_usage_observer.h"
#include "storage/registry.h"
#include "usage_stats/usage_stats.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace session {

using commands::CandidateList;
using commands::CandidateWord;
using commands::Command;
using commands::CompositionMode;
using commands::Input;
using commands::KeyEvent;
using commands::Output;
using commands::Request;
using commands::RequestForUnitTest;
using config::CharacterFormManager;
using config::Config;
using config::ConfigHandler;
using protobuf::FieldDescriptor;
using protobuf::Message;
using protobuf::TextFormat;
using session::SessionHandlerTool;

bool CreateSession(SessionHandlerInterface *handler, uint64_t *id) {
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

bool DeleteSession(SessionHandlerInterface *handler, uint64_t id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::DELETE_SESSION);
  return handler->EvalCommand(&command);
}

bool CleanUp(SessionHandlerInterface *handler, uint64_t id) {
  Command command;
  command.mutable_input()->set_id(id);
  command.mutable_input()->set_type(commands::Input::CLEANUP);
  return handler->EvalCommand(&command);
}

bool IsGoodSession(SessionHandlerInterface *handler, uint64_t id) {
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
  *input.mutable_key() = key;
  input.MergeFrom(option);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::TestSendKeyWithOption(const commands::KeyEvent &key,
                                               const commands::Input &option,
                                               commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::TEST_SEND_KEY);
  *input.mutable_key() = key;
  input.MergeFrom(option);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SelectCandidate(uint32_t id,
                                         commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::SELECT_CANDIDATE);
  input.mutable_command()->set_id(id);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SubmitCandidate(uint32_t id,
                                         commands::Output *output) {
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
  *input.mutable_request() = request;
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SetConfig(const config::Config &config,
                                   commands::Output *output) {
  commands::Input input;
  input.set_type(commands::Input::SET_CONFIG);
  *input.mutable_config() = config;
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SyncData() { return data_manager_->Wait(); }

void SessionHandlerTool::SetCallbackText(const std::string &text) {
  callback_text_ = text;
}

bool SessionHandlerTool::EvalCommandInternal(commands::Input *input,
                                             commands::Output *output,
                                             bool allow_callback) {
  input->set_id(id_);
  commands::Command command;
  *command.mutable_input() = *input;
  bool result = handler_->EvalCommand(&command);
  if (result && output != nullptr) {
    *output = command.output();
  }

  // If callback is allowed and the callback field exists, evaluate the callback
  // command.
  if (result && allow_callback && command.output().has_callback() &&
      command.output().callback().has_session_command()) {
    commands::Input input2;
    input2.set_type(commands::Input::SEND_COMMAND);
    *input2.mutable_command() = command.output().callback().session_command();
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
    : SessionHandlerInterpreter(EngineFactory::Create().value()) {}

SessionHandlerInterpreter::SessionHandlerInterpreter(
    std::unique_ptr<EngineInterface> engine) {
  client_ = std::make_unique<SessionHandlerTool>(std::move(engine));
  config_ = std::make_unique<Config>();
  last_output_ = std::make_unique<Output>();
  request_ = std::make_unique<Request>();

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
  FileUtil::UnlinkOrLogError(
      mozc::ConfigFileStream::GetFileName("user://boundary.db"));
  FileUtil::UnlinkOrLogError(
      mozc::ConfigFileStream::GetFileName("user://segment.db"));
  FileUtil::UnlinkOrLogError(
      mozc::UserHistoryPredictor::GetUserHistoryFileName());
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

const Output &SessionHandlerInterpreter::LastOutput() const {
  return *last_output_;
}

const CandidateWord &SessionHandlerInterpreter::GetCandidateByValue(
    const absl::string_view value) {
  const Output &output = LastOutput();

  for (const CandidateWord &candidate :
       output.all_candidate_words().candidates()) {
    if (candidate.value() == value) {
      return candidate;
    }
  }

  for (const CandidateWord &candidate :
       output.removed_candidate_words_for_debug().candidates()) {
    if (candidate.value() == value) {
      return candidate;
    }
  }

  static CandidateWord *fallback_candidate = new CandidateWord;
  return *fallback_candidate;
}

bool SessionHandlerInterpreter::GetCandidateIdByValue(
    const absl::string_view value, uint32_t *id) {
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
  std::vector<std::string> names =
      absl::StrSplit(full_name, '.', absl::SkipEmpty());

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

std::vector<std::string> SessionHandlerInterpreter::Parse(
    const std::string &line) {
  std::vector<std::string> args;
  if (line.empty() || line.front() == '#') {
    return args;
  }
  std::vector<absl::string_view> columns = absl::StrSplit(line, '\t');
  for (absl::string_view column : columns) {
    if (column.empty()) {
      args.push_back("");
      continue;
    }
    // If the first and last characters are doublequotes, trim them.
    if (column.size() > 1 && column.front() == '"' && column.back() == '"') {
      column.remove_prefix(1);
      column.remove_suffix(1);
    }
    args.push_back(std::string(column));
  }
  return args;
}

#define MOZC_ASSERT_EQ_MSG(expected, actual, message) \
  if (expected != actual) {                           \
    return absl::InvalidArgumentError(message);       \
  }
#define MOZC_ASSERT_EQ(expected, actual)   \
  if (expected != actual) {                \
    return absl::InvalidArgumentError(""); \
  }
#define MOZC_ASSERT_TRUE_MSG(result, message)   \
  if (!(result)) {                              \
    return absl::InvalidArgumentError(message); \
  }
#define MOZC_ASSERT_TRUE(result)           \
  if (!(result)) {                         \
    return absl::InvalidArgumentError(""); \
  }

absl::Status SessionHandlerInterpreter::Eval(
    const std::vector<std::string> &args) {
  if (args.empty()) {
    // Skip empty args
    return absl::Status();
  }

  SyncDataToStorage();

  const std::string &command = args[0];
  // TODO(hidehiko): Refactor out about each command when the number of
  //   supported commands is increased.
  if (command == "RESET_CONTEXT") {
    MOZC_ASSERT_EQ(1, args.size());
    ResetContext();
  } else if (command == "SEND_KEYS") {
    MOZC_ASSERT_EQ(2, args.size());
    const std::string &keys = args[1];
    KeyEvent key_event;
    for (size_t i = 0; i < keys.size(); ++i) {
      key_event.Clear();
      key_event.set_key_code(keys[i]);
      MOZC_ASSERT_TRUE_MSG(client_->SendKey(key_event, last_output_.get()),
                           absl::StrCat("Failed at ", i, "th key"));
    }
  } else if (command == "SEND_KANA_KEYS") {
    MOZC_ASSERT_TRUE_MSG(
        args.size() >= 3,
        absl::StrCat("SEND_KEY requires more than or equal to two args ",
                     absl::StrJoin(args, "\t")));
    const std::string &keys = args[1];
    const std::string &kanas = args[2];
    MOZC_ASSERT_EQ_MSG(
        keys.size(), Util::CharsLen(kanas),
        "1st and 2nd column must have the same number of characters.");
    KeyEvent key_event;
    for (size_t i = 0; i < keys.size(); ++i) {
      key_event.Clear();
      key_event.set_key_code(keys[i]);
      key_event.set_key_string(std::string(Util::Utf8SubString(kanas, i, 1)));
      MOZC_ASSERT_TRUE_MSG(
          client_->SendKey(key_event, last_output_.get()),
          absl::StrCat("Failed at ", i, "th ", absl::StrJoin(args, "\t")));
    }
  } else if (command == "SEND_KEY") {
    MOZC_ASSERT_EQ(2, args.size());
    KeyEvent key_event;
    MOZC_ASSERT_TRUE(KeyParser::ParseKey(args[1], &key_event));
    MOZC_ASSERT_TRUE(client_->SendKey(key_event, last_output_.get()));
  } else if (command == "SEND_KEY_WITH_OPTION") {
    MOZC_ASSERT_TRUE(args.size() >= 3);
    KeyEvent key_event;
    MOZC_ASSERT_TRUE(KeyParser::ParseKey(args[1], &key_event));
    Input option;
    for (size_t i = 2; i < args.size(); ++i) {
      MOZC_ASSERT_TRUE(ParseProtobufFromString(args[i], &option));
    }
    MOZC_ASSERT_TRUE(
        client_->SendKeyWithOption(key_event, option, last_output_.get()));
  } else if (command == "TEST_SEND_KEY") {
    MOZC_ASSERT_EQ(2, args.size());
    KeyEvent key_event;
    MOZC_ASSERT_TRUE(KeyParser::ParseKey(args[1], &key_event));
    MOZC_ASSERT_TRUE(client_->TestSendKey(key_event, last_output_.get()));
  } else if (command == "TEST_SEND_KEY_WITH_OPTION") {
    MOZC_ASSERT_TRUE(args.size() >= 3);
    KeyEvent key_event;
    MOZC_ASSERT_TRUE(KeyParser::ParseKey(args[1], &key_event));
    Input option;
    for (size_t i = 2; i < args.size(); ++i) {
      MOZC_ASSERT_TRUE(ParseProtobufFromString(args[i], &option));
    }
    MOZC_ASSERT_TRUE(
        client_->TestSendKeyWithOption(key_event, option, last_output_.get()));
  } else if (command == "SELECT_CANDIDATE") {
    MOZC_ASSERT_EQ(2, args.size());
    MOZC_ASSERT_TRUE(client_->SelectCandidate(NumberUtil::SimpleAtoi(args[1]),
                                              last_output_.get()));
  } else if (command == "SELECT_CANDIDATE_BY_VALUE") {
    MOZC_ASSERT_EQ(2, args.size());
    uint32_t id;
    MOZC_ASSERT_TRUE(GetCandidateIdByValue(args[1], &id));
    MOZC_ASSERT_TRUE(client_->SelectCandidate(id, last_output_.get()));
  } else if (command == "SUBMIT_CANDIDATE") {
    MOZC_ASSERT_EQ(2, args.size());
    MOZC_ASSERT_TRUE(client_->SubmitCandidate(NumberUtil::SimpleAtoi(args[1]),
                                              last_output_.get()));
  } else if (command == "SUBMIT_CANDIDATE_BY_VALUE") {
    MOZC_ASSERT_EQ(2, args.size());
    uint32_t id;
    MOZC_ASSERT_TRUE(GetCandidateIdByValue(args[1], &id));
    MOZC_ASSERT_TRUE(client_->SubmitCandidate(id, last_output_.get()));
  } else if (command == "EXPAND_SUGGESTION") {
    MOZC_ASSERT_TRUE(client_->ExpandSuggestion(last_output_.get()));
  } else if (command == "UNDO_OR_REWIND") {
    MOZC_ASSERT_TRUE(client_->UndoOrRewind(last_output_.get()));
  } else if (command == "SWITCH_INPUT_MODE") {
    MOZC_ASSERT_EQ(2, args.size());
    CompositionMode composition_mode;
    MOZC_ASSERT_TRUE_MSG(CompositionMode_Parse(args[1], &composition_mode),
                         "Unknown CompositionMode");
    MOZC_ASSERT_TRUE(client_->SwitchInputMode(composition_mode));
  } else if (command == "SET_DEFAULT_REQUEST") {
    *request_ = Request::default_instance();
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "SET_MOBILE_REQUEST") {
    RequestForUnitTest::FillMobileRequest(request_.get());
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "SET_REQUEST") {
    MOZC_ASSERT_EQ(3, args.size());
    MOZC_ASSERT_TRUE(
        SetOrAddFieldValueFromString(args[1], args[2], request_.get()));
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "SET_CONFIG") {
    MOZC_ASSERT_EQ(3, args.size());
    MOZC_ASSERT_TRUE(
        SetOrAddFieldValueFromString(args[1], args[2], config_.get()));
    MOZC_ASSERT_TRUE(client_->SetConfig(*config_, last_output_.get()));
  } else if (command == "SET_SELECTION_TEXT") {
    MOZC_ASSERT_EQ(2, args.size());
    client_->SetCallbackText(args[1]);
  } else if (command == "UPDATE_MOBILE_KEYBOARD") {
    MOZC_ASSERT_EQ(3, args.size());
    Request::SpecialRomanjiTable special_romanji_table;
    MOZC_ASSERT_TRUE_MSG(
        Request::SpecialRomanjiTable_Parse(args[1], &special_romanji_table),
        "Unknown SpecialRomanjiTable");
    Request::SpaceOnAlphanumeric space_on_alphanumeric;
    MOZC_ASSERT_TRUE_MSG(
        Request::SpaceOnAlphanumeric_Parse(args[2], &space_on_alphanumeric),
        "Unknown SpaceOnAlphanumeric");
    request_->set_special_romanji_table(special_romanji_table);
    request_->set_space_on_alphanumeric(space_on_alphanumeric);
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "CLEAR_ALL") {
    MOZC_ASSERT_EQ(1, args.size());
    ClearAll();
  } else if (command == "CLEAR_USER_PREDICTION") {
    MOZC_ASSERT_EQ(1, args.size());
    ClearUserPrediction();
  } else if (command == "CLEAR_USAGE_STATS") {
    MOZC_ASSERT_EQ(1, args.size());
    ClearUsageStats();
  } else {
    return absl::Status(absl::StatusCode::kUnimplemented, "");
  }

  return absl::Status();
}

void SessionHandlerInterpreter::SetRequest(const commands::Request &request) {
  *request_ = request;
}

}  // namespace session
}  // namespace mozc
