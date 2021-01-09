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

namespace mozc {
namespace composer {

CompositionInput::CompositionInput()
    : has_conversion_(false), is_new_input_(false), transliterator_(nullptr) {}

CompositionInput::~CompositionInput() {}

void CompositionInput::Clear() {
  raw_.clear();
  conversion_.clear();
  has_conversion_ = false;
  is_new_input_ = false;
  transliterator_ = nullptr;
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
  is_new_input_ = input.is_new_input();
  transliterator_ = input.transliterator();
}

const std::string &CompositionInput::raw() const { return raw_; }

std::string *CompositionInput::mutable_raw() { return &raw_; }

void CompositionInput::set_raw(const std::string &raw) { raw_ = raw; }

const std::string &CompositionInput::conversion() const {
  if (has_conversion_) {
    return conversion_;
  } else {
    LOG(WARNING) << "conversion is not set.";
    static const std::string kEmptyString = "";
    return kEmptyString;
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

void CompositionInput::set_is_new_input(bool is_new_input) {
  is_new_input_ = is_new_input;
}

const TransliteratorInterface *CompositionInput::transliterator() const {
  return transliterator_;
}

void CompositionInput::set_transliterator(const TransliteratorInterface *t12r) {
  transliterator_ = t12r;
}

}  // namespace composer
}  // namespace mozc
