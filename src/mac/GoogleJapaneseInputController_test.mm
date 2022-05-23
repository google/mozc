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

#import "mac/GoogleJapaneseInputController.h"

#import <Cocoa/Cocoa.h>

#import "mac/GoogleJapaneseInputControllerInterface.h"
#import "mac/KeyCodeMap.h"

#include <objc/objc-class.h>

#include "base/logging.h"
#include "base/mac_util.h"
#include "base/util.h"
#include "client/client_mock.h"
#include "protocol/candidates.pb.h"
#include "renderer/renderer_interface.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

@interface MockIMKServer : IMKServer <ServerCallback> {
  // The controller which accepts user's clicks
  __weak id<ControllerCallback> expectedController_;
  int setCurrentController_count_;
}
@property(readwrite, weak) id<ControllerCallback> expectedController;
@end

@implementation MockIMKServer
@synthesize expectedController = expectedController_;

- (MockIMKServer *)init {
  self = [super init];
  expectedController_ = nil;
  setCurrentController_count_ = 0;
  return self;
}

- (void)sendData:(NSData *)data {
  // Do nothing
}
- (void)outputResult:(NSData *)data {
  // Do nothing
}

- (void)setCurrentController:(id<ControllerCallback>)controller {
  EXPECT_EQ(expectedController_, controller);
  ++setCurrentController_count_;
}
@end

@interface MockClient : NSObject {
  __weak NSString *bundleIdentifier;
  NSRect expectedCursor;
  NSRange expectedRange;
  NSDictionary *expectedAttributes;
  std::string selectedMode_;
  NSString *insertedText_;
  NSString *overriddenLayout_;
  NSAttributedString *attributedString_;
  std::map<std::string, int> counters_;
}
@property(readwrite, weak) NSString *bundleIdentifier;
@property(readwrite, assign) NSRect expectedCursor;
@property(readwrite) NSDictionary *expectedAttributes;
@property(readwrite, assign) NSRange expectedRange;
@property(readonly) std::string selectedMode;
@property(readonly) NSString *insertedText;
@property(readonly) NSString *overriddenLayout;
@end

@implementation MockClient
@synthesize bundleIdentifier;
@synthesize expectedCursor;
@synthesize expectedAttributes;
@synthesize expectedRange;
@synthesize selectedMode = selectedMode_;
@synthesize insertedText = insertedText_;
@synthesize overriddenLayout = overriddenLayout_;

- (MockClient *)init {
  self = [super init];
  self.bundleIdentifier = @"com.google.exampleBundle";
  expectedRange = NSMakeRange(NSNotFound, NSNotFound);
  return self;
}

- (void)dealloc {
}

- (int)getCounter:(const char *)selector {
  return counters_[selector];
}

- (NSConnection *)connectionForProxy {
  return nil;
}

- (NSDictionary *)attributesForCharacterIndex:(int)index lineHeightRectangle:(NSRect *)rect {
  counters_["attributesForCharacterIndex:lineHeightRectangle:"]++;
  *rect = expectedCursor;
  return expectedAttributes;
}

- (NSRange)selectedRange {
  counters_["selectedRange"]++;
  return expectedRange;
}

- (NSRange)markedRange {
  counters_["markedRange"]++;
  return expectedRange;
}

- (void)selectInputMode:(NSString *)mode {
  counters_["selectInputMode:"]++;
  selectedMode_.assign([mode UTF8String]);
}

- (void)setAttributedString:(NSAttributedString *)newString {
  attributedString_ = [newString copy];
}

- (NSInteger)length {
  return [attributedString_ length];
}

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)range {
  return [attributedString_ attributedSubstringFromRange:range];
}

// a placeholder method which is necessary internally
- (void)setMarkedText:(id)string
       selectionRange:(NSRange)selectionRange
     replacementRange:(NSRange)replacementRange {
  // Do nothing
}

- (void)insertText:(NSString *)result replacementRange:(NSRange)range {
  counters_["insertText:replacementRange:"]++;
  insertedText_ = result;
}

- (void)overrideKeyboardWithKeyboardNamed:(NSString *)newLayout {
  counters_["overrideKeyboardWithKeyboardNamed:"]++;
  overriddenLayout_ = newLayout;
}
@end

