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

#ifndef MOZC_BASE_VLOG_H_
#define MOZC_BASE_VLOG_H_

#include "absl/log/log.h"

namespace mozc::internal {

// Returns the current verbose log level, which is the maximum of --v flag and
// `verbose_level` in the config.
int GetVLogLevel();

// Updates the (mirror of) `verbose_level` in the config.
//
// To avoid dependency on the config from logging library, vlog holds a copy of
// the `verbose_level` internally, and config handlers are expected to call this
// setter accordingly.
void SetConfigVLogLevel(int v);

}  // namespace mozc::internal

#define MOZC_VLOG_IS_ON(severity) (mozc::internal::GetVLogLevel() >= severity)

#define MOZC_VLOG(severity) LOG_IF(INFO, MOZC_VLOG_IS_ON(severity))

#define MOZC_DVLOG(severity) DLOG_IF(INFO, MOZC_VLOG_IS_ON(severity))

#endif  // MOZC_BASE_VLOG_H_
