// Copyright 2010-2014, Google Inc.
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

#include "base/const.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "client/client.h"
#include "config/config.pb.h"
#include "session/commands.pb.h"
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

#ifdef ENABLE_GTK_RENDERER
#include "renderer/renderer_client.h"
#include "unix/ibus/gtk_candidate_window_handler.h"
#endif  // ENABLE_GTK_RENDERER

#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
#include "unix/ibus/selection_monitor.h"
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR

#ifdef ENABLE_GTK_RENDERER
DEFINE_bool(use_mozc_renderer, true,
            "The engine tries to use mozc_renderer if available.");
#endif  // ENABLE_GTK_RENDERER

namespace {

// A key which associates an IBusProperty object with MozcEngineProperty.
const char kGObjectDataKey[] = "ibus-mozc-aux-data";
// An ID for a candidate which is not associated with a text.
const int32 kBadCandidateId = -1;
// The ibus-memconf section name in which we're interested.
const char kMozcSectionName[] = "engine/Mozc";

#ifdef ENABLE_GTK_RENDERER
const char kMozcPanelSectionName[] = "panel";
#endif  // ENABLE_GTK_RENDERER

// Default UI locale
const char kMozcDefaultUILocale[] = "en_US.UTF-8";

// for every 5 minutes, call SyncData
const uint64 kSyncDataInterval = 5 * 60;

const char *kUILocaleEnvNames[] = {
  "LC_ALL",
  "LC_MESSAGES",
  "LANG",
};

string GetMessageLocale() {
  for (size_t i = 0; i < arraysize(kUILocaleEnvNames); ++i) {
    const char *env_ptr = ::getenv(kUILocaleEnvNames[i]);
    if (env_ptr == NULL || env_ptr[0] == '\0') {
      continue;
    }
    return env_ptr;
  }
  return kMozcDefaultUILocale;
}

bool GetString(GVariant *value, string *out_string) {
  if (g_variant_classify(value) != G_VARIANT_CLASS_STRING) {
    return false;
  }
  *out_string = static_cast<const char *>(g_variant_get_string(value, NULL));
  return true;
}

bool GetBoolean(GVariant *value, bool *out_boolean) {
  if (g_variant_classify(value) != G_VARIANT_CLASS_BOOLEAN) {
    return false;
  }
  *out_boolean = (g_variant_get_boolean(value) != FALSE);
  return true;
}

struct IBusMozcEngineClass {
  IBusEngineClass parent;
};

struct IBusMozcEngine {
  IBusEngine parent;
  mozc::ibus::MozcEngine *engine;
};

IBusEngineClass *g_parent_class = NULL;

GObject *MozcEngineClassConstructor(
    GType type,
    guint n_construct_properties,
    GObjectConstructParam *construct_properties) {
  return G_OBJECT_CLASS(g_parent_class)->constructor(type,
                                                     n_construct_properties,
                                                     construct_properties);
}

void MozcEngineClassDestroy(IBusObject *engine) {
  IBUS_OBJECT_CLASS(g_parent_class)->destroy(engine);
}

void MozcEngineClassInit(gpointer klass, gpointer class_data) {
  IBusEngineClass *engine_class = IBUS_ENGINE_CLASS(klass);

  VLOG(2) << "MozcEngineClassInit is called";
  mozc::ibus::EngineRegistrar::Register(
      mozc::Singleton<mozc::ibus::MozcEngine>::get(), engine_class);

  g_parent_class = reinterpret_cast<IBusEngineClass*>(
      g_type_class_peek_parent(klass));

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->constructor = MozcEngineClassConstructor;
  IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS(klass);
  ibus_object_class->destroy = MozcEngineClassDestroy;
}

void MozcEngineInstanceInit(GTypeInstance *instance, gpointer klass) {
  IBusMozcEngine *engine = reinterpret_cast<IBusMozcEngine*>(instance);
  engine->engine = mozc::Singleton<mozc::ibus::MozcEngine>::get();
}

}  // namespace

