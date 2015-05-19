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
#include "engine/engine_factory.h"
#include "engine/engine_interface.h"
#include "net/jsoncpp.h"
#include "net/json_util.h"
#include "session/commands.pb.h"
#include "session/japanese_session_factory.h"
#include "session/key_parser.h"
#include "session/request_handler.h"
#include "session/session.h"
#include "session/session_handler.h"

using mozc::commands::CompositionMode;
using mozc::commands::SessionCommand;
using mozc::net::JsonUtil;

namespace mozc {
namespace session {

class NaclSessionHandlerInstance : public pp::Instance {
 public:
  explicit NaclSessionHandlerInstance(PP_Instance instance);
  virtual ~NaclSessionHandlerInstance() {}
  virtual void HandleMessage(const pp::Var &var_message);

 private:
  scoped_ptr<mozc::EngineInterface> engine_;
  scoped_ptr<SessionHandlerInterface> handler_;
  scoped_ptr<JapaneseSessionFactory> session_factory_;
  DISALLOW_COPY_AND_ASSIGN(NaclSessionHandlerInstance);
};

NaclSessionHandlerInstance::NaclSessionHandlerInstance(PP_Instance instance)
    : pp::Instance(instance) {
  engine_.reset(mozc::EngineFactory::Create());
  session_factory_.reset(new JapaneseSessionFactory(engine_.get()));
  SessionFactoryManager::SetSessionFactory(session_factory_.get());
  handler_.reset(new SessionHandler());
}

void NaclSessionHandlerInstance::HandleMessage(const pp::Var &var_message) {
  if (!var_message.is_string()) {
    return;
  }

  Json::Value request;
  if (!Json::Reader().parse(var_message.AsString(), request)) {
    LOG(ERROR) << "Error occured during JSON parsing";
    return;
  }

  if (!request.isMember("id") || !request.isMember("cmd")) {
    DLOG(ERROR) << "request error";
    return;
  }
  const int32 msg_id = request["id"].asInt();
  commands::Command command;
  JsonUtil::JsonValueToProtobufMessage(request["cmd"], &command);
  handler_->EvalCommand(&command);
  Json::Value response(Json::objectValue);
  response["id"] = Json::Value(msg_id);
  JsonUtil::ProtobufMessageToJsonValue(command, &response["cmd"]);
  PostMessage(Json::FastWriter().write(response));
}

class NaclSessionHandlerModule : public pp::Module {
 public:
  NaclSessionHandlerModule() : pp::Module() {
  }
  virtual ~NaclSessionHandlerModule() {}

 protected:
  virtual pp::Instance *CreateInstance(PP_Instance instance) {
    return new NaclSessionHandlerInstance(instance);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NaclSessionHandlerModule);
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

  return new mozc::session::NaclSessionHandlerModule();
}

}  // namespace pp
