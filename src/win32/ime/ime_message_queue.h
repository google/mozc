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

#ifndef MOZC_WIN32_IME_IME_MESSAGE_QUEUE_H_
#define MOZC_WIN32_IME_IME_MESSAGE_QUEUE_H_

#include <windows.h>

#include <vector>

#include "base/port.h"
#include "win32/base/immdev.h"

namespace mozc {
namespace win32 {

// This class behaves as a temporary buffer which keeps IME messages regardless
// of the number of messages.
class MessageQueue {
 public:
  explicit MessageQueue(HIMC himc);

  // Attach the message list comes from ImeToAsciiEx callback.
  void Attach(LPTRANSMSGLIST transmsg);

  // Detach from the message list.
  // Returns the message count in the list.
  int Detach();

  // Push the given message.
  void AddMessage(UINT message, WPARAM wparam, LPARAM lparam);

  // Send the messages to context if not attached to a message list.
  bool Send();

  const std::vector<TRANSMSG> &messages() const;

 private:
  HIMC himc_;
  LPTRANSMSGLIST transmsg_;
  std::vector<TRANSMSG> messages_;
  int transmsg_count_;

  DISALLOW_COPY_AND_ASSIGN(MessageQueue);
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_IME_IME_MESSAGE_QUEUE_H_
