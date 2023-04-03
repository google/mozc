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

#include "dictionary/file/dictionary_file.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/mmap.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/section.h"
#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace dictionary {

DictionaryFile::DictionaryFile(const DictionaryFileCodecInterface *file_codec)
    : file_codec_(file_codec) {
  DCHECK(file_codec_);
}

absl::Status DictionaryFile::OpenFromFile(const std::string &file) {
  absl::StatusOr<Mmap> mapping = Mmap::Map(file);
  if (!mapping.ok()) {
    return std::move(mapping).status();
  }
  mapping_ = *std::move(mapping);
  return OpenFromImage(mapping_.begin(), mapping_.size());
}

absl::Status DictionaryFile::OpenFromImage(const char *image, int length) {
  sections_.clear();
  return file_codec_->ReadSections(image, length, &sections_);
}

const char *DictionaryFile::GetSection(const absl::string_view section_name,
                                       int *len) const {
  DCHECK(len);
  const std::string &name = file_codec_->GetSectionName(section_name);
  for (const auto &section : sections_) {
    if (section.name == name) {
      *len = section.len;
      return section.ptr;
    }
  }
  return nullptr;
}

}  // namespace dictionary
}  // namespace mozc