namespace {
const NSTimeInterval kDoubleTapInterval = 0.5;
int openURL_count = 0;
BOOL openURL_test(id self, SEL selector, NSURL *url) {
  openURL_count++;
  return true;
}

}  // namespace

class MockRenderer : public mozc::renderer::RendererInterface {
 public:
  MockRenderer() : Activate_counter_(0), IsAvailable_counter_(0), ExecCommand_counter_(0) {}

  virtual bool Activate() {
    Activate_counter_++;
    return true;
  }
  int counter_Activate() const { return Activate_counter_; }

  virtual bool ExecCommand(const mozc::commands::RendererCommand &command) {
    ExecCommand_counter_++;
    called_command_.CopyFrom(command);
    return true;
  }
  int counter_ExecCommand() const { return ExecCommand_counter_; }
  const mozc::commands::RendererCommand &CalledCommand() const { return called_command_; }

  virtual bool IsAvailable() const {
    IsAvailable_counter_++;
    return true;
  }
  int counter_IsAvailable() const { return IsAvailable_counter_; }

#undef MockMethod

 private:
  int Activate_counter_;
  mutable int IsAvailable_counter_;
  int ExecCommand_counter_;
  mozc::commands::RendererCommand called_command_;
};

class GoogleJapaneseInputControllerTest : public testing::Test {
 protected:
  void SetUp() {
    mock_server_ = [[MockIMKServer alloc] init];
    mock_client_ = [[MockClient alloc] init];
    // setup workspace
    Method method = class_getInstanceMethod([NSWorkspace class], @selector(openURL:));
    method_setImplementation(method, reinterpret_cast<IMP>(openURL_test));
    openURL_count = 0;

    [GoogleJapaneseInputController initializeConstants];
    SetUpController();
  }

  void SetUpController() {
    controller_ = [[GoogleJapaneseInputController alloc] initWithServer:mock_server_
                                                               delegate:nil
                                                                 client:mock_client_];
    controller_.imkClientForTest = mock_client_;
    mock_server_.expectedController = controller_;
    mock_mozc_client_ = new mozc::client::ClientMock;
    controller_.mozcClient = mock_mozc_client_;
    mock_renderer_ = new MockRenderer;
    controller_.renderer = mock_renderer_;
  }

  void ResetClientBundleIdentifier(NSString *new_bundle_id) {
    controller_ = nil;
    mock_client_.bundleIdentifier = new_bundle_id;
    SetUpController();
  }

  mozc::client::ClientMock *mock_mozc_client_;
  MockClient *mock_client_;
  MockRenderer *mock_renderer_;

  GoogleJapaneseInputController *controller_;

 private:
  MockIMKServer *mock_server_;
};

// Because preedit has NSMarkedClauseSegment attribute which has to
// change for each call of creating preedit, we can't use simple
// isEqualToAttributedString method.
bool ComparePreedit(NSAttributedString *expected, NSAttributedString *actual) {
  if ([expected length] != [actual length]) {
    LOG(ERROR) << "length is different";
    return false;
  }

  if (![[expected string] isEqualToString:[actual string]]) {
    LOG(ERROR) << "content string is different";
    return false;
  }

  // Check the attributes
  int cursor = 0;
  NSSet *knownAttributeNames =
      [NSSet setWithObjects:NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName,
                            NSMarkedClauseSegmentAttributeName, nil];
  while (cursor < [expected length]) {
    NSRange expectedRange = NSMakeRange(NSNotFound, NSNotFound);
    NSRange actualRange = NSMakeRange(NSNotFound, NSNotFound);
    NSDictionary *expectedAttributes = [expected attributesAtIndex:cursor
                                                    effectiveRange:&expectedRange];
    NSDictionary *actualAttributes = [actual attributesAtIndex:cursor effectiveRange:&actualRange];
    // false if attribute over different range.
    if (expectedRange.location != actualRange.location ||
        expectedRange.length != actualRange.length) {
      LOG(ERROR) << "range is different";
      return false;
    }

    // Check attributes for underlines
    NSNumber *expectedUnderlineStyle =
        [expectedAttributes valueForKey:NSUnderlineStyleAttributeName];
    NSNumber *actualUnderlineStyle = [expectedAttributes valueForKey:NSUnderlineStyleAttributeName];
    if (![expectedUnderlineStyle isEqualToNumber:actualUnderlineStyle] ||
        actualUnderlineStyle == nil) {
      LOG(ERROR) << "underline style is different at range (" << expectedRange.location << ", "
                 << expectedRange.length << ")";
      return false;
    }
    NSColor *expectedUnderlineColor =
        [expectedAttributes valueForKey:NSUnderlineColorAttributeName];
    NSColor *actualUnderlineColor = [actualAttributes valueForKey:NSUnderlineColorAttributeName];
    if (![[NSString stringWithFormat:@"%@", expectedUnderlineColor]
            isEqualToString:[NSString stringWithFormat:@"%@", actualUnderlineColor]]) {
      LOG(ERROR) << "underline color is different at range (" << expectedRange.location << ", "
                 << expectedRange.length << ")";
      return false;
    }

    // Check if it contains unknown attributes
    for (NSString *key in [expectedAttributes keyEnumerator]) {
      if (![knownAttributeNames containsObject:key]) {
        LOG(ERROR) << "expected value contains an unknown key: " << [key UTF8String];
        return false;
      }
    }
    for (NSString *key in [actualAttributes keyEnumerator]) {
      if (![knownAttributeNames containsObject:key]) {
        LOG(ERROR) << "actual value contains an unknown key: " << [key UTF8String];
        return false;
      }
    }
    cursor += expectedRange.length;
  }

  // true if it passes every check
  return true;
}

