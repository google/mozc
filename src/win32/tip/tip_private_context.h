// Copyright 2010-2013, Google Inc.
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

#include <Windows.h>

#include "base/port.h"
#include "base/scoped_ptr.h"

namespace mozc {

namespace client {
class ClientInterface;
}  // namespace client

namespace commands {
class Output;
}  // namespace commands

namespace win32 {

struct InputState;
struct InputBehavior;
class SurrogatePairObserver;
class VKBackBasedDeleter;

namespace tsf {

class TipUiElementManager;

class TipPrivateContext {
 public:
  TipPrivateContext(DWORD text_edit_sink_cookie,
                    DWORD text_layout_sink_cookie);
  ~TipPrivateContext();

  client::ClientInterface *GetClient();
  SurrogatePairObserver *GetSurrogatePairObserver();
  TipUiElementManager *GetUiElementManager();
  VKBackBasedDeleter *GetDeleter();

  const commands::Output &last_output() const;
  commands::Output *mutable_last_output();
  const InputState &input_state() const;
  InputState *mutable_input_state();
  const InputBehavior &input_behavior() const;
  InputBehavior *mutable_input_behavior();
  DWORD text_edit_sink_cookie() const;
  DWORD text_layout_sink_cookie() const;

 private:
  scoped_ptr<client::ClientInterface> client_;
  scoped_ptr<SurrogatePairObserver> surrogate_pair_observer_;
  scoped_ptr<commands::Output> last_output_;
  scoped_ptr<InputState> input_state_;
  scoped_ptr<InputBehavior> input_behavior_;
  scoped_ptr<TipUiElementManager> ui_element_manager_;
  scoped_ptr<VKBackBasedDeleter> deleter_;

  const DWORD text_edit_sink_cookie_;
  const DWORD text_layout_sink_cookie_;

  DISALLOW_COPY_AND_ASSIGN(TipPrivateContext);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_PRIVATE_CONTEXT_H_
