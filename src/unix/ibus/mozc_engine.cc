// Copyright 2010-2011, Google Inc.
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

#include <ibus.h>
#include <cstdio>
#include <sstream>
#include <string>

#include "base/base.h"
#include "base/const.h"
#include "base/protobuf/descriptor.h"
#include "base/protobuf/message.h"
#include "base/singleton.h"
#include "base/util.h"
#include "session/commands.pb.h"
#include "session/config.pb.h"
#include "session/ime_switch_util.h"
#include "unix/ibus/config_util.h"
#include "unix/ibus/engine_registrar.h"
#include "unix/ibus/key_translator.h"
#include "unix/ibus/mozc_engine_property.h"
#include "unix/ibus/path_util.h"

#ifdef OS_CHROMEOS
// use standalone (in-process) session
#include "unix/ibus/session.h"
#else
// use server/client session
#include "client/session.h"
#endif

namespace {

// A key which associates an IBusProperty object with MozcEngineProperty.
const char kGObjectDataKey[] = "ibus-mozc-aux-data";
// An ID for a candidate which is not associated with a text.
const int32 kBadCandidateId = -1;
// The ibus-memconf section name in which we're interested.
const char kMozcSectionName[] = "engine/Mozc";

// Icon path for MozcTool
const char kMozcToolIconPath[] = "tool.png";

// for every 5 minutes, call SyncData
const uint64 kSyncDataInterval = 5 * 60;

// Backspace key code
const guint kBackSpaceKeyCode = 14;

// Left shift key code
const guint kShiftLeftKeyCode = 42;

#ifdef OS_CHROMEOS
// IBus config names for Mozc.
// This list must match the preference names of ibus-mozc for Chrome OS.
// Check the following file in the chrome repository when you modify this list.
// chrome/browser/chromeos/language_preferences.cc
const gchar* kMozcConfigNames[] = {
  "incognito_mode",
  "use_auto_ime_turn_off",
  "use_date_conversion",
  "use_single_kanji_conversion",
  "use_symbol_conversion",
  "use_number_conversion",
  "use_history_suggest",
  "use_dictionary_suggest",
  "preedit_method",
  "session_keymap",
  "punctuation_method",
  "symbol_method",
  "space_character_form",
  "history_learning_level",
  //  "selection_shortcut",  // currently not supported
  "shift_key_mode_switch",
  "numpad_character_form",
  "suggestions_size",
};
#endif  // OS_CHROMEOS

uint64 GetTime() {
  return static_cast<uint64>(time(NULL));
}

// Returns true if mozc_tool is installed.
bool IsMozcToolAvailable() {
  return mozc::Util::FileExists(
      mozc::Util::JoinPath(mozc::Util::GetServerDirectory(), mozc::kMozcTool));
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

// Returns an IBusText composed from |preedit| to render preedit text.
// Caller must release the returned IBusText object.
IBusText *ComposePreeditText(const commands::Preedit &preedit) {
  string data;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    data.append(preedit.segment(i).value());
  }
  IBusText *text = ibus_text_new_from_string(data.c_str());

  int start = 0;
  int end = 0;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    const commands::Preedit::Segment &segment = preedit.segment(i);
    IBusAttrUnderline attr = IBUS_ATTR_UNDERLINE_ERROR;
    switch (segment.annotation()) {
      case commands::Preedit::Segment::NONE:
        attr = IBUS_ATTR_UNDERLINE_NONE;
        break;
      case commands::Preedit::Segment::UNDERLINE:
        attr = IBUS_ATTR_UNDERLINE_SINGLE;
        break;
      case commands::Preedit::Segment::HIGHLIGHT:
        attr = IBUS_ATTR_UNDERLINE_DOUBLE;
        break;
      default:
        LOG(ERROR) << "unknown annotation:" << segment.annotation();
        break;
    }
    end += segment.value_length();
    ibus_text_append_attribute(text,
                               IBUS_ATTR_TYPE_UNDERLINE,
                               attr,
                               start,
                               end);

    // Many applications show a single underline regardless of using
    // IBUS_ATTR_UNDERLINE_SINGLE or IBUS_ATTR_UNDERLINE_DOUBLE for some
    // reasons. Here we add a background color for the highlighted candidate
    // to make it easiliy distinguishable.
    if (segment.annotation() == commands::Preedit::Segment::HIGHLIGHT) {
      const guint kBackgroundColor = 0xD1EAFF;
      ibus_text_append_attribute(text,
                                 IBUS_ATTR_TYPE_BACKGROUND,
                                 kBackgroundColor,
                                 start,
                                 end);
      // IBUS_ATTR_TYPE_FOREGROUND is necessary to highlight the segment on
      // Firefox.
      const guint kForegroundColor = 0x000000;
      ibus_text_append_attribute(text,
                                 IBUS_ATTR_TYPE_FOREGROUND,
                                 kForegroundColor,
                                 start,
                                 end);
    }
    start = end;
  }

