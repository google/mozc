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

#ifndef MOZC_WIN32_TIP_TIP_STATUS_H_
#define MOZC_WIN32_TIP_TIP_STATUS_H_

#include <Windows.h>
#include <msctf.h>

#include "base/port.h"

namespace mozc {

namespace commands {
class Status;
}  // namespace commands

namespace win32 {
namespace tsf {

// A tentative, kitchen-sink class for getting/setting IME status via TSF APIs.
// TODO(yukawa): Revisit and refactor when minimum implementation has done.
class TipStatus {
 public:
  TipStatus() = delete;
  TipStatus(const TipStatus &) = delete;
  TipStatus &operator=(const TipStatus &) = delete;

  // Returns true if the keyboard state specified by |thread_mgr| is open.
  static bool IsOpen(ITfThreadMgr *thread_mgr);

  // Returns true if the context state specified by |context| is disabled.
  static bool IsDisabledContext(ITfContext *context);

  // Returns true if the context state specified by |context| is empty.
  static bool IsEmptyContext(ITfContext *context);

  // Returns true if TSF conversion mode is successfully retrieved into |mode|.
  static bool GetInputModeConversion(ITfThreadMgr *thread_mgr,
                                     TfClientId client_id, DWORD *mode);

  // Returns true if TSF keyboard open/closed mode is updated.
  static bool SetIMEOpen(ITfThreadMgr *thread_mgr, TfClientId client_id,
                         bool open);

  // Returns true if TSF conversion mode is updated.
  static bool SetInputModeConversion(ITfThreadMgr *thread_mgr, DWORD client_id,
                                     DWORD native_mode);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_STATUS_H_
