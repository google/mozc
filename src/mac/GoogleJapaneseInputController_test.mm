// Copyright 2010-2011, Google Inc.
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

#include "base/mac_util.h"
#include "client/session_interface.h"
#include "renderer/renderer_interface.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

@interface MockIMKServer : IMKServer <ServerCallback> {
  // The controller which accepts user's clicks
  id<ControllerCallback> expectedController_;
  int setCurrentController_count_;
}
@property(readwrite, assign) id<ControllerCallback> expectedController;
@end

@implementation MockIMKServer
@synthesize expectedController = expectedController_;

- (MockIMKServer*)init {
  self = [super init];
  expectedController_ = nil;
  setCurrentController_count_ = 0;
  return self;
}

- (void)rendererClicked:(NSData *)data {
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
  NSString *bundleIdentifier;
  NSRect expectedCursor;
  NSRange expectedRange;
  string *selectedMode_;
  NSString *insertedText_;
  NSString *overriddenLayout_;
  map<string, int> *counters_;
}
@property(readwrite, assign) NSString *bundleIdentifier;
@property(readwrite, assign) NSRect expectedCursor;
@property(readwrite, assign) NSRange expectedRange;
@property(readonly) string *selectedMode;
@property(readonly) NSString *insertedText;
@property(readonly) NSString *overriddenLayout;
@end

@implementation MockClient
@synthesize bundleIdentifier;
@synthesize expectedCursor;
@synthesize expectedRange;
@synthesize selectedMode = selectedMode_;
@synthesize insertedText = insertedText_;
@synthesize overriddenLayout = overriddenLayout_;

- (MockClient *)init {
  self = [super init];
  self.bundleIdentifier = @"com.google.exampleBundle";
  expectedRange = NSMakeRange(NSNotFound, NSNotFound);
  counters_ = new map<string, int>;
  selectedMode_ = new string;
  return self;
}

- (void)dealloc {
  delete counters_;
  delete selectedMode_;
  [bundleIdentifier release];
  [insertedText_ release];
  [overriddenLayout_ release];
  [super dealloc];
}

- (int)getCounter:(const char *)selector {
  return (*counters_)[selector];
}

- (NSConnection *)connectionForProxy {
  return nil;
}

- (NSDictionary *)attributesForCharacterIndex:(int)index
                          lineHeightRectangle:(NSRect *)rect {
  (*counters_)["attributesForCharacterIndex:lineHeightRectangle:"]++;
  *rect = expectedCursor;
  return nil;
}

- (NSRange)selectedRange {
  (*counters_)["selectedRange"]++;
  return expectedRange;
}

- (void)selectInputMode:(NSString *)mode {
  (*counters_)["selectInputMode:"]++;
  selectedMode_->assign([mode UTF8String]);
}

// a dummy method which is necessary internally
- (void)setMarkedText:(id)string
       selectionRange:(NSRange)selectionRange
     replacementRange:(NSRange)replacementRange {
  // Do nothing
}

- (void)insertText:(NSString *)result replacementRange:(NSRange)range {
  (*counters_)["insertText:replacementRange:"]++;
  [insertedText_ release];
  insertedText_ = [result retain];
}

- (void)overrideKeyboardWithKeyboardNamed:(NSString *)newLayout {
  (*counters_)["overrideKeyboardWithKeyboardNamed:"]++;
  [overriddenLayout_ release];
  overriddenLayout_ = [newLayout retain];
}
@end

@interface MockScreen : NSScreen
+ (NSRect)mockScreen;
@end

@implementation MockScreen
// a dummy frame of 1024x768.  This method is used to share size of
// screen among the implementation and test case.
+ (NSRect)mockScreen {
  return NSMakeRect(0, 0, 1024, 768);
}

- (NSRect)frame {
  return [MockScreen mockScreen];
}
@end

namespace {
int openURL_count = 0;
BOOL openURL_test(id self, SEL selector, NSURL *url) {
  openURL_count++;
  return true;
}

NSArray *dummy_screens(id self, SEL selector) {
  return [NSArray arrayWithObject:[[[MockScreen alloc] init] autorelease]];
}
}  // anonymous namespace


