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

#include "unix/ibus/property_handler.h"

#include <string>

#include "base/const.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/system_util.h"
#include "client/client.h"  // For client interface
#include "unix/ibus/message_translator.h"
#include "unix/ibus/mozc_engine_property.h"
#include "unix/ibus/path_util.h"

// On Gnome Shell with IBus 1.5, new property named "symbol" is used to
// represent the mode indicator on the system panel. Note that "symbol" does
// not exist in IBus 1.4.x.
#if IBUS_CHECK_VERSION(1, 5, 0)
#define MOZC_IBUS_HAS_SYMBOL
#endif  // IBus >= 1.5

namespace mozc {
namespace ibus {

namespace {

// A key which associates an IBusProperty object with MozcEngineProperty.
const char kGObjectDataKey[] = "ibus-mozc-aux-data";

// Icon path for MozcTool
const char kMozcToolIconPath[] = "tool.png";

// Returns true if mozc_tool is installed.
bool IsMozcToolAvailable() {
  return FileUtil::FileExists(SystemUtil::GetToolPath());
}

bool GetDisabled(IBusEngine *engine) {
  bool disabled = false;
#if defined(MOZC_ENABLE_IBUS_INPUT_PURPOSE)
  guint purpose = IBUS_INPUT_PURPOSE_FREE_FORM;
  guint hints = IBUS_INPUT_HINT_NONE;
  ibus_engine_get_content_type(engine, &purpose, &hints);
  disabled = (purpose == IBUS_INPUT_PURPOSE_PASSWORD ||
              purpose == IBUS_INPUT_PURPOSE_PIN);
#endif  // MOZC_ENABLE_IBUS_INPUT_PURPOSE
  return disabled;
}

}  // namespace

PropertyHandler::PropertyHandler(
    std::unique_ptr<MessageTranslatorInterface> translator,
    bool is_active_on_launch,
    client::ClientInterface *client)
    : prop_root_(ibus_prop_list_new()),
      prop_composition_mode_(nullptr),
      prop_mozc_tool_(nullptr),
      client_(client),
      translator_(std::move(translator)),
      original_composition_mode_(kMozcEngineInitialCompositionMode),
      is_activated_(is_active_on_launch),
      is_disabled_(false) {
  commands::SessionCommand command;
  if (is_activated_) {
    command.set_type(commands::SessionCommand::TURN_ON_IME);
  } else {
    command.set_type(commands::SessionCommand::TURN_OFF_IME);
  }
  command.set_composition_mode(original_composition_mode_);
  commands::Output output;
  if (!client->SendCommand(command, &output)) {
    LOG(ERROR) << "SendCommand failed";
  }

  AppendCompositionPropertyToPanel();
  AppendToolPropertyToPanel();

  // We have to sink |prop_root_| as well so ibus_engine_register_properties()
  // in FocusIn() does not destruct it.
  g_object_ref_sink(prop_root_);
}

PropertyHandler::~PropertyHandler() {
  if (prop_composition_mode_) {
    // The ref counter will drop to one.
    g_object_unref(prop_composition_mode_);
    prop_composition_mode_ = nullptr;
  }

  if (prop_mozc_tool_) {
    // The ref counter will drop to one.
    g_object_unref(prop_mozc_tool_);
    prop_mozc_tool_ = nullptr;
  }

  if (prop_root_) {
    // Destroy all objects under the root.
    g_object_unref(prop_root_);
    prop_root_ = nullptr;
  }
}

void PropertyHandler::Register(IBusEngine *engine) {
  ibus_engine_register_properties(engine, prop_root_);
  UpdateContentType(engine);
}

// TODO(nona): do not use kMozcEngine*** directory.
void PropertyHandler::AppendCompositionPropertyToPanel() {
  if (kMozcEngineProperties == nullptr || kMozcEnginePropertiesSize == 0) {
    return;
  }

  // |sub_prop_list| is a radio menu which is shown when a button in the
  // language panel (i.e. |prop_composition_mode_| below) is clicked.
  IBusPropList *sub_prop_list = ibus_prop_list_new();

  // Create items for the radio menu.
  const commands::CompositionMode initial_mode =
      is_activated_ ? original_composition_mode_
                    : kMozcEnginePropertyIMEOffState->composition_mode;

  std::string icon_path_for_panel;
  const char *mode_symbol = nullptr;
  for (size_t i = 0; i < kMozcEnginePropertiesSize; ++i) {
    const MozcEngineProperty &entry = kMozcEngineProperties[i];
    IBusText *label = ibus_text_new_from_string(
        translator_->MaybeTranslate(entry.label).c_str());
    IBusPropState state = PROP_STATE_UNCHECKED;
    if (entry.composition_mode == initial_mode) {
      state = PROP_STATE_CHECKED;
      icon_path_for_panel = GetIconPath(entry.icon);
      mode_symbol = entry.label_for_panel;
    }
    IBusProperty *item = ibus_property_new(
        entry.key, PROP_TYPE_RADIO, label, nullptr /* icon */,
        nullptr /* tooltip */, TRUE /* sensitive */, TRUE /* visible */, state,
        nullptr /* sub props */);
    g_object_set_data(G_OBJECT(item), kGObjectDataKey, (gpointer)&entry);
    ibus_prop_list_append(sub_prop_list, item);
    // |sub_prop_list| owns |item| by calling g_object_ref_sink for the |item|.
  }
  DCHECK(!icon_path_for_panel.empty());
  DCHECK(mode_symbol != nullptr);

  const std::string &mode_label =
      translator_->MaybeTranslate("Input Mode") + " (" + mode_symbol + ")";
  IBusText *label = ibus_text_new_from_string(mode_label.c_str());

  // The label of |prop_composition_mode_| is shown in the language panel.
  // Note that the property name "InputMode" is hard-coded in the Gnome shell.
  // Do not change the name. Othewise the Gnome shell fails to recognize that
  // this property indicates Mozc's input mode.
  // See /usr/share/gnome-shell/js/ui/status/keyboard.js for details.
  prop_composition_mode_ = ibus_property_new(
      "InputMode", PROP_TYPE_MENU, label, icon_path_for_panel.c_str(),
      nullptr /* tooltip */, TRUE /* sensitive */, TRUE /* visible */,
      PROP_STATE_UNCHECKED, sub_prop_list);

  // Gnome shell uses symbol property for the mode indicator text icon iff the
  // property name is "InputMode".
#ifdef MOZC_IBUS_HAS_SYMBOL
  IBusText *symbol = ibus_text_new_from_static_string(mode_symbol);
  ibus_property_set_symbol(prop_composition_mode_, symbol);
#endif  // MOZC_IBUS_HAS_SYMBOL

  // Likewise, |prop_composition_mode_| owns |sub_prop_list|. We have to sink
  // |prop_composition_mode_| here so ibus_engine_update_property() call in
  // PropertyActivate() does not destruct the object.
  g_object_ref_sink(prop_composition_mode_);

  ibus_prop_list_append(prop_root_, prop_composition_mode_);
}

void PropertyHandler::UpdateContentTypeImpl(IBusEngine *engine, bool disabled) {
  const bool prev_is_disabled = is_disabled_;
  is_disabled_ = disabled;
  if (prev_is_disabled == is_disabled_) {
    return;
  }
  const auto visible_mode =
      (prev_is_disabled && !is_disabled_ && IsActivated())
          ? original_composition_mode_
          : kMozcEnginePropertyIMEOffState->composition_mode;
  UpdateCompositionModeIcon(engine, visible_mode);
}

void PropertyHandler::ResetContentType(IBusEngine *engine) {
  UpdateContentTypeImpl(engine, false);
}

void PropertyHandler::UpdateContentType(IBusEngine *engine) {
  UpdateContentTypeImpl(engine, GetDisabled(engine));
}

// TODO(nona): do not use kMozcEngine*** directory.
void PropertyHandler::AppendToolPropertyToPanel() {
  if (kMozcEngineToolProperties == nullptr ||
      kMozcEngineToolPropertiesSize == 0 ||
      !IsMozcToolAvailable()) {
    return;
  }

  // |sub_prop_list| is a radio menu which is shown when a button in the
  // language panel (i.e. |prop_composition_mode_| below) is clicked.
  IBusPropList *sub_prop_list = ibus_prop_list_new();

  for (size_t i = 0; i < kMozcEngineToolPropertiesSize; ++i) {
    const MozcEngineToolProperty &entry = kMozcEngineToolProperties[i];
    IBusText *label = ibus_text_new_from_string(
        translator_->MaybeTranslate(entry.label).c_str());
    // TODO(yusukes): It would be better to use entry.icon here?
    IBusProperty *item = ibus_property_new(
        entry.mode, PROP_TYPE_NORMAL, label, nullptr /* icon */,
        nullptr /* tooltip */, TRUE, TRUE, PROP_STATE_UNCHECKED, nullptr);
    g_object_set_data(G_OBJECT(item), kGObjectDataKey, (gpointer)&entry);
    ibus_prop_list_append(sub_prop_list, item);
  }

  IBusText *tool_label =
      ibus_text_new_from_string(translator_->MaybeTranslate("Tools").c_str());
  const std::string icon_path = GetIconPath(kMozcToolIconPath);
  prop_mozc_tool_ = ibus_property_new("MozcTool", PROP_TYPE_MENU, tool_label,
                                      icon_path.c_str(), nullptr /* tooltip */,
                                      TRUE /* sensitive */, TRUE /* visible */,
                                      PROP_STATE_UNCHECKED, sub_prop_list);

  // Likewise, |prop_mozc_tool_| owns |sub_prop_list|. We have to sink
  // |prop_mozc_tool_| here so ibus_engine_update_property() call in
  // PropertyActivate() does not destruct the object.
  g_object_ref_sink(prop_mozc_tool_);

  ibus_prop_list_append(prop_root_, prop_mozc_tool_);
}

void PropertyHandler::Update(IBusEngine *engine,
                             const commands::Output &output) {
  if (IsDisabled()) {
    return;
  }

  if (output.has_status() &&
      (output.status().activated() != is_activated_ ||
       output.status().mode() != original_composition_mode_)) {
    if (output.status().activated()) {
      UpdateCompositionModeIcon(engine, output.status().mode());
    } else {
      DCHECK(kMozcEnginePropertyIMEOffState);
      UpdateCompositionModeIcon(
          engine, kMozcEnginePropertyIMEOffState->composition_mode);
    }
    is_activated_ = output.status().activated();
    original_composition_mode_ = output.status().mode();
  }
}

void PropertyHandler::UpdateCompositionModeIcon(
    IBusEngine *engine, const commands::CompositionMode new_composition_mode) {
  if (prop_composition_mode_ == nullptr) {
    return;
  }

  const MozcEngineProperty *entry = nullptr;
  for (size_t i = 0; i < kMozcEnginePropertiesSize; ++i) {
    if (kMozcEngineProperties[i].composition_mode == new_composition_mode) {
      entry = &(kMozcEngineProperties[i]);
      break;
    }
  }
  DCHECK(entry);

  for (guint prop_index = 0;; ++prop_index) {
    IBusProperty *prop = ibus_prop_list_get(
        ibus_property_get_sub_props(prop_composition_mode_), prop_index);
    if (prop == nullptr) {
      break;
    }
    if (!g_strcmp0(entry->key, ibus_property_get_key(prop))) {
      // Update the language panel.
      ibus_property_set_icon(prop_composition_mode_,
                             GetIconPath(entry->icon).c_str());
      // Update the radio menu item.
      ibus_property_set_state(prop, PROP_STATE_CHECKED);
    } else {
      ibus_property_set_state(prop, PROP_STATE_UNCHECKED);
    }
    // No need to call unref since ibus_prop_list_get does not add ref.
  }

  const char *mode_symbol = entry->label_for_panel;
  // Update the text icon for Gnome shell.
#ifdef MOZC_IBUS_HAS_SYMBOL
  IBusText *symbol = ibus_text_new_from_static_string(mode_symbol);
  ibus_property_set_symbol(prop_composition_mode_, symbol);
#endif  // MOZC_IBUS_HAS_SYMBOL

  const std::string &mode_label =
      translator_->MaybeTranslate("Input Mode") + " (" + mode_symbol + ")";
  IBusText *label = ibus_text_new_from_string(mode_label.c_str());
  ibus_property_set_label(prop_composition_mode_, label);

  ibus_engine_update_property(engine, prop_composition_mode_);
}

void PropertyHandler::SetCompositionMode(
    IBusEngine *engine, commands::CompositionMode composition_mode) {
  commands::SessionCommand command;
  commands::Output output;

  // In the case of Mozc, there are two state values of IME, IMEOn/IMEOff and
  // composition_mode. However in IBus we can only control composition mode, not
  // IMEOn/IMEOff. So we use one composition state as IMEOff and the others as
  // IMEOn. This setting can be configured with setting
  // kMozcEnginePropertyIMEOffState. If kMozcEnginePropertyIMEOffState is
  // nullptr, it means current IME should not be off.
  if (kMozcEnginePropertyIMEOffState && is_activated_ &&
      composition_mode == kMozcEnginePropertyIMEOffState->composition_mode) {
    command.set_type(commands::SessionCommand::TURN_OFF_IME);
    command.set_composition_mode(original_composition_mode_);
    client_->SendCommand(command, &output);
  } else {
    command.set_type(commands::SessionCommand::SWITCH_INPUT_MODE);
    command.set_composition_mode(composition_mode);
    client_->SendCommand(command, &output);
  }
  DCHECK(output.has_status());
  original_composition_mode_ = output.status().mode();
  is_activated_ = output.status().activated();
}

void PropertyHandler::ProcessPropertyActivate(IBusEngine *engine,
                                              const gchar *property_name,
                                              guint property_state) {
  if (IsDisabled()) {
    return;
  }

  if (prop_mozc_tool_) {
    for (guint prop_index = 0;; ++prop_index) {
      IBusProperty *prop = ibus_prop_list_get(
          ibus_property_get_sub_props(prop_mozc_tool_), prop_index);
      if (prop == nullptr) {
        break;
      }
      if (!g_strcmp0(property_name, ibus_property_get_key(prop))) {
        const MozcEngineToolProperty *entry =
            reinterpret_cast<const MozcEngineToolProperty *>(
                g_object_get_data(G_OBJECT(prop), kGObjectDataKey));
        DCHECK(entry->mode);
        if (!client_->LaunchTool(entry->mode, "")) {
          LOG(ERROR) << "cannot launch: " << entry->mode;
        }
        return;
      }
    }
  }

  if (property_state != PROP_STATE_CHECKED) {
    return;
  }

  if (prop_composition_mode_) {
    for (guint prop_index = 0;; ++prop_index) {
      IBusProperty *prop = ibus_prop_list_get(
          ibus_property_get_sub_props(prop_composition_mode_), prop_index);
      if (prop == nullptr) {
        break;
      }
      if (!g_strcmp0(property_name, ibus_property_get_key(prop))) {
        const MozcEngineProperty *entry =
            reinterpret_cast<const MozcEngineProperty *>(
                g_object_get_data(G_OBJECT(prop), kGObjectDataKey));
        SetCompositionMode(engine, entry->composition_mode);
        UpdateCompositionModeIcon(engine, entry->composition_mode);
        break;
      }
    }
  }
}

bool PropertyHandler::IsActivated() const { return is_activated_; }

bool PropertyHandler::IsDisabled() const { return is_disabled_; }

commands::CompositionMode PropertyHandler::GetOriginalCompositionMode() const {
  return original_composition_mode_;
}

}  // namespace ibus
}  // namespace mozc
