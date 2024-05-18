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

#include <Carbon/Carbon.h>
#include <objc/message.h>

#import "CandidateView.h"

#include "absl/log/log.h"
#include "base/coordinates.h"
#include "protocol/commands.pb.h"
#include "renderer/mac/CandidateWindow.h"

using mozc::commands::Candidates;

namespace mozc {
namespace renderer {
namespace mac {

CandidateWindow::CandidateWindow() : command_sender_(nullptr) {}

CandidateWindow::~CandidateWindow() {}

void CandidateWindow::SetSendCommandInterface(
    client::SendCommandInterface* send_command_interface) {
  DLOG(INFO) << "CandidateWindow::SetSendCommandInterface()";
  command_sender_ = send_command_interface;

  const CandidateView* candidate_view = (CandidateView*)view_;
  [candidate_view setSendCommandInterface:send_command_interface];
}

void CandidateWindow::InitWindow() {
  RendererBaseWindow::InitWindow();
  const CandidateView* candidate_view = (CandidateView*)view_;
  [candidate_view setSendCommandInterface:command_sender_];
}
const mozc::renderer::TableLayout* CandidateWindow::GetTableLayout() const {
  const CandidateView* candidate_view = (CandidateView*)view_;
  return [candidate_view tableLayout];
}

void CandidateWindow::SetCandidates(const Candidates& candidates) {
  DLOG(INFO) << "CandidateWindow::SetCandidates";
  if (candidates.candidate_size() == 0) {
    return;
  }

  if (!window_) {
    InitWindow();
  }
  CandidateView* candidate_view = (CandidateView*)view_;
  [candidate_view setCandidates:&candidates];
  [candidate_view setNeedsDisplay:YES];
  NSSize size = [candidate_view updateLayout];
  ResizeWindow(size.width, size.height);
}

void CandidateWindow::ResetView() {
  DLOG(INFO) << "CandidateWindow::ResetView()";
  view_ = [[CandidateView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
}

}  // namespace mac
}  // namespace renderer
}  // namespace mozc
