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

#include "unix/fcitx5/mozc_state.h"

#include <fcitx-utils/i18n.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/stringutils.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputpanel.h>
#include <string>

#include "base/const.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/system_util.h"
#include "base/util.h"
#include "session/ime_switch_util.h"
#include "unix/fcitx5/fcitx_key_event_handler.h"
#include "unix/fcitx5/mozc_connection.h"
#include "unix/fcitx5/mozc_engine.h"
#include "unix/fcitx5/mozc_response_parser.h"
#include "unix/fcitx5/surrounding_text_util.h"

namespace {

const struct CompositionMode {
  const char* icon;
  const char* label;
  const char* description;
  mozc::commands::CompositionMode mode;
} kPropCompositionModes[] = {
    {
        "mozc-direct.png",
        "A",
        N_("Direct"),
        mozc::commands::DIRECT,
    },
    {
        "mozc-hiragana.png",
        "\xe3\x81\x82",  // Hiragana letter A in UTF-8.
        N_("Hiragana"),
        mozc::commands::HIRAGANA,
    },
    {
        "mozc-katakana_full.png",
        "\xe3\x82\xa2",  // Katakana letter A.
        N_("Full Katakana"),
        mozc::commands::FULL_KATAKANA,
    },
    {
        "mozc-alpha_half.png",
        "A",
        N_("Half ASCII"),
        mozc::commands::HALF_ASCII,
    },
    {
        "mozc-alpha_full.png",
        "\xef\xbc\xa1",  // Full width ASCII letter A.
        N_("Full ASCII"),
        mozc::commands::FULL_ASCII,
    },
    {
        "mozc-katakana_half.png",
        "\xef\xbd\xb1",  // Half width Katakana letter A.
        N_("Half Katakana"),
        mozc::commands::HALF_KATAKANA,
    },
};
const size_t kNumCompositionModes = arraysize(kPropCompositionModes);

// This array must correspond with the CompositionMode enum in the
// mozc/session/command.proto file.
static_assert(mozc::commands::NUM_OF_COMPOSITIONS ==
                  arraysize(kPropCompositionModes),
              "number of modes must match");

}  // namespace

namespace fcitx {

// For unittests.
MozcState::MozcState(InputContext* ic, mozc::client::ClientInterface* client,
                     MozcEngine* engine)
    : ic_(ic),
      client_(client),
      engine_(engine),
      handler_(std::make_unique<KeyEventHandler>()),
      parser_(std::make_unique<MozcResponseParser>(engine_)) {
  // mozc::Logging::SetVerboseLevel(1);
  VLOG(1) << "MozcState created.";
  const bool is_vertical = true;
  parser_->SetUseAnnotation(is_vertical);

  if (client_->EnsureConnection()) {
    UpdatePreeditMethod();
  }
}

MozcState::~MozcState() {
  client_->SyncData();
  VLOG(1) << "MozcState destroyed.";
}

void MozcState::UpdatePreeditMethod() {
  mozc::config::Config config;
  if (!client_->GetConfig(&config)) {
    LOG(ERROR) << "GetConfig failed";
    return;
  }
  preedit_method_ = config.has_preedit_method() ? config.preedit_method()
                                                : mozc::config::Config::ROMAN;
}

bool MozcState::TrySendKeyEvent(
    InputContext* ic, KeySym sym, uint32 keycode, KeyStates state,
    mozc::commands::CompositionMode composition_mode, bool layout_is_jp,
    bool is_key_up, mozc::commands::Output* out, string* out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  // Call EnsureConnection just in case MozcState::MozcConnection() fails
  // to establish the server connection.
  if (!client_->EnsureConnection()) {
    *out_error = "EnsureConnection failed";
    VLOG(1) << "EnsureConnection failed";
    return false;
  }

  mozc::commands::KeyEvent event;
  if (!handler_->GetKeyEvent(sym, keycode, state, preedit_method_, layout_is_jp,
                             is_key_up, &event))
    return false;

  if ((composition_mode == mozc::commands::DIRECT) &&
      !mozc::config::ImeSwitchUtil::IsDirectModeCommand(event)) {
    VLOG(1) << "In DIRECT mode. Not consumed.";
    return false;  // not consumed.
  }

  mozc::commands::Context context;
  SurroundingTextInfo surrounding_text_info;
  if (GetSurroundingText(ic, &surrounding_text_info,
                         engine_->clipboardAddon())) {
    context.set_preceding_text(surrounding_text_info.preceding_text);
    context.set_following_text(surrounding_text_info.following_text);
  }

  VLOG(1) << "TrySendKeyEvent: " << std::endl << event.DebugString();
  if (!client_->SendKeyWithContext(event, context, out)) {
    *out_error = "SendKey failed";
    VLOG(1) << "ERROR";
    return false;
  }
  VLOG(1) << "OK: " << std::endl << out->DebugString();
  return true;
}

bool MozcState::TrySendClick(int32 unique_id, mozc::commands::Output* out,
                             string* out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SELECT_CANDIDATE);
  command.set_id(unique_id);
  return TrySendRawCommand(command, out, out_error);
}

