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

#include "unix/emacs/session_pool.h"

namespace mozc {
namespace emacs {

static const int kMaxSessions = 64;  // max number of parallel sessions

SessionPool::SessionPool() : lru_cache_(kMaxSessions), next_id_(1) {
}

int SessionPool::CreateSession() {
  // Emacs supports at-least 28-bit integer.
  const int k28BitIntMax = 134217727;
  while (lru_cache_.HasKey(next_id_)) {
    if (++next_id_ <= 0 || k28BitIntMax < next_id_) {
      next_id_ = 1;  // Keep next_id_ to be a positive 28-bit integer.
    }
  }
  lru_cache_.Insert(next_id_, linked_ptr<Session>(new Session()));
  return next_id_++;
}

void SessionPool::DeleteSession(int id) {
  lru_cache_.Erase(id);
}

linked_ptr<SessionPool::Session> SessionPool::GetSession(int id) {
  const linked_ptr<Session> *value = lru_cache_.Lookup(id);
  if (value) {
    lru_cache_.Insert(id, *value);  // Put id at the head of LRU.
    return *value;
  } else {
    Session *session = new Session();
    linked_ptr<Session> session_ptr(session);
    lru_cache_.Insert(id, session_ptr);
    return session_ptr;
  }
}

}  // namespace emacs
}  // namespace mozc
