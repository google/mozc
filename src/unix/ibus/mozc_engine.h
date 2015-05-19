// Copyright 2010-2012, Google Inc.
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

#include <set>
#include <vector>
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit_prod.h"
#include "unix/ibus/engine_interface.h"

namespace mozc {

namespace client {
class ClientInterface;
}

namespace ibus {

class CandidateWindowHandlerInterface;
class KeyEventHandler;
class LaunchToolTest;
class MessageTranslatorInterface;
class PreeditHandlerInterface;
class PropertyHandlerInterface;
#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
class SelectionMonitorInterface;
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR

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

  // EngineInterface functions
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
  // The callback function to the "disconnected" signal to the bus object.
  static void Disconnected(IBusBus *bus, gpointer user_data);
  // The callback function to the "value-changed" signal to the config object.
  static void ConfigValueChanged(IBusConfig *config,
                                 const gchar *section,
                                 const gchar *name,
                                 GVariant *value,
                                 gpointer user_data);

  // Initializes mozc config.
  static void InitConfig(IBusConfig *config);

#if ENABLE_GTK_RENDERER
  // Initialize RendererConfiguration.
  // TODO(nona): Introduce ConfigHandler.
  void InitRendererConfig(IBusConfig *config);
#endif  // ENABLE_GTK_RENDERER

 private:
  // Updates the preedit text and the candidate window and inserts result
  // based on the content of |output|.
  bool UpdateAll(IBusEngine *engine, const commands::Output &output);
  // Inserts a result text based on the content of |output|.
  bool UpdateResult(IBusEngine *engine, const commands::Output &output) const;
  // Updates |unique_candidate_ids_|.
  bool UpdateCandidateIDMapping(const commands::Output &output);
  // Updates the deletion range message based on the content of |output|.
  bool UpdateDeletionRange(IBusEngine *engine, const commands::Output &output);

  // Updates the callback message based on the content of |output|.
  bool ExecuteCallback(IBusEngine *engine, const commands::Output &output);
  // Updates the configuration.
  void UpdateConfig(
      const gchar *section, const gchar *name, GVariant *gvalue);

  // Launches Mozc tool with appropriate arguments.
  bool LaunchTool(const commands::Output &output) const;

  // Updates internal preedit_method (Roman/Kana) state
  void UpdatePreeditMethod();

  // Calls SyncData command. if |force| is false, SyncData is called
  // at an appropriate timing to reduce IPC calls. if |force| is true,
  // always calls SyncData.
  void SyncData(bool force);

  // Reverts internal state of mozc_server by sending SessionCommand::REVERT IPC
  // message, then hides a preedit string and the candidate window.
  void RevertSession(IBusEngine *engine);

  // Sends current caret location to mozc_server.
  void SendCaretLocation(uint32 x, uint32 y, uint32 width, uint32 height);

  CandidateWindowHandlerInterface *GetCandidateWindowHandler(
      IBusEngine *engine);

  uint64 last_sync_time_;
  scoped_ptr<KeyEventHandler> key_event_handler_;
  scoped_ptr<client::ClientInterface> client_;
#ifdef MOZC_ENABLE_X11_SELECTION_MONITOR
  scoped_ptr<SelectionMonitorInterface> selection_monitor_;
#endif  // MOZC_ENABLE_X11_SELECTION_MONITOR

  scoped_ptr<PropertyHandlerInterface> property_handler_;
  scoped_ptr<PreeditHandlerInterface> preedit_handler_;

  // TODO(nona): Introduce CandidateWindowHandlerManager to avoid direct access.
  scoped_ptr<CandidateWindowHandlerInterface> gtk_candidate_window_handler_;
  scoped_ptr<CandidateWindowHandlerInterface> ibus_candidate_window_handler_;
  vector<IBusProperty *> prop_switch_properties_;
  config::Config::PreeditMethod preedit_method_;

  // Unique IDs of candidates that are currently shown.
  vector<int32> unique_candidate_ids_;

  friend class LaunchToolTest;
  FRIEND_TEST(LaunchToolTest, LaunchToolTest);

  DISALLOW_COPY_AND_ASSIGN(MozcEngine);
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_MOZC_ENGINE_H_