  return text;
}

// Returns a cursor position used for updating preedit.
// NOTE: We do not use a cursor position obtained from Mozc when the candidate
// window is shown since ibus uses the cursor position to locate the candidate
// window and the position obtained from Mozc is not what we expect.
int CursorPos(const commands::Output &output) {
  if (!output.has_preedit()) {
    return 0;
  }
  if (output.preedit().has_highlighted_position()) {
    return output.preedit().highlighted_position();
  }
  return output.preedit().cursor();
}

// Returns an IBusText used for showing the auxiliary text in the candidate
// window. Returns NULL if no text has to be shown. Caller must release the
// returned IBusText object.
IBusText *ComposeAuxiliaryText(const commands::Candidates &candidates) {
  if (!candidates.has_footer()) {
    // We don't have to show the auxiliary text.
    return NULL;
  }
  const commands::Footer &footer = candidates.footer();

  std::string auxiliary_text;
  if (footer.has_label()) {
    // TODO(yusukes,mozc-team): label() is not localized. Currently, it's always
    // written in Japanese (in UTF-8).
    auxiliary_text = footer.label();
  } else if (footer.has_sub_label()) {
    // Windows client shows sub_label() only when label() is not specified. We
    // follow the policy.
    auxiliary_text = footer.sub_label();
  }

  if (footer.has_index_visible() && footer.index_visible() &&
      candidates.has_focused_index()) {
    // Max size of candidates is 200 so 128 is sufficient size for the buffer.
    char index_buf[128] = {0};
    const int result = snprintf(index_buf,
                                sizeof(index_buf) - 1,
                                "%s%d/%d",
                                (auxiliary_text.empty() ? "" : " "),
                                candidates.focused_index() + 1,
                                candidates.size());
    DCHECK_GE(result, 0) << "snprintf in ComposeAuxiliaryText failed";
    auxiliary_text += index_buf;
  }
  return auxiliary_text.empty() ?
      NULL : ibus_text_new_from_string(auxiliary_text.c_str());
}

MozcEngine::MozcEngine()
    : last_sync_time_(GetTime()),
      key_translator_(new KeyTranslator),
#ifdef OS_CHROMEOS
      session_(new ibus::Session),
#else
      session_(new client::Session),
