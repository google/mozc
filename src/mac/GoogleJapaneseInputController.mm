// Copyright 2010-2014, Google Inc.
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

#import "mac/GoogleJapaneseInputController.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <InputMethodKit/IMKInputController.h>
#import <InputMethodKit/IMKServer.h>

#include <unistd.h>

#include <cstdlib>
#include <set>

#import "mac/GoogleJapaneseInputControllerInterface.h"
#import "mac/GoogleJapaneseInputServer.h"
#import "mac/KeyCodeMap.h"

#include "base/const.h"
#include "base/logging.h"
#include "base/mac_process.h"
#include "base/mac_util.h"
#include "base/mutex.h"
#include "base/process.h"
#include "base/util.h"
#include "client/client.h"
#include "config/config.pb.h"
#include "ipc/ipc.h"
#include "renderer/renderer_client.h"
#include "session/commands.pb.h"
#include "session/ime_switch_util.h"

using mozc::commands::Candidates;
using mozc::commands::Capability;
using mozc::commands::CompositionMode;
using mozc::commands::Input;
using mozc::commands::KeyEvent;
using mozc::commands::Output;
using mozc::commands::Preedit;
using mozc::commands::RendererCommand;
using mozc::commands::SessionCommand;
using mozc::config::Config;
using mozc::config::ImeSwitchUtil;
using mozc::kProductNameInEnglish;
using mozc::once_t;
using mozc::CallOnce;
using mozc::MacProcess;

namespace {
// set of bundle IDs of applications on which Mozc should not open urls.
const set<string> *gNoOpenLinkApps = NULL;
// The mapping from the CompositionMode enum to the actual id string
// of composition modes.
const map<CompositionMode, NSString *> *gModeIdMap = NULL;
const set<string> *gNoSelectedRangeApps = NULL;
const set<string> *gNoDisplayModeSwitchApps = NULL;
const set<string> *gNoSurroundingTextApps = NULL;

// TODO(horo): This value should be get from system configuration.
//  DoubleClickInterval can be get from NSEvent (MacOSX ver >= 10.6)
const NSTimeInterval kDoubleTapInterval = 0.5;

const int kMaxSurroundingLength = 20;
// In some apllications when the client's text length is large, getting the
// surrounding text takes too much time. So we set this limitation.
const int kGetSurroundingTextClientLengthLimit = 1000;

NSString *GetLabelForSuffix(const string &suffix) {
  string label = mozc::MacUtil::GetLabelForSuffix(suffix);
  return [[NSString stringWithUTF8String:label.c_str()] retain];
}

CompositionMode GetCompositionMode(NSString *modeID) {
  if (modeID == NULL) {
    LOG(ERROR) << "modeID could not be initialized.";
    return mozc::commands::DIRECT;
  }

  // The name of direct input mode.  This name is determined at
  // Info.plist.  We don't use com.google... instead of
  // com.apple... because of a hack for Java Swing applications like
  // JEdit.  If we use our own IDs for those modes, such applications
  // work incorrectly for some reasons.
  //
  // The document for ID names is available at:
  // http://developer.apple.com/legacy/mac/library/documentation/Carbon/
  // Reference/Text_Services_Manager/Reference/reference.html
  if ([modeID isEqual:@"com.apple.inputmethod.Roman"]) {
    // TODO(komatsu): This should be mozc::commands::HALF_ASCII, when
    // we can handle the difference between the direct mode and the
    // half ascii mode.
    DLOG(INFO) << "com.apple.inputmethod.Roman";
    return mozc::commands::HALF_ASCII;
  }

  if ([modeID isEqual:@"com.apple.inputmethod.Japanese.Katakana"]) {
    DLOG(INFO) << "com.apple.inputmethod.Japanese.Katakana";
    return mozc::commands::FULL_KATAKANA;
  }

  if ([modeID isEqual:@"com.apple.inputmethod.Japanese.HalfWidthKana"]) {
    DLOG(INFO) << "com.apple.inputmethod.Japanese.HalfWidthKana";
    return mozc::commands::HALF_KATAKANA;
  }

  if ([modeID isEqual:@"com.apple.inputmethod.Japanese.FullWidthRoman"]) {
    DLOG(INFO) << "com.apple.inputmethod.Japanese.FullWidthRoman";
    return mozc::commands::FULL_ASCII;
  }

  if ([modeID isEqual:@"com.apple.inputmethod.Japanese"]) {
    DLOG(INFO) << "com.apple.inputmethod.Japanese";
    return mozc::commands::HIRAGANA;
  }

  LOG(ERROR) << "The code should not reach here.";
  return mozc::commands::DIRECT;
}

bool IsBannedApplication(const set<string>* bundleIdSet,
                         const string& bundleId) {
  return bundleIdSet == NULL || bundleId.empty() ||
      bundleIdSet->find(bundleId) != bundleIdSet->end();
}
}  // anonymous namespace