class MockSession : public mozc::client::SessionInterface {
 public:
  MockSession() {}
#define MockMethod(return_type, method_name, arg, body) \
  virtual return_type method_name(arg) { \
    call_counters_[#method_name]++; body; \
  } \
  int counter_##method_name() const { return call_counters_[#method_name]; }
#define BoolMethod(method_name, arg) \
  MockMethod(bool, method_name, arg, return true)
#define VoidMethod(method_name, arg) \
  MockMethod(void, method_name, arg, ;)

  bool IsValidRunLevel() const { return true; }
  BoolMethod(EnsureSession, void);
  BoolMethod(EnsureConnection, void);
  BoolMethod(CheckVersionOrRestartServer, void);
  BoolMethod(ClearUserHistory, void);
  BoolMethod(ClearUserPrediction, void);
  BoolMethod(ClearUnusedUserPrediction, void);
  BoolMethod(Shutdown, void);
  BoolMethod(SyncData, void);
  BoolMethod(Reload, void);
  BoolMethod(NoOperation, void);
  VoidMethod(set_timeout, int timeout);
  VoidMethod(set_restricted, bool restricted);
  VoidMethod(set_server_program, const string &server_program);
  VoidMethod(Reset, void);
  VoidMethod(EnableCascadingWindow, bool enable);
  BoolMethod(Cleanup, void);

  MockMethod(bool, GetConfig, mozc::config::Config *config, {
      config->CopyFrom(expected_config_); return true;
    });
  MockMethod(bool, SetConfig, const mozc::config::Config &config, {
      expected_config_.CopyFrom(config); return true;
    });

  MockMethod(void, set_client_capability,
             const mozc::commands::Capability &capability, {
               actual_capability_.CopyFrom(capability);
             });
  const mozc::commands::Capability &actual_capability() {
    return actual_capability_;
  }

#undef VoidMethod
#undef BoolMethod
#undef MockMethod

#define OutputMethod(method_name, arg_type) \
  virtual bool method_name(arg_type arg, mozc::commands::Output *output) {   \
      call_counters_[#method_name]++; \
      called_##method_name##_.CopyFrom(arg); \
      output->CopyFrom(outputs_[#method_name]); \
      return true; \
  } \
  arg_type called_##method_name() const { return called_##method_name##_; } \
  int counter_##method_name() const { return call_counters_[#method_name]; } \
  void set_output_##method_name(const mozc::commands::Output &output) {   \
      outputs_[#method_name].CopyFrom(output); \
  }

  OutputMethod(SendKey, const mozc::commands::KeyEvent &);
  OutputMethod(TestSendKey, const mozc::commands::KeyEvent &);
  OutputMethod(SendCommand, const mozc::commands::SessionCommand &);

  bool LaunchTool(const string &mode, const string &extra_arg) {
    return true;
  }
  bool OpenBrowser(const string &url) {
    return true;
  }

  bool PingServer() const {
    call_counters_["PingServer"]++;
    return true;
  }
  int counter_PingServer() const { return call_counters_["PingServer"]; }

 private:
  mutable map<string, int> call_counters_;
  map<string, mozc::commands::Output> outputs_;
  mozc::config::Config expected_config_;
  mozc::commands::Capability actual_capability_;
  mozc::commands::KeyEvent called_SendKey_;
  mozc::commands::KeyEvent called_TestSendKey_;
  mozc::commands::SessionCommand called_SendCommand_;
};

class MockRenderer : public mozc::renderer::RendererInterface {
 public:
  MockRenderer()
      : Activate_counter_(0),
        IsAvailable_counter_(0),
        ExecCommand_counter_(0) {
  }

  virtual bool Activate() {
    Activate_counter_++;
    return true;
  }
  int counter_Activate() const {
    return Activate_counter_;
  }

  virtual bool ExecCommand(const mozc::commands::RendererCommand &command) {
    ExecCommand_counter_++;
    called_command_.CopyFrom(command);
    return true;
  }
  int counter_ExecCommand() const {
    return ExecCommand_counter_;
  }
  const mozc::commands::RendererCommand &CalledCommand() const {
    return called_command_;
  }

 virtual bool IsAvailable() const {
   IsAvailable_counter_++;
   return true;
 }
 int counter_IsAvailable() const {
   return IsAvailable_counter_;
 }

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
    pool_ = [[NSAutoreleasePool alloc] init];
    mock_server_ = [[MockIMKServer alloc] init];
    mock_client_ = [[MockClient alloc] init];
    // setup workspace
    Method method = class_getInstanceMethod(
        [NSWorkspace class], @selector(openURL:));
    method_setImplementation(method, reinterpret_cast<IMP>(openURL_test));
    openURL_count = 0;

    // setup NSScreen
    method = class_getClassMethod([NSScreen class], @selector(screens));
    method_setImplementation(method, reinterpret_cast<IMP>(dummy_screens));

    [GoogleJapaneseInputController initializeConstants];
    SetUpController();
  }
  void TearDown() {
    [controller_ release];
    [mock_server_ release];
    // MockSession and MockRenderer are released during the release of
    // |controller_|.
    [pool_ release];
  }

  void SetUpController() {
    controller_ = [[GoogleJapaneseInputController alloc]
                    initWithServer:mock_server_
                          delegate:nil
                            client:mock_client_];
    mock_server_.expectedController = controller_;
    mock_session_ = new MockSession;
    controller_.session = mock_session_;
    mock_renderer_ = new MockRenderer;
    controller_.renderer = mock_renderer_;
  }

  void ResetClientBundleIdentifier(NSString *new_bundle_id) {
    [controller_ release];
    mock_client_.bundleIdentifier = new_bundle_id;
    SetUpController();
  }

  MockSession *mock_session_;
  MockClient *mock_client_;
  MockRenderer *mock_renderer_;

  GoogleJapaneseInputController *controller_;

 private:
  MockIMKServer *mock_server_;
  NSAutoreleasePool *pool_;
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
      [NSSet setWithObjects:NSUnderlineStyleAttributeName,
             NSUnderlineColorAttributeName,
             NSMarkedClauseSegmentAttributeName,
             nil];
  while (cursor < [expected length]) {
    NSRange expectedRange = NSMakeRange(NSNotFound, NSNotFound);
    NSRange actualRange = NSMakeRange(NSNotFound, NSNotFound);
    NSDictionary *expectedAttributes =
        [expected attributesAtIndex:cursor effectiveRange:&expectedRange];
    NSDictionary *actualAttributes =
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
    NSNumber *actualUnderlineStyle =
        [expectedAttributes valueForKey:NSUnderlineStyleAttributeName];
    if (![expectedUnderlineStyle isEqualToNumber:actualUnderlineStyle] ||
        actualUnderlineStyle == nil) {
      LOG(ERROR) << "underline style is different at range ("
                 << expectedRange.location << ", "
                 << expectedRange.length << ")";
      return false;
    }
    NSColor *expectedUnderlineColor =
        [expectedAttributes valueForKey:NSUnderlineColorAttributeName];
    NSColor *actualUnderlineColor =
        [actualAttributes valueForKey:NSUnderlineColorAttributeName];
    if (![[NSString stringWithFormat:@"%@", expectedUnderlineColor]
          isEqualToString:[NSString stringWithFormat:@"%@",
                                    actualUnderlineColor]]) {
      LOG(ERROR) << "underline color is different at range ("
                 << expectedRange.location << ", "
                 << expectedRange.length << ")";
      return false;
    }

    // Check if it contains unknown attributes
    for (NSString *key in [expectedAttributes keyEnumerator]) {
      if (![knownAttributeNames containsObject:key]) {
        LOG(ERROR) << "expected value contains an unknown key: "
                   << [key UTF8String];
        return false;
      }
    }
    for (NSString *key in [actualAttributes keyEnumerator]) {
      if (![knownAttributeNames containsObject:key]) {
        LOG(ERROR) << "actual value contains an unknown key: "
                   << [key UTF8String];
        return false;
      }
    }
    cursor += expectedRange.length;
  }

  // true if it passes every check
  return true;
}

TEST_F(GoogleJapaneseInputControllerTest, UpdateComposedString) {
  // If preedit is NULL, it still calls setMarkedText, with an empty string.
  NSMutableAttributedString *expected =
      [[[NSAttributedString alloc] initWithString:@""] autorelease];
  [controller_ updateComposedString:NULL];
  EXPECT_TRUE([expected
                isEqualToAttributedString:[controller_ composedString:nil]]);

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

  NSDictionary *highlightAttributes =
      [controller_ markForStyle:kTSMHiliteSelectedConvertedText
                        atRange:NSMakeRange(NSNotFound, 0)];
  NSDictionary *underlineAttributes =
      [controller_ markForStyle:kTSMHiliteConvertedText
                        atRange:NSMakeRange(NSNotFound, 0)];
  expected =
      [[[NSMutableAttributedString alloc] initWithString:@"abcdef"]
        autorelease];
  [expected addAttributes:underlineAttributes range:NSMakeRange(0, 1)];
  [expected addAttributes:highlightAttributes range:NSMakeRange(1, 2)];
  [expected addAttributes:underlineAttributes range:NSMakeRange(3, 3)];
  [controller_ updateComposedString:&preedit];
  NSAttributedString *actual = [controller_ composedString:nil];
  EXPECT_TRUE(ComparePreedit(expected, actual))
      << [[NSString stringWithFormat:@"expected:%@ actual:%@", expected, actual]
           UTF8String];
}

TEST_F(GoogleJapaneseInputControllerTest, ClearCandidates) {
  [controller_ clearCandidates];
  EXPECT_EQ(1, mock_renderer_->counter_ExecCommand());
  // After clearing candidates, the candidate window has to be invisible.
  EXPECT_FALSE(mock_renderer_->CalledCommand().visible());
}

TEST_F(GoogleJapaneseInputControllerTest, UpdateCandidates) {
  // When output is null, same as ClearCandidate
  [controller_ updateCandidates:NULL];
  EXPECT_EQ(1, mock_renderer_->counter_ExecCommand());
  EXPECT_FALSE(mock_renderer_->CalledCommand().visible());
  EXPECT_EQ(0,
            [mock_client_
              getCounter:"attributesForCharacterIndex:lineHeightRectangle:"]);

  // create an output
  mozc::commands::Output output;
  // a candidate has to have at least a candidate
  mozc::commands::Candidates *candidates = output.mutable_candidates();
  candidates->set_focused_index(0);
  candidates->set_size(1);
  mozc::commands::Candidates::Candidate *candidate =
      candidates->add_candidate();
  candidate->set_index(0);
  candidate->set_value("abc");
  NSRect screen_rect = [MockScreen mockScreen];

  // setup the cursor position
  mock_client_.expectedCursor = NSMakeRect(50, 50, 1, 10);
  [controller_ updateCandidates:&output];
  EXPECT_EQ(1,
            [mock_client_
              getCounter:"attributesForCharacterIndex:lineHeightRectangle:"]);
  const mozc::commands::RendererCommand *rendererCommand =
      controller_.rendererCommand;
  EXPECT_EQ(2, mock_renderer_->counter_ExecCommand());
  EXPECT_TRUE(rendererCommand->visible());
  const mozc::commands::RendererCommand::Rectangle &preedit_rectangle =
      rendererCommand->preedit_rectangle();
  EXPECT_EQ(50, preedit_rectangle.left());
  EXPECT_EQ(screen_rect.origin.y + screen_rect.size.height - 60,
            preedit_rectangle.top());
  EXPECT_EQ(51, preedit_rectangle.right());
  EXPECT_EQ(screen_rect.origin.y + screen_rect.size.height - 50,
            preedit_rectangle.bottom());

  // reshow the candidate window again -- but cursor position has changed.
  mock_client_.expectedCursor = NSMakeRect(60, 50, 1, 10);
  [controller_ updateCandidates:&output];
  // Does not change: not call again
  EXPECT_EQ(1,
            [mock_client_
              getCounter:"attributesForCharacterIndex:lineHeightRectangle:"]);
  rendererCommand = controller_.rendererCommand;
  EXPECT_EQ(3, mock_renderer_->counter_ExecCommand());
  EXPECT_TRUE(rendererCommand->visible());
  // Does not change the position
  EXPECT_EQ(preedit_rectangle.DebugString(),
            rendererCommand->preedit_rectangle().DebugString());

  // Then update without candidate entries -- goes invisible.
  output.Clear();
  [controller_ updateCandidates:&output];
  // Does not change: not call again
  EXPECT_EQ(1,
            [mock_client_
              getCounter:"attributesForCharacterIndex:lineHeightRectangle:"]);
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
  mock_session_->set_output_SendKey(output);

  [controller_ switchModeToDirect:mock_client_];
  EXPECT_EQ(mozc::commands::DIRECT, controller_.mode);
  EXPECT_EQ(1, mock_session_->counter_SendKey());
  EXPECT_TRUE(mock_session_->called_SendKey().has_special_key());
  EXPECT_EQ(mozc::commands::KeyEvent::OFF,
            mock_session_->called_SendKey().special_key());
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
  EXPECT_EQ(1, mock_session_->counter_SendKey());
  EXPECT_TRUE(mock_session_->called_SendKey().has_special_key());
  EXPECT_EQ(mozc::commands::KeyEvent::ON,
            mock_session_->called_SendKey().special_key());
  EXPECT_EQ(1, mock_session_->counter_SendCommand());
  EXPECT_EQ(mozc::commands::SessionCommand::SWITCH_INPUT_MODE,
            mock_session_->called_SendCommand().type());
  EXPECT_EQ(mozc::commands::HIRAGANA,
            mock_session_->called_SendCommand().composition_mode());

  // Switch from HIRAGANA to KATAKANA.  Just sending mode switching command.
  controller_.mode = mozc::commands::HIRAGANA;
  [controller_ switchModeInternal:mozc::commands::HALF_KATAKANA];
  EXPECT_EQ(mozc::commands::HALF_KATAKANA, controller_.mode);
  EXPECT_EQ(1, mock_session_->counter_SendKey());
  EXPECT_EQ(2, mock_session_->counter_SendCommand());
  EXPECT_EQ(mozc::commands::SessionCommand::SWITCH_INPUT_MODE,
            mock_session_->called_SendCommand().type());
  EXPECT_EQ(mozc::commands::HALF_KATAKANA,
            mock_session_->called_SendCommand().composition_mode());

  // going to same mode does not cause sendcommand
  [controller_ switchModeInternal:mozc::commands::HALF_KATAKANA];
  EXPECT_EQ(mozc::commands::HALF_KATAKANA, controller_.mode);
  EXPECT_EQ(1, mock_session_->counter_SendKey());
  EXPECT_EQ(2, mock_session_->counter_SendCommand());
}

TEST_F(GoogleJapaneseInputControllerTest, SwitchDisplayMode) {
  EXPECT_TRUE(mock_client_.selectedMode->empty());
  EXPECT_EQ(mozc::commands::DIRECT, controller_.mode);
  [controller_ switchDisplayMode];
  EXPECT_EQ(1, [mock_client_ getCounter:"selectInputMode:"]);
  string expected = mozc::MacUtil::GetLabelForSuffix("Roman");
  EXPECT_EQ(expected, *(mock_client_.selectedMode));

  // Does not change the display mode for MS Word.  See
  // GoogleJapaneseInputController.mm for the detailed information.
  ResetClientBundleIdentifier(@"com.microsoft.Word");
  [controller_ switchMode:mozc::commands::HIRAGANA client:mock_client_];
  EXPECT_EQ(mozc::commands::HIRAGANA, controller_.mode);
  [controller_ switchDisplayMode];
  // still remains 1 and display mode does not change.
  EXPECT_EQ(1, [mock_client_ getCounter:"selectInputMode:"]);
  expected = mozc::MacUtil::GetLabelForSuffix("Roman");
  EXPECT_EQ(expected, *(mock_client_.selectedMode));
}

TEST_F(GoogleJapaneseInputControllerTest, commitText) {
  controller_.replacementRange = NSMakeRange(0, 1);
  [controller_ commitText:"foo" client:mock_client_];

  EXPECT_EQ(1, [mock_client_ getCounter:"insertText:replacementRange:"]);
  EXPECT_TRUE([@"foo" isEqualToString:mock_client_.insertedText]);
  // location has to be cleared after the commit.
  // Do not use NSNotFound directly in EXPECT_EQ because type checker
  // gets confused for the comparision of enums.
  int expected = NSNotFound;
  EXPECT_EQ(expected, [controller_ replacementRange].location);
}

TEST_F(GoogleJapaneseInputControllerTest, handleConfig) {
  // Does not support multiple-calculation
  mozc::config::Config config;
  config.set_preedit_method(mozc::config::Config::KANA);
  config.set_yen_sign_character(mozc::config::Config::BACKSLASH);
  config.set_use_japanese_layout(true);
  mock_session_->SetConfig(config);

  [controller_ handleConfig];
  EXPECT_EQ(1, mock_session_->counter_GetConfig());
  EXPECT_EQ(KANA, controller_.keyCodeMap.inputMode);
  EXPECT_EQ(mozc::config::Config::BACKSLASH, controller_.yenSignCharacter);
  EXPECT_EQ(1, [mock_client_ getCounter:"overrideKeyboardWithKeyboardNamed:"]);
  EXPECT_TRUE([@"com.apple.keylayout.US"
                isEqualToString:mock_client_.overriddenLayout])
      << [mock_client_.overriddenLayout UTF8String];
}
