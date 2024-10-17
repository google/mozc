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

#import <Cocoa/Cocoa.h>

#include "protocol/candidates.pb.h"
#include "protocol/renderer_command.pb.h"

namespace mozc {
namespace client {
class SendCommandInterface;
}  // namespace mozc::client

namespace renderer {
class TableLayout;
class RendererStyle;
}  // namespace mozc::renderer
}  // namespace mozc

// usage type for each column.
enum COLUMN_TYPE {
  COLUMN_SHORTCUT = 0,  // show shortcut key
  COLUMN_GAP1,          // padding region 1
  COLUMN_CANDIDATE,     // show candidate string
  COLUMN_DESCRIPTION,   // show description message
  NUMBER_OF_COLUMNS,    // number of columns. (this item should be last)
};

// CandidateView is an NSView subclass to draw the candidate window
// according to the current candidates.
@interface CandidateView : NSView

// setCandidateWindow: sets the candidate window to be rendered.
- (void)setCandidateWindow:(const mozc::commands::CandidateWindow *)candidate_window;

// setController: sets the reference of MozcImkInputController.
// It will be used when mouse clicks.  It doesn't take ownerships of
// |controller|.
- (void)setSendCommandInterface:(mozc::client::SendCommandInterface *)command_sender;

// Checks the |candidates_| and recalculates the layout using |tableLayout_|.
// It also returns the size which is necessary to draw all GUI elements.
- (NSSize)updateLayout;

// Returns the table layout of the current candidates.
- (const mozc::renderer::TableLayout *)tableLayout;
@end