bool MozcState::TrySendCompositionMode(mozc::commands::CompositionMode mode,
                                       mozc::commands::Output* out,
                                       string* out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SWITCH_INPUT_MODE);
  command.set_composition_mode(mode);
  return TrySendRawCommand(command, out, out_error);
}

bool MozcState::TrySendCommand(mozc::commands::SessionCommand::CommandType type,
                               mozc::commands::Output* out,
                               string* out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  command.set_type(type);
  return TrySendRawCommand(command, out, out_error);
}

bool MozcState::TrySendRawCommand(const mozc::commands::SessionCommand& command,
                                  mozc::commands::Output* out,
                                  string* out_error) const {
  VLOG(1) << "TrySendRawCommand: " << std::endl << command.DebugString();
  if (!client_->SendCommand(command, out)) {
    *out_error = "SendCommand failed";
    VLOG(1) << "ERROR";
    return false;
  }
  VLOG(1) << "OK: " << std::endl << out->DebugString();
  return true;
}

// This function is called when users press or release a key.
bool MozcState::ProcessKeyEvent(KeySym sym, uint32 keycode, KeyStates state,
                                bool layout_is_jp, bool is_key_up) {
  auto normalized_key = Key(sym, state).normalize();
  if (displayUsage_) {
    if (is_key_up) {
      return true;
    }

    if (normalized_key.check(Key(FcitxKey_Escape))) {
      displayUsage_ = false;
      ProcessKeyEvent(FcitxKey_VoidSymbol, 0, KeyState::NoState, layout_is_jp,
                      false);
    }
    return true;
  }

  if (normalized_key.check(Key(FcitxKey_H, KeyState::Ctrl_Alt))) {
    if (!title_.empty() || !description_.empty()) {
      DisplayUsage();
      return true;
    }
  }

  string error;
  mozc::commands::Output raw_response;
  if (!TrySendKeyEvent(ic_, sym, keycode, state, composition_mode_,
                       layout_is_jp, is_key_up, &raw_response, &error)) {
    // TODO(yusukes): Show |error|.
    return false;  // not consumed.
  }

  return ParseResponse(raw_response);
}

// This function is called from SCIM framework when users click the candidate
// window.
void MozcState::SelectCandidate(int32 id) {
  if (id == kBadCandidateId) {
    LOG(ERROR) << "The clicked candidate doesn't have unique ID.";
    return;
  }
  VLOG(1) << "select_candidate, id=" << id;

  string error;
  mozc::commands::Output raw_response;
  if (!TrySendClick(id, &raw_response, &error)) {
    LOG(ERROR) << "IPC failed. error=" << error;
    SetAuxString(error);
    DrawAll();
  } else {
    ParseResponse(raw_response);
  }
}

// This function is called from SCIM framework.
void MozcState::Reset() {
  VLOG(1) << "resetim";
  string error;
  mozc::commands::Output raw_response;
  if (TrySendCommand(mozc::commands::SessionCommand::REVERT, &raw_response,
                     &error)) {
    parser_->ParseResponse(raw_response, ic_);
  }
  ClearAll();  // just in case.
  DrawAll();
}

