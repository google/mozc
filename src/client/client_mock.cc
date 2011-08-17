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

#include "client/client_mock.h"

namespace mozc {
namespace client {

// Most of methods in ClientMock looks almost same.  Here defines the
// boilerplates for those methods.
#define MockConstBoolImplementation(method_name, argument) \
  bool ClientMock::method_name(argument) const {           \
    function_counter_[#method_name]++;                     \
    map<string, bool>::const_iterator it =                 \
        return_bool_values_.find(#method_name);            \
    if (it != return_bool_values_.end()) {                 \
      return it->second;                                   \
    }                                                      \
    return false;                                          \
  }
#define MockBoolImplementation(method_name, argument)      \
  bool ClientMock::method_name(argument) {                 \
    function_counter_[#method_name]++;                     \
    map<string, bool>::const_iterator it =                 \
        return_bool_values_.find(#method_name);            \
    if (it != return_bool_values_.end()) {                 \
      return it->second;                                   \
    }                                                      \
    return false;                                          \
  }
#define MockVoidImplementation(method_name, argument)      \
  void ClientMock::method_name(argument) {                 \
    function_counter_[#method_name]++;                     \
    return;                                                \
  }

MockVoidImplementation(SetIPCClientFactory,
                       IPCClientFactoryInterface *client_factory);
MockVoidImplementation(SetServerLauncher,
                       ServerLauncherInterface *server_launcher);
MockConstBoolImplementation(IsValidRunLevel, void);
MockBoolImplementation(EnsureConnection, void);
MockBoolImplementation(EnsureSession, void);
MockBoolImplementation(CheckVersionOrRestartServer, void);
MockBoolImplementation(ClearUserHistory, void);
MockBoolImplementation(ClearUserPrediction, void);
MockBoolImplementation(ClearUnusedUserPrediction, void);
MockBoolImplementation(Shutdown, void);
MockBoolImplementation(SyncData, void);
MockBoolImplementation(Reload, void);
MockBoolImplementation(Cleanup, void);
MockVoidImplementation(Reset, void);
MockConstBoolImplementation(PingServer, void);
MockBoolImplementation(NoOperation, void);
MockVoidImplementation(EnableCascadingWindow, bool enable);
MockVoidImplementation(set_timeout, int timeout);
MockVoidImplementation(set_restricted, bool restricted);
MockVoidImplementation(set_server_program, const string &program_path);
MockVoidImplementation(set_client_capability,
                       const commands::Capability &capability);
MockBoolImplementation(LaunchToolWithProtoBuf, const commands::Output &output);
MockBoolImplementation(OpenBrowser, const string &url);

#undef MockConstImplementation
#undef MockBoolImplementation
#undef MockVoidImplementation

// Another boilerplate for the method with an "output" as its second
// argument.
#define MockImplementationWithOutput(method_name, argtype)                   \
  bool ClientMock::method_name(argtype argument, commands::Output *output) { \
    function_counter_[#method_name]++;                                       \
    called_##method_name##_.CopyFrom(argument);                              \
    map<string, commands::Output>::const_iterator it =                       \
        outputs_.find(#method_name);                                         \
    if (it != outputs_.end()) {                                              \
      output->CopyFrom(it->second);                                          \
    }                                                                        \
    map<string, bool>::const_iterator retval =                               \
        return_bool_values_.find(#method_name);                              \
    if (retval != return_bool_values_.end()) {                               \
      return retval->second;                                                 \
    }                                                                        \
    return false;                                                            \
  }

MockImplementationWithOutput(SendKey, const commands::KeyEvent &);
MockImplementationWithOutput(TestSendKey, const commands::KeyEvent &);
MockImplementationWithOutput(SendCommand, const commands::SessionCommand &);

#undef MockImplementationWithOutput

// Exceptional methods.
// GetConfig needs to obtain the "called_config_".
bool ClientMock::GetConfig(config::Config *config) {
  function_counter_["GetConfig"]++;
  config->CopyFrom(called_config_);
  map<string, bool>::const_iterator it = return_bool_values_.find("GetConfig");
  if (it != return_bool_values_.end()) {
    return it->second;
  }
  return false;
}

// SetConfig needs to set the "called_config_".
bool ClientMock::SetConfig(const config::Config &config) {
  function_counter_["SetConfig"]++;
  called_config_.CopyFrom(config);
  map<string, bool>::const_iterator it = return_bool_values_.find("SetConfig");
  if (it != return_bool_values_.end()) {
    return it->second;
  }
  return false;
}

// LaunchTool arguments are quite different from other methods.
bool ClientMock::LaunchTool(const string &mode, const string &extra_arg) {
  function_counter_["LaunchTool"]++;
  return return_bool_values_["LaunchTool"];
}

// Other methods to deal with internal data such like operations over
// function counters or setting the expected return values.
void ClientMock::ClearFunctionCounter() {
  for (map<string, int>::iterator it = function_counter_.begin();
       it != function_counter_.end(); it++) {
    it->second = 0;
  }
}

void ClientMock::SetBoolFunctionReturn(string func_name, bool value) {
  return_bool_values_[func_name] = value;
}

int ClientMock::GetFunctionCallCount(string key) {
  return function_counter_[key];
}

}  // namespace mozc
}  // namespace ibus