NSTimeInterval GetDoubleTapInterval() {
  // TODO(horo): This value should be get from system configuration.
  return kDoubleTapInterval;
}

BOOL SendKeyEvent(unsigned short keyCode, GoogleJapaneseInputController *controller,
                  MockClient *client) {
  // tap Kana-key
  NSEvent *kanaKeyEvent = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                           location:NSZeroPoint
                                      modifierFlags:0
                                          timestamp:0
                                       windowNumber:0
                                            context:nil
                                         characters:@" "
                        charactersIgnoringModifiers:@" "
                                          isARepeat:NO
                                            keyCode:keyCode];
  return [controller handleEvent:kanaKeyEvent client:client];
}

TEST_F(GoogleJapaneseInputControllerTest, UpdateComposedString) {
  // If preedit is nullptr, it still calls setMarkedText, with an empty string.
  NSMutableAttributedString *expected = [[NSMutableAttributedString alloc] initWithString:@""];
  [controller_ updateComposedString:nullptr];
  EXPECT_TRUE([expected isEqualToAttributedString:[controller_ composedString:nil]]);

  // If preedit has some data, it returns a data with highlighting information
  mozc::commands::Preedit preedit;
  mozc::commands::Preedit::Segment *new_segment = preedit.add_segment();
  // UNDERLINE segment
  new_segment->set_annotation(mozc::commands::Preedit::Segment::UNDERLINE);
  new_segment->set_value("a");
  new_segment->set_value_length(1);
  new_segment = preedit.add_segment();
  // Second segment is HIGHLIGHT
  new_segment->set_annotation(mozc::commands::Preedit::Segment::HIGHLIGHT);
  new_segment->set_value("bc");
  new_segment->set_value_length(2);
  // Third segment is NONE annotation but it seems same as UNDERLINE
  // at this time
  new_segment = preedit.add_segment();
  new_segment->set_annotation(mozc::commands::Preedit::Segment::NONE);
  new_segment->set_value("def");
  new_segment->set_value_length(2);

  NSDictionary *highlightAttributes = [controller_ markForStyle:kTSMHiliteSelectedConvertedText
                                                        atRange:NSMakeRange(NSNotFound, 0)];
  NSDictionary *underlineAttributes = [controller_ markForStyle:kTSMHiliteConvertedText
                                                        atRange:NSMakeRange(NSNotFound, 0)];
  expected = [[NSMutableAttributedString alloc] initWithString:@"abcdef"];
  [expected addAttributes:underlineAttributes range:NSMakeRange(0, 1)];
  [expected addAttributes:highlightAttributes range:NSMakeRange(1, 2)];
  [expected addAttributes:underlineAttributes range:NSMakeRange(3, 3)];
  [controller_ updateComposedString:&preedit];
  NSAttributedString *actual = [controller_ composedString:nil];
  EXPECT_TRUE(ComparePreedit(expected, actual))
      << [[NSString stringWithFormat:@"expected:%@ actual:%@", expected, actual] UTF8String];
}

