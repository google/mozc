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

// The implementation of SessionFactory.

#include "session/japanese_session_factory.h"

#include "base/singleton.h"
#include "composer/table.h"
#include "converter/converter_interface.h"
#include "session/internal/keymap.h"
#include "session/session_interface.h"
#include "session/session.h"

namespace mozc {
namespace session {

JapaneseSessionFactory::JapaneseSessionFactory()
    : is_available_(false) {

  if (!Singleton<composer::Table>::get()->Initialize()) {
    return;
  }
  is_available_ = true;
}

JapaneseSessionFactory::~JapaneseSessionFactory() {
}

void JapaneseSessionFactory::Reload() {
  Singleton<composer::Table>::get()->Reload();
}

SessionInterface *JapaneseSessionFactory::NewSession() {
  return new Session();
}

UserDataManagerInterface *JapaneseSessionFactory::GetUserDataManager() {
  return ConverterFactory::GetConverter()->GetUserDataManager();
}

bool JapaneseSessionFactory::IsAvailable() const {
  return is_available_;
}

}  // namespace session
}  // namespace mozc
