// Copyright 2010-2013, Google Inc.
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
#include <Cocoa/Cocoa.h>
#include <objc/message.h>

#include "base/base.h"
#include "base/coordinates.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "session/commands.pb.h"
#include "renderer/mac/RendererBaseWindow.h"


namespace mozc {
namespace renderer{
namespace mac {

namespace {
  // kWindowHighResolutionCapableAttribute is defined in HIToolbox/MacWindows.h
  // (version >= 10.7.4)
  // http://developer.apple.com/library/mac/#documentation/GraphicsAnimation/Conceptual/HighResolutionOSX/APIs/APIs.html
  // TODO(horo): Remove this line once we update the SDK version to 10.7.
  const WindowAttributes kWindowHighResolutionCapableAttribute = 1 << 20;
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

    NSEvent *actualEvent = [NSEvent mouseEventWithType:[event type]
                                              location:[event locationInWindow]
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

RendererBaseWindow::RendererBaseWindow()
    : window_(NULL){
}

void RendererBaseWindow::InitWindow() {
  if (window_ != NULL) {
    LOG(ERROR) << "window is already initialized.";
  }
  DLOG(INFO) << "RendererBaseWindow::InitWindow";
  // Creating Window
  ::Rect rect;
  rect.top = 0;
  rect.left = 0;
  rect.bottom = 1;
  rect.right = 1;
  WindowAttributes attributes = kWindowNoTitleBarAttribute |
                                kWindowCompositingAttribute |
                                kWindowStandardHandlerAttribute;
  if (MacUtil::OSVersionIsGreaterOrEqual(10, 7, 4)) {
    // kWindowHighResolutionCapableAttribute is available in 10.7.4 and later.
    attributes |= kWindowHighResolutionCapableAttribute;
  }
  CreateNewWindow(kUtilityWindowClass, attributes, &rect, &window_);

  // Changing the window level as same as overlay window class, which
  // means "top".
  WindowGroupRef groupRef = GetWindowGroup(window_);
  int groupLevel;
  GetWindowGroupLevelOfType(GetWindowGroupOfClass(kOverlayWindowClass),
                            kWindowGroupLevelPromoted, &groupLevel);
  SetWindowGroupLevel(groupRef, groupLevel);
  SetWindowGroupParent(groupRef, GetWindowGroupOfClass(kAllWindowClasses));

  ResetView();

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
}

RendererBaseWindow::~RendererBaseWindow() {
    ::DisposeWindow(window_);
}

Size RendererBaseWindow::GetWindowSize() const {
  if (!window_) {
    LOG(ERROR) << "You have to intialize window beforehand";
    return Size(0, 0);
  }
  ::Rect globalBounds;
  ::GetWindowBounds(window_, kWindowContentRgn, &globalBounds);
  return Size(globalBounds.right - globalBounds.left,
                        globalBounds.bottom - globalBounds.top);
}

void RendererBaseWindow::Hide() {
  if (window_) {
    ::HideWindow(window_);
    ::DisposeWindow(window_);
    window_ = NULL;
  }
}

void RendererBaseWindow::Show() {
  if (!window_) {
    InitWindow();
  }
  ::MoveWindow(window_, pos_.x, pos_.y, YES);
  ::ShowWindow(window_);
}

bool RendererBaseWindow::IsVisible() {
  if (!window_) {
    return false;
  }
  return (IsWindowVisible(window_) == YES);
}

void RendererBaseWindow::ResetView() {
  DLOG(INFO) << "RendererBaseWindow::ResetView()";
  view_.reset([[NSView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)]);
}

void RendererBaseWindow::MoveWindow(const Point &point) {
  DLOG(INFO) << "RendererBaseWindow::MoveWindow()";
  pos_ = point;
  if (!window_) {
    InitWindow();
  }
  ::MoveWindow(window_, point.x, point.y, YES);
}

}  // namespace mozc::renderer::mac
}  // namespace mozc::renderer
}  // namespace mozc
