// Copyright 2010-2012, Google Inc.
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
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include "base/const.h"
#include "base/scoped_cftyperef.h"
#include "base/singleton.h"
#include "base/util.h"

namespace mozc {
namespace {
const char kServerDirectory[] =
    "/Library/Input Methods/" kProductPrefix ".app/Contents/Resources";
const unsigned char kPrelauncherPath[] =
    "/Library/Input Methods/" kProductPrefix ".app/Contents/Resources/"
    kProductPrefix "Prelauncher.app";

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const char kProjectPrefix[] =
    "com.google.inputmethod.Japanese.";
#elif defined(MOZC_BUILD)
const char kProjectPrefix[] =
    "org.mozc.inputmethod.Japanese.";
#else
#error Unknown branding
#endif

// Returns the reference of prelauncher login item.
// If the prelauncher login item does not exist this function returns NULL.
// Otherwise you must release the reference.
LSSharedFileListItemRef GetPrelauncherLoginItem() {
  LSSharedFileListItemRef prelauncher_item = NULL;
  scoped_cftyperef<CFURLRef> url(
    CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, kPrelauncherPath,
        strlen((const char *)kPrelauncherPath), true));
  if (!url.get()) {
    LOG(ERROR) << "CFURLCreateFromFileSystemRepresentation error:"
               << " Cannot create CFURL object.";
    return NULL;
  }

  scoped_cftyperef<LSSharedFileListRef> login_items(
      LSSharedFileListCreate(
          kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL));
  if (!login_items.get()) {
    LOG(ERROR) << "LSSharedFileListCreate error: Cannot get the login items.";
    return NULL;
  }

  scoped_cftyperef<CFArrayRef> login_items_array(
      LSSharedFileListCopySnapshot(login_items.get(), NULL));
  if (!login_items_array.get()) {
    LOG(ERROR) << "LSSharedFileListCopySnapshot error:"
               << " Cannot get the login items.";
    return NULL;
  }

  for(CFIndex i = 0; i < CFArrayGetCount(login_items_array.get()); ++i) {
    LSSharedFileListItemRef item =
        reinterpret_cast<LSSharedFileListItemRef>(const_cast<void *>(
            CFArrayGetValueAtIndex(login_items_array.get(), i)));
    if (!item) {
      LOG(ERROR) << "CFArrayGetValueAtIndex error:"
                 << " Cannot get the login item.";
      return NULL;
    }

    CFURLRef item_url_ref = NULL;
    if (LSSharedFileListItemResolve(item, 0, &item_url_ref, NULL) == noErr) {
      if (!item_url_ref) {
        LOG(ERROR) << "LSSharedFileListItemResolve error:"
                   << " Cannot get the login item url.";
        return NULL;
      }
      if (CFEqual(item_url_ref, url.get())) {
        prelauncher_item = item;
        CFRetain(prelauncher_item);
      }
    }
  }

  return prelauncher_item;
}
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
           stringByAppendingPathComponent:@kProductPrefix]
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

namespace {

class OSVersionCache {
 public:
  OSVersionCache() : succeeded_(false) {
    // TODO(horo): Gestalt function is deprecated in OS X v10.8.
    //             Consider how to get the version after 10.8.
    if (Gestalt(gestaltSystemVersionMajor, &major_version_) == noErr &&
        Gestalt(gestaltSystemVersionMinor, &minor_version_) == noErr &&
        Gestalt(gestaltSystemVersionBugFix, &fix_version_) == noErr) {
      succeeded_ = true;
    }
  }
  bool GetOSVersion(int32 *major, int32 *minor, int32 *fix) const {
    if (!succeeded_) {
      return false;
    }
    *major = major_version_;
    *minor = minor_version_;
    *fix = fix_version_;
    return true;
  }

 private:
  SInt32 major_version_;
  SInt32 minor_version_;
  SInt32 fix_version_;
  bool succeeded_;
};

}  // namespace

bool MacUtil::GetOSVersion(int32 *major, int32 *minor, int32 *fix) {
  return Singleton<OSVersionCache>::get()->GetOSVersion(major, minor, fix);
}

