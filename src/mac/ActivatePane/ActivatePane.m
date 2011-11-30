//
//  ActivatePane.m
//  GoogleJapaneseInput
//
//  Created by Jun Mukai on 09/10/08.
//  Copyright 2009 Google Inc. All rights reserved.
//

#import "ActivatePane.h"

#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>

static const unsigned char kInstalledLocation[] =
    "/Library/Input Methods/GoogleJapaneseInput.app";
static NSString *kLaunchdPlistFiles[] = {
  @"/Library/LaunchAgents/com.google.inputmethod.Japanese.Converter.plist",
  @"/Library/LaunchAgents/com.google.inputmethod.Japanese.Renderer.plist",
  nil};

// Load the installed Google Japanese Input to the system.
static void RegisterGoogleJapaneseInput() {
  CFURLRef installedLocationURL = CFURLCreateFromFileSystemRepresentation(
      NULL,  kInstalledLocation, strlen((const char *)kInstalledLocation), NO);
  if (installedLocationURL) {
    TISRegisterInputSource(installedLocationURL);
  }
}

// Put modes of Google Japanese Input at the list of pull-down menu
// for IMEs, and make it as the default IME.
static void ActivateGoogleJapaneseInput() {
  CFArrayRef sourceList = TISCreateInputSourceList(NULL, true);
  for (int i = 0; i < CFArrayGetCount(sourceList); ++i) {
    TISInputSourceRef inputSource = (TISInputSourceRef)(CFArrayGetValueAtIndex(
        sourceList, i));
    NSString *sourceID = (NSString *)(TISGetInputSourceProperty(
        inputSource, kTISPropertyInputSourceID));
    if ([sourceID isEqualToString:@"com.google.inputmethod.Japanese"]) {
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
        [NSArray arrayWithObjects:@"load", @"-S", @"Aqua",
                 kLaunchdPlistFiles[i], nil];
    NSTask *task = [NSTask launchedTaskWithLaunchPath:@"/bin/launchctl"
                                            arguments:arguments];
    [task waitUntilExit];
  }
}

// Check if Google Japanese Input is already active on the user
// machine (e.g. the user already installed Google Japanese Input).
// Called at the initialization of this module.
static BOOL IsAlreadyActive() {
  BOOL isActive = NO;
  CFArrayRef sourceList = TISCreateInputSourceList(NULL, true);
  for (int i = 0; i < CFArrayGetCount(sourceList); ++i) {
    TISInputSourceRef inputSource = (TISInputSourceRef)(CFArrayGetValueAtIndex(
        sourceList, i));
    NSString *sourceID = (NSString *)(TISGetInputSourceProperty(
        inputSource, kTISPropertyInputSourceID));
    if ([sourceID isEqualToString:@"com.google.inputmethod.Japanese"]) {
      CFBooleanRef isEnabled = (CFBooleanRef)(TISGetInputSourceProperty(
          inputSource, kTISPropertyInputSourceIsEnabled));
      CFBooleanRef isSelected = (CFBooleanRef)(TISGetInputSourceProperty(
          inputSource, kTISPropertyInputSourceIsEnabled));
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
#ifdef CHANNEL_DEV
  return YES;
#endif  // CHANNEL_DEV
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSArray *libraryPaths = NSSearchPathForDirectoriesInDomains(
      NSLibraryDirectory, NSUserDomainMask, YES);
  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSString *libraryPath = nil;
  NSString *usageStatsDBPath = nil;
  BOOL exists = NO;
  if (libraryPaths == nil || [libraryPaths count] == 0) {
    [pool drain];
    return NO;
  }
  libraryPath = [libraryPaths objectAtIndex:0];
  usageStatsDBPath =
      [[[[libraryPath stringByAppendingPathComponent:@"Application Support"]
          stringByAppendingPathComponent:@"Google"]
        stringByAppendingPathComponent:@"JapaneseInput"]
        stringByAppendingPathComponent:@".usagestats.db"];
  exists = [fileManager fileExistsAtPath:usageStatsDBPath];
  [pool drain];
  return exists;
}

// Put the usagestats file of sending report onto the user's config
// directory.  This function should be called only if the user checks
// the |_putUsageStatsDB| button.
static BOOL StoreDefaultConfigWithSendingUsageStats() {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  NSArray *libraryPaths = NSSearchPathForDirectoriesInDomains(
      NSLibraryDirectory, NSUserDomainMask, YES);
  NSFileManager *fileManager = [NSFileManager defaultManager];
  NSString *libraryPath = nil;
  NSString *googlePath = nil;
  NSString *japaneseInputPath = nil;
  NSString *usageStatsDBPath = nil;
  if (libraryPaths == nil || [libraryPaths count] == 0) {
    [pool drain];
    return NO;
  }

  libraryPath = [libraryPaths objectAtIndex:0];
  googlePath =
      [[libraryPath stringByAppendingPathComponent:@"Application Support"]
        stringByAppendingPathComponent:@"Google"];
  if (![fileManager fileExistsAtPath:googlePath]) {
    NSDictionary *defaultAttributes =
        [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:0755]
                                    forKey:NSFilePosixPermissions];
    if (![fileManager createDirectoryAtPath:googlePath
                withIntermediateDirectories:YES
                                 attributes:defaultAttributes
                                      error:NULL]) {
      [pool drain];
      return NO;
    }
  }

  japaneseInputPath =
      [googlePath stringByAppendingPathComponent:@"JapaneseInput"];
  if (![fileManager fileExistsAtPath:japaneseInputPath]) {
    NSDictionary *japaneseInputAttributes =
        [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:0700]
                                    forKey:NSFilePosixPermissions];
    if (![fileManager createDirectoryAtPath:japaneseInputPath
                withIntermediateDirectories:YES
                                 attributes:japaneseInputAttributes
                                      error:NULL]) {
      [pool drain];
      return NO;
    }
  }

  usageStatsDBPath =
      [japaneseInputPath stringByAppendingPathComponent:@".usagestats.db"];
  if (![fileManager fileExistsAtPath:usageStatsDBPath]) {
    // The value of usage stats is a 32-bit int and 1 means "sending
    // the usage stats to Google".
    const int sending = 1;
    NSDictionary *usageStatsDBAttributes =
        [NSDictionary dictionaryWithObject:[NSNumber numberWithInt:0400]
                                    forKey:NSFilePosixPermissions];
    if ([fileManager createFileAtPath:usageStatsDBPath
                             contents:[NSData dataWithBytes:&sending
                                                     length:4]
                           attributes:usageStatsDBAttributes]) {
      [pool drain];
      return YES;
    }
  }

  [pool drain];
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
  NSString *doNotActivateLabel =
      [self localizedStringForKey:@"doNotActivateLabel"];
  if (doNotActivateLabel) {
    [_doNotActivateCell setTitle:doNotActivateLabel];
  }

  // usage stats message
  if (!_hasUsageStatsDB) {
    NSString *usageStatsMessage =
        [self localizedStringForKey:@"usageStatsMessage"];
    if (usageStatsMessage) {
      [_putUsageStatsDBMessage setStringValue:usageStatsMessage];
    }
  } else {
    [_putUsageStatsDB removeFromSuperview];
    [_putUsageStatsDBMessage removeFromSuperview];
  }

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

  if (!_alreadyActivated &&
      dir == InstallerDirectionForward) {
    // Make the package visible from i18n system preference
    RegisterGoogleJapaneseInput();
    if ([_activateCell state] == NSOnState &&
        [_doNotActivateCell state] == NSOffState) {
      // means clicks "next page" when "activate" menu is on
      ActivateGoogleJapaneseInput();
      _alreadyActivated = YES;
    }
    if (!_hasUsageStatsDB && [_putUsageStatsDB state] == NSOnState) {
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
