// Copyright 2010, Google Inc.
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

#include "base/base.h"
#include "base/mutex.h"
#include "base/singleton.h"

namespace mozc {
namespace {

const size_t kMaxFinalizersSize = 256;

size_t g_finalizers_size = 0;
Mutex *g_mutex = NULL;
once_t g_mutex_once = MOZC_ONCE_INIT;
once_t g_singleton_finalize_once = MOZC_ONCE_INIT;

SingletonFinalizer::FinalizerFunc g_finalizers[kMaxFinalizersSize];

void InitSingletonMutex() {
  g_mutex = new Mutex;
  CHECK(g_mutex);
}
}  // namespace


void SingletonFinalizer::AddFinalizer(FinalizerFunc func) {
  // TODO(taku):
  // we only allow up to kMaxFinalizersSize functions here.
  CallOnce(&g_mutex_once, &InitSingletonMutex);
  {
    scoped_lock l(g_mutex);
    CHECK_LT(g_finalizers_size, kMaxFinalizersSize);
    g_finalizers[g_finalizers_size++] = func;
  }
}

namespace {
void DeleteSingleton() {
  // delete instances in reverse order.
  for (int i = static_cast<int>(g_finalizers_size) - 1; i >= 0; --i) {
    (*g_finalizers[i])();
  }
  // set kMaxFinalizersSize so that AddFinalizers cannot be called
  // twice
  g_finalizers_size = kMaxFinalizersSize;
  delete g_mutex;
  g_mutex = NULL;
}
}  // namespace

void SingletonFinalizer::Finalize() {
  CallOnce(&g_singleton_finalize_once, &DeleteSingleton);
}
}  // mozc
