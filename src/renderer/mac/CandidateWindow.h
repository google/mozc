// Copyright 2010-2012, Google Inc.
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

#ifndef MOZC_RENDERER_MAC_CANDIDATE_WINDOW_H_
#define MOZC_RENDERER_MAC_CANDIDATE_WINDOW_H_

#import <Carbon/Carbon.h>

#include "base/scoped_nsobject.h"
#include "renderer/mac/RendererBaseWindow.h"

namespace mozc {
namespace client {
class SendCommandInterface;
}  // namespace mozc::client
namespace commands {
class Candidates;
}  // namespace mozc::commands
namespace renderer {
class TableLayout;

namespace mac {
// CandidateWindow holds a carbon window and maintains the connection
// between a window and a CandidateView.
class CandidateWindow : public RendererBaseWindow {
 public:
  explicit CandidateWindow();
  ~CandidateWindow();
  void SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface);
  void SetCandidates(const commands::Candidates &candidates);
  const renderer::TableLayout *GetTableLayout() const;
 private:
  void InitWindow();
  void ResetView();
  mozc::client::SendCommandInterface *command_sender_;
  DISALLOW_COPY_AND_ASSIGN(CandidateWindow);
};

}  // namespace mozc::renderer::mac
}  // namespace mozc::renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_MAC_CANDIDATE_WINDOW_H_
