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

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "base/const.h"
#include "base/file_util.h"
#include "base/system_util.h"
#include "client/client.h"  // For client interface
#include "protocol/commands.pb.h"
#include "unix/ibus/ibus_header.h"
#include "unix/ibus/ibus_wrapper.h"
#include "unix/ibus/message_translator.h"
#include "unix/ibus/path_util.h"

namespace mozc {
namespace ibus {

namespace {

// A key for MozcEngineProperty used by g_object_data.
constexpr char kMozcEnginePropertyKey[] = "ibus-mozc-aux-data";

// Icon path for MozcTool
constexpr char kMozcToolIconPath[] = "tool.png";

struct MozcEngineProperty {
  commands::CompositionMode composition_mode;
  const char *key;    // IBus property key for the mode.
  const char *label;  // text for the radio menu (ibus-anthy compatible).
  const char *label_for_panel;  // text for the language panel.
  const char *icon;
};

// The list of properties used in ibus-mozc.
constexpr MozcEngineProperty kMozcEngineProperties[] = {
    {
        commands::DIRECT,
        "InputMode.Direct",
        "Direct input",
        "A",
        "direct.png",
    },
    {
        commands::HIRAGANA,
        "InputMode.Hiragana",
        "Hiragana",
        "あ",
        "hiragana.png",
    },
    {
        commands::FULL_KATAKANA,
        "InputMode.Katakana",
        "Katakana",
        "ア",
        "katakana_full.png",
    },
    {
        commands::HALF_ASCII,
        "InputMode.Latin",
        "Latin",
        "_A",
        "alpha_half.png",
    },
    {
        commands::FULL_ASCII,
        "InputMode.WideLatin",
        "Wide Latin",
        "Ａ",
        "alpha_full.png",
    },
    {
        commands::HALF_KATAKANA,
        "InputMode.HalfWidthKatakana",
        "Half width katakana",
        "_ｱ",
        "katakana_half.png",
    },
};

struct MozcEngineToolProperty {
  const char *key;    // IBus property key for the MozcTool.
  const char *mode;   // command line passed as --mode=
  const char *label;  // text for the menu.
  const char *icon;   // icon
};

constexpr MozcEngineToolProperty kMozcEngineToolProperties[] = {
    {
        "Tool.ConfigDialog",
        "config_dialog",
        "Properties",
        "properties.png",
    },
    {
        "Tool.DictionaryTool",
        "dictionary_tool",
        "Dictionary Tool",
        "dictionary.png",
    },
    {
        "Tool.WordRegisterDialog",
        "word_register_dialog",
        "Add Word",
        "word_register.png",
    },
    {
        "Tool.AboutDialog",
        "about_dialog",
        "About Mozc",
        nullptr,
    },
};

constexpr commands::CompositionMode kImeOffCompositionMode = commands::DIRECT;

// Returns true if mozc_tool is installed.
bool IsMozcToolAvailable() {
  if (absl::Status s = FileUtil::FileExists(SystemUtil::GetToolPath());
      !s.ok()) {
    LOG(ERROR) << s;
    return false;
  }
  return true;
}

bool GetDisabled(IbusEngineWrapper *engine) {
  bool disabled = false;
  uint purpose = IBUS_INPUT_PURPOSE_FREE_FORM;
  uint hints = IBUS_INPUT_HINT_NONE;
  engine->GetContentType(&purpose, &hints);
  disabled = (purpose == IBUS_INPUT_PURPOSE_PASSWORD ||
              purpose == IBUS_INPUT_PURPOSE_PIN);
  return disabled;
}

}  // namespace

PropertyHandler::PropertyHandler(
    std::unique_ptr<MessageTranslatorInterface> translator,
    bool is_active_on_launch, client::ClientInterface *client)
    : prop_root_(),
      prop_composition_mode_(nullptr),
      prop_mozc_tool_(nullptr),
      client_(client),
      translator_(std::move(translator)),
      original_composition_mode_(commands::HIRAGANA),
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
  prop_root_.RefSink();
}

PropertyHandler::~PropertyHandler() {
  // The ref counter will drop to one.
  prop_composition_mode_.Unref();

  // The ref counter will drop to one.
  prop_mozc_tool_.Unref();

  // Destroy all objects under the root.
  prop_root_.Unref();
}

void PropertyHandler::Register(IbusEngineWrapper *engine) {
  engine->RegisterProperties(&prop_root_);
  UpdateContentType(engine);
}

// TODO(nona): do not use kMozcEngine*** directory.
void PropertyHandler::AppendCompositionPropertyToPanel() {
  // |sub_prop_list| is a radio menu which is shown when a button in the
  // language panel (i.e. |prop_composition_mode_| below) is clicked.
  IbusPropListWrapper sub_prop_list;

  // Create items for the radio menu.
  const commands::CompositionMode initial_mode =
      is_activated_ ? original_composition_mode_ : kImeOffCompositionMode;

  std::string icon_path_for_panel;
  const char *mode_symbol = nullptr;
  for (const MozcEngineProperty &entry : kMozcEngineProperties) {
    const std::string label = translator_->MaybeTranslate(entry.label);
    IBusPropState state = PROP_STATE_UNCHECKED;
    if (entry.composition_mode == initial_mode) {
      state = PROP_STATE_CHECKED;
      icon_path_for_panel = GetIconPath(entry.icon);
      mode_symbol = entry.label_for_panel;
    }
    IbusPropertyWrapper item(entry.key, PROP_TYPE_RADIO, label, "" /* icon */,
                             state, nullptr /* sub props */);
    item.SetData(kMozcEnginePropertyKey, entry);
    // |sub_prop_list| owns |item|.
    sub_prop_list.Append(&item);
  }
  DCHECK(!icon_path_for_panel.empty());
  DCHECK(mode_symbol != nullptr);

  const std::string mode_label =
      translator_->MaybeTranslate("Input Mode") + " (" + mode_symbol + ")";

  // The label of |prop_composition_mode_| is shown in the language panel.
  // Note that the property name "InputMode" is hard-coded in the Gnome shell.
  // Do not change the name. Otherwise the Gnome shell fails to recognize that
  // this property indicates Mozc's input mode.
  // See /usr/share/gnome-shell/js/ui/status/keyboard.js for details.
  prop_composition_mode_.Initialize("InputMode", PROP_TYPE_MENU, mode_label,
                                    icon_path_for_panel, PROP_STATE_UNCHECKED,
                                    sub_prop_list.GetPropList());

  // Gnome shell uses symbol property for the mode indicator text icon iff the
  // property name is "InputMode".
  prop_composition_mode_.SetSymbol(mode_symbol);

  // Likewise, |prop_composition_mode_| owns |sub_prop_list|. We have to sink
  // |prop_composition_mode_| here so ibus_engine_update_property() call in
  // PropertyActivate() does not destruct the object.
  prop_composition_mode_.RefSink();

  prop_root_.Append(&prop_composition_mode_);
}

void PropertyHandler::UpdateContentTypeImpl(IbusEngineWrapper *engine,
                                            bool disabled) {
  const bool prev_is_disabled = is_disabled_;
  is_disabled_ = disabled;
  if (prev_is_disabled == is_disabled_) {
    return;
  }
  const auto visible_mode = (prev_is_disabled && !is_disabled_ && IsActivated())
                                ? original_composition_mode_
                                : kImeOffCompositionMode;
  UpdateCompositionModeIcon(engine, visible_mode);
}

void PropertyHandler::ResetContentType(IbusEngineWrapper *engine) {
  UpdateContentTypeImpl(engine, false);
}

void PropertyHandler::UpdateContentType(IbusEngineWrapper *engine) {
  UpdateContentTypeImpl(engine, GetDisabled(engine));
}

// TODO(nona): do not use kMozcEngine*** directory.
void PropertyHandler::AppendToolPropertyToPanel() {
  if (!IsMozcToolAvailable()) {
    return;
  }

  // |sub_prop_list| is a radio menu which is shown when a button in the
  // language panel (i.e. |prop_composition_mode_| below) is clicked.
  IbusPropListWrapper sub_prop_list;

  for (const MozcEngineToolProperty &entry : kMozcEngineToolProperties) {
    const std::string label = translator_->MaybeTranslate(entry.label);
    // TODO(yusukes): It would be better to use entry.icon here?
    IbusPropertyWrapper item(entry.mode, PROP_TYPE_NORMAL, label, "" /* icon */,
                             PROP_STATE_UNCHECKED, nullptr);
    item.SetData(kMozcEnginePropertyKey, entry);
    sub_prop_list.Append(&item);
  }

  const std::string tool_label = translator_->MaybeTranslate("Tools");
  const std::string icon_path = GetIconPath(kMozcToolIconPath);
  prop_mozc_tool_.Initialize("MozcTool", PROP_TYPE_MENU, tool_label, icon_path,
                             PROP_STATE_UNCHECKED, sub_prop_list.GetPropList());

  // Likewise, |prop_mozc_tool_| owns |sub_prop_list|. We have to sink
  // |prop_mozc_tool_| here so ibus_engine_update_property() call in
  // PropertyActivate() does not destruct the object.
  prop_mozc_tool_.RefSink();

  prop_root_.Append(&prop_mozc_tool_);
}

void PropertyHandler::Update(IbusEngineWrapper *engine,
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
      UpdateCompositionModeIcon(engine, kImeOffCompositionMode);
    }
    is_activated_ = output.status().activated();
    original_composition_mode_ = output.status().mode();
  }
}

