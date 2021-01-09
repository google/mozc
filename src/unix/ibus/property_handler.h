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

#ifndef MOZC_UNIX_IBUS_PROPERTY_HANDLER_H_
#define MOZC_UNIX_IBUS_PROPERTY_HANDLER_H_

#include <memory>
#include <vector>

#include "base/port.h"
#include "unix/ibus/ibus_header.h"
#include "unix/ibus/property_handler_interface.h"

namespace mozc {

namespace client {
class ClientInterface;
}  // namespace client
namespace ibus {

class MessageTranslatorInterface;

class PropertyHandler : public PropertyHandlerInterface {
 public:
  // This class takes the ownership of translator, but not client.
  PropertyHandler(MessageTranslatorInterface *translator,
                  client::ClientInterface *client);
  virtual ~PropertyHandler();

  virtual void Register(IBusEngine *engine);
  virtual void ResetContentType(IBusEngine *engine);
  virtual void UpdateContentType(IBusEngine *engine);
  virtual void Update(IBusEngine *engine, const commands::Output &output);
  virtual void ProcessPropertyActivate(IBusEngine *engine,
                                       const gchar *property_name,
                                       guint property_state);
  virtual bool IsActivated() const;
  virtual bool IsDisabled() const;
  virtual commands::CompositionMode GetOriginalCompositionMode() const;

 private:
  void UpdateContentTypeImpl(IBusEngine *engine, bool disabled);
  // Appends composition properties into panel
  void AppendCompositionPropertyToPanel();
  // Appends tool properties into panel
  void AppendToolPropertyToPanel();
  // Appends switch properties into panel
  void UpdateCompositionModeIcon(
      IBusEngine *engine, const commands::CompositionMode new_composition_mode);
  void SetCompositionMode(IBusEngine *engine,
                          commands::CompositionMode composition_mode);

  IBusPropList *prop_root_;
  IBusProperty *prop_composition_mode_;
  IBusProperty *prop_mozc_tool_;
  client::ClientInterface *client_;
  std::unique_ptr<MessageTranslatorInterface> translator_;
  commands::CompositionMode original_composition_mode_;
  bool is_activated_;
  bool is_disabled_;
};

}  // namespace ibus
}  // namespace mozc
#endif  // MOZC_UNIX_IBUS_PROPERTY_HANDLER_H_
