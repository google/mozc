// Copyright 2010-2013, Google Inc.
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

#ifndef MOZC_UNIX_IBUS_SRC_ENGINE_INTERFACE_H_
#define MOZC_UNIX_IBUS_SRC_ENGINE_INTERFACE_H_

#include "unix/ibus/ibus_header.h"

namespace mozc {
namespace ibus {

// Defines interface functions for signals sent to IBusEngine.
// See http://ibus.googlecode.com/svn/docs/ibus/IBusEngine.html
// for details about the signals which this interface handles.
// A concete class is registered to ibus with mozc::ibus::EngineRegistrar.
class EngineInterface {
 public:
  EngineInterface() {}
  virtual ~EngineInterface() {}

  // The interface function for the "candidate-clicked" signal
  virtual void CandidateClicked(IBusEngine *engine,
                                guint index,
                                guint button,
                                guint state) = 0;

  // The interface function for the "cursor-down" signal
  virtual void CursorDown(IBusEngine *engine) = 0;

  // The interface function for the "cursor-up" signal
  virtual void CursorUp(IBusEngine *engine) = 0;

  // The interface function for the "disable" signal
  virtual void Disable(IBusEngine *engine) = 0;

  // The interface function for the "enable" signal
  virtual void Enable(IBusEngine *engine) = 0;

  // The interface function for the "focus-in" signal
  virtual void FocusIn(IBusEngine *engine) = 0;

  // The interface function for the "focus-out" signal
  virtual void FocusOut(IBusEngine *engine) = 0;

  // The interface function for the "page-down" signal
  virtual void PageDown(IBusEngine *engine) = 0;

  // The interface function for the "page-up" signal
  virtual void PageUp(IBusEngine *engine) = 0;

  // The interface function for the "process-key-event" signal
  virtual gboolean ProcessKeyEvent(IBusEngine *engine,
                                   guint keyval,
                                   guint keycode,
                                   guint state) = 0;

  // The interface function for the "property-activate" signal
  virtual void PropertyActivate(IBusEngine *engine,
                                const gchar *property_name,
                                guint property_state) = 0;

  // The interface function for the "property-hide" signal
  virtual void PropertyHide(IBusEngine *engine,
                            const gchar *property_name) = 0;

  // The interface function for the "property-show" signal
  virtual void PropertyShow(IBusEngine *engine,
                            const gchar *property_name) = 0;

  // The interface function for the "reset" signal
  virtual void Reset(IBusEngine *engine) = 0;

  // The interface function for the "set-capabilities" signal
  virtual void SetCapabilities(IBusEngine *engine,
                               guint capabilities) = 0;

  // The interface function for the "set-cursor-location" signal
  virtual void SetCursorLocation(IBusEngine *engine,
                                 gint x,
                                 gint y,
                                 gint w,
                                 gint h) = 0;
};

}  // namespace ibus
}  // namespace mozc

#endif  //  MOZC_UNIX_IBUS_SRC_ENGINE_INTERFACE_H_
