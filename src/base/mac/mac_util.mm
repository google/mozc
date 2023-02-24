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

#import "base/mac/mac_util.h"

#import <Foundation/Foundation.h>
#include <TargetConditionals.h>

#include <string>

#include "base/const.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/singleton.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else  // TARGET_OS_IPHONE
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/IOKitLib.h>
#include <launch.h>
#endif  // TARGET_OS_IPHONE

namespace mozc {
namespace {
const char kServerDirectory[] = "/Library/Input Methods/" kProductPrefix ".app/Contents/Resources";
#if TARGET_OS_OSX
const unsigned char kPrelauncherPath[] =
    "/Library/Input Methods/" kProductPrefix ".app/Contents/Resources/" kProductPrefix
    "Prelauncher.app";
#endif  // TARGET_OS_OSX

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const char kProjectPrefix[] = "com.google.inputmethod.Japanese.";
#elif defined(MOZC_BUILD)
const char kProjectPrefix[] = "org.mozc.inputmethod.Japanese.";
#else  // GOOGLE_JAPANESE_INPUT_BUILD, MOZC_BUILD
#error Unknown branding
#endif  // GOOGLE_JAPANESE_INPUT_BUILD, MOZC_BUILD

#if TARGET_OS_OSX
// Returns the reference of prelauncher login item.
// If the prelauncher login item does not exist this function returns nullptr.
// Otherwise you must release the reference.
LSSharedFileListItemRef GetPrelauncherLoginItem() {
  LSSharedFileListItemRef prelauncher_item = nullptr;
  scoped_cftyperef<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, kPrelauncherPath, strlen((const char *)kPrelauncherPath), true));
  if (!url.get()) {
    LOG(ERROR) << "CFURLCreateFromFileSystemRepresentation error: Cannot create CFURL object.";
    return nullptr;
  }

  scoped_cftyperef<LSSharedFileListRef> login_items(
      LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, nullptr));
  if (!login_items.get()) {
    LOG(ERROR) << "LSSharedFileListCreate error: Cannot get the login items.";
    return nullptr;
  }

  scoped_cftyperef<CFArrayRef> login_items_array(
      LSSharedFileListCopySnapshot(login_items.get(), nullptr));
  if (!login_items_array.get()) {
    LOG(ERROR) << "LSSharedFileListCopySnapshot error: Cannot get the login items.";
    return nullptr;
  }

  for (CFIndex i = 0; i < CFArrayGetCount(login_items_array.get()); ++i) {
    LSSharedFileListItemRef item = reinterpret_cast<LSSharedFileListItemRef>(
        const_cast<void *>(CFArrayGetValueAtIndex(login_items_array.get(), i)));
    if (!item) {
      LOG(ERROR) << "CFArrayGetValueAtIndex error: Cannot get the login item.";
      return nullptr;
    }

    CFURLRef item_url_ref = nullptr;
    if (LSSharedFileListItemResolve(item, 0, &item_url_ref, nullptr) == noErr) {
      if (!item_url_ref) {
        LOG(ERROR) << "LSSharedFileListItemResolve error: Cannot get the login item url.";
        return nullptr;
      }
      if (CFEqual(item_url_ref, url.get())) {
        prelauncher_item = item;
        CFRetain(prelauncher_item);
      }
    }
  }

  return prelauncher_item;
}
#endif  // TARGET_OS_OSX

std::string GetSearchPathForDirectoriesInDomains(NSSearchPathDirectory directory) {
  std::string dir;
  @autoreleasepool {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(directory, NSUserDomainMask, YES);
    if ([paths count] > 0) {
      dir.assign([[paths objectAtIndex:0] fileSystemRepresentation]);
    }
  }
  return dir;
}

}  // namespace

std::string MacUtil::GetLabelForSuffix(const absl::string_view suffix) {
  return absl::StrCat(kProjectPrefix, suffix);
}

std::string MacUtil::GetApplicationSupportDirectory() {
  return GetSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory);
}

