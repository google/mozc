// Copyright 2010, Google Inc.
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

// Pool of Session instances, implemented with LRUCache.

#ifndef MOZC_UNIX_EMACS_SESSION_POOL_H_
#define MOZC_UNIX_EMACS_SESSION_POOL_H_

#include "base/linked_ptr.h"
#include "client/session.h"
#include "storage/lru_cache.h"

namespace mozc {
namespace emacs {

class SessionPool {
 public:
  typedef mozc::client::Session Session;

  SessionPool();
  virtual ~SessionPool() {}

  // Returns a new session ID, which is not used in this pool.
  int CreateSession();

  // Deletes a session.  If the specified session ID is not in this pool,
  // does nothing.
  void DeleteSession(int id);

  // Returns a Session instance.  If the specified session ID is not in this
  // pool, creates a new Session and returns it.
  linked_ptr<Session> GetSession(int id);

 private:
  mozc::LRUCache<int, linked_ptr<Session> > lru_cache_;
  int next_id_;

  DISALLOW_COPY_AND_ASSIGN(SessionPool);
};

}  // namespace emacs
}  // namespace mozc

#endif  // MOZC_UNIX_EMACS_SESSION_POOL_H_
