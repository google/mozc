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

#ifndef MOZC_UNIX_IBUS_MOZC_ENGINE_H_
#define MOZC_UNIX_IBUS_MOZC_ENGINE_H_

#include <cstdint>
#include <memory>
#include <set>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "base/port.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "unix/ibus/candidate_window_handler.h"
#include "unix/ibus/engine_interface.h"
#include "unix/ibus/ibus_candidate_window_handler.h"
#include "unix/ibus/ibus_config.h"
#include "unix/ibus/ibus_wrapper.h"
#include "unix/ibus/preedit_handler.h"
#include "unix/ibus/property_handler.h"

namespace mozc {

namespace client {
class ClientInterface;
}

namespace ibus {

class CandidateWindowHandlerInterface;
class KeyEventHandler;
class LaunchToolTest;

// Implements EngineInterface and handles signals from IBus daemon.
// This class mainly does the two things:
// - Converting IBus key events to Mozc's key events and passes them to the
//   Mozc's session module and
// - Reflecting the output from the Mozc's session module in IBus UIs such as
//   the preedit, the candidate window and the result text.
class MozcEngine : public EngineInterface {
 public:
  MozcEngine();
  MozcEngine(const MozcEngine &) = delete;
  MozcEngine &operator=(const MozcEngine &) = delete;
  virtual ~MozcEngine();

  // EngineInterface functions
  void CandidateClicked(IbusEngineWrapper *engine, uint index, uint button,
                        uint state) override;
  void CursorDown(IbusEngineWrapper *engine) override;
  void CursorUp(IbusEngineWrapper *engine) override;
  void Disable(IbusEngineWrapper *engine) override;
  void Enable(IbusEngineWrapper *engine) override;
  void FocusIn(IbusEngineWrapper *engine) override;
  void FocusOut(IbusEngineWrapper *engine) override;
  void PageDown(IbusEngineWrapper *engine) override;
  void PageUp(IbusEngineWrapper *engine) override;
  bool ProcessKeyEvent(IbusEngineWrapper *engine, uint keyval, uint keycode,
                       uint state) override;
  void PropertyActivate(IbusEngineWrapper *engine, const char *property_name,
                        uint property_state) override;
  void PropertyHide(IbusEngineWrapper *engine,
                    const char *property_name) override;
  void PropertyShow(IbusEngineWrapper *engine,
                    const char *property_name) override;
  void Reset(IbusEngineWrapper *engine) override;
  void SetCapabilities(IbusEngineWrapper *engine, uint capabilities) override;
  void SetCursorLocation(IbusEngineWrapper *engine, int x, int y, int w,
                         int h) override;
  void SetContentType(IbusEngineWrapper *engine, uint purpose,
                      uint hints) override;

 private:
  // Updates the preedit text and the candidate window and inserts result
  // based on the content of |output|.
  bool UpdateAll(IbusEngineWrapper *engine, const commands::Output &output);
  // Inserts a result text based on the content of |output|.
  bool UpdateResult(IbusEngineWrapper *engine,
                    const commands::Output &output) const;
  // Updates |unique_candidate_ids_|.
  bool UpdateCandidateIDMapping(const commands::Output &output);
  // Updates the deletion range message based on the content of |output|.
  bool UpdateDeletionRange(IbusEngineWrapper *engine,
                           const commands::Output &output);

  // Updates the callback message based on the content of |output|.
  bool ExecuteCallback(IbusEngineWrapper *engine,
                       const commands::Output &output);

  // Launches Mozc tool with appropriate arguments.
  bool LaunchTool(const commands::Output &output) const;

  // Updates internal preedit_method (Roman/Kana) state
  void UpdatePreeditMethod();

  // Calls SyncData command. if |force| is false, SyncData is called
  // at an appropriate timing to reduce IPC calls. if |force| is true,
  // always calls SyncData.
  void SyncData(bool force);

  // Reverts internal state of mozc_server by sending SessionCommand::REVERT IPC
  // message, then hides a preedit string and the candidate window.
  void RevertSession(IbusEngineWrapper *engine);

  CandidateWindowHandlerInterface *GetCandidateWindowHandler(
      IbusEngineWrapper *engine);

  absl::Time last_sync_time_;
  std::unique_ptr<KeyEventHandler> key_event_handler_;
  std::unique_ptr<client::ClientInterface> client_;

  std::unique_ptr<PropertyHandler> property_handler_;
  std::unique_ptr<PreeditHandler> preedit_handler_;

  // If true, uses Mozc candidate window instead of IBus default one.
  bool use_mozc_candidate_window_;
  // TODO(nona): Introduce CandidateWindowHandlerManager to avoid direct access.
  CandidateWindowHandler mozc_candidate_window_handler_;
  IBusCandidateWindowHandler ibus_candidate_window_handler_;
  config::Config::PreeditMethod preedit_method_;

  // Unique IDs of candidates that are currently shown.
  std::vector<int32_t> unique_candidate_ids_;
  IbusConfig ibus_config_;

  friend class MozcEngineTestPeer;
};

bool CanUseMozcCandidateWindow(
    const IbusConfig &ibus_config,
    const absl::flat_hash_map<std::string, std::string> &env);

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_MOZC_ENGINE_H_
