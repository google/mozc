// Copyright 2010, Google Inc.
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

#include "unix/scim/scim_mozc.h"

#include <string>

#include "base/const.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/util.h"
#include "unix/scim/mozc_connection.h"
#include "unix/scim/mozc_lookup_table.h"
#include "unix/scim/mozc_response_parser.h"
#include "unix/scim/scim_key_translator.h"

namespace {

const char kConfigName[] = "/Panel/Gtk/LookupTableVertical";

const char kPropTool[] = "/Mozc/Tool";
const char kPropToolIcon[] = SCIM_ICONDIR "/scim-mozc-tool.png";
const char kPropToolDictionary[] = "/Mozc/Tool/dictionary";
const char kPropToolDictionaryIcon[] = SCIM_ICONDIR "/scim-mozc-dictionary.png";
const char kPropToolProperty[] = "/Mozc/Tool/property";
const char kPropToolPropertyIcon[] = SCIM_ICONDIR "/scim-mozc-property.png";

const char kPropCompositionModeIcon[] = "/Mozc/CompositionMode";

const struct CompositionMode {
  const char *icon;
  const char *label;
  const char *config_path;
  const char *description;
  mozc::commands::CompositionMode mode;
} kPropCompositionModes[] = {
  {
    "",  // TODO(yusukes): use icons.
    "A",
    "/Mozc/CompositionMode/direct",
    "Direct",
    mozc::commands::DIRECT,
  }, {
    "",
    "\xe3\x81\x82",  // Hiragana letter A in UTF-8.
    "/Mozc/CompositionMode/hiragana",
    "Hiragana",
    mozc::commands::HIRAGANA,
  }, {
    "",
    "\xe3\x82\xa2",  // Katakana letter A.
    "/Mozc/CompositionMode/full_katakana",
    "Full Katakana",
    mozc::commands::FULL_KATAKANA,
  }, {
    "",
    "_A",
    "/Mozc/CompositionMode/half_ascii",
    "Half ASCII",
    mozc::commands::HALF_ASCII,
  }, {
    "",
    "\xef\xbc\xa1",  // Full width ASCII letter A.
    "/Mozc/CompositionMode/full_ascii",
    "Full ASCII",
    mozc::commands::FULL_ASCII,
  }, {
    "",
    "_\xef\xbd\xb1",  // Half width Katakana letter A.
    "/Mozc/CompositionMode/half_katakana",
    "Half Katakana",
    mozc::commands::HALF_KATAKANA,
  },
};
const size_t kNumCompositionModes = arraysize(kPropCompositionModes);

// This array must correspond with the CompositionMode enum in the
// mozc/session/command.proto file.
COMPILE_ASSERT(
    mozc::commands::NUM_OF_COMPOSITIONS == arraysize(kPropCompositionModes),
    bad_number_of_modes);

}  // namespace