void PropertyHandler::UpdateCompositionModeIcon(
    IbusEngineWrapper *engine, commands::CompositionMode new_composition_mode) {
  if (!prop_composition_mode_.IsInitialized()) {
    return;
  }

  const MozcEngineProperty *entry = nullptr;
  for (const MozcEngineProperty &property : kMozcEngineProperties) {
    if (property.composition_mode == new_composition_mode) {
      entry = &property;
      break;
    }
  }
  DCHECK(entry);

  for (uint prop_index = 0;; ++prop_index) {
    IbusPropertyWrapper prop = prop_composition_mode_.GetSubProp(prop_index);
    if (!prop.IsInitialized()) {
      break;
    }
    if (entry->key == prop.GetKey()) {
      // Update the language panel.
      prop_composition_mode_.SetIcon(GetIconPath(entry->icon));
      // Update the radio menu item.
      prop.SetState(PROP_STATE_CHECKED);
    } else {
      prop.SetState(PROP_STATE_UNCHECKED);
    }
    engine->UpdateProperty(&prop);
    // No need to call unref since GetSubProp (ibus_prop_list_get) does not add
    // ref.
  }

  const char *mode_symbol = entry->label_for_panel;
  // Update the text icon for Gnome shell.
  prop_composition_mode_.SetSymbol(mode_symbol);

  const std::string mode_label =
      translator_->MaybeTranslate("Input Mode") + " (" + mode_symbol + ")";
  prop_composition_mode_.SetLabel(mode_label);

  engine->UpdateProperty(&prop_composition_mode_);
}

