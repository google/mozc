// Copyright 2010-2012, Google Inc.
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

#include <string>
#include <vector>

#include "base/base.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/section.h"

namespace mozc {

DictionaryFile::DictionaryFile() {}

DictionaryFile::~DictionaryFile() {}

bool DictionaryFile::OpenFromFile(const string &file) {
  mapping_.reset(new Mmap());
  CHECK(mapping_->Open(file.c_str()));
  return OpenFromImage(mapping_->begin(), mapping_->size());
}

bool DictionaryFile::OpenFromImage(const char *image, int length) {
  sections_.clear();
  const bool result =
      DictionaryFileCodecFactory::GetCodec()->ReadSections(
          image, length, &sections_);
  return result;
}

const char *DictionaryFile::GetSection(const string &section_name,
                                       int *len) const {
  DCHECK(len);
  const string name =
      DictionaryFileCodecFactory::GetCodec()->GetSectionName(section_name);
  for (size_t i = 0; i < sections_.size(); ++i) {
    if (sections_[i].name == name) {
      *len = sections_[i].len;
      return sections_[i].ptr;
    }
  }
  return NULL;
}
}  // namespace mozc
