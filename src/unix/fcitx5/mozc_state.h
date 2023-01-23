// Copyright 2012~2013, Weng Xuetian <wengxt@gmail.com>
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

#ifndef MOZC_UNIX_FCITX_FCITX_MOZC_H_
#define MOZC_UNIX_FCITX_FCITX_MOZC_H_

#include <fcitx-utils/key.h>
#include <fcitx/event.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/text.h>

#include <memory>

#include "base/port.h"
#include "base/run_level.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"

namespace fcitx {
const int32_t kBadCandidateId = -12345;
class MozcConnectionInterface;
class MozcResponseParser;
class KeyTranslator;
class KeyEventHandler;
class MozcEngine;

class MozcState : public InputContextProperty {
 public:
  // This constructor is used by unittests.
  MozcState(InputContext *ic, mozc::client::ClientInterface *client,
            MozcEngine *engine);
  MozcState(const MozcState &) = delete;
  virtual ~MozcState();

  void UpdatePreeditMethod();

  bool ProcessKeyEvent(KeySym sym, uint32_t keycode, KeyStates state,
                       bool layout_is_jp, bool is_key_up);
  void SelectCandidate(int idx);
  void Reset();
  void FocusIn();
  void FocusOut(const InputContextEvent &event);
  bool Paging(bool prev);

  // Functions called by the MozcResponseParser class to update UI.

  // Displays a 'result' (aka 'commit string') on FCITX UI.
  void SetResultString(const std::string &result_string);
  // Displays a 'preedit' string on FCITX UI. This function takes ownership
  // of preedit_info. If the parameter is NULL, hides the string currently
  // displayed.
  void SetPreeditInfo(Text preedit_info);
  // Displays an auxiliary message (e.g., an error message, a title of
  // candidate window). If the string is empty (""), hides the message
  // currently being displayed.
  void SetAuxString(const std::string &str);
  // Sets a current composition mode (e.g., Hankaku Katakana).
  void SetCompositionMode(mozc::commands::CompositionMode mode,
                          bool updateUI = true);

  void SendCompositionMode(mozc::commands::CompositionMode mode);

  // Sets the url to be opened by the default browser.
  void SetUrl(const std::string &url);

  const std::string &GetIconFile(const std::string key);

  mozc::commands::CompositionMode GetCompositionMode() {
    return composition_mode_;
  }

  mozc::client::ClientInterface *GetClient() { return client_.get(); }

  bool SendCommand(const mozc::commands::SessionCommand &session_command,
                   mozc::commands::Output *new_output);

  void SetUsage(const std::string &title, const std::string &description);

  void DrawAll();

 private:
  void DisplayUsage();
  // Sends key event to the server. If the IPC succeeds, returns true and the
  // response is stored on 'out' (and 'out_error' is not modified). If the IPC
  // fails, returns false and the error message is stored on 'out_error'. In
  // this case, 'out' is not modified.
  bool TrySendKeyEvent(InputContext *ic, KeySym sym, uint32_t keycode,
                       KeyStates state,
                       mozc::commands::CompositionMode composition_mode,
                       bool layout_is_jp, bool is_key_up,
                       mozc::commands::Output *out,
                       std::string *out_error) const;

  // Sends 'mouse click on the candidate window' event to the server.
  bool TrySendClick(int32_t unique_id, mozc::commands::Output *out,
                    std::string *out_error) const;

  // Sends composition mode to the server.
  bool TrySendCompositionMode(mozc::commands::CompositionMode mode,
                              mozc::commands::Output *out,
                              std::string *out_error) const;

  // Sends a command to the server.
  bool TrySendCommand(mozc::commands::SessionCommand::CommandType type,
                      mozc::commands::Output *out,
                      std::string *out_error) const;

  bool TrySendRawCommand(const mozc::commands::SessionCommand &command,
                         mozc::commands::Output *out,
                         std::string *out_error) const;

  // Parses the response from mozc_server. Returns whether the server consumes
  // the input or not (true means 'consumed').
  bool ParseResponse(const mozc::commands::Output &request);

  void ClearAll();
  void DrawPreeditInfo();
  void DrawAux();

  // Open url_ with a default browser.
  void OpenUrl();

  InputContext *ic_;
  std::unique_ptr<mozc::client::ClientInterface> client_;
  MozcEngine *engine_;

  mozc::commands::CompositionMode composition_mode_ = mozc::commands::HIRAGANA;
  mozc::config::Config::PreeditMethod preedit_method_ =
      mozc::config::Config::ROMAN;
  const std::unique_ptr<KeyEventHandler> handler_;
  const std::unique_ptr<MozcResponseParser> parser_;

  bool displayUsage_ = false;
  Text preedit_;
  std::string aux_;  // error tooltip, or candidate window title.
  std::string url_;  // URL to be opened by a browser.
  std::string description_;
  std::string title_;
};

}  // namespace fcitx

#endif  // MOZC_UNIX_FCITX_FCITX_MOZC_H_
