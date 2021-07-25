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
  CompositionInput();
  ~CompositionInput();

  CompositionInput(const CompositionInput &) = delete;
  CompositionInput &operator=(const CompositionInput &) = delete;

  bool Init(const commands::KeyEvent &key_event, bool use_typing_correction,
            bool is_new_input);
  void InitFromRaw(const std::string &raw, bool is_new_input);
  void InitFromRawAndConv(const std::string &raw, const std::string &conversion,
                          bool is_new_input);

  void Clear();
  bool Empty() const;
  void CopyFrom(const CompositionInput &input);

  const std::string &raw() const;
  std::string *mutable_raw();
  void set_raw(const std::string &raw);

  const std::string &conversion() const;
  std::string *mutable_conversion();
  void set_conversion(const std::string &conversion);
  bool has_conversion() const;

  const protobuf::RepeatedPtrField<commands::KeyEvent::ProbableKeyEvent>
      &probable_key_events() const;
  void set_probable_key_events(
      const protobuf::RepeatedPtrField<commands::KeyEvent::ProbableKeyEvent>
          &probable_key_events);

  bool is_new_input() const;
  void set_is_new_input(bool is_new_input);

 private:
  std::string raw_;
  std::string conversion_;
  bool has_conversion_;
  protobuf::RepeatedPtrField<commands::KeyEvent::ProbableKeyEvent>
      probable_key_events_;

  bool is_new_input_;
};

}  // namespace composer
}  // namespace mozc

#endif  // MOZC_COMPOSER_INTERNAL_COMPOSITION_INPUT_H_