namespace mozc {
namespace ibus {

namespace {
struct SurroundingTextInfo {
  SurroundingTextInfo()
      : relative_selected_length(0) {}
  int32 relative_selected_length;
  string preceding_text;
  string selection_text;
  string following_text;
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
  // http://ibus.googlecode.com/svn/docs/ibus-1.4/IBusText.html
  // http://developer.gnome.org/gobject/stable/gobject-The-Base-Object-Type.html#gobject-The-Base-Object-Type.description
  IBusText *text = NULL;
  ibus_engine_get_surrounding_text(engine, &text, &cursor_pos,
                                   &anchor_pos);
  const string surrounding_text(ibus_text_get_text(text));

#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
  if (cursor_pos == anchor_pos && selection_monitor != NULL) {
    const SelectionInfo &info = selection_monitor->GetSelectionInfo();
    guint new_anchor_pos = 0;
    if (SurroundingTextUtil::GetAnchorPosFromSelection(
            surrounding_text, info.selected_text,
            cursor_pos, &new_anchor_pos)) {
      anchor_pos = new_anchor_pos;
    }
  }
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR

  if (!SurroundingTextUtil::GetSafeDelta(cursor_pos, anchor_pos,
                                         &info->relative_selected_length)) {
    LOG(ERROR) << "Too long text selection.";
    return false;
  }