#endif
      prop_root_(NULL),
      prop_composition_mode_(NULL),
      prop_mozc_tool_(NULL),
      current_composition_mode_(kMozcEngineInitialCompositionMode),
      preedit_method_(config::Config::ROMAN),
      ignore_reset_for_deletion_range_workaround_(false) {
  // |sub_prop_list| is a radio menu which is shown when a button in the
  // language panel (i.e. |prop_composition_mode_| below) is clicked.
  IBusPropList *sub_prop_list = ibus_prop_list_new();

  // Create items for the radio menu.
  string icon_path_for_panel;
  for (size_t i = 0; i < kMozcEnginePropertiesSize; ++i) {
    const MozcEngineProperty &entry = kMozcEngineProperties[i];
    IBusText *label = ibus_text_new_from_static_string(entry.label);
    IBusPropState state = PROP_STATE_UNCHECKED;
    if (entry.composition_mode == kMozcEngineInitialCompositionMode) {
      state = PROP_STATE_CHECKED;
      icon_path_for_panel = GetIconPath(entry.icon);
    }
    IBusProperty *item = ibus_property_new(entry.key,
                                           PROP_TYPE_RADIO,
                                           label,
                                           NULL /* icon */,
                                           NULL /* tooltip */,
                                           TRUE /* sensitive */,
                                           TRUE /* visible */,
                                           state,
                                           NULL /* sub props */);
    g_object_set_data(G_OBJECT(item), kGObjectDataKey, (gpointer)&entry);
    ibus_prop_list_append(sub_prop_list, item);
    // |sub_prop_list| owns |item| by calling g_object_ref_sink for the |item|.
  }
  DCHECK(!icon_path_for_panel.empty());

  // The label of |prop_composition_mode_| is shown in the language panel.
  prop_composition_mode_ = ibus_property_new("CompositionMode",
                                             PROP_TYPE_MENU,
                                             NULL /* label */,
                                             icon_path_for_panel.c_str(),
                                             NULL /* tooltip */,
                                             TRUE /* sensitive */,
                                             TRUE /* visible */,
                                             PROP_STATE_UNCHECKED,
                                             sub_prop_list);

  // Likewise, |prop_composition_mode_| owns |sub_prop_list|. We have to sink
  // |prop_composition_mode_| here so ibus_engine_update_property() call in
  // PropertyActivate() does not destruct the object.
  g_object_ref_sink(prop_composition_mode_);

#ifndef OS_CHROMEOS
  if (IsMozcToolAvailable()) {
    // Create items for MozcTool
    sub_prop_list = ibus_prop_list_new();

    for (size_t i = 0; i < kMozcEngineToolPropertiesSize; ++i) {
      const MozcEngineToolProperty &entry = kMozcEngineToolProperties[i];
      IBusText *label = ibus_text_new_from_static_string(entry.label);
      // TODO(yusukes): It would be better to use entry.icon here?
      IBusProperty *item = ibus_property_new(entry.mode,
                                             PROP_TYPE_NORMAL,
                                             label,
                                             NULL /* icon */,
                                             NULL /* tooltip */,
                                             TRUE,
                                             TRUE,
                                             PROP_STATE_UNCHECKED,
                                             NULL);
      g_object_set_data(G_OBJECT(item), kGObjectDataKey, (gpointer)&entry);
      ibus_prop_list_append(sub_prop_list, item);
    }

    const string icon_path = GetIconPath(kMozcToolIconPath);
    prop_mozc_tool_ = ibus_property_new("MozcTool",
                                        PROP_TYPE_MENU,
                                        NULL /* label */,
                                        icon_path.c_str(),
                                        NULL /* tooltip */,
                                        TRUE /* sensitive */,
                                        TRUE /* visible */,
                                        PROP_STATE_UNCHECKED,
                                        sub_prop_list);

    // Likewise, |prop_mozc_tool_| owns |sub_prop_list|. We have to sink
    // |prop_mozc_tool_| here so ibus_engine_update_property() call in
    // PropertyActivate() does not destruct the object.
    g_object_ref_sink(prop_mozc_tool_);
  }
#endif

  // |prop_root_| is used for registering properties in FocusIn().
  prop_root_ = ibus_prop_list_new();
  // We have to sink |prop_root_| as well so ibus_engine_register_properties()
  // in FocusIn() does not destruct it.
  g_object_ref_sink(prop_root_);

  ibus_prop_list_append(prop_root_, prop_composition_mode_);

#ifndef OS_CHROMEOS
  if (IsMozcToolAvailable()) {
    ibus_prop_list_append(prop_root_, prop_mozc_tool_);
  }
#endif

  // We don't enable the DELETE_PRECEDING_TEXT feature on Chrome OS for now
  // since it's highly experimental.
#ifndef OS_CHROMEOS
  // Currently client capability is fixed.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session_->set_client_capability(capability);
  // TODO(yusukes): write a unit test to check if the capability is set
  // as expected.
#endif
}

MozcEngine::~MozcEngine() {
  SyncData(true);
  if (prop_composition_mode_) {
    // The ref counter will drop to one.
    g_object_unref(prop_composition_mode_);
    prop_composition_mode_ = NULL;
  }
  if (prop_mozc_tool_) {
    // The ref counter will drop to one.
    g_object_unref(prop_mozc_tool_);
    prop_mozc_tool_ = NULL;
  }
  if (prop_root_) {
    // Destroy all objects under the root.
    g_object_unref(prop_root_);
    prop_root_ = NULL;
  }
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
  session_->SendCommand(command, &output);
  UpdateAll(engine, output);
}