std::string MacUtil::GetCachesDirectory() {
  return GetSearchPathForDirectoriesInDomains(NSCachesDirectory);
}

std::string MacUtil::GetLoggingDirectory() {
  std::string dir;
  @autoreleasepool {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
    if ([paths count] > 0) {
      dir.assign([[[[paths objectAtIndex:0] stringByAppendingPathComponent:@"Logs"]
          stringByAppendingPathComponent:@kProductPrefix] fileSystemRepresentation]);
    }
  }
  return dir;
}

std::string MacUtil::GetOSVersionString() {
  std::string version;
  @autoreleasepool {
    version.assign([[[NSProcessInfo processInfo] operatingSystemVersionString]
        cStringUsingEncoding:NSUTF8StringEncoding]);
  }
  return version;
}

std::string MacUtil::GetServerDirectory() { return kServerDirectory; }

std::string MacUtil::GetResourcesDirectory() {
  std::string result;
  @autoreleasepool {
    NSBundle *mainBundle = [NSBundle mainBundle];
    if (mainBundle) {
      NSString *resourcePath = [mainBundle resourcePath];
      if (resourcePath) {
        result.assign([resourcePath fileSystemRepresentation]);
      }
    }
  }
  return result;
}

#if TARGET_OS_IPHONE
std::string MacUtil::GetSerialNumber() {
  std::string result;
  NSString *const kSerialNumberNSString = [[UIDevice currentDevice].identifierForVendor UUIDString];
  if (kSerialNumberNSString != nil) {
    result.assign([kSerialNumberNSString UTF8String]);
  }
  return result;
}
#else   // TARGET_OS_IPHONE
std::string MacUtil::GetSerialNumber() {
  // Please refer to TN1103 for the details
  // http://developer.apple.com/library/mac/#technotes/tn/tn1103.html
  std::string result;
  io_service_t platformExpert = IOServiceGetMatchingService(
      kIOMasterPortDefault, IOServiceMatching("IOPlatformExpertDevice"));

  if (platformExpert) {
    CFTypeRef serialNumberAsCFString = IORegistryEntryCreateCFProperty(
        platformExpert, CFSTR(kIOPlatformSerialNumberKey), kCFAllocatorDefault, 0);
    if (serialNumberAsCFString) {
      NSString *serialNumberNSString = (__bridge NSString *)serialNumberAsCFString;
      result.assign([serialNumberNSString UTF8String]);
    }

    IOObjectRelease(platformExpert);
  }

  // Return the empty string if failed.
  return result;
}

bool MacUtil::StartLaunchdService(const std::string &service_name, pid_t *pid) {
  int dummy_pid = 0;
  if (pid == nullptr) {
    pid = &dummy_pid;
  }
  const std::string label = GetLabelForSuffix(service_name);

  launch_data_t start_renderer_command = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
  launch_data_dict_insert(start_renderer_command, launch_data_new_string(label.c_str()),
                          LAUNCH_KEY_STARTJOB);
  launch_data_t result_data = launch_msg(start_renderer_command);
  launch_data_free(start_renderer_command);
  if (result_data == nullptr) {
    LOG(ERROR) << "Failed to launch the specified service";
    return false;
  }
  launch_data_free(result_data);

  // Getting PID by using launch_msg API.
  launch_data_t get_renderer_info = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
  launch_data_dict_insert(get_renderer_info, launch_data_new_string(label.c_str()),
                          LAUNCH_KEY_GETJOB);
  launch_data_t renderer_info = launch_msg(get_renderer_info);
  launch_data_free(get_renderer_info);
  if (renderer_info == nullptr) {
    LOG(ERROR) << "Unexpected error: launchd doesn't return the data for the service.";
    return false;
  }

  launch_data_t pid_data = launch_data_dict_lookup(renderer_info, LAUNCH_JOBKEY_PID);
  if (pid_data == nullptr) {
    LOG(ERROR) << "Unexpected error: launchd response doesn't have PID";
    launch_data_free(renderer_info);
    return false;
  }
  *pid = launch_data_get_integer(pid_data);
  launch_data_free(renderer_info);
  return true;
}

