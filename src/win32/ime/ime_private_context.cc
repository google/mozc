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

#include "win32/ime/ime_private_context.h"

#include <cstdint>

#include "base/run_level.h"
#include "client/client_interface.h"
#include "protocol/commands.pb.h"
#include "win32/base/config_snapshot.h"
#include "win32/base/deleter.h"
#include "win32/base/focus_hierarchy_observer.h"
#include "win32/base/indicator_visibility_tracker.h"
#include "win32/base/input_state.h"
#include "win32/base/surrogate_pair_observer.h"
#include "win32/ime/ime_core.h"
#include "win32/ime/ime_scoped_context.h"
#include "win32/ime/ime_ui_visibility_tracker.h"

namespace mozc {
namespace win32 {
namespace {

#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
constexpr uint32_t kMagicNumber = 0x4d6f7a63;  // 'Mozc'
#else  // GOOGLE_JAPANESE_INPUT_BUILD
constexpr uint32_t kMagicNumber = 0x637a6f4d;  // 'cozM'
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

static HIMCC InitializeHIMCC(HIMCC himcc, DWORD size) {
  if (himcc == nullptr) {
    return ::ImmCreateIMCC(size);
  } else {
    return ::ImmReSizeIMCC(himcc, size);
  }
}

}  // namespace

bool PrivateContext::Initialize() {
  magic_number = kMagicNumber;
  thread_id = ::GetCurrentThreadId();

  client = mozc::client::ClientFactory::NewClient();
  // VKBackBasedDeleter is responsible for supporting DELETE_PRECEDING_TEXT.
  mozc::commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  client->set_client_capability(capability);

  ime_behavior = new InputBehavior();
  ime_state = new InputState();
  ui_visibility_tracker = new UIVisibilityTracker();
  indicator_visibility_tracker = new IndicatorVisibilityTracker();
  last_output = new mozc::commands::Output();
  deleter = new VKBackBasedDeleter();
  surrogate_pair_observer = new SurrogatePairObserver();
  focus_hierarchy_observer = FocusHierarchyObserver::Create();
  return true;
}

bool PrivateContext::Uninitialize() {
  if (!Validate()) {
    return false;
  }
  delete client;
  delete ime_state;
  delete ime_behavior;
  delete indicator_visibility_tracker;
  delete ui_visibility_tracker;
  delete last_output;
  delete deleter;
  delete surrogate_pair_observer;
  delete focus_hierarchy_observer;
  magic_number = 0;
  thread_id = 0;
  client = nullptr;
  ime_state = nullptr;
  ime_behavior = nullptr;
  ui_visibility_tracker = nullptr;
  indicator_visibility_tracker = nullptr;
  last_output = nullptr;
  deleter = nullptr;
  surrogate_pair_observer = nullptr;
  focus_hierarchy_observer = nullptr;
  return true;
}

bool PrivateContext::Validate() const {
  if (magic_number != kMagicNumber) {
    // This object has not been initialized, or unknown data structure.
    return false;
  }
  // As revealed in b/3195434, HIMC behaves as if it is *NOT* bound to a
  // specific thread in spite of the description in MSDN.
  // This is why we cannot check the thread ID here.
  return true;
}

bool PrivateContextUtil::IsValidPrivateContext(HIMCC private_data_handle) {
  if (private_data_handle == nullptr) {
    return false;
  }
  if (::ImmGetIMCCSize(private_data_handle) != sizeof(PrivateContext)) {
    return false;
  }
  ScopedHIMCC<PrivateContext> private_context(private_data_handle);
  return private_context->Validate();
}

bool PrivateContextUtil::EnsurePrivateContextIsInitialized(
    HIMCC *private_data_handle_pointer) {
  if (private_data_handle_pointer == nullptr) {
    return false;
  }

  const HIMCC previous_private_data_handle = *private_data_handle_pointer;
  if (IsValidPrivateContext(previous_private_data_handle)) {
    // Already initialized.  Nothing to do.
    return true;
  }

  // Allocate memory.
  *private_data_handle_pointer = InitializeHIMCC(
      *private_data_handle_pointer, sizeof(mozc::win32::PrivateContext));
  const HIMCC new_private_data_handle = *private_data_handle_pointer;
  if (new_private_data_handle == nullptr) {
    // Failed to allocate memory.
    return false;
  }

  ScopedHIMCC<mozc::win32::PrivateContext> private_context_allocator(
      new_private_data_handle);
  private_context_allocator->Initialize();
  if (!mozc::RunLevel::IsValidClientRunLevel()) {
    private_context_allocator->ime_behavior->disabled = true;
    // Return FALSE when inactive mode, hoping to prevent UIWnd from being
    // created.  (But actually, the return value seems to be ignored.)
    return false;
  }

  // Try to reflect the current config to the IME behavior.
  ConfigSnapshot::Info snapshot;
  if (ConfigSnapshot::Get(private_context_allocator->client, &snapshot)) {
    auto *behavior = private_context_allocator->ime_behavior;
    behavior->prefer_kana_input = snapshot.use_kana_input;
    behavior->use_romaji_key_to_toggle_input_style =
        snapshot.use_keyboard_to_change_preedit_method;
    behavior->use_mode_indicator = snapshot.use_mode_indicator;
    behavior->direct_mode_keys = snapshot.direct_mode_keys;
    behavior->initialized = true;
  }

  return true;
}

}  // namespace win32
}  // namespace mozc
