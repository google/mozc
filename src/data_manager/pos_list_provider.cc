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

#include "data_manager/pos_list_provider.h"

#include <string>

#include "base/embedded_file.h"
#include "base/logging.h"
#include "base/serialized_string_array.h"

namespace mozc {
namespace {

#if defined(MOZC_BUILD)
// Contains the definition of kPosArray (embedded file).
#include "data_manager/oss/pos_list.h"
#elif defined(GOOGLE_JAPANESE_INPUT_BUILD)  // MOZC_BUILD
#else   // defined(MOZC_BUILD) or defined(GOOGLE_JAPANESE_INPUT_BUILD)
#error "Neither MOZC_BUILD nor GOOGLE_JAPANESE_INPUT_BUILD are defined"
#endif  // defined(GOOGLE_JAPANESE_INPUT_BUILD)

}  // namespace

PosListProvider::PosListProvider() = default;
PosListProvider::~PosListProvider() = default;

void PosListProvider::GetPosList(std::vector<std::string> *pos_list) const {
  SerializedStringArray array;
  CHECK(array.Init(LoadEmbeddedFile(kPosArray)));
  pos_list->resize(array.size());
  for (size_t i = 0; i < array.size(); ++i) {
    (*pos_list)[i].assign(array[i].begin(), array[i].end());
  }
}

}  // namespace mozc
