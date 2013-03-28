// Copyright 2010-2013, Google Inc.
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

#include <vector>
#include "base/base.h"
#include "base/init.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/util.h"

namespace mozc {
namespace {
class RegisterModuleHandler {
 public:
  virtual void Call() = 0;  // virtual
  void Add(RegisterModuleFunction func);
  RegisterModuleHandler() {}
  virtual ~RegisterModuleHandler() {}

 protected:
  Mutex mutex_;
  vector<RegisterModuleFunction> funcs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegisterModuleHandler);
};

void RegisterModuleHandler::Add(RegisterModuleFunction func) {
  scoped_lock l(&mutex_);
  funcs_.push_back(func);
}

class Initializer : public RegisterModuleHandler {
 public:
  void Call() {
    scoped_lock l(&mutex_);
    VLOG(1) << "Initializer is called";
    for (size_t i = 0; i < funcs_.size(); ++i) {
      funcs_[i]();
    }
    // clear func not to be called twice
    funcs_.clear();
  }
};

class Reloader : public RegisterModuleHandler {
 public:
  void Call() {
    scoped_lock l(&mutex_);
    for (size_t i = 0; i < funcs_.size(); ++i) {
      funcs_[i]();
    }
  }
};

class Finalizer : public RegisterModuleHandler {
 public:
  void Call() {
    scoped_lock l(&mutex_);
    VLOG(1) << "Finalizer is called";
    // reverse order, as new module will depend on old module.
    for (int i = static_cast<int>(funcs_.size() - 1); i >= 0; --i) {
      funcs_[i]();
    }
    // clear func not to be called twice
    funcs_.clear();
  }
};

class ShutdownHandler : public RegisterModuleHandler {
 public:
  void Call() {
    scoped_lock l(&mutex_);
    VLOG(1) << "ShutdownHandler is called";
    // reverse order, as new module will depend on old module.
    for (int i = static_cast<int>(funcs_.size() - 1); i >= 0; --i) {
      funcs_[i]();
    }
    // clear func not to be called twice
    funcs_.clear();
  }
};
}  // namespace
}  // mozc


namespace mozc {

InitializerRegister::InitializerRegister(const char *name,
                                         RegisterModuleFunction func) {
  Singleton<Initializer>::get()->Add(func);
}

// TODO(taku): this function should be thread-safe
void RunInitializers() {
  Singleton<Initializer>::get()->Call();
}
}   // mozc

namespace mozc {

ReloaderRegister::ReloaderRegister(const char *name,
                                   RegisterModuleFunction func) {
  Singleton<Reloader>::get()->Add(func);
}

void RunReloaders() {
  Singleton<Reloader>::get()->Call();
}

FinalizerRegister::FinalizerRegister(const char *name,
                                     RegisterModuleFunction func) {
  Singleton<Finalizer>::get()->Add(func);
}

void RunFinalizers() {
  Singleton<Finalizer>::get()->Call();
  SingletonFinalizer::Finalize();
}

ShutdownHandlerRegister::ShutdownHandlerRegister(const char *name,
                                                 RegisterModuleFunction func) {
  Singleton<ShutdownHandler>::get()->Add(func);
}

void RunShutdownHandlers() {
  Singleton<ShutdownHandler>::get()->Call();
}
}  // mozc