namespace mozc_unix_scim {

/* static */
ScimMozc *ScimMozc::CreateScimMozc(scim::IMEngineFactoryBase *factory,
                                   const scim::String &encoding, int id,
                                   const scim::ConfigPointer *config) {
  return new ScimMozc(factory, encoding, id,
                      config,
                      MozcConnection::CreateMozcConnection(),
                      new MozcResponseParser);
}

// For unittests.
ScimMozc::ScimMozc(scim::IMEngineFactoryBase *factory,
                   const scim::String &encoding, int id,
                   const scim::ConfigPointer *config,
                   MozcConnectionInterface *connection,
                   MozcResponseParser *parser)
    : scim::IMEngineInstanceBase(factory, encoding, id),
      connection_(connection),
      parser_(parser),
      composition_mode_(mozc::commands::HIRAGANA) {
  VLOG(1) << "ScimMozc created.";
  const bool is_vertical
      = config ? (*config)->read(kConfigName, false) : false;
  parser_->set_use_annotation(is_vertical);
  InitializeBar();
}

ScimMozc::~ScimMozc() {
  VLOG(1) << "ScimMozc destroyed.";
}

// This function is called from SCIM framework when users press or release a
// key.
bool ScimMozc::process_key_event(const scim::KeyEvent &key) {
  VLOG(1) << "process_key_event, key.code=" << key.code;

  if (!connection_->CanSend(key)) {
    VLOG(1) << "Mozc doesn't handle the key. Not consumed.";
    return false;  // not consumed.
  }

  string error;
  mozc::commands::Output raw_response;
  if (!connection_->TrySendKeyEvent(
          key, composition_mode_, &raw_response, &error)) {
    // TODO(yusukes): Show |error|.
    return false;  // not consumed.
  }

  return ParseResponse(raw_response);
}

// This function is called from SCIM framework when users click the candidate
// window.
void ScimMozc::select_candidate(unsigned int index) {
  VLOG(1) << "select_candidate, index=" << index;

  if (!candidates_.get()) {
    LOG(ERROR) << "Candidate window clicked, but we don't have the instance.";
    return;
  }

  const int32 id = candidates_->GetId(index);
  if (id == kBadCandidateId) {
    LOG(ERROR) << "The clicked candidate doesn't have unique ID.";
    return;
  }
  VLOG(1) << "select_candidate, id=" << id;

  string error;
  mozc::commands::Output raw_response;
  if (!connection_->TrySendClick(id, &raw_response, &error)) {
    LOG(ERROR) << "IPC failed. error=" << error;
    SetAuxString(error);
    DrawAll();
  } else {
    ParseResponse(raw_response);
  }
}

// This function is called from SCIM framework.
void ScimMozc::reset() {
  VLOG(1) << "reset";
  string error;
  mozc::commands::Output raw_response;
  if (connection_->TrySendCommand(
          mozc::commands::SessionCommand::REVERT, &raw_response, &error)) {
    parser_->ParseResponse(raw_response, this);
  }
  ClearAll();  // just in case.
  DrawAll();
}

// This function is called from SCIM framework when the ic gets focus.
void ScimMozc::focus_in() {
  VLOG(1) << "focus_in";
  DrawAll();
  InitializeBar();
}

// This function is called when the ic loses focus.
void ScimMozc::focus_out() {
  VLOG(1) << "focus_out";
  string error;
  mozc::commands::Output raw_response;
  if (connection_->TrySendCommand(
          mozc::commands::SessionCommand::REVERT, &raw_response, &error)) {
    parser_->ParseResponse(raw_response, this);
  }
  ClearAll();  // just in case.
  DrawAll();
  // TODO(yusukes): Call session::SyncData() like ibus-mozc.
}

// This function is called from SCIM framework when Mozc related icon in the
// SCIM toolbar is pressed.
void ScimMozc::trigger_property(const scim::String &property) {
  VLOG(1) << "trigger_property: " << property;

  for (size_t i = 0; i < kNumCompositionModes; ++i) {
    if (property == kPropCompositionModes[i].config_path) {
      if (kPropCompositionModes[i].mode == mozc::commands::DIRECT) {
        // Commit a preedit string.
        string error;
        mozc::commands::Output raw_response;
        if (connection_->TrySendCommand(mozc::commands::SessionCommand::SUBMIT,
                                        &raw_response, &error)) {
          parser_->ParseResponse(raw_response, this);
        }
        DrawAll();
        // Switch to the DIRECT mode.
        SetCompositionMode(mozc::commands::DIRECT);
      } else {
        // Send the SWITCH_INPUT_MODE command.
        string error;
        mozc::commands::Output raw_response;
        if (connection_->TrySendCompositionMode(
                kPropCompositionModes[i].mode, &raw_response, &error)) {
          parser_->ParseResponse(raw_response, this);
        }
      }
      return;
    }
  }

  string args;
  if (property == kPropToolDictionary) {
    args = "--mode=dictionary_tool";
  } else if (property == kPropToolProperty) {
    args = "--mode=config_dialog";
  } else {
    // Unknown property.
    return;
  }

  // Spawn mozc_tool.
  // TODO(yusukes): Use session::LaunchTool().
  mozc::Process::SpawnMozcProcess("mozc_tool", args);
}

bool ScimMozc::ParseResponse(const mozc::commands::Output &raw_response) {
  ClearAll();
  const bool consumed = parser_->ParseResponse(raw_response, this);
  if (!consumed) {
    VLOG(1) << "The input was not consumed by Mozc.";
  }
  OpenUrl();
  DrawAll();
  return consumed;
}

void ScimMozc::SetResultString(const scim::WideString &result_string) {
  commit_string(result_string);
}

void ScimMozc::SetCandidateWindow(const MozcLookupTable *new_candidates) {
  candidates_.reset(new_candidates);
}

void ScimMozc::SetPreeditInfo(const PreeditInfo *preedit_info) {
  preedit_info_.reset(preedit_info);
}

void ScimMozc::SetAuxString(const scim::String &str) {
  aux_ = str;
}

void ScimMozc::SetCompositionMode(mozc::commands::CompositionMode mode) {
  composition_mode_ = mode;
  // Update the bar.
  const char *icon = GetCurrentCompositionModeIcon();
  const char *label = GetCurrentCompositionModeLabel();
  scim::Property p = scim::Property(
      kPropCompositionModeIcon, label, icon, "Composition mode");
  update_property(p);
}

void ScimMozc::SetUrl(const string &url) {
  url_ = url;
}

void ScimMozc::ClearAll() {
  SetCandidateWindow(NULL);
  SetPreeditInfo(NULL);
  SetAuxString("");
  url_.clear();
}

void ScimMozc::DrawCandidateWindow() {
  if (!candidates_.get()) {
    VLOG(1) << "HideCandidateWindow";
    hide_lookup_table();
  } else {
    VLOG(1) << "DrawCandidateWindow";
    update_lookup_table(*candidates_.get());
    show_lookup_table();
  }
}

void ScimMozc::DrawPreeditInfo() {
  if (!preedit_info_.get()) {
    hide_preedit_string();
  } else {
    VLOG(1) << "DrawPreeditInfo: cursor=" << preedit_info_->cursor_pos;
    update_preedit_string(preedit_info_->str,
                          preedit_info_->attribute_list);
    update_preedit_caret(preedit_info_->cursor_pos);
    show_preedit_string();
  }
}

void ScimMozc::DrawAux() {
  if (aux_.empty()) {
    hide_aux_string();
  } else {
    update_aux_string(scim::utf8_mbstowcs(aux_));
    show_aux_string();
  }
}

void ScimMozc::DrawAll() {
  DrawPreeditInfo();
  DrawAux();
  DrawCandidateWindow();
}

void ScimMozc::OpenUrl() {
  if (url_.empty()) {
    return;
  }
  mozc::Process::OpenBrowser(url_);
  url_.clear();
}

void ScimMozc::InitializeBar() {
  VLOG(1) << "Registering properties";
  // TODO(yusukes): L10N needed for "Tool", "Dictionary", and "Property".
  scim::PropertyList prop_list;

  const char *icon = GetCurrentCompositionModeIcon();
  const char *label = GetCurrentCompositionModeLabel();
  scim::Property p = scim::Property(
      kPropCompositionModeIcon, label, icon, "Composition mode");
  prop_list.push_back(p);
  for (size_t i = 0; i < kNumCompositionModes; ++i) {
    p = scim::Property(kPropCompositionModes[i].config_path,
                       kPropCompositionModes[i].description,
                       kPropCompositionModes[i].icon,
                       kPropCompositionModes[i].description);
    prop_list.push_back(p);
  }

  if (mozc::Util::FileExists(mozc::Util::JoinPath(
          mozc::Util::GetServerDirectory(), mozc::kMozcTool))) {
    // Construct "tool" icon and its menu.
    p = scim::Property(kPropTool, "", kPropToolIcon, "Tool");
    prop_list.push_back(p);
    p = scim::Property(
        kPropToolDictionary, "Dictionary", kPropToolDictionaryIcon);
    prop_list.push_back(p);
    p = scim::Property(kPropToolProperty, "Property", kPropToolPropertyIcon);
    prop_list.push_back(p);
  }

  register_properties(prop_list);
}

const char *ScimMozc::GetCurrentCompositionModeIcon() const {
  DCHECK(composition_mode_ < kNumCompositionModes);
  if (composition_mode_ < kNumCompositionModes) {
    return kPropCompositionModes[composition_mode_].icon;
  }
  return "";
}

const char *ScimMozc::GetCurrentCompositionModeLabel() const {
  DCHECK(composition_mode_ < kNumCompositionModes);
  if (composition_mode_ < kNumCompositionModes) {
    return kPropCompositionModes[composition_mode_].label;
  }
  return "";
}

}  // namespace mozc_unix_scim
