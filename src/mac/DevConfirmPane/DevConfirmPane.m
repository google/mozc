//
//  DevConfirmPane.m
//  GoogleJapaneseInput
//
//  Created by Tsuyoshi Horo on 11/09/07.
//  Copyright 2011 Google Inc. All rights reserved.
//

#import "DevConfirmPane.h"

#import <Carbon/Carbon.h>
#import <Foundation/Foundation.h>

@interface DevConfirmPane ()
// Returns localized string for the key in the bundle.  Returns nil if
// not found.  This is a wrapper of [NSBundle localizedStringForKey:]
// which returns the key if not found.
- (NSString *)localizedStringForKey:(NSString *)key;
@end

@implementation DevConfirmPane
// This method is called once when this module is loaded.
- (void)awakeFromNib {
  _understandFlag = NO;
  _agreeFlag = NO;
}

- (IBAction)checkBoxClicked:(id)sender {
  _understandFlag =
      ([_understandCheckBox state] == NSOnState);
  _agreeFlag =
      ([_agreeCheckBox state] == NSOnState);
  if (_understandFlag && _agreeFlag) {
    [self setNextEnabled:YES];
  } else {
    [self setNextEnabled:NO];
  }
}

- (NSString *)title {
  NSString *titleString = [self localizedStringForKey:@"Title"];
  if (titleString) {
    return titleString;
  }
  return @"About an evaluation version";
}

// This method is called when the pane is visible.
- (void)willEnterPane:(InstallerSectionDirection)dir {
  // Main messages
  NSString *mainMessage = [self localizedStringForKey:@"mainMessage"];
  if (mainMessage) {
    [_mainMessage setStringValue:mainMessage];
  }
  // First checkbox messages
  // "I understand that this version may be unstable compared to the stable version."
  NSString *understandMessage =
      [self localizedStringForKey:@"understandMessage"];
  if (understandMessage) {
    [_understandMessage setStringValue:understandMessage];
  }
  // Second checkbox messages
  // "I agree to allow usage statistics and crash reports to be sent automatically to Google."
  NSString *agreeMessage = [self localizedStringForKey:@"agreeMessage"];
  if (agreeMessage) {
    [_agreeMessage setStringValue:agreeMessage];
  }

  if (_understandFlag) {
    [_understandCheckBox setState:NSOnState];
  }
  if (_agreeFlag) {
    [_agreeCheckBox setState:NSOnState];
  }

  if (_understandFlag && _agreeFlag) {
    [self setNextEnabled:YES];
  } else {
    [self setNextEnabled:NO];
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
