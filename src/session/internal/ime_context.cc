// Copyright 2010-2011, Google Inc.
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

#include "session/internal/ime_context.h"

#include "composer/composer.h"
#include "session/session_converter_interface.h"

namespace mozc {
namespace session {

ImeContext::ImeContext()
    : create_time_(0),
      last_command_time_(0),
      composer_(NULL),
      converter_(NULL),
      state_(NONE) {}
ImeContext::~ImeContext() {}

uint64 ImeContext::create_time() const { return create_time_; }
void ImeContext::set_create_time(uint64 create_time) {
  create_time_ = create_time;
}
uint64 ImeContext::last_command_time() const { return last_command_time_; }
void ImeContext::set_last_command_time(uint64 last_time) {
  last_command_time_ = last_time;
}

const composer::Composer &ImeContext::composer() const {
  DCHECK(composer_.get());
  return *composer_;
}
composer::Composer *ImeContext::mutable_composer() {
  DCHECK(composer_.get());
  return composer_.get();
}
void ImeContext::set_composer(composer::Composer *composer) {
  DCHECK(composer);
  composer_.reset(composer);
}

const SessionConverterInterface &ImeContext::converter() const {
  return *converter_;
}
SessionConverterInterface *ImeContext::mutable_converter() {
  return converter_.get();
}
void ImeContext::set_converter(SessionConverterInterface *converter) {
  converter_.reset(converter);
}

ImeContext::State ImeContext::state() const {
  return state_;
}
void ImeContext::set_state(ImeContext::State state) {
  state_ = state;
}

const TransformTable &ImeContext::transform_table() const {
  return transform_table_;
}
TransformTable *ImeContext::mutable_transform_table() {
  return &transform_table_;
}

config::Config::SessionKeymap ImeContext::keymap() const {
  return keymap_;
}

void ImeContext::set_keymap(config::Config::SessionKeymap keymap) {
  keymap_ = keymap;
}

const commands::Capability &ImeContext::client_capability() const {
  return client_capability_;
}
commands::Capability *ImeContext::mutable_client_capability() {
  return &client_capability_;
}

const commands::ApplicationInfo &ImeContext::application_info() const {
  return application_info_;
}
commands::ApplicationInfo *ImeContext::mutable_application_info() {
  return &application_info_;
}

const commands::Output &ImeContext::output() const {
  return output_;
}
commands::Output *ImeContext::mutable_output() {
  return &output_;
}

// static
void ImeContext::CopyContext(const ImeContext &src, ImeContext *dest) {
  DCHECK(dest);

  dest->mutable_output()->CopyFrom(src.output());
  dest->mutable_converter()->CopyFrom(src.converter());
  dest->set_state(src.state());

  switch (dest->state()) {
    case DIRECT:
    case PRECOMPOSITION:
      // Do nothing more.
      return;

    case COMPOSITION:
      dest->mutable_composer()->CopyFromForSubmission(src.composer());
      return;

    case CONVERSION:
      dest->mutable_composer()->CopyFromForConversion(src.composer());
      return;

    default:
      LOG(ERROR) << "Unknown state: " << dest->state();
  }
}

}  // namespace session
}  // namespace mozc