  const uint32 selection_start = min(cursor_pos, anchor_pos);
  const uint32 selection_length = abs(info->relative_selected_length);
  info->preceding_text = surrounding_text.substr(0, selection_start);
  Util::SubString(surrounding_text,
                  selection_start,
                  selection_length,
                  &info->selection_text);
  info->following_text = surrounding_text.substr(
      selection_start + selection_length);
  return true;
}

}  // namespace

MozcEngine::MozcEngine()
    : last_sync_time_(Util::GetTime()),
      key_event_handler_(new KeyEventHandler),
      client_(client::ClientFactory::NewClient()),
#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
      selection_monitor_(SelectionMonitorFactory::Create(1024)),
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR
      property_handler_(new PropertyHandler(
          new LocaleBasedMessageTranslator(GetMessageLocale()), client_.get())),
      preedit_handler_(new PreeditHandler()),
#ifdef ENABLE_GTK_RENDERER
      gtk_candidate_window_handler_(new GtkCandidateWindowHandler(
          new renderer::RendererClient())),
#else
      gtk_candidate_window_handler_(NULL),
#endif  // ENABLE_GTK_RENDERER
      ibus_candidate_window_handler_(new IBusCandidateWindowHandler()),
      preedit_method_(config::Config::ROMAN) {
  // Currently client capability is fixed.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  client_->set_client_capability(capability);

#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
  if (selection_monitor_.get() != NULL) {
    selection_monitor_->StartMonitoring();
  }
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR

  // TODO(yusukes): write a unit test to check if the capability is set
  // as expected.
}

MozcEngine::~MozcEngine() {
  SyncData(true);
}

void MozcEngine::CandidateClicked(
    IBusEngine *engine,
    guint index,
    guint button,
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
  ibus_engine_get_surrounding_text(engine, NULL, NULL, NULL);
}

void MozcEngine::FocusIn(IBusEngine *engine) {
  property_handler_->Register(engine);
  UpdatePreeditMethod();
}

void MozcEngine::FocusOut(IBusEngine *engine) {
  GetCandidateWindowHandler(engine)->Hide(engine);
  property_handler_->ResetContentType(engine);

  // Do not call SubmitSession or RevertSession. Preedit string will commit on
  // Focus Out event automatically by ibus_engine_update_preedit_text_with_mode
  // which called in UpdatePreedit method.
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

gboolean MozcEngine::ProcessKeyEvent(
    IBusEngine *engine,
    guint keyval,
    guint keycode,
    guint modifiers) {
  VLOG(2) << "keyval: " << keyval
          << ", keycode: " << keycode
          << ", modifiers: " << modifiers;
  if (property_handler_->IsDisabled()) {
    return FALSE;
  }

  // Send current caret location to mozc_server to manage suggest window
  // position.
  // TODO(nona): Merge SendKey event to reduce IPC cost.
  // TODO(nona): Add a unit test against b/6209562.
  SendCaretLocation(engine->cursor_area.x,
                    engine->cursor_area.y,
                    engine->cursor_area.width,
                    engine->cursor_area.height);

  // TODO(yusukes): use |layout| in IBusEngineDesc if possible.
  const bool layout_is_jp =
      !g_strcmp0(ibus_engine_get_name(engine), "mozc-jp");

  commands::KeyEvent key;
  if (!key_event_handler_->GetKeyEvent(
          keyval, keycode, modifiers, preedit_method_, layout_is_jp, &key)) {
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

void MozcEngine::PropertyHide(IBusEngine *engine,
                              const gchar *property_name) {
  // We can ignore the signal.
}

void MozcEngine::PropertyShow(IBusEngine *engine,
                              const gchar *property_name) {
  // We can ignore the signal.
}

void MozcEngine::Reset(IBusEngine *engine) {
  RevertSession(engine);
}

void MozcEngine::SetCapabilities(IBusEngine *engine,
                                 guint capabilities) {
  // Do nothing.
}

void MozcEngine::SetCursorLocation(IBusEngine *engine,
                                   gint x,
                                   gint y,
                                   gint w,
                                   gint h) {
  // Do nothing
}

void MozcEngine::SetContentType(IBusEngine *engine,
                                guint purpose,
                                guint hints) {
  const bool prev_disabled =
      property_handler_->IsDisabled();
  property_handler_->UpdateContentType(engine);
  if (!prev_disabled && property_handler_->IsDisabled()) {
    // Make sure on-going composition is reverted.
    RevertSession(engine);
  }
}

GType MozcEngine::GetType() {
  static GType type = 0;

  static const GTypeInfo type_info = {
    sizeof(IBusMozcEngineClass),
    NULL,
    NULL,
    MozcEngineClassInit,
    NULL,
    NULL,
    sizeof(IBusMozcEngine),
    0,
    MozcEngineInstanceInit,
  };

  if (type == 0) {
    type = g_type_register_static(IBUS_TYPE_ENGINE,
                                  "IBusMozcEngine",
                                  &type_info,
                                  static_cast<GTypeFlags>(0));
    DCHECK_NE(type, 0) << "g_type_register_static failed";
  }

  return type;
}

// static
void MozcEngine::Disconnected(IBusBus *bus, gpointer user_data) {
  ibus_quit();
}

void MozcEngine::ConfigValueChanged(IBusConfig *config,
                                    const gchar *section,
                                    const gchar *name,
                                    GVariant *value,
                                    gpointer user_data) {
  // This function might be called _before_ MozcEngineClassInit is called if
  // you press the "Configure..." button for Mozc before switching to the Mozc
  // input method.
  MozcEngine *engine = mozc::Singleton<MozcEngine>::get();
  engine->UpdateConfig(section, name, value);
}

#ifdef ENABLE_GTK_RENDERER
void MozcEngine::InitRendererConfig(IBusConfig *config) {
  GVariant *custom_font_value = ibus_config_get_value(config,
                                                      kMozcPanelSectionName,
                                                      "custom_font");
  GVariant *use_custom_font_value = ibus_config_get_value(config,
                                                          kMozcPanelSectionName,
                                                          "use_custom_font");
  bool use_custom_font;
  if (GetBoolean(use_custom_font_value, &use_custom_font)) {
    gtk_candidate_window_handler_->OnIBusUseCustomFontDescriptionChanged(
        use_custom_font);
  } else {
    LOG(ERROR) << "Initialize Failed: "
               << "Cannot get panel:use_custom_font configuration.";
  }
  string font_description;
  if (GetString(custom_font_value, &font_description)) {
    gtk_candidate_window_handler_->OnIBusCustomFontDescriptionChanged(
       font_description);
  } else {
    LOG(ERROR) << "Initialize Failed: "
               << "Cannot get panel:custom_font configuration.";
  }
}
#endif  // ENABLE_GTK_RENDERER

// TODO(mazda): Move the impelementation to an appropriate file.
void MozcEngine::InitConfig(IBusConfig *config) {
#ifdef ENABLE_GTK_RENDERER
  MozcEngine *engine = mozc::Singleton<MozcEngine>::get();
  engine->InitRendererConfig(config);
#endif  // ENABLE_GTK_RENDERER
}

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
  if (output.has_deletion_range() &&
      output.deletion_range().offset() < 0 &&
      output.deletion_range().offset() + output.deletion_range().length() >=
          0) {
    // Nowadays 'ibus_engine_delete_surrounding_text' becomes functional on
    // many of the major applications.  Confirmed that it works on
    // Firefox 10.0, LibreOffice 3.3.4 and GEdit 3.2.3.
    ibus_engine_delete_surrounding_text(
        engine,
        output.deletion_range().offset(), output.deletion_range().length());
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

void MozcEngine::UpdateConfig(const gchar *section,
                              const gchar *name,
                              GVariant *value) {
  if (!section || !name || !value) {
    return;
  }
#ifdef ENABLE_GTK_RENDERER
  // TODO(nona): Introduce ConfigHandler
  if (g_strcmp0(section, kMozcPanelSectionName) == 0) {
    if (g_strcmp0(name, "use_custom_font") == 0) {
      bool use_custom_font;
      if (!GetBoolean(value, &use_custom_font)) {
        LOG(ERROR) << "Cannot get " << section << ":" << name << " value.";
        return;
      }
      gtk_candidate_window_handler_->OnIBusUseCustomFontDescriptionChanged(
          use_custom_font);
    } else if (g_strcmp0(name, "custom_font") == 0) {
      string font_description;
      if (!GetString(value, &font_description)) {
        LOG(ERROR) << "Cannot get " << section << ":" << name << " value.";
        return;
      }
      gtk_candidate_window_handler_->OnIBusCustomFontDescriptionChanged(
          font_description);
    }
  }
#endif  // ENABLE_GTK_RENDERER
}

void MozcEngine::UpdatePreeditMethod() {
  config::Config config;
  if (!client_->GetConfig(&config)) {
    LOG(ERROR) << "GetConfig failed";
    return;
  }
  preedit_method_ = config.has_preedit_method() ?
      config.preedit_method() : config::Config::ROMAN;
}

void MozcEngine::SyncData(bool force) {
  if (client_.get() == NULL) {
    return;
  }

  const uint64 current_time = Util::GetTime();
  if (force ||
      (current_time >= last_sync_time_ &&
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
      // As far as I've tested on Ubuntu 11.10, most of applications which
      // accept 'ibus_engine_delete_surrounding_text' doe not set
      // IBUS_CAP_SURROUNDING_TEXT bit.
      // So we should carefully uncomment the following code.
      // -----
      // if (!(engine->client_capabilities & IBUS_CAP_SURROUNDING_TEXT)) {
      //   return false;
      // }
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
    const int32 offset = surrounding_text_info.relative_selected_length > 0
        ? -surrounding_text_info.relative_selected_length  // forward selection
        : 0;                                               // backward selection
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
#else
  if (!FLAGS_use_mozc_renderer) {
    return ibus_candidate_window_handler_.get();
  }

  if (!(engine->client_capabilities & IBUS_CAP_PREEDIT_TEXT)) {
    return ibus_candidate_window_handler_.get();
  }

  // TODO(nona): integrate with renderer/renderer_client.cc
  const string renderer_path = FileUtil::JoinPath(
      SystemUtil::GetServerDirectory(), "mozc_renderer");
  if (!FileUtil::FileExists(renderer_path)) {
    return ibus_candidate_window_handler_.get();
  }

  // TODO(nona): Check executable bit for renderer.
  return gtk_candidate_window_handler_.get();
#endif  // not ENABLE_GTK_RENDERER
}

void MozcEngine::SendCaretLocation(uint32 x, uint32 y, uint32 width,
                                   uint32 height) {
  commands::Output output;
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::SEND_CARET_LOCATION);
  commands::Rectangle *caret_rectangle = command.mutable_caret_rectangle();
  caret_rectangle->set_x(x);
  caret_rectangle->set_y(y);
  caret_rectangle->set_width(width);
  caret_rectangle->set_height(height);
  client_->SendCommand(command, &output);
}

}  // namespace ibus
}  // namespace mozc
