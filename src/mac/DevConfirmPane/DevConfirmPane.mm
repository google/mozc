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
  _understandFlag = ([_understandCheckBox state] == NSOnState);
  _agreeFlag = ([_agreeCheckBox state] == NSOnState);
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
  NSString *understandMessage = [self localizedStringForKey:@"understandMessage"];
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