TEST_F(GoogleJapaneseInputControllerTest, ClearCandidates) {
  [controller_ clearCandidates];
  EXPECT_EQ(1, mock_renderer_->counter_ExecCommand());
  // After clearing candidates, the candidate window has to be invisible.
  EXPECT_FALSE(mock_renderer_->CalledCommand().visible());
}

TEST_F(GoogleJapaneseInputControllerTest, UpdateCandidates) {
  // When output is null, same as ClearCandidate
  [controller_ updateCandidates:nullptr];
  // Run the runloop so "delayedUpdateCandidates" can be called
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
  EXPECT_EQ(1, mock_renderer_->counter_ExecCommand());
  EXPECT_FALSE(mock_renderer_->CalledCommand().visible());
  EXPECT_EQ(0, [mock_client_ getCounter:"attributesForCharacterIndex:lineHeightRectangle:"]);

  // create an output
  mozc::commands::Output output;
  // a candidate has to have at least a candidate
  mozc::commands::Candidates *candidates = output.mutable_candidates();
  candidates->set_focused_index(0);
  candidates->set_size(1);
  mozc::commands::Candidates::Candidate *candidate = candidates->add_candidate();
  candidate->set_index(0);
  candidate->set_value("abc");

  // setup the cursor position
  mock_client_.expectedCursor = NSMakeRect(50, 50, 1, 10);
  mock_client_.expectedAttributes =
      @{@"IMKBaseline" : [NSValue valueWithPoint:NSMakePoint(50, 718)]};
  [controller_ updateCandidates:&output];
  // Run the runloop so "delayedUpdateCandidates" can be called
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
  EXPECT_EQ(1, [mock_client_ getCounter:"attributesForCharacterIndex:lineHeightRectangle:"]);
  const mozc::commands::RendererCommand *rendererCommand = controller_.rendererCommand;
  EXPECT_EQ(2, mock_renderer_->counter_ExecCommand());
  EXPECT_TRUE(rendererCommand->visible());
  const mozc::commands::RendererCommand::Rectangle &preedit_rectangle =
      rendererCommand->preedit_rectangle();
  EXPECT_EQ(50, preedit_rectangle.left());
  EXPECT_EQ(708, preedit_rectangle.top());
  EXPECT_EQ(51, preedit_rectangle.right());
  EXPECT_EQ(718, preedit_rectangle.bottom());

  // reshow the candidate window again -- but cursor position has changed.
  mock_client_.expectedCursor = NSMakeRect(60, 50, 1, 10);
  [controller_ updateCandidates:&output];
  // Run the runloop so "delayedUpdateCandidates" can be called
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
  // Does not change: not call again
  EXPECT_EQ(1, [mock_client_ getCounter:"attributesForCharacterIndex:lineHeightRectangle:"]);
  rendererCommand = controller_.rendererCommand;
  EXPECT_EQ(3, mock_renderer_->counter_ExecCommand());
  EXPECT_TRUE(rendererCommand->visible());
  // Does not change the position
  EXPECT_EQ(preedit_rectangle.DebugString(), rendererCommand->preedit_rectangle().DebugString());

  // Then update without candidate entries -- goes invisible.
  output.Clear();
  [controller_ updateCandidates:&output];
  // Run the runloop so "delayedUpdateCandidates" can be called
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
  // Does not change: not call again
  EXPECT_EQ(1, [mock_client_ getCounter:"attributesForCharacterIndex:lineHeightRectangle:"]);
  EXPECT_EQ(4, mock_renderer_->counter_ExecCommand());
  EXPECT_FALSE(rendererCommand->visible());
}

TEST_F(GoogleJapaneseInputControllerTest, OpenLink) {
  EXPECT_EQ(0, openURL_count);
  [controller_ openLink:[NSURL URLWithString:@"http://www.example.com/"]];
  // openURL is invoked
  EXPECT_EQ(1, openURL_count);
  // Change to the unsecure bundle ID
  ResetClientBundleIdentifier(@"com.apple.securityagent");
  // The counter does not change during the initialization
  EXPECT_EQ(1, openURL_count);
  [controller_ openLink:[NSURL URLWithString:@"http://www.example.com/"]];
  // openURL is not invoked
  EXPECT_EQ(1, openURL_count);
}

