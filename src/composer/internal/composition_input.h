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

#ifndef MOZC_COMPOSER_INTERNAL_COMPOSITION_INPUT_H_
#define MOZC_COMPOSER_INTERNAL_COMPOSITION_INPUT_H_

#include <string>

#include "base/protobuf/repeated_field.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace composer {

class CompositionInput final {
 public:
  using ProbableKeyEvent = commands::KeyEvent::ProbableKeyEvent;
  using ProbableKeyEvents = protobuf::RepeatedPtrField<ProbableKeyEvent>;

  CompositionInput() = default;

  CompositionInput(const CompositionInput &x) = default;
  CompositionInput &operator=(const CompositionInput &x) = default;

  ~CompositionInput() = default;

  bool Init(const commands::KeyEvent &key_event, bool use_typing_correction,
            bool is_new_input);
  void InitFromRaw(const std::string &raw, bool is_new_input);
  void InitFromRawAndConv(const std::string &raw, const std::string &conversion,
                          bool is_new_input);

  void Clear();
  bool Empty() const;

  const std::string &raw() const { return raw_; }
  void clear_raw() { raw_.clear(); }
  std::string *mutable_raw() { return &raw_; }
  void set_raw(const std::string &raw) { raw_ = raw; }

  const std::string &conversion() const;
  void clear_conversion();
  void set_conversion(const std::string &conversion);

  bool has_conversion() const { return has_conversion_; }

  const ProbableKeyEvents &probable_key_events() const {
    return probable_key_events_;
  }
  void set_probable_key_events(const ProbableKeyEvents &probable_key_events) {
    probable_key_events_ = probable_key_events;
  }

  bool is_new_input() const { return is_new_input_; }
  void set_is_new_input(bool is_new_input) { is_new_input_ = is_new_input; }

 private:
  std::string raw_;
  std::string conversion_;
  ProbableKeyEvents probable_key_events_;
  bool has_conversion_ = false;
  bool is_new_input_ = false;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_COMPOSITION_INPUT_H_