void PropertyHandler::SetCompositionMode(
    commands::CompositionMode composition_mode) {
  commands::SessionCommand command;
  commands::Output output;

  // In the case of Mozc, there are two state values of IME, IMEOn/IMEOff and
  // composition_mode. However in IBus we can only control composition mode, not
  // IMEOn/IMEOff. So we use one composition state as IMEOff and the others as
  // IMEOn. This setting can be configured with setting
  // kMozcEnginePropertyIMEOffState. If kMozcEnginePropertyIMEOffState is
  // nullptr, it means current IME should not be off.
  if (is_activated_ && composition_mode == kImeOffCompositionMode) {
    command.set_type(commands::SessionCommand::TURN_OFF_IME);
    command.set_composition_mode(original_composition_mode_);
    client_->SendCommand(command, &output);
  } else {
    command.set_type(commands::SessionCommand::SWITCH_COMPOSITION_MODE);
    command.set_composition_mode(composition_mode);
    client_->SendCommand(command, &output);
  }
  DCHECK(output.has_status());
  original_composition_mode_ = output.status().mode();
  is_activated_ = output.status().activated();
}

void PropertyHandler::ProcessPropertyActivate(IbusEngineWrapper *engine,
                                              const char *property_name,
                                              uint property_state) {
  if (IsDisabled()) {
    return;
  }

  if (prop_mozc_tool_.IsInitialized()) {
    for (uint prop_index = 0;; ++prop_index) {
      IbusPropertyWrapper prop = prop_mozc_tool_.GetSubProp(prop_index);
      if (!prop.IsInitialized()) {
        break;
      }
      if (prop.GetKey() != property_name) {
        continue;
      }

      const MozcEngineToolProperty *entry =
          prop.GetData<MozcEngineToolProperty>(kMozcEnginePropertyKey);
      if (!entry || !entry->mode) {
        continue;
      }
      if (!client_->LaunchTool(entry->mode, "")) {
        LOG(ERROR) << "cannot launch: " << entry->mode;
      }
      return;
    }
  }

  if (property_state != PROP_STATE_CHECKED) {
    return;
  }

  if (prop_composition_mode_.IsInitialized()) {
    for (uint prop_index = 0;; ++prop_index) {
      IbusPropertyWrapper prop = prop_composition_mode_.GetSubProp(prop_index);
      if (!prop.IsInitialized()) {
        break;
      }
      if (prop.GetKey() != property_name) {
        continue;
      }

      const MozcEngineProperty *entry =
          prop.GetData<MozcEngineProperty>(kMozcEnginePropertyKey);
      if (!entry) {
        continue;
      }
      SetCompositionMode(entry->composition_mode);
      UpdateCompositionModeIcon(engine, entry->composition_mode);
      break;
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