TEST_F(GoogleJapaneseInputControllerTest, SwitchModeToDirect) {
  // setup the IME status
  controller_.mode = mozc::commands::HIRAGANA;
  mozc::commands::Preedit preedit;
  mozc::commands::Preedit::Segment *new_segment = preedit.add_segment();
  new_segment->set_annotation(mozc::commands::Preedit::Segment::UNDERLINE);
  new_segment->set_value("abcdef");
  new_segment->set_value_length(6);
  [controller_ updateComposedString:&preedit];
  mozc::commands::Output output;
  output.mutable_result()->set_type(mozc::commands::Result::STRING);
  output.mutable_result()->set_value("foo");
  mock_mozc_client_->set_output_SendKeyWithContext(output);

  [controller_ switchModeToDirect:mock_client_];
  EXPECT_EQ(mozc::commands::DIRECT, controller_.mode);
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  EXPECT_TRUE(mock_mozc_client_->called_SendKeyWithContext().has_special_key());
  EXPECT_EQ(mozc::commands::KeyEvent::OFF,
            mock_mozc_client_->called_SendKeyWithContext().special_key());
  EXPECT_EQ(1, [mock_client_ getCounter:"insertText:replacementRange:"]);
  EXPECT_TRUE([@"foo" isEqualToString:mock_client_.insertedText]);
  EXPECT_EQ(0, [[controller_ composedString:nil] length]);
  EXPECT_FALSE(controller_.rendererCommand->visible());
}

TEST_F(GoogleJapaneseInputControllerTest, SwitchModeInternal) {
  // When a mode changes from DIRECT, it should invoke "ON" command beforehand.
  controller_.mode = mozc::commands::DIRECT;
  [controller_ switchModeInternal:mozc::commands::HIRAGANA];
  EXPECT_EQ(mozc::commands::HIRAGANA, controller_.mode);
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  EXPECT_TRUE(mock_mozc_client_->called_SendKeyWithContext().has_special_key());
  EXPECT_EQ(mozc::commands::KeyEvent::ON,
            mock_mozc_client_->called_SendKeyWithContext().special_key());
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::SWITCH_INPUT_MODE,
            mock_mozc_client_->called_SendCommandWithContext().type());
  EXPECT_EQ(mozc::commands::HIRAGANA,
            mock_mozc_client_->called_SendCommandWithContext().composition_mode());

  // Switch from HIRAGANA to KATAKANA.  Just sending mode switching command.
  controller_.mode = mozc::commands::HIRAGANA;
  [controller_ switchModeInternal:mozc::commands::HALF_KATAKANA];
  EXPECT_EQ(mozc::commands::HALF_KATAKANA, controller_.mode);
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  EXPECT_EQ(2, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::SWITCH_INPUT_MODE,
            mock_mozc_client_->called_SendCommandWithContext().type());
  EXPECT_EQ(mozc::commands::HALF_KATAKANA,
            mock_mozc_client_->called_SendCommandWithContext().composition_mode());

  // going to same mode does not cause sendcommand
  [controller_ switchModeInternal:mozc::commands::HALF_KATAKANA];
  EXPECT_EQ(mozc::commands::HALF_KATAKANA, controller_.mode);
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  EXPECT_EQ(2, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
}

TEST_F(GoogleJapaneseInputControllerTest, SwitchDisplayMode) {
  EXPECT_TRUE(mock_client_.selectedMode.empty());
  EXPECT_EQ(mozc::commands::DIRECT, controller_.mode);
  [controller_ switchDisplayMode];
  EXPECT_EQ(1, [mock_client_ getCounter:"selectInputMode:"]);
  std::string expected = mozc::MacUtil::GetLabelForSuffix("Roman");
  EXPECT_EQ(expected, mock_client_.selectedMode);

  // Does not change the display mode for MS Word.  See
  // GoogleJapaneseInputController.mm for the detailed information.
  ResetClientBundleIdentifier(@"com.microsoft.Word");
  [controller_ switchMode:mozc::commands::HIRAGANA client:mock_client_];
  EXPECT_EQ(mozc::commands::HIRAGANA, controller_.mode);
  [controller_ switchDisplayMode];
  // still remains 1 and display mode does not change.
  EXPECT_EQ(1, [mock_client_ getCounter:"selectInputMode:"]);
  expected = mozc::MacUtil::GetLabelForSuffix("Roman");
  EXPECT_EQ(expected, mock_client_.selectedMode);
}

