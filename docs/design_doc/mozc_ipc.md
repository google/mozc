# Mozc IPC

## Abstract

This document describes why IPC is necessary to run Mozc, and how each
implementation should be.

## Background

Mozc is an input method product and it consits with multiple processes
to achieve text input.  We have "converter" process to maintain
conversions and "renderer" process to render the candidates of input.

## Implementation requirements

Mozc IPC call happens for EVERY key events.  Thus we need to care the
privacy/security.  Currently we adopt following policy:

  * the IPC name has to be private to the user: processes which run with another user's auth must not access to the IPC.
  * the IPC name has to be safe to squatters.  For example, using random IPC name will reduce the danger of squatters.

In addition, Mozc IPC call is "one-shot".  It doesn't require any
"connections".  When a key event arrives to Mozc, it creates an
IPCClient instance, calls Call() method to send the command to the
server, gets its response, and then destroy IPCClient object.

So you don't need to care the maintenance of connections, but you need
to care the performance a bit.  If one of a step during this is very
slow, it will damage the performance of Mozc.

Note that IPC calls happen only when a user presses a key or clicks
mouses.  So if performance of an implementation is not the best, it
might not be a problem.  Please think about the balance of
implementation and security.
