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

#import "mac/mozc_imk_input_controller.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <InputMethodKit/IMKInputController.h>
#import <InputMethodKit/IMKServer.h>

#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <utility>

#import "mac/KeyCodeMap.h"
#import "mac/renderer_receiver.h"

#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "base/const.h"
#include "base/mac/mac_process.h"
#include "base/mac/mac_util.h"
#include "base/process.h"
#include "base/util.h"
#include "client/client.h"
#include "ipc/ipc.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "renderer/renderer_client.h"

using mozc::kProductNameInEnglish;
using mozc::MacProcess;
using mozc::commands::Capability;
using mozc::commands::CompositionMode;
using mozc::commands::KeyEvent;
using mozc::commands::Output;
using mozc::commands::Preedit;
using mozc::commands::RendererCommand;
using mozc::commands::SessionCommand;
using mozc::config::Config;
// less<> is necessary to compare between std::string and absl::string_view.
using SetOfString = std::set<std::string, std::less<>>;

namespace {
// Global object used as a singleton used as a proxy to receive messages from
// the renderer process.
RendererReceiver *gRendererReceiver = nil;

// TODO(horo): This value should be get from system configuration.
//  DoubleClickInterval can be get from NSEvent (MacOSX ver >= 10.6)
constexpr NSTimeInterval kDoubleTapInterval = 0.5;

constexpr int kMaxSurroundingLength = 20;
// In some apllications when the client's text length is large, getting the
// surrounding text takes too much time. So we set this limitation.
constexpr int kGetSurroundingTextClientLengthLimit = 1000;

constexpr absl::string_view kRomanModeId = "com.apple.inputmethod.Roman";
constexpr absl::string_view kKatakanaModeId = "com.apple.inputmethod.Japanese.Katakana";
constexpr absl::string_view kHalfWidthKanaModeId = "com.apple.inputmethod.Japanese.HalfWidthKana";
constexpr absl::string_view kFullWidthRomanModeId = "com.apple.inputmethod.Japanese.FullWidthRoman";
constexpr absl::string_view kHiraganaModeId = "com.apple.inputmethod.Japanese";

CompositionMode GetCompositionMode(absl::string_view mode_id) {
  if (mode_id.empty()) {
    LOG(ERROR) << "mode_id is initialized.";
    return mozc::commands::DIRECT;
  }
  DLOG(INFO) << mode_id;

  // The information for ID names was available at
  // /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/System/Library/Frameworks/
  // Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/Headers/TextServices.h
  // These IDs are also defined in Info.plist.
  if (mode_id == kRomanModeId) {
    return mozc::commands::HALF_ASCII;
  }
  if (mode_id == kKatakanaModeId) {
    return mozc::commands::FULL_KATAKANA;
  }
  if (mode_id == kHalfWidthKanaModeId) {
    return mozc::commands::HALF_KATAKANA;
  }
  if (mode_id == kFullWidthRomanModeId) {
    return mozc::commands::FULL_ASCII;
  }
  if (mode_id == kHiraganaModeId) {
    return mozc::commands::HIRAGANA;
  }

  LOG(ERROR) << "The code should not reach here.";
  return mozc::commands::DIRECT;
}

absl::string_view GetModeId(CompositionMode mode) {
  switch (mode) {
    case mozc::commands::DIRECT:
    case mozc::commands::HALF_ASCII:
      return kRomanModeId;
    case mozc::commands::FULL_KATAKANA:
      return kKatakanaModeId;
    case mozc::commands::HALF_KATAKANA:
      return kHalfWidthKanaModeId;
    case mozc::commands::FULL_ASCII:
      return kFullWidthRomanModeId;
    case mozc::commands::HIRAGANA:
      return kHiraganaModeId;
    default:
      LOG(ERROR) << "The code should not reach here.";
      return kRomanModeId;
  }
}

bool CanOpenLink(absl::string_view bundle_id) {
  // Should not open links during screensaver.
  return bundle_id != "com.apple.securityagent";
}

bool CanSelectedRange(absl::string_view bundle_id) {
  // Do not call selectedRange: method for the following
  // applications because it could lead to application crash.
  const bool is_supported = bundle_id != "com.microsoft.Excel" &&
                            bundle_id != "com.microsoft.Powerpoint" &&
                            bundle_id != "com.microsoft.Word";
  return is_supported;
}

bool CanDisplayModeSwitch(absl::string_view bundle_id) {
  // Do not call selectInputMode: method for the following
  // applications because it could cause some unexpected behavior.
  // MS-Word: When the display mode goes to ASCII but there is no
  // compositions, it goes to direct input mode instead of Half-ASCII
  // mode.  When the first composition character is alphanumeric (such
  // like pressing Shift-A at first), that character is directly
  // inserted into application instead of composition starting "A".
  return bundle_id != "com.microsoft.Word";
}

bool CanSurroundingText(absl::string_view bundle_id) {
  // Disables the surrounding text feature for the following application
  // because calling attributedSubstringFromRange to it is very heavy.
  return bundle_id != "com.evernote.Evernote";
}
}  // namespace

