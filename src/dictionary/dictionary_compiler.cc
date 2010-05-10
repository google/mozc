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

// we here independent dictionary_compile.cc as
// this file cannot be included in mozc_server

#include "dictionary/dictionary.h"

#include "base/base.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "dictionary/user_dictionary.h"

DECLARE_int32(maximum_cost_threshold);

namespace mozc {

void Dictionary::Compile(DictionaryType type,
                         const char *text_file,
                         const char *binary_file) {
#if defined OS_WINDOWS && defined _DEBUG
  // Seems that dictionary compiler won't work due to allocation faiulre
  // with debug build. So, we restrict the size of dictionary for debug build.
  FLAGS_maximum_cost_threshold = 8000;
#endif

  switch (type) {
    case SYSTEM:
      SystemDictionaryBuilder::Compile(text_file, binary_file);
      break;
    default:
      break;
  }
}
}  // namespace mozc
