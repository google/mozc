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
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <sstream>
#include <string>

#include "base/clock.h"
#include "base/const.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "client/client.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/ime_switch_util.h"
#include "unix/ibus/engine_registrar.h"
#include "unix/ibus/ibus_candidate_window_handler.h"
#include "unix/ibus/key_event_handler.h"
#include "unix/ibus/message_translator.h"
#include "unix/ibus/mozc_engine_property.h"
#include "unix/ibus/path_util.h"
#include "unix/ibus/preedit_handler.h"
#include "unix/ibus/property_handler.h"
#include "unix/ibus/surrounding_text_util.h"
#include "absl/flags/flag.h"
#include "absl/memory/memory.h"

#ifdef ENABLE_GTK_RENDERER
#include "renderer/renderer_client.h"
#include "unix/ibus/gtk_candidate_window_handler.h"
#endif  // ENABLE_GTK_RENDERER

#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
#include "unix/ibus/selection_monitor.h"
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR

#ifdef ENABLE_GTK_RENDERER
ABSL_FLAG(bool, use_mozc_renderer, true,
          "The engine tries to use mozc_renderer if available.");
#endif  // ENABLE_GTK_RENDERER

namespace {

// The ID for candidates which are not associated with texts.
const int32 kBadCandidateId = -1;

// Default UI locale
constexpr char kMozcDefaultUILocale[] = "en_US.UTF-8";

// for every 5 minutes, call SyncData
const uint64 kSyncDataInterval = 5 * 60;

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

struct IBusMozcEngineClass {
  IBusEngineClass parent;
};

struct IBusMozcEngine {
  IBusEngine parent;
  mozc::ibus::MozcEngine *engine;
};

IBusEngineClass *g_parent_class = nullptr;

GObject *MozcEngineClassConstructor(
    GType type, guint n_construct_properties,
    GObjectConstructParam *construct_properties) {
  return G_OBJECT_CLASS(g_parent_class)
      ->constructor(type, n_construct_properties, construct_properties);
}

void MozcEngineClassDestroy(IBusObject *engine) {
  IBUS_OBJECT_CLASS(g_parent_class)->destroy(engine);
}

void MozcEngineClassInit(gpointer klass, gpointer class_data) {
  IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);

  VLOG(2) << "MozcEngineClassInit is called";
  mozc::ibus::EngineRegistrar::Register(
      mozc::Singleton<mozc::ibus::MozcEngine>::get(), engine_class);

  g_parent_class =
      reinterpret_cast<IBusEngineClass *>(g_type_class_peek_parent(klass));

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->constructor = MozcEngineClassConstructor;
  IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS(klass);
  ibus_object_class->destroy = MozcEngineClassDestroy;
}

void MozcEngineInstanceInit(GTypeInstance *instance, gpointer klass) {
  IBusMozcEngine *engine = reinterpret_cast<IBusMozcEngine *>(instance);
  engine->engine = mozc::Singleton<mozc::ibus::MozcEngine>::get();
}

}  // namespace

