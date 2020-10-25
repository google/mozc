// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_UNIX_IBUS_MESSAGE_TRANSLATOR_H_
#define MOZC_UNIX_IBUS_MESSAGE_TRANSLATOR_H_

#include <map>
#include <string>

#include "base/port.h"

namespace mozc {
namespace ibus {

// This class is responsible for translation from given message to
// the target message.
// TODO(team): Consider to use other libraries such as gettext.
class MessageTranslatorInterface {
 public:
  virtual ~MessageTranslatorInterface();

  // Returns translated string if possible.
  // Returns |message| if fails to translate.
  virtual std::string MaybeTranslate(const std::string &message) const = 0;
};

// This class never translates messages actually.
class NullMessageTranslator : public MessageTranslatorInterface {
 public:
  NullMessageTranslator();

  // Always returns |message|.
  virtual std::string MaybeTranslate(const std::string &message) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullMessageTranslator);
};

// Locale based translator. Currently only "ja_JP.UTF-8" is
// supported.
class LocaleBasedMessageTranslator : public MessageTranslatorInterface {
 public:
  explicit LocaleBasedMessageTranslator(const std::string &locale_name);
  virtual std::string MaybeTranslate(const std::string &message) const;

 private:
  std::map<std::string, std::string> utf8_japanese_map_;
  DISALLOW_COPY_AND_ASSIGN(LocaleBasedMessageTranslator);
};

}  // namespace ibus
}  // namespace mozc

#endif  // MOZC_UNIX_IBUS_MESSAGE_TRANSLATOR_H_
