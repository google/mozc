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

#ifndef MOZC_WIN32_TIP_TIP_EDIT_SESSION_H_
#define MOZC_WIN32_TIP_TIP_EDIT_SESSION_H_

#include <msctf.h>

#include <cstdint>
#include <string>
#include <string_view>

#include "protocol/commands.pb.h"
#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {
namespace tsf {

// Utility functions to begin edit session for various purposes.
class TipEditSession {
 public:
  TipEditSession() = delete;
  TipEditSession(const TipEditSession &) = delete;
  TipEditSession &operator=(const TipEditSession &) = delete;

  // Begins a sync edit session with |new_output| to update the context. Note
  // that sync edit session is guaranteed to be capable only in key event
  // handler and ITfFnReconversion::QueryRange. In other cases, you should use
  // OnOutputReceivedAsync instead.
  static bool OnOutputReceivedSync(TipTextService *text_service,
                                   ITfContext *context,
                                   commands::Output new_output);

  // Begins an async edit session with |new_output| to update the context.
  static bool OnOutputReceivedAsync(TipTextService *text_service,
                                    ITfContext *context,
                                    commands::Output new_output);

  // Begins a sync edit session to invoke reconversion that is initialized by
  // the application.
  static bool ReconvertFromApplicationSync(TipTextService *text_service,
                                           ITfRange *range);

  // Begins an async edit session to handle on-layout-changed event.
  static bool OnLayoutChangedAsync(TipTextService *text_service,
                                   ITfContext *context);

  // Begins an async edit session to handle on-mode-changed event.
  static bool OnSetFocusAsync(TipTextService *text_service,
                              ITfDocumentMgr *document_manager);

  // Begins an async edit session to handle on-mode-changed event.
  static bool OnModeChangedAsync(TipTextService *text_service);
  // Begins an async edit session to handle on-open-close-changed event.
  static bool OnOpenCloseChangedAsync(TipTextService *text_service);
  // Begins an async edit session to handle a renderer callback event.
  static bool OnRendererCallbackAsync(TipTextService *text_service,
                                      ITfContext *context, WPARAM wparam,
                                      LPARAM lparam);

  // Begins an async edit session to submit the current candidate.
  static bool SubmitAsync(TipTextService *text_service, ITfContext *context);
  // Begins an async edit session to cancel the current composition.
  static bool CancelCompositionAsync(TipTextService *text_service,
                                     ITfContext *context);
  // Begins an async edit session to highlight the candidate specified by
  // |candidate_id|.
  static bool HilightCandidateAsync(TipTextService *text_service,
                                    ITfContext *context, int candidate_id);
  // Begins an async edit session to select the candidate specified by
  // |candidate_id|.
  static bool SelectCandidateAsync(TipTextService *text_service,
                                   ITfContext *context, int candidate_id);

  // Begins an async edit session to change input mode specified by
  // |native_mode|.
  static bool SwitchInputModeAsync(TipTextService *text_service,
                                   uint32_t mozc_mode);

  // Begins a sync edit session to retrieve the text from |range|.
  static bool GetTextSync(TipTextService *text_service, ITfRange *range,
                          std::wstring *text, bool *is_composing);

  // Begins an async edit session to set |text| to |range|.
  static bool SetTextAsync(TipTextService *text_service, std::wstring_view text,
                           ITfRange *range);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_EDIT_SESSION_H_
