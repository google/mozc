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
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "engine/engine_interface.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/session_handler.h"
#include "session/session_observer_interface.h"

namespace mozc {
namespace session {

// Session utility for stress tests.
class SessionHandlerTool {
 public:
  explicit SessionHandlerTool(std::unique_ptr<EngineInterface> engine);
  SessionHandlerTool(const SessionHandlerTool &) = delete;
  SessionHandlerTool &operator=(const SessionHandlerTool &) = delete;

  bool CreateSession();
  bool DeleteSession();
  bool CleanUp();
  bool ClearUserPrediction();
  bool ClearUserHistory();
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
  bool UpdateComposition(absl::Span<const std::string> args,
                         commands::Output *output);
  bool SelectCandidate(uint32_t id, commands::Output *output);
  bool SubmitCandidate(uint32_t id, commands::Output *output);

  bool Reload();
  bool ResetContext();
  bool UndoOrRewind(commands::Output *output);
  // Try to delete the candidate from the history.
  // The target candidate is specified with the |id|. If |id| is not specified,
  // the current focused candidate will be specified.
  bool DeleteCandidateFromHistory(std::optional<int> id,
                                  commands::Output *output);
  bool SwitchInputMode(commands::CompositionMode composition_mode);
  bool SetRequest(const commands::Request &request, commands::Output *output);
  bool SetConfig(const config::Config &config, commands::Output *output);
  bool SyncData();
  void SetCallbackText(absl::string_view text);
  bool ReloadSupplementalModel(absl::string_view model_path);

 private:
  bool EvalCommand(commands::Input *input, commands::Output *output);
  bool EvalCommandInternal(commands::Input *input, commands::Output *output,
                           bool allow_callback);

  uint64_t id_;  // Session ID
  EngineInterface *engine_ = nullptr;
  std::unique_ptr<SessionObserverInterface> usage_observer_;
  std::unique_ptr<SessionHandler> handler_;
  std::string callback_text_;
};

class SessionHandlerInterpreter final {
 public:
  SessionHandlerInterpreter();
  explicit SessionHandlerInterpreter(std::unique_ptr<EngineInterface> engine);
  ~SessionHandlerInterpreter();

  void ClearState();
  void ClearAll();
  void ResetContext();
  void SyncDataToStorage();
  void ClearUserPrediction();
  void ClearUsageStats();
  const commands::Output &LastOutput() const;
  const commands::CandidateWord &GetCandidateByValue(
      absl::string_view value) const;
  bool GetCandidateIdByValue(absl::string_view value, uint32_t *id) const;
  std::vector<uint32_t> GetCandidateIdsByValue(absl::string_view value) const;
  std::vector<uint32_t> GetRemovedCandidateIdsByValue(
      absl::string_view value) const;
  std::vector<std::string> Parse(absl::string_view line);
  absl::Status Eval(absl::Span<const std::string> args);
  void SetRequest(const commands::Request &request);
  void ReloadSupplementalModel(absl::string_view model_path);

 private:
  std::unique_ptr<SessionHandlerTool> client_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<commands::Output> last_output_;
  std::unique_ptr<commands::Request> request_;
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_HANDLER_TOOL_H_