namespace mozc {
namespace ibus {

namespace {
struct SurroundingTextInfo {
  SurroundingTextInfo() : relative_selected_length(0) {}
  int32 relative_selected_length;
  std::string preceding_text;
  std::string selection_text;
  std::string following_text;
};

bool GetSurroundingText(IBusEngine *engine,
#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
                        SelectionMonitorInterface *selection_monitor,
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR
                        SurroundingTextInfo *info) {
  if (!(engine->client_capabilities & IBUS_CAP_SURROUNDING_TEXT)) {
    VLOG(1) << "Give up CONVERT_REVERSE due to client_capabilities: "
            << engine->client_capabilities;
    return false;
  }
  guint cursor_pos = 0;
  guint anchor_pos = 0;
  // DO NOT call g_object_unref against this.
  // http://developer.gnome.org/gobject/stable/gobject-The-Base-Object-Type.html#gobject-The-Base-Object-Type.description
  IBusText *text = nullptr;
  ibus_engine_get_surrounding_text(engine, &text, &cursor_pos, &anchor_pos);
  const std::string surrounding_text(ibus_text_get_text(text));

#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
  if (cursor_pos == anchor_pos && selection_monitor != nullptr) {
    const SelectionInfo &info = selection_monitor->GetSelectionInfo();
    guint new_anchor_pos = 0;
    if (SurroundingTextUtil::GetAnchorPosFromSelection(
            surrounding_text, info.selected_text, cursor_pos,
            &new_anchor_pos)) {
      anchor_pos = new_anchor_pos;
    }
  }
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR

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

#if defined(ENABLE_GTK_RENDERER) || defined(ENABLE_QT_RENDERER)
CandidateWindowHandlerInterface *createGtkCandidateWindowHandler(
    ::mozc::renderer::RendererClient *renderer_client) {
  if (!absl::GetFlag(FLAGS_use_mozc_renderer)) {
    return nullptr;
  }
  if (GetEnv("XDG_SESSION_TYPE") == "wayland") {
    // mozc_renderer is not supported on wayland session.
    return nullptr;
  }
  auto *handler = new GtkCandidateWindowHandler(renderer_client);
  handler->RegisterGSettingsObserver();
  return handler;
}
#endif  // ENABLE_GTK_RENDERER || ENABLE_QT_RENDERER

}  // namespace

MozcEngine::MozcEngine()
    : last_sync_time_(Clock::GetTime()),
      key_event_handler_(new KeyEventHandler),
      client_(CreateAndConfigureClient()),
#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
      selection_monitor_(SelectionMonitorFactory::Create(1024)),
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR
      preedit_handler_(new PreeditHandler()),
#if defined(ENABLE_GTK_RENDERER) || defined(ENABLE_QT_RENDERER)
      gtk_candidate_window_handler_(
          createGtkCandidateWindowHandler(new renderer::RendererClient())),
#endif  // ENABLE_GTK_RENDERER || ENABLE_QT_RENDERER
      ibus_candidate_window_handler_(new IBusCandidateWindowHandler()),
      preedit_method_(config::Config::ROMAN) {
#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
  if (selection_monitor_ != nullptr) {
    selection_monitor_->StartMonitoring();
  }
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR

  ibus_config_.Initialize();
  property_handler_ = absl::make_unique<PropertyHandler>(
      absl::make_unique<LocaleBasedMessageTranslator>(GetMessageLocale()),
      ibus_config_.IsActiveOnLaunch(), client_.get());

  // TODO(yusukes): write a unit test to check if the capability is set
  // as expected.
}

MozcEngine::~MozcEngine() { SyncData(true); }

void MozcEngine::CandidateClicked(IBusEngine *engine, guint index, guint button,
                                  guint state) {
  if (index >= unique_candidate_ids_.size()) {
    return;
  }
  const int32 id = unique_candidate_ids_[index];
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

void MozcEngine::CursorDown(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::CursorUp(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::Disable(IBusEngine *engine) {
  RevertSession(engine);
  GetCandidateWindowHandler(engine)->Hide(engine);
  key_event_handler_->Clear();
}

void MozcEngine::Enable(IBusEngine *engine) {
  // Launch mozc_server
  client_->EnsureConnection();
  UpdatePreeditMethod();

  // When ibus-mozc is disabled by the "next input method" hot key, ibus-daemon
  // does not call MozcEngine::Disable(). Call RevertSession() here so the
  // mozc_server could discard a preedit string before the hot key is pressed
  // (crosbug.com/4596).
  RevertSession(engine);

  // If engine wants to use surrounding text, we should call
  // ibus_engine_get_surrounding_text once when the engine enabled.
  ibus_engine_get_surrounding_text(engine, nullptr, nullptr, nullptr);
}

void MozcEngine::FocusIn(IBusEngine *engine) {
  property_handler_->Register(engine);
  UpdatePreeditMethod();
}

void MozcEngine::FocusOut(IBusEngine *engine) {
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

void MozcEngine::PageDown(IBusEngine *engine) {
  // TODO(mazda,yusukes): Implement this to support arrow icons inside the Gtk+
  // candidate window.
}

void MozcEngine::PageUp(IBusEngine *engine) {
  // TODO(mazda,yusukes): Implement this to support arrow icons inside the Gtk+
  // candidate window.
}

gboolean MozcEngine::ProcessKeyEvent(IBusEngine *engine, guint keyval,
                                     guint keycode, guint modifiers) {
  VLOG(2) << "keyval: " << keyval << ", keycode: " << keycode
          << ", modifiers: " << modifiers;
  if (property_handler_->IsDisabled()) {
    return FALSE;
  }

  // layout_is_jp is only used determine Kana input with US layout.
  const std::string &layout =
      ibus_config_.GetLayout(ibus_engine_get_name(engine));
  const bool layout_is_jp = (layout != "us");

  commands::KeyEvent key;
  if (!key_event_handler_->GetKeyEvent(keyval, keycode, modifiers,
                                       preedit_method_, layout_is_jp, &key)) {
    // Doesn't send a key event to mozc_server.
    return FALSE;
  }

  VLOG(2) << key.DebugString();
  if (!property_handler_->IsActivated() &&
      !config::ImeSwitchUtil::IsDirectModeCommand(key)) {
    return FALSE;
  }

  key.set_activated(property_handler_->IsActivated());
  key.set_mode(property_handler_->GetOriginalCompositionMode());

  commands::Context context;
  SurroundingTextInfo surrounding_text_info;
  if (GetSurroundingText(engine,
#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
                         selection_monitor_.get(),
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR
                         &surrounding_text_info)) {
    context.set_preceding_text(surrounding_text_info.preceding_text);
    context.set_following_text(surrounding_text_info.following_text);
  }
  commands::Output output;
  if (!client_->SendKeyWithContext(key, context, &output)) {
    LOG(ERROR) << "SendKey failed";
    return FALSE;
  }

  VLOG(2) << output.DebugString();

  UpdateAll(engine, output);

  const bool consumed = output.consumed();
  return consumed ? TRUE : FALSE;
}

void MozcEngine::PropertyActivate(IBusEngine *engine,
                                  const gchar *property_name,
                                  guint property_state) {
  property_handler_->ProcessPropertyActivate(engine, property_name,
                                             property_state);
}

void MozcEngine::PropertyHide(IBusEngine *engine, const gchar *property_name) {
  // We can ignore the signal.
}

void MozcEngine::PropertyShow(IBusEngine *engine, const gchar *property_name) {
  // We can ignore the signal.
}

void MozcEngine::Reset(IBusEngine *engine) { RevertSession(engine); }

void MozcEngine::SetCapabilities(IBusEngine *engine, guint capabilities) {
  // Do nothing.
}

void MozcEngine::SetCursorLocation(IBusEngine *engine, gint x, gint y, gint w,
                                   gint h) {
  GetCandidateWindowHandler(engine)->UpdateCursorRect(engine);
}

void MozcEngine::SetContentType(IBusEngine *engine, guint purpose,
                                guint hints) {
  const bool prev_disabled = property_handler_->IsDisabled();
  property_handler_->UpdateContentType(engine);
  if (!prev_disabled && property_handler_->IsDisabled()) {
    // Make sure on-going composition is reverted.
    RevertSession(engine);
  }
}

GType MozcEngine::GetType() {
  static GType type = 0;

  static const GTypeInfo type_info = {
      sizeof(IBusMozcEngineClass), nullptr, nullptr,
      MozcEngineClassInit,         nullptr, nullptr,
      sizeof(IBusMozcEngine),      0,       MozcEngineInstanceInit,
  };

  if (type == 0) {
    type = g_type_register_static(IBUS_TYPE_ENGINE, "IBusMozcEngine",
                                  &type_info, static_cast<GTypeFlags>(0));
    DCHECK_NE(type, 0) << "g_type_register_static failed";
  }

  return type;
}

// static
void MozcEngine::Disconnected(IBusBus *bus, gpointer user_data) { ibus_quit(); }

bool MozcEngine::UpdateAll(IBusEngine *engine, const commands::Output &output) {
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

bool MozcEngine::UpdateDeletionRange(IBusEngine *engine,
                                     const commands::Output &output) {
  if (output.has_deletion_range() && output.deletion_range().offset() < 0 &&
      output.deletion_range().offset() + output.deletion_range().length() >=
          0) {
    // Nowadays 'ibus_engine_delete_surrounding_text' becomes functional on
    // many of the major applications.  Confirmed that it works on
    // Firefox 10.0, LibreOffice 3.3.4 and GEdit 3.2.3.
    ibus_engine_delete_surrounding_text(engine,
                                        output.deletion_range().offset(),
                                        output.deletion_range().length());
  }
  return true;
}

bool MozcEngine::UpdateResult(IBusEngine *engine,
                              const commands::Output &output) const {
  if (!output.has_result()) {
    VLOG(2) << "output doesn't contain result";
    return true;
  }

  IBusText *text = ibus_text_new_from_string(output.result().value().c_str());
  ibus_engine_commit_text(engine, text);
  // |text| is released by ibus_engine_commit_text.
  return true;
}

bool MozcEngine::UpdateCandidateIDMapping(const commands::Output &output) {
  if (!output.has_candidates() || output.candidates().candidate_size() == 0) {
    return true;
  }

  unique_candidate_ids_.clear();
  const commands::Candidates &candidates = output.candidates();
  for (int i = 0; i < candidates.candidate_size(); ++i) {
    if (candidates.candidate(i).has_id()) {
      const int32 id = candidates.candidate(i).id();
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

  const uint64 current_time = Clock::GetTime();
  if (force || (current_time >= last_sync_time_ &&
                current_time - last_sync_time_ >= kSyncDataInterval)) {
    VLOG(1) << "Syncing data";
    client_->SyncData();
    last_sync_time_ = current_time;
  }
}

bool MozcEngine::LaunchTool(const commands::Output &output) const {
  if (!client_->LaunchToolWithProtoBuf(output)) {
    VLOG(2) << output.DebugString() << " Launch Failed";
    return false;
  }

  return true;
}

void MozcEngine::RevertSession(IBusEngine *engine) {
  // TODO(team): We should skip following actions when there is no on-going
  // omposition.
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::REVERT);
  commands::Output output;
  if (!client_->SendCommand(command, &output)) {
    LOG(ERROR) << "RevertSession() failed";
    return;
  }
  UpdateAll(engine, output);
}

bool MozcEngine::ExecuteCallback(IBusEngine *engine,
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
      if (!(engine->client_capabilities & IBUS_CAP_SURROUNDING_TEXT)) {
        return false;
      }
      break;
    case commands::SessionCommand::CONVERT_REVERSE: {
      if (!GetSurroundingText(engine,
#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
                              selection_monitor_.get(),
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR
                              &surrounding_text_info)) {
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
    const int32 offset =
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
    IBusEngine *engine) {
#ifndef ENABLE_GTK_RENDERER
  return ibus_candidate_window_handler_.get();
#else   // ENABLE_GTK_RENDERER
  if (!gtk_candidate_window_handler_) {
    return ibus_candidate_window_handler_.get();
  }

  if (!(engine->client_capabilities & IBUS_CAP_PREEDIT_TEXT)) {
    return ibus_candidate_window_handler_.get();
  }

  // TODO(nona): integrate with renderer/renderer_client.cc
  const std::string renderer_path =
      FileUtil::JoinPath(SystemUtil::GetServerDirectory(), "mozc_renderer");
  if (absl::Status s = FileUtil::FileExists(renderer_path); !s.ok()) {
    LOG(ERROR) << s;
    return ibus_candidate_window_handler_.get();
  }

  // TODO(nona): Check executable bit for renderer.
  return gtk_candidate_window_handler_.get();
#endif  // not ENABLE_GTK_RENDERER
}

}  // namespace ibus
}  // namespace mozc