TEST_F(GoogleJapaneseInputControllerTest, commitText) {
  controller_.replacementRange = NSMakeRange(0, 1);
  [controller_ commitText:"foo" client:mock_client_];

  EXPECT_EQ(1, [mock_client_ getCounter:"insertText:replacementRange:"]);
  EXPECT_TRUE([@"foo" isEqualToString:mock_client_.insertedText]);
  // location has to be cleared after the commit.
  EXPECT_EQ(NSNotFound, [controller_ replacementRange].location);
}

TEST_F(GoogleJapaneseInputControllerTest, handleConfig) {
  // Does not support multiple-calculation
  mozc::config::Config config;
  config.set_preedit_method(mozc::config::Config::KANA);
  config.set_yen_sign_character(mozc::config::Config::BACKSLASH);
  config.set_use_japanese_layout(true);
  mock_mozc_client_->SetConfig(config);
  mock_mozc_client_->SetBoolFunctionReturn("GetConfig", true);

  [controller_ handleConfig];
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("GetConfig"));
  EXPECT_EQ(KANA, controller_.keyCodeMap.inputMode);
  EXPECT_EQ(mozc::config::Config::BACKSLASH, controller_.yenSignCharacter);
  EXPECT_EQ(1, [mock_client_ getCounter:"overrideKeyboardWithKeyboardNamed:"]);
  EXPECT_TRUE([@"com.apple.keylayout.US" isEqualToString:mock_client_.overriddenLayout])
      << [mock_client_.overriddenLayout UTF8String];
}

TEST_F(GoogleJapaneseInputControllerTest, DoubleTapKanaReconvert) {
  // tap (short) tap -> emit undo command
  controller_.mode = mozc::commands::HIRAGANA;

  // set attributedString for Reconvert
  [mock_client_ setAttributedString:[[NSAttributedString alloc] initWithString:@"abcde"]];
  mock_client_.expectedRange = NSMakeRange(1, 3);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it did not send command.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));

  // Sleep less than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 / 2.0);

  // Send Kana-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it sent an undo command.
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::CONVERT_REVERSE,
            mock_mozc_client_->called_SendCommandWithContext().type());
  EXPECT_EQ("bcd", mock_mozc_client_->called_SendCommandWithContext().text());
}

TEST_F(GoogleJapaneseInputControllerTest, DoubleTapKanaUndo) {
  // tap (short) tap -> emit undo command
  controller_.mode = mozc::commands::HIRAGANA;

  // set expectedRange for Undo
  mock_client_.expectedRange = NSMakeRange(1, 0);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it did not send command.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));

  // Sleep less than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 / 2.0);

  // Send Kana-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it sent an undo command.
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::UNDO,
            mock_mozc_client_->called_SendCommandWithContext().type());
}

TEST_F(GoogleJapaneseInputControllerTest, DoubleTapKanaUndoTimeOver) {
  // tap (long) tap -> don't emit undo command
  controller_.mode = mozc::commands::HIRAGANA;

  // set expectedRange for Undo
  mock_client_.expectedRange = NSMakeRange(1, 0);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it did not send command.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));

  // Sleep more than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 * 2.0);

  // Send Kana-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it did not send command.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
}

TEST_F(GoogleJapaneseInputControllerTest, SingleAndDoubleTapKanaUndo) {
  // tap (long) tap (short) tap -> emit once
  controller_.mode = mozc::commands::HIRAGANA;

  // set expectedRange for Undo
  mock_client_.expectedRange = NSMakeRange(1, 0);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it did not send command.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));

  // Sleep more than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 * 2.0);

  // Send Kana-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it did not send command.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));

  // Sleep less than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 / 2.0);

  // Send Kana-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it sent an undo command.
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::UNDO,
            mock_mozc_client_->called_SendCommandWithContext().type());
}

TEST_F(GoogleJapaneseInputControllerTest, TripleTapKanaUndo) {
  // tap (short) tap (short) tap -> emit twice
  controller_.mode = mozc::commands::HIRAGANA;

  // set expectedRange for Undo
  mock_client_.expectedRange = NSMakeRange(1, 0);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it did not send command.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));

  // Sleep less than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 / 2.0);

  // Send Kana-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it sent an undo command.
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::UNDO,
            mock_mozc_client_->called_SendCommandWithContext().type());

  // Sleep less than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 / 2.0);

  // Send Kana-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it sent an undo command.
  EXPECT_EQ(2, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::UNDO,
            mock_mozc_client_->called_SendCommandWithContext().type());
}

