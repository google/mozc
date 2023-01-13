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

#include "client/client.h"
#include "protocol/commands.pb.h"
#include "unix/ibus/ibus_header.h"
#include "unix/ibus/message_translator.h"


namespace mozc {
namespace ibus {

class PropertyHandler {
 public:
  // This class takes the ownership of translator, but not client.
  PropertyHandler(std::unique_ptr<MessageTranslatorInterface> translator,
                  bool is_active_on_launch,
                  client::ClientInterface *client);
  ~PropertyHandler();

  // Registers current properties into engine.
  void Register(IbusEngineWrapper *engine);

  void ResetContentType(IbusEngineWrapper *engine);
  void UpdateContentType(IbusEngineWrapper *engine);

  // Update properties.
  void Update(IbusEngineWrapper *engine, const commands::Output &output);

  void ProcessPropertyActivate(IbusEngineWrapper *engine,
                               const char *property_name, uint property_state);

  // Following two methods represent two aspects of an IME state.
  // * (activated, disabled) == (false, false)
  //     This is the state so-called "IME is off". However, an IME is expected
  //     to monitor keyevents that are assigned to DirectMode. A user should be
  //     able to turn on the IME by using shortcut or GUI menu.
  // * (activated, disabled) == (false, true)
  //     This is a state where an IME is expected to do nothing. A user should
  //     be unable to turn on the IME by using shortcut nor GUI menu. This state
  //     is used mainly on the password field. IME becomes to be "turned-off"
  //     once |disabled| state is flipped to false.
  // * (activated, disabled) == (true, false)
  //     This is the state so-called "IME is on". A user should be able to turn
  //     off the IME by using shortcut or GUI menu.
  // * (activated, disabled) == (true, true)
  //     This is the state where an IME is expected to do nothing. A user should
  //     be unable to turn on the IME by using shortcut nor GUI menu. This state
  //     is used mainly on the password field. IME becomes to be "turned-on"
  //     once |disabled| state is flipped to false.
  bool IsActivated() const;
  bool IsDisabled() const;

  // Returns original composition mode before.
  commands::CompositionMode GetOriginalCompositionMode() const;

 private:
  void UpdateContentTypeImpl(IbusEngineWrapper *engine, bool disabled);
  // Appends composition properties into panel
  void AppendCompositionPropertyToPanel();
  // Appends tool properties into panel
  void AppendToolPropertyToPanel();
  // Appends switch properties into panel
  void UpdateCompositionModeIcon(
      IbusEngineWrapper *engine,
      commands::CompositionMode new_composition_mode);
  void SetCompositionMode(commands::CompositionMode composition_mode);

  IbusPropListWrapper prop_root_;
  IbusPropertyWrapper prop_composition_mode_;
  IbusPropertyWrapper prop_mozc_tool_;
  client::ClientInterface *client_;
  std::unique_ptr<MessageTranslatorInterface> translator_;
  commands::CompositionMode original_composition_mode_;
  bool is_activated_;
  bool is_disabled_;
};

}  // namespace ibus
}  // namespace mozc
#endif  // MOZC_UNIX_IBUS_PROPERTY_HANDLER_H_
