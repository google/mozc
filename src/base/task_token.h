// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_BASE_TASK_TOKEN_H_
#define MOZC_BASE_TASK_TOKEN_H_

#include "base/port.h"
#include "base/scoped_ptr.h"

namespace mozc {
class Mutex;

class TaskToken {
 public:
  TaskToken() : id_(0) {}
  explicit TaskToken(uint64 id) : id_(id) {}
  inline uint64 id() const {
    return id_;
  }
  inline bool operator==(const TaskToken& other) const {
    return id_ == other.id_;
  }
  inline bool operator!=(const TaskToken& other) const {
    return id_ != other.id_;
  }
  inline bool operator>(const TaskToken& other) const {
    return id_ > other.id_;
  }
  inline bool operator>=(const TaskToken& other) const {
    return id_ >= other.id_;
  }
  inline bool operator<(const TaskToken& other) const {
    return id_ < other.id_;
  }
  inline bool operator<=(const TaskToken& other) const {
    return id_ <= other.id_;
  }
  inline bool isValid() const {
    return (id_ != 0);
  }

 private:
  uint64 id_;
};

class ThreadSafeTaskTokenManager {
 public:
  ThreadSafeTaskTokenManager();
  ~ThreadSafeTaskTokenManager();
  TaskToken NewToken();

 private:
  uint64 task_id_count_;
  scoped_ptr<Mutex> mutex_;
  DISALLOW_COPY_AND_ASSIGN(ThreadSafeTaskTokenManager);
};

}  // namespace mozc

#endif  // MOZC_BASE_TASK_TOKEN_H_
