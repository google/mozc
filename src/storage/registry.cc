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

#include "storage/registry.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/vlog.h"
#include "storage/storage_interface.h"
#include "storage/tiny_storage.h"

namespace mozc {
namespace storage {
namespace {

ABSL_CONST_INIT absl::Mutex g_mutex(absl::kConstInit);

constexpr absl::string_view RegistryFileName() {
  if constexpr (TargetIsWindows()) {
    return "registry.db";
  } else {
    return ".registry.db";  // hidden file
  }
}

class StorageInitializer {
 public:
  StorageInitializer() { SetStorage(TinyStorage::New()); }

  StorageInterface *GetStorage() const { return storage_.get(); }

  void SetStorage(std::unique_ptr<StorageInterface> storage) {
    storage_ = std::move(storage);
    if (!storage_->Open(FileUtil::JoinPath(
            SystemUtil::GetUserProfileDirectory(), RegistryFileName()))) {
      LOG(ERROR) << "cannot open registry";
    }
  }

 private:
  std::unique_ptr<StorageInterface> storage_;
};
}  // namespace

bool Registry::Erase(const std::string &key) {
  absl::MutexLock l(&g_mutex);
  return Singleton<StorageInitializer>::get()->GetStorage()->Erase(key);
}

bool Registry::Sync() {
  absl::MutexLock l(&g_mutex);
  return Singleton<StorageInitializer>::get()->GetStorage()->Sync();
}

// clear internal keys and values
bool Registry::Clear() {
  absl::MutexLock l(&g_mutex);
  return Singleton<StorageInitializer>::get()->GetStorage()->Clear();
}

void Registry::SetStorage(std::unique_ptr<StorageInterface> handler) {
  MOZC_VLOG(1) << "New storage interface is set";
  absl::MutexLock l(&g_mutex);
  Singleton<StorageInitializer>::get()->SetStorage(std::move(handler));
}

bool Registry::LookupInternal(const std::string &key, std::string *value) {
  absl::MutexLock l(&g_mutex);  // just for safe
  return Singleton<StorageInitializer>::get()->GetStorage()->Lookup(key, value);
}

bool Registry::InsertInternal(const std::string &key,
                              const std::string &value) {
  absl::MutexLock l(&g_mutex);
  return Singleton<StorageInitializer>::get()->GetStorage()->Insert(key, value);
}
}  // namespace storage
}  // namespace mozc
