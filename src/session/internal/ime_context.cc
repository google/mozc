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

// ImeContext class contains the whole internal variables representing
// a session.

#include "session/internal/ime_context.h"

#include "base/logging.h"
#include "composer/composer.h"
#include "session/session_converter_interface.h"

namespace mozc {
namespace session {

using commands::Request;

ImeContext::ImeContext()
    : create_time_(0),
      last_command_time_(0),
      composer_(NULL),
      converter_(NULL),
      state_(NONE),
      request_(&Request::default_instance()) {
}
ImeContext::~ImeContext() {}

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

void ImeContext::SetRequest(const commands::Request *request) {
  request_ = request;
  converter_->SetRequest(request_);
  composer_->SetRequest(request_);
}

const commands::Request &ImeContext::GetRequest() const {
  DCHECK(request_);
  return *request_;
}

// static
void ImeContext::CopyContext(const ImeContext &src, ImeContext *dest) {
  DCHECK(dest);

  dest->set_create_time(src.create_time());
  dest->set_last_command_time(src.last_command_time());

  dest->mutable_composer()->CopyFrom(src.composer());
  dest->converter_.reset(src.converter().Clone());

  dest->set_state(src.state());
  dest->set_keymap(src.keymap());

  dest->request_ = src.request_;

  dest->mutable_client_capability()->CopyFrom(src.client_capability());
  dest->mutable_application_info()->CopyFrom(src.application_info());
  dest->mutable_composition_rectangle()->CopyFrom(src.composition_rectangle());
  dest->mutable_caret_rectangle()->CopyFrom(src.caret_rectangle());
  dest->mutable_output()->CopyFrom(src.output());
}

}  // namespace session
}  // namespace mozc
