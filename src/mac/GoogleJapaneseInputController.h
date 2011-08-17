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

#import <InputMethodKit/InputMethodKit.h>
#import "mac/common.h"

#include "session/commands.pb.h"  // for CompositionMode

@class NSMenu;
@class KeyCodeMap;

namespace mozc {
namespace client {
class ClientInterface;
}  // namespace mozc::client

namespace commands {
class Output;
class RendererCommand;
}

namespace renderer {
class RendererInterface;
}  // namespace mozc::renderer
}  // namespace mozc

// GoogleJapaneseInputController is a subclass of
// |IMKInputController|, which holds a connection from a client
// application to the mozc server (Japanese IME server) on the
// machine.
//
// For the detail of IMKInputController itself, see the ADC document
// http://developer.apple.com/documentation/Cocoa/Reference/IMKInputController_Class/
@interface GoogleJapaneseInputController :
    IMKInputController<ControllerCallback> {
 @private
  // Instance variables are all strong references.

  // |composedString_| stores the current preedit text
  NSMutableAttributedString *composedString_;

  // |originalString_| stores original key strokes.
  NSMutableString *originalString_;

  // |cursorPosition_| is the position of cursor in the preedit.  If
  // no cursor is found, its value should be NSNotFound.
  int cursorPosition_;

  // |mode_| stores the current input mode (Direct or conversion).
  mozc::commands::CompositionMode mode_;

  // |yenSignCharacter_| holds the character for the YEN_SIGN key in
  // JIS keyboard.  This config is separated from |keyCodeMap_|
  // because it is for DIRECT mode.
  mozc::config::Config::YenSignCharacter yenSignCharacter_;

  // Check the kana/ascii input mode at the key event if true.
  // Because it requires GetConfig which asks Converter server, we
  // want to delay the checking to the key event timing but we don't
  // want to call this every key events.
  BOOL checkInputMode_;

  // |keyCodeMap_| manages the mapping between Mac key code and mozc
  // key events.
  KeyCodeMap *keyCodeMap_;

  // |clientBundle_| is the Bundle ID of the client application which
  // the controller communicates with.
  string *clientBundle_;

  NSRange replacementRange_;

  // |lastKanaKeyTime_| is set to the time when Kana-key is tapped,
  // and is set to 0 when other key is tapped.
  NSTimeInterval lastKanaKeyTime_;

  // |candidateController_| controls the candidate windows.
  mozc::renderer::RendererInterface *candidateController_;

  // |rendererCommand_| stores the command sent to |candidateController_|
  mozc::commands::RendererCommand *rendererCommand_;

  // |mozcClient_| manages connection to the mozc server.
  mozc::client::ClientInterface *mozcClient_;

  // |imkServer_| holds the reference to GoogleJapaneseInputServer.
  id<ServerCallback> imkServer_;

  // |menu_| is the NSMenu to be shown in the pulldown menu-list of
  // the IME.
  IBOutlet NSMenu *menu_;
}

// candidateClicked: is called when the user clicks a candidate item
// in candidate windows.
- (void)candidateClicked:(int)id;

// reconversionClicked: is called when the user clicks "Reconversion"
// menu item.
- (IBAction)reconversionClicked:(id)sender;

// configClicked: is called when the user clicks "Configure Mozc..."
// menu item.
- (IBAction)configClicked:(id)sender;

// dictionaryToolClicked: is called when the user clicks "Dictionary
// Tool..."  menu item.
- (IBAction)dictionaryToolClicked:(id)sender;

// registerWordClicked: is called when the user clicks "Add a word..."
// menu item.
- (IBAction)registerWordClicked:(id)sender;

// characterPadClicked: is called when the user clicks "Character Pad..."
// menu item.
- (IBAction)characterPadClicked:(id)sender;

// aboutDialogClicked: is called when the user clicks "About Mozc..."
// menu item.
- (IBAction)aboutDialogClicked:(id)sender;

// outputResult: put result text in the specified |output| into the
// client application.
- (void)outputResult:(mozc::commands::Output *)output;

// create instances for global objects which will be referred from the
// controller instances.
+ (void)initializeConstants;
@end
