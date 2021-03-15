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

#include "unix/ibus/gtk_candidate_window_handler.h"

#include <gio/gio.h>
#include <unistd.h>

#include "base/logging.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/renderer_interface.h"
#include "renderer/unix/const.h"

namespace mozc {
namespace ibus {

namespace {

const char kDefaultFont[] = "SansSerif 11";
const gchar kIBusPanelSchema[] = "org.freedesktop.ibus.panel";
const gchar kIBusPanelUseCustomFont[] = "use-custom-font";
const gchar kIBusPanelCustomFont[] = "custom-font";

bool GetString(GVariant *value, std::string *out_string) {
  if (g_variant_classify(value) != G_VARIANT_CLASS_STRING) {
    return false;
  }
  *out_string = static_cast<const char *>(g_variant_get_string(value, nullptr));
  return true;
}

bool GetBoolean(GVariant *value, bool *out_boolean) {
  if (g_variant_classify(value) != G_VARIANT_CLASS_BOOLEAN) {
    return false;
  }
  *out_boolean = (g_variant_get_boolean(value) != FALSE);
  return true;
}

bool HasScheme(const char *schema_name) {
  GSettingsSchemaSource *schema_source = g_settings_schema_source_get_default();
  if (schema_source == nullptr) {
    return false;
  }
  GSettingsSchema *schema =
      g_settings_schema_source_lookup(schema_source, schema_name, TRUE);
  if (schema == nullptr) {
    return false;
  }
  g_settings_schema_unref(schema);
  return true;
}

GSettings *OpenIBusPanelSettings() {
  if (!HasScheme(kIBusPanelSchema)) {
    return nullptr;
  }
  return g_settings_new(kIBusPanelSchema);
}

// The callback function to the "changed" signal to GSettings object.
void GSettingsChangedCallback(GSettings *settings, const gchar *key,
                              gpointer user_data) {
  GtkCandidateWindowHandler *handler =
      reinterpret_cast<GtkCandidateWindowHandler *>(user_data);
  if (g_strcmp0(key, kIBusPanelUseCustomFont) == 0) {
    GVariant *use_custom_font_value =
        g_settings_get_value(settings, kIBusPanelUseCustomFont);
    bool use_custom_font = false;
    if (GetBoolean(use_custom_font_value, &use_custom_font)) {
      handler->OnIBusUseCustomFontDescriptionChanged(use_custom_font);
    } else {
      LOG(ERROR) << "Cannot get panel:use_custom_font configuration.";
    }
  } else if (g_strcmp0(key, kIBusPanelCustomFont) == 0) {
    GVariant *custom_font_value =
        g_settings_get_value(settings, kIBusPanelCustomFont);
    std::string font_description;
    if (GetString(custom_font_value, &font_description)) {
      handler->OnIBusCustomFontDescriptionChanged(font_description);
    } else {
      LOG(ERROR) << "Cannot get panel:custom_font configuration.";
    }
  }
}

}  // namespace

class GSettingsObserver {
 public:
  explicit GSettingsObserver(GtkCandidateWindowHandler *handler)
      : settings_(OpenIBusPanelSettings()), settings_observer_id_(0) {
    if (settings_ != nullptr) {
      gpointer ptr = reinterpret_cast<gpointer>(handler);
      settings_observer_id_ = g_signal_connect(
          settings_, "changed", G_CALLBACK(GSettingsChangedCallback), ptr);
      // Emulate state changes to set the initial values to the renderer.
      GSettingsChangedCallback(settings_, kIBusPanelUseCustomFont, ptr);
      GSettingsChangedCallback(settings_, kIBusPanelCustomFont, ptr);
    }
  }

  ~GSettingsObserver() {
    if (settings_ != nullptr) {
      if (settings_observer_id_ != 0) {
        g_signal_handler_disconnect(settings_, settings_observer_id_);
      }
      g_object_unref(settings_);
    }
  }

 private:
  GSettings *settings_;
  gulong settings_observer_id_;
};

GtkCandidateWindowHandler::GtkCandidateWindowHandler(
    renderer::RendererInterface *renderer)
    : renderer_(renderer),
      last_update_output_(new commands::Output()),
      use_custom_font_description_(false) {}

GtkCandidateWindowHandler::~GtkCandidateWindowHandler() {}

bool GtkCandidateWindowHandler::SendUpdateCommand(
    IBusEngine *engine, const commands::Output &output, bool visibility) const {
  using commands::RendererCommand;
  RendererCommand command;

  *command.mutable_output() = output;
  command.set_type(RendererCommand::UPDATE);
  command.set_visible(visibility);
  RendererCommand::ApplicationInfo *appinfo =
      command.mutable_application_info();

  auto *preedit_rectangle = command.mutable_preedit_rectangle();
  const auto &cursor_area = engine->cursor_area;
  preedit_rectangle->set_left(cursor_area.x);
  preedit_rectangle->set_top(cursor_area.y);
  preedit_rectangle->set_right(cursor_area.x + cursor_area.width);
  preedit_rectangle->set_bottom(cursor_area.y + cursor_area.height);

  // Set pid
  static_assert(sizeof(::getpid()) <= sizeof(appinfo->process_id()),
                "|appinfo->process_id()| must have sufficient room.");
  appinfo->set_process_id(::getpid());

  // Do not set thread_id returned from ::pthread_self() because:
  // 1. the returned value is valid only in the caller process.
  // 2. the returned value may exceed uint32.
  // TODO(team): Consider to use ::gettid()

  // Set InputFramework type
  appinfo->set_input_framework(RendererCommand::ApplicationInfo::IBus);
  appinfo->set_pango_font_description(GetFontDescription());

  return renderer_->ExecCommand(command);
}

void GtkCandidateWindowHandler::Update(IBusEngine *engine,
                                       const commands::Output &output) {
  *last_update_output_ = output;

  UpdateCursorRect(engine);
}

void GtkCandidateWindowHandler::UpdateCursorRect(IBusEngine *engine) {
  const bool has_candidates =
      last_update_output_->has_candidates() &&
      last_update_output_->candidates().candidate_size() > 0;
  SendUpdateCommand(engine, *last_update_output_, has_candidates);
}

void GtkCandidateWindowHandler::Hide(IBusEngine *engine) {
  SendUpdateCommand(engine, *last_update_output_, false);
}

void GtkCandidateWindowHandler::Show(IBusEngine *engine) {
  SendUpdateCommand(engine, *last_update_output_, true);
}

void GtkCandidateWindowHandler::OnIBusCustomFontDescriptionChanged(
    const std::string &custom_font_description) {
  custom_font_description_.assign(custom_font_description);
}

void GtkCandidateWindowHandler::OnIBusUseCustomFontDescriptionChanged(
    bool use_custom_font_description) {
  use_custom_font_description_ = use_custom_font_description;
}

void GtkCandidateWindowHandler::RegisterGSettingsObserver() {
  settings_observer_.reset(new GSettingsObserver(this));
}

std::string GtkCandidateWindowHandler::GetFontDescription() const {
  if (!use_custom_font_description_) {
    // TODO(nona): Load application default font settings.
    return kDefaultFont;
  }
  DCHECK(!custom_font_description_.empty());
  return custom_font_description_;
}

}  // namespace ibus
}  // namespace mozc