@implementation GoogleJapaneseInputController
#pragma mark accessors for testing
@synthesize keyCodeMap = keyCodeMap_;
@synthesize yenSignCharacter = yenSignCharacter_;
@synthesize mode = mode_;
@synthesize rendererCommand = rendererCommand_;
@synthesize replacementRange = replacementRange_;
@synthesize imkClientForTest = imkClientForTest_;
- (mozc::client::ClientInterface *)mozcClient {
  return mozcClient_;
}
- (void)setMozcClient:(mozc::client::ClientInterface *)newMozcClient {
  delete mozcClient_;
  mozcClient_ = newMozcClient;
}
- (mozc::renderer::RendererInterface *)renderer {
  return candidateController_;
}
- (void)setRenderer:(mozc::renderer::RendererInterface *)newRenderer {
  delete candidateController_;
  candidateController_ = newRenderer;
}


#pragma mark object init/dealloc
// Initializer designated in IMKInputController. see:
// http://developer.apple.com/documentation/Cocoa/Reference/IMKInputController_Class/

- (id)initWithServer:(IMKServer *)server
            delegate:(id)delegate
              client:(id)inputClient {
  self = [super initWithServer:server delegate:delegate client:inputClient];
  if (!self) {
    return self;
  }
  keyCodeMap_ = [[KeyCodeMap alloc] init];
  clientBundle_ = new(nothrow) string;
  replacementRange_ = NSMakeRange(NSNotFound, 0);
  originalString_ = [[NSMutableString alloc] init];
  composedString_ = [[NSMutableAttributedString alloc] init];
  cursorPosition_ = NSNotFound;
  mode_ = mozc::commands::DIRECT;
  checkInputMode_ = YES;
  suppressSuggestion_ = NO;
  yenSignCharacter_ = mozc::config::Config::YEN_SIGN;
  candidateController_ = new(nothrow) mozc::renderer::RendererClient;
  rendererCommand_ = new(nothrow)RendererCommand;
  mozcClient_ = mozc::client::ClientFactory::NewClient();
  imkServer_ = reinterpret_cast<id<ServerCallback> >(server);
  imkClientForTest_ = nil;
  lastKeyDownTime_ = 0;
  lastKeyCode_ = 0;

  // We don't check the return value of NSBundle because it fails during tests.
  [NSBundle loadNibNamed:@"Config" owner:self];
  if (!originalString_ || !composedString_ || !candidateController_ ||
      !rendererCommand_ || !mozcClient_ || !clientBundle_) {
    [self release];
    self = nil;
  } else {
    DLOG(INFO) << [[NSString stringWithFormat:@"initWithServer: %@ %@ %@",
                             server, delegate, inputClient] UTF8String];
    if (!candidateController_->Activate()) {
      LOG(ERROR) << "Cannot activate renderer";
      delete candidateController_;
      candidateController_ = NULL;
    }
    [self setupClientBundle:inputClient];
    [self setupCapability];
    RendererCommand::ApplicationInfo *applicationInfo =
        rendererCommand_->mutable_application_info();
    applicationInfo->set_process_id(::getpid());
    // thread_id and receiver_handle are not used currently in Mac but
    // set some values to prevent warning.
    applicationInfo->set_thread_id(0);
    applicationInfo->set_receiver_handle(0);
  }

  return self;
}

- (void)dealloc {
  [keyCodeMap_ release];
  [originalString_ release];
  [composedString_ release];
  [imkClientForTest_ release];
  delete clientBundle_;
  delete candidateController_;
  delete mozcClient_;
  delete rendererCommand_;
  DLOG(INFO) << "dealloc server";
  [super dealloc];
}

- (id)client {
  if (imkClientForTest_) {
    return imkClientForTest_;
  }
  return [super client];
}

- (NSMenu*)menu {
  return menu_;
}

