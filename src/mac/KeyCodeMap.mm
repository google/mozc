// Copyright 2010-2020, Google Inc.
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

#import "mac/KeyCodeMap.h"

#include <map>

#import <Carbon/Carbon.h>

#include "base/logging.h"
#include "base/mutex.h"
#include "protocol/commands.pb.h"

using mozc::commands::KeyEvent;
using mozc::once_t;
using mozc::CallOnce;

#include "mac/init_kanamap.h"
#include "mac/init_specialcharmap.h"
#include "mac/init_specialkeymap.h"

static const unichar kYenMark = 0xA5;

@interface KeyCodeMap ()
// Extract Mac modifier flag bits from |flags| and set them into
// |keyEvent|.
- (void)addModifierFlags:(NSUInteger)flags
          toMozcKeyEvent:(KeyEvent *)keyEvent;

// Extract key event information from |event| of NSFlagsChanged.
// Returns YES only if the flag change should create an Mozc key
// events.
- (BOOL)handleModifierFlagsChange:(NSEvent *)event
                   toMozcKeyEvent:(KeyEvent *)keyEvent;
@end

@implementation KeyCodeMap
@synthesize inputMode = inputMode_;

- (id)init {
  CallOnce(&kOnceForKanaMap, InitKanaMap);
  CallOnce(&kOnceForSpecialKeyMap, InitSpecialKeyMap);
  CallOnce(&kOnceForSpecialCharMap, InitSpecialCharMap);
  if (kSpecialKeyMap == nullptr) {
    self = nil;
    return self;
  }
  self = [super init];
  if (!self) {
    inputMode_ = ASCII;
  }
  return self;
}

- (void)addModifierFlags:(NSUInteger)flags
          toMozcKeyEvent:(KeyEvent *)keyEvent {
  if ((flags & NSShiftKeyMask)) {
    keyEvent->add_modifier_keys(KeyEvent::SHIFT);
  }
  if (flags & NSControlKeyMask) {
    keyEvent->add_modifier_keys(KeyEvent::CTRL);
  }
  if (flags & NSAlternateKeyMask) {
    keyEvent->add_modifier_keys(KeyEvent::ALT);
  }
}

- (BOOL)handleModifierFlagsChange:(NSEvent *)event
                   toMozcKeyEvent:(KeyEvent *)keyEvent {
  NSUInteger newFlags = [event modifierFlags];
  // Clear unused modifier flag bits.
  newFlags = newFlags & (NSShiftKeyMask |
                         NSControlKeyMask | NSAlternateKeyMask);
  if (~modifierFlags_ & newFlags) {
    // New modifier key is pressed: overwrite |modifierFlags_|.
    // Consider the case as follows:
    // 1. shift and control is pressed
    // 2. only control is released
    // 3. alt is newly pressed
    // At that time, modifierFlags_ should be shift+alt.
    modifierFlags_ = newFlags;
  }
  // If new modifier key is not pressed (i.e. some keys are released),
  // we do not update |modifierFlags_| at that time.  |modifierFlags_|
  // will become 0 when all modifier keys are released.

  if (newFlags != 0) {
    // The user is still pressing some modifier keys.
    return NO;
  }
  if (newFlags == 0 && modifierFlags_ == 0) {
    // The user released modifier keys but modifierFlags_ are already cleared.
    // Ex: Shift -> a (send Shift-a) -> release a -> release Shift (here)
    return NO;
  }

  [self addModifierFlags:modifierFlags_ toMozcKeyEvent:keyEvent];
  modifierFlags_ = 0;
  return YES;
}

- (BOOL)isModeSwitchingKey:(NSEvent *)event {
  NSUInteger keyCode = [event keyCode];
  return keyCode == kVK_JIS_Eisu || keyCode == kVK_JIS_Kana;
}

