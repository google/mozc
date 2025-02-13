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
  ImeContext() = default;
  explicit ImeContext(
      std::unique_ptr<engine::EngineConverterInterface> converter);
  explicit ImeContext(const ImeContext &src);

  ImeContext &operator=(const ImeContext &) = delete;

  absl::Time create_time() const { return data_.create_time; }
  void set_create_time(absl::Time create_time) {
    data_.create_time = create_time;
  }

  absl::Time last_command_time() const { return data_.last_command_time; }
  void set_last_command_time(absl::Time last_command_time) {
    data_.last_command_time = last_command_time;
  }

  const composer::Composer &composer() const { return data_.composer; }
  composer::Composer *mutable_composer() { return &data_.composer; }

  const engine::EngineConverterInterface &converter() const {
    return *converter_;
  }
  engine::EngineConverterInterface *mutable_converter() {
    return converter_.get();
  }

  const KeyEventTransformer &key_event_transformer() const {
    return data_.key_event_transformer;
  }

  enum State {
    NONE = 0,
    DIRECT = 1,
    PRECOMPOSITION = 2,
    COMPOSITION = 4,
    CONVERSION = 8,
  };
  State state() const { return data_.state; }
  void set_state(State state) { data_.state = state; }

  void SetRequest(const commands::Request &request);
  const commands::Request &GetRequest() const;

  void SetConfig(const config::Config &config);
  const config::Config &GetConfig() const;

  void SetKeyMapManager(const keymap::KeyMapManager &key_map_manager);
  const keymap::KeyMapManager &GetKeyMapManager() const;

  const commands::Capability &client_capability() const {
    return data_.client_capability;
  }
  commands::Capability *mutable_client_capability() {
    return &data_.client_capability;
  }

  const commands::ApplicationInfo &application_info() const {
    return data_.application_info;
  }
  commands::ApplicationInfo *mutable_application_info() {
    return &data_.application_info;
  }

  // Note that this may not be the latest info: this is likely to be a snapshot
  // of during the precomposition state and may not be updated during
  // composition/conversion state.
  const commands::Context &client_context() const {
    return data_.client_context;
  }
  commands::Context *mutable_client_context() { return &data_.client_context; }

  const commands::Output &output() const { return data_.output; }
  commands::Output *mutable_output() { return &data_.output; }

 private:
  // Separate copyable data and non-copyable data to
  // easily overload copy operator.
  struct CopyableData {
    CopyableData();

    // TODO(team): Actual use of |create_time| is to keep the time when the
    // session holding this instance is created and not the time when this
    // instance is created. We may want to move out |create_time| from
    // ImeContext to Session, or somewhere more appropriate.
    absl::Time create_time;
    absl::Time last_command_time;

    // TODO(team): We want to avoid using raw pointer to share
    // frequently updated object with large footprint.
    // Replace them with copy or std::shared_ptr to prevent dangling pointer.
    const commands::Request *request;
    const config::Config *config;
    const keymap::KeyMapManager *key_map_manager;

    composer::Composer composer;
    KeyEventTransformer key_event_transformer;

    State state;
    commands::Capability client_capability;
    commands::ApplicationInfo application_info;
    commands::Context client_context;

    // Storing the last output consisting of the last result and the
    // last performed command.
    commands::Output output;
  };

  CopyableData data_;

  // converter_ should explicitly be copied via Clone() method.
  std::unique_ptr<engine::EngineConverterInterface> converter_;
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_IME_CONTEXT_H_
