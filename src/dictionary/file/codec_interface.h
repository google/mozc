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

#ifndef MOZC_DICTIONARY_FILE_CODEC_INTERFACE_H_
#define MOZC_DICTIONARY_FILE_CODEC_INTERFACE_H_

#include <iosfwd>
#include <string>
#include <vector>

#include "base/port.h"
#include "base/status.h"
#include "dictionary/file/section.h"

namespace mozc {
namespace dictionary {

class DictionaryFileCodecInterface {
 public:
  // Writes sections to an output file stream.
  virtual void WriteSections(const std::vector<DictionaryFileSection> &sections,
                             std::ostream *ofs) const = 0;

  // Reads sections from memory image.
  virtual mozc::Status ReadSections(
      const char *image, int length,
      std::vector<DictionaryFileSection> *sections) const = 0;

  // Gets section name.
  virtual std::string GetSectionName(const std::string &name) const = 0;

 protected:
  DictionaryFileCodecInterface() = default;
  virtual ~DictionaryFileCodecInterface() = default;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_FILE_CODEC_INTERFACE_H_
