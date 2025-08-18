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

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

#import "mac/KeyCodeMap.h"

#include <objc/objc-class.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/mac/mac_util.h"
#include "client/client_mock.h"
#include "protocol/candidate_window.pb.h"
#include "renderer/renderer_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"

@interface MockIMKServer : NSObject <ServerCallback> {
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
  EXPECT_EQ(controller, expectedController_);
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
  absl::flat_hash_map<std::string, int> counters_;
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

namespace mozc {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;

MATCHER_P(HasSpecialKey, key, "") { return arg.has_special_key() && arg.special_key() == key; }
MATCHER_P(Type, type, "") { return arg.type() == type; }
MATCHER_P(CompositionMode, mode, "") { return arg.composition_mode() == mode; }
MATCHER_P(Text, text, "") { return arg.text() == text; }

constexpr NSTimeInterval kDoubleTapInterval = 0.5;
int gOpenURLCount = 0;
BOOL openURL_test(id self, SEL selector, NSURL *url) {
  gOpenURLCount++;
  return true;
}

class MockRenderer : public renderer::RendererInterface {
 public:
  MockRenderer() : Activate_counter_(0), IsAvailable_counter_(0), ExecCommand_counter_(0) {}

  bool Activate() override {
    Activate_counter_++;
    return true;
  }
  int counter_Activate() const { return Activate_counter_; }

  bool ExecCommand(const commands::RendererCommand &command) override {
    ExecCommand_counter_++;
    called_command_ = command;
    return true;
  }
  int counter_ExecCommand() const { return ExecCommand_counter_; }
  const commands::RendererCommand &CalledCommand() const { return called_command_; }

  bool IsAvailable() const override {
    IsAvailable_counter_++;
    return true;
  }
  int counter_IsAvailable() const { return IsAvailable_counter_; }

#undef MockMethod

 private:
  int Activate_counter_;
  mutable int IsAvailable_counter_;
  int ExecCommand_counter_;
  commands::RendererCommand called_command_;
};

class MozcImkInputControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_server_ = [[MockIMKServer alloc] init];
    mock_client_ = [[MockClient alloc] init];
    // setup workspace
    Method method = class_getInstanceMethod([NSWorkspace class], @selector(openURL:));
    method_setImplementation(method, reinterpret_cast<IMP>(openURL_test));
    gOpenURLCount = 0;

    SetUpController();
  }

  void TearDown() override {
    [controller_ setMozcClient:std::unique_ptr<mozc::client::ClientMock>()];
    [controller_ setRenderer:std::unique_ptr<MockRenderer>()];
    mock_renderer_ = nullptr;
    mock_mozc_client_ = nullptr;
    mock_server_ = nil;
    mock_client_ = nil;
    controller_ = nil;
  }

  void SetUpController() {
    // Initialize MozcImkInputController for testing by setting initWithServer to nil.
    controller_ = [[MozcImkInputController alloc] initWithServer:nil
                                                        delegate:nil
                                                          client:mock_client_];
    mock_server_.expectedController = controller_;
    auto mock_mozc_client = std::make_unique<mozc::client::ClientMock>();
    mock_mozc_client_ = mock_mozc_client.get();
    [controller_ setMozcClient:std::move(mock_mozc_client)];
    auto mock_renderer = std::make_unique<MockRenderer>();
    mock_renderer_ = mock_renderer.get();
    [controller_ setRenderer:std::move(mock_renderer)];
  }

  void ResetClientBundleIdentifier(NSString *new_bundle_id) {
    controller_ = nil;
    mock_client_.bundleIdentifier = new_bundle_id;
    SetUpController();
  }

  client::ClientMock *mock_mozc_client_;
  MockClient *mock_client_;
  const MockRenderer *mock_renderer_;