bool MacUtil::OSVersionIsGreaterOrEqual(int32 major, int32 minor, int32 fix) {
  int32 major_version = 0;
  int32 minor_version = 0;
  int32 fix_version = 0;

  if (!GetOSVersion(&major_version, &minor_version, &fix_version)) {
    return false;
  }
  if ((major_version > major) ||
      ((major_version == major) &&
           ((minor_version > minor) ||
            ((minor_version == minor) && (fix_version >= fix))))) {
    return true;
  }
  return false;
}

string MacUtil::GetServerDirectory() {
  return kServerDirectory;
}

string MacUtil::GetResourcesDirectory() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  string result;

  NSBundle *mainBundle = [NSBundle mainBundle];
  if (mainBundle) {
    NSString *resourcePath = [mainBundle resourcePath];
    if (resourcePath) {
      result.assign([resourcePath fileSystemRepresentation]);
    }
  }
  [pool drain];
  return result;
}

string MacUtil::GetSerialNumber() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  // Please refer to TN1103 for the details
  // http://developer.apple.com/library/mac/#technotes/tn/tn1103.html
  string result;
  io_service_t platformExpert = IOServiceGetMatchingService(
      kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));

  if (platformExpert) {
    CFTypeRef serialNumberAsCFString =
        IORegistryEntryCreateCFProperty(
            platformExpert, CFSTR(kIOPlatformSerialNumberKey),
            kCFAllocatorDefault, 0);
    if (serialNumberAsCFString) {
      const NSString *serialNumberNSString = reinterpret_cast<const NSString *>(
          serialNumberAsCFString);
      result.assign([serialNumberNSString UTF8String]);
    }

    IOObjectRelease(platformExpert);
  }

  [pool drain];
  // Return the empty string if failed.
  return result;
}

bool MacUtil::StartLaunchdService(const string &service_name,
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

bool MacUtil::CheckPrelauncherLoginItemStatus() {
  scoped_cftyperef<LSSharedFileListItemRef> prelauncher_item(
      GetPrelauncherLoginItem());
  return (prelauncher_item.get() != NULL);
}

void MacUtil::RemovePrelauncherLoginItem() {
  scoped_cftyperef<LSSharedFileListItemRef> prelauncher_item(
      GetPrelauncherLoginItem());

  if (!prelauncher_item.get()) {
    DLOG(INFO) << "prelauncher_item not found.  Probably not registered yet.";
    return;
  }
  scoped_cftyperef<LSSharedFileListRef> login_items(
      LSSharedFileListCreate(
          kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL));
  if (!login_items.get()) {
    LOG(ERROR) << "LSSharedFileListCreate error: Cannot get the login items.";
    return;
  }
  LSSharedFileListItemRemove(login_items.get(), prelauncher_item.get());
}

void MacUtil::AddPrelauncherLoginItem() {
  if (CheckPrelauncherLoginItemStatus()) {
    return;
  }
  scoped_cftyperef<LSSharedFileListRef> login_items(
      LSSharedFileListCreate(
          kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, NULL));
  if (!login_items.get()) {
    LOG(ERROR) << "LSSharedFileListCreate error: Cannot get the login items.";
    return;
  }
  scoped_cftyperef<CFURLRef> url(
    CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault, kPrelauncherPath,
        strlen((const char *)kPrelauncherPath), true));

  if (!url.get()) {
    LOG(ERROR) << "CFURLCreateFromFileSystemRepresentation error:"
               << " Cannot create CFURL object.";
    return;
  }
  scoped_cftyperef<LSSharedFileListItemRef> new_item(
      LSSharedFileListInsertItemURL(
          login_items.get(), kLSSharedFileListItemLast, NULL, NULL, url.get(),
          NULL, NULL));
  if (!new_item.get()) {
    LOG(ERROR) << "LSSharedFileListInsertItemURL error:"
               << " Cannot insert the prelauncher to the login items.";
    return;
  }
}
}  // namespace mozc
