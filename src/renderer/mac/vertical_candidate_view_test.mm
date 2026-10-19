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

#include "base/coordinates.h"
#include "client/client_interface.h"
#include "protocol/candidate_window.pb.h"
#include "protocol/commands.pb.h"
#import "renderer/mac/CandidateView.h"
#include "renderer/mac/mac_view_util.h"
#include "renderer/table_layout.h"
#include "testing/gunit.h"

namespace mozc::renderer::mac {
namespace {

commands::CandidateWindow MakeCandidates() {
  commands::CandidateWindow candidates;
  candidates.set_size(20);
  candidates.set_focused_index(0);
  candidates.mutable_footer()->set_label("候補");
  candidates.mutable_footer()->set_index_visible(true);
  for (int i = 0; i < 3; ++i) {
    auto *candidate = candidates.add_candidate();
    candidate->set_id(100 + i);
    candidate->set_index(i);
    candidate->set_value("縦書き候補");
  }
  return candidates;
}

TEST(VerticalCandidateViewTest, LayoutSwitchingAndRightToLeftColumns) {
  [NSApplication sharedApplication];
  CandidateView *view = [[CandidateView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
  const auto candidates = MakeCandidates();
  [view setCandidateWindow:&candidates];
  const NSSize horizontal = [view updateLayout];
  [view setVerticalWriting:YES];
  const NSSize vertical = [view updateLayout];
  const TableLayout *layout = [view tableLayout];
  const Rect first = [view clientRect:layout->GetRowRect(0)];
  const Rect second = [view clientRect:layout->GetRowRect(1)];
  EXPECT_GT(first.Left(), second.Left());
  EXPECT_EQ(first.Top(), second.Top());
  const Rect footer = [view clientRect:layout->GetFooterRect()];
  EXPECT_LE(footer.Right(), second.Left());
  EXPECT_EQ(vertical.width, layout->GetTotalSize().height);
  EXPECT_EQ(vertical.height, layout->GetTotalSize().width);
  [view setVerticalWriting:NO];
  const NSSize restored = [view updateLayout];
  EXPECT_EQ(restored.width, horizontal.width);
  EXPECT_EQ(restored.height, horizontal.height);
}

TEST(VerticalCandidateViewTest, NativeVerticalTextMeasurement) {
  [NSApplication sharedApplication];
  NSAttributedString *text = [[NSAttributedString alloc]
      initWithString:@"日本語の縦書き（候補）。"
          attributes:@{NSFontAttributeName : [NSFont systemFontOfSize:18]}];
  const NSSize horizontal = MacViewUtil::TextSize(text, false);
  const NSSize vertical = MacViewUtil::TextSize(text, true);
  EXPECT_GT(horizontal.width, horizontal.height);
  EXPECT_GT(vertical.height, vertical.width);
  EXPECT_GT(vertical.width, 0);
}

class CommandRecorder : public client::SendCommandInterface {
 public:
  bool SendCommand(const commands::SessionCommand &command, commands::Output *) override {
    last = command;
    ++count;
    return true;
  }
  commands::SessionCommand last;
  int count = 0;
};

TEST(VerticalCandidateViewTest, MouseSelectsTheTransformedCandidateColumn) {
  [NSApplication sharedApplication];
  CandidateView *view = [[CandidateView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
  const auto candidates = MakeCandidates();
  CommandRecorder recorder;
  [view setSendCommandInterface:&recorder];
  [view setCandidateWindow:&candidates];
  [view setVerticalWriting:YES];
  const NSSize size = [view updateLayout];
  [view setFrameSize:size];
  NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, size.width, size.height)
                                                 styleMask:NSWindowStyleMaskBorderless
                                                   backing:NSBackingStoreBuffered
                                                     defer:YES];
  [window setReleasedWhenClosed:NO];
  [window setContentView:view];
  const TableLayout *layout = [view tableLayout];
  for (int i = 0; i < 3; ++i) {
    const Rect rect = [view clientRect:layout->GetRowRect(i)];
    const NSPoint local =
        NSMakePoint(rect.Left() + rect.Width() / 2, rect.Top() + rect.Height() / 2);
    const NSPoint point = [view convertPoint:local toView:nil];
    NSEvent *event = [NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
                                        location:point
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:[window windowNumber]
                                         context:nil
                                     eventNumber:0
                                      clickCount:1
                                        pressure:0];
    [view mouseUp:event];
    EXPECT_EQ(recorder.count, i + 1);
    EXPECT_EQ(recorder.last.type(), commands::SessionCommand::SELECT_CANDIDATE);
    EXPECT_EQ(recorder.last.id(), 100 + i);
  }
  [view setSendCommandInterface:nullptr];
  [window close];
}

}  // namespace
}  // namespace mozc::renderer::mac
