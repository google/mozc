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

// ImeContext class contains the whole internal variables representing
// a session.

#ifndef MOZC_SESSION_INTERNAL_IME_CONTEXT_H_
#define MOZC_SESSION_INTERNAL_IME_CONTEXT_H_

#include <cstdint>
#include <memory>

#include "base/port.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/internal/key_event_transformer.h"

namespace mozc {

namespace composer {
class Composer;
}  // namespace composer

namespace session {
class SessionConverterInterface;

class ImeContext {
 public:
  ImeContext();
  virtual ~ImeContext();

  uint64_t create_time() const { return create_time_; }
  void set_create_time(uint64_t create_time) { create_time_ = create_time; }

  uint64_t last_command_time() const { return last_command_time_; }
  void set_last_command_time(uint64_t last_command_time) {
    last_command_time_ = last_command_time;
  }

  // Note that before using getter methods,
  // |composer_| must be set non-null value.
  const composer::Composer &composer() const;
  composer::Composer *mutable_composer();
  void set_composer(composer::Composer *composer);

  const SessionConverterInterface &converter() const;
  SessionConverterInterface *mutable_converter();
  void set_converter(SessionConverterInterface *converter);

  const KeyEventTransformer &key_event_transformer() const {
    return key_event_transformer_;
  }

  enum State {
    NONE = 0,
    DIRECT = 1,
    PRECOMPOSITION = 2,
    COMPOSITION = 4,
    CONVERSION = 8,
  };
  State state() const { return state_; }
  void set_state(State state) { state_ = state; }

  // Returns the current keymap.  This might be temporary and different from
  // the keymap in the config.
  config::Config::SessionKeymap keymap() const { return keymap_; }
  void set_keymap(config::Config::SessionKeymap keymap) { keymap_ = keymap; }

  void SetRequest(const commands::Request *request);
  const commands::Request &GetRequest() const;

  void SetConfig(const config::Config *config);
  const config::Config &GetConfig() const;

  const commands::Capability &client_capability() const {
    return client_capability_;
  }
  commands::Capability *mutable_client_capability() {
    return &client_capability_;
  }

  const commands::ApplicationInfo &application_info() const {
    return application_info_;
  }
  commands::ApplicationInfo *mutable_application_info() {
    return &application_info_;
  }

  // Note that this may not be the latest info: this is likely to be a snapshot
  // of during the precomposition state and may not be updated during
  // composition/conversion state.
  const commands::Context &client_context() const { return client_context_; }
  commands::Context *mutable_client_context() { return &client_context_; }

  const commands::Output &output() const { return output_; }
  commands::Output *mutable_output() { return &output_; }

  // Copy |source| context to |destination| context.
  // TODO(hsumita): Renames it as CopyFrom and make it non-static to keep
  // consistency with other classes.
  static void CopyContext(const ImeContext &src, ImeContext *dest);

 private:
  // TODO(team): Actual use of |create_time_| is to keep the time when the
  // session holding this instance is created and not the time when this
  // instance is created. We may want to move out |create_time_| from ImeContext
  // to Session, or somewhere more appropriate.
  uint64_t create_time_;
  uint64_t last_command_time_;

  std::unique_ptr<composer::Composer> composer_;

  std::unique_ptr<SessionConverterInterface> converter_;

  KeyEventTransformer key_event_transformer_;

  State state_;

  const commands::Request *request_;
  const config::Config *config_;

  config::Config::SessionKeymap keymap_;

  commands::Capability client_capability_;

  commands::ApplicationInfo application_info_;

  commands::Context client_context_;

  // Storing the last output consisting of the last result and the
  // last performed command.
  commands::Output output_;

  DISALLOW_COPY_AND_ASSIGN(ImeContext);
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_INTERNAL_IME_CONTEXT_H_
