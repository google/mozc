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

#import <Cocoa/Cocoa.h>
#import <InputMethodKit/InputMethodKit.h>

#import "mac/KeyCodeMap.h"
#import "mac/common.h"
#import "mac/renderer_receiver.h"

#include <cstdint>
#include <memory>
#include <string>

// For mozc::commands::CompositionMode
#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/renderer_interface.h"

/** MozcImkInputController is a subclass of |IMKInputController|, which holds a connection
 * from a client application to the mozc server (Japanese IME server) on the machine.
 *
 * For the detail of IMKInputController itself, see the ADC document
 * http://developer.apple.com/documentation/Cocoa/Reference/IMKInputController_Class/
 */
@interface MozcImkInputController : IMKInputController <ControllerCallback> {
 @private
  /** Instance variables are all strong references. */

  /** |composedString_| stores the current preedit text */
  NSMutableAttributedString *composedString_;

  /** |originalString_| stores original key strokes. */
  NSMutableString *originalString_;

  /** |cursorPosition_| is the position of cursor in the preedit.  If no cursor is found, its value
   * should be -1. */
  int cursorPosition_;

  /** |mode_| stores the current input mode (Direct or conversion). */
  mozc::commands::CompositionMode mode_;

  /** |yenSignCharacter_| holds the character for the YEN_SIGN key in JIS keyboard.  This config is
   * separated from |keyCodeMap_| because it is for DIRECT mode. */
  mozc::config::Config::YenSignCharacter yenSignCharacter_;

  /** |suppressSuggestion_| indicates whether to suppress the suggestion. */
  bool suppressSuggestion_;

  /** |keyCodeMap_| manages the mapping between Mac key code and mozc key events. */
  KeyCodeMap *keyCodeMap_;

  /** |clientBundle_| is the Bundle ID of the client application which the controller communicates
   * with. */
  std::string clientBundle_;

  NSRange replacementRange_;

  /** |lastKeyDownTime_| and |lastKeyCode_| are used to handle double tapping. */
  NSTimeInterval lastKeyDownTime_;
  uint16_t lastKeyCode_;

  /** |mozcRenderer_| controls the candidate windows. */
  std::unique_ptr<mozc::renderer::RendererInterface> mozcRenderer_;

  /** |rendererCommand_| stores the command sent to |mozcRenderer_| */
  mozc::commands::RendererCommand rendererCommand_;

  /** |mozcClient_| manages connection to the mozc server. */
  std::unique_ptr<mozc::client::ClientInterface> mozcClient_;

  /** |imkClientForTest_| holds the reference to the client object for unit test. */
  id imkClientForTest_;

  /** |menu_| is the NSMenu to be shown in the pulldown menu-list of the IME. */
  IBOutlet NSMenu *menu_;
}

/** These are externally accessible to achieve tests. */
@property(readonly) mozc::client::ClientInterface *mozcClient;
@property(readwrite, retain, nonatomic) KeyCodeMap *keyCodeMap;
@property(readonly) mozc::renderer::RendererInterface *renderer;
@property(readonly) mozc::config::Config::YenSignCharacter yenSignCharacter;
@property(readwrite, assign) mozc::commands::CompositionMode mode;
@property(readonly) const mozc::commands::RendererCommand &rendererCommand;
@property(readwrite, assign) NSRange replacementRange;
@property(readwrite, retain) id imkClientForTest;

/** Sets the RendererReceiver used by all instances of the controller.
 * the RendererReceiver is a singleton object used as a proxy to receive messages from
 * the renderer process and propage it to the active controller instance.
 *
 * @param rendererReceiver The RendererReceiver object referred by all instances of the controller.
 */
+ (void)setGlobalRendererReceiver:(RendererReceiver *)rendererReceiver;

/** reconversionClicked: is called when the user clicks "Reconversion" menu item.
 *
 * @param sender The sender of this request (unused).
 */
- (IBAction)reconversionClicked:(id)sender;

/** configClicked: is called when the user clicks "Configure Mozc..." menu item.
 *
 * @param sender The sender of this request (unused).
 */
- (IBAction)configClicked:(id)sender;

