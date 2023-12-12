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

#include "usage_stats/usage_stats_uploader.h"

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "storage/registry.h"

namespace mozc {
namespace usage_stats {

namespace {
constexpr absl::string_view kRegistryPrefix = "usage_stats.";
constexpr absl::string_view kLastUploadKey = "last_upload";
constexpr absl::string_view kMozcVersionKey = "mozc_version";
constexpr absl::string_view kClientIdKey = "client_id";
}  // namespace

bool UsageStatsUploader::Send(void *data) {
  // Clear local data in the case of migration.
  // This UsageStatsUploader::Send() may be registered to Scheduler in
  // SessionServer and called periodically.
  ClearMetaData();
  return true;
}

void UsageStatsUploader::ClearMetaData() {
  const std::string upload_key = absl::StrCat(kRegistryPrefix, kLastUploadKey);
  const std::string mozc_version_key =
      absl::StrCat(kRegistryPrefix, kMozcVersionKey);
  const std::string client_id_key = absl::StrCat(kRegistryPrefix, kClientIdKey);
  storage::Registry::Erase(upload_key);
  storage::Registry::Erase(mozc_version_key);
  storage::Registry::Erase(client_id_key);
}

}  // namespace usage_stats
}  // namespace mozc
