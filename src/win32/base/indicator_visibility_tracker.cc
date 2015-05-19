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

#include "win32/base/indicator_visibility_tracker.h"

namespace mozc {
namespace win32 {
namespace {

IndicatorVisibilityTracker::Action GetDefaultAction(
    bool previously_visible, bool now_visible) {
  if (previously_visible == now_visible) {
    return IndicatorVisibilityTracker::kNothing;
  }
  return IndicatorVisibilityTracker::kUpdateUI;
}

}  // namespace

struct IndicatorVisibilityTracker::InternalState {
 public:
  InternalState()
      : visible(false) {}

  bool visible;

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalState);
};

IndicatorVisibilityTracker::IndicatorVisibilityTracker()
    : state_(new InternalState) {}

IndicatorVisibilityTracker::~IndicatorVisibilityTracker() {}

IndicatorVisibilityTracker::Action
IndicatorVisibilityTracker::OnDissociateContext() {
  const bool original = state_->visible;
  state_->visible = false;
  return GetDefaultAction(original, state_->visible);
}

IndicatorVisibilityTracker::Action IndicatorVisibilityTracker::OnTestKey(
    const VirtualKey &key, bool is_down, bool eaten) {
  if (!is_down) {
    return kNothing;
  }
  const bool original = state_->visible;
  state_->visible = false;
  return GetDefaultAction(original, state_->visible);
}

IndicatorVisibilityTracker::Action IndicatorVisibilityTracker::OnKey(
    const VirtualKey &key, bool is_down, bool eaten) {
  if (!is_down) {
    return kNothing;
  }
  const bool original = state_->visible;
  state_->visible = false;
  return GetDefaultAction(original, state_->visible);
}

IndicatorVisibilityTracker::Action
IndicatorVisibilityTracker::OnMoveFocusedWindow() {
  const bool original = state_->visible;
  state_->visible = false;
  return GetDefaultAction(original, state_->visible);
}

IndicatorVisibilityTracker::Action
IndicatorVisibilityTracker::OnChangeInputMode() {
  const bool original = state_->visible;
  state_->visible = true;
  return GetDefaultAction(original, state_->visible);
}

bool IndicatorVisibilityTracker::IsVisible() const {
  return state_->visible;
}

}  // namespace win32
}  // namespace mozc
