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

// Defines codec for dictionary file

#ifndef MOZC_DICTIONARY_FILE_CODEC_H_
#define MOZC_DICTIONARY_FILE_CODEC_H_

#include <atomic>
#include <cstdint>
#include <iosfwd>
#include <ostream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "dictionary/file/codec_interface.h"
#include "dictionary/file/section.h"

namespace mozc {
namespace dictionary {

class DictionaryFileCodec : public DictionaryFileCodecInterface {
 public:
  DictionaryFileCodec() = default;
  DictionaryFileCodec(const DictionaryFileCodec &) = delete;
  DictionaryFileCodec &operator=(const DictionaryFileCodec &) = delete;

  void WriteSections(absl::Span<const DictionaryFileSection> sections,
                     std::ostream *ofs) const override;
  absl::Status ReadSections(
      const char *image, int length,
      std::vector<DictionaryFileSection> *sections) const override;
  std::string GetSectionName(absl::string_view name) const override;

 private:
  void WriteHeader(std::ostream *ofs) const;
  void WriteSection(const DictionaryFileSection &section,
                    std::ostream *ofs) const;

  // Seed value for name string finger print
  // Made it mutable for reading sections. std::atomic is used to make it
  // thread-safe.
  mutable std::atomic<int32_t> seed_ = 2135654146;
  // Magic value for simple file validation
  const int32_t filemagic_ = 20110701;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_FILE_CODEC_H_
