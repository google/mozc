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

#ifndef MOZC_WIN32_IME_IME_PRIVATE_CONTEXT_H_
#define MOZC_WIN32_IME_IME_PRIVATE_CONTEXT_H_

#include <windows.h>

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "win32/base/immdev.h"

namespace mozc {
namespace client {
class ClientInterface;
}  // namespace client

namespace commands {
class Output;
}  // namespace commands

namespace win32 {
struct ImeState;
struct ImeBehavior;
class SurrogatePairObserver;
class UIVisibilityTracker;
class VKBackBasedDeleter;

// A POD, which stores client information to communicate with the converter.
struct PrivateContext {
  bool Initialize();
  bool Uninitialize();
  bool Validate() const;
  uint32 magic_number;
  DWORD thread_id;
  ImeState *ime_state;
  ImeBehavior *ime_behavior;
  mozc::client::ClientInterface *client;
  UIVisibilityTracker *ui_visibility_tracker;
  mozc::commands::Output *last_output;
  VKBackBasedDeleter *deleter;
  SurrogatePairObserver *surrogate_pair_observer;
};

// This class is a temporal solution of b/3021166
// TODO(yukawa): refactor the lifetime management mechanism for PrivateContext.
class PrivateContextUtil {
 public:
  // Returns true if given private data handle is pointing a valid
  // PrivateContext.
  static bool IsValidPrivateContext(HIMCC private_data_handle);

  // Returns true if given private data handle is consistent state.  If the
  // pointed data is not a Mozc's PrivateContext, this method initializes it.
  static bool EnsurePrivateContextIsInitialized(
      HIMCC *private_data_handle_pointer);

 private:
  DISALLOW_COPY_AND_ASSIGN(PrivateContextUtil);
};
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_IME_IME_PRIVATE_CONTEXT_H_
