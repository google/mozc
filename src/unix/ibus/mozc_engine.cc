// Copyright 2010, Google Inc.
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
#include "unix/ibus/engine_registrar.h"
#include "unix/ibus/key_translator.h"
#include "unix/ibus/session.h"

namespace {

struct IBusMozcEngineClass {
  IBusEngineClass parent;
};

struct IBusMozcEngine {
  IBusEngine parent;
  mozc::ibus::MozcEngine *engine;
};

mozc::ibus::MozcEngine *g_engine = NULL;
IBusEngineClass *g_parent_class = NULL;

GObject* MozcEngineClassConstructor(
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

  if (!g_engine) {
    g_engine = new mozc::ibus::MozcEngine;
  } else {
    VLOG(1) << "MozcEngine has been already instantiated.";
  }
  mozc::ibus::EngineRegistrar::Register(g_engine, engine_class);

  g_parent_class = reinterpret_cast<IBusEngineClass*>(
      g_type_class_peek_parent(klass));

  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  object_class->constructor = MozcEngineClassConstructor;
  IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS(klass);
  ibus_object_class->destroy = MozcEngineClassDestroy;
}

void MozcEngineInstanceInit(GTypeInstance *instance, gpointer klass) {
  IBusMozcEngine *engine = reinterpret_cast<IBusMozcEngine*>(instance);
  engine->engine = g_engine;
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
      const guint kBackGroundColor = 0xD1EAFF;
      ibus_text_append_attribute(text,
                                 IBUS_ATTR_TYPE_BACKGROUND,
                                 kBackGroundColor,
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
  if (!output.has_candidates()) {
    return output.preedit().cursor();
  }
  if (output.preedit().has_highlighted_position()) {
    return output.preedit().highlighted_position();
  }
  return 0;
}

// Returns an IBusText used for showing the auxiliary text in the candidate
// window.
// Caller must release the returned IBusText object.
IBusText *ComposeAuxiliaryText(const commands::Candidates &candidates) {
  // Max size of candidates is 200 so 128 is sufficient size for the buffer.
  char buf[128];
  const int result = snprintf(buf,
                              128,
                              "%d / %d",
                              candidates.focused_index() + 1,
                              candidates.size());
  DCHECK_GE(result, 0) << "snprintf in ComposeAuxiliaryText failed";
  return ibus_text_new_from_string(buf);
}

MozcEngine::MozcEngine()
    : key_translator_(new KeyTranslator),
      session_(new Session) {
}

MozcEngine::~MozcEngine() {
}

void MozcEngine::CandidateClicked(
    IBusEngine *engine,
    guint index,
    guint button,
    guint state) {
  // TODO(mazda): Implement this.
}

void MozcEngine::CursorDown(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::CursorUp(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::Disable(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::Enable(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::FocusIn(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::FocusOut(IBusEngine *engine) {
  commands::SessionCommand command;
  command.set_type(commands::SessionCommand::REVERT);
  commands::Output output;
  session_->SendCommand(command, &output);
  UpdateAll(engine, output);
}

void MozcEngine::PageDown(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::PageUp(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

gboolean MozcEngine::ProcessKeyEvent(
    IBusEngine *engine,
    guint keyval,
    guint keycode,
    guint modifiers) {
  VLOG(2) << "keyval: " << keyval
          << ", keycode: " << keycode
          << ", modifiers: " << modifiers;

  if (modifiers & IBUS_RELEASE_MASK) {
    return FALSE;
  }

  commands::KeyEvent key;
  if (!key_translator_->Translate(keyval, keycode, modifiers, &key)) {
    LOG(ERROR) << "Translate failed";
    return FALSE;
  }

  VLOG(2) << key.DebugString();

  commands::Output output;
  if (!session_->SendKey(key, &output)) {
    LOG(ERROR) << "SendKey failed";
    return FALSE;
  }

  VLOG(2) << output.DebugString();

  UpdateAll(engine, output);

  bool consumed = output.consumed();
  // TODO(mazda): Check if this code is necessary
  // if (!consumed) {
  //   ibus_engine_forward_key_event(engine, keyval, keycode, modifiers);
  //  }
  return consumed ? TRUE : FALSE;
}

void MozcEngine::PropertyActivate(
    IBusEngine *engine,
    const gchar *property_name,
    guint property_state) {
}

void MozcEngine::PropertyHide(IBusEngine *engine,
                              const gchar *property_name) {
  // TODO(mazda): Implement this.
}

void MozcEngine::PropertyShow(IBusEngine *engine,
                              const gchar *property_name) {
  // TODO(mazda): Implement this.
}

void MozcEngine::Reset(IBusEngine *engine) {
  // TODO(mazda): Implement this.
}

void MozcEngine::SetCapabilities(IBusEngine *engine,
                                 guint capabilities) {
  // TODO(mazda): Implement this.
}

void MozcEngine::SetCursorLocation(IBusEngine *engine,
                                   gint x,
                                   gint y,
                                   gint w,
                                   gint h) {
  // TODO(mazda): Implement this.
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

// The callback function to the "disconnected" signal.
void MozcEngine::Disconnected(IBusBus *bus, gpointer user_data) {
  ibus_quit();
  if (g_engine) {
    delete g_engine;
    g_engine = NULL;
  }
}

bool MozcEngine::UpdateAll(IBusEngine *engine,
                           const commands::Output &output) const {
  UpdateResult(engine, output);
  UpdatePreedit(engine, output);
  UpdateCandidates(engine, output);
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

// TODO(mazda): Replace this with a candidate window for Chrome OS.
bool MozcEngine::UpdateCandidates(IBusEngine *engine,
                                  const commands::Output &output) const {
  if (!output.has_candidates()) {
    ibus_engine_hide_auxiliary_text(engine);
    ibus_engine_hide_lookup_table(engine);
    return true;
  }

  const guint kPageSize = 9;
  const gboolean kRound = TRUE;
  const commands::Candidates &candidates = output.candidates();
  const gboolean cursor_visible = candidates.has_focused_index() ?
      TRUE : FALSE;
  int cursor_pos = 0;
  if (candidates.has_focused_index()) {
    for (int i =0; i < candidates.candidate_size(); ++i) {
      if (candidates.focused_index() == candidates.candidate(i).index()) {
        cursor_pos = i;
      }
    }
  }
  IBusLookupTable *table = ibus_lookup_table_new(kPageSize,
                                                 cursor_pos,
                                                 cursor_visible,
                                                 kRound);

  for (int i = 0; i < candidates.candidate_size(); ++i) {
    IBusText *text =
        ibus_text_new_from_string(candidates.candidate(i).value().c_str());
    ibus_lookup_table_append_candidate(table, text);
    // |text| is released by ibus_engine_update_lookup_table along with |table|.
  }
  ibus_engine_update_lookup_table(engine, table, TRUE);
  // |table| is released by ibus_engine_update_lookup_table.

  if (candidates.has_focused_index()) {
    IBusText *auxiliary_text = ComposeAuxiliaryText(candidates);
    ibus_engine_update_auxiliary_text(engine, auxiliary_text, TRUE);
    // |auxiliary_text| is released by ibus_engine_update_auxiliary_text.
  } else {
    ibus_engine_hide_auxiliary_text(engine);
  }

  return true;
}

}  // namespace ibus
}  // namespace mozc
