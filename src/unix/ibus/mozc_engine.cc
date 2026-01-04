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

#include "unix/ibus/mozc_engine.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/clock.h"
#include "base/const.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/vlog.h"
#include "client/client.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "renderer/renderer_client.h"
#include "unix/ibus/candidate_window_handler.h"
#include "unix/ibus/engine_registrar.h"
#include "unix/ibus/ibus_candidate_window_handler.h"
#include "unix/ibus/ibus_config.h"
#include "unix/ibus/ibus_wrapper.h"
#include "unix/ibus/key_event_handler.h"
#include "unix/ibus/message_translator.h"
#include "unix/ibus/path_util.h"
#include "unix/ibus/preedit_handler.h"
#include "unix/ibus/property_handler.h"
#include "unix/ibus/surrounding_text_util.h"

ABSL_FLAG(bool, use_mozc_renderer, true,
          "The engine tries to use mozc_renderer if available.");

namespace mozc {
namespace ibus {
namespace {

// The ID for candidates which are not associated with texts.
const int32_t kBadCandidateId = -1;

// Default UI locale
constexpr char kMozcDefaultUILocale[] = "en_US.UTF-8";

// for every 5 minutes, call SyncData
const absl::Duration kSyncDataInterval = absl::Minutes(5);

const char *kUILocaleEnvNames[] = {
    "LC_ALL",
    "LC_MESSAGES",
    "LANG",
};

std::string GetEnv(const char *envname) {
  const char *result = ::getenv(envname);
  return result != nullptr ? std::string(result) : "";
}

std::string GetMessageLocale() {
  for (size_t i = 0; i < std::size(kUILocaleEnvNames); ++i) {
    const std::string result = GetEnv(kUILocaleEnvNames[i]);
    if (!result.empty()) {
      return result;
    }
  }
  return kMozcDefaultUILocale;
}

struct SurroundingTextInfo {
  SurroundingTextInfo() : relative_selected_length(0) {}
  int32_t relative_selected_length;
  std::string preceding_text;
  std::string selection_text;
  std::string following_text;
};

bool GetSurroundingText(IbusEngineWrapper *engine, SurroundingTextInfo *info) {
  if (!(engine->CheckCapabilities(IBUS_CAP_SURROUNDING_TEXT))) {
    MOZC_VLOG(1) << "Give up CONVERT_REVERSE due to client_capabilities: "
                 << engine->GetCapabilities();
    return false;
  }
  uint cursor_pos = 0;
  uint anchor_pos = 0;
  const absl::string_view surrounding_text =
      engine->GetSurroundingText(&cursor_pos, &anchor_pos);

  if (!SurroundingTextUtil::GetSafeDelta(cursor_pos, anchor_pos,
                                         &info->relative_selected_length)) {
    LOG(ERROR) << "Too long text selection.";
    return false;
  }

  // あい[うえ]お
  //     ^1   ^2
  //
  // [, ]: selection boundary (not actual characters).
  // 1: pos1 (cursor_pos or anchor_pos)
  // 2: pos2 (pos1 + selection_length)
  //
  // surrounding_text = "あいうえお"
  // preceding_text = "あい"
  // selection_text = "うえ"
  // following_text = "お"

  const size_t pos1 = std::min(cursor_pos, anchor_pos);
  const size_t selection_length = abs(info->relative_selected_length);
  const size_t pos2 = pos1 + selection_length;
  const size_t text_length = Util::CharsLen(surrounding_text);

  if (text_length < pos2) {
    LOG(ERROR) << "selection is out of surrounding_text: ('" << surrounding_text
               << "', " << pos2 << ").";
    return false;
  }

  Util::Utf8SubString(surrounding_text, 0, pos1, &(info->preceding_text));
  Util::Utf8SubString(surrounding_text, pos1, selection_length,
                      &(info->selection_text));
  Util::Utf8SubString(surrounding_text, pos2, text_length - pos2,
                      &(info->following_text));
  return true;
}

std::unique_ptr<client::ClientInterface> CreateAndConfigureClient() {
  std::unique_ptr<client::ClientInterface> client(
      client::ClientFactory::NewClient());
  // Currently client capability is fixed.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  client->set_client_capability(capability);
  return client;
}

std::optional<absl::string_view> GetMapValue(
    const absl::flat_hash_map<std::string, std::string> &map,
    absl::string_view key) {
  absl::flat_hash_map<std::string, std::string>::const_iterator it =
      map.find(key);
  if (it == map.end()) {
    return std::nullopt;
  }
  return absl::string_view(it->second);
}

bool IsWaylandSession(
    const absl::flat_hash_map<std::string, std::string> &env) {
  return env.contains("WAYLAND_DISPLAY");
}

std::vector<std::string> GetCurrentDesktops(
    const absl::flat_hash_map<std::string, std::string> &env) {
  const std::optional<absl::string_view> env_xdg_current_desktop =
      GetMapValue(env, "XDG_CURRENT_DESKTOP");
  if (!env_xdg_current_desktop.has_value()) {
    return {};
  }
  // $XDG_CURRENT_DESKTOP is a colon-separated list.
  // https://specifications.freedesktop.org/desktop-entry-spec/desktop-entry-spec-latest.html#recognized-keys
  return absl::StrSplit(env_xdg_current_desktop.value(), ':');
}

void UpdateEnvironMap(absl::flat_hash_map<std::string, std::string> &env,
                      const char *envname) {
  const char *result = ::getenv(envname);
  if (result == nullptr) {
    return;
  }
  env.insert_or_assign(envname, result);
}

bool UseMozcCandidateWindow(const IbusConfig &ibus_config) {
  if (!absl::GetFlag(FLAGS_use_mozc_renderer)) {
    return false;
  }

  const std::string renderer_path =
      FileUtil::JoinPath(SystemUtil::GetServerDirectory(), "mozc_renderer");
  if (absl::Status s = FileUtil::FileExists(renderer_path); !s.ok()) {
    LOG(ERROR) << s;
    return false;
  }

  absl::flat_hash_map<std::string, std::string> env;
  UpdateEnvironMap(env, "MOZC_IBUS_CANDIDATE_WINDOW");
  UpdateEnvironMap(env, "XDG_CURRENT_DESKTOP");
  UpdateEnvironMap(env, "WAYLAND_DISPLAY");

  return CanUseMozcCandidateWindow(ibus_config, env);
}

}  // namespace

MozcEngine::MozcEngine()
    : last_sync_time_(Clock::GetAbslTime()),
      key_event_handler_(std::make_unique<KeyEventHandler>()),
      client_(CreateAndConfigureClient()),
      preedit_handler_(std::make_unique<PreeditHandler>()),
      use_mozc_candidate_window_(false),
      mozc_candidate_window_handler_(renderer::RendererClient::Create()),
      preedit_method_(config::Config::ROMAN) {
  ibus_config_.Initialize();
  use_mozc_candidate_window_ = UseMozcCandidateWindow(ibus_config_);
  if (use_mozc_candidate_window_) {
    mozc_candidate_window_handler_.RegisterGSettingsObserver();
  }
  property_handler_ = std::make_unique<PropertyHandler>(
      std::make_unique<LocaleBasedMessageTranslator>(GetMessageLocale()),
      ibus_config_.IsActiveOnLaunch(), client_.get());

  // TODO(yusukes): write a unit test to check if the capability is set
  // as expected.
}

MozcEngine::~MozcEngine() { SyncData(true); }

void MozcEngine::CandidateClicked(IbusEngineWrapper *engine, uint index,
                                  uint button, uint state) {
  if (index >= unique_candidate_ids_.size()) {
    return;
  }
  const int32_t id = unique_candidate_ids_[index];
  if (id == kBadCandidateId) {
    return;
  }
  commands::Output output;
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::SELECT_CANDIDATE);
  command.set_id(id);
  client_->SendCommand(command, &output);
  UpdateAll(engine, output);
}

void MozcEngine::CursorDown(IbusEngineWrapper *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::CursorUp(IbusEngineWrapper *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::Disable(IbusEngineWrapper *engine) {
  RevertSession(engine);
  GetCandidateWindowHandler(engine)->Hide(engine);
  key_event_handler_->Clear();
}

namespace {
commands::CompositionMode ConvertCompositionMode(
    ibus::Engine::CompositionMode mode) {
  switch (mode) {
    case ibus::Engine::DIRECT:
      return commands::DIRECT;
    case ibus::Engine::HIRAGANA:
      return commands::HIRAGANA;
    case ibus::Engine::FULL_KATAKANA:
      return commands::FULL_KATAKANA;
    case ibus::Engine::HALF_ASCII:
      return commands::HALF_ASCII;
    case ibus::Engine::FULL_ASCII:
      return commands::FULL_ASCII;
    case ibus::Engine::HALF_KATAKANA:
      return commands::HALF_KATAKANA;
    default:
      return commands::NUM_OF_COMPOSITIONS;
  }
}
}  // namespace

void MozcEngine::Enable(IbusEngineWrapper *engine) {
  // Launch mozc_server
  client_->EnsureConnection();
  UpdatePreeditMethod();

  // When ibus-mozc is disabled by the "next input method" hot key, ibus-daemon
  // does not call MozcEngine::Disable(). Call RevertSession() here so the
  // mozc_server could discard a preedit string before the hot key is pressed
  // (crosbug.com/4596).
  RevertSession(engine);

  engine->EnableSurroundingText();

  commands::CompositionMode mode = ConvertCompositionMode(
      ibus_config_.GetCompositionMode(engine->GetName()));

  if (mode == commands::NUM_OF_COMPOSITIONS) {
    // Do nothing.
  } else {
    commands::SessionCommand command;
    if (mode == commands::DIRECT) {
      command.set_type(commands::SessionCommand::TURN_OFF_IME);
    } else {
      command.set_type(commands::SessionCommand::TURN_ON_IME);
      command.set_composition_mode(mode);
    }
    commands::Output output;
    if (!client_->SendCommand(command, &output)) {
      LOG(ERROR) << "SendCommand failed";
    }
    property_handler_->Update(engine, output);
  }
}

void MozcEngine::FocusIn(IbusEngineWrapper *engine) {
  property_handler_->Register(engine);
  UpdatePreeditMethod();
}

void MozcEngine::FocusOut(IbusEngineWrapper *engine) {
  GetCandidateWindowHandler(engine)->Hide(engine);
  property_handler_->ResetContentType(engine);

  // Note that the preedit string (if any) will be committed by IBus runtime
  // because we are specifying |IBUS_ENGINE_PREEDIT_COMMIT| flag to
  // |ibus_engine_update_preedit_text_with_mode|. All we need to do here is
  // simply resetting the current session in case there is a non-empty
  // preedit text. Note that |RevertSession| is supposed to do nothing when
  // there is no preedit text.
  // See https://github.com/google/mozc/issues/255 for details.
  RevertSession(engine);
  SyncData(false);
}

void MozcEngine::PageDown(IbusEngineWrapper *engine) {
  // TODO(mazda,yusukes): Implement this to support arrow icons inside the Gtk+
  // candidate window.
}

void MozcEngine::PageUp(IbusEngineWrapper *engine) {
  // TODO(mazda,yusukes): Implement this to support arrow icons inside the Gtk+
  // candidate window.
}

bool MozcEngine::ProcessKeyEvent(IbusEngineWrapper *engine, uint keyval,
                                 uint keycode, uint modifiers) {
  MOZC_VLOG(2) << "keyval: " << keyval << ", keycode: " << keycode
               << ", modifiers: " << modifiers;
  if (property_handler_->IsDisabled()) {
    return false;
  }

  // layout_is_jp is only used determine Kana input with US layout.
  const absl::string_view layout = ibus_config_.GetLayout(engine->GetName());
  const bool layout_is_jp = (layout != "us");

  commands::KeyEvent key;
  if (!key_event_handler_->GetKeyEvent(keyval, keycode, modifiers,
                                       preedit_method_, layout_is_jp, &key)) {
    // Doesn't send a key event to mozc_server.
    return false;
  }

  MOZC_VLOG(2) << key;
  if (!property_handler_->IsActivated() && !client_->IsDirectModeCommand(key)) {
    return false;
  }

  key.set_activated(property_handler_->IsActivated());
  key.set_mode(property_handler_->GetOriginalCompositionMode());

  commands::Context context;
  SurroundingTextInfo surrounding_text_info;
  if (GetSurroundingText(engine, &surrounding_text_info)) {
    context.set_preceding_text(surrounding_text_info.preceding_text);
    context.set_following_text(surrounding_text_info.following_text);
  }
  commands::Output output;
  if (!client_->SendKeyWithContext(key, context, &output)) {
    LOG(ERROR) << "SendKey failed";
    return false;
  }

  MOZC_VLOG(2) << output;

  UpdateAll(engine, output);

  return output.consumed();
}

void MozcEngine::PropertyActivate(IbusEngineWrapper *engine,
                                  const char *property_name,
                                  uint property_state) {
  property_handler_->ProcessPropertyActivate(engine, property_name,
                                             property_state);
}

void MozcEngine::PropertyHide(IbusEngineWrapper *engine,
                              const char *property_name) {
  // We can ignore the signal.
}

void MozcEngine::PropertyShow(IbusEngineWrapper *engine,
                              const char *property_name) {
  // We can ignore the signal.
}

void MozcEngine::Reset(IbusEngineWrapper *engine) { RevertSession(engine); }

void MozcEngine::SetCapabilities(IbusEngineWrapper *engine, uint capabilities) {
  // Do nothing.
}

void MozcEngine::SetCursorLocation(IbusEngineWrapper *engine, int x, int y,
                                   int w, int h) {
  GetCandidateWindowHandler(engine)->UpdateCursorRect(engine);
}

void MozcEngine::SetContentType(IbusEngineWrapper *engine, uint purpose,
                                uint hints) {
  const bool prev_disabled = property_handler_->IsDisabled();
  property_handler_->UpdateContentType(engine);
  if (!prev_disabled && property_handler_->IsDisabled()) {
    // Make sure on-going composition is reverted.
    RevertSession(engine);
  }
}

bool MozcEngine::UpdateAll(IbusEngineWrapper *engine,
                           const commands::Output &output) {
  UpdateDeletionRange(engine, output);
  UpdateResult(engine, output);
  preedit_handler_->Update(engine, output);
  GetCandidateWindowHandler(engine)->Update(engine, output);
  UpdateCandidateIDMapping(output);

  property_handler_->Update(engine, output);

  LaunchTool(output);
  ExecuteCallback(engine, output);
  return true;
}

bool MozcEngine::UpdateDeletionRange(IbusEngineWrapper *engine,
                                     const commands::Output &output) {
  if (output.has_deletion_range() && output.deletion_range().offset() < 0 &&
      output.deletion_range().offset() + output.deletion_range().length() >=
          0) {
    engine->DeleteSurroundingText(output.deletion_range().offset(),
                                  output.deletion_range().length());
  }
  return true;
}

bool MozcEngine::UpdateResult(IbusEngineWrapper *engine,
                              const commands::Output &output) const {
  if (!output.has_result()) {
    MOZC_VLOG(2) << "output doesn't contain result";
    return true;
  }

  engine->CommitText(output.result().value());
  return true;
}

bool MozcEngine::UpdateCandidateIDMapping(const commands::Output &output) {
  if (!output.has_candidate_window() ||
      output.candidate_window().candidate_size() == 0) {
    return true;
  }

  unique_candidate_ids_.clear();
  const commands::CandidateWindow &candidate_window = output.candidate_window();
  for (int i = 0; i < candidate_window.candidate_size(); ++i) {
    if (candidate_window.candidate(i).has_id()) {
      const int32_t id = candidate_window.candidate(i).id();
      unique_candidate_ids_.push_back(id);
    } else {
      // The parent node of the cascading window does not have an id since the
      // node does not contain a candidate word.
      unique_candidate_ids_.push_back(kBadCandidateId);
    }
  }
  return true;
}

void MozcEngine::UpdatePreeditMethod() {
  config::Config config;
  if (!client_->GetConfig(&config)) {
    LOG(ERROR) << "GetConfig failed";
    return;
  }
  preedit_method_ = config.has_preedit_method() ? config.preedit_method()
                                                : config::Config::ROMAN;
}

void MozcEngine::SyncData(bool force) {
  if (client_ == nullptr) {
    return;
  }

  const absl::Time current_time = Clock::GetAbslTime();
  if (force || (current_time >= last_sync_time_ &&
                current_time - last_sync_time_ >= kSyncDataInterval)) {
    MOZC_VLOG(1) << "Syncing data";
    client_->SyncData();
    last_sync_time_ = current_time;
  }
}

bool MozcEngine::LaunchTool(const commands::Output &output) const {
  if (!client_->LaunchToolWithProtoBuf(output)) {
    MOZC_VLOG(2) << output << " Launch Failed";
    return false;
  }

  return true;
}

void MozcEngine::RevertSession(IbusEngineWrapper *engine) {
  // TODO(team): We should skip following actions when there is no on-going
  // composition.
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::REVERT);
  commands::Output output;
  if (!client_->SendCommand(command, &output)) {
    LOG(ERROR) << "RevertSession() failed";
    return;
  }
  UpdateAll(engine, output);
}

bool MozcEngine::ExecuteCallback(IbusEngineWrapper *engine,
                                 const commands::Output &output) {
  if (!output.has_callback()) {
    return false;
  }

  // TODO(nona): Make IBus interface class and add unittest for ibus APIs.
  if (!output.callback().has_session_command()) {
    LOG(ERROR) << "callback does not have session_command";
    return false;
  }

  const commands::SessionCommand &callback_command =
      output.callback().session_command();

  if (!callback_command.has_type()) {
    LOG(ERROR) << "callback_command has no type";
    return false;
  }

  commands::SessionCommand session_command;
  session_command.set_type(callback_command.type());

  // TODO(nona): Make a function to handle CONVERT_REVERSE.
  // Used by CONVERT_REVERSE and/or UNDO
  // This value represents how many characters are selected as a relative
  // distance of characters. Positive value represents forward text selection
  // and negative value represents backword text selection.
  // Note that you should not allow 0x80000000 for |relative_selected_length|
  // because you cannot safely use |-relative_selected_length| nor
  // |abs(relative_selected_length)| in this case due to integer overflow.
  SurroundingTextInfo surrounding_text_info;

  switch (callback_command.type()) {
    case commands::SessionCommand::UNDO:
      // Having |IBUS_CAP_SURROUNDING_TEXT| does not necessarily mean that the
      // client supports |ibus_engine_delete_surrounding_text()|, but there is
      // no other good criteria.
      if (!(engine->CheckCapabilities(IBUS_CAP_SURROUNDING_TEXT))) {
        return false;
      }
      break;
    case commands::SessionCommand::CONVERT_REVERSE: {
      if (!GetSurroundingText(engine, &surrounding_text_info)) {
        return false;
      }
      session_command.set_text(surrounding_text_info.selection_text);
      break;
    }
    default:
      return false;
  }

  commands::Output new_output;
  if (!client_->SendCommand(session_command, &new_output)) {
    LOG(ERROR) << "Callback Command Failed";
    return false;
  }

  if (callback_command.type() == commands::SessionCommand::CONVERT_REVERSE) {
    // We need to remove selected text as a first step of reconversion.
    commands::DeletionRange *range = new_output.mutable_deletion_range();
    // Use DeletionRange field to remove the selected text.
    // For forward selection (that is, |relative_selected_length > 0|), the
    // offset should be a negative value to delete preceding text.
    // For backward selection (that is, |relative_selected_length < 0|),
    // IBus and/or some applications seem to expect |offset == 0| somehow.
    const int32_t offset =
        surrounding_text_info.relative_selected_length > 0
            ? -surrounding_text_info
                   .relative_selected_length  // forward selection
            : 0;                              // backward selection
    range->set_offset(offset);
    range->set_length(abs(surrounding_text_info.relative_selected_length));
  }

  // Here uses recursion of UpdateAll but it's okay because the converter
  // ensures that the second output never contains callback.
  UpdateAll(engine, new_output);
  return true;
}

CandidateWindowHandlerInterface *MozcEngine::GetCandidateWindowHandler(
    IbusEngineWrapper *engine) {
  if (use_mozc_candidate_window_ &&
      engine->CheckCapabilities(IBUS_CAP_PREEDIT_TEXT)) {
    return &mozc_candidate_window_handler_;
  }
  return &ibus_candidate_window_handler_;
}

bool CanUseMozcCandidateWindow(
    const IbusConfig &ibus_config,
    const absl::flat_hash_map<std::string, std::string> &env) {
  if (!ibus_config.IsMozcRendererEnabled()) {
    return false;
  }

  const std::optional<absl::string_view> env_candidate_window =
      GetMapValue(env, "MOZC_IBUS_CANDIDATE_WINDOW");
  if (env_candidate_window.has_value() &&
      env_candidate_window.value() == "ibus") {
    return false;
  }

  if (!IsWaylandSession(env)) {
    return true;
  }

  const std::vector<std::string> current_desktops = GetCurrentDesktops(env);
  if (current_desktops.empty()) {
    return false;
  }
  for (const std::string &compatible_desktop :
       ibus_config.GetMozcRendererCompatibleWaylandDesktopNames()) {
    if (std::find(current_desktops.begin(), current_desktops.end(),
                  compatible_desktop) != current_desktops.end()) {
      return true;
    }
  }
  return false;
}

}  // namespace ibus
}  // namespace mozc
