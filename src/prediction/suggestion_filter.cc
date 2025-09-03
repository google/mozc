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

#include "prediction/suggestion_filter.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/util.h"
#include "storage/existence_filter.h"

namespace mozc {

using ::mozc::storage::ExistenceFilter;

absl::StatusOr<SuggestionFilter> SuggestionFilter::Create(
    const absl::Span<const uint32_t> data) {
  absl::StatusOr<ExistenceFilter> filter = ExistenceFilter::Read(data);
  if (!filter.ok()) {
    LOG(ERROR) << "SuggestionFilterData is broken: " << filter.status();
    return filter.status();
  }
  return SuggestionFilter(*std::move(filter));
}

SuggestionFilter SuggestionFilter::CreateOrDie(
    const absl::Span<const uint32_t> data) {
  absl::StatusOr<SuggestionFilter> filter = Create(data);
  CHECK_OK(filter);
  return *std::move(filter);
}

bool SuggestionFilter::IsBadSuggestion(const absl::string_view text) const {
  std::string lower_text(text);
  Util::LowerString(&lower_text);
  return filter_.Exists(lower_text);
}

}  // namespace mozc
