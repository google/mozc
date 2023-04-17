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

#ifndef MOZC_WIN32_TIP_TIP_PRIVATE_CONTEXT_H_
#define MOZC_WIN32_TIP_TIP_PRIVATE_CONTEXT_H_

#include <windows.h>

#include <memory>

#include "base/port.h"

namespace mozc {

namespace client {
class ClientInterface;
}  // namespace client

namespace commands {
class Output;
}  // namespace commands

namespace win32 {

struct InputBehavior;
class SurrogatePairObserver;
class VKBackBasedDeleter;
class VirtualKey;

namespace tsf {

class TipUiElementManager;

class TipPrivateContext {
 public:
  TipPrivateContext();
  TipPrivateContext(const TipPrivateContext &) = delete;
  TipPrivateContext &operator=(const TipPrivateContext &) = delete;
  ~TipPrivateContext();

  void EnsureInitialized();
  client::ClientInterface *GetClient();
  SurrogatePairObserver *GetSurrogatePairObserver();
  TipUiElementManager *GetUiElementManager();
  VKBackBasedDeleter *GetDeleter();

  const commands::Output &last_output() const;
  commands::Output *mutable_last_output();
  const VirtualKey &last_down_key() const;
  VirtualKey *mutable_last_down_key();
  const InputBehavior &input_behavior() const;
  InputBehavior *mutable_input_behavior();

 private:
  class InternalState;
  std::unique_ptr<InternalState> state_;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_PRIVATE_CONTEXT_H_
