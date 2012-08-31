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

#include "base/singleton.h"

#ifdef OS_WINDOWS
#include <Windows.h>
#endif  // OS_WINDOWS

#include "base/base.h"
#include "base/mutex.h"

namespace mozc {
namespace {

const size_t kMaxFinalizersSize = 256;
size_t g_finalizers_size = 0;

SingletonFinalizer::FinalizerFunc g_finalizers[kMaxFinalizersSize];

// We can't use CHECK logic for Singleton because CHECK (and LOG)
// obtains the singleton LogStream by calling Singleton::get().  If
// something goes wrong during Singleton::get of the LogStream, it
// recursively calls Singleton::get again to report errors, which
// leads an inifinite wait loop because the first Singleton::get locks
// everything.
// ExitWithError() exits the program without reporting errors, which
// is not good but better than an inifinite loop.
void ExitWithError() {
  // This logic is copied from logging.h
#ifdef OS_WINDOWS
  ::RaiseException(::GetLastError(),
                   EXCEPTION_NONCONTINUABLE,
                   NULL, NULL);
#else
  exit(-1);
#endif
}
}  // anonymous namespace

void SingletonFinalizer::AddFinalizer(FinalizerFunc func) {
  // When g_finalizers_size is equal to kMaxFinalizersSize,
  // SingletonFinalizer::Finalize is called already.
  if (g_finalizers_size >= kMaxFinalizersSize) {
    ExitWithError();
  }
  // This part is not thread safe.
  // When two different classes are instantiated at the same time,
  // this code will raise an exception.
  g_finalizers[g_finalizers_size++] = func;
}

void SingletonFinalizer::Finalize() {
  // This part is not thread safe.
  // When two different classes are instantiated at the same time,
  // this code will raise an exception.
  for (int i = static_cast<int>(g_finalizers_size) - 1; i >= 0; --i) {
    (*g_finalizers[i])();
  }
  g_finalizers_size = 0;
}
}  // mozc
