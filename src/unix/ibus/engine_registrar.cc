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

#include "unix/ibus/engine_registrar.h"

#include "base/logging.h"
#include "unix/ibus/engine_interface.h"

namespace {
mozc::ibus::EngineInterface *g_engine = nullptr;
}

namespace mozc {
namespace ibus {

bool EngineRegistrar::Register(EngineInterface *engine,
                               IBusEngineClass *engine_class) {
  DCHECK(engine) << "engine is nullptr";
  DCHECK(!g_engine) << "engine is already registered";

  g_engine = engine;

  engine_class->cursor_down = CursorDown;
  engine_class->candidate_clicked = CandidateClicked;
  engine_class->cursor_down = CursorDown;
  engine_class->cursor_up = CursorUp;
  engine_class->disable = Disable;
  engine_class->enable = Enable;
  engine_class->focus_in = FocusIn;
  engine_class->focus_out = FocusOut;
  engine_class->page_down = PageDown;
  engine_class->page_up = PageUp;
  engine_class->process_key_event = ProcessKeyEvent;
  engine_class->property_activate = PropertyActivate;
  engine_class->property_hide = PropertyHide;
  engine_class->property_show = PropertyShow;
  engine_class->reset = Reset;
  engine_class->set_capabilities = SetCapabilities;
  engine_class->set_cursor_location = SetCursorLocation;
#if defined(MOZC_ENABLE_IBUS_INPUT_PURPOSE)
  engine_class->set_content_type = SetContentType;
#endif  // MOZC_ENABLE_IBUS_INPUT_PURPOSE
  return true;
}

EngineInterface *EngineRegistrar::Unregister(IBusEngineClass *engine_class) {
  DCHECK(g_engine) << "engine is not registered";

  engine_class->cursor_down = nullptr;
  engine_class->candidate_clicked = nullptr;
  engine_class->cursor_down = nullptr;
  engine_class->cursor_up = nullptr;
  engine_class->disable = nullptr;
  engine_class->enable = nullptr;
  engine_class->focus_in = nullptr;
  engine_class->focus_out = nullptr;
  engine_class->page_down = nullptr;
  engine_class->page_up = nullptr;
  engine_class->process_key_event = nullptr;
  engine_class->property_activate = nullptr;
  engine_class->property_hide = nullptr;
  engine_class->property_show = nullptr;
  engine_class->reset = nullptr;
  engine_class->set_capabilities = nullptr;
  engine_class->set_cursor_location = nullptr;
#if defined(MOZC_ENABLE_IBUS_INPUT_PURPOSE)
  engine_class->set_content_type = nullptr;
#endif  // MOZC_ENABLE_IBUS_INPUT_PURPOSE

  mozc::ibus::EngineInterface *previous = g_engine;
  g_engine = nullptr;
  return previous;
}

void EngineRegistrar::CandidateClicked(IBusEngine *engine, guint index,
                                       guint button, guint state) {
  g_engine->CandidateClicked(engine, index, button, state);
}

void EngineRegistrar::CursorDown(IBusEngine *engine) {
  g_engine->CursorDown(engine);
}

void EngineRegistrar::CursorUp(IBusEngine *engine) {
  g_engine->CursorUp(engine);
}

void EngineRegistrar::Disable(IBusEngine *engine) { g_engine->Disable(engine); }

void EngineRegistrar::Enable(IBusEngine *engine) { g_engine->Enable(engine); }

void EngineRegistrar::FocusIn(IBusEngine *engine) { g_engine->FocusIn(engine); }

void EngineRegistrar::FocusOut(IBusEngine *engine) {
  g_engine->FocusOut(engine);
}

void EngineRegistrar::PageDown(IBusEngine *engine) {
  g_engine->PageDown(engine);
}

void EngineRegistrar::PageUp(IBusEngine *engine) { g_engine->PageUp(engine); }

gboolean EngineRegistrar::ProcessKeyEvent(IBusEngine *engine, guint keyval,
                                          guint keycode, guint state) {
  return g_engine->ProcessKeyEvent(engine, keyval, keycode, state);
}

void EngineRegistrar::PropertyActivate(IBusEngine *engine,
                                       const gchar *property_name,
                                       guint property_state) {
  g_engine->PropertyActivate(engine, property_name, property_state);
}

void EngineRegistrar::PropertyHide(IBusEngine *engine,
                                   const gchar *property_name) {
  g_engine->PropertyHide(engine, property_name);
}

void EngineRegistrar::PropertyShow(IBusEngine *engine,
                                   const gchar *property_name) {
  g_engine->PropertyShow(engine, property_name);
}

void EngineRegistrar::Reset(IBusEngine *engine) { g_engine->Reset(engine); }

void EngineRegistrar::SetCapabilities(IBusEngine *engine, guint capabilities) {
  g_engine->SetCapabilities(engine, capabilities);
}

void EngineRegistrar::SetCursorLocation(IBusEngine *engine, gint x, gint y,
                                        gint w, gint h) {
  g_engine->SetCursorLocation(engine, x, y, w, h);
}

void EngineRegistrar::SetContentType(IBusEngine *engine, guint purpose,
                                     guint hints) {
  g_engine->SetContentType(engine, purpose, hints);
}

}  // namespace ibus
}  // namespace mozc