+ (void)initializeConstants {
  set<string> *noOpenlinkApps = new(nothrow) set<string>;
  if (noOpenlinkApps) {
    // should not open links during screensaver.
    noOpenlinkApps->insert("com.apple.securityagent");
    gNoOpenLinkApps = noOpenlinkApps;
  }

  map<CompositionMode, NSString *> *newMap =
      new(nothrow) map<CompositionMode, NSString *>;
  if (newMap) {
    (*newMap)[mozc::commands::DIRECT] = GetLabelForSuffix("Roman");
    (*newMap)[mozc::commands::HIRAGANA] = GetLabelForSuffix("base");
    (*newMap)[mozc::commands::FULL_KATAKANA] = GetLabelForSuffix("Katakana");
    (*newMap)[mozc::commands::HALF_ASCII] = GetLabelForSuffix("Roman");
    (*newMap)[mozc::commands::FULL_ASCII] = GetLabelForSuffix("FullWidthRoman");
    (*newMap)[mozc::commands::HALF_KATAKANA] =
        GetLabelForSuffix("FullWidthRoman");
    gModeIdMap = newMap;
  }

  set<string> *noSelectedRangeApps = new(nothrow) set<string>;
  if (noSelectedRangeApps) {
    // Do not call selectedRange: method for the following
    // applications because it could lead to application crash.
    noSelectedRangeApps->insert("com.microsoft.Excel");
    noSelectedRangeApps->insert("com.microsoft.Powerpoint");
    noSelectedRangeApps->insert("com.microsoft.Word");
    gNoSelectedRangeApps = noSelectedRangeApps;
  }

  // Do not call selectInputMode: method for the following
  // applications because it could cause some unexpected behavior.
  // MS-Word: When the display mode goes to ASCII but there is no
  // compositions, it goes to direct input mode instead of Half-ASCII
  // mode.  When the first composition character is alphanumeric (such
  // like pressing Shift-A at first), that character is directly
  // inserted into application instead of composition starting "A".
  set<string> *noDisplayModeSwitchApps = new(nothrow) set<string>;
  if (noDisplayModeSwitchApps) {
    noDisplayModeSwitchApps->insert("com.microsoft.Word");
    gNoDisplayModeSwitchApps = noDisplayModeSwitchApps;
  }

  set<string> *noSurroundingTextApps = new(nothrow) set<string>;
  if (noSurroundingTextApps) {
    // Disables the surrounding text feature for the following application
    // because calling attributedSubstringFromRange to it is very heavy.
    noSurroundingTextApps->insert("com.evernote.Evernote");
    gNoSurroundingTextApps = noSurroundingTextApps;
  }
}

#pragma mark IMKStateSetting Protocol
// Currently it just ignores the following methods:
//   Modes, showPreferences, valueForTag
// They are described at
// http://developer.apple.com/documentation/Cocoa/Reference/IMKStateSetting_Protocol/

- (void)activateServer:(id)sender {
  [super activateServer:sender];
  checkInputMode_ = YES;
  if (rendererCommand_->visible() && candidateController_) {
    candidateController_->ExecCommand(*rendererCommand_);
  }
  [self handleConfig];
  [imkServer_ setCurrentController:self];

  string window_name, window_owner;
  if (mozc::MacUtil::GetFrontmostWindowNameAndOwner(&window_name,
                                                    &window_owner)) {
    DLOG(INFO) << "frontmost window name: \"" << window_name << "\" "
               << "owner: \"" << window_owner << "\"";
    if (mozc::MacUtil::IsSuppressSuggestionWindow(window_name, window_owner)) {
      suppressSuggestion_ = YES;
    } else {
      suppressSuggestion_ = NO;
    }
  }

  DLOG(INFO) << kProductNameInEnglish << " client (" << self
             << "): activated for " << sender;
  DLOG(INFO) << "sender bundleID: " << *clientBundle_;
}

- (void)deactivateServer:(id)sender {
  RendererCommand clearCommand;
  clearCommand.set_type(RendererCommand::UPDATE);
  clearCommand.set_visible(false);
  clearCommand.clear_output();
  if (candidateController_) {
    candidateController_->ExecCommand(clearCommand);
  }
  DLOG(INFO) << kProductNameInEnglish << " client (" << self
             << "): deactivated";
  DLOG(INFO) << "sender bundleID: " << *clientBundle_;
  [super deactivateServer:sender];
}

- (NSUInteger)recognizedEvents:(id)sender {
  // Because we want to handle single Shift key pressing later, now I
  // turned on NSFlagsChanged also.
  return NSKeyDownMask | NSFlagsChangedMask;
}

// This method is called when a user changes the input mode.
- (void)setValue:(id)value forTag:(long)tag client:(id)sender {
  CompositionMode new_mode = GetCompositionMode(value);

  if (new_mode == mozc::commands::HALF_ASCII && [composedString_ length] == 0) {
    new_mode = mozc::commands::DIRECT;
  }

  [self switchMode:new_mode client:sender];
  [self handleConfig];
  [super setValue:value forTag:tag client:sender];
}


#pragma mark internal methods

- (void)handleConfig {
  // Get the config and set client-side behaviors
  Config config;
  if (!mozcClient_->GetConfig(&config)) {
    LOG(ERROR) << "Cannot obtain the current config";
    return;
  }

  InputMode input_mode = ASCII;
  if (config.preedit_method() == Config::KANA) {
    input_mode = KANA;
  }
  [keyCodeMap_ setInputMode:input_mode];
  yenSignCharacter_ = config.yen_sign_character();

  if (config.use_japanese_layout()) {
    // Apple does not have "Japanese" layout actually -- here sets
    // "US" layout, which means US-ASCII layout or JIS layout
    // depending on which type of keyboard is actually connected.
    [[self client] overrideKeyboardWithKeyboardNamed:@"com.apple.keylayout.US"];
  }
}

