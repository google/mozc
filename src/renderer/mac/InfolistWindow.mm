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

#import "InfolistView.h"

#include "absl/log/log.h"
#include "base/coordinates.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "renderer/mac/InfolistWindow.h"

using mozc::commands::CandidateWindow;
using mozc::commands::Output;
using mozc::commands::SessionCommand;

@interface InfolistWindowTimerHandler : NSObject {
 @private
  mozc::renderer::mac::InfolistWindow* infolist_window_;
}
- (InfolistWindowTimerHandler*)initWithInfolistWindow:
    (mozc::renderer::mac::InfolistWindow*)infolist_window;
- (void)onTimer:(NSTimer*)timer;
@end

@implementation InfolistWindowTimerHandler
- (InfolistWindowTimerHandler*)initWithInfolistWindow:
    (mozc::renderer::mac::InfolistWindow*)infolist_window {
  self = [super init];
  infolist_window_ = infolist_window;
  return self;
}
- (void)onTimer:(NSTimer*)timer {
  if (infolist_window_) {
    infolist_window_->onTimer(timer);
  }
}
@end

namespace mozc {
namespace renderer {
namespace mac {

InfolistWindow::InfolistWindow() : lasttimer_(nullptr), command_sender_(nullptr) {
  timer_handler_ = [[InfolistWindowTimerHandler alloc] initWithInfolistWindow:this];
}

InfolistWindow::~InfolistWindow() {}

void InfolistWindow::SetSendCommandInterface(client::SendCommandInterface* send_command_interface) {
  command_sender_ = send_command_interface;
}

void InfolistWindow::SetCandidateWindow(const CandidateWindow& candidate_window) {
  if (candidate_window.candidate_size() == 0) {
    return;
  }

  if (!window_) {
    InitWindow();
  }
  InfolistView* infolist_view = (InfolistView*)view_;
  [infolist_view setCandidateWindow:&candidate_window];
  [infolist_view setNeedsDisplay:YES];
  NSSize size = [infolist_view updateLayout];
  ResizeWindow(size.width, size.height);
}

void InfolistWindow::DelayHide(int delay) {
  DLOG(INFO) << "InfolistWindow::DelayHide()";
  if (lasttimer_) {
    [lasttimer_ invalidate];
  }
  visible_ = false;
  const NSTimeInterval interval = delay / 1000.0;
  lasttimer_ = [NSTimer scheduledTimerWithTimeInterval:interval
                                                target:timer_handler_
                                              selector:@selector(onTimer:)
                                              userInfo:nil
                                               repeats:NO];
}

void InfolistWindow::DelayShow(int delay) {
  DLOG(INFO) << "InfolistWindow::DelayShow()";
  if (lasttimer_) {
    [lasttimer_ invalidate];
  }
  visible_ = true;
  const NSTimeInterval interval = delay / 1000.0;
  lasttimer_ = [NSTimer scheduledTimerWithTimeInterval:interval
                                                target:timer_handler_
                                              selector:@selector(onTimer:)
                                              userInfo:nil
                                               repeats:NO];
}

void InfolistWindow::Hide() {
  RendererBaseWindow::Hide();
  visible_ = false;
}

void InfolistWindow::Show() {
  RendererBaseWindow::Show();
  visible_ = true;
}

void InfolistWindow::onTimer(NSTimer* timer) {
  DLOG(INFO) << "InfolistWindow::onTimer()";
  if (visible_) {
    Show();
  } else {
    Hide();
  }
  lasttimer_ = nil;
}

void InfolistWindow::ResetView() {
  DLOG(INFO) << "InfolistWindow::ResetView()";
  view_ = [[InfolistView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)];
}

}  // namespace mac
}  // namespace renderer
}  // namespace mozc
