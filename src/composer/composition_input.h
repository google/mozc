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

#ifndef MOZC_COMPOSER_COMPOSITION_INPUT_H_
#define MOZC_COMPOSER_COMPOSITION_INPUT_H_

#include <string>
#include <utility>

#include "base/protobuf/repeated_ptr_field.h"
#include "base/strings/assign.h"
#include "composer/table.h"
#include "protocol/commands.pb.h"

namespace mozc {
namespace composer {

class CompositionInput final {
 public:
  using ProbableKeyEvent = commands::KeyEvent::ProbableKeyEvent;
  using ProbableKeyEvents = protobuf::RepeatedPtrField<ProbableKeyEvent>;

  CompositionInput() = default;

  // Copyable and movable.
  CompositionInput(const CompositionInput& x) = default;
  CompositionInput& operator=(const CompositionInput& x) = default;
  CompositionInput(CompositionInput&& x) = default;
  CompositionInput& operator=(CompositionInput&& x) = default;

  // Initializes with KeyEvent.
  // If KeyEvent has a special key (e.g. Henkan),
  // it is used as an input of a command key. (e.g. "{henkan}").
  bool Init(const Table& table, const commands::KeyEvent& key_event,
            bool is_new_input);
  void InitFromRaw(std::string raw, bool is_new_input);
  void InitFromRawAndConv(std::string raw, std::string conversion,
                          bool is_new_input);

  void Clear();
  bool Empty() const;

  const std::string& raw() const { return raw_; }
  void clear_raw() { raw_.clear(); }
  std::string* mutable_raw() { return &raw_; }
  template <typename String>
  void set_raw(String&& raw) {
    strings::Assign(raw_, std::forward<String>(raw));
  }

  const std::string& conversion() const { return conversion_; }
  void clear_conversion() { conversion_.clear(); }
  template <typename String>
  void set_conversion(String&& conversion) {
    strings::Assign(conversion_, std::forward<String>(conversion));
  }

  const ProbableKeyEvents& probable_key_events() const {
    return probable_key_events_;
  }
  void set_probable_key_events(const ProbableKeyEvents& probable_key_events) {
    probable_key_events_ = probable_key_events;
  }

  bool is_new_input() const { return is_new_input_; }
  void set_is_new_input(bool is_new_input) { is_new_input_ = is_new_input; }
  bool is_asis() const { return is_asis_; }

 private:
  std::string raw_;
  std::string conversion_;
  ProbableKeyEvents probable_key_events_;
  bool is_new_input_ = false;
  bool is_asis_ = false;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_COMPOSITION_INPUT_H_