  MozcImkInputController *controller_;

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
  NSSet<NSAttributedStringKey> *knownAttributeNames =
      [NSSet setWithObjects:NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName,
                            NSMarkedClauseSegmentAttributeName, nil];
  while (cursor < [expected length]) {
    NSRange expectedRange = NSMakeRange(NSNotFound, NSNotFound);
    NSRange actualRange = NSMakeRange(NSNotFound, NSNotFound);
    NSDictionary<NSAttributedStringKey, id> *expectedAttributes =
        [expected attributesAtIndex:cursor effectiveRange:&expectedRange];
    NSDictionary<NSAttributedStringKey, id> *actualAttributes =
        [actual attributesAtIndex:cursor effectiveRange:&actualRange];
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

BOOL SendKeyEvent(unsigned short keyCode, MozcImkInputController *controller, MockClient *client) {
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

TEST_F(MozcImkInputControllerTest, UpdateComposedString) {
  // If preedit is nullptr, it still calls setMarkedText, with an empty string.
  NSMutableAttributedString *expected = [[NSMutableAttributedString alloc] initWithString:@""];
  [controller_ updateComposedString:nullptr];
  EXPECT_TRUE([expected isEqualToAttributedString:[controller_ composedString:nil]]);

  // If preedit has some data, it returns a data with highlighting information
  commands::Preedit preedit;
  commands::Preedit::Segment *new_segment = preedit.add_segment();
  // UNDERLINE segment
  new_segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
  new_segment->set_value("a");
  new_segment->set_value_length(1);
  new_segment = preedit.add_segment();
  // Second segment is HIGHLIGHT
  new_segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
  new_segment->set_value("bc");
  new_segment->set_value_length(2);
  // Third segment is NONE annotation but it seems same as UNDERLINE
  // at this time
  new_segment = preedit.add_segment();
  new_segment->set_annotation(commands::Preedit::Segment::NONE);
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

TEST_F(MozcImkInputControllerTest, ClearCandidates) {
  [controller_ clearCandidates];
  EXPECT_EQ(mock_renderer_->counter_ExecCommand(), 1);
  // After clearing candidates, the candidate window has to be invisible.
  EXPECT_FALSE(mock_renderer_->CalledCommand().visible());
}

TEST_F(MozcImkInputControllerTest, UpdateCandidates) {
  // When output is null, same as ClearCandidate
  [controller_ updateCandidates:nullptr];
  // Run the runloop so "delayedUpdateCandidates" can be called
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
  EXPECT_EQ(mock_renderer_->counter_ExecCommand(), 1);
  EXPECT_FALSE(mock_renderer_->CalledCommand().visible());
  EXPECT_EQ([mock_client_ getCounter:"attributesForCharacterIndex:lineHeightRectangle:"], 0);

  // create an output
  commands::Output output;
  // a candidate has to have at least a candidate
  commands::CandidateWindow *candidate_window = output.mutable_candidate_window();
  candidate_window->set_focused_index(0);
  candidate_window->set_size(1);
  commands::CandidateWindow::Candidate *candidate = candidate_window->add_candidate();
  candidate->set_index(0);
  candidate->set_value("abc");

  // setup the cursor position
  mock_client_.expectedCursor = NSMakeRect(50, 50, 1, 10);
  mock_client_.expectedAttributes =
      @{@"IMKBaseline" : [NSValue valueWithPoint:NSMakePoint(50, 718)]};
  [controller_ updateCandidates:&output];
  // Run the runloop so "delayedUpdateCandidates" can be called
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
  EXPECT_EQ([mock_client_ getCounter:"attributesForCharacterIndex:lineHeightRectangle:"], 1);
  const commands::RendererCommand &rendererCommand = controller_.rendererCommand;
  EXPECT_EQ(mock_renderer_->counter_ExecCommand(), 2);
  EXPECT_TRUE(rendererCommand.visible());
  const commands::RendererCommand::Rectangle &preedit_rectangle =
      rendererCommand.preedit_rectangle();
  EXPECT_EQ(preedit_rectangle.left(), 50);
  EXPECT_EQ(preedit_rectangle.top(), 708);
  EXPECT_EQ(preedit_rectangle.right(), 51);
  EXPECT_EQ(preedit_rectangle.bottom(), 718);

  // reshow the candidate window again -- but cursor position has changed.
  mock_client_.expectedCursor = NSMakeRect(60, 50, 1, 10);
  [controller_ updateCandidates:&output];
  // Run the runloop so "delayedUpdateCandidates" can be called
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
  // Does not change: not call again
  EXPECT_EQ([mock_client_ getCounter:"attributesForCharacterIndex:lineHeightRectangle:"], 1);
  EXPECT_EQ(mock_renderer_->counter_ExecCommand(), 3);
  EXPECT_TRUE(rendererCommand.visible());
  // Does not change the position
  EXPECT_EQ(rendererCommand.preedit_rectangle().DebugString(), preedit_rectangle.DebugString());

  // Then update without candidate entries -- goes invisible.
  output.Clear();
  [controller_ updateCandidates:&output];
  // Run the runloop so "delayedUpdateCandidates" can be called
  [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
  // Does not change: not call again
  EXPECT_EQ([mock_client_ getCounter:"attributesForCharacterIndex:lineHeightRectangle:"], 1);
  EXPECT_EQ(mock_renderer_->counter_ExecCommand(), 4);
  EXPECT_FALSE(rendererCommand.visible());
}

TEST_F(MozcImkInputControllerTest, OpenLink) {
  EXPECT_EQ(gOpenURLCount, 0);
  [controller_ openLink:[NSURL URLWithString:@"http://www.example.com/"]];
  // openURL is invoked
  EXPECT_EQ(gOpenURLCount, 1);
  // Change to the unsecure bundle ID
  ResetClientBundleIdentifier(@"com.apple.securityagent");
  // The counter does not change during the initialization
  EXPECT_EQ(gOpenURLCount, 1);
  [controller_ openLink:[NSURL URLWithString:@"http://www.example.com/"]];
  // openURL is not invoked
  EXPECT_EQ(gOpenURLCount, 1);
}

TEST_F(MozcImkInputControllerTest, SwitchModeToDirect) {
  // setup the IME status
  controller_.mode = commands::HIRAGANA;
  commands::Preedit preedit;
  commands::Preedit::Segment *new_segment = preedit.add_segment();
  new_segment->set_annotation(commands::Preedit::Segment::UNDERLINE);
  new_segment->set_value("abcdef");
  new_segment->set_value_length(6);
  [controller_ updateComposedString:&preedit];
  commands::Output output;
  output.mutable_result()->set_type(commands::Result::STRING);
  output.mutable_result()->set_value("foo");
  EXPECT_CALL(*mock_mozc_client_,
              SendKeyWithContext(HasSpecialKey(commands::KeyEvent::OFF), _, NotNull()))
      .WillOnce(DoAll(SetArgPointee<2>(output), Return(true)));

  [controller_ switchMode:commands::DIRECT client:mock_client_];
  EXPECT_EQ(controller_.mode, commands::DIRECT);
  EXPECT_EQ([mock_client_ getCounter:"insertText:replacementRange:"], 1);
  EXPECT_TRUE([@"foo" isEqualToString:mock_client_.insertedText]);
  EXPECT_EQ([[controller_ composedString:nil] length], 0);
  EXPECT_FALSE(controller_.rendererCommand.visible());
}

TEST_F(MozcImkInputControllerTest, SwitchMode) {
  // When a mode changes from DIRECT, it should invoke "ON" command beforehand.
  EXPECT_CALL(*mock_mozc_client_,
              SendKeyWithContext(HasSpecialKey(commands::KeyEvent::ON), _, NotNull()))
      .WillOnce(Return(true));
  commands::SessionCommand actual_command;
  EXPECT_CALL(*mock_mozc_client_, SendCommandWithContext(_, _, NotNull()))
      .Times(2)
      .WillRepeatedly(DoAll(SaveArg<0>(&actual_command), Return(true)));

  controller_.mode = commands::DIRECT;
  [controller_ switchMode:commands::HIRAGANA client:mock_client_];
  EXPECT_EQ(controller_.mode, commands::HIRAGANA);
  EXPECT_THAT(actual_command, Type(commands::SessionCommand::SWITCH_INPUT_MODE));
  EXPECT_THAT(actual_command, CompositionMode(commands::HIRAGANA));

  // Switch from HIRAGANA to KATAKANA.  Just sending mode switching command.
  controller_.mode = commands::HIRAGANA;
  [controller_ switchMode:commands::HALF_KATAKANA client:mock_client_];
  EXPECT_EQ(controller_.mode, commands::HALF_KATAKANA);
  EXPECT_THAT(actual_command, Type(commands::SessionCommand::SWITCH_INPUT_MODE));
  EXPECT_THAT(actual_command, CompositionMode(commands::HALF_KATAKANA));
  Mock::VerifyAndClearExpectations(&mock_mozc_client_);

  // going to same mode does not cause sendcommand
  [controller_ switchMode:commands::HALF_KATAKANA client:mock_client_];
  EXPECT_EQ(controller_.mode, commands::HALF_KATAKANA);
}

TEST_F(MozcImkInputControllerTest, SwitchDisplayMode) {
  EXPECT_TRUE(mock_client_.selectedMode.empty());
  EXPECT_EQ(controller_.mode, commands::DIRECT);
  [controller_ switchDisplayMode];
  EXPECT_EQ([mock_client_ getCounter:"selectInputMode:"], 1);
  EXPECT_EQ(mock_client_.selectedMode, "com.apple.inputmethod.Roman");

  // Does not change the display mode for MS Word.  See
  // MozcImkInputController.mm for the detailed information.
  ResetClientBundleIdentifier(@"com.microsoft.Word");
  [controller_ switchMode:commands::HIRAGANA client:mock_client_];
  EXPECT_EQ(controller_.mode, commands::HIRAGANA);
  [controller_ switchDisplayMode];
  // still remains 1 and display mode does not change.
  EXPECT_EQ([mock_client_ getCounter:"selectInputMode:"], 1);
  EXPECT_EQ(mock_client_.selectedMode, "com.apple.inputmethod.Roman");
}

TEST_F(MozcImkInputControllerTest, commitText) {
  controller_.replacementRange = NSMakeRange(0, 1);
  [controller_ commitText:"foo" client:mock_client_];

  EXPECT_EQ([mock_client_ getCounter:"insertText:replacementRange:"], 1);
  EXPECT_TRUE([@"foo" isEqualToString:mock_client_.insertedText]);
  // location has to be cleared after the commit.
  EXPECT_EQ([controller_ replacementRange].location, NSNotFound);
}

TEST_F(MozcImkInputControllerTest, handleConfig) {
  // Does not support multiple-calculation
  config::Config config;
  config.set_preedit_method(config::Config::KANA);
  config.set_yen_sign_character(config::Config::BACKSLASH);
  config.set_use_japanese_layout(true);
  EXPECT_CALL(*mock_mozc_client_, GetConfig(NotNull()))
      .WillOnce(DoAll(SetArgPointee<0>(config), Return(true)));

  [controller_ handleConfig];
  EXPECT_EQ(controller_.keyCodeMap.inputMode, KANA);
  EXPECT_EQ(controller_.yenSignCharacter, config::Config::BACKSLASH);
  EXPECT_EQ([mock_client_ getCounter:"overrideKeyboardWithKeyboardNamed:"], 1);
  EXPECT_TRUE([@"com.apple.keylayout.US" isEqualToString:mock_client_.overriddenLayout])
      << [mock_client_.overriddenLayout UTF8String];
}

TEST_F(MozcImkInputControllerTest, DoubleTapKanaReconvert) {
  // tap (short) tap -> emit undo command
  controller_.mode = commands::HIRAGANA;

  // set attributedString for Reconvert
  [mock_client_ setAttributedString:[[NSAttributedString alloc] initWithString:@"abcde"]];
  mock_client_.expectedRange = NSMakeRange(1, 3);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
  // Check if it did not send keys or commands.
  Mock::VerifyAndClearExpectations(mock_mozc_client_);

  // Sleep less than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) / 2);

  // Send Kana-key.
  commands::SessionCommand actual_command;
  EXPECT_CALL(*mock_mozc_client_, SendCommandWithContext(_, _, NotNull()))
      .WillOnce(DoAll(SaveArg<0>(&actual_command), Return(true)));
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
  // Check if it sent an undo command.
  EXPECT_THAT(actual_command, Type(commands::SessionCommand::CONVERT_REVERSE));
  EXPECT_THAT(actual_command, Text("bcd"));
}

TEST_F(MozcImkInputControllerTest, DoubleTapKanaUndo) {
  // tap (short) tap -> emit undo command
  controller_.mode = commands::HIRAGANA;

  // set expectedRange for Undo
  mock_client_.expectedRange = NSMakeRange(1, 0);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
  // Check if it did not send keys or commands.
  Mock::VerifyAndClearExpectations(mock_mozc_client_);

  // Sleep less than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) / 2);

  // Send Kana-key.
  EXPECT_CALL(*mock_mozc_client_,
              SendCommandWithContext(Type(commands::SessionCommand::UNDO), _, NotNull()))
      .WillOnce(Return(true));
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
}

TEST_F(MozcImkInputControllerTest, DoubleTapKanaUndoTimeOver) {
  // tap (long) tap -> don't emit undo command
  controller_.mode = commands::HIRAGANA;

  // set expectedRange for Undo
  mock_client_.expectedRange = NSMakeRange(1, 0);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);

  // Sleep more than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) * 2);

  // Send Kana-key.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
}

TEST_F(MozcImkInputControllerTest, SingleAndDoubleTapKanaUndo) {
  // tap (long) tap (short) tap -> emit once
  controller_.mode = commands::HIRAGANA;

  // set expectedRange for Undo
  mock_client_.expectedRange = NSMakeRange(1, 0);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);

  // Sleep more than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) * 2);

  // Send Kana-key.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
  // Check if it did not send keys or commands.
  Mock::VerifyAndClearExpectations(mock_mozc_client_);

  // Sleep less than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) / 2);

  // Send Kana-key.
  EXPECT_CALL(*mock_mozc_client_,
              SendCommandWithContext(Type(commands::SessionCommand::UNDO), _, NotNull()))
      .WillOnce(Return(true));
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
}

TEST_F(MozcImkInputControllerTest, TripleTapKanaUndo) {
  // tap (short) tap (short) tap -> emit twice
  controller_.mode = commands::HIRAGANA;

  // set expectedRange for Undo
  mock_client_.expectedRange = NSMakeRange(1, 0);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
  // Check if it did not send keys or commands.
  Mock::VerifyAndClearExpectations(mock_mozc_client_);

  // Sleep less than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) / 2);

  // Send Kana-key.
  EXPECT_CALL(*mock_mozc_client_,
              SendCommandWithContext(Type(commands::SessionCommand::UNDO), _, NotNull()))
      .Times(2)
      .WillRepeatedly(Return(true));
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);

  // Sleep less than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) / 2);

  // Send Kana-key.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
}

TEST_F(MozcImkInputControllerTest, QuadrupleTapKanaUndo) {
  // tap (short) tap (short) tap (short) tap -> emit thrice
  controller_.mode = commands::HIRAGANA;

  // set expectedRange for Undo
  mock_client_.expectedRange = NSMakeRange(1, 0);

  // Send Kana-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
  Mock::VerifyAndClearExpectations(mock_mozc_client_);

  // Sleep less than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) / 2);

  // Send Kana-key.
  EXPECT_CALL(*mock_mozc_client_,
              SendCommandWithContext(Type(commands::SessionCommand::UNDO), _, NotNull()))
      .Times(3)
      .WillRepeatedly(Return(true));
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);

  // Sleep less than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) / 2);

  // Send Kana-key.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);

  // Sleep less than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) / 2);

  // Send Kana-key.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Kana, controller_, mock_client_), YES);
}

TEST_F(MozcImkInputControllerTest, DoubleTapEisuCommitRawText) {
  // Send Eisu-key.
  // Because of special hack for Eisu/Kana keys, it returns YES.
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Eisu, controller_, mock_client_), YES);
  Mock::VerifyAndClearExpectations(mock_mozc_client_);

  // Sleep less than DoubleTapInterval (sec)
  absl::SleepFor(absl::Seconds(GetDoubleTapInterval()) / 2);

  // Send Eisu-key.
  EXPECT_CALL(*mock_mozc_client_,
              SendCommandWithContext(Type(commands::SessionCommand::COMMIT_RAW_TEXT), _, NotNull()))
      .WillOnce(Return(true));
  EXPECT_EQ(SendKeyEvent(kVK_JIS_Eisu, controller_, mock_client_), YES);
}