@implementation MozcImkInputController
#pragma mark accessors for testing
@synthesize keyCodeMap = keyCodeMap_;
@synthesize yenSignCharacter = yenSignCharacter_;
@synthesize mode = mode_;
@synthesize rendererCommand = rendererCommand_;
@synthesize replacementRange = replacementRange_;
@synthesize imkClientForTest = imkClientForTest_;
- (mozc::client::ClientInterface *)mozcClient {
  return mozcClient_.get();
}
- (void)setMozcClient:(std::unique_ptr<mozc::client::ClientInterface>)newMozcClient {
  mozcClient_ = std::move(newMozcClient);
}
- (mozc::renderer::RendererInterface *)renderer {
  return mozcRenderer_.get();
}
- (void)setRenderer:(std::unique_ptr<mozc::renderer::RendererInterface>)newRenderer {
  mozcRenderer_ = std::move(newRenderer);
}

#pragma mark object init/dealloc
// Initializer designated in IMKInputController. see:
// https://developer.apple.com/documentation/inputmethodkit/imkinputcontroller?language=objc

- (id)initWithServer:(IMKServer *)server delegate:(id)delegate client:(id)inputClient {
  // If server is nil, we are in the unit test environment.
  if (server == nil) {
    self = [super init];
    imkClientForTest_ = inputClient;
  } else {
    self = [super initWithServer:server delegate:delegate client:inputClient];
    imkClientForTest_ = nil;
  }
  if (!self) {
    return self;
  }
  keyCodeMap_ = [[KeyCodeMap alloc] init];
  replacementRange_ = NSMakeRange(NSNotFound, 0);
  originalString_ = [[NSMutableString alloc] init];
  composedString_ = [[NSMutableAttributedString alloc] init];
  cursorPosition_ = -1;
  mode_ = mozc::commands::DIRECT;
  suppressSuggestion_ = false;
  yenSignCharacter_ = mozc::config::Config::YEN_SIGN;
  mozcRenderer_ = std::make_unique<mozc::renderer::RendererClient>();
  mozcClient_ = mozc::client::ClientFactory::NewClient();
  lastKeyDownTime_ = 0;
  lastKeyCode_ = 0;

  // We don't check the return value of NSBundle because it fails during tests.
  [[NSBundle mainBundle] loadNibNamed:@"Config" owner:self topLevelObjects:nil];
  if (!originalString_ || !composedString_ || !mozcRenderer_ || !mozcClient_) {
    self = nil;
  } else {
    DLOG(INFO) << [[NSString
        stringWithFormat:@"initWithServer: %@ %@ %@", server, delegate, inputClient] UTF8String];
    if (!mozcRenderer_->Activate()) {
      LOG(ERROR) << "Cannot activate renderer";
      mozcRenderer_.reset();
    }
    [self setupClientBundle:inputClient];
    [self setupCapability];
    RendererCommand::ApplicationInfo *applicationInfo = rendererCommand_.mutable_application_info();
    applicationInfo->set_process_id(::getpid());
    // thread_id and receiver_handle are not used currently in Mac but
    // set some values to prevent warning.
    applicationInfo->set_thread_id(0);
    applicationInfo->set_receiver_handle(0);
  }

  return self;
}

- (void)dealloc {
  keyCodeMap_ = nil;
  originalString_ = nil;
  composedString_ = nil;
  imkClientForTest_ = nil;
  mozcRenderer_.reset();
  mozcClient_.reset();
  DLOG(INFO) << "dealloc server";
}

- (id)client {
  if (imkClientForTest_) {
    return imkClientForTest_;
  }
  return [super client];
}

- (NSMenu *)menu {
  return menu_;
}

#pragma mark IMKStateSetting Protocol
// Currently it just ignores the following methods:
//   Modes, showPreferences, valueForTag
// They are described at
// https://developer.apple.com/documentation/inputmethodkit/imkstatesetting?language=objc