- (void)setupClientBundle:(id)sender {
  NSString *bundleIdentifier = [sender bundleIdentifier];
  if (bundleIdentifier != nil && [bundleIdentifier length] > 0) {
    clientBundle_->assign([bundleIdentifier UTF8String]);
  }
}

- (void)setupCapability {
  Capability capability;

  if (IsBannedApplication(gNoSelectedRangeApps, *clientBundle_)) {
    capability.set_text_deletion(Capability::NO_TEXT_DELETION_CAPABILITY);
  } else {
    capability.set_text_deletion(Capability::DELETE_PRECEDING_TEXT);
  }

  mozcClient_->set_client_capability(capability);
}

// Mode changes to direct and clean up the status.
- (void)switchModeToDirect:(id)sender {
  mode_ = mozc::commands::DIRECT;
  DLOG(INFO) << "Mode switch: HIRAGANA, KATAKANA, etc. -> DIRECT";
  KeyEvent keyEvent;
  Output output;
  keyEvent.set_special_key(mozc::commands::KeyEvent::OFF);
  mozcClient_->SendKey(keyEvent, &output);
  if (output.has_result()) {
    [self commitText:output.result().value().c_str() client:sender];
  }
  if ([composedString_ length] > 0) {
    [self updateComposedString:NULL];
    [self clearCandidates];
  }
}

// change the mode to the new mode and turn-on the IME if necessary.
- (void)switchModeInternal:(CompositionMode)new_mode {
  if (mode_ == mozc::commands::DIRECT) {
    // Input mode changes from direct to an active mode.
    DLOG(INFO) << "Mode switch: DIRECT -> HIRAGANA, KATAKANA, etc.";
    KeyEvent keyEvent;
    Output output;
    keyEvent.set_special_key(mozc::commands::KeyEvent::ON);
    mozcClient_->SendKey(keyEvent, &output);
  }

  if (mode_ != new_mode) {
    // Switch input mode.
    DLOG(INFO) << "Switch input mode.";
    SessionCommand command;
    command.set_type(mozc::commands::SessionCommand::SWITCH_INPUT_MODE);
    command.set_composition_mode(new_mode);
    Output output;
    mozcClient_->SendCommand(command, &output);
    mode_ = new_mode;
  }
}

- (void)switchMode:(CompositionMode)new_mode client:(id)sender {
  if (mode_ == new_mode) {
    return;
  }
  if (mode_ != mozc::commands::DIRECT && new_mode == mozc::commands::DIRECT) {
    [self switchModeToDirect:sender];
  } else if (new_mode != mozc::commands::DIRECT) {
    [self switchModeInternal:new_mode];
  }
}

- (void)switchDisplayMode {
  if (gModeIdMap == NULL) {
    LOG(ERROR) << "gModeIdMap is not initialized correctly.";
    return;
  }
  if (IsBannedApplication(gNoDisplayModeSwitchApps, *clientBundle_)) {
    return;
  }

  map<CompositionMode, NSString *>::const_iterator it = gModeIdMap->find(mode_);
  if (it == gModeIdMap->end()) {
    LOG(ERROR) << "mode: " << mode_ << " is invalid";
    return;
  }

  [[self client] selectInputMode:it->second];
}

- (void)commitText:(const char *)text client:(id)sender {
  if (text == NULL) {
    return;
  }

  [sender insertText:[NSString stringWithUTF8String:text]
    replacementRange:replacementRange_];
  replacementRange_ = NSMakeRange(NSNotFound, 0);
}

- (void)launchWordRegisterTool:(id)client {
  ::setenv(mozc::kWordRegisterEnvironmentName, "", 1);
  if (!IsBannedApplication(gNoSelectedRangeApps, *clientBundle_)) {
    NSRange selectedRange = [client selectedRange];
    if (selectedRange.location != NSNotFound &&
        selectedRange.length != NSNotFound &&
        selectedRange.length > 0) {
      NSString *text =
        [[client attributedSubstringFromRange:selectedRange] string];
      if (text != nil) {
        :: setenv(mozc::kWordRegisterEnvironmentName, [text UTF8String], 1);
      }
    }
  }
  MacProcess::LaunchMozcTool("word_register_dialog");
}

