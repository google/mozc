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

#include "client/session_mock.h"

namespace mozc {
namespace client {


bool SessionMock::IsValidRunLevel() const {
  function_counter_["IsValidRunLevel"]++;
  return return_bool_values_["IsValidRunLevel"];
}

bool SessionMock::EnsureConnection() {
  function_counter_["EnsureConnection"]++;
  return return_bool_values_["EnsureConnection"];
}

bool SessionMock::EnsureSession() {
  function_counter_["EnsureSession"]++;
  return return_bool_values_["EnsureSession"];
}

bool SessionMock::CheckVersionOrRestartServer() {
  function_counter_["CheckVersionOrRestartServer"]++;
  return return_bool_values_["CheckVersionOrRestartServer"];
}

bool SessionMock::SendKey(const commands::KeyEvent &key,
                          commands::Output *output) {
  function_counter_["SendKey"]++;
  return return_bool_values_["SendKey"];
}

bool SessionMock::TestSendKey(const commands::KeyEvent &key,
                          commands::Output *output) {
  function_counter_["TestSendKey"]++;
  return return_bool_values_["TestSendKey"];
}

bool SessionMock::SendCommand(const commands::SessionCommand &command,
                          commands::Output *output) {
  function_counter_["SendCommand"]++;
  return return_bool_values_["SendCommand"];
}

bool SessionMock::GetConfig(config::Config *config) {
  function_counter_["GetConfig"]++;
  return return_bool_values_["GetConfig"];
}

bool SessionMock::SetConfig(const config::Config &config) {
  function_counter_["SetConfig"]++;
  return return_bool_values_["SetConfig"];
}

bool SessionMock::ClearUserHistory() {
  function_counter_["ClearUserHistory"]++;
  return return_bool_values_["ClearUserHistory"];
}

bool SessionMock::ClearUserPrediction() {
  function_counter_["ClearUserPrediction"]++;
  return return_bool_values_["ClearUserPrediction"];
}

bool SessionMock::ClearUnusedUserPrediction() {
  function_counter_["ClearUnusedUserPrediction"]++;
  return return_bool_values_["ClearUnusedUserPrediction"];
}

bool SessionMock::Shutdown() {
  function_counter_["Shutdown"]++;
  return return_bool_values_["Shutdown"];
}

bool SessionMock::SyncData() {
  function_counter_["SyncData"]++;
  return return_bool_values_["SyncData"];
}

bool SessionMock::Reload() {
  function_counter_["Reload"]++;
  return return_bool_values_["Reload"];
}

bool SessionMock::Cleanup() {
  function_counter_["Cleanup"]++;
  return return_bool_values_["Cleanup"];
}

void SessionMock::Reset() {
  function_counter_["Reset"]++;
  return;
}

bool SessionMock::PingServer() const {
  function_counter_["PingServer"]++;
  return return_bool_values_["PingServer"];
}

bool SessionMock::NoOperation() {
  function_counter_["NoOperation"]++;
  return return_bool_values_["NoOperation"];
}

void SessionMock::EnableCascadingWindow(bool enable) {
  function_counter_["EnableCascadingWindow"]++;
  return;
}

void SessionMock::set_timeout(int timeout) {
  function_counter_["set_timeout"]++;
  return;
}

void SessionMock::set_restricted(bool restricted) {
  function_counter_["set_restricted"]++;
  return;
}

void SessionMock::set_server_program(const string &program_path) {
  function_counter_["set_server_program"]++;
  return;
}

void SessionMock::set_client_capability(
    const commands::Capability &capability) {
  function_counter_["set_client_capability"]++;
  return;
}

bool SessionMock::LaunchTool(const string &mode, const string &extra_arg) {
  function_counter_["LaunchTool"]++;
  return return_bool_values_["LaunchTool"];
}

bool SessionMock::OpenBrowser(const string &url) {
  function_counter_["OpenBrowser"]++;
  return return_bool_values_["OpenBrowser"];
}

void SessionMock::ClearFunctionCounter() {
  for (map<string, int>::iterator it = function_counter_.begin();
       it != function_counter_.end(); it++) {
    it->second = 0;
  }
}

void SessionMock::SetBoolFunctionReturn(string func_name, bool value) {
  return_bool_values_[func_name] = value;
}

int SessionMock::GetFunctionCallCount(string key) {
  return function_counter_[key];
}

map<string, int> SessionMock::function_counter_;
map<string, bool> SessionMock::return_bool_values_;

}  // namespace mozc
}  // namespace ibus