- (void)activateServer:(id)sender {
  if (imkClientForTest_) {
    return;
  }
  [super activateServer:sender];
  [self setupClientBundle:sender];
  if (rendererCommand_.visible() && mozcRenderer_) {
    mozcRenderer_->ExecCommand(rendererCommand_);
  }
  [self handleConfig];

  // Sets this controller as the active controller to receive messages from the renderer process.
  [gRendererReceiver setCurrentController:self];

  std::string window_name, window_owner;
  if (mozc::MacUtil::GetFrontmostWindowNameAndOwner(&window_name, &window_owner)) {
    DLOG(INFO) << "frontmost window name: \"" << window_name << "\" " << "owner: \"" << window_owner
               << "\"";
    suppressSuggestion_ = mozc::MacUtil::IsSuppressSuggestionWindow(window_name, window_owner);
  }

  DLOG(INFO) << kProductNameInEnglish << " client (" << self << "): activated for " << sender;
  DLOG(INFO) << "sender bundleID: " << clientBundle_;
}

- (void)deactivateServer:(id)sender {
  if (imkClientForTest_) {
    return;
  }

  RendererCommand clearCommand;
  clearCommand.set_type(RendererCommand::UPDATE);
  clearCommand.set_visible(false);
  clearCommand.clear_output();
  if (mozcRenderer_) {
    mozcRenderer_->ExecCommand(clearCommand);
  }
  DLOG(INFO) << kProductNameInEnglish << " client (" << self << "): deactivated";
  DLOG(INFO) << "sender bundleID: " << clientBundle_;
  [super deactivateServer:sender];
}

- (NSUInteger)recognizedEvents:(id)sender {
  // Because we want to handle single Shift key pressing later, now I
  // turned on NSFlagsChanged also.
  return NSEventMaskKeyDown | NSEventMaskFlagsChanged;
}

// This method is called when a user changes the input mode.
- (void)setValue:(id)value forTag:(long)tag client:(id)sender {
  if (imkClientForTest_) {
    return;
  }

  CompositionMode new_mode = [value isKindOfClass:[NSString class]]
                                 ? GetCompositionMode([value UTF8String])
                                 : mozc::commands::DIRECT;
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
    clientBundle_.assign([bundleIdentifier UTF8String]);
  }
}

- (void)setupCapability {
  Capability capability;

  if (CanSelectedRange(clientBundle_)) {
    capability.set_text_deletion(Capability::DELETE_PRECEDING_TEXT);
  } else {
    capability.set_text_deletion(Capability::NO_TEXT_DELETION_CAPABILITY);
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
    [self updateComposedString:nullptr];
    [self clearCandidates];
  }
}

- (void)switchMode:(CompositionMode)new_mode client:(id)sender {
  if (mode_ == new_mode) {
    return;
  }

  if (new_mode == mozc::commands::DIRECT) {
    // Turn off the IME and commit the composing text.
    DLOG(INFO) << "Mode switch: HIRAGANA, KATAKANA, etc. -> DIRECT";
    KeyEvent keyEvent;
    Output output;
    keyEvent.set_special_key(mozc::commands::KeyEvent::OFF);
    mozcClient_->SendKey(keyEvent, &output);
    if (output.has_result()) {
      [self commitText:output.result().value().c_str() client:sender];
    }
    if ([composedString_ length] > 0) {
      [self updateComposedString:nullptr];
      [self clearCandidates];
    }
    mode_ = mozc::commands::DIRECT;
    return;
  }

  if (mode_ == mozc::commands::DIRECT) {
    // Turn on the IME as the mode is changed from DIRECT to an active mode.
    DLOG(INFO) << "Mode switch: DIRECT -> HIRAGANA, KATAKANA, etc.";
    KeyEvent keyEvent;
    Output output;
    keyEvent.set_special_key(mozc::commands::KeyEvent::ON);
    mozcClient_->SendKey(keyEvent, &output);
  }

  // Switch composition mode.
  DLOG(INFO) << "Switch composition mode.";
  SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SWITCH_COMPOSITION_MODE);
  command.set_composition_mode(new_mode);
  Output output;
  mozcClient_->SendCommand(command, &output);
  mode_ = new_mode;
}

- (void)switchDisplayMode {
  if (!CanDisplayModeSwitch(clientBundle_)) {
    return;
  }

  absl::string_view mode_id = GetModeId(mode_);
  [[self client] selectInputMode:[NSString stringWithUTF8String:mode_id.data()]];
}

