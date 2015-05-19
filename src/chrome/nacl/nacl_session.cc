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

// TODO(horo): write tests.

#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>
#include <ppapi/cpp/var.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#include "base/logging.h"
#include "base/protobuf/descriptor.h"
#include "base/util.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "net/jsoncpp.h"
#include "session/commands.pb.h"
#include "session/session.h"
#include "session/key_parser.h"
#include "session/request_handler.h"

using mozc::commands::SessionCommand;
using mozc::commands::CompositionMode;
using mozc::protobuf::Message;
using mozc::protobuf::Reflection;
using mozc::protobuf::FieldDescriptor;

namespace mozc {
namespace session {
namespace {

Json::Value ProtobufRepeatedFieldValueToJsonValue(
    const Message &message, const Reflection &reflection,
    const FieldDescriptor &field, int index);

Json::Value ProtobufFieldValueToJsonValue(
    const Message &message, const Reflection &reflection,
    const FieldDescriptor &field);

Json::Value ProtobufMessageToJsonValue(
    const Message &message) {
  Json::Value value(Json::objectValue);
  const Reflection *reflection = message.GetReflection();
  vector<const FieldDescriptor*> fields;
  reflection->ListFields(message, &fields);

  for (size_t i = 0; i < fields.size(); ++i) {
    if (fields[i]->is_repeated()) {
      Json::Value items(Json::arrayValue);
      const int count = reflection->FieldSize(message, fields[i]);
      for (int j = 0; j < count; ++j) {
        const Json::Value item =
            ProtobufRepeatedFieldValueToJsonValue(
                message, *reflection, *fields[i], j);
        items.append(item);
      }
      value[fields[i]->name()] = items;
    } else {
      value[fields[i]->name()] =
          ProtobufFieldValueToJsonValue(message, *reflection, *fields[i]);
    }
  }
  return value;
}

Json::Value ProtobufRepeatedFieldValueToJsonValue(
    const Message &message, const Reflection &reflection,
    const FieldDescriptor &field, int index) {
  switch (field.cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return Json::Value(reflection.GetRepeatedInt32(message, &field, index));
    case FieldDescriptor::CPPTYPE_INT64:
      return Json::Value(reflection.GetRepeatedInt64(message, &field, index));
    case FieldDescriptor::CPPTYPE_UINT32:
      return Json::Value(reflection.GetRepeatedUInt32(message, &field, index));
    case FieldDescriptor::CPPTYPE_UINT64:
      return Json::Value(reflection.GetRepeatedUInt64(message, &field, index));
    case FieldDescriptor::CPPTYPE_FLOAT:
      return Json::Value(reflection.GetRepeatedFloat(message, &field, index));
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return Json::Value(reflection.GetRepeatedDouble(message, &field, index));
    case FieldDescriptor::CPPTYPE_BOOL:
      return Json::Value(reflection.GetRepeatedBool(message, &field, index));
    case FieldDescriptor::CPPTYPE_ENUM:
      return Json::Value(
          reflection.GetRepeatedEnum(message, &field, index)->name());
    case FieldDescriptor::CPPTYPE_STRING: {
      string scratch;
      const string &str = reflection.GetRepeatedStringReference(
                message, &field, index, &scratch);
      return Json::Value(str);
    }
    case FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtobufMessageToJsonValue(
          reflection.GetRepeatedMessage(message, &field, index));
    default:
      DLOG(WARNING) << "unsupported filed CppType: " << field.cpp_type();
      break;
  }
  return Json::Value();
}

Json::Value ProtobufFieldValueToJsonValue(
    const Message &message,
    const Reflection &reflection,
    const FieldDescriptor &field) {
  switch (field.cpp_type()) {
    case FieldDescriptor::CPPTYPE_INT32:
      return Json::Value(reflection.GetInt32(message, &field));
    case FieldDescriptor::CPPTYPE_INT64:
      return Json::Value(reflection.GetInt64(message, &field));
    case FieldDescriptor::CPPTYPE_UINT32:
      return Json::Value(reflection.GetUInt32(message, &field));
    case FieldDescriptor::CPPTYPE_UINT64:
      return Json::Value(reflection.GetUInt64(message, &field));
    case FieldDescriptor::CPPTYPE_FLOAT:
      return Json::Value(reflection.GetFloat(message, &field));
    case FieldDescriptor::CPPTYPE_DOUBLE:
      return Json::Value(reflection.GetDouble(message, &field));
    case FieldDescriptor::CPPTYPE_BOOL:
      return Json::Value(reflection.GetBool(message, &field));
    case FieldDescriptor::CPPTYPE_ENUM:
      return Json::Value(reflection.GetEnum(message, &field)->name());
    case FieldDescriptor::CPPTYPE_STRING: {
      string scratch;
      const string &str = reflection.GetStringReference(
                message, &field, &scratch);
      return Json::Value(str);
    }
    case FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtobufMessageToJsonValue(
          reflection.GetMessage(message, &field));
    default:
      DLOG(WARNING) << "unsupported filed CppType: " << field.cpp_type();
      break;
  }
  return Json::Value();
}

typedef pair<string, SessionCommand::CommandType> CommandTypePair;
typedef pair<string, CompositionMode> CompositionModePair;
typedef pair<string, SessionCommand::InputFieldType> InputFieldTypePair;

typedef map<string, SessionCommand::CommandType> CommandTypeMap;
typedef map<string, CompositionMode> CompositionModeMap;
typedef map<string, SessionCommand::InputFieldType> InputFieldTypeMap;

static const CommandTypePair kCommandTypePairs[] = {
  make_pair("REVERT", SessionCommand::REVERT),
  make_pair("SUBMIT", SessionCommand::SUBMIT),
  make_pair("SELECT_CANDIDATE", SessionCommand::SELECT_CANDIDATE),
  make_pair("HIGHLIGHT_CANDIDATE", SessionCommand::HIGHLIGHT_CANDIDATE),
  make_pair("SWITCH_INPUT_MODE", SessionCommand::SWITCH_INPUT_MODE),
  make_pair("GET_STATUS", SessionCommand::GET_STATUS),
  make_pair("SUBMIT_CANDIDATE", SessionCommand::SUBMIT_CANDIDATE),
  make_pair("CONVERT_REVERSE", SessionCommand::CONVERT_REVERSE),
  make_pair("UNDO", SessionCommand::UNDO),
  make_pair("RESET_CONTEXT", SessionCommand::RESET_CONTEXT),
  make_pair("MOVE_CURSOR", SessionCommand::MOVE_CURSOR),
  make_pair("SWITCH_INPUT_FIELD_TYPE", SessionCommand::SWITCH_INPUT_FIELD_TYPE),
  make_pair("USAGE_STATS_EVENT", SessionCommand::USAGE_STATS_EVENT),
  make_pair("UNDO_OR_REWIND", SessionCommand::UNDO_OR_REWIND),
  make_pair("EXPAND_SUGGESTION", SessionCommand::EXPAND_SUGGESTION)
};

static const CompositionModePair kCompositionModePairs[] = {
  make_pair("DIRECT", commands::DIRECT),
  make_pair("HIRAGANA", commands::HIRAGANA),
  make_pair("FULL_KATAKANA", commands::FULL_KATAKANA),
  make_pair("HALF_ASCII", commands::HALF_ASCII),
  make_pair("FULL_ASCII", commands::FULL_ASCII),
  make_pair("HALF_KATAKANA", commands::HALF_KATAKANA)
};

static const InputFieldTypePair kInputFieldTypePairs[] = {
  make_pair("NORMAL", SessionCommand::NORMAL),
  make_pair("PASSWORD", SessionCommand::PASSWORD),
  make_pair("TEL", SessionCommand::TEL),
  make_pair("NUMBER", SessionCommand::NUMBER)
};

static const CommandTypeMap kCommandTypeMap =
    CommandTypeMap(kCommandTypePairs,
                   kCommandTypePairs + arraysize(kCommandTypePairs));
static const CompositionModeMap kCompositionModeMap =
    CompositionModeMap(
        kCompositionModePairs,
        kCompositionModePairs + arraysize(kCompositionModePairs));
static const InputFieldTypeMap kInputFieldTypeMap =
    InputFieldTypeMap(kInputFieldTypePairs,
                      kInputFieldTypePairs + arraysize(kInputFieldTypePairs));

}  // namespace

class NaclSessionInstance : public pp::Instance {
 public:
  explicit NaclSessionInstance(PP_Instance instance);
  virtual ~NaclSessionInstance() {}
  virtual void HandleMessage(const pp::Var &var_message);

