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

#ifndef MOZC_IPC_NAMED_EVENT_H_
#define MOZC_IPC_NAMED_EVENT_H_

#ifdef OS_WINDOWS
#include <windows.h>
#else
#include <semaphore.h>
#endif

#include <string>
#include "base/port.h"

// Named Event class for Inter Process Communication:
// This is a shallow wrapper for Windows CreateEvent() API.
// For Linux/Mac, emulate the CreateEvent behavior
// with SysV Semaphore
//
// Example:
//
// (Process 1)
// NamedEventListener listner("foo");   // Create named event listener named foo
// CHECK(listener.IsAvailable());
// listener.Wait(10000);  // Wait until an event comes.
// Access shared resource
//
// (Process 2)
// NamedEventNotifier notifier("foo");  // Open named event
// CHECK(notifier.IsAvailable())
// send signals to the listener blocked with Wait() method
// notifier.Notify();
//
// After mozc UI launches mozc_server.exe,
// mozc UI needs to wait until mozc_server is ready.
// For this purpose, mozc UI makes a listener object.
//  When mozc_server completes all initialization and
// is ready for conversion, mozc_server sends a notification
// event to the client.
//
// Example
// - UI process
// if (mozc_server is not running) {
//   CreateProcess("mozc_server.exe" ...);
//   mozc::NamedEventListener l("session");
//   Display Splash Window
//   l.Wait(10000);  // 10 second
//   Close splash window
//   Start conversion
// }
//
// - Server process
//  Initialize all static objects
//  Start session server
//  mozc::NamedEventNotifier n("session");
//  n.Notify();

namespace mozc {

// Util class for Named Event
class NamedEventUtil {
 public:
  // return real event name
  // Windows: <kEventPathPrefix>.<sid>.<name>
  // Linux/Mac: <Util::GetUserProfileDirectory()>/.<name>.event
  static const string GetEventPath(const char *name);

 private:
  DISALLOW_COPY_AND_ASSIGN(NamedEventUtil);
};

class NamedEventListener {
 public:
  // Create Named Event named "name"
  // Windows: <kEventPathPrefix>.<sid>.<name>
  // Linux/Mac: <Util::GetUserProfileDirectory()>/.event.<name>
  explicit NamedEventListener(const char *name);
  virtual ~NamedEventListener();

  // Return true if NamedEventListener is available
  bool IsAvailable() const;

  // return true if NamedEventListner is created by this instance.
  // If NamedEventListner opened an exiting handle, return false.
  bool IsOwner() const;

  // Wait until the listener receives a notification
  // event from NamedEventNotifier.
  // You can set the timeout (in msec)
  // if timeout is negative, Waits forever.
  bool Wait(int msec);

  // Wait until the listener receives a notification or
  // the process specfied with pid is terminated.
  // return TIMEOUT: reached timeout
  // return EVENT_SIGNALED: event is signaled.
  // return PROCESS_SIGNALED: process is signaled.
  // NOTE: Only available on Windows and Linux.
  enum {
    TIMEOUT = 0,
    EVENT_SIGNALED = 1,
    PROCESS_SIGNALED = 2,
  };
  int WaitEventOrProcess(int msec, size_t pid);

 private:
  bool is_owner_;

#ifdef OS_WINDOWS
  HANDLE handle_;
#else
  sem_t *sem_;
  string key_filename_;
#endif
};

class NamedEventNotifier {
 public:
  // Open Named event named "name"
  // The named event object should be created by
  // NamedEventListener before calling the constructor
  explicit NamedEventNotifier(const char *name);
  virtual ~NamedEventNotifier();

  // return true if NamedEventNotifier is available
  bool IsAvailable() const;

  // send an wake-up signal to NamedEventListener
  bool Notify();

 private:
#ifdef OS_WINDOWS
  HANDLE handle_;
#else
  sem_t *sem_;
#endif
};
}  // namespace mozc

#endif  // MOZC_IPC_NAMED_EVENT_H_
