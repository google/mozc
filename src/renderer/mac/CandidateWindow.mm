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

#include <Carbon/Carbon.h>
#include <objc/message.h>

#import "CandidateView.h"

#include "base/base.h"
#include "session/commands.pb.h"
#include "renderer/coordinates.h"
#include "renderer/mac/CandidateWindow.h"

using mozc::commands::Candidates;

namespace mozc {
namespace renderer{
namespace mac {

namespace {
enum CandidateStatusCode {
  userDataMissingErr = 1000,
  unknownEventErr = 1001,
};

OSStatus EventHandler(EventHandlerCallRef handlerCallRef,
                      EventRef carbonEvent,
                      void *userData) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSEvent *event = [NSEvent eventWithEventRef:carbonEvent];

  if (!userData) {
    [pool drain];
    return userDataMissingErr;
  }

  NSView *target = reinterpret_cast<NSView *>(userData);
  SEL eventType = @selector(unknownEvent);
  switch ([event type]) {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      eventType = @selector(mouseDown:);
      break;

    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      eventType = @selector(mouseUp:);
      break;

    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
      eventType = @selector(mouseDragged:);
      break;

    case NSScrollWheel:
      eventType = @selector(scrollWheel:);
      break;

    default:
      break;
  }

  if (eventType == @selector(unknownEvent)) {
    [pool drain];
    return unknownEventErr;
  }

  // The locationInWindow of the |event| is global for some reasons.
  // Here we convert it into window-local.
  NSPoint location = [event locationInWindow];
  NSRect windowRect = [[target window] frame];
  location.x = location.x - windowRect.origin.x;
  location.y = location.y - windowRect.origin.y;
  NSEvent *actualEvent = [NSEvent mouseEventWithType:[event type]
                                            location:location
                                       modifierFlags:[event modifierFlags]
                                           timestamp:[event timestamp]
                                        windowNumber:[event windowNumber]
                                             context:[event context]
                                         eventNumber:[event eventNumber]
                                          clickCount:[event clickCount]
                                            pressure:[event pressure]];

  objc_msgSend(target, eventType, actualEvent);
  [pool drain];
  return noErr;
}
}  // anonymous namespace

CandidateWindow::CandidateWindow()
    : window_(NULL), command_sender_(NULL) {
  InitCandidateWindow();
}

void CandidateWindow::InitCandidateWindow() {
  // Creating Window
  ::Rect rect;
  SetRect(&rect, 0, 0, 1, 1);
  CreateNewWindow(kUtilityWindowClass,
                  kWindowNoTitleBarAttribute | kWindowCompositingAttribute |
                  kWindowStandardHandlerAttribute, &rect, &window_);

  // Changing the window level as same as overlay window class, which
  // means "top".
  WindowGroupRef groupRef = GetWindowGroup(window_);
  int groupLevel;
  GetWindowGroupLevelOfType(GetWindowGroupOfClass(kOverlayWindowClass),
                            kWindowGroupLevelPromoted, &groupLevel);
  SetWindowGroupLevel(groupRef, groupLevel);
  SetWindowGroupParent(groupRef, GetWindowGroupOfClass(kAllWindowClasses));

  view_.reset([[CandidateView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)]);

  // Embedding CandidateView into the |window_| using Carbon API
  HIViewRef hiView, contentView;
  HIRect bounds;
  HICocoaViewCreate(view_.get(), 0, &hiView);
  HIViewFindByID(HIViewGetRoot(window_), kHIViewWindowContentID,
                 &contentView);
  HIViewGetBounds(contentView, &bounds);
  HIViewSetFrame(hiView, &bounds);
  HIViewAddSubview(contentView, hiView);
  HIViewSetVisible(hiView, true);

  // Adjust Carbon layouts
  HILayoutInfo layoutInfo;
  layoutInfo.version = kHILayoutInfoVersionZero;
  HIViewGetLayoutInfo(hiView, &layoutInfo);
  layoutInfo.binding.top.toView = contentView;
  layoutInfo.binding.top.kind = kHILayoutBindTop;
  layoutInfo.binding.left.toView = contentView;
  layoutInfo.binding.left.kind = kHILayoutBindLeft;
  layoutInfo.binding.right.toView = contentView;
  layoutInfo.binding.right.kind = kHILayoutBindRight;
  layoutInfo.binding.bottom.toView = contentView;
  layoutInfo.binding.bottom.kind = kHILayoutBindBottom;
  HIViewSetLayoutInfo(hiView, &layoutInfo);
  HIViewApplyLayout(hiView);

  // Setting mouse event handler
  EventHandlerUPP handler = NewEventHandlerUPP(EventHandler);
  EventTypeSpec spec[] = { { kEventClassMouse, kEventMouseDown },
                           { kEventClassMouse, kEventMouseUp },
                           { kEventClassMouse, kEventMouseDragged },
                           { kEventClassMouse, kEventMouseScroll } };
  InstallEventHandler(GetWindowEventTarget(window_), handler,
                      arraysize(spec), spec, view_.get(), NULL);
  [view_.get() setSendCommandInterface:command_sender_];
}

CandidateWindow::~CandidateWindow() {
  ::DisposeWindow(window_);
}

void CandidateWindow::SetSendCommandInterface(
    client::SendCommandInterface *send_command_interface) {
  command_sender_ = send_command_interface;
  [view_.get() setSendCommandInterface:send_command_interface];
}

void CandidateWindow::Hide() {
  if (window_) {
    ::HideWindow(window_);
    ::DisposeWindow(window_);
    window_ = NULL;
  }
}

void CandidateWindow::Show() {
  if (!window_) {
    InitCandidateWindow();
  }
  ::ShowWindow(window_);
}

const mozc::renderer::TableLayout *CandidateWindow::GetTableLayout() const {
  return [view_.get() tableLayout];
}

renderer::Size CandidateWindow::GetWindowSize() const {
  if (!window_) {
    LOG(ERROR) << "You have to intialize window beforehand";
    return renderer::Size(0, 0);
  }
  ::Rect globalBounds;
  ::GetWindowBounds(window_, kWindowContentRgn, &globalBounds);
  return renderer::Size(globalBounds.right - globalBounds.left,
                        globalBounds.bottom - globalBounds.top);
}

void CandidateWindow::SetCandidates(const Candidates &candidates) {
  if (candidates.candidate_size() == 0) {
    return;
  }

  if (!window_) {
    InitCandidateWindow();
  }
  [view_.get() setCandidates:&candidates];
  [view_.get() setNeedsDisplay:YES];
  NSSize size = [view_.get() updateLayout];
  ::SizeWindow(window_, size.width, size.height, YES);
}

void CandidateWindow::MoveWindow(const renderer::Point &point) {
  if (!window_) {
    InitCandidateWindow();
  }
  ::MoveWindow(window_, point.x, point.y, YES);
}
}  // namespace mozc::renderer::mac
}  // namespace mozc::renderer
}  // namespace mozc
