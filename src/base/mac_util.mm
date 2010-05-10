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

#import "base/mac_util.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#include <launch.h>

#include "base/util.h"

namespace mozc {
namespace {
const char kServerDirectory[] =
    "/Library/Input Methods/GoogleJapaneseInput.app/Contents/Resources";
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const char kProjectPrefix[] =
    "com.google.inputmethod.Japanese.";
#elif defined(MOZC_BUILD)
const char kProjectPrefix[] =
    "org.mozc.inputmethod.Japanese.";
#else
#error Unknown branding
#endif
}  // anonymous namespace

string MacUtil::GetLabelForSuffix(const string &suffix) {
  return string(kProjectPrefix) + suffix;
}

string MacUtil::GetApplicationSupportDirectory() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  string dir;
  NSArray *paths = NSSearchPathForDirectoriesInDomains(
      NSApplicationSupportDirectory, NSUserDomainMask, YES);
  if ([paths count] > 0) {
    dir.assign([[paths objectAtIndex:0] fileSystemRepresentation]);
  }
  [pool drain];
  return dir;
}

string MacUtil::GetLoggingDirectory() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  string dir;
  NSArray *paths = NSSearchPathForDirectoriesInDomains(
      NSLibraryDirectory, NSUserDomainMask, YES);
  if ([paths count] > 0) {
    dir.assign(
        [[[[paths objectAtIndex:0] stringByAppendingPathComponent:@"Logs"]
           stringByAppendingPathComponent:@"GoogleJapaneseInput"]
          fileSystemRepresentation]);
  }
  [pool drain];
  return dir;
}

string MacUtil::GetOSVersionString() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  string version;
  version.assign([[[NSProcessInfo processInfo] operatingSystemVersionString]
                  cStringUsingEncoding : NSUTF8StringEncoding]);
  [pool drain];
  return version;
}

string MacUtil::GetServerDirectory() {
  return kServerDirectory;
}

bool MacUtil::StartLaunchdServce(const string &service_name,
                                 pid_t *pid) {
  int dummy_pid = 0;
  if (pid == NULL) {
    pid = &dummy_pid;
  }
  const string label = GetLabelForSuffix(service_name);

  launch_data_t start_renderer_command =
      launch_data_alloc(LAUNCH_DATA_DICTIONARY);
  launch_data_dict_insert(start_renderer_command,
                          launch_data_new_string(label.c_str()),
                          LAUNCH_KEY_STARTJOB);
  launch_data_t result_data = launch_msg(start_renderer_command);
  launch_data_free(start_renderer_command);
  if (result_data == NULL) {
    LOG(ERROR) << "Failed to launch the specified service";
    return false;
  }
  launch_data_free(result_data);

  // Getting PID by using launch_msg API.
  launch_data_t get_renderer_info =
      launch_data_alloc(LAUNCH_DATA_DICTIONARY);
  launch_data_dict_insert(get_renderer_info,
                          launch_data_new_string(label.c_str()),
                          LAUNCH_KEY_GETJOB);
  launch_data_t renderer_info = launch_msg(get_renderer_info);
  launch_data_free(get_renderer_info);
  if (renderer_info == NULL) {
    LOG(ERROR) << "Unexpected error: launchd doesn't return the data "
               << "for the service.";
    return false;
  }

  launch_data_t pid_data = launch_data_dict_lookup(
      renderer_info, LAUNCH_JOBKEY_PID);
  if (pid_data == NULL) {
    LOG(ERROR) <<
        "Unexpected error: launchd response doesn't have PID";
    launch_data_free(renderer_info);
    return false;
  }
  *pid = launch_data_get_integer(pid_data);
  launch_data_free(renderer_info);
  return true;
}
}  // namespace mozc
