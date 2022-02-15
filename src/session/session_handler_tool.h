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

#ifndef MOZC_SESSION_SESSION_HANDLER_TOOL_H_
#define MOZC_SESSION_SESSION_HANDLER_TOOL_H_

#include <cstdint>
#include <memory>

#include "engine/engine_interface.h"
#include "engine/user_data_manager_interface.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/session_handler_interface.h"
#include "absl/status/status.h"

namespace mozc {
namespace session {

// Session utility for stress tests.
class SessionHandlerTool {
 public:
  explicit SessionHandlerTool(std::unique_ptr<EngineInterface> engine);
  ~SessionHandlerTool();

  bool CreateSession();
  bool DeleteSession();
  bool CleanUp();
  bool ClearUserPrediction();
  bool SendKey(const commands::KeyEvent &key, commands::Output *output) {
    return SendKeyWithOption(key, commands::Input::default_instance(), output);
  }
  bool SendKeyWithOption(const commands::KeyEvent &key,
                         const commands::Input &option,
                         commands::Output *output);
  bool TestSendKey(const commands::KeyEvent &key, commands::Output *output) {
    return TestSendKeyWithOption(key, commands::Input::default_instance(),
                                 output);
  }
  bool TestSendKeyWithOption(const commands::KeyEvent &key,
                             const commands::Input &option,
                             commands::Output *output);
  bool SelectCandidate(uint32_t id, commands::Output *output);
  bool SubmitCandidate(uint32_t id, commands::Output *output);
  bool ExpandSuggestion(commands::Output *output);

  bool Reload();
  bool ResetContext();
  bool UndoOrRewind(commands::Output *output);
  bool SwitchInputMode(commands::CompositionMode composition_mode);
  bool SetRequest(const commands::Request &request, commands::Output *output);
  bool SetConfig(const config::Config &config, commands::Output *output);
  bool SyncData();
  void SetCallbackText(const std::string &text);

 private:
  bool EvalCommand(commands::Input *input, commands::Output *output);
  bool EvalCommandInternal(commands::Input *input, commands::Output *output,
                           bool allow_callback);

  uint64_t id_;  // Session ID
  std::unique_ptr<SessionObserverInterface> usage_observer_;
  UserDataManagerInterface *data_manager_;
  std::unique_ptr<SessionHandlerInterface> handler_;
  std::string callback_text_;

  DISALLOW_COPY_AND_ASSIGN(SessionHandlerTool);
};

class SessionHandlerInterpreter {
 public:
  SessionHandlerInterpreter();
  explicit SessionHandlerInterpreter(std::unique_ptr<EngineInterface> engine);
  virtual ~SessionHandlerInterpreter();

  void ClearState();
  void ClearAll();
  void ResetContext();
  void SyncDataToStorage();
  void ClearUserPrediction();
  void ClearUsageStats();
  const commands::Output& LastOutput() const;
  const commands::CandidateWord& GetCandidateByValue(
      const absl::string_view value);
  bool GetCandidateIdByValue(const absl::string_view value, uint32_t *id);
  std::vector<std::string> Parse(const std::string &line);
  absl::Status Eval(const std::vector<std::string> &args);

 private:
  // std::unique_ptr<EngineInterface> engine_;
  std::unique_ptr<SessionHandlerTool> client_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<commands::Output> last_output_;
  std::unique_ptr<commands::Request> request_;
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_HANDLER_TOOL_H_
