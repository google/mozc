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

#ifndef MOZC_WIN32_TIP_TIP_THREAD_CONTEXT_H_
#define MOZC_WIN32_TIP_TIP_THREAD_CONTEXT_H_

#include <cstdint>

#include "win32/tip/tip_input_mode_manager.h"

namespace mozc {
namespace win32 {
namespace tsf {

class TipThreadContext {
 public:
  TipThreadContext();

  TipInputModeManager *GetInputModeManager() { return &input_mode_manager_; }

  int32_t GetFocusRevision() const { return focus_revision_; }
  void IncrementFocusRevision();

  void set_use_async_lock_in_key_handler(bool value) {
    use_async_lock_in_key_handler_ = value;
  }
  bool use_async_lock_in_key_handler() const {
    return use_async_lock_in_key_handler_;
  }

 private:
  TipInputModeManager input_mode_manager_;
  int32_t focus_revision_ = 0;

  // A workaround for MS Word's failure mode.
  // See https://github.com/google/mozc/issues/819 for details.
  // TODO(https://github.com/google/mozc/issues/821): Remove this workaround.
  bool use_async_lock_in_key_handler_ = false;
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_THREAD_CONTEXT_H_
