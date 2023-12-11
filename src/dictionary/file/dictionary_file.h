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

// This code manages ar/cpio/tar like file structure that contains
// multiple sections in a file. Each section has a name and size.

#ifndef MOZC_DICTIONARY_FILE_DICTIONARY_FILE_H_
#define MOZC_DICTIONARY_FILE_DICTIONARY_FILE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/mmap.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "dictionary/file/codec_interface.h"

namespace mozc {
namespace dictionary {

struct DictionaryFileSection;

class DictionaryFile final {
 public:
  explicit DictionaryFile(const DictionaryFileCodecInterface *file_codec);

  DictionaryFile(const DictionaryFile &) = delete;
  DictionaryFile &operator=(const DictionaryFile &) = delete;

  ~DictionaryFile() = default;

  // Opens from a file.
  absl::Status OpenFromFile(const std::string &file);

  // Opens from a memory block.
  absl::Status OpenFromImage(const char *image, int len);

  // Gets a pointer to the section having |section_name|. Image size is set to
  // |len|. Returns nullptr when not found.
  const char *GetSection(absl::string_view section_name, int *len) const;

 private:
  // DictionaryFile does not take the ownership of |file_codec_|.
  const DictionaryFileCodecInterface *file_codec_;
  Mmap mapping_;
  std::vector<DictionaryFileSection> sections_;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_FILE_DICTIONARY_FILE_H_
