# Mac internal

[TOC]

## IME Application

### Application overview

In macOS, each IME is an application which uses IMKit (Input Method
kit) framework. Any macOS systems have IMKit.framework under
/System/Library/Frameworks.
Each IME application should be singleton for each user. Application declares
some meta data about IME in Info.plist file inside of the application package.
OS scrapes the Info.plist files under some specific folders (like
"/Library/Input Method"), and then treat them as IMEs. In their main file, an
IMKServer is created and then run in the default loop. IMKServer waits for the
client applications with a Mach IPC.
When a client application (like TextEdit) is invoked,
the following things happen:

1.  Create a widget with TextServiceManager (TSM)
1.  The client application tries to connect the IMKServer of the current
    preference. If no process is waiting for the connection, it invokes the
    singleton IME application
    1.  And then, IME application creates IMKServer and then accepts the
        connection from the client application
1.  When connection is established, IMKServer creates an instance of
    IMKInputController subclass (you can specify the name of the class in
    Info.plist)
1.  IMKServer invokes the initialization method
    (initWithServer:client:delegate:) and activation method (activateServer:)
1.  When the user types a key, a key event is sent to the IMKInputController via
    Mach IPC connection

When the client application loses the focus, the application sends
deactivateServer: method to the IMKInputController. When the user turned off the
IME (switch the IME to other IME), the application sends deactivateServer: and
then release the IMKInputController instance. However, IMKServer instance and
the process itself survives. The process does not end until the machine shuts
down.
Although any key events / IME behaviors are achieved by Mach IPC, Mach IPC
layers are hidden from IMKit framework. All you need to do is implement
requisite methods of IMKInputController. When some event happens, appropriate
method is called. Any public methods of IMKInputController accepts sender (or
sometimes called "client") as an argument, which is a proxy object of the client
application. By calling method to this sender, IMKInputController puts like
preedit.

### Mode switching

An IME has multiple mode like hiragana/katakana/direct/.... Each mode are
described in the Info.plist. Unfortunately, however, the mode switch events are
not notified to the IMKInputController. However, the Controller can tell the
current mode ID. In Mozc, MozcController checks the current mode ID in every key
event and monitors the mode switch.
In addiiton, pressing KANA key is also a key event of space bar.
Mozc distinguish this key event from the normal space bar by physical key code
(see below).

## Key event handling

### Basic key event

IMKInputController has three way to accept key events, but the best way (most
precise way) is handleEvent:client: method. It accepts two arguments: the key
event itself (NSEvent) and the sender as I mentioned above. Key event has the
following information:

*   physical key code (with keyCode method)
*   a string of the key event (with characters method)
*   a string of the key event without any modifiers (with
    charactersIgnoringModifiers method). Ex. if the user types Ctrl-a,
    "characters" string is "\\001", but "charactersIgnoringModifiers" string is
    "a".
*   Modifier bits like ctrl, shift, command, or alt (with modifiers method)

Mac Mozc has a class, KeyCodeMap, to manage the mapping between those Mac key
events and Mozc key events. Basically we only see string, string without
modifiers, and modifiers. But physical key code is also used for some special
keys.

### Modifier key event

Some IME accepts a key event like "pressing shift". This behavior is achieved by
other functionality. By default, IMKInputController cannot accept modifier only
key events. In order to accept them, you must implement recognizedEvents:. This
method returns a masks. If the mask contains NSFlagsChangedMask, any modifier
change event is notified and converted to the key event when all modifiers are
released.

### Kana key event

Kana typing is another tricky functionalities. The proxy object can accept
overrideKeyboardWithKeyboardNamed: method to specify the keyboard layout of the
input session. "com.apple.keylayout.KANA" is the layout of kana input. This
overriding is cleared at the timing of loosing focus (deactivateServer:). So you
do not need to override the previous layout at some timing, but you need to call
this method every time when activated.
In kana layout, string and string without modifiers are already hiragana
characters. We cannot say which key is actually typed. For example,
MozcController gets "ぉ" if the user presses Shift-6. If the user pressed F9
then, what string should be returned? The answer is: "&" for JIS keyboard but
"^" for US layout. But IME can't tell which keyboard is used for each key event.
We know the physical key code, so Mozc has two mappings (US and JIS) between
physical key code and ascii.

## Methods to sender

When MozcController sends the key event to the mozc server and gets the result,
it must render the result to the user (via "sender" proxy object). The following
methods are used.

*   The return value type of handleEvent:client: method is BOOL. If it returns
    YES, the key event is consumed. Otherwise the key event is sent to the
    client application.
*   insertText:replacementRange: is the method to put the result text (fixed
    text) to the client. replacementRange: is NotFound (NSMakeRange(NSNotFound,
    0)) if you want to put the text into the cursor position.
*   updateComposition of IMKInputController class. This callsed self
    composedString: method then, and put the composedString as the preedit
*   selectionRange: of IMKInputController class. This is also called in
    updateComposition method. In despite of the name of the method, it specifies
    the cursor position. It returns a range but the length of the range should
    be 0 (otherwise the cursor position doesn't change).

composedString: method returns NSAttributedString (or NSString). But IME
applications can use only a few restricted attributes for preedit.
IMKInputController gets those attribute dictionary by calling
markForStyle:range: method to self.

## Other IMKit APIs

*   menu: IMKInputController can specify the menu items in the dropdown menu of
    IME by menu method. It returns NSMenu. Currently MozcController uses a
    static menu items defined by Interface Builder (see
    [English,Japanese].lproj/Config.xib).
*   commitComposition: When a user clicks other window during preedit, the
    preedit text is fixed and then focus is switched. commitComposition is
    called before fixing the preedit text. IMKInputController should clean up
    the current status in this method.
*   bundle identifier: sender proxy object returns the name of the application
    (bundle ID) by bundleIdentifier method. We can use some special behaviors
    according to the bundle ID.
*   Character position: IMKInputController can get the position (in pixel) of
    the specified character in preedit by calling
    attributesForCharacterIndex:lineHeightRectangle: method of sender proxy.
    This information is used to show the candidate windows in the best position.

## Unused IMKit APIs note

*   attributedSubstringFromRange method of sender proxy returns the text of the
    client application. By combining this method and selectedRange method, you
    can get the selected range and achieve reconversion or web-search
    integration.
*   supportUnicode method of sender proxy checks the availability of Unicode of
    the client application.

## Special Applications gotchas (as of 2009)

*   Firefox doesn't support IMKit well. Therefore sometimes IME goes wrong. For
    example, attributedSubstringFromRange does not returns any string, and
    supportUnicode method returns NO always. There's no way to workaround such
    gochas in the IME side.
*   Carbon Emacs, Aquamacs, and Emacs.app returns mysterious pixel position of
    characters.
*   If the IME has some network functionality like web search integration, the
    IMKInputController should check the bundle identifier of the client
    application. The login window of screen saver is "com.apple.securityagent".
    If the client is this application, you should not invoke network
    functionalities.
*   Spotlight and Dashboard has special handling of mouse event. I don't find
    the workaround yet.
