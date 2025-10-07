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

#import "ActivatePane.h"

#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>

// Set kUseUsageStats to control usage stats.
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
constexpr bool kUseUsageStats = true;
#else   // GOOGLE_JAPANESE_INPUT_BUILD
constexpr bool kUseUsageStats = false;
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
static const unsigned char kInstalledLocation[] = "/Library/Input Methods/GoogleJapaneseInput.app";
static NSString *kLaunchdPlistFiles[] = {
    @"/Library/LaunchAgents/com.google.inputmethod.Japanese.Converter.plist",
    @"/Library/LaunchAgents/com.google.inputmethod.Japanese.Renderer.plist", nil};
static NSString *const kSourceID = @"com.google.inputmethod.Japanese";
#else   // GOOGLE_JAPANESE_INPUT_BUILD
static const unsigned char kInstalledLocation[] = "/Library/Input Methods/Mozc.app";
static NSString *kLaunchdPlistFiles[] = {
    @"/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Converter.plist",
    @"/Library/LaunchAgents/org.mozc.inputmethod.Japanese.Renderer.plist", nil};
static NSString *const kSourceID = @"org.mozc.inputmethod.Japanese";
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

// Load the installed Google Japanese Input to the system.
static void RegisterGoogleJapaneseInput() {
  CFURLRef installedLocationURL = CFURLCreateFromFileSystemRepresentation(
      nullptr, kInstalledLocation, strlen((const char *)kInstalledLocation), NO);
  if (installedLocationURL) {
    TISRegisterInputSource(installedLocationURL);
  }
}

// Put modes of Google Japanese Input at the list of pull-down menu
// for IMEs, and make it as the default IME.
static void ActivateGoogleJapaneseInput() {
  CFArrayRef sourceList = TISCreateInputSourceList(nullptr, true);
  for (int i = 0; i < CFArrayGetCount(sourceList); ++i) {
    TISInputSourceRef inputSource = (TISInputSourceRef)(CFArrayGetValueAtIndex(sourceList, i));
    NSString *sourceID =
        (__bridge NSString *)(TISGetInputSourceProperty(inputSource, kTISPropertyInputSourceID));
    if ([sourceID isEqualToString:kSourceID]) {
      TISEnableInputSource(inputSource);
      TISSelectInputSource(inputSource);
      break;
    }
  }
}

// Load the launchd preferences
static void LoadLaunchdPlistFiles() {
  int i = 0;
  for (; kLaunchdPlistFiles[i] != nil; ++i) {
    NSArray *arguments =
        [NSArray arrayWithObjects:@"load", @"-S", @"Aqua", kLaunchdPlistFiles[i], nil];
    NSTask *task = [NSTask launchedTaskWithLaunchPath:@"/bin/launchctl" arguments:arguments];
    [task waitUntilExit];
  }
}

// Check if Google Japanese Input is already active on the user
// machine (e.g. the user already installed Google Japanese Input).
// Called at the initialization of this module.
static BOOL IsAlreadyActive() {
  BOOL isActive = NO;
  CFArrayRef sourceList = TISCreateInputSourceList(nullptr, true);
  for (int i = 0; i < CFArrayGetCount(sourceList); ++i) {
    TISInputSourceRef inputSource = (TISInputSourceRef)(CFArrayGetValueAtIndex(sourceList, i));
    NSString *sourceID =
        (__bridge NSString *)(TISGetInputSourceProperty(inputSource, kTISPropertyInputSourceID));
    if ([sourceID isEqualToString:kSourceID]) {
      CFBooleanRef isEnabled =
          (CFBooleanRef)(TISGetInputSourceProperty(inputSource, kTISPropertyInputSourceIsEnabled));
      CFBooleanRef isSelected =
          (CFBooleanRef)(TISGetInputSourceProperty(inputSource, kTISPropertyInputSourceIsEnabled));
      if (CFBooleanGetValue(isEnabled) || CFBooleanGetValue(isSelected)) {
        isActive = YES;
        break;
      }
    }
  }
  return isActive;
}