/** dictionaryToolClicked: is called when the user clicks "Dictionary Tool..."  menu item.
 *
 * @param sender The sender of this request (unused).
 */
- (IBAction)dictionaryToolClicked:(id)sender;

/** registerWordClicked: is called when the user clicks "Add a word..." menu item.
 *
 * @param sender The sender of this request (unused).
 */
- (IBAction)registerWordClicked:(id)sender;

/** aboutDialogClicked: is called when the user clicks "About Mozc..." menu item.
 *
 * @param sender The sender of this request (unused).
 */
- (IBAction)aboutDialogClicked:(id)sender;

/** Sets the ClientInterface to use in the controller.
 *
 * @param newMozcClient The client object to communicate with the Mozc server process.
 */
- (void)setMozcClient:(std::unique_ptr<mozc::client::ClientInterface>)newMozcClient;

/** Sets the RendererInterface to use in the controller.
 *
 * @param newRenderer The client object to communicate with the candidate renderer process.
 */
- (void)setRenderer:(std::unique_ptr<mozc::renderer::RendererInterface>)newRenderer;

/** Updates |composedString_| from the result of a key event and puts the updated composed string to
 * the client application.
 *
 * @param preedit The protobuf data representing the composed string.
 */
- (void)updateComposedString:(const mozc::commands::Preedit *)preedit;

/** Updates |candidates_| from the result of a key event.
 *
 * @param output The protobuf data that contains the data of candidate words.
 */
- (void)updateCandidates:(const mozc::commands::Output *)output;

/** Clears all candidate data in |candidates_|. */
- (void)clearCandidates;

/** Opens a link specified by the URL.
 *
 * @param url The URL information.
 */
- (void)openLink:(NSURL *)url;

/** Switches to a new mode and sync the current mode with the converter.
 *
 * @param new_mode The protobuf enum of the new input mode.
 * @param sender The sender of this request.
 */
- (void)switchMode:(mozc::commands::CompositionMode)new_mode client:(id)sender;

/** Switches the mode icon in the task bar according to |mode_|. */
- (void)switchDisplayMode;

/** Commits the specified text to the current client.
 *
 * @param text The text to be committed.
 * @param sender The sender of this request.
 */
- (void)commitText:(const char *)text client:(id)sender;

/** Conducts the reconvert event.  It could have several tricks such like invoking UNDO instead if
 * nothing is selected.  |sender| has to be the proxy object to the client application, which might
 * not be same as the sender of the click event itself when the user clicks the menu item.
 *
 * @param command The protobuf data to send to the Mozc server process.
 * @param sender The sender of this request.
 */
- (void)invokeReconvert:(const mozc::commands::SessionCommand *)command client:(id)sender;

/** Conducts the undo command.
 *
 * @param sender The sender of this request.
 */
- (void)invokeUndo:(id)sender;

/** Processes output fields such as preedit, output text, candidates, and modes and calls methods
 * above.
 *
 * @param output The protobuf of the output data.
 * @param sender The sender of this request.
 */
- (void)processOutput:(const mozc::commands::Output *)output client:(id)sender;

/** Obtains the current configuration from the server and update client-specific configurations. */
- (void)handleConfig;

/** Sets up the client capability */
- (void)setupCapability;

/** Sets up the client bundle for the sender.
 *
 * @param sender The sender of this request.
 */
- (void)setupClientBundle:(id)sender;

/** Launches the word register tool with the current selection range.
 *
 * @param client The host application. The selection data in this client is used as an input.
 */
- (void)launchWordRegisterTool:(id)client;

/** Fills the surrounding context (preceding_text and following_text). Returns false if fails to get
 * the surrounding context from the client.
 *
 * @param context The protobuf data used to be fill with the context information.
 * @param client The host application as the source of surrounding context.
 * @return YES if context was filled; NO otherwise.
 */
- (BOOL)fillSurroundingContext:(mozc::commands::Context *)context client:(id<IMKTextInput>)client;

@end

/** GoogleJapaneseInputController is an alias of MozcImkInputController for backward compatibility.
 * This will be removed in the future when all clients are migrated to the new name and performed
 * relogin at least once.
 */
@interface GoogleJapaneseInputController : MozcImkInputController {
}
@end