- (void)invokeReconvert:(const SessionCommand *)command client:(id)sender {
  if (IsBannedApplication(gNoSelectedRangeApps, *clientBundle_)) {
    return;
  }

  NSRange selectedRange = [sender selectedRange];
  if (selectedRange.location == NSNotFound ||
      selectedRange.length == NSNotFound) {
    // the application does not support reconversion.
    return;
  }

  DLOG(INFO) << selectedRange.location << ", " << selectedRange.length;
  SessionCommand sending_command;
  Output output;
  sending_command.CopyFrom(*command);

  if (selectedRange.length == 0) {
    // Currently no range is selected for reconversion.  Tries to
    // invoke UNDO instead.
    [self invokeUndo:sender];
    return;
  }

  if (!sending_command.has_text()) {
    NSString *text = [[sender attributedSubstringFromRange:selectedRange] string];
    if (!text) {
      return;
    }
    sending_command.set_text([text UTF8String]);
  }

  if (mozcClient_->SendCommand(sending_command, &output)) {
    replacementRange_ = selectedRange;
    [self processOutput:&output client:sender];
  }
}

- (void)invokeUndo:(id)sender {
  if (IsBannedApplication(gNoSelectedRangeApps, *clientBundle_)) {
    return;
  }

  NSRange selectedRange = [sender selectedRange];
  if (selectedRange.location == NSNotFound ||
      selectedRange.length == NSNotFound ||
      // Some applications such like iTunes does not return NSNotFound
      // range but (0, 0).  However, the range starting with negative
      // location has to be invalid, then we can reject such apps.
      selectedRange.location == 0) {
    return;
  }

  DLOG(INFO) << selectedRange.location << ", " << selectedRange.length;
  SessionCommand command;
  Output output;
  command.set_type(SessionCommand::UNDO);
  if (mozcClient_->SendCommand(command, &output)) {
    [self processOutput:&output client:sender];
  }
}

- (void)processOutput:(const mozc::commands::Output *)output client:(id)sender {
  if (output == NULL) {
    return;
  }
  if (!output->consumed()) {
    return;
  }

  DLOG(INFO) << output->DebugString();
  if (output->has_url()) {
    NSString *url = [NSString stringWithUTF8String:output->url().c_str()];
    [self openLink:[NSURL URLWithString:url]];
  }

  if (output->has_result()) {
    [self commitText:output->result().value().c_str() client:sender];
  }

  // Handles deletion range.  We do not even handle it for some
  // applications to prevent application crashes.
  if (output->has_deletion_range() &&
      !IsBannedApplication(gNoSelectedRangeApps, *clientBundle_)) {
    if ([composedString_ length] == 0 &&
        replacementRange_.location == NSNotFound) {
      NSRange selectedRange = [sender selectedRange];
      const mozc::commands::DeletionRange &deletion_range =
          output->deletion_range();
      if (selectedRange.location != NSNotFound ||
          selectedRange.length != NSNotFound ||
          selectedRange.location + deletion_range.offset() > 0) {
        // The offset is a negative value.  See session/commands.proto for
        // the details.
        selectedRange.location += deletion_range.offset();
        selectedRange.length += deletion_range.length();
        replacementRange_ = selectedRange;
      }
    } else {
      // We have to consider the case that there is already
      // composition and/or we already set the position of the
      // composition by replacementRange_.  We do nothing here at this
      // time because we already found that it will involve several
      // buggy behaviors with Carbon apps and MS Office.
      // TODO(mukai): find the right behavior.
    }
  }

  [self updateComposedString:&(output->preedit())];
  [self updateCandidates:output];

  if (output->has_mode()) {
    CompositionMode new_mode = output->mode();
    // Do not allow HALF_ASCII with empty composition.  This should be
    // handled in the converter, but just in case.
    if (new_mode == mozc::commands::HALF_ASCII &&
        (!output->has_preedit() || output->preedit().segment_size() == 0)) {
      new_mode = mozc::commands::DIRECT;
      [self switchMode:new_mode client:sender];
    }
    if (new_mode != mode_) {
      mode_ = new_mode;
      [self switchDisplayMode];
    }
  }

  if (output->has_launch_tool_mode()) {
    switch (output->launch_tool_mode()) {
      case mozc::commands::Output::CONFIG_DIALOG:
        MacProcess::LaunchMozcTool("config_dialog");
        break;
      case mozc::commands::Output::DICTIONARY_TOOL:
        MacProcess::LaunchMozcTool("dictionary_tool");
        break;
      case mozc::commands::Output::WORD_REGISTER_DIALOG:
        [self launchWordRegisterTool:sender];
        break;
      default:
        // do nothing
        break;
    }
  }

  // Handle callbacks.
  if (output->has_callback() && output->callback().has_session_command()) {
    if (output->callback().has_delay_millisec()) {
      callback_command_.CopyFrom(output->callback());
      // In the current implementation, if the subsequent key event also makes
      // callback, the second callback will be called in the timimg of the first
      // callback.
      [self performSelector:@selector(sendCallbackCommand)
                 withObject:nil
                 afterDelay:output->callback().has_delay_millisec() / 1000.0];
      return;
    }
    const SessionCommand &callback_command =
        output->callback().session_command();
    if (callback_command.type() == SessionCommand::CONVERT_REVERSE) {
      [self invokeReconvert:&callback_command client:sender];
    } else if (callback_command.type() == SessionCommand::UNDO) {
      [self invokeUndo:sender];
    } else {
      Output output_for_callback;
      if (mozcClient_->SendCommand(callback_command, &output_for_callback)) {
        [self processOutput:&output_for_callback client:sender];
      }
    }
  }
}