// Check if the usagestats.db file already exists.  Called at the
// initialization of this module.
static BOOL HasUsageStatsDB() {
  // If channel is dev, we don't want to show the "turn-on usage
  // stats" button, so the situation is same as that there are already
  // usage stats config file.
#ifndef GOOGLE_JAPANESE_INPUT_BUILD
  return NO;
#elif defined(CHANNEL_DEV)
  return YES;
#endif  // CHANNEL_DEV
  NSArray *libraryPaths =
      NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSString *libraryPath = nil;
  NSString *usageStatsDBPath = nil;
  BOOL exists = NO;
  if (libraryPaths == nil || [libraryPaths count] == 0) {
    return NO;
  }
  libraryPath = [libraryPaths objectAtIndex:0];
  usageStatsDBPath = [[[[libraryPath stringByAppendingPathComponent:@"Application Support"]
      stringByAppendingPathComponent:@"Google"] stringByAppendingPathComponent:@"JapaneseInput"]
      stringByAppendingPathComponent:@".usagestats.db"];
  exists = [fileManager fileExistsAtPath:usageStatsDBPath];
  return exists;
}

// Put the usagestats file of sending report onto the user's config
// directory.  This function should be called only if the user checks
// the |_putUsageStatsDB| button.
static BOOL StoreDefaultConfigWithSendingUsageStats() {
  if (!kUseUsageStats) {
    return NO;
  }

  NSArray *libraryPaths =
      NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, YES);
  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSString *libraryPath = nil;
  NSString *googlePath = nil;
  NSString *japaneseInputPath = nil;
  NSString *usageStatsDBPath = nil;
  if (libraryPaths == nil || [libraryPaths count] == 0) {
    return NO;
  }

  libraryPath = [libraryPaths objectAtIndex:0];
  googlePath = [[libraryPath stringByAppendingPathComponent:@"Application Support"]
      stringByAppendingPathComponent:@"Google"];
  if (![fileManager fileExistsAtPath:googlePath]) {
    NSDictionary *defaultAttributes =
        [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:0755]
                                    forKey:NSFilePosixPermissions];
    if (![fileManager createDirectoryAtPath:googlePath
                withIntermediateDirectories:YES
                                 attributes:defaultAttributes
                                      error:nullptr]) {
      return NO;
    }
  }

  japaneseInputPath = [googlePath stringByAppendingPathComponent:@"JapaneseInput"];
  if (![fileManager fileExistsAtPath:japaneseInputPath]) {
    NSDictionary *japaneseInputAttributes =
        [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:0700]
                                    forKey:NSFilePosixPermissions];
    if (![fileManager createDirectoryAtPath:japaneseInputPath
                withIntermediateDirectories:YES
                                 attributes:japaneseInputAttributes
                                      error:nullptr]) {
      return NO;
    }
  }

  usageStatsDBPath = [japaneseInputPath stringByAppendingPathComponent:@".usagestats.db"];
  if (![fileManager fileExistsAtPath:usageStatsDBPath]) {
    // The value of usage stats is a 32-bit int and 1 means "sending
    // the usage stats to Google".
    const int sending = 1;
    NSDictionary *usageStatsDBAttributes =
        [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:0400]
                                    forKey:NSFilePosixPermissions];
    if ([fileManager createFileAtPath:usageStatsDBPath
                             contents:[NSData dataWithBytes:&sending length:4]
                           attributes:usageStatsDBAttributes]) {
      return YES;
    }
  }

  return NO;
}

@interface ActivatePane ()
// Returns localized string for the key in the bundle.  Returns nil if
// not found.  This is a wrapper of [NSBundle localizedStringForKey:]
// which returns the key if not found.
- (NSString *)localizedStringForKey:(NSString *)key;
@end

