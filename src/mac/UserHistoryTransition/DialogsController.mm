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

#import "DialogsController.h"

#include "base/util.h"
#include "mac/UserHistoryTransition/user_history_transition.h"
#include "prediction/user_history_predictor.h"

enum ModalResult {
  DIALOG_OK,
  DIALOG_CANCEL,
};

namespace {
string deprecatedUserHistoryPath() {
  return mozc::UserHistoryPredictor::GetUserHistoryFileName() + ".oldformat";
}

bool WaitForModalWindow(NSWindow *window) {
  [window center];
  uint result = [[NSApplication sharedApplication] runModalForWindow:window];
  [window orderOut:nil];
  return result != DIALOG_CANCEL;
}
}  // anonymous namespace

@implementation DialogsController

- (void)awakeFromNib {
  const string deprecated_path = deprecatedUserHistoryPath();
  NSWindow *firstDialog = noNeedDialog_;
  if (mozc::Util::FileExists(deprecated_path)) {
    firstDialog = confirmDialog_;
  }
  [firstDialog orderFront:nil];
  [firstDialog center];
}

- (IBAction)cancel:(id)sender {
  [[NSApplication sharedApplication] terminate:self];
}

- (IBAction)runTransition:(id)sender {
  [confirmDialog_ orderOut:nil];
  const string deprecated_path = deprecatedUserHistoryPath();
  if (mozc::UserHistoryTransition::DoTransition(deprecated_path, true)) {
    WaitForModalWindow(successDialog_);
  } else {
    WaitForModalWindow(errorDialog_);
  }
  [[NSApplication sharedApplication] terminate:self];
}

- (IBAction)terminate:(id)sender {
  [[NSApplication sharedApplication] terminate:self];
}

- (IBAction)orderFrontStandardAboutPanel:(id)sender {
  // do nothing
}
@end
