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

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>
#import <InputMethodKit/InputMethodKit.h>

#import "mac/mozc_imk_input_controller.h"
#import "mac/renderer_receiver.h"

#include <memory>

#include "absl/flags/flag.h"
#include "absl/log/log.h"
#include "base/const.h"
#include "base/init_mozc.h"
#include "base/run_level.h"
#include "client/client.h"

ABSL_FLAG(bool, register_input_source, false,
          "Register and enable the input source in the system, then exit.");

namespace {

// Registers the input source bundle with the Text Input Services (TIS)
// and enables the bundle, which automatically enables all default input modes
// (Hiragana, Katakana, Roman, etc.) defined in Info.plist, then selects the
// bundle so that the primary input mode (Hiragana) is active.
// This is intended to be called by the postflight installer script so that
// the input source appears enabled in the system and menu bar after login.
void RegisterInputSource() {
  NSBundle *bundle = [NSBundle mainBundle];
  CFURLRef bundleURL = (__bridge CFURLRef)[bundle bundleURL];
  if (bundleURL != nullptr) {
    TISRegisterInputSource(bundleURL);
  }

  NSString *bundleID = [bundle bundleIdentifier];
  if (bundleID == nil) {
    return;
  }

  NSArray *sourceList = CFBridgingRelease(TISCreateInputSourceList(nullptr, true));
  for (id object in sourceList) {
    TISInputSourceRef inputSource = (__bridge TISInputSourceRef)object;
    NSString *sourceID =
        (__bridge NSString *)TISGetInputSourceProperty(inputSource, kTISPropertyInputSourceID);
    if ([sourceID isEqualToString:bundleID]) {
      TISEnableInputSource(inputSource);
      TISSelectInputSource(inputSource);
      break;
    }
  }
}

}  // namespace

int main(int argc, char *argv[]) {
  mozc::InitMozc(argv[0], &argc, &argv);

  if (absl::GetFlag(FLAGS_register_input_source)) {
    RegisterInputSource();
    return 0;
  }

  if (!mozc::RunLevel::IsValidClientRunLevel()) {
    return -1;
  }

  // Initialize imkServer
  NSBundle *bundle = [NSBundle mainBundle];
  NSDictionary *infoDictionary = [bundle infoDictionary];
  NSString *connectionName = [infoDictionary objectForKey:@"InputMethodConnectionName"];
  IMKServer *imkServer = [[IMKServer alloc] initWithName:connectionName
                                        bundleIdentifier:[bundle bundleIdentifier]];
  if (!imkServer) {
    LOG(FATAL) << mozc::kProductNameInEnglish << " failed to initialize";
    return -1;
  }
  DLOG(INFO) << mozc::kProductNameInEnglish << " initialized";

  NSString *rendererConnectionName = @kProductPrefix "_Renderer_Connection";
  RendererReceiver *rendererReceiver =
      [[RendererReceiver alloc] initWithName:rendererConnectionName];
  [MozcImkInputController setGlobalRendererReceiver:rendererReceiver];

  // Start the converter server at this time explicitly to prevent the
  // slow-down of the response for initial key event.
  {
    std::unique_ptr<mozc::client::Client> client(new mozc::client::Client);
    client->PingServer();
  }
  NSApplicationMain(argc, (const char **)argv);
  return 0;
}
