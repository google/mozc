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

#ifndef MOZC_BASE_EMBEDDED_FILE_H_
#define MOZC_BASE_EMBEDDED_FILE_H_

// Utility to embed a file in C++ binary.
//
// Usage:
// 1) Generate C++ code to be included in a source file.
//    $ ./path/to/artifacts/embed_file
//      --input=/path/to/file
//      --name=kVarName
//      --output=/path/to/header
//
// 2) Include the generated file (/path/to/header) in cc file:
//
//    #include "base/embedded_file.h"
//
//    namespace {
//    // The definition of kVarName, which was specified in --name flag in 1),
//    // is included here (#include in anonymous namespace is recommended).
//    #include "/path/to/header"
//    }
//
//    // In this translation unit, the file content can be retrieved by:
//    absl::string_view data = LoadEmbeddedFile(kVarName);

#include <cstddef>
#include <cstdint>

#include "absl/strings/string_view.h"

namespace mozc {

// Stores a byte data of file and its file size.  To create this structure, use
// embed_file.py.  The first address of embedded file data is aligned at 64 bit
// boundary, so we can embed data that requires normal alignment (8, 16, etc.).
struct EmbeddedFile {
  const uint64_t* const data;
  const size_t size;
};

// Interprets EmbeddedFile as a byte array.
inline absl::string_view LoadEmbeddedFile(EmbeddedFile f) {
  return absl::string_view(reinterpret_cast<const char*>(f.data), f.size);
}

}  // namespace mozc

#endif  // MOZC_BASE_EMBEDDED_FILE_H_
