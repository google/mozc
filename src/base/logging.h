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

#ifndef MOZC_BASE_LOGGING_H_
#define MOZC_BASE_LOGGING_H_

// These includes are kept for legacy reasons. Prefer including them directly,
// unless you use DFATAL severity backported below.
#include "absl/log/check.h"  // IWYU pragma: keep
#include "absl/log/log.h"    // IWYU pragma: keep

// Note that abseil HEAD has ABSL_LTS_RELEASE_VERSION undefined.
#if defined(ABSL_LTS_RELEASE_VERSION) && ABSL_LTS_RELEASE_VERSION < 20240116
// Older version of abseil doesn't ship with DFATAL. This is a very hacky
// backport that needs to be removed as soon as we migrate to Abseil LTS
// 20240116.
#ifdef DEBUG
#define ABSL_LOG_INTERNAL_CONDITION_DFATAL(type, condition) \
  ABSL_LOG_INTERNAL_CONDITION_FATAL(type, condition)

namespace absl {
ABSL_NAMESPACE_BEGIN
static constexpr absl::LogSeverity kLogDebugFatal = absl::LogSeverity::kFatal;
ABSL_NAMESPACE_END
}  // namespace absl
#else  // DEBUG
#define ABSL_LOG_INTERNAL_CONDITION_DFATAL(type, condition) \
  ABSL_LOG_INTERNAL_CONDITION_ERROR(type, condition)

namespace absl {
ABSL_NAMESPACE_BEGIN
static constexpr absl::LogSeverity kLogDebugFatal = absl::LogSeverity::kError;
ABSL_NAMESPACE_END
}  // namespace absl
#endif  // !DEBUG
#endif  // ABSL_LTS_RELEASE_VERSION < 20240116

#endif  // MOZC_BASE_LOGGING_H_
