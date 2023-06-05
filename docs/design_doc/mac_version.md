# Mac version

[TOC]

This document describes how Mac version works and what's inside of the Mac
version. To build, test, and several tasks are in other documents.

## IMKit.framework

To develop an input method in macOS, you need to develop the input method kit
framework
([IMKit.framework](http://developer.apple.com/library/mac/#documentation/Cocoa/Reference/InputMethodKitFrameworkRef/_index.html))
in Cocoa. This does not directly mean you have to develop input method with
Objective-C, you can use Ruby or Python or Java, but I chose Objective-C because
of code compatibility.

There are two major classes in IMKit: IMKInputServer and IMKInputController.

IMKInputController is the superclass of input method session. It is instantiated
when a new application is invoked or a new text area is focused, and
handleEvent: is invoked for key events. The major task of Mac client is to
convert the NSEvent to Mozc's key event and send to the converter. Also, this
class handles the OS status change such like mode change by mouse clicks.

IMKInputServer handles all of the connection between applications, and invokes
IMKInputController. Normally we don't need to inherit IMKInputServer but use the
standard server class directly, but we do inherit this as
GoogleJapaneseInputServer for some reasons. See *Communication with renderer*
section for the details.

The IMKit.framework configurations are in Info.plist of the client application.
You'll see the GoogleJapaneseInputController class name in mac/Info.plist.

## Communication with converter

The communication is done by client/ library, and its core implementation is
ipc/mach\_ipc.cc.
This uses Mach kernel's messaging system. We do not use
standard NSConnection because we encountered a memory leak issue of NSConnection
in Snow Leopard (the older implementation can be seen at ipc/mac\_ipc.mm).

Mach kernel's messaging is quite simple. It also provides structural message
passing but we do not use it: serialize the protobuf message into a binary
string and send it.

One core concern is name registration. The Mach messaging specifies the target
by a name string, and the operating system must know which name represents which
process. It had been done by the process itself until Tiger, but from Leopard,
this approach hasn't been working. Rather, you need newly introduced *launchd*
mechanism.

*launchd* manages operating system services. It behaves as xinetd and crond in
Linux. You can put configuration file into /Library/LaunchDaemons (system level
services running with root) or /Library/LaunchAgents (user level services
running with the login account). We put the server paths, the service name
(com.google.inputmethod.Japanese.Converter), and launch policy into the
configuration file under /Library/LaunchAgents. Then, when the client tries to
communicate to a service with the name of
"com.google.inputmethod.Japanese.Converter", the launchd already knows this name
is corresponded to the converter process, and then put it into the converter.

One security concern arises here: if some malicious process changes the
configuration of launchd, the client connects to a different process which may
be a key logger. Then, the IPC client code periodically checks the launchd
configuration by its API and verifies the target process path name. Also the
server side process automatically exists if the obtained IPC name is different
from the expecatation.

Another side effect of using launchd is that the operating system automatically
launches the Converter process. It means that Mac version doesn't use several
Mozc services such like "start\_server" or "server\_process". There are still
them, and the code is saying to call them, but mostly such code path won't run
at all.

## Renderer

Renderer is extremely OS-dependent. We use both Carbon and Cocoa APIs to develop
the window. We use Carbon API to create the window because I don't know how to
create a window of:

*   no focus is allowed
*   appears above of the Dashboard
*   mouse click doesn't change the application focus
*   but mouse click event comes to the candidate window

See renderer/mac/RendererWindowBase.mm to achieve these conditions. We noticed
that some conditions doesn't achieved in OSX Lion, but we do not have specific
plans to fix this.

On the other hand, we just use standard NSView to render the candidates because
we do not have specific reason to avoid Cocoa.

One of the most tricky part in renderer is sending the mouse click event back to
the client. That's tricky because renderer is an IPC server and client does not
run Mozc's IPC server system -- it already has a standard NSRunloop event
handling due to IMKit.framework.

This is why we inherit IMKInputServer, as I described above. The
GoogleJapaneseInputServer registers a new event handling for a new connection
actually. When a click happens, the renderer packs the protobuf data into a
binary string, pack it into NSData, and send this to the client's connection by
a standard Cocoa's IPC using NSConnection.

You may notice "wait, does NSConnection cause memory leaks?" This is absolutely
true, but I decided not to care this because the frequency of mouse clicks in
the candidate window is quite small. Much smaller than the ones in the
communication with converter because its memory leak was caused for every key
events.

## Installer and packaging

We use the standard installer productbuild and pkgbuild.
To achieve automatic turn-on and check of dev version, we use InstallerPane.
They are mac/ActivatePane and mac/DevConfirmPane.
They look like a standard Cocoa bundle.

## Other Mac utils

Several utilities are used for Mac specific binaries. You can find code around
base/mac\_util.mm and base/mac\_process.mm mac\_util has simple utilities for
getting paths or something like that, and mac\_process has utilities to invoke
other processes or open browsers.

## Mozc tools designs

mozc\_tool is really tricky part actually. In Windows and Linux, mozc\_tool is a
single binary but changes the behavior by the flag of --mode. This approach does
not work in Mac actually. In Mac, a binary is tied to an application, and an
application works simply in one way. You cannot put a flag to a application. If
you invoke a same application, the OS kindly stop the process invocation and
just raises the existing application to the top -- so single binary approach
does not allow users to put config dialog and dictionary tool at the same time.

Then, to reduce the application size, Qt frameworks are located only in
ConfigDialog.app and share with other tools.

## Icon and images

Mac data files are under data/images/mac. The file with icns suffix is the file.
You need to use IconComposer.app in the Xcode to edit this.
