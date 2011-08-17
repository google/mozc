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

@class NSURL;

#include "session/commands.pb.h"

@interface GoogleJapaneseInputController ()
// Updates |composedString_| from the result of a key event and put
// the updated composed string to the client application.
- (void)updateComposedString:(const mozc::commands::Preedit *)preedit;

// Updates |candidates_| from the result of a key event.
- (void)updateCandidates:(const mozc::commands::Output *)output;

// Clear all candidate data in |candidates_|.
- (void)clearCandidates;

// Open link specified by the URL.
- (void)openLink:(NSURL *)url;


// Auxiliary methods for switchMode: below.
- (void)switchModeToDirect:(id)sender;
- (void)switchModeInternal:(mozc::commands::CompositionMode)new_mode;

// Switches to a new mode and sync the current mode with the converter.
- (void)switchMode:(mozc::commands::CompositionMode)new_mode
            client:(id)sender;

// Switch the mode icon in the task bar according to |mode_|.
- (void)switchDisplayMode;

// Commit the specified text to the current client.
- (void)commitText:(const char *)text client:(id)sender;

// Conduct the reconvert event.  It could have several tricks such
// like invoking UNDO instead if nothing is selected.  |sender| has to
// be the proxy object to the client application, which might not be
// same as the sender of the click event itself when the user clicks
// the menu item.
- (void)invokeReconvert:(const mozc::commands::SessionCommand *)command
                 client:(id)sender;

// Conduct the undo command.
- (void)invokeUndo:(id)sender;

// Process output fields such as preedit, output text, candidates, and
// modes and calls methods above.
- (void)processOutput:(const mozc::commands::Output *)output client:(id)sender;

// Obtain the current configuration from the server and update
// client-specific configurations.
- (void)handleConfig;

// Set up the client capability
- (void)setupCapability;
// Set up the cilent bundle for the sender.
- (void)setupClientBundle:(id)sender;

// Launch the word register tool with the current selection range.
- (void)launchWordRegisterTool:(id)client;

// They are externally accessible to achieve tests.
@property(readwrite, assign, nonatomic)
    mozc::client::ClientInterface *mozcClient;
@property(readwrite, retain, nonatomic) KeyCodeMap *keyCodeMap;
@property(readwrite, assign, nonatomic)
    mozc::renderer::RendererInterface *renderer;
@property(readonly) mozc::config::Config::YenSignCharacter  yenSignCharacter;
@property(readwrite, assign) mozc::commands::CompositionMode mode;
@property(readonly) mozc::commands::RendererCommand *rendererCommand;
@property(readwrite, assign) NSRange replacementRange;
@end