#pragma mark Mozc Server methods


#pragma mark IMKServerInput Protocol
// Currently GoogleJapaneseInputController uses handleEvent:client:
// method to handle key events.  It does not support inputText:client:
// nor inputText:key:modifiers:client:.
// Because GoogleJapaneseInputController does not use IMKCandidates,
// the following methods are not needed to implement:
//   candidates
//
// The meaning of these methods are described at:
// http://developer.apple.com/documentation/Cocoa/Reference/IMKServerInput_Additions/

- (id)originalString:(id)sender {
  return originalString_;
}

- (void)updateComposedString:(const Preedit *)preedit {
  // If the last and the current composed string length is 0,
  // we don't call updateComposition.
  if (([composedString_ length] == 0) &&
      ((preedit == NULL || preedit->segment_size() == 0))) {
    return;
  }

  [composedString_
    deleteCharactersInRange:NSMakeRange(0, [composedString_ length])];
  cursorPosition_ = NSNotFound;
  if (preedit != NULL) {
    cursorPosition_ = preedit->cursor();
    for (size_t i = 0; i < preedit->segment_size(); ++i) {
      NSDictionary *highlightAttributes =
          [self markForStyle:kTSMHiliteSelectedConvertedText
                     atRange:NSMakeRange(NSNotFound, 0)];
      NSDictionary *underlineAttributes =
          [self markForStyle:kTSMHiliteConvertedText
                     atRange:NSMakeRange(NSNotFound, 0)];
      const Preedit::Segment& seg = preedit->segment(i);
      NSDictionary *attr = (seg.annotation() == Preedit::Segment::HIGHLIGHT)?
          highlightAttributes : underlineAttributes;
      NSString *seg_string =
          [NSString stringWithUTF8String:seg.value().c_str()];
      NSAttributedString *seg_attributed_string =
          [[[NSAttributedString alloc]
             initWithString:seg_string attributes:attr]
            autorelease];
      [composedString_ appendAttributedString:seg_attributed_string];
    }
  }
  if ([composedString_ length] == 0) {
    [originalString_ setString:@""];
    replacementRange_ = NSMakeRange(NSNotFound, 0);
  }

  // Make composed string visible to the client applications.
  [self updateComposition];
}

- (void)commitComposition:(id)sender {
  if ([composedString_ length] == 0) {
    DLOG(INFO) << "Nothing is committed.";
    return;
  }
  [self commitText:[[composedString_ string] UTF8String] client:sender];

  SessionCommand command;
  Output output;
  command.set_type(SessionCommand::SUBMIT);
  mozcClient_->SendCommand(command, &output);
  [self clearCandidates];
  [self updateComposedString:NULL];
}

- (id)composedString:(id)sender {
  return composedString_;
}

- (void)clearCandidates {
  rendererCommand_->set_type(RendererCommand::UPDATE);
  rendererCommand_->set_visible(false);
  rendererCommand_->clear_output();
  if (candidateController_) {
    candidateController_->ExecCommand(*rendererCommand_);
  }
}

// |selecrionRange| method is defined at IMKInputController class and
// means the position of cursor actually.
- (NSRange)selectionRange {
  return (cursorPosition_ == NSNotFound) ?
      [super selectionRange] : // default behavior defined at super class
      NSMakeRange(cursorPosition_, 0);
}