TEST_F(GoogleJapaneseInputControllerTest, QuadrupleTapKanaUndo) {
  // tap (short) tap (short) tap (short) tap -> emit thrice
  controller_.mode = mozc::commands::HIRAGANA;

  // set expectedRange for Undo
  mock_client_.expectedRange = NSMakeRange(1, 0);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it did not send command.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));

  // Sleep less than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 / 2.0);

  // Send Kana-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it sent an undo command.
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::UNDO,
            mock_mozc_client_->called_SendCommandWithContext().type());

  // Sleep less than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 / 2.0);

  // Send Kana-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it sent an undo command.
  EXPECT_EQ(2, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::UNDO,
            mock_mozc_client_->called_SendCommandWithContext().type());

  // Sleep less than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 / 2.0);

  // Send Kana-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it sent an undo command.
  EXPECT_EQ(3, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::UNDO,
            mock_mozc_client_->called_SendCommandWithContext().type());
}

TEST_F(GoogleJapaneseInputControllerTest, DoubleTapEisuCommitRawText) {
  // Send Eisu-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Eisu, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it did not send command.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));

  // Sleep less than DoubleTapInterval (sec)
  mozc::Util::Sleep(GetDoubleTapInterval() * 1000.0 / 2.0);

  // Send Eisu-key.
  EXPECT_EQ(YES, SendKeyEvent(kVK_JIS_Eisu, controller_, mock_client_));
  // Check if it did not send key.
  EXPECT_EQ(0, mock_mozc_client_->GetFunctionCallCount("SendKeyWithContext"));
  // Check if it sent an undo command.
  EXPECT_EQ(1, mock_mozc_client_->GetFunctionCallCount("SendCommandWithContext"));
  EXPECT_EQ(mozc::commands::SessionCommand::COMMIT_RAW_TEXT,
            mock_mozc_client_->called_SendCommandWithContext().type());
}

TEST_F(GoogleJapaneseInputControllerTest, fillSurroundingContext) {
  [mock_client_ setAttributedString:[[NSAttributedString alloc] initWithString:@"abcde"]];
  mock_client_.expectedRange = NSMakeRange(2, 1);
  mozc::commands::Context context;
  [controller_ fillSurroundingContext:&context client:(id)mock_client_];
  EXPECT_EQ("ab", context.preceding_text());
  EXPECT_EQ(1, [mock_client_ getCounter:"selectedRange"]);
  EXPECT_EQ(0, [mock_client_ getCounter:"markedRange"]);

  [mock_client_ setAttributedString:[[NSAttributedString alloc]
                                        initWithString:@"012345678901234567890abcde"]];

  mock_client_.expectedRange = NSMakeRange(1, 0);
  [controller_ fillSurroundingContext:&context client:(id)mock_client_];
  EXPECT_EQ("0", context.preceding_text());
  EXPECT_EQ(2, [mock_client_ getCounter:"selectedRange"]);
  EXPECT_EQ(0, [mock_client_ getCounter:"markedRange"]);

  mock_client_.expectedRange = NSMakeRange(22, 1);
  [controller_ fillSurroundingContext:&context client:(id)mock_client_];
  EXPECT_EQ("2345678901234567890a", context.preceding_text());
  EXPECT_EQ(3, [mock_client_ getCounter:"selectedRange"]);
  EXPECT_EQ(0, [mock_client_ getCounter:"markedRange"]);

  [mock_client_ setAttributedString:[[NSAttributedString alloc] initWithString:@"012abc345"]];
  mozc::commands::Preedit preedit;
  mozc::commands::Preedit::Segment *new_segment = preedit.add_segment();
  new_segment->set_annotation(mozc::commands::Preedit::Segment::HIGHLIGHT);
  new_segment->set_value("abc");
  new_segment->set_value_length(3);
  [controller_ updateComposedString:&preedit];
  mock_client_.expectedRange = NSMakeRange(3, 3);
  [controller_ fillSurroundingContext:&context client:(id)mock_client_];
  EXPECT_EQ("012", context.preceding_text());
  EXPECT_EQ(4, [mock_client_ getCounter:"selectedRange"]);
  EXPECT_EQ(0, [mock_client_ getCounter:"markedRange"]);
}
