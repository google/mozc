// Copyright 2010, Google Inc.
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

#import "base/mac_process.h"

#import <Cocoa/Cocoa.h>

#include "base/base.h"
#include "base/const.h"
#include "base/mac_util.h"
#include "base/util.h"

namespace mozc {
namespace {
  const char kFileSchema[] = "file://";
}  // namespace

bool MacProcess::OpenBrowserForMac(const string &url) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSURL *nsURL = nil;
  if (url.find(kFileSchema) == 0) {
    // for making URL from "file://...", use fileURLWithPath
    const string filepath = url.substr(strlen(kFileSchema));
    NSString *nsStr = [[NSString alloc] initWithBytes:filepath.data()
                                        length:filepath.size()
                                        encoding:NSUTF8StringEncoding];
    nsURL = [NSURL fileURLWithPath:nsStr];
  } else {
    NSString *nsStr = [[NSString alloc] initWithBytes:url.data()
                                        length:url.size()
                                        encoding:NSUTF8StringEncoding];
    nsURL = [NSURL URLWithString:nsStr];
  }
  if (nsURL) {
    [[NSWorkspace sharedWorkspace] openURL:nsURL];
  }
  [pool drain];
  return true;
}

bool MacProcess::OpenApplication(const string &path) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSString *nsStr = [[NSString alloc] initWithBytes:path.data()
                                      length:path.size()
                                      encoding:NSUTF8StringEncoding];
  [[NSWorkspace sharedWorkspace] launchApplication:nsStr];
  [pool drain];
  return true;
}

namespace {
bool LaunchMozcToolInternal(const string &tool_name, const string &error_type) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  // FLAGS_error_type is used where FLAGS_mode is "error_message_dialog".
  setenv("FLAGS_error_type", error_type.c_str(), 1);

  // If normal expected tool_name is specified, we invoke specific application.
  NSString *appName = nil;
  NSString *toolAppPath = nil;
  if (tool_name == "about_dialog") {
    appName = @"AboutDialog.app";
  } else if (tool_name == "config_dialog") {
    appName = @"ConfigDialog.app";
  } else if (tool_name == "dictionary_tool") {
    appName = @"DictionaryTool.app";
  } else if (tool_name == "error_message_dialog") {
    appName = @"ErrorMessageDialog.app";
  } else if (tool_name == "word_register_dialog") {
    appName = @"WordRegisterDialog.app";
  }

  if (appName != nil) {
    toolAppPath = [[[NSBundle mainBundle] resourcePath]
                    stringByAppendingPathComponent:appName];
  } else {
    // Otherwise, we tries to invoke the application by settings FLAGS_mode.
    // use --fromenv option to specify tool name
    setenv("FLAGS_mode", tool_name.c_str(), 1);
    toolAppPath = [[[NSBundle mainBundle] resourcePath]
                    stringByAppendingPathComponent:@ kProductPrefix "Tool.app"];
  }

  bool succeeded =
      [[NSWorkspace sharedWorkspace] launchApplication:toolAppPath];
  [pool drain];
  return succeeded;
}
}  // namespace

bool MacProcess::LaunchMozcTool(const string &tool_name) {
  return LaunchMozcToolInternal(tool_name, "");
}

bool MacProcess::LaunchErrorMessageDialog(const string &error_type) {
  return LaunchMozcToolInternal("error_message_dialog", error_type);
}

}  // namespace mozc