- (void)delayedUpdateCandidates {
  // The candidate window position is not recalculated if the
  // candidate already appears on the screen.  Therefore, if a user
  // moves client application window by mouse, candidate window won't
  // follow the move of window.  This is done because:
  //  - some applications like Emacs or Google Chrome don't return the
  //    cursor position correctly.  The candidate window moves
  //    frequently with those application, which irritates users.
  //  - Kotoeri does this too.
  if ((!rendererCommand_->visible()) &&
      (rendererCommand_->output().candidates().candidate_size() > 0)) {
    NSRect preeditRect = NSZeroRect;
    const int32 position = rendererCommand_->output().candidates().position();
    // Some applications throws error when we call attributesForCharacterIndex.
    DLOG(INFO) << "attributesForCharacterIndex: " << position;
    @try {
      [[self client] attributesForCharacterIndex:position
                             lineHeightRectangle:&preeditRect];
    }
    @catch (NSException *exception) {
      LOG(ERROR) << "Exception from [" << *clientBundle_ << "] "
                 << [[exception name] UTF8String] << ","
                 << [[exception reason] UTF8String];
    }
    DLOG(INFO) << "  preeditRect: (("
               << preeditRect.origin.x << ", "
               << preeditRect.origin.y << "), ("
               << preeditRect.size.width << ", "
               << preeditRect.size.height << "))";
    NSScreen *baseScreen = nil;
    NSRect baseFrame = NSZeroRect;
    for (baseScreen in [NSScreen screens]) {
      baseFrame = [baseScreen frame];
      if (baseFrame.origin.x == 0 && baseFrame.origin.y == 0) {
        break;
      }
    }
    int baseHeight = baseFrame.size.height;
    rendererCommand_->mutable_preedit_rectangle()->set_left(
        preeditRect.origin.x);
    rendererCommand_->mutable_preedit_rectangle()->set_top(
        baseHeight - preeditRect.origin.y - preeditRect.size.height);
    rendererCommand_->mutable_preedit_rectangle()->set_right(
        preeditRect.origin.x + preeditRect.size.width);
    rendererCommand_->mutable_preedit_rectangle()->set_bottom(
        baseHeight - preeditRect.origin.y);
  }

  rendererCommand_->set_visible(
    rendererCommand_->output().candidates().candidate_size() > 0);
  if (candidateController_) {
    candidateController_->ExecCommand(*rendererCommand_);
  }
}

- (void)updateCandidates:(const Output *)output {
  if (output == NULL) {
    [self clearCandidates];
    return;
  }

  rendererCommand_->set_type(RendererCommand::UPDATE);
  rendererCommand_->mutable_output()->CopyFrom(*output);

  // Runs delayedUpdateCandidates in the next event loop.
  // This is because some applications like Google Docs with Chrome returns
  // incorrect cursor position if we call attributesForCharacterIndex here.
  [self performSelector:@selector(delayedUpdateCandidates)
             withObject:nil
             afterDelay:0];
}

- (void)openLink:(NSURL *)url {
  // Open a link specified by |url|.  Any opening link behavior should
  // call this method because it checks the capability of application.
  // On some application like login window of screensaver, opening
  // link behavior should not happen because it can cause some
  // security issues.
  if (!clientBundle_ || IsBannedApplication(gNoOpenLinkApps, *clientBundle_)) {
    return;
  }
  [[NSWorkspace sharedWorkspace] openURL:url];
}

- (BOOL)fillSurroundingContext:(mozc::commands::Context *)context
                        client:(id<IMKTextInput>)client {
  NSInteger totalLength = [client length];
  if (totalLength == 0 || totalLength == NSNotFound ||
      totalLength > kGetSurroundingTextClientLengthLimit) {
    return false;
  }
  NSRange selectedRange = [client selectedRange];
  if (selectedRange.location == NSNotFound ||
      selectedRange.length == NSNotFound) {
    return false;
  }
  NSRange precedingRange = NSMakeRange(0, selectedRange.location);
  if (selectedRange.location > kMaxSurroundingLength) {
    precedingRange =
        NSMakeRange(selectedRange.location - kMaxSurroundingLength,
                    kMaxSurroundingLength);
  }
  NSString *precedingString =
    [[client attributedSubstringFromRange:precedingRange] string];
  if (precedingString) {
    context->set_preceding_text([precedingString UTF8String]);
    DLOG(INFO) << "preceding_text: \"" << context->preceding_text() << "\"";
  }
  return true;
}

