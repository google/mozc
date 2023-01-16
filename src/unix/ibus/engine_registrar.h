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

#ifndef MOZC_UNIX_IBUS_ENGINE_REGISTRAR_H_
#define MOZC_UNIX_IBUS_ENGINE_REGISTRAR_H_

#include "unix/ibus/engine_interface.h"
#include "unix/ibus/ibus_header.h"

namespace mozc {
namespace ibus {

// Resisters the engine and returns the identifier for this engine class.
GType RegisterEngine(EngineInterface *engine);

// Provides functions to register/unregister interface functions implemented by
// a concrete class of EngineInterface as signal handlers of IBusEngine.
class EngineRegistrar {
 public:
  // Registers signal handlers called to `engine_class`.
  static bool Register(IBusEngineClass *engine_class);

  // Unregisters all signal handlers registered to the ibus framework.
  // Returns an engine interface registered previously.
  static EngineInterface *Unregister(IBusEngineClass *engine_class);

 private:
  EngineRegistrar();
  ~EngineRegistrar();

  static void CandidateClicked(IBusEngine *engine, guint index, guint button,
                               guint state);
  static void CursorDown(IBusEngine *engine);
  static void CursorUp(IBusEngine *engine);
  static void Disable(IBusEngine *engine);
  static void Enable(IBusEngine *engine);
  static void FocusIn(IBusEngine *engine);
  static void FocusOut(IBusEngine *engine);
  static void PageDown(IBusEngine *engine);
  static void PageUp(IBusEngine *engine);
  static gboolean ProcessKeyEvent(IBusEngine *engine, guint keyval,
                                  guint keycode, guint state);
  static void PropertyActivate(IBusEngine *engine, const gchar *property_name,
                               guint property_state);
  static void PropertyHide(IBusEngine *engine, const gchar *property_name);
  static void PropertyShow(IBusEngine *engine, const gchar *property_name);
  static void Reset(IBusEngine *engine);
  static void SetCapabilities(IBusEngine *engine, guint capabilities);
  static void SetCursorLocation(IBusEngine *engine, gint x, gint y, gint w,
                                gint h);
  static void SetContentType(IBusEngine *engine, guint purpose, guint hints);
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_ENGINE_REGISTRAR_H_
