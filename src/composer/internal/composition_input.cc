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

#include "composer/internal/composition_input.h"

#include "base/logging.h"
#include "base/util.h"

namespace mozc {

using ProbableKeyEvent = commands::KeyEvent::ProbableKeyEvent;
using ProbableKeyEvents = protobuf::RepeatedPtrField<ProbableKeyEvent>;

namespace composer {

CompositionInput::CompositionInput()
    : has_conversion_(false), is_new_input_(false) {}

CompositionInput::~CompositionInput() = default;

bool CompositionInput::Init(const commands::KeyEvent &key_event,
                            bool use_typing_correction, bool is_new_input) {
  std::string raw;
  if (key_event.has_key_code()) {
    Util::UCS4ToUTF8(key_event.key_code(), &raw);
  } else if (key_event.has_key_string()) {
    raw = key_event.key_string();
  } else {
    LOG(WARNING) << "input is empty";
    return false;
  }
  set_raw(raw);

  if (key_event.has_key_string()) {
    set_conversion(key_event.key_string());
  }
  if (use_typing_correction) {
    set_probable_key_events(key_event.probable_key_event());
  }
  set_is_new_input(is_new_input);
  return true;
}

void CompositionInput::InitFromRaw(const std::string &raw, bool is_new_input) {
  set_raw(raw);
  set_is_new_input(is_new_input);
}

void CompositionInput::InitFromRawAndConv(const std::string &raw,
                                          const std::string &conversion,
                                          bool is_new_input) {
  set_raw(raw);
  set_conversion(conversion);
  set_is_new_input(is_new_input);
}

void CompositionInput::Clear() {
  raw_.clear();
  conversion_.clear();
  has_conversion_ = false;
  probable_key_events_.Clear();
  is_new_input_ = false;
}

bool CompositionInput::Empty() const {
  if (has_conversion()) {
    return raw().empty() && conversion().empty();
  } else {
    return raw().empty();
  }
}

void CompositionInput::CopyFrom(const CompositionInput &input) {
  raw_ = input.raw();
  if (input.has_conversion()) {
    conversion_ = input.conversion();
    has_conversion_ = true;
  } else {
    conversion_.clear();
    has_conversion_ = false;
  }
  probable_key_events_ = input.probable_key_events();
  is_new_input_ = input.is_new_input();
}

const std::string &CompositionInput::raw() const { return raw_; }

std::string *CompositionInput::mutable_raw() { return &raw_; }

void CompositionInput::set_raw(const std::string &raw) { raw_ = raw; }

const std::string &CompositionInput::conversion() const {
  if (has_conversion_) {
    return conversion_;
  } else {
    LOG(WARNING) << "conversion is not set.";
    static const std::string *kEmptyString = new std::string();
    return *kEmptyString;
  }
}

std::string *CompositionInput::mutable_conversion() {
  has_conversion_ = true;
  // If has_conversion_ was false, conversion_ should be empty.
  return &conversion_;
}

void CompositionInput::set_conversion(const std::string &conversion) {
  conversion_ = conversion;
  has_conversion_ = true;
}

bool CompositionInput::has_conversion() const { return has_conversion_; }

bool CompositionInput::is_new_input() const { return is_new_input_; }

const ProbableKeyEvents &CompositionInput::probable_key_events() const {
  return probable_key_events_;
}

void CompositionInput::set_probable_key_events(
    const ProbableKeyEvents &probable_key_events) {
  probable_key_events_ = probable_key_events;
}


void CompositionInput::set_is_new_input(bool is_new_input) {
  is_new_input_ = is_new_input;
}

}  // namespace composer
}  // namespace mozc