void MozcEngine::CursorDown(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::CursorUp(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::Disable(IBusEngine *engine) {
  // Stop ignoring "reset" signal.  See ProcessKeyevent().
  ignore_reset_for_deletion_range_workaround_ = false;

  RevertSession(engine);
}

void MozcEngine::Enable(IBusEngine *engine) {
  // Stop ignoring "reset" signal.  See ProcessKeyevent().
  ignore_reset_for_deletion_range_workaround_ = false;

  // Launch mozc_server
  session_->EnsureConnection();
  UpdatePreeditMethod();

  // When ibus-mozc is disabled by the "next input method" hot key, ibus-daemon
  // does not call MozcEngine::Disable(). Call RevertSession() here so the
  // mozc_server could discard a preedit string before the hot key is pressed
  // (crosbug.com/4596).
  RevertSession(engine);
}

void MozcEngine::FocusIn(IBusEngine *engine) {
  ibus_engine_register_properties(engine, prop_root_);
  UpdatePreeditMethod();
}

void MozcEngine::FocusOut(IBusEngine *engine) {
  // Stop ignoring "reset" signal.  See ProcessKeyevent().
  ignore_reset_for_deletion_range_workaround_ = false;

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

gboolean MozcEngine::ProcessKeyEvent(
    IBusEngine *engine,
    guint keyval,
    guint keycode,
    guint modifiers) {
  VLOG(2) << "keyval: " << keyval
          << ", keycode: " << keycode
          << ", modifiers: " << modifiers;

  // Stop ignoring "reset" signal.  See the code of deletion below.
  ignore_reset_for_deletion_range_workaround_ = false;

  // Since IBus for ChromeOS is based on in-process conversion,
  // it is basically ok to call GetConfig() at every keyevent.
  // On the other hands, IBus for Linux is based on out-process (IPC)
  // conversion and user may install large keybinding/roman-kana tables.
  // To reduce IPC overheads, we don't call UpdatePreeditMethod()
  // at every keyevent. When user changes the preedit method via
  // config dialog, the dialog shows a message saying that
  // "preedit method is enabled after new applications.".
  // This behavior is the same as Google Japanese Input for Windows.
#ifdef OS_CHROMEOS
  UpdatePreeditMethod();
#endif

  // TODO(yusukes): use |layout| in IBusEngineDesc if possible.
  const bool layout_is_jp =
      !g_strcmp0(ibus_engine_get_name(engine), "mozc-jp");

  commands::KeyEvent key;
  if (!key_translator_->Translate(
          keyval, keycode, modifiers, preedit_method_, layout_is_jp, &key)) {
    LOG(ERROR) << "Translate failed";
    return FALSE;
  }

  if (!ProcessModifiers((modifiers & IBUS_RELEASE_MASK) != 0,
                        keyval,
                        &key,
                        &currently_pressed_modifiers_,
                        &modifiers_to_be_sent_)) {
    return FALSE;
  }

  VLOG(2) << key.DebugString();
  if ((current_composition_mode_ == commands::DIRECT) &&
      // We DO consume keys that enable Mozc such as Henkan even when in the
      // DIRECT mode.
      !config::ImeSwitchUtil::IsTurnOnInDirectMode(key)) {
    return FALSE;
  }

  commands::Output output;
  if (!session_->SendKey(key, &output)) {
    LOG(ERROR) << "SendKey failed";
    return FALSE;
  }

  VLOG(2) << output.DebugString();

  if (output.has_deletion_range() &&
      output.deletion_range().offset() < 0 &&
      output.deletion_range().offset() + output.deletion_range().length() >=
          0) {
    // Delete some characters of preceding text.  We really want to use
    // ibus_engine_delete_surrounding_text(), but it does not work on many
    // applications, e.g. Chrome, Firefox.  So we currently forward backspaces
    // to application.
    // NOTE: There are some workarounds.  They must be maintained
    // continuosly.
    // NOTE: It cannot delete range of characters not adjacent to the left side
    // of cursor.  If the range is not adjacent to the cursor, ignore it.  If
    // the range contains the cursor, only characters on the left side of cursor
    // are deleted.
    const int length = -output.deletion_range().offset();

    // Some applications, e.g. Chrome, delete preedit character when we forward
    // a backspace.  We can avoid it by hiding preedit before forwarding
    // backspaces.
    ibus_engine_hide_preedit_text(engine);

    // Forward backspaces.
    for (size_t i = 0; i < length; ++i) {
      ibus_engine_forward_key_event(
          engine, IBUS_BackSpace, kBackSpaceKeyCode, 0);
    }

    // This is a workaround.  Some applications (e.g. Chrome, etc.) send
    // strange signals such as "focus_out" and "focus_in" after forwarding
    // Backspace key event.  However, such strange signals are not sent if we
    // forward another key event after backspaces.  Here we choose Shift L,
    // which is one of the most quiet key event.
    ibus_engine_forward_key_event(engine, IBUS_Shift_L, kShiftLeftKeyCode, 0);

    // This is also a workaround.  Some applications (e.g. gedit, etc.) send a
    // "reset" signal after forwarding backspaces.  This signal is sent after
    // this ProcessKeyEvent() returns, and we want to avoid reverting the
    // session.
    // See also Reset(), which calls RevertSession() only if
    // ignore_reset_for_deletion_range_workaround_ is false.  This flag is
    // reset on Disable(), FocusOut(), and ProcessKeyEvent().  Reset() also
    // reset this flag, i.e. this flag does not work more than once.
    ignore_reset_for_deletion_range_workaround_ = true;
  }

  UpdateAll(engine, output);

  const bool consumed = output.consumed();
  return consumed ? TRUE : FALSE;
}

void MozcEngine::SetCompositionMode(
    IBusEngine *engine, commands::CompositionMode composition_mode) {
  commands::SessionCommand command;
  commands::Output output;
  if (composition_mode == commands::DIRECT) {
    // Commit a preedit string.
    command.set_type(commands::SessionCommand::SUBMIT);
    session_->SendCommand(command, &output);
    UpdateAll(engine, output);
  } else {
    command.set_type(commands::SessionCommand::SWITCH_INPUT_MODE);
    command.set_composition_mode(composition_mode);
    session_->SendCommand(command, &output);
  }
  current_composition_mode_ = composition_mode;
}

void MozcEngine::PropertyActivate(IBusEngine *engine,
                                  const gchar *property_name,
                                  guint property_state) {
  size_t i = 0;
  IBusProperty *prop = NULL;

#ifndef OS_CHROMEOS
  if (prop_mozc_tool_) {
    while ((prop = ibus_prop_list_get(prop_mozc_tool_->sub_props, i++))) {
      if (!g_strcmp0(property_name, prop->key)) {
        const MozcEngineToolProperty *entry =
            reinterpret_cast<const MozcEngineToolProperty*>(
                g_object_get_data(G_OBJECT(prop), kGObjectDataKey));
        DCHECK(entry->mode);
        if (!session_->LaunchTool(entry->mode, "")) {
          LOG(ERROR) << "cannot launch: " << entry->mode;
        }
        return;
      }
    }
  }
#endif

  if (property_state != PROP_STATE_CHECKED) {
    return;
  }

  i = 0;
  while ((prop = ibus_prop_list_get(prop_composition_mode_->sub_props, i++))) {
    if (!g_strcmp0(property_name, prop->key)) {
      const MozcEngineProperty *entry =
          reinterpret_cast<const MozcEngineProperty*>(
              g_object_get_data(G_OBJECT(prop), kGObjectDataKey));
      DCHECK(entry);
      if (entry) {
        // Update Mozc state.
        SetCompositionMode(engine, entry->composition_mode);
        // Update the language panel.
        ibus_property_set_icon(prop_composition_mode_,
                               GetIconPath(entry->icon).c_str());
      }
      // Update the radio menu item.
      ibus_property_set_state(prop, PROP_STATE_CHECKED);
    } else {
      ibus_property_set_state(prop, PROP_STATE_UNCHECKED);
    }
    // No need to call unref since ibus_prop_list_get does not add ref.
  }
  ibus_engine_update_property(engine, prop_composition_mode_);
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
  // Ignore this signal if ignore_reset_for_deletion_range_workaround_ is true.
  // This is workaround of deletion of surrounding text.
  // See also ProcessKeyEvent().
  if (ignore_reset_for_deletion_range_workaround_) {
    VLOG(2) << "Reset signal is ignored";
    // We currently do not know the cases that application sends reset signal
    // more than once, so reset the flag here.
    ignore_reset_for_deletion_range_workaround_ = false;
    return;
  }
  RevertSession(engine);
}

void MozcEngine::SetCapabilities(IBusEngine *engine,
                                 guint capabilities) {
  // TODO(mazda,yusukes): For now, there's nothing to do here. If mozc starts to
  // support the reconversion feature, we should checkIBUS_CAP_SURROUNDING_TEXT
  // here to see if the current client can provide surrounding text information.
}

void MozcEngine::SetCursorLocation(IBusEngine *engine,
                                   gint x,
                                   gint y,
                                   gint w,
                                   gint h) {
  // We can ignore the signal.
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

#ifdef OS_CHROMEOS
#if IBUS_CHECK_VERSION(1, 3, 99)
// For IBus 1.4.
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
#else
// For IBus 1.2 and 1.3.
void MozcEngine::ConfigValueChanged(IBusConfig *config,
                                    const gchar *section,
                                    const gchar *name,
                                    GValue *value,
                                    gpointer user_data) {
  MozcEngine *engine = mozc::Singleton<MozcEngine>::get();
  engine->UpdateConfig(section, name, value);
}
#endif

// TODO(mazda): Move the impelementation to an appropriate file.
void MozcEngine::InitConfig(IBusConfig *config) {
  map<string, const char*> name_to_field;
  for (size_t i = 0; i < arraysize(kMozcConfigNames); ++i) {
    // Mozc uses identical names for ibus config names and protobuf config
    // field names.
    name_to_field.insert(make_pair(kMozcConfigNames[i], kMozcConfigNames[i]));
  }
  // Initialize the mozc config with the config loaded from ibus-memconf, which
  // is the primary config storage on Chrome OS.
  ConfigUtil::InitConfig(config, kMozcSectionName, name_to_field);
}

#endif  // OS_CHROMEOS

// static
bool MozcEngine::ProcessModifiers(
    bool is_key_up,
    gint keyval,
    commands::KeyEvent *key,
    set<gint> *currently_pressed_modifiers,
    set<commands::KeyEvent::ModifierKey> *modifiers_to_be_sent) {
  // Manage modifier key event.
  // Modifier key event is sent on key up if non-modifier key has not been
  // pressed since key down of modifier keys and no modifier keys are pressed
  // anymore.
  // Following examples are expected behaviors.
  //
  // E.g.) Usual key is sent on key down.  Modifier keys are not sent if usual
  //       key is sent.
  //    <Event from ibus> <Event to server>
  //     Shift down      | None
  //     "a" down        | Shift+a
  //     "a" up          | None
  //     Shift up        | None
  //
  // E.g.) Modifier key is sent on key up.
  //    <Event from ibus> <Event to server>
  //     Shift down      | None
  //     Shift up        | Shift
  //
  // E.g.) Multiple modifier keys are sent on the last key up.
  //    <Event from ibus> <Event to server>
  //     Shift down      | None
  //     Control down    | None
  //     Shift up        | None
  //     Control up      | Control+Shift
  const bool is_modifier_only =
      !(key->has_key_code() || key->has_special_key());
  if (is_key_up) {
    if (!is_modifier_only) {
      return false;
    }
    currently_pressed_modifiers->erase(keyval);
    if (!currently_pressed_modifiers->empty() ||
        modifiers_to_be_sent->empty()) {
      return false;
    }
    // Modifier key event fires
    key->mutable_modifier_keys()->Clear();
    for (set<commands::KeyEvent::ModifierKey>::const_iterator it =
             modifiers_to_be_sent->begin();
         it != modifiers_to_be_sent->end();
         ++it) {
      key->add_modifier_keys(*it);
    }
    modifiers_to_be_sent->clear();
  } else if (is_modifier_only) {
    if (currently_pressed_modifiers->empty() ||
        !modifiers_to_be_sent->empty()) {
      for (size_t i = 0; i < key->modifier_keys_size(); ++i) {
        modifiers_to_be_sent->insert(key->modifier_keys(i));
      }
    }
    currently_pressed_modifiers->insert(keyval);
    return false;
  }
  // Clear modifier data just in case if |key| has no modifier keys.
  if (key->modifier_keys_size() == 0) {
    currently_pressed_modifiers->clear();
    modifiers_to_be_sent->clear();
  }
  return true;
}

bool MozcEngine::UpdateAll(IBusEngine *engine, const commands::Output &output) {
  UpdateResult(engine, output);
  UpdatePreedit(engine, output);
  UpdateCandidates(engine, output);
  if (output.has_mode()) {
    UpdateCompositionMode(engine, output.mode());
  }
  LaunchTool(output);
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

bool MozcEngine::UpdatePreedit(IBusEngine *engine,
                               const commands::Output &output) const {
  if (!output.has_preedit()) {
    ibus_engine_hide_preedit_text(engine);
    return true;
  }
  IBusText *text = ComposePreeditText(output.preedit());
  ibus_engine_update_preedit_text(engine, text, CursorPos(output), TRUE);
  // |text| is released by ibus_engine_update_preedit_text.
  return true;
}

bool MozcEngine::UpdateCandidates(IBusEngine *engine,
                                  const commands::Output &output) {
  unique_candidate_ids_.clear();
  if (!output.has_candidates()) {
    ibus_engine_hide_auxiliary_text(engine);
    ibus_engine_hide_lookup_table(engine);
    return true;
  }

  const gboolean kRound = TRUE;
  const commands::Candidates &candidates = output.candidates();
  const gboolean cursor_visible = candidates.has_focused_index() ?
      TRUE : FALSE;
  int cursor_pos = 0;
  if (candidates.has_focused_index()) {
    for (int i = 0; i < candidates.candidate_size(); ++i) {
      if (candidates.focused_index() == candidates.candidate(i).index()) {
        cursor_pos = i;
      }
    }
  }
  IBusLookupTable *table = ibus_lookup_table_new(kPageSize,
                                                 cursor_pos,
                                                 cursor_visible,
                                                 kRound);
#if IBUS_CHECK_VERSION(1, 3, 0)
  if (candidates.direction() == commands::Candidates::VERTICAL) {
    ibus_lookup_table_set_orientation(table, IBUS_ORIENTATION_VERTICAL);
  } else {
    ibus_lookup_table_set_orientation(table, IBUS_ORIENTATION_HORIZONTAL);
  }
#endif

  for (int i = 0; i < candidates.candidate_size(); ++i) {
    const commands::Candidates::Candidate &candidate = candidates.candidate(i);

#ifdef OS_CHROMEOS
    const bool has_description = candidate.has_annotation() &&
                                 candidate.annotation().has_description();
    IBusText *text = NULL;
    if (has_description) {
      // |kDelimiter| divides a candidate and an annotation.
      static const char kDelimiter[] = " ";

      // Append an annotation to a candidate word. Both are separated
      // by |kDelimiter|.
      text = ibus_text_new_from_string(
                 (candidate.value() + kDelimiter +
                  candidate.annotation().description()).c_str());

      // The candidate window on Chrome OS will know
      // the start index of an annotation by specific number (e.g. 0x888888).
      // TODO(nhiroki): We should modify the way when iBus supports annotations.
      const guint kAnnotationForegroundColor = 0x888888;

      // Insert an attribute. It incidates annotation's
      // start and end index.
      const guint start = Util::CharsLen(candidate.value() + kDelimiter);
      const guint end = start +
          Util::CharsLen(candidate.annotation().description());
      ibus_text_append_attribute(text,
                                 IBUS_ATTR_TYPE_FOREGROUND,
                                 kAnnotationForegroundColor,
                                 start,
                                 end);
    } else {
      text = ibus_text_new_from_string(candidate.value().c_str());
    }
#else
    IBusText *text = ibus_text_new_from_string(candidate.value().c_str());
#endif

    ibus_lookup_table_append_candidate(table, text);
    // |text| is released by ibus_engine_update_lookup_table along with |table|.

    const bool has_label = candidate.has_annotation() &&
        candidate.annotation().has_shortcut();
    // Need to append an empty string when the candidate does not have a
    // shortcut. Otherwise the ibus lookup table shows numeric labels.
    // NOTE: Since the candidate window for Chrome OS does not support custom
    // labels, it always shows numeric labels.
    IBusText *label =
        ibus_text_new_from_string(has_label ?
                                  candidate.annotation().shortcut().c_str() :
                                  "");
    ibus_lookup_table_append_label(table, label);
    // |label| is released by ibus_engine_update_lookup_table along with
    // |table|.
  }
  ibus_engine_update_lookup_table(engine, table, TRUE);
  // |table| is released by ibus_engine_update_lookup_table.

  IBusText *auxiliary_text = ComposeAuxiliaryText(candidates);
  if (auxiliary_text) {
    ibus_engine_update_auxiliary_text(engine, auxiliary_text, TRUE);
    // |auxiliary_text| is released by ibus_engine_update_auxiliary_text.
  } else {
    ibus_engine_hide_auxiliary_text(engine);
  }

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

#ifdef OS_CHROMEOS
void MozcEngine::UpdateConfig(const gchar *section,
                              const gchar *name,
#if IBUS_CHECK_VERSION(1, 3, 99)
                              GVariant *value
#else
                              GValue *value
#endif
                              ) {
  if (!section || !name || !value) {
    return;
  }
  if (g_strcmp0(section, kMozcSectionName) != 0) {
    return;
  }

  config::Config mozc_config;
  session_->GetConfig(&mozc_config);
  ConfigUtil::SetFieldForName(name, value, &mozc_config);

  // Update config1.db.
  session_->SetConfig(mozc_config);
  session_->SyncData();  // TODO(yusukes): remove this call?
  VLOG(2) << "Session::SetConfig() is called: " << name;
}
#endif  // OS_CHROMEOS

void MozcEngine::UpdateCompositionMode(
    IBusEngine *engine, const commands::CompositionMode new_composition_mode) {
  if (current_composition_mode_ == new_composition_mode) {
    return;
  }
  for (size_t i = 0; i < kMozcEnginePropertiesSize; ++i) {
    const MozcEngineProperty &entry = kMozcEngineProperties[i];
    if (entry.composition_mode == new_composition_mode) {
      PropertyActivate(engine, entry.key, PROP_STATE_CHECKED);
    }
  }
}

void MozcEngine::UpdatePreeditMethod() {
  config::Config config;
  if (!session_->GetConfig(&config)) {
    LOG(ERROR) << "GetConfig failed";
    return;
  }
  preedit_method_ = config.has_preedit_method() ?
      config.preedit_method() : config::Config::ROMAN;
}

void MozcEngine::SyncData(bool force) {
  if (session_.get() == NULL) {
    return;
  }

  const uint64 current_time = GetTime();
  if (force ||
      (current_time >= last_sync_time_ &&
       current_time - last_sync_time_ >= kSyncDataInterval)) {
    VLOG(1) << "Syncing data";
    session_->SyncData();
    last_sync_time_ = current_time;
  }
}

bool MozcEngine::LaunchTool(const commands::Output &output) const {
  if (!output.has_launch_tool_mode()) {
    return false;
  }

  string mode = "";
  switch (output.launch_tool_mode()) {
    case commands::Output::CONFIG_DIALOG:
      mode = "config_dialog";
      break;
    case commands::Output::DICTIONARY_TOOL:
      mode = "dictionary_tool";
      break;
    case commands::Output::WORD_REGISTER_DIALOG:
      mode = "word_register_dialog";
      break;
    case commands::Output::NO_TOOL:
    default:
      // do nothing
      return false;
      break;
  }

  if (!session_->LaunchTool(mode, "")) {
    VLOG(2) << mode << " Launch Failed";
    return false;
  }

  return true;
}

void MozcEngine::RevertSession(IBusEngine *engine) {
  const commands::CompositionMode original_composition_mode =
      current_composition_mode_;

  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::REVERT);
  commands::Output output;
  if (!session_->SendCommand(command, &output)) {
    LOG(ERROR) << "RevertSession() failed";
    return;
  }
  UpdateAll(engine, output);  // may update |current_composition_mode_|.

  // If the original composition mode is DIRECT, we should resume the setting.
  if (original_composition_mode == commands::DIRECT) {
    UpdateCompositionMode(engine, original_composition_mode);
  }
}

}  // namespace ibus
}  // namespace mozc
