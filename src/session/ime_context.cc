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

#include <memory>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/memory/memory.h"
#include "absl/time/time.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "engine/engine_converter_interface.h"
#include "protocol/commands.pb.h"
#include "session/keymap.h"

namespace mozc {
namespace session {
namespace {

const keymap::KeyMapManager &DefaultKeyMapManager() {
  static const absl::NoDestructor<keymap::KeyMapManager> kDefaultKeyMapManager;
  return *kDefaultKeyMapManager;
}

}  // namespace

ImeContext::CopyableData::CopyableData()
    : create_time(absl::InfinitePast()),
      last_command_time(absl::InfinitePast()),
      request(&commands::Request::default_instance()),
      config(&config::ConfigHandler::DefaultConfig()),
      key_map_manager(&DefaultKeyMapManager()),
      composer(composer::Table::GetSharedDefaultTable(), *request, *config),
      state(NONE) {
  key_event_transformer.ReloadConfig(*config);
}

ImeContext::ImeContext(
    std::unique_ptr<engine::EngineConverterInterface> converter)
    : converter_(std::move(converter)) {}

ImeContext::ImeContext(const ImeContext &src) : data_(src.data_) {
  if (src.converter_) {
    converter_ = absl::WrapUnique(src.converter().Clone());
  }
}

void ImeContext::SetRequest(const commands::Request &request) {
  data_.request = &request;
  if (converter_) {
    converter_->SetRequest(*data_.request);
  }
  data_.composer.SetRequest(*data_.request);
}

const commands::Request &ImeContext::GetRequest() const {
  return *data_.request;
}

void ImeContext::SetConfig(const config::Config &config) {
  data_.config = &config;

  if (converter_) {
    converter_->SetConfig(*data_.config);
  }

  data_.composer.SetConfig(*data_.config);
  data_.key_event_transformer.ReloadConfig(*data_.config);
}

const config::Config &ImeContext::GetConfig() const { return *data_.config; }

void ImeContext::SetKeyMapManager(
    const keymap::KeyMapManager &key_map_manager) {
  data_.key_map_manager = &key_map_manager;
}

const keymap::KeyMapManager &ImeContext::GetKeyMapManager() const {
  return *data_.key_map_manager;
}
}  // namespace session
}  // namespace mozc
