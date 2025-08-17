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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "base/number_util.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/protobuf/text_format.h"
#include "base/strings/assign.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "composer/key_parser.h"
#include "config/character_form_manager.h"
#include "config/config_handler.h"
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/request_test_util.h"
#include "session/session_handler.h"

namespace mozc {
namespace session {
namespace {

using ::mozc::commands::CandidateWord;
using ::mozc::commands::Command;
using ::mozc::commands::CompositionMode;
using ::mozc::commands::Input;
using ::mozc::commands::KeyEvent;
using ::mozc::commands::Output;
using ::mozc::commands::Request;
using ::mozc::config::CharacterFormManager;
using ::mozc::config::Config;
using ::mozc::config::ConfigHandler;
using ::mozc::protobuf::FieldDescriptor;
using ::mozc::protobuf::Message;
using ::mozc::protobuf::TextFormat;
using ::mozc::session::SessionHandlerTool;

std::string ToTextFormat(const Message& proto) {
  return ::mozc::protobuf::Utf8Format(proto);
}

}  // namespace

SessionHandlerTool::SessionHandlerTool(std::unique_ptr<EngineInterface> engine)
    : id_(0),
      engine_(engine.get()),
      handler_(std::make_unique<SessionHandler>(std::move(engine))) {}

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

bool SessionHandlerTool::ClearUserHistory() {
  Command command;
  command.mutable_input()->set_id(id_);
  command.mutable_input()->set_type(commands::Input::CLEAR_USER_HISTORY);
  return handler_->EvalCommand(&command);
}

bool SessionHandlerTool::SendKeyWithOption(const commands::KeyEvent& key,
                                           const commands::Input& option,
                                           commands::Output* output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_KEY);
  *input.mutable_key() = key;
  input.MergeFrom(option);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::TestSendKeyWithOption(const commands::KeyEvent& key,
                                               const commands::Input& option,
                                               commands::Output* output) {
  commands::Input input;
  input.set_type(commands::Input::TEST_SEND_KEY);
  *input.mutable_key() = key;
  input.MergeFrom(option);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::UpdateComposition(absl::Span<const std::string> args,
                                           commands::Output* output) {
  DCHECK_EQ(0, args.size() % 2);
  commands::Input input;
  //  input.set_type(commands::Input::UPDATE_COMPOSITION);
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(
      commands::SessionCommand::UPDATE_COMPOSITION);
  for (int i = 0; i < args.size(); i += 2) {
    commands::SessionCommand::CompositionEvent* composition_event =
        input.mutable_command()->add_composition_events();
    composition_event->set_composition_string(args[i]);
    if (double value = 0.0; NumberUtil::SafeStrToDouble(args[i + 1], &value)) {
      composition_event->set_probability(value);
    }
  }
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SelectCandidate(uint32_t id,
                                         commands::Output* output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::SELECT_CANDIDATE);
  input.mutable_command()->set_id(id);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SubmitCandidate(uint32_t id,
                                         commands::Output* output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::SUBMIT_CANDIDATE);
  input.mutable_command()->set_id(id);
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

bool SessionHandlerTool::UndoOrRewind(commands::Output* output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(commands::SessionCommand::UNDO_OR_REWIND);
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::DeleteCandidateFromHistory(std::optional<int> id,
                                                    commands::Output* output) {
  commands::Input input;
  input.set_type(commands::Input::SEND_COMMAND);
  input.mutable_command()->set_type(
      commands::SessionCommand::DELETE_CANDIDATE_FROM_HISTORY);
  if (id.has_value()) {
    input.mutable_command()->set_id(*id);
  }
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

bool SessionHandlerTool::SetRequest(const commands::Request& request,
                                    commands::Output* output) {
  commands::Input input;
  input.set_type(commands::Input::SET_REQUEST);
  *input.mutable_request() = request;
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SetConfig(const config::Config& config,
                                   commands::Output* output) {
  commands::Input input;
  input.set_type(commands::Input::SET_CONFIG);
  *input.mutable_config() = config;
  return EvalCommand(&input, output);
}

bool SessionHandlerTool::SyncData() {
  engine_->Sync();
  engine_->Wait();
  return true;
}

void SessionHandlerTool::SetCallbackText(const absl::string_view text) {
  strings::Assign(callback_text_, text);
}

bool SessionHandlerTool::ReloadSupplementalModel(absl::string_view model_path) {
  commands::Input input;
  input.mutable_engine_reload_request()->set_file_path(model_path);
  input.set_type(commands::Input::RELOAD_SUPPLEMENTAL_MODEL);
  commands::Output output;
  return EvalCommand(&input, &output);
}

bool SessionHandlerTool::EvalCommandInternal(commands::Input* input,
                                             commands::Output* output,
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

bool SessionHandlerTool::EvalCommand(commands::Input* input,
                                     commands::Output* output) {
  return EvalCommandInternal(input, output, true);
}

SessionHandlerInterpreter::SessionHandlerInterpreter()
    : SessionHandlerInterpreter(EngineFactory::Create().value()) {}

SessionHandlerInterpreter::SessionHandlerInterpreter(
    std::unique_ptr<EngineInterface> engine) {
  client_ = std::make_unique<SessionHandlerTool>(std::move(engine));
  last_output_ = std::make_unique<Output>();
  request_ = std::make_unique<Request>();
  config_ = ConfigHandler::GetCopiedConfig();

  // Set up session.
  CHECK(client_->CreateSession()) << "Client initialization is failed.";
}

SessionHandlerInterpreter::~SessionHandlerInterpreter() {
  // Shut down.
  CHECK(client_->DeleteSession());

  ClearState();
}

void SessionHandlerInterpreter::ClearState() {
  const Config& config = ConfigHandler::DefaultConfig();
  ConfigHandler::SetConfig(config);

  // CharacterFormManager is not automatically updated when the config is
  // updated.
  CharacterFormManager::GetCharacterFormManager()->ReloadConfig(config);

  CHECK(client_->ClearUserHistory());

  // Some destructors may save the state on storages. To clear the state, we
  // explicitly call destructors before clearing storages.
  FileUtil::UnlinkOrLogError(
      ConfigFileStream::GetFileName("user://boundary.db"));
  FileUtil::UnlinkOrLogError(
      ConfigFileStream::GetFileName("user://segment.db"));
}

void SessionHandlerInterpreter::ClearAll() {
  ResetContext();
  ClearUserPrediction();
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
  CHECK(client_->ClearUserHistory());
  SyncDataToStorage();
}

const Output& SessionHandlerInterpreter::LastOutput() const {
  return *last_output_;
}

const CandidateWord& SessionHandlerInterpreter::GetCandidateByValue(
    const absl::string_view value) const {
  const Output& output = LastOutput();

  for (const CandidateWord& candidate :
       output.all_candidate_words().candidates()) {
    if (candidate.value() == value) {
      return candidate;
    }
  }

  for (const CandidateWord& candidate :
       output.removed_candidate_words_for_debug().candidates()) {
    if (candidate.value() == value) {
      return candidate;
    }
  }

  static absl::NoDestructor<CandidateWord> fallback_candidate;
  return *fallback_candidate;
}

bool SessionHandlerInterpreter::GetCandidateIdByValue(
    const absl::string_view value, uint32_t* id) const {
  const Output& output = LastOutput();
  if (!output.has_all_candidate_words()) {
    return false;
  }

  for (const CandidateWord& candidate :
       output.all_candidate_words().candidates()) {
    if (candidate.has_value() && candidate.value() == value) {
      *id = candidate.id();
      return true;
    }
  }
  return false;
}

std::vector<uint32_t> SessionHandlerInterpreter::GetCandidateIdsByValue(
    absl::string_view value) const {
  const Output& output = LastOutput();
  if (!output.has_all_candidate_words()) {
    return {};
  }

  std::vector<uint32_t> ids;
  for (const CandidateWord& candidate :
       output.all_candidate_words().candidates()) {
    if (candidate.has_value() && candidate.value() == value) {
      ids.push_back(candidate.id());
    }
  }
  return ids;
}

std::vector<uint32_t> SessionHandlerInterpreter::GetRemovedCandidateIdsByValue(
    absl::string_view value) const {
  const Output& output = LastOutput();
  if (!output.has_removed_candidate_words_for_debug()) {
    return {};
  }

  std::vector<uint32_t> ids;
  for (const CandidateWord& candidate :
       output.removed_candidate_words_for_debug().candidates()) {
    if (candidate.has_value() && candidate.value() == value) {
      ids.push_back(candidate.id());
    }
  }
  return ids;
}

bool SetOrAddFieldValueFromString(const absl::string_view name,
                                  const absl::string_view value,
                                  Message* message) {
  const FieldDescriptor* field =
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

bool SetOrAddFieldValueFromString(const absl::Span<const std::string> names,
                                  const absl::string_view value,
                                  Message* message) {
  if (names.empty()) {
    LOG(ERROR) << "Empty names is passed";
    return false;
  }
  const std::string& first = names[0];
  if (names.size() == 1) {
    return SetOrAddFieldValueFromString(first, value, message);
  }
  const FieldDescriptor* field =
      message->GetDescriptor()->FindFieldByName(first);
  Message* field_message =
      message->GetReflection()->MutableMessage(message, field);
  return SetOrAddFieldValueFromString(names.subspan(1), value, field_message);
}

// Parses protobuf from string without validation.
// input sample: context.experimental_features="chrome_omnibox"
// We cannot use TextFormat::ParseFromString since it doesn't allow invalid
// protobuf. (e.g. lack of required field)
bool ParseProtobufFromString(const absl::string_view text, Message* message) {
  const size_t separator_pos = text.find('=');
  const absl::string_view full_name = text.substr(0, separator_pos);
  const absl::string_view value = text.substr(separator_pos + 1);
  std::vector<absl::string_view> names =
      absl::StrSplit(full_name, '.', absl::SkipEmpty());

  Message* msg = message;
  for (size_t i = 0; i < names.size() - 1; ++i) {
    const FieldDescriptor* field =
        msg->GetDescriptor()->FindFieldByName(names[i]);
    if (!field) {
      LOG(ERROR) << "Unknown field name: " << names[i];
      return false;
    }
    msg = msg->GetReflection()->MutableMessage(msg, field);
  }

  return SetOrAddFieldValueFromString(names.back(), value, msg);
}

std::vector<std::string> SessionHandlerInterpreter::Parse(
    const absl::string_view line) {
  std::vector<std::string> args;
  if (line.empty() || line.front() == '#') {
    return args;
  }
  std::vector<absl::string_view> columns = absl::StrSplit(line, '\t');
  for (absl::string_view column : columns) {
    if (column.empty()) {
      args.emplace_back();
      continue;
    }
    // If the first and last characters are doublequotes, trim them.
    if (column.size() > 1 && column.front() == '"' && column.back() == '"') {
      column.remove_prefix(1);
      column.remove_suffix(1);
    }
    args.emplace_back(column);
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

#define MOZC_EXPECT_EQ_MSG(expected, actual, message) \
  if ((expected) != (actual)) {                       \
    return absl::InternalError(message);              \
  }
#define MOZC_EXPECT_EQ(expected, actual) \
  if ((expected) != (actual)) {          \
    return absl::InternalError("");      \
  }

#define MOZC_EXPECT_TRUE_MSG(result, message) \
  if (!(result)) {                            \
    return absl::InternalError(message);      \
  }
#define MOZC_EXPECT_TRUE(result)    \
  if (!(result)) {                  \
    return absl::InternalError(""); \
  }

// Placeholders
#define MOZC_EXPECT_STATS_NOT_EXIST(name) \
  while (false) {                         \
  }
#define MOZC_EXPECT_COUNT_STATS(name, value) \
  while (false) {                            \
  }
#define MOZC_EXPECT_INTEGER_STATS(name, value) \
  while (false) {                              \
  }
#define MOZC_EXPECT_BOOLEAN_STATS(name, value) \
  while (false) {                              \
  }
// Uses args to suppress compiler warnings.
#define MOZC_EXPECT_TIMING_STATS(name, total, num, min, max) \
  while (false && total && num && min && max) {              \
  }

absl::Status SessionHandlerInterpreter::Eval(
    const absl::Span<const std::string> args) {
  if (args.empty()) {
    // Skip empty args
    return absl::Status();
  }

  SyncDataToStorage();

  const std::string& command = args[0];
  // TODO(hidehiko): Refactor out about each command when the number of
  //   supported commands is increased.
  if (command == "RESET_CONTEXT") {
    MOZC_ASSERT_EQ(1, args.size());
    ResetContext();
  } else if (command == "SEND_KEYS") {
    MOZC_ASSERT_EQ(2, args.size());
    const std::string& keys = args[1];
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
    const std::string& keys = args[1];
    const std::string& kanas = args[2];
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
  } else if (command == "UPDATE_COMPOSITION") {
    MOZC_ASSERT_EQ(1, args.size() % 2);
    MOZC_ASSERT_TRUE(
        client_->UpdateComposition(args.subspan(1), last_output_.get()));
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
  } else if (command == "UNDO_OR_REWIND") {
    MOZC_ASSERT_TRUE(client_->UndoOrRewind(last_output_.get()));
  } else if (command == "DELETE_CANDIDATE_FROM_HISTORY") {
    MOZC_ASSERT_TRUE(args.size() == 1 || args.size() == 2);
    std::optional<int> id = std::nullopt;
    if (args.size() == 2) {
      id = NumberUtil::SimpleAtoi(args[1]);
    }
    MOZC_ASSERT_TRUE(
        client_->DeleteCandidateFromHistory(id, last_output_.get()));
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
    request_test_util::FillMobileRequest(request_.get());
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "SET_HANDWRITING_REQUEST") {
    request_test_util::FillMobileRequestForHandwriting(request_.get());
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "SET_REQUEST") {
    MOZC_ASSERT_TRUE(args.size() >= 3);
    MOZC_ASSERT_TRUE(SetOrAddFieldValueFromString(
        std::vector<std::string>(args.begin() + 1, args.end() - 1),
        *(args.end() - 1), request_.get()));
    MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
  } else if (command == "SET_CONFIG") {
    MOZC_ASSERT_TRUE(args.size() >= 3);
    MOZC_ASSERT_TRUE(SetOrAddFieldValueFromString(
        std::vector<std::string>(args.begin() + 1, args.end() - 1),
        *(args.end() - 1), &config_));
    MOZC_ASSERT_TRUE(client_->SetConfig(config_, last_output_.get()));
  } else if (command == "MERGE_DECODER_EXPERIMENT_PARAMS") {
    MOZC_ASSERT_EQ(2, args.size());
    if (const std::string& textproto = args[1]; !textproto.empty()) {
      mozc::commands::DecoderExperimentParams params;
      CHECK(mozc::protobuf::TextFormat::ParseFromString(textproto, &params))
          << "Invalid DecoderExperimentParams: " << textproto;
      request_->mutable_decoder_experiment_params()->MergeFrom(params);
      LOG(INFO) << "DecoderExperimentParams was set:\n"
                << request_->decoder_experiment_params();
      MOZC_ASSERT_TRUE(client_->SetRequest(*request_, last_output_.get()));
    }
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
  } else if (command == "EXPECT_CONSUMED") {
    MOZC_ASSERT_EQ(args.size(), 2);
    MOZC_ASSERT_TRUE(last_output_->has_consumed());
    MOZC_EXPECT_EQ(last_output_->consumed(), args[1] == "true");
  } else if (command == "EXPECT_PREEDIT") {
    // Concat preedit segments and assert.
    const std::string& expected_preedit =
        TextNormalizer::NormalizeText(args.size() == 1 ? "" : args[1]);
    std::string preedit_string;
    const mozc::commands::Preedit& preedit = last_output_->preedit();
    for (int i = 0; i < preedit.segment_size(); ++i) {
      preedit_string += preedit.segment(i).value();
    }
    MOZC_EXPECT_EQ_MSG(
        preedit_string, expected_preedit,
        absl::StrCat("Expected preedit: ", expected_preedit, "\n",
                     "Actual preedit: ", ToTextFormat(preedit)));
  } else if (command == "EXPECT_PREEDIT_IN_DETAIL") {
    MOZC_ASSERT_TRUE(!args.empty());
    const mozc::commands::Preedit& preedit = last_output_->preedit();
    MOZC_ASSERT_EQ(preedit.segment_size(), args.size() - 1);
    for (int i = 0; i < preedit.segment_size(); ++i) {
      MOZC_EXPECT_EQ_MSG(preedit.segment(i).value(),
                         TextNormalizer::NormalizeText(args[i + 1]),
                         absl::StrCat("Segment index = ", i));
    }
  } else if (command == "EXPECT_PREEDIT_CURSOR_POS") {
    // Concat preedit segments and assert.
    MOZC_ASSERT_EQ(args.size(), 2);
    const size_t expected_pos = NumberUtil::SimpleAtoi(args[1]);
    const mozc::commands::Preedit& preedit = last_output_->preedit();
    MOZC_EXPECT_EQ_MSG(preedit.cursor(), expected_pos, ToTextFormat(preedit));
  } else if (command == "EXPECT_CANDIDATE") {
    MOZC_ASSERT_EQ(args.size(), 3);
    uint32_t candidate_id = 0;
    const bool has_result = GetCandidateIdByValue(args[2], &candidate_id);
    MOZC_EXPECT_TRUE_MSG(
        has_result,
        absl::StrCat(args[2], " is not found\n",
                     ToTextFormat(last_output_->candidate_window())));
    if (has_result) {
      MOZC_EXPECT_EQ_MSG(
          candidate_id, NumberUtil::SimpleAtoi(args[1]),
          absl::StrCat(args[1], " is not found\n",
                       ToTextFormat(last_output_->candidate_window())));
    }
  } else if (command == "EXPECT_CANDIDATE_DESCRIPTION") {
    MOZC_ASSERT_EQ(args.size(), 3);
    const CandidateWord& cand = GetCandidateByValue(args[1]);
    const bool has_cand = !cand.value().empty();
    MOZC_EXPECT_TRUE_MSG(
        has_cand, absl::StrCat(args[1], " is not found\n",
                               ToTextFormat(last_output_->candidate_window())));
    MOZC_EXPECT_TRUE(has_cand);
    MOZC_EXPECT_EQ_MSG(cand.annotation().description(), args[2],
                       ToTextFormat(cand));
  } else if (command == "EXPECT_RESULT") {
    if (args.size() == 2 && !args[1].empty()) {
      MOZC_ASSERT_TRUE(last_output_->has_result());
      const mozc::commands::Result& result = last_output_->result();
      MOZC_EXPECT_EQ_MSG(result.value(), TextNormalizer::NormalizeText(args[1]),
                         ToTextFormat(result));
    } else {
      MOZC_EXPECT_TRUE_MSG(!last_output_->has_result(),
                           ToTextFormat(last_output_->result()));
    }
  } else if (command == "EXPECT_IN_ALL_CANDIDATE_WORDS") {
    MOZC_ASSERT_EQ(args.size(), 2);
    uint32_t candidate_id = 0;
    const bool has_result = GetCandidateIdByValue(args[1], &candidate_id);
    MOZC_EXPECT_TRUE_MSG(has_result, absl::StrCat(args[1], " is not found.\n",
                                                  ToTextFormat(*last_output_)));
  } else if (command == "EXPECT_NOT_IN_ALL_CANDIDATE_WORDS") {
    MOZC_ASSERT_EQ(args.size(), 2);
    uint32_t candidate_id = 0;
    const bool has_result = GetCandidateIdByValue(args[1], &candidate_id);
    MOZC_EXPECT_TRUE_MSG(
        !has_result,
        absl::StrCat(args[1], " is found.\n", ToTextFormat(*last_output_)));
  } else if (command == "EXPECT_HAS_CANDIDATES") {
    if (args.size() == 2 && !args[1].empty()) {
      MOZC_ASSERT_TRUE(last_output_->has_candidate_window());
      MOZC_ASSERT_TRUE_MSG(last_output_->candidate_window().size() >
                               NumberUtil::SimpleAtoi(args[1]),
                           ToTextFormat(*last_output_));
    } else {
      MOZC_ASSERT_TRUE(last_output_->has_candidate_window());
    }
  } else if (command == "EXPECT_NO_CANDIDATES") {
    MOZC_ASSERT_TRUE(!last_output_->has_candidate_window());
  } else if (command == "EXPECT_SEGMENTS_SIZE") {
    MOZC_ASSERT_EQ(args.size(), 2);
    MOZC_ASSERT_EQ(last_output_->preedit().segment_size(),
                   NumberUtil::SimpleAtoi(args[1]));
  } else if (command == "EXPECT_HIGHLIGHTED_SEGMENT_INDEX") {
    MOZC_ASSERT_EQ(args.size(), 2);
    MOZC_ASSERT_TRUE(last_output_->has_preedit());
    const mozc::commands::Preedit& preedit = last_output_->preedit();
    int index = -1;
    for (int i = 0; i < preedit.segment_size(); ++i) {
      if (preedit.segment(i).annotation() ==
          mozc::commands::Preedit::Segment::HIGHLIGHT) {
        index = i;
        break;
      }
    }
    MOZC_ASSERT_EQ(index, NumberUtil::SimpleAtoi(args[1]));
  } else {
    return absl::Status(absl::StatusCode::kUnimplemented, "");
  }

  return absl::OkStatus();
}

void SessionHandlerInterpreter::SetRequest(const commands::Request& request) {
  *request_ = request;
}

void SessionHandlerInterpreter::ReloadSupplementalModel(
    absl::string_view model_path) {
  client_->ReloadSupplementalModel(model_path);
}

}  // namespace session
}  // namespace mozc
