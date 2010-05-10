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

#ifndef MOZC_UNIX_IBUS_MOZC_ENGINE_H_
#define MOZC_UNIX_IBUS_MOZC_ENGINE_H_

#include <ibus.h>
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "session/commands.pb.h"
#include "unix/ibus/engine_interface.h"

namespace mozc {
namespace ibus {

class KeyTranslator;
class Session;

// Implements EngineInterface and handles signals from IBus daemon.
// This class mainly does the two things:
// - Converting IBus key events to Mozc's key events and passes them to the
//   Mozc's session module and
// - Reflecting the output from the Mozc's session module in IBus UIs such as
//   the preedit, the candidate window and the result text.
class MozcEngine : public EngineInterface {
 public:
  MozcEngine();
  virtual ~MozcEngine();

  void CandidateClicked(IBusEngine *engine,
                        guint index,
                        guint button,
                        guint state);
  void CursorDown(IBusEngine *engine);
  void CursorUp(IBusEngine *engine);
  void Disable(IBusEngine *engine);
  void Enable(IBusEngine *engine);
  void FocusIn(IBusEngine *engine);
  void FocusOut(IBusEngine *engine);
  void PageDown(IBusEngine *engine);
  void PageUp(IBusEngine *engine);
  gboolean ProcessKeyEvent(IBusEngine *engine,
                           guint keyval,
                           guint keycode,
                           guint state);
  void PropertyActivate(IBusEngine *engine,
                        const gchar *property_name,
                        guint property_state);
  void PropertyHide(IBusEngine *engine,
                    const gchar *property_name);
  void PropertyShow(IBusEngine *engine,
                    const gchar *property_name);
  void Reset(IBusEngine *engine);
  void SetCapabilities(IBusEngine *engine,
                       guint capabilities);
  void SetCursorLocation(IBusEngine *engine,
                         gint x,
                         gint y,
                         gint w,
                         gint h);

  // Returns the GType which this class represents.
  static GType GetType();
  // The callback function to the "disconnected" signal.
  static void Disconnected(IBusBus *bus, gpointer user_data);

 private:
  // Updates the preedit text and the candidate window and inserts result
  // based on the content of |output|.
  bool UpdateAll(IBusEngine *engine, const commands::Output &output) const;
  // Inserts a result text based on the content of |output|.
  bool UpdateResult(IBusEngine *engine, const commands::Output &output) const;
  // Updates the preedit text based on the content of |output|.
  bool UpdatePreedit(IBusEngine *engine, const commands::Output &output) const;
  // Updates the candidate window based on the content of |output|.
  bool UpdateCandidates(IBusEngine *engine,
                        const commands::Output &output) const;

  scoped_ptr<KeyTranslator> key_translator_;
  scoped_ptr<Session> session_;

  DISALLOW_COPY_AND_ASSIGN(MozcEngine);
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_MOZC_ENGINE_H_
