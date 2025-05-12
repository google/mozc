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

#include "renderer/init_mozc_renderer.h"

#include <cstdlib>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "base/init_mozc.h"
#include "base/run_level.h"
#include "base/system_util.h"

ABSL_DECLARE_FLAG(bool, restricted);

namespace mozc::renderer {

void InitMozcRenderer(const char* argv0, int* argc, char*** argv) {
  const mozc::RunLevel::RunLevelType run_level =
      mozc::RunLevel::GetRunLevel(mozc::RunLevel::RENDERER);
  if (run_level >= mozc::RunLevel::DENY) {
    std::exit(-1);
  }

  mozc::SystemUtil::DisableIME();

  // restricted mode
  if (run_level == mozc::RunLevel::RESTRICTED) {
    absl::SetFlag(&FLAGS_restricted, true);
  }
  mozc::InitMozc(argv0, argc, argv);
}

}  // namespace mozc::renderer
