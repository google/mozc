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

#include "win32/ime/ime_message_queue.h"

#include "win32/ime/ime_scoped_context.h"

namespace mozc {
namespace win32 {
namespace {

static HIMCC InitializeHIMCC(HIMCC himcc, DWORD size) {
  if (himcc == NULL) {
    return ::ImmCreateIMCC(size);
  } else {
    return ::ImmReSizeIMCC(himcc, size);
  }
}

}  // namespace

MessageQueue::MessageQueue(HIMC himc)
    : himc_(himc), transmsg_(NULL), transmsg_count_(0) {
}

void MessageQueue::Attach(LPTRANSMSGLIST transmsg) {
  Send();
  transmsg_ = transmsg;
}

int MessageQueue::Detach() {
  const LPTRANSMSGLIST transmsg = transmsg_;
  transmsg_ = NULL;
  const int transmsg_count = transmsg_count_;
  transmsg_count_ = 0;

  // If we are not using the extended message vector, means that the
  // TRANSMSGLIST is big enough, simply return the count in TRANSMSGLIST.
  if (messages_.size() == 0) {
    return transmsg_count;
  }

  // If |transmsg_| is not big enough to store all messages, those extra
  // messages are stored temporarily in |messages_| vector. In this case, all
  // the messages should be stored in the message buffer of the context.
  // Generally, |transmsg_| can contains 256 messages, but this number is not
  // documented, so there are chances that it may be full.

  ScopedHIMC<INPUTCONTEXT> context(himc_);
  // If anything wrong, return the message count in TRANSMSGLIST, the extra
  // messages will be saved in vector so they can be sent out later.
  if (context.get() == NULL) {
    return transmsg_count;
  }

  // In this case, use |message_buffer| to send messages.
  const size_t total_num_messages = transmsg_count + messages_.size();
  const size_t total_bytes = total_num_messages * sizeof(TRANSMSG);
  context->hMsgBuf = InitializeHIMCC(context->hMsgBuf, total_bytes);
  ScopedHIMCC<TRANSMSG> message_buffer(context->hMsgBuf);
  if (message_buffer.get() == NULL) {
    return transmsg_count;
  }

  // At first, messages in |transmsg->TransMsg| must be copied into
  // |message_buffer|.
  for (size_t i = 0; i < transmsg_count; ++i) {
    message_buffer.get()[i].message = transmsg->TransMsg[i].message;
    message_buffer.get()[i].wParam = transmsg->TransMsg[i].wParam;
    message_buffer.get()[i].lParam = transmsg->TransMsg[i].lParam;
  }
  // Next, copy the rest messages from |messages_| to |message_buffer|.
  const size_t count_in_vector = messages_.size();
  for (size_t i = transmsg_count; i < total_num_messages; ++i) {
    const size_t index_in_vector = i - transmsg_count;
    message_buffer.get()[i].message = messages_[index_in_vector].message;
    message_buffer.get()[i].wParam = messages_[index_in_vector].wParam;
    message_buffer.get()[i].lParam = messages_[index_in_vector].lParam;
  }
  messages_.clear();

  return total_num_messages;
}

void MessageQueue::AddMessage(UINT message, WPARAM wparam, LPARAM lparam) {
  // If transmsg_ is not big enough, we store extra messages in messages_
  // vector.
  if ((transmsg_ != NULL) && (transmsg_count_ < transmsg_->uMsgCount)) {
    transmsg_->TransMsg[transmsg_count_].message = message;
    transmsg_->TransMsg[transmsg_count_].wParam = wparam;
    transmsg_->TransMsg[transmsg_count_].lParam = lparam;
    transmsg_count_++;
    return;
  }

  const TRANSMSG transmsg = { message, wparam, lparam };
  messages_.push_back(transmsg);
}

bool MessageQueue::Send() {
  // Don't send if attached to a TRANSMSGLIST, these messages will be sent via
  // the buffer provided by ImeToAsciiEx.
  if (transmsg_ != NULL) {
    return false;
  }
  if (messages_.size() == 0) {
    return false;
  }

  ScopedHIMC<INPUTCONTEXT> context(himc_);
  if (context.get() == NULL) {
    return false;
  }

  const int size = static_cast<int>(messages_.size()) * sizeof(TRANSMSG);
  context->hMsgBuf = InitializeHIMCC(context->hMsgBuf, size);
  ScopedHIMCC<TRANSMSG> message_buffer(context->hMsgBuf);
  if (message_buffer.get() == NULL) {
    return false;
  }

  const size_t count_in_vector = messages_.size();
  for (size_t i = 0; i < count_in_vector; ++i) {
    message_buffer.get()[i].message = messages_[i].message;
    message_buffer.get()[i].wParam = messages_[i].wParam;
    message_buffer.get()[i].lParam = messages_[i].lParam;
  }
  context->dwNumMsgBuf = count_in_vector;
  messages_.clear();

  if (!::ImmGenerateMessage(himc_)) {
    return false;
  }

  return true;
}

}  // namespace win32
}  // namespace mozc
