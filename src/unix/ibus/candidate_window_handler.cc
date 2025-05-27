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

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/renderer_interface.h"
#include "unix/ibus/ibus_wrapper.h"

namespace mozc {
namespace ibus {

namespace {

constexpr char kDefaultFont[] = "SansSerif 11";
constexpr char kIBusPanelSchema[] = "org.freedesktop.ibus.panel";
constexpr char kIBusPanelUseCustomFont[] = "use-custom-font";
constexpr char kIBusPanelCustomFont[] = "custom-font";

}  // namespace

class GsettingsObserver {
 public:
  explicit GsettingsObserver(CandidateWindowHandler *handler)
      : settings_(GsettingsWrapper(kIBusPanelSchema)),
        settings_observer_id_(0) {
    if (!settings_.IsInitialized()) {
      return;
    }
    settings_observer_id_ =
        settings_.SignalConnect("changed", OnChanged, handler);

    // Emulate state changes to set the initial values to the renderer.
    handler->OnSettingsUpdated(kIBusPanelUseCustomFont,
                               settings_.GetVariant(kIBusPanelUseCustomFont));
    handler->OnSettingsUpdated(kIBusPanelCustomFont,
                               settings_.GetVariant(kIBusPanelCustomFont));
  }

  ~GsettingsObserver() {
    if (!settings_.IsInitialized()) {
      return;
    }
    if (settings_observer_id_ != 0) {
      settings_.SignalHandlerDisconnect(settings_observer_id_);
    }
    settings_.Unref();
  }

 private:
  // The callback function to the "changed" signal to GSettings object.
  static void OnChanged(GSettings *settings, const char *key, void *user_data) {
    CandidateWindowHandler *handler =
        reinterpret_cast<CandidateWindowHandler *>(user_data);
    handler->OnSettingsUpdated(key, GsettingsWrapper(settings).GetVariant(key));
  }

  GsettingsWrapper settings_;
  gulong settings_observer_id_;
};

CandidateWindowHandler::CandidateWindowHandler(
    std::unique_ptr<renderer::RendererInterface> renderer)
    : renderer_(std::move(renderer)),
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
  // 2. the returned value may exceed uint32_t.
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
      last_update_output_->has_candidate_window() &&
      last_update_output_->candidate_window().candidate_size() > 0;
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
  settings_observer_ = std::make_unique<GsettingsObserver>(this);
}

std::string CandidateWindowHandler::GetFontDescription() const {
  if (!use_custom_font_description_) {
    // TODO(nona): Load application default font settings.
    return kDefaultFont;
  }
  DCHECK(!custom_font_description_.empty());
  return custom_font_description_;
}

void CandidateWindowHandler::OnSettingsUpdated(
    absl::string_view key, const GsettingsWrapper::Variant &value) {
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
