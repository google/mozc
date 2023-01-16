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

#include "unix/ibus/candidate_window_handler.h"

#include <gio/gio.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <variant>

#include "base/logging.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/renderer_interface.h"
#include "renderer/unix/const.h"
#include "unix/ibus/ibus_wrapper.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace ibus {

namespace {

constexpr char kDefaultFont[] = "SansSerif 11";
constexpr char kIBusPanelSchema[] = "org.freedesktop.ibus.panel";
constexpr char kIBusPanelUseCustomFont[] = "use-custom-font";
constexpr char kIBusPanelCustomFont[] = "custom-font";

CandidateWindowHandler::Variant GetVariant(GSettings *settings,
                                           std::string_view key) {
  CandidateWindowHandler::Variant value;
  GVariant *variant = g_settings_get_value(settings, key.data());
  if (g_variant_classify(variant) == G_VARIANT_CLASS_BOOLEAN) {
    value = static_cast<bool>(g_variant_get_boolean(variant));
  } else if (g_variant_classify(variant) == G_VARIANT_CLASS_STRING) {
    value = std::string(g_variant_get_string(variant, nullptr));
  }
  g_variant_unref(variant);
  return value;
}

GSettings *CreateGsettings(const char *schema_name) {
  GSettingsSchemaSource *schema_source = g_settings_schema_source_get_default();
  if (schema_source == nullptr) {
    return nullptr;
  }
  GSettingsSchema *schema =
      g_settings_schema_source_lookup(schema_source, schema_name, TRUE);
  if (schema == nullptr) {
    return nullptr;
  }
  g_settings_schema_unref(schema);
  return g_settings_new(schema_name);
}

void UpdateSettings(GSettings *settings, absl::string_view key,
                    CandidateWindowHandler *handler) {
  CandidateWindowHandler::Variant variant = GetVariant(settings, key);
  handler->OnSettingsUpdated(key, variant);
}

// The callback function to the "changed" signal to GSettings object.
void GSettingsChangedCallback(GSettings *settings, const gchar *key,
                              gpointer user_data) {
  CandidateWindowHandler *handler =
      reinterpret_cast<CandidateWindowHandler *>(user_data);
  UpdateSettings(settings, key, handler);
}

}  // namespace

class GSettingsObserver {
 public:
  explicit GSettingsObserver(CandidateWindowHandler *handler)
      : settings_(CreateGsettings(kIBusPanelSchema)), settings_observer_id_(0) {
    if (settings_ == nullptr) {
      return;
    }
    gpointer ptr = reinterpret_cast<gpointer>(handler);
    settings_observer_id_ = g_signal_connect(
        settings_, "changed", G_CALLBACK(GSettingsChangedCallback), ptr);
    // Emulate state changes to set the initial values to the renderer.
    UpdateSettings(settings_, kIBusPanelUseCustomFont, handler);
    UpdateSettings(settings_, kIBusPanelCustomFont, handler);
  }

  ~GSettingsObserver() {
    if (settings_ == nullptr) {
      return;
    }
    if (settings_observer_id_ != 0) {
      g_signal_handler_disconnect(settings_, settings_observer_id_);
    }
    g_object_unref(settings_);
  }

 private:
  GSettings *settings_;
  gulong settings_observer_id_;
};

CandidateWindowHandler::CandidateWindowHandler(
    renderer::RendererInterface *renderer)
    : renderer_(renderer),
      last_update_output_(new commands::Output()),
      use_custom_font_description_(false) {}

CandidateWindowHandler::~CandidateWindowHandler() {}

bool CandidateWindowHandler::SendUpdateCommand(IbusEngineWrapper *engine,
                                               const commands::Output &output,
                                               bool visibility) {
  using commands::RendererCommand;
  RendererCommand command;

  *command.mutable_output() = output;
  command.set_type(RendererCommand::UPDATE);
  command.set_visible(visibility);
  RendererCommand::ApplicationInfo *appinfo =
      command.mutable_application_info();

  auto *preedit_rectangle = command.mutable_preedit_rectangle();
  const IbusEngineWrapper::Rectangle &cursor_area = engine->GetCursorArea();
  preedit_rectangle->set_left(cursor_area.x);
  preedit_rectangle->set_top(cursor_area.y);
  preedit_rectangle->set_right(cursor_area.x + cursor_area.width);
  preedit_rectangle->set_bottom(cursor_area.y + cursor_area.height);

  // `cursor_area` represents the position of the cursor only, however
  // `preedit_rectangle` should represent the whole area of the preedit.
  // To workaround the gap, `preedit_begin_` stores the cursor position on
  // the beginning of the preedit.
  if (output.preedit().segment_size() == 0) {
    preedit_begin_ = *preedit_rectangle;
  } else if (preedit_begin_.top() == preedit_rectangle->top() &&
             preedit_begin_.bottom() == preedit_rectangle->bottom()) {
    // If the Y coordinates are moved, it means that the preedit is:
    //   1. moved for some reasons, or
    //   2. extended to multiple lines.
    // The workaround is only applied when the Y coordinates are not moved.
    preedit_rectangle->set_left(preedit_begin_.left());
  }

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

void CandidateWindowHandler::Update(IbusEngineWrapper *engine,
                                    const commands::Output &output) {
  *last_update_output_ = output;

  UpdateCursorRect(engine);
}

void CandidateWindowHandler::UpdateCursorRect(IbusEngineWrapper *engine) {
  const bool has_candidates =
      last_update_output_->has_candidates() &&
      last_update_output_->candidates().candidate_size() > 0;
  SendUpdateCommand(engine, *last_update_output_, has_candidates);
}

void CandidateWindowHandler::Hide(IbusEngineWrapper *engine) {
  SendUpdateCommand(engine, *last_update_output_, false);
}

void CandidateWindowHandler::Show(IbusEngineWrapper *engine) {
  SendUpdateCommand(engine, *last_update_output_, true);
}

void CandidateWindowHandler::OnIBusCustomFontDescriptionChanged(
    const std::string &custom_font_description) {
  custom_font_description_.assign(custom_font_description);
}

void CandidateWindowHandler::OnIBusUseCustomFontDescriptionChanged(
    bool use_custom_font_description) {
  use_custom_font_description_ = use_custom_font_description;
}

void CandidateWindowHandler::RegisterGSettingsObserver() {
  settings_observer_ = std::make_unique<GSettingsObserver>(this);
}

std::string CandidateWindowHandler::GetFontDescription() const {
  if (!use_custom_font_description_) {
    // TODO(nona): Load application default font settings.
    return kDefaultFont;
  }
  DCHECK(!custom_font_description_.empty());
  return custom_font_description_;
}

void CandidateWindowHandler::OnSettingsUpdated(absl::string_view key,
                                               const Variant &value) {
  if (key == kIBusPanelUseCustomFont) {
    if (std::holds_alternative<bool>(value)) {
      bool use_custom_font = std::get<bool>(value);
      OnIBusUseCustomFontDescriptionChanged(use_custom_font);
    } else {
      LOG(ERROR) << "Cannot get panel:use_custom_font configuration.";
    }
    return;
  }

  if (key == kIBusPanelCustomFont) {
    if (std::holds_alternative<std::string>(value)) {
      std::string font_description = std::get<std::string>(value);
      OnIBusCustomFontDescriptionChanged(font_description);
    } else {
      LOG(ERROR) << "Cannot get panel:custom_font configuration.";
    }
  }
}

}  // namespace ibus
}  // namespace mozc