 private:
  void SendKey(const Json::Value &request);
  void SendCommand(const Json::Value &request);
  scoped_ptr<Session> session_;
  scoped_ptr<mozc::composer::Table> table_;
  DISALLOW_COPY_AND_ASSIGN(NaclSessionInstance);
};

NaclSessionInstance::NaclSessionInstance(PP_Instance instance)
      : pp::Instance(instance), session_(new Session()),
        table_(new mozc::composer::Table()) {
  table_->InitializeWithRequestAndConfig(commands::RequestHandler::GetRequest(),
                                         config::ConfigHandler::GetConfig());
  session_->SetTable(table_.get());
}

void NaclSessionInstance::HandleMessage(const pp::Var& var_message) {
  if (!var_message.is_string()) {
    return;
  }

  Json::Value parsed;
  if (!Json::Reader().parse(var_message.AsString(), parsed)) {
    LOG(ERROR) << "Error occured during JSON parsing";
    return;
  }
  const string &method = parsed["method"].asString();
  if (method == "SendKey") {
    SendKey(parsed);
  } else if (method == "SendCommand") {
    SendCommand(parsed);
  }
}

void NaclSessionInstance::SendKey(const Json::Value &request) {
  if (!request.isMember("id") || !request.isMember("key")) {
    DLOG(ERROR) << "request error";
    return;
  }
  const int32 msg_id = request["id"].asInt();
  const string &key_string = request["key"].asString();

  commands::Command command;
  command.mutable_input()->set_type(commands::Input::SEND_KEY);
  if (!KeyParser::ParseKey(key_string,
                           command.mutable_input()->mutable_key())) {
    DLOG(ERROR) << "ParseKey error";
  }
  session_->SendKey(&command);
  Json::Value output(Json::objectValue);

  Json::Value response(Json::objectValue);
  response["id"] = Json::Value(msg_id);
  response["command"] = ProtobufMessageToJsonValue(command);
  PostMessage(Json::FastWriter().write(response));
}

void NaclSessionInstance::SendCommand(const Json::Value &request) {
  if (!request.isMember("id") || !request.isMember("type")) {
    DLOG(ERROR) << "request error";
    return;
  }
  const int32 msg_id = request["id"].asInt();
  const string &type_string = request["type"].asString();

  commands::Command command;
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  SessionCommand *session_command =
      command.mutable_input()->mutable_command();
  // TODO(horo): create JsonToProtobufMessage() function.
  if (kCommandTypeMap.count(type_string) != 0) {
    session_command->set_type(kCommandTypeMap.find(type_string)->second);
  }
  if (request.isMember("candidate_id")) {
    session_command->set_id(request["candidate_id"].asInt());
  }
  if (request.isMember("composition_mode")) {
    const string &composition_mode_string =
        request["composition_mode"].asString();
    if (kCompositionModeMap.count(composition_mode_string) != 0) {
      session_command->set_composition_mode(
          kCompositionModeMap.find(composition_mode_string)->second);
    }
  }
  if (request.isMember("text")) {
    session_command->set_text(request["text"].asString());
  }
  if (request.isMember("cursor_position")) {
    session_command->set_cursor_position(request["cursor_position"].asInt());
  }
  if (request.isMember("input_field_type")) {
    const string &input_field_type_string =
        request["input_field_type"].asString();
    if (kInputFieldTypeMap.count(input_field_type_string) != 0) {
      session_command->set_input_field_type(
          kInputFieldTypeMap.find(input_field_type_string)->second);
    }
  }

  session_->SendCommand(&command);
  Json::Value response(Json::objectValue);
  response["id"] = Json::Value(msg_id);
  response["command"] = ProtobufMessageToJsonValue(command);
  PostMessage(Json::FastWriter().write(response));
}


class NaclSessionModule : public pp::Module {
 public:
  NaclSessionModule() : pp::Module() {
  }
  virtual ~NaclSessionModule() {}

 protected:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    return new NaclSessionInstance(instance);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NaclSessionModule);
};

}  // namespace session
}  // namespace mozc

namespace pp {

Module* CreateModule() {
  // We use dummy argc and argv to call InitGoogle().
  int argc = 1;
  char argv0[] = "NaclModule";
  char *argv_body[] = {argv0, NULL};
  char **argv = argv_body;
  InitGoogle(argv[0], &argc, &argv, true);

  return new mozc::session::NaclSessionModule();
}

}  // namespace pp