@implementation ActivatePane
- (id)initWithSection:(id)parent {
  self = [super initWithSection:parent];
  _alreadyActivated = NO;
  _isUpgrade = NO;
  _hasUsageStatsDB = NO;
  return self;
}

// This method is called once when this module is loaded.
- (void)awakeFromNib {
  _isUpgrade = IsAlreadyActive();
  _hasUsageStatsDB = _isUpgrade || HasUsageStatsDB();
  if (_isUpgrade) {
    _alreadyActivated = YES;
  }
}

- (NSString *)title {
  NSString *titleString = [self localizedStringForKey:@"Title"];
  if (titleString) {
    return titleString;
  }
  return @"Automatic Activation";
}

// This method is called when the pane is visible.
- (void)willEnterPane:(InstallerSectionDirection)dir {
  // Main messages
  NSString *mainMessage = [self localizedStringForKey:@"mainMessage"];
  if (mainMessage) {
    [_mainMessage setStringValue:mainMessage];
  }

  // Upgrade Message. Blank if it's not upgrade.
  if (_isUpgrade) {
    NSString *upgradeMessage = [self localizedStringForKey:@"upgradeMessage"];
    if (upgradeMessage) {
      [_upgradeMessage setStringValue:upgradeMessage];
    }
  } else {
    [_upgradeMessage setStringValue:@""];
  }

  // Button label for "activate"
  NSString *activateLabel = [self localizedStringForKey:@"activateLabel"];
  if (activateLabel) {
    [_activateCell setTitle:activateLabel];
  }

  // Button label for "do not activate"
  NSString *doNotActivateLabel = [self localizedStringForKey:@"doNotActivateLabel"];
  if (doNotActivateLabel) {
    [_doNotActivateCell setTitle:doNotActivateLabel];
  }

  // usage stats message
  // Note, setHidden is used instead of removeFromSuperview because removeFromSuperview requires
  // additional logic for memory management. Otherwise it may result crash of installer.
#ifndef GOOGLE_JAPANESE_INPUT_BUILD
  [_putUsageStatsDB setHidden:YES];
  [_putUsageStatsDBMessage setHidden:YES];
#else   // GOOGLE_JAPANESE_INPUT_BUILD
  if (!_hasUsageStatsDB) {
    NSString *usageStatsMessage = [self localizedStringForKey:@"usageStatsMessage"];
    if (usageStatsMessage) {
      [_putUsageStatsDBMessage setStringValue:usageStatsMessage];
    }
  } else {
    [_putUsageStatsDB setHidden:YES];
    [_putUsageStatsDBMessage setHidden:YES];
  }
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

  if (_alreadyActivated) {
    [_mainMessage setEnabled:NO];
    [_activateCell setEnabled:NO];
    [_doNotActivateCell setEnabled:NO];
  }
}

// This method is called when the user leave the pane.
- (void)willExitPane:(InstallerSectionDirection)dir {
  if (dir == InstallerDirectionForward) {
    LoadLaunchdPlistFiles();
  }

  if (!_alreadyActivated && dir == InstallerDirectionForward) {
    // Make the package visible from i18n system preference
    RegisterGoogleJapaneseInput();
    if ([_activateCell state] == NSControlStateValueOn &&
        [_doNotActivateCell state] == NSControlStateValueOff) {
      // means clicks "next page" when "activate" menu is on
      ActivateGoogleJapaneseInput();
      _alreadyActivated = YES;
    }

    if (kUseUsageStats && !_hasUsageStatsDB && [_putUsageStatsDB state] == NSControlStateValueOn) {
      StoreDefaultConfigWithSendingUsageStats();
    }
  }
}

- (NSString *)localizedStringForKey:(NSString *)key {
  NSBundle *bundle = [NSBundle bundleForClass:[self class]];
  if (!bundle) {
    return nil;
  }

  NSString *value = [bundle localizedStringForKey:key value:nil table:nil];
  // value is same as "key" if failed.
  if ([value isEqualToString:key]) {
    return nil;
  }
  return value;
}
@end
