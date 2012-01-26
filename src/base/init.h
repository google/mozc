// Copyright 2010-2012, Google Inc.
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

// This is a simple port of googleinit

// Example:
//  static const char* my_hostname = NULL;
//  REGISTER_MODULE_INITIALIZER(hostname, {
//   // Code to initialize "my_hostname"
//  });
//
//  static const char* my_hostname = NULL;
//  static void InitMyHostname() {
//    // Code to initialize "my_hostname"
//  }
//  REGISTER_MODULE_INITIALIZER(my_hostname, InitMyHostname());


#ifndef MOZC_BASE_INIT_H_
#define MOZC_BASE_INIT_H_

#include <string>
#include "base/port.h"

typedef void (*RegisterModuleFunction)();

namespace mozc {

class InitializerRegister {
 public:
  InitializerRegister(const char *name,
                      RegisterModuleFunction function);
};

// call all initialize functions
void RunInitializers();

#define REGISTER_MODULE_INITIALIZER(name, body) \
  static void mozc_initializer_##name() { body; } \
  static const mozc::InitializerRegister \
  initializer_##name(#name, mozc_initializer_##name);

class ReloaderRegister {
 public:
  ReloaderRegister(const char *name,
                   RegisterModuleFunction function);
};

// Main thread can call finalizers at the end of main().
// You can register all cleanup routines with finalizer.
class FinalizerRegister {
 public:
  FinalizerRegister(const char *name,
                    RegisterModuleFunction function);
};

// Can define callback functions which are called
// when operating systems or installer/uninstaller
// sends "shutdown" event to the converter/renderere.
// Callback function MUST be thread safe, as
// this handler may be called asynchronously. You may
// not implement complicated operations with this handler.
class ShutdownHandlerRegister {
 public:
  ShutdownHandlerRegister(const char *name,
                          RegisterModuleFunction function);
};

void RunReloaders();
void RunFinalizers();
void RunShutdownHandlers();
}  // namespace mozc

// Reloaders are also called after initializers are invoked.
#define REGISTER_MODULE_RELOADER(name, body) \
  static void mozc_reloader_##name() { body; } \
  static const mozc::ReloaderRegister \
  reloader_##name(#name, mozc_reloader_##name);

#define REGISTER_MODULE_SHUTDOWN_HANDLER(name, body) \
  static void mozc_shutdown_handler_##name() { body; } \
  static const mozc::ShutdownHandlerRegister \
  reloader_##name(#name, mozc_shutdown_handler_##name);

#define REGISTER_MODULE_FINALIZER(name, body) \
  static void mozc_finalizer_##name() { body; } \
  static const mozc::FinalizerRegister \
  finalizer_##name(#name, mozc_finalizer_##name);

#endif  // MOZC_BASE_INIT_H_