- (BOOL)getMozcKeyCodeFromKeyEvent:(NSEvent *)event
                    toMozcKeyEvent:(KeyEvent *)keyEvent {
  // This method uses |charactersIgnoringModifiers| basically but uses
  // |characters| if its modifier is only Shift.
  // For example:
  // \C-a   => CTRL + a  where characters = "\x01"
  // \S-a   => A         where characters = "\x41"
  // \C-S-a => CTRL + SHIFT + a where characters = "\x61"
  // \C-[   => Ctrl + [  where characters = "\x1b"
  // \S-[   => {         where characters = "\x7b"
  // \C-S-[ => CTRL + SHIFT + [ where characters = "\x5b"
  //
  // The main reason to do this is to avoid the confusion between \C-[
  // and ESC.
  //
  // There should be discussion like \C-S-a => CTRL + A instead of
  // CTRL + SHIFT + a, but we're not doing this currently because
  // Mozc must have keyboard layout information like "\S-[ => {" for
  // the case of input is symbol.

  if ([event type] == NSFlagsChanged) {
    return [self handleModifierFlagsChange:event toMozcKeyEvent:keyEvent];
  } else {
    modifierFlags_ = 0;
  }

  NSString *inputString = [event characters];
  NSString *inputStringRaw = [event charactersIgnoringModifiers];
  if ([inputString length] == 0) {
    return NO;
  }

  NSUInteger nsModifiers = [event modifierFlags];
  // strip caps lock effects.
  nsModifiers &= (~NSAlphaShiftKeyMask & NSDeviceIndependentModifierFlagsMask);
  unsigned short keyCode = [event keyCode];
  unichar inputChar = [((nsModifiers == NSShiftKeyMask) ?
                        inputString : inputStringRaw) characterAtIndex:0];
  std::map<unsigned short, KeyEvent::SpecialKey>::const_iterator sp_iter =
      kSpecialKeyMap->find(keyCode);
  if (sp_iter != kSpecialKeyMap->end()) {
    keyEvent->set_special_key(sp_iter->second);
  } else {
    std::map<unichar, KeyEvent::SpecialKey>::const_iterator spc_iter =
        kSpecialCharMap->find(inputChar);
    if (spc_iter != kSpecialCharMap->end()) {
      keyEvent->set_special_key(spc_iter->second);
    } else if (isgraph(inputChar)) {
      keyEvent->set_key_code(inputChar);
    } else if (inputChar == kYenMark) {
      keyEvent->set_key_code('\\');
    }

    // fill kana "key_string" if mode is kana.
    if (inputMode_ == KANA && !keyEvent->has_special_key()) {
      std::map<unsigned short, const char *>::const_iterator kana_iter;
      if (nsModifiers == NSShiftKeyMask && kKanaMapShift &&
          (kana_iter = kKanaMapShift->find(keyCode)) != kKanaMapShift->end()) {
        keyEvent->set_key_string(kana_iter->second);
      } else if (kKanaMap &&
                 (kana_iter = kKanaMap->find(keyCode)) != kKanaMap->end()) {
        keyEvent->set_key_string(kana_iter->second);
      }
    }
  }

  if (nsModifiers & NSCommandKeyMask) {
    // When command key is pressed, it is not a mozc key command.
    // Just ignore.
    return NO;
  }

#ifdef DEBUG
  // We enclose this logging with DEBUG explicitly because we found
  // that stringWithFormat is sometimes not trustworthy and DLOG(INFO)
  // will compute the string-to-be-logged even in production.
  LOG(INFO) << [[NSString stringWithFormat:@"%@", event] UTF8String]
            << " -> " << keyEvent->DebugString();
#endif

  if (nsModifiers == NSShiftKeyMask && !keyEvent->has_special_key()) {
    // If only the modifier is Shift and |keyEvent| is not normal key,
    // don't put any modifiers.
    return YES;
  }
  [self addModifierFlags:nsModifiers toMozcKeyEvent:keyEvent];
  return YES;
}

@end