- (BOOL)handleEvent:(NSEvent *)event client:(id)sender {
  if ([event type] == NSCursorUpdate) {
    [self updateComposition];
    return NO;
  }
  if ([event type] != NSKeyDown && [event type] != NSFlagsChanged) {
    return NO;
  }
  // Cancels the callback.
  callback_command_.Clear();

  // Handle KANA key and EISU key.  We explicitly handles this here
  // for mode switch because some text area such like iPhoto person
  // name editor does not call setValue:forTag:client: method.
  // see: http://www.google.com/support/forum/p/ime/thread?tid=3aafb74ff71a1a69&hl=ja&fid=3aafb74ff71a1a690004aa3383bc9f5d
  if ([event type] == NSKeyDown) {
    NSTimeInterval currentTime = [[NSDate date] timeIntervalSince1970];
    const NSTimeInterval elapsedTime = currentTime - lastKeyDownTime_;
    const bool isDoubleTap = ([event keyCode] == lastKeyCode_) &&
                             (elapsedTime < kDoubleTapInterval);
    lastKeyDownTime_ = currentTime;
    lastKeyCode_ = [event keyCode];

    // these calling of switchMode: can be duplicated if the
    // application sends the setValue:forTag:client: and handleEvent:
    // at the same key event, but that's okay because switchMode:
    // method does nothing if the new mode is same as the current
    // mode.
    if ([event keyCode] == kVK_JIS_Kana) {
      [self switchMode:mozc::commands::HIRAGANA client:sender];
      [self switchDisplayMode];
      if (isDoubleTap) {
        SessionCommand command;
        command.set_type(SessionCommand::CONVERT_REVERSE);
        [self invokeReconvert:&command client:sender];
      }
    } else if ([event keyCode] == kVK_JIS_Eisu) {
      if (isDoubleTap) {
        SessionCommand command;
        command.set_type(SessionCommand::COMMIT_RAW_TEXT);
        [self sendCommand:command];
      }
      CompositionMode new_mode = ([composedString_ length] == 0) ?
          mozc::commands::DIRECT : mozc::commands::HALF_ASCII;
      [self switchMode:new_mode client:sender];
      [self switchDisplayMode];
    }
  }

  if ([keyCodeMap_ isModeSwitchingKey:event]) {
    // Special hack for Eisu/Kana keys.  Sometimes those key events
    // come to this method but we should ignore them because some
    // applications like PhotoShop is stuck.
    return YES;
  }

  // Get the Mozc key event
  KeyEvent keyEvent;
  if (![keyCodeMap_ getMozcKeyCodeFromKeyEvent:event
                    toMozcKeyEvent:&keyEvent]) {
    // Modifier flags change (not submitted to the server yet), or
    // unsupported key pressed.
    return NO;
  }

  // If the key event is turn on event, the key event has to be sent
  // to the server anyway.
  if (mode_ == mozc::commands::DIRECT &&
      !ImeSwitchUtil::IsDirectModeCommand(keyEvent)) {
    // Yen sign special hack: although the current mode is DIRECT,
    // backslash is sent instead of yen sign for JIS yen key with no
    // modifiers.  This behavior is based on the configuration.
    if ([event keyCode] == kVK_JIS_Yen &&
        [event modifierFlags] == 0 &&
        yenSignCharacter_ == mozc::config::Config::BACKSLASH) {
      [self commitText:"\\" client:sender];
      return YES;
    }
    return NO;
  }

  // Send the key event to the server actually
  Output output;

  if (isprint(keyEvent.key_code())) {
    [originalString_ appendFormat:@"%c", keyEvent.key_code()];
  }

  mozc::commands::Context context;
  if (suppressSuggestion_) {
    // TODO(komatsu, horo): Support Google Omnibox too.
    context.add_experimental_features("google_search_box");
  }
  keyEvent.set_mode(mode_);

  if ([composedString_ length] == 0 &&
      !IsBannedApplication(gNoSelectedRangeApps, *clientBundle_) &&
      !IsBannedApplication(gNoSurroundingTextApps, *clientBundle_)) {
    [self fillSurroundingContext:&context client:sender];
  }
  if (!mozcClient_->SendKeyWithContext(keyEvent, context, &output)) {
    return NO;
  }

  [self processOutput:&output client:sender];
  return output.consumed();
}

- (void)sendCallbackCommand {
  if (callback_command_.has_session_command()) {
    const SessionCommand command = callback_command_.session_command();
    callback_command_.Clear();
    [self sendCommand:command];
  }
}

#pragma mark callbacks
- (void)sendCommand:(const SessionCommand &)command {
  Output output;
  if (!mozcClient_->SendCommand(command, &output)) {
    return;
  }

  [self processOutput:&output client:[self client]];
}

- (IBAction)reconversionClicked:(id)sender {
  SessionCommand command;
  command.set_type(SessionCommand::CONVERT_REVERSE);
  [self invokeReconvert:&command client:[self client]];
}

- (IBAction)configClicked:(id)sender {
  MacProcess::LaunchMozcTool("config_dialog");
}

- (IBAction)dictionaryToolClicked:(id)sender {
  MacProcess::LaunchMozcTool("dictionary_tool");
}

- (IBAction)registerWordClicked:(id)sender {
  [self launchWordRegisterTool:[self client]];
}

- (IBAction)characterPaletteClicked:(id)sender {
  MacProcess::LaunchMozcTool("character_palette");
}

- (IBAction)handWritingClicked:(id)sender {
  MacProcess::LaunchMozcTool("hand_writing");
}

- (IBAction)aboutDialogClicked:(id)sender {
  MacProcess::LaunchMozcTool("about_dialog");
}

- (void)outputResult:(mozc::commands::Output *)output {
  if (output == NULL || !output->has_result()) {
    return;
  }
  [self commitText:output->result().value().c_str() client:[self client]];
}
@end
