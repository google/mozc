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

#import <Foundation/Foundation.h>

#include "protocol/commands.pb.h"

/** ControllerCallback is the collection of methods to send events from RendererReceiver to
 * MozcImkInputController.
 *
 * This protocol is designed to be used in the following way:
 * 1. MozcImkInputController implements this protocol and registers itself to RendererReceiver.
 * 2. RendererReceiver sends a SessionCommand to MozcImkInputController via this protocol.
 *
 * Note, MozcImkInputController is instanced per host application, and RendererReceiver is a
 * singleton.
 */
@protocol ControllerCallback

/** Sends a SessionCommand to the controller.
 *
 * @param command a SessionCommand protobuf message.
 */
- (void)sendCommand:(const mozc::commands::SessionCommand &)command;

/** Sends a result output to the controller.
 * This method could be called from some utility tools (e.g. handwriting, voice input)
 *
 * @param data an Output protobuf message.
 */
- (void)outputResult:(const mozc::commands::Output &)output;
@end

/** ServerCallback is a protocol to send the events (e.g. mouse click of a candidate word)
 * from the renderer process to the IMKController process via IPC (i.e. NSConnection).
 */
@protocol ServerCallback

/** Is called when a user clicks an item in a candidate window or when the renderer sends the usage
 * stats event information.
 *
 * @param data A serialized mozc::commands::SessionCommand protobuf message.
 */
- (void)sendData:(NSData *)data;

/** Registers the current controller.
 *
 * @param controller The current controller.
 */
- (void)setCurrentController:(id<ControllerCallback>)controller;

/** Is called to output the result to the host application via the IMKController.
 *
 * @param result A serialized mozc::commands::Output protobuf message.
 */
- (void)outputResult:(NSData *)result;
@end
