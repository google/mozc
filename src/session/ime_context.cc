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

#include "session/ime_context.h"

#include "absl/log/check.h"
#include "composer/composer.h"
#include "engine/engine_converter_interface.h"
#include "protocol/commands.pb.h"
#include "session/keymap.h"

namespace mozc {
namespace session {

const composer::Composer &ImeContext::composer() const {
  DCHECK(composer_.get());
  return *composer_;
}

composer::Composer *ImeContext::mutable_composer() {
  DCHECK(composer_.get());
  return composer_.get();
}

void ImeContext::SetRequest(const commands::Request &request) {
  request_ = &request;
  converter_->SetRequest(*request_);
  composer_->SetRequest(*request_);
}

const commands::Request &ImeContext::GetRequest() const {
  DCHECK(request_);
  return *request_;
}

void ImeContext::SetConfig(const config::Config &config) {
  config_ = &config;

  DCHECK(converter_.get());
  converter_->SetConfig(*config_);

  DCHECK(composer_.get());
  composer_->SetConfig(*config_);

  key_event_transformer_.ReloadConfig(*config_);
}

const config::Config &ImeContext::GetConfig() const {
  DCHECK(config_);
  return *config_;
}

void ImeContext::SetKeyMapManager(
    const keymap::KeyMapManager &key_map_manager) {
  key_map_manager_ = &key_map_manager;
}

const keymap::KeyMapManager &ImeContext::GetKeyMapManager() const {
  if (key_map_manager_) {
    return *key_map_manager_;
  }
  static const keymap::KeyMapManager *void_key_map_manager =
      new keymap::KeyMapManager();
  return *void_key_map_manager;
}

// static
void ImeContext::CopyContext(const ImeContext &src, ImeContext *dest) {
  DCHECK(dest);

  dest->set_create_time(src.create_time());
  dest->set_last_command_time(src.last_command_time());

  *dest->mutable_composer() = src.composer();
  dest->converter_.reset(src.converter().Clone());
  dest->key_event_transformer_ = src.key_event_transformer_;

  dest->set_state(src.state());

  dest->SetRequest(*src.request_);
  dest->SetConfig(*src.config_);
  dest->SetKeyMapManager(src.GetKeyMapManager());

  *dest->mutable_client_capability() = src.client_capability();
  *dest->mutable_application_info() = src.application_info();
  *dest->mutable_output() = src.output();
}

}  // namespace session
}  // namespace mozc
