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

#ifndef MOZC_SESSION_IME_CONTEXT_H_
#define MOZC_SESSION_IME_CONTEXT_H_

#include <memory>
#include <utility>

#include "absl/time/time.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "engine/engine_converter_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/key_event_transformer.h"
#include "session/keymap.h"

namespace mozc {
namespace session {

class ImeContext final {
 public:
  ImeContext()
      : request_(&commands::Request::default_instance()),
        config_(&config::ConfigHandler::DefaultConfig()),
        key_map_manager_(nullptr) {}

  ImeContext(const ImeContext &) = delete;
  ImeContext &operator=(const ImeContext &) = delete;

  absl::Time create_time() const { return create_time_; }
  void set_create_time(absl::Time create_time) { create_time_ = create_time; }

  absl::Time last_command_time() const { return last_command_time_; }
  void set_last_command_time(absl::Time last_command_time) {
    last_command_time_ = last_command_time;
  }

  // Note that before using getter methods,
  // |composer_| must be set non-null value.
  const composer::Composer &composer() const;
  composer::Composer *mutable_composer();
  void set_composer(std::unique_ptr<composer::Composer> composer) {
    composer_ = std::move(composer);
  }

  const engine::EngineConverterInterface &converter() const {
    return *converter_;
  }
  engine::EngineConverterInterface *mutable_converter() {
    return converter_.get();
  }
  void set_converter(
      std::unique_ptr<engine::EngineConverterInterface> converter) {
    converter_ = std::move(converter);
  }

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

  void SetRequest(const commands::Request *request);
  const commands::Request &GetRequest() const;

  void SetConfig(const config::Config *config);
  const config::Config &GetConfig() const;

  void SetKeyMapManager(const keymap::KeyMapManager *key_map_manager);
  const keymap::KeyMapManager &GetKeyMapManager() const;

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
  absl::Time create_time_ = absl::InfinitePast();
  absl::Time last_command_time_ = absl::InfinitePast();

  std::unique_ptr<composer::Composer> composer_;
  std::unique_ptr<engine::EngineConverterInterface> converter_;
  KeyEventTransformer key_event_transformer_;

  const commands::Request *request_;
  const config::Config *config_;
  const keymap::KeyMapManager *key_map_manager_;

  State state_ = NONE;
  commands::Capability client_capability_;
  commands::ApplicationInfo application_info_;
  commands::Context client_context_;

  // Storing the last output consisting of the last result and the
  // last performed command.
  commands::Output output_;
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_IME_CONTEXT_H_