bool MacUtil::CheckPrelauncherLoginItemStatus() {
  scoped_cftyperef<LSSharedFileListItemRef> prelauncher_item(GetPrelauncherLoginItem());
  return (prelauncher_item.get() != nullptr);
}

void MacUtil::RemovePrelauncherLoginItem() {
  scoped_cftyperef<LSSharedFileListItemRef> prelauncher_item(GetPrelauncherLoginItem());

  if (!prelauncher_item.get()) {
    DLOG(INFO) << "prelauncher_item not found.  Probably not registered yet.";
    return;
  }
  scoped_cftyperef<LSSharedFileListRef> login_items(
      LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, nullptr));
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
      LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, nullptr));
  if (!login_items.get()) {
    LOG(ERROR) << "LSSharedFileListCreate error: Cannot get the login items.";
    return;
  }
  scoped_cftyperef<CFURLRef> url(CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, kPrelauncherPath, strlen((const char *)kPrelauncherPath), true));

  if (!url.get()) {
    LOG(ERROR) << "CFURLCreateFromFileSystemRepresentation error:"
               << " Cannot create CFURL object.";
    return;
  }
  scoped_cftyperef<LSSharedFileListItemRef> new_item(LSSharedFileListInsertItemURL(
      login_items.get(), kLSSharedFileListItemLast, nullptr, nullptr, url.get(), nullptr, nullptr));
  if (!new_item.get()) {
    LOG(ERROR) << "LSSharedFileListInsertItemURL error:"
               << " Cannot insert the prelauncher to the login items.";
    return;
  }
}

bool MacUtil::GetFrontmostWindowNameAndOwner(std::string *name, std::string *owner) {
  DCHECK(name);
  DCHECK(owner);
  scoped_cftyperef<CFArrayRef> window_list(::CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID));
  const CFIndex window_count = CFArrayGetCount(window_list.get());
  for (CFIndex i = 0; i < window_count; ++i) {
    const NSDictionary *window_data =
        static_cast<const NSDictionary *>(CFArrayGetValueAtIndex(window_list.get(), i));
    if ([[window_data objectForKey:(id)kCGWindowSharingState] intValue] == kCGWindowSharingNone) {
      // Skips not shared window.
      continue;
    }
    NSString *window_name = [window_data objectForKey:(id)kCGWindowName];
    NSString *owner_name = [window_data objectForKey:(id)kCGWindowOwnerName];
    NSNumber *window_layer = [window_data objectForKey:(id)kCGWindowLayer];

    if ((window_name == nil) || (owner_name == nil) || (window_layer == nil)) {
      continue;
    }
    // Ignores the windows which aren't normal window level.
    if ([window_layer intValue] != kCGNormalWindowLevel) {
      continue;
    }

    // Hack to ignore the window (name == "" and owner == "Google Chrome")
    // Chrome browser seems to create a window which has no name in front of the
    // actual frontmost Chrome window.
    if ([window_name isEqualToString:@""] && [owner_name isEqualToString:@"Google Chrome"]) {
      continue;
    }
    name->assign([window_name UTF8String]);
    owner->assign([owner_name UTF8String]);
    return true;
  }
  return false;
}

bool MacUtil::IsSuppressSuggestionWindow(const std::string &name, const std::string &owner) {
  // TODO(horo): Make a function to check the name, then share it with the
  //             Windows client.
  // Currently we don't support "Firefox", because in Firefox "activateServer:"
  // of IMKStateSetting Protocol is not called when the user changes the
  // browsing tab.
  return (("Google Chrome" == owner) || ("Safari" == owner)) &&
         (("Google" == name) || absl::EndsWith(name, " - Google 検索") ||
          absl::EndsWith(name, " - Google Search"));
}
#endif  // TARGET_OS_IPHONE

}  // namespace mozc
