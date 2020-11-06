// Copyright 2010-2020, Google Inc.
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

#ifndef MOZC_DICTIONARY_FILE_DICTIONARY_FILE_BUILDER_H_
#define MOZC_DICTIONARY_FILE_DICTIONARY_FILE_BUILDER_H_

#include <set>
#include <string>
#include <vector>

#include "base/port.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/section.h"

namespace mozc {
namespace dictionary {

class DictionaryFileBuilder {
 public:
  explicit DictionaryFileBuilder(DictionaryFileCodecInterface *file_codec);
  virtual ~DictionaryFileBuilder();

  // Adds a section from a file
  bool AddSectionFromFile(const std::string &section_name,
                          const std::string &file_name);

  // Writes the image of dictionary file to a file.
  void WriteImageToFile(const std::string &file_name) const;

 private:
  // DictionaryFileBuilder does not take the ownership of |file_codec_|.
  DictionaryFileCodecInterface *file_codec_;
  std::vector<DictionaryFileSection> sections_;
  std::set<std::string> added_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryFileBuilder);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_FILE_DICTIONARY_FILE_BUILDER_H_
