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

#include "win32/tip/tip_thread_context.h"

#include <limits>
#include <memory>

#include "base/win_util.h"
#include "win32/base/focus_hierarchy_observer.h"
#include "win32/tip/tip_input_mode_manager.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

TipInputModeManager::Config GetConfig() {
  TipInputModeManager::Config config;
  config.use_global_mode = WinUtil::IsPerUserInputSettingsEnabled();
  return config;
}

}  // namespace

class TipThreadContext::InternalState {
 public:
  InternalState() : input_mode_manager(GetConfig()), focus_revision(0) {}
  TipInputModeManager input_mode_manager;
  std::unique_ptr<FocusHierarchyObserver> focus_hierarchy_observer;
  int32 focus_revision;
};

TipThreadContext::TipThreadContext() : state_(new InternalState) {}

TipThreadContext::~TipThreadContext() {}

TipInputModeManager *TipThreadContext::GetInputModeManager() {
  return &state_->input_mode_manager;
}

const FocusHierarchyObserver *TipThreadContext::GetFocusHierarchyObserver()
    const {
  return state_->focus_hierarchy_observer.get();
}

void TipThreadContext::InitializeFocusHierarchyObserver() {
  if (state_->focus_hierarchy_observer.get() == nullptr) {
    state_->focus_hierarchy_observer.reset(FocusHierarchyObserver::Create());
    state_->focus_hierarchy_observer->SyncFocusHierarchy();
  }
}

int32 TipThreadContext::GetFocusRevision() const {
  return state_->focus_revision;
}

void TipThreadContext::IncrementFocusRevision() {
  if (state_->focus_revision < std::numeric_limits<int32>::max()) {
    state_->focus_revision++;
  } else {
    state_->focus_revision = 0;
  }
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
