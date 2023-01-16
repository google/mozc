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

#ifndef MOZC_UNIX_IBUS_ENGINE_INTERFACE_H_
#define MOZC_UNIX_IBUS_ENGINE_INTERFACE_H_

#include "unix/ibus/ibus_header.h"
#include "unix/ibus/ibus_wrapper.h"

namespace mozc {
namespace ibus {

// Defines interface functions for signals sent to IBusEngine.
// See http://ibus.googlecode.com/svn/docs/ibus/IBusEngine.html
// for details about the signals which this interface handles.
// A concete class is registered to ibus with mozc::ibus::EngineRegistrar.
class EngineInterface {
 public:
  EngineInterface() = default;
  virtual ~EngineInterface() = default;

  // The interface function for the "candidate-clicked" signal
  virtual void CandidateClicked(IbusEngineWrapper *engine, uint index,
                                uint button, uint state) = 0;

  // The interface function for the "cursor-down" signal
  virtual void CursorDown(IbusEngineWrapper *engine) = 0;

  // The interface function for the "cursor-up" signal
  virtual void CursorUp(IbusEngineWrapper *engine) = 0;

  // The interface function for the "disable" signal
  virtual void Disable(IbusEngineWrapper *engine) = 0;

  // The interface function for the "enable" signal
  virtual void Enable(IbusEngineWrapper *engine) = 0;

  // The interface function for the "focus-in" signal
  virtual void FocusIn(IbusEngineWrapper *engine) = 0;

  // The interface function for the "focus-out" signal
  virtual void FocusOut(IbusEngineWrapper *engine) = 0;

  // The interface function for the "page-down" signal
  virtual void PageDown(IbusEngineWrapper *engine) = 0;

  // The interface function for the "page-up" signal
  virtual void PageUp(IbusEngineWrapper *engine) = 0;

  // The interface function for the "process-key-event" signal
  virtual bool ProcessKeyEvent(IbusEngineWrapper *engine, uint keyval,
                               uint keycode, uint state) = 0;

  // The interface function for the "property-activate" signal
  virtual void PropertyActivate(IbusEngineWrapper *engine,
                                const char *property_name,
                                uint property_state) = 0;

  // The interface function for the "property-hide" signal
  virtual void PropertyHide(IbusEngineWrapper *engine,
                            const char *property_name) = 0;

  // The interface function for the "property-show" signal
  virtual void PropertyShow(IbusEngineWrapper *engine,
                            const char *property_name) = 0;

  // The interface function for the "reset" signal
  virtual void Reset(IbusEngineWrapper *engine) = 0;

  // The interface function for the "set-capabilities" signal
  virtual void SetCapabilities(IbusEngineWrapper *engine,
                               uint capabilities) = 0;

  // The interface function for the "set-cursor-location" signal
  virtual void SetCursorLocation(IbusEngineWrapper *engine, int x, int y, int w,
                                 int h) = 0;

  // The interface function for the "set-content-type" signal
  virtual void SetContentType(IbusEngineWrapper *engine, uint purpose,
                              uint hints) = 0;
};

}  // namespace ibus
}  // namespace mozc

#endif  //  MOZC_UNIX_IBUS_ENGINE_INTERFACE_H_
