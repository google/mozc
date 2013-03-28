// Copyright 2010-2013, Google Inc.
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

#include "win32/tip/tip_private_context.h"

#include <msctf.h>

#include "base/win_util.h"
#include "client/client_interface.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "win32/base/deleter.h"
#include "win32/base/input_state.h"
#include "win32/base/surrogate_pair_observer.h"
#include "win32/tip/tip_ui_element_manager.h"
#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

using ::mozc::client::ClientFactory;
using ::mozc::client::ClientInterface;
using ::mozc::commands::Capability;
using ::mozc::commands::Output;
using ::mozc::config::Config;

struct KanaInputStyle {
  bool use_kana_input;
  bool use_keyboard_to_change_preedit_method;
};

KanaInputStyle GetDefaultKanaInputStyleImpl(ClientInterface *client) {
  KanaInputStyle style = {};
  bool in_appcontainer = false;
  if (!WinUtil::IsProcessInAppContainer(::GetCurrentProcess(),
                                        &in_appcontainer)) {
    return style;
  }

  Config config;
  if (in_appcontainer) {
    // config1.db is not readable from AppContainer. So retrieve the config
    // data from the server process.
    if (!client->CheckVersionOrRestartServer()) {
      return style;
    }
    if (!client->GetConfig(&config)) {
      return style;
    }
  } else {
    // config1.db should be readable in this case.
    config.CopyFrom(config::ConfigHandler::GetConfig());
  }

  style.use_kana_input = (config.preedit_method() == Config::KANA);
  style.use_keyboard_to_change_preedit_method =
      config.use_keyboard_to_change_preedit_method();
  return style;
}

KanaInputStyle GetDefaultKanaInputStyle(ClientInterface *client) {
  // Note: Thread-safety is not required.
  static KanaInputStyle default_style =
      GetDefaultKanaInputStyleImpl(client);
  return default_style;
}

}  // namespace

TipPrivateContext::TipPrivateContext(DWORD text_edit_sink_cookie,
                                     DWORD text_layout_sink_cookie)
    : client_(ClientFactory::NewClient()),
      surrogate_pair_observer_(new SurrogatePairObserver),
      last_output_(new Output()),
      input_state_(new InputState),
      input_behavior_(new InputBehavior),
      ui_element_manager_(new TipUiElementManager),
      deleter_(new VKBackBasedDeleter),
      text_edit_sink_cookie_(text_edit_sink_cookie),
      text_layout_sink_cookie_(text_layout_sink_cookie) {
  Capability capability;
  capability.set_text_deletion(Capability::DELETE_PRECEDING_TEXT);
  client_->set_client_capability(capability);

  const KanaInputStyle &kana_input_style =
      GetDefaultKanaInputStyle(client_.get());
  input_behavior_->prefer_kana_input = kana_input_style.use_kana_input;
  input_behavior_->use_romaji_key_to_toggle_input_style =
      kana_input_style.use_keyboard_to_change_preedit_method;
}

TipPrivateContext::~TipPrivateContext() {}

ClientInterface *TipPrivateContext::GetClient() {
  return client_.get();
}

SurrogatePairObserver *TipPrivateContext::GetSurrogatePairObserver() {
  return surrogate_pair_observer_.get();
}

TipUiElementManager *TipPrivateContext::GetUiElementManager() {
  return ui_element_manager_.get();
}

VKBackBasedDeleter *TipPrivateContext::GetDeleter() {
  return deleter_.get();
}

const Output &TipPrivateContext::last_output() const {
  return *last_output_;
}

Output *TipPrivateContext::mutable_last_output() {
  return last_output_.get();
}

const InputState &TipPrivateContext::input_state() const {
  return *input_state_;
}

InputState *TipPrivateContext::mutable_input_state() {
  return input_state_.get();
}

const InputBehavior &TipPrivateContext::input_behavior() const {
  return *input_behavior_;
}

InputBehavior *TipPrivateContext::mutable_input_behavior() {
  return input_behavior_.get();
}

DWORD TipPrivateContext::text_edit_sink_cookie() const {
  return text_edit_sink_cookie_;
}

DWORD TipPrivateContext::text_layout_sink_cookie() const {
  return text_layout_sink_cookie_;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
