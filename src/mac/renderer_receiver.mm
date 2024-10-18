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

#import "mac/renderer_receiver.h"

#include "protocol/commands.pb.h"

@implementation RendererReceiver {
  /** The current active controller that handles events from the renderer process. */
  id<ControllerCallback> _currentController;

  /** NSConnection to communicate with the renderer process. */
  NSConnection *_rendererConnection;
}

- (id)initWithName:(NSString *)name {
  self = [super init];
  if (self) {
    // _rendererConnection receives IPC calls from the renderer process.
    // See: renderer/mac/mac_server_send_command.mm
    _rendererConnection = [[NSConnection alloc] init];
    [_rendererConnection setRootObject:self];
    [_rendererConnection registerName:name];
  }
  return self;
}

#pragma mark ServerCallback
// Methods inherited from the ServerCallback protocol (see: common.h).

// sendData is a method of the ServerCallback protocol.
- (void)sendData:(NSData *)data {
  if (!_currentController) {
    return;
  }

  mozc::commands::SessionCommand command;
  const int32_t length = static_cast<int32_t>([data length]);
  if (!command.ParseFromArray([data bytes], length)) {
    return;
  }
  [_currentController sendCommand:command];
}

// outputResult is a method of the ServerCallback protocol.
- (void)outputResult:(NSData *)result {
  mozc::commands::Output output;
  const int32_t length = static_cast<int32_t>([result length]);
  if (!output.ParseFromArray([result bytes], length)) {
    return;
  }

  [_currentController outputResult:output];
}

// setCurrentController is a method of the ServerCallback protocol.
- (void)setCurrentController:(id<ControllerCallback>)controller {
  _currentController = controller;
}
@end
