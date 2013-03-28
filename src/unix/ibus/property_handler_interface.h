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

#ifndef MOZC_UNIX_IBUS_PROPERTY_HANDLER_INTERFACE_H_
#define MOZC_UNIX_IBUS_PROPERTY_HANDLER_INTERFACE_H_

#include "base/port.h"
// TODO(nona): remove this header.
#include "session/commands.pb.h"  // For CompositionMode

namespace mozc {
namespace ibus {

class PropertyHandlerInterface {
 public:
  PropertyHandlerInterface() {}
  virtual ~PropertyHandlerInterface() {}

  // Registers current properties into engine.
  virtual void Register(IBusEngine *engine) ABSTRACT;

  // Update properties.
  virtual void Update(IBusEngine *engine,
                      const commands::Output &output) ABSTRACT;

  virtual void ProcessPropertyActivate(IBusEngine *engine,
                                       const gchar *property_name,
                                       guint property_state) ABSTRACT;
  // Returns if IME is activated or not.
  virtual bool IsActivated() const ABSTRACT;

  // Returns original composition mode before.
  virtual commands::CompositionMode GetOriginalCompositionMode() const ABSTRACT;
};

}  // namespace ibus
}  // namespace mozc
#endif  // MOZC_UNIX_IBUS_PROPERTY_HANDLER_INTERFACE_H_