TEST_F(MozcImkInputControllerTest, fillSurroundingContext) {
  [mock_client_ setAttributedString:[[NSAttributedString alloc] initWithString:@"abcde"]];
  mock_client_.expectedRange = NSMakeRange(2, 1);
  commands::Context context;
  [controller_ fillSurroundingContext:&context client:(id)mock_client_];
  EXPECT_EQ(context.preceding_text(), "ab");
  EXPECT_EQ([mock_client_ getCounter:"selectedRange"], 1);
  EXPECT_EQ([mock_client_ getCounter:"markedRange"], 0);

  [mock_client_ setAttributedString:[[NSAttributedString alloc]
                                        initWithString:@"012345678901234567890abcde"]];

  mock_client_.expectedRange = NSMakeRange(1, 0);
  [controller_ fillSurroundingContext:&context client:(id)mock_client_];
  EXPECT_EQ(context.preceding_text(), "0");
  EXPECT_EQ([mock_client_ getCounter:"selectedRange"], 2);
  EXPECT_EQ([mock_client_ getCounter:"markedRange"], 0);

  mock_client_.expectedRange = NSMakeRange(22, 1);
  [controller_ fillSurroundingContext:&context client:(id)mock_client_];
  EXPECT_EQ(context.preceding_text(), "2345678901234567890a");
  EXPECT_EQ([mock_client_ getCounter:"selectedRange"], 3);
  EXPECT_EQ([mock_client_ getCounter:"markedRange"], 0);

  [mock_client_ setAttributedString:[[NSAttributedString alloc] initWithString:@"012abc345"]];
  commands::Preedit preedit;
  commands::Preedit::Segment *new_segment = preedit.add_segment();
  new_segment->set_annotation(commands::Preedit::Segment::HIGHLIGHT);
  new_segment->set_value("abc");
  new_segment->set_value_length(3);
  [controller_ updateComposedString:&preedit];
  mock_client_.expectedRange = NSMakeRange(3, 3);
  [controller_ fillSurroundingContext:&context client:(id)mock_client_];
  EXPECT_EQ(context.preceding_text(), "012");
  EXPECT_EQ([mock_client_ getCounter:"selectedRange"], 4);
  EXPECT_EQ([mock_client_ getCounter:"markedRange"], 0);
}

}  // namespace
}  // namespace mozc