bool MozcState::Paging(bool prev) {
  VLOG(1) << "paging";
  string error;
  mozc::commands::SessionCommand::CommandType command =
      prev ? mozc::commands::SessionCommand::CONVERT_PREV_PAGE
           : mozc::commands::SessionCommand::CONVERT_NEXT_PAGE;
  mozc::commands::Output raw_response;
  if (TrySendCommand(command, &raw_response, &error)) {
    parser_->ParseResponse(raw_response, ic_);
    return true;
  }
  return false;
}

// This function is called when the ic gets focus.
void MozcState::FocusIn() {
  VLOG(1) << "MozcState::FocusIn()";

  UpdatePreeditMethod();
  DrawAll();
}

// This function is called when the ic loses focus.
void MozcState::FocusOut() {
  VLOG(1) << "MozcState::FocusOut()";
  string error;
  mozc::commands::Output raw_response;
  if (TrySendCommand(mozc::commands::SessionCommand::REVERT, &raw_response,
                     &error)) {
    parser_->ParseResponse(raw_response, ic_);
  }
  ClearAll();  // just in case.
  DrawAll();
  // TODO(yusukes): Call client::SyncData() like ibus-mozc.
}

bool MozcState::ParseResponse(const mozc::commands::Output& raw_response) {
  ClearAll();
  const bool consumed = parser_->ParseResponse(raw_response, ic_);
  if (!consumed) {
    VLOG(1) << "The input was not consumed by Mozc.";
  }
  OpenUrl();
  DrawAll();
  return consumed;
}

void MozcState::SetResultString(const string& result_string) {
  ic_->commitString(result_string);
}

void MozcState::SetPreeditInfo(Text preedit_info) {
  preedit_ = std::move(preedit_info);
}

void MozcState::SetAuxString(const string& str) { aux_ = str; }

void MozcState::SetCompositionMode(mozc::commands::CompositionMode mode) {
  composition_mode_ = mode;
  DCHECK(composition_mode_ < kNumCompositionModes);
  engine_->compositionModeUpdated(ic_);
}

void MozcState::SendCompositionMode(mozc::commands::CompositionMode mode) {
  // Send the SWITCH_INPUT_MODE command.
  string error;
  mozc::commands::Output raw_response;
  if (TrySendCompositionMode(kPropCompositionModes[mode].mode, &raw_response,
                             &error)) {
    auto oldMode = composition_mode_;
    parser_->ParseResponse(raw_response, ic_);
    if (oldMode != composition_mode_) {
      engine_->instance()->showInputMethodInformation(ic_);
    }
  }
}

void MozcState::SetUrl(const string& url) { url_ = url; }

void MozcState::ClearAll() {
  SetPreeditInfo(Text());
  SetAuxString("");
  ic_->inputPanel().reset();
  url_.clear();
}

void MozcState::DrawAll() {
  Text preedit = preedit_;
  if (!aux_.empty()) {
    string aux;
    if (preedit.size()) {
      aux += " ";
    }
    aux += "[";
    aux += aux_;
    aux += "]";
    preedit.append(aux);
  }
  ic_->inputPanel().setPreedit(std::move(preedit));
  ic_->inputPanel().setClientPreedit(preedit_);
  ic_->updatePreedit();
  ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void MozcState::OpenUrl() {
  if (url_.empty()) {
    return;
  }
  mozc::Process::OpenBrowser(url_);
  url_.clear();
}

bool MozcState::SendCommand(
    const mozc::commands::SessionCommand& session_command,
    mozc::commands::Output* new_output) {
  string error;
  return TrySendRawCommand(session_command, new_output, &error);
}

void MozcState::SetUsage(const string& title, const string& description) {
  title_ = title;
  description_ = description;
}

void MozcState::DisplayUsage() {
  displayUsage_ = true;

  ic_->inputPanel().reset();
  auto candidateList = std::make_unique<DisplayOnlyCandidateList>();

  auto lines = stringutils::split(description_, "\n");
  candidateList->setLayoutHint(CandidateLayoutHint::Vertical);
  candidateList->setContent(lines);
  ic_->inputPanel().setCandidateList(std::move(candidateList));
  auto str = title_ + " [" + _("Press Escape to go back") + "]";
  ic_->inputPanel().setAuxUp(Text(str));
  ic_->updatePreedit();
  ic_->updateUserInterface(UserInterfaceComponent::InputPanel);
}

}  // namespace fcitx
