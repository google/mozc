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

// The factory class of Mozc sessions.

#ifndef MOZC_SESSION_SESSION_FACTORY_MANAGER_H_
#define MOZC_SESSION_SESSION_FACTORY_MANAGER_H_

#include "base/base.h"

namespace mozc {
class UserDataManagerInterface;

namespace session {
class SessionInterface;

// SessionFactory creates a new Session for SessionHandler.
class SessionFactoryInterface {
 public:
  virtual ~SessionFactoryInterface() {}

  // Create a new session object.
  virtual SessionInterface *NewSession() ABSTRACT;

  virtual UserDataManagerInterface *GetUserDataManager() ABSTRACT;

  // Reload the configuration if it has.
  virtual void Reload() {}

  // Returns true if the session factory is available.
  virtual bool IsAvailable() const { return true; }

};

// SessionFactoryManager manages the session factory which creates the session
// for the current input.
class SessionFactoryManager {
 public:
  static SessionFactoryInterface *GetSessionFactory();
  static void SetSessionFactory(SessionFactoryInterface *factory);
 private:
  SessionFactoryManager();
  ~SessionFactoryManager();
};
}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_MANAGER_FACTORY_H_
