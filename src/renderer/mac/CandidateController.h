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

#ifndef MOZC_RENDERER_MAC_CANDIDATE_CONTROLLER_H_
#define MOZC_RENDERER_MAC_CANDIDATE_CONTROLLER_H_

#include "protocol/renderer_command.pb.h"
#include "renderer/renderer_interface.h"

namespace mozc {
namespace client {
class SendCommandInterface;
}  // namespace client

namespace renderer {
namespace mac {
class CandidateWindow;
class InfolistWindow;

// CandidateController implements the renderer interface for Mac.  For
// the detailed information of renderer interface, see
// renderer/renderer_interface.h.
class CandidateController : public RendererInterface {
 public:
  CandidateController();
  CandidateController(const CandidateController &) = delete;
  CandidateController &operator=(const CandidateController &) = delete;
  ~CandidateController();
  virtual bool Activate();
  virtual bool IsAvailable() const;
  virtual bool ExecCommand(const commands::RendererCommand &command);
  virtual void SetSendCommandInterface(
      client::SendCommandInterface *send_command_interface);

 private:
  // Relocate windows to prevent overlaps.
  void AlignWindows();

  // We don't use std::unique_ptr<> for those two pointers because we don't
  // want to include CandidateWindow.h when the user of this class
  // includes this file.  Because CandidateWindow.h and InfolistWindow.h are in
  // Objective-C++, the user file must be .mm files if we use
  // std::unique_ptr<>.
  CandidateWindow *candidate_window_;
  CandidateWindow *cascading_window_;
  InfolistWindow *infolist_window_;
  mozc::commands::RendererCommand command_;
};

}  // namespace mac
}  // namespace renderer
}  // namespace mozc

#endif  // MOZC_RENDERER_MAC_CANDIDATE_CONTROLLER_H_
