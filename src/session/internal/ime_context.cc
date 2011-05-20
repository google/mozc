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
#include "session/internal/keymap.h"
#include "session/session_converter_interface.h"

namespace mozc {
namespace session {

ImeContext::ImeContext()
    : create_time_(0),
      last_command_time_(0),
      composer_(NULL),
      converter_(NULL),
      keymap_(NULL),
      private_keymap_(NULL),
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
  return *composer_;
}
composer::Composer *ImeContext::mutable_composer() {
  return composer_.get();
}
void ImeContext::set_composer(composer::Composer *composer) {
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

const keymap::KeyMapManager &ImeContext::keymap() const {
  return *keymap_;
}
void ImeContext::set_keymap(const keymap::KeyMapManager *keymap) {
  keymap_ = keymap;
}

const keymap::KeyMapManager &ImeContext::private_keymap() const {
  return *private_keymap_;
}
keymap::KeyMapManager *ImeContext::mutable_private_keymap() {
  return private_keymap_.get();
}
void ImeContext::set_private_keymap(keymap::KeyMapManager *keymap) {
  private_keymap_.reset(keymap);
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

const string &ImeContext::initial_composition() const {
  return initial_composition_;
}
void ImeContext::set_initial_composition(const string &composition) {
  initial_composition_ = composition;
}

// static
void ImeContext::CopyContext(const ImeContext &src, ImeContext *dest) {
  DCHECK(dest);

  // Copy output
  dest->mutable_output()->CopyFrom(src.output());

  // Copy history segments
  vector<string> history;
  src.converter().GetHistorySegments(&history);
  dest->mutable_converter()->SetHistorySegments(history);

  // Copy composition modes
  dest->mutable_composer()->SetInputMode(src.composer().GetInputMode());
  dest->mutable_composer()->SetOutputMode(src.composer().GetOutputMode());

  // Copy state
  dest->set_state(src.state());

  // Copy initial composition in reverse conversion
  dest->set_initial_composition(src.initial_composition());

  string composition;
  switch (dest->state()) {
    case DIRECT:
    case PRECOMPOSITION:
      // Do nothing more.
      return;

    case COMPOSITION:
      // Copy composition
      src.composer().GetStringForSubmission(&composition);
      dest->mutable_composer()->InsertCharacterPreedit(composition);
      return;

    case CONVERSION:
      src.composer().GetQueryForConversion(&composition);
      dest->mutable_composer()->InsertCharacterPreedit(composition);

      // TODO(komatsu): Consider prediction.
      dest->mutable_converter()->Convert(&dest->composer());
      return;

    default:
      LOG(ERROR) << "Unknown state: " << dest->state();
  }
}

}  // namespace session
}  // namespace mozc