- (void)commitText:(const char *)text client:(id)sender {
  if (text == nullptr) {
    return;
  }

  [sender insertText:[NSString stringWithUTF8String:text] replacementRange:replacementRange_];
  replacementRange_ = NSMakeRange(NSNotFound, 0);
}

- (void)launchWordRegisterTool:(id)client {
  ::setenv(mozc::kWordRegisterEnvironmentName, "", 1);
  if (CanSelectedRange(clientBundle_)) {
    NSRange selectedRange = [client selectedRange];
    if (selectedRange.location != NSNotFound && selectedRange.length != NSNotFound &&
        selectedRange.length > 0) {
      NSString *text = [[client attributedSubstringFromRange:selectedRange] string];
      if (text != nil) {
        ::setenv(mozc::kWordRegisterEnvironmentName, [text UTF8String], 1);
      }
    }
  }
  MacProcess::LaunchMozcTool("word_register_dialog");
}

- (void)invokeReconvert:(const SessionCommand *)command client:(id)sender {
  if (!CanSelectedRange(clientBundle_)) {
    return;
  }

  NSRange selectedRange = [sender selectedRange];
  if (selectedRange.location == NSNotFound || selectedRange.length == NSNotFound) {
    // the application does not support reconversion.
    return;
  }

  DLOG(INFO) << selectedRange.location << ", " << selectedRange.length;
  SessionCommand sending_command;
  Output output;
  sending_command = *command;

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
  if (!CanSelectedRange(clientBundle_)) {
    return;
  }

  NSRange selectedRange = [sender selectedRange];
  if (selectedRange.location == NSNotFound || selectedRange.length == NSNotFound ||
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
  if (output == nullptr) {
    return;
  }
  if (!output->consumed()) {
    return;
  }

  DLOG(INFO) << output->Utf8DebugString();
  if (output->has_url()) {
    NSString *url = [NSString stringWithUTF8String:output->url().c_str()];
    [self openLink:[NSURL URLWithString:url]];
  }

  if (output->has_result()) {
    [self commitText:output->result().value().c_str() client:sender];
  }

  // Handles deletion range.  We do not even handle it for some
  // applications to prevent application crashes.
  if (output->has_deletion_range() && CanSelectedRange(clientBundle_)) {
    if ([composedString_ length] == 0 && replacementRange_.location == NSNotFound) {
      NSRange selectedRange = [sender selectedRange];
      const mozc::commands::DeletionRange &deletion_range = output->deletion_range();
      if (selectedRange.location != NSNotFound || selectedRange.length != NSNotFound ||
          selectedRange.location + deletion_range.offset() > 0) {
        // The offset is a negative value.  See protocol/commands.proto for
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
    const SessionCommand &callback_command = output->callback().session_command();
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
// Currently MozcImkInputController uses handleEvent:client:
// method to handle key events.  It does not support inputText:client:
// nor inputText:key:modifiers:client:.
// Because MozcImkInputController does not use IMKCandidates,
// the following methods are not needed to implement:
//   candidates
//
// The meaning of these methods are described at:
// https://developer.apple.com/documentation/inputmethodkit/imkserverinput?language=objc

- (id)originalString:(id)sender {
  return originalString_;
}

- (void)updateComposedString:(const Preedit *)preedit {
  // If the last and the current composed string length is 0,
  // we don't update the composition.
  if (([composedString_ length] == 0) && ((preedit == nullptr || preedit->segment_size() == 0))) {
    return;
  }

  [composedString_ deleteCharactersInRange:NSMakeRange(0, [composedString_ length])];
  cursorPosition_ = -1;
  if (preedit != nullptr) {
    cursorPosition_ = preedit->cursor();
    for (size_t i = 0; i < preedit->segment_size(); ++i) {
      NSDictionary *highlightAttributes = [self markForStyle:kTSMHiliteSelectedConvertedText
                                                     atRange:NSMakeRange(NSNotFound, 0)];
      NSDictionary *underlineAttributes = [self markForStyle:kTSMHiliteConvertedText
                                                     atRange:NSMakeRange(NSNotFound, 0)];
      const Preedit::Segment &seg = preedit->segment(static_cast<int32_t>(i));
      NSDictionary *attr = (seg.annotation() == Preedit::Segment::HIGHLIGHT) ? highlightAttributes
                                                                             : underlineAttributes;
      NSString *seg_string = [NSString stringWithUTF8String:seg.value().c_str()];
      NSAttributedString *seg_attributed_string =
          [[NSAttributedString alloc] initWithString:seg_string attributes:attr];
      [composedString_ appendAttributedString:seg_attributed_string];
    }
  }
  if ([composedString_ length] == 0) {
    [originalString_ setString:@""];
    replacementRange_ = NSMakeRange(NSNotFound, 0);
  }

  // Update the composed string of the client applications.
  [[self client] setMarkedText:composedString_
                selectionRange:[self selectionRange]
              replacementRange:replacementRange_];
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
  [self updateComposedString:nullptr];
}

- (id)composedString:(id)sender {
  return composedString_;
}

- (void)clearCandidates {
  rendererCommand_.set_type(RendererCommand::UPDATE);
  rendererCommand_.set_visible(false);
  rendererCommand_.clear_output();
  if (mozcRenderer_) {
    mozcRenderer_->ExecCommand(rendererCommand_);
  }
}

// |selecrionRange| method is defined at IMKInputController class and
// means the position of cursor actually.
- (NSRange)selectionRange {
  if (imkClientForTest_) {
    return NSMakeRange(cursorPosition_, 0);
  }

  return (cursorPosition_ == -1)
             ? [super selectionRange]  // default behavior defined at super class
             : NSMakeRange(cursorPosition_, 0);
}

- (void)delayedUpdateCandidates {
  if (!mozcRenderer_) {
    return;
  }

  // If there is no candidate, the candidate window is closed.
  if (rendererCommand_.output().candidate_window().candidate_size() == 0) {
    rendererCommand_.set_visible(false);
    mozcRenderer_->ExecCommand(rendererCommand_);
    return;
  }

  // The candidate window position is not recalculated if the
  // candidate already appears on the screen.  Therefore, if a user
  // moves client application window by mouse, candidate window won't
  // follow the move of window.  This is done because:
  //  - some applications like Emacs or Google Chrome don't return the
  //    cursor position correctly.  The candidate window moves
  //    frequently with those application, which irritates users.
  //  - Kotoeri does this too.
  if (rendererCommand_.visible()) {
    // Call ExecCommand anyway to update other information like candidate words.
    mozcRenderer_->ExecCommand(rendererCommand_);
    return;
  }

  rendererCommand_.set_visible(true);

  NSRect preeditRect = NSZeroRect;
  const int32_t position = rendererCommand_.output().candidate_window().position();
  // Some applications throws error when we call attributesForCharacterIndex.
  DLOG(INFO) << "attributesForCharacterIndex: " << position;
  @try {
    NSDictionary *clientData = [[self client] attributesForCharacterIndex:position
                                                      lineHeightRectangle:&preeditRect];

    // IMKBaseline: Left-bottom of the composition.
    NSPoint baseline = [clientData[@"IMKBaseline"] pointValue];
    // IMKTextOrientation: 0: vertical writing, 1: horizontal writing.
    // IMKLineHeight: Height of the composition (in horizontal writing).
    // NSFont: Font information of the composition.
    // IMKLineAscent: Not sure. A float number. (e.g. 9.240234)

    const int right_offset = preeditRect.size.width;
    const int top_offset = -preeditRect.size.height;

    RendererCommand::Rectangle *rect = rendererCommand_.mutable_preedit_rectangle();
    rect->set_left(baseline.x);
    rect->set_right(baseline.x + right_offset);
    rect->set_top(baseline.y + top_offset);
    rect->set_bottom(baseline.y);

  } @catch (NSException *exception) {
    LOG(ERROR) << "Exception from [" << clientBundle_ << "] " << [[exception name] UTF8String]
               << "," << [[exception reason] UTF8String];
  }

  mozcRenderer_->ExecCommand(rendererCommand_);
}

- (void)updateCandidates:(const Output *)output {
  if (output == nullptr) {
    [self clearCandidates];
    return;
  }

  rendererCommand_.set_type(RendererCommand::UPDATE);
  *rendererCommand_.mutable_output() = *output;

  // Runs delayedUpdateCandidates in the next event loop.
  // This is because some applications like Google Docs with Chrome returns
  // incorrect cursor position if we call attributesForCharacterIndex here.
  [self performSelector:@selector(delayedUpdateCandidates) withObject:nil afterDelay:0];
}

- (void)openLink:(NSURL *)url {
  // Open a link specified by |url|.  Any opening link behavior should
  // call this method because it checks the capability of application.
  // On some application like login window of screensaver, opening
  // link behavior should not happen because it can cause some
  // security issues.
  if (CanOpenLink(clientBundle_)) {
    [[NSWorkspace sharedWorkspace] openURL:url];
  }
}

- (BOOL)fillSurroundingContext:(mozc::commands::Context *)context client:(id<IMKTextInput>)client {
  NSInteger totalLength = [client length];
  if (totalLength == 0 || totalLength == NSNotFound ||
      totalLength > kGetSurroundingTextClientLengthLimit) {
    return false;
  }
  NSRange selectedRange = [client selectedRange];
  if (selectedRange.location == NSNotFound || selectedRange.length == NSNotFound) {
    return false;
  }
  NSRange precedingRange = NSMakeRange(0, selectedRange.location);
  if (selectedRange.location > kMaxSurroundingLength) {
    precedingRange =
        NSMakeRange(selectedRange.location - kMaxSurroundingLength, kMaxSurroundingLength);
  }
  NSString *precedingString = [[client attributedSubstringFromRange:precedingRange] string];
  if (precedingString) {
    context->set_preceding_text([precedingString UTF8String]);
    DLOG(INFO) << "preceding_text: \"" << context->preceding_text() << "\"";
  }
  return true;
}

- (BOOL)handleEvent:(NSEvent *)event client:(id)sender {
  if (event == nullptr || [event isEqual:[NSNull null]]) {
    return NO;
  }
  if ([event type] == NSEventTypeCursorUpdate) {
    [[self client] setMarkedText:composedString_
                  selectionRange:[self selectionRange]
                replacementRange:replacementRange_];
    return NO;
  }
  if ([event type] != NSEventTypeKeyDown && [event type] != NSEventTypeFlagsChanged) {
    return NO;
  }

  // Handle KANA key and EISU key.  We explicitly handles this here
  // for mode switch because some text area such like iPhoto person
  // name editor does not call setValue:forTag:client: method.
  // see:
  // http://www.google.com/support/forum/p/ime/thread?tid=3aafb74ff71a1a69&hl=ja&fid=3aafb74ff71a1a690004aa3383bc9f5d
  if ([event type] == NSEventTypeKeyDown) {
    NSTimeInterval currentTime = [[NSDate date] timeIntervalSince1970];
    const NSTimeInterval elapsedTime = currentTime - lastKeyDownTime_;
    const bool isDoubleTap =
        ([event keyCode] == lastKeyCode_) && (elapsedTime < kDoubleTapInterval);
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
      CompositionMode new_mode =
          ([composedString_ length] == 0) ? mozc::commands::DIRECT : mozc::commands::HALF_ASCII;
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
  if (![keyCodeMap_ getMozcKeyCodeFromKeyEvent:event toMozcKeyEvent:&keyEvent]) {
    // Modifier flags change (not submitted to the server yet), or
    // unsupported key pressed.
    return NO;
  }

  // If the key event is turn on event, the key event has to be sent
  // to the server anyway.
  if (mode_ == mozc::commands::DIRECT && !mozcClient_->IsDirectModeCommand(keyEvent)) {
    // Yen sign special hack: although the current mode is DIRECT,
    // backslash is sent instead of yen sign for JIS yen key with no
    // modifiers.  This behavior is based on the configuration.
    if ([event keyCode] == kVK_JIS_Yen && [event modifierFlags] == 0 &&
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

  if ([composedString_ length] == 0 && CanSelectedRange(clientBundle_) &&
      CanSurroundingText(clientBundle_)) {
    [self fillSurroundingContext:&context client:sender];
  }
  if (!mozcClient_->SendKeyWithContext(keyEvent, context, &output)) {
    return NO;
  }

  [self processOutput:&output client:sender];
  return output.consumed();
}

#pragma mark ControllerCallback
- (void)sendCommand:(const SessionCommand &)command {
  Output output;
  if (!mozcClient_->SendCommand(command, &output)) {
    return;
  }

  [self processOutput:&output client:[self client]];
}

- (void)outputResult:(const mozc::commands::Output &)output {
  if (!output.has_result()) {
    return;
  }
  [self commitText:output.result().value().c_str() client:[self client]];
}

#pragma mark callbacks
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

- (IBAction)aboutDialogClicked:(id)sender {
  MacProcess::LaunchMozcTool("about_dialog");
}

+ (void)setGlobalRendererReceiver:(RendererReceiver *)rendererReceiver {
  gRendererReceiver = rendererReceiver;
}
@end

// An alias of MozcImkInputController for backward compatibility.
@implementation GoogleJapaneseInputController
@end
