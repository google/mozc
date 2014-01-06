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

#ifndef MOZC_WIN32_TIP_TIP_EDIT_SESSION_IMPL_H_
#define MOZC_WIN32_TIP_TIP_EDIT_SESSION_IMPL_H_

#include <msctf.h>

#include "base/port.h"

namespace mozc {

namespace commands {
class Output;
class SessionCommand;
}  // namespace commands

namespace win32 {
namespace tsf {

class TipTextService;

// A helper function to update the context based on the response from
// mozc_server.
// TODO(yukawa): Use more descriptive class name.
class TipEditSessionImpl {
 public:
  // A high level logic to handle on-composition-terminated event.
  static HRESULT OnCompositionTerminated(TipTextService *text_service,
                                         ITfContext *context,
                                         ITfComposition *composition,
                                         TfEditCookie write_cookie);


  // Does post-edit status checking for composition (if exists). For example,
  // when the composition is canceled by the application, this method sends
  // REVERT message to the server so that the status is kept to be consistent.
  static HRESULT OnEndEdit(TipTextService *text_service,
                           ITfContext *context,
                           TfEditCookie write_cookie,
                           ITfEditRecord *edit_record);

  // A core logic of response handler. This function does
  // - Updates composition string.
  // - Updates candidate strings.
  // - Updates private context including IME On/Off state and input mode.
  // - Invokes UI update. (by calling UpdateUI)
  static HRESULT UpdateContext(TipTextService *text_service,
                               ITfContext *context,
                               TfEditCookie write_cookie,
                               const commands::Output &output);

  // A core logic of UI handler. This function does
  // - Invokes UI update.
  static void UpdateUI(TipTextService *text_service,
                       ITfContext *context,
                       TfEditCookie read_cookie);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(TipEditSessionImpl);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_EDIT_SESSION_IMPL_H_
